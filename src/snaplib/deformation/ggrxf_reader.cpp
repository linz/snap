#include "deformation/ggrxf_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "deformation/ggrxf_date.h"
#include "deformation/grid_geometry.h"
#include "deformation/netcdf_ragged_node_value_source.h"
#include "deformation/netcdf_scalar_node_value_source.h"
#include "deformation/netcdf_wrapper.h"
#include "deformation/ragged_grid_data.h"
#include "deformation/ragged_layer.h"
#include "deformation/scalar_grid_data.h"
#include "deformation/scalar_layer.h"
#include "deformation/time_function.h"

namespace deformation {

namespace {

// The boundary slop this format's own spatial indexing uses for every grid
// - not a stored file attribute (every grid shares this one fixed value),
// see GridGeometry's own cellTolerance parameter.
constexpr double kCellTolerance = 0.00001;

// The fixed, metadata-free scale a linear ragged segment's own raw
// segment_start_value applies (mm per raw unit) - not a netCDF scale_factor
// attribute; compound-type data has no such attribute here, so both the
// producer and every reader hardcode the same value.
constexpr double kSegmentStartValueScale = 0.01;

// The fixed, metadata-free per-day rate scale a linear ragged segment's own
// raw segment_scale_factor applies (mm per day per raw unit) - same
// rationale as kSegmentStartValueScale above. Written as a fraction of
// 365.2425 purely because that is how this constant's own magnitude is
// conventionally stated (an annual rate divided down to a daily one); Δ
// itself is always a plain day count, never a calendar year, and no value
// anywhere in this format's own stored data is ever "per year" at read
// time - only this fixed constant's own derivation happens to reference a
// year length.
constexpr double kSegmentScaleFactorLinearScale = 0.01 / 365.2425;

// This format's fixed east/north/up displacement-axis component letters,
// in order - "up" is only present for a 3-axis (rather than 2-axis) model.
constexpr char kComponentLetters[3] = { 'e', 'n', 'u' };

// This format's fixed displacement-component names, in canonical order -
// GGRXF's own `gridParameters` layer attribute names a subset (and
// possibly a different order) of these; index into this array is the
// canonical component index (0 = east, 1 = north, 2 = up) every
// ScalarGridData's own result array is indexed by, regardless of a
// particular grid's own raw file-axis order. Only `scalar_with_time_function`
// grids need this - a `ragged_segments` grid's own axis identity comes
// from its own segment_count_{e,n,u}/segments_{e,n,u} variable names
// directly (see kComponentLetters above and raggedAxisCount() below), not
// from any orderable list, so there is nothing to remap there (confirmed
// directly against lib/ggrxf/reader.py's own `_evaluate_ragged_grid`,
// which always pairs e/n/u fixed, never consulting `gridParameters`).
constexpr std::array<const char *, 3> kCanonicalParameterNames = {
    "displacementEast", "displacementNorth", "displacementUp"
};

// A scalar_with_time_function layer's own `gridParameters` (GGRXF's own
// "Layer attributes"), resolved to a raw-file-axis -> canonical-component
// mapping every grid under this layer shares (gridParameters is declared
// once per layer, so every grid underneath necessarily agrees - see
// ScalarLayer's own class comment).
//
// The returned vector is indexed by raw file axis (the order `displacement`'s
// own third dimension actually stores values in, the same order
// gridParameters itself lists them in); each entry's own value is the
// canonical component index (0 = east, 1 = north, 2 = up, per
// kCanonicalParameterNames above) that raw axis should be written to. For
// example, gridParameters = ["displacementUp", "displacementEast"] resolves
// to {2, 0}: raw axis 0 (up) belongs at canonical slot 2, raw axis 1 (east)
// at canonical slot 0 - this is the permutation NetcdfScalarNodeValueSource
// applies when placing each raw node value into its own result array (see
// its own constructor comment).
//
// Defaults to every component in canonical order ({0, 1, 2}) if the
// attribute is absent, GGRXF's own documented default. Throws, naming the
// offending value, if any listed name isn't one of the 3 real component
// names - a malformed file, not something to silently reinterpret.
std::vector<std::size_t> scalarCanonicalAxisIndices( const netcdf::NetcdfGroup &layerGroup )
{
    std::vector<std::string> names;
    if( layerGroup.hasAttribute( "gridParameters" ) ) {
        names = layerGroup.readStringListAttribute( "gridParameters" );
    } else {
        names.assign( kCanonicalParameterNames.begin(), kCanonicalParameterNames.end() );
    }

    std::vector<std::size_t> indices;
    indices.reserve( names.size() );
    for( const auto &name : names ) {
        const auto it = std::find( kCanonicalParameterNames.begin(), kCanonicalParameterNames.end(), name );
        if( it == kCanonicalParameterNames.end() ) {
            throw std::runtime_error(
                "ggrxf_reader: '" + layerGroup.name() + "'.gridParameters lists '" + name
                + "', expected one of displacementEast/displacementNorth/displacementUp" );
        }
        indices.push_back( static_cast<std::size_t>( it - kCanonicalParameterNames.begin() ) );
    }
    return indices;
}

// The prefix every grid group this format's own writer names its groups
// with ("grid_<N>") - see gridGroupOrderIndex() below.
const std::string kGridGroupPrefix = "grid_";

// The <N> in gridGroup's own name ("grid_<N>") - what
// GGRXF_DEFORMATION_FORMAT.md defines "file order" to mean for a layer's
// own root grids (see rootGridGroupsInFileOrder() below): ascending numeric
// order by this index, not whatever order a given netCDF-4/HDF5 library's
// own group-iteration API happens to return, which the spec explicitly
// does not guarantee reflects write order. Throws if gridGroup's own name
// doesn't match the expected "grid_<N>" pattern - every grid group this
// format's own writer produces is named this way, so a mismatch means a
// malformed file or a parser assumption violated elsewhere.
std::uint64_t gridGroupOrderIndex( const netcdf::NetcdfGroup &gridGroup )
{
    const std::string &name = gridGroup.name();
    if( name.compare( 0, kGridGroupPrefix.size(), kGridGroupPrefix ) != 0 ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + name + "' does not match the expected '" + kGridGroupPrefix + "<N>' naming convention" );
    }
    return std::stoull( name.substr( kGridGroupPrefix.size() ) );
}

// A layer's own root grid subgroups, sorted into the file order
// gridGroupOrderIndex() defines - root grids are resolved by first-match in
// that order (see ScalarLayer's/RaggedLayer's own class comments), so this
// order is semantically meaningful, unlike a GridParent's own descendant
// collection (collectScalarDescendants()/collectRaggedDescendants() below),
// which is re-sorted by gridPriority afterward regardless of the order it's
// collected in.
std::vector<netcdf::NetcdfGroup> rootGridGroupsInFileOrder( const netcdf::NetcdfGroup &layerGroup )
{
    auto groups = layerGroup.subgroups();
    std::sort( groups.begin(), groups.end(), []( const netcdf::NetcdfGroup &a, const netcdf::NetcdfGroup &b ) {
        return gridGroupOrderIndex( a ) < gridGroupOrderIndex( b );
    } );
    return groups;
}

// Reads and validates one grid group's own affineCoeffs attribute (node
// (i, j) <-> world (c0, c1) - see GridGeometry's own constructor for what
// each element means). Throws if the file's own attribute isn't exactly
// length 6 - a malformed file, or this reader misreading it, either way
// not safe to proceed past silently.
std::array<double, 6> readAffineCoeffs( const netcdf::NetcdfGroup &gridGroup )
{
    const auto raw = gridGroup.readNumericAttribute<double>( "affineCoeffs" );
    if( raw.size() != 6 ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "'.affineCoeffs has " + std::to_string( raw.size() )
            + " elements, expected 6" );
    }
    std::array<double, 6> coeffs{};
    std::copy( raw.begin(), raw.end(), coeffs.begin() );
    return coeffs;
}

// One collected descendant of a nest root (see collectScalarDescendants()/
// collectRaggedDescendants() below): the leaf grid itself (its own on-disk
// nesting, if any, already discarded - it was walked, not preserved), the
// gridPriority that placed it here, and its own group name, kept only for
// error messages if validateAndAddDescendants() below rejects it.
template <typename NodeData>
struct DescendantEntry
{
    std::unique_ptr<Grid<NodeData>> grid;
    std::uint8_t priority;
    std::string name;
};

// Reads gridGroup's own gridPriority, throwing (naming gridGroup) if it's
// absent - every descendant of a nest root is required to carry one (see
// GGRXF_CPP_READER_PLAN.md's own grid-priority design notes), unlike a
// root's own optional attribute (see the "no subgroups" case in
// parseScalarRootGrid()/parseRaggedRootGrid() below).
std::uint8_t readRequiredGridPriority( const netcdf::NetcdfGroup &gridGroup )
{
    if( ! gridGroup.hasAttribute( "gridPriority" ) ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "' has no gridPriority - required for every descendant of a nested grid" );
    }
    return gridGroup.readNumericAttribute<std::uint8_t>( "gridPriority" )[0];
}

// Validates `descendants`' own gridPriority values are exactly the
// consecutive set {1, ..., descendants.size()} (throwing, naming the
// offending descendant and value, on a gap/duplicate/out-of-range entry),
// then sorts them ascending by that value and addChild()s each into `root`
// in that order - the one gridPriority value a caller (root) never reads or
// stores itself, only its resulting list position (see GridParent's own
// class comment: children_ is held in ascending order, read once here and
// never again). Shared by parseScalarRootGrid()/parseRaggedRootGrid(),
// since this validation/ordering step is identical for both grid types.
template <typename NodeData>
void validateAndAddDescendants( GridParent<NodeData> &root, std::vector<DescendantEntry<NodeData>> descendants )
{
    const std::size_t childCount = descendants.size();
    std::vector<bool> seen( childCount + 1, false );   // index 0 unused - valid priorities here start at 1
    for( const auto &entry : descendants ) {
        if( entry.priority == 0 || entry.priority > childCount ) {
            throw std::runtime_error(
                "ggrxf_reader: '" + entry.name + "' has gridPriority " + std::to_string( entry.priority )
                + ", expected within [1, " + std::to_string( childCount ) + "] for this " + std::to_string( childCount )
                + "-descendant nest" );
        }
        if( seen[entry.priority] ) {
            throw std::runtime_error(
                "ggrxf_reader: '" + entry.name + "' has gridPriority " + std::to_string( entry.priority )
                + ", already used by another descendant of the same nest" );
        }
        seen[entry.priority] = true;
    }

    std::sort( descendants.begin(), descendants.end(),
               []( const auto &a, const auto &b ) { return a.priority < b.priority; } );
    for( auto &entry : descendants ) {
        root.addChild( std::move( entry.grid ) );
    }
}

// One scalar_with_time_function grid group's own geometry and node data -
// everything parseScalarLeafGrid()/parseScalarRootGrid() below need to
// construct either a plain Grid or a GridParent, before either one decides
// what (if anything) to do about this group's own subgroups.
struct ScalarGridContent
{
    GridGeometry geometry;
    ScalarGridData data;
};

// Reads gridGroup's own geometry (affineCoeffs/iNodeCount/jNodeCount) and
// its own displacement array in full (once), constructing a
// NetcdfScalarNodeValueSource to hold it - never recurses into gridGroup's
// own subgroups, see ScalarGridContent's own doc comment above for why that
// is a separate, caller-side decision. canonicalAxisIndices is this grid's
// own layer's own gridParameters resolved to a raw-file-axis ->
// canonical-component mapping (see scalarCanonicalAxisIndices() above) -
// one entry per raw file axis, in raw file-axis order.
ScalarGridContent parseScalarGridContent(
    const netcdf::NetcdfGroup &gridGroup, const std::vector<std::size_t> &canonicalAxisIndices )
{
    const auto affineCoeffs = readAffineCoeffs( gridGroup );
    const std::uint32_t iNodeCount = gridGroup.readNumericAttribute<std::uint32_t>( "iNodeCount" )[0];
    const std::uint32_t jNodeCount = gridGroup.readNumericAttribute<std::uint32_t>( "jNodeCount" )[0];
    GridGeometry geometry( affineCoeffs, iNodeCount, jNodeCount, kCellTolerance );

    auto displacementVar = gridGroup.variable( "displacement" );
    const auto dims = displacementVar.dimensionLengths();
    if( dims.size() != 3 ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "'.displacement has " + std::to_string( dims.size() )
            + " dimensions, expected 3" );
    }
    // This grid's own displacement-axis count comes from the file's own
    // stored array; canonicalAxisIndices comes from this grid's own
    // layer's own gridParameters attribute - a separately-sourced count
    // that must still agree, since NetcdfScalarNodeValueSource pairs raw
    // axis position with canonicalAxisIndices position-for-position.
    const std::size_t axisCount = dims[2];
    if( axisCount != canonicalAxisIndices.size() ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "'.displacement has " + std::to_string( axisCount )
            + " axes, but this layer's own gridParameters lists " + std::to_string( canonicalAxisIndices.size() ) );
    }

    const double scaleFactor = displacementVar.readNumericAttribute<double>( "scale_factor" )[0];
    const std::optional<std::int32_t> missingValue = displacementVar.hasAttribute( "missing_value" )
        ? std::optional<std::int32_t>( displacementVar.readNumericAttribute<std::int32_t>( "missing_value" )[0] )
        : std::nullopt;
    const bool hasEndDay = gridGroup.hasVariable( "has_end_day" )
        && gridGroup.variable( "has_end_day" ).readValues<std::uint8_t>()[0] != 0;

    auto valueSource = std::make_unique<NetcdfScalarNodeValueSource>(
        jNodeCount, canonicalAxisIndices, displacementVar.readValues<std::int32_t>(), missingValue, hasEndDay );
    return ScalarGridContent{ std::move( geometry ), ScalarGridData( scaleFactor, std::move( valueSource ) ) };
}

// Parses gridGroup's own geometry/data only, as a plain leaf Grid - used
// both for a non-nested root grid, and for every descendant collected by
// collectScalarDescendants() below (whose own on-disk subgroups, if any,
// are walked separately by that same function, never preserved as this
// leaf's own children).
std::unique_ptr<Grid<ScalarGridData>> parseScalarLeafGrid(
    const netcdf::NetcdfGroup &gridGroup, const std::vector<std::size_t> &canonicalAxisIndices )
{
    auto content = parseScalarGridContent( gridGroup, canonicalAxisIndices );
    return std::make_unique<Grid<ScalarGridData>>( content.geometry, std::move( content.data ) );
}

// Recursively collects every descendant of gridGroup - at every depth, not
// just its own direct subgroups - into `descendants`, each paired with its
// own required gridPriority (see readRequiredGridPriority()). Whatever
// on-disk nesting a descendant itself carries is walked here (so nothing is
// missed) but never preserved on the leaf Grid parseScalarLeafGrid()
// constructs for it - see GGRXF_CPP_READER_PLAN.md's own grid-priority
// design notes for why a nest's whole subtree flattens into one
// GridParent's own children_, not a multi-level Grid<T> tree.
void collectScalarDescendants(
    const netcdf::NetcdfGroup &gridGroup, std::vector<DescendantEntry<ScalarGridData>> &descendants,
    const std::vector<std::size_t> &canonicalAxisIndices )
{
    for( const auto &childGroup : gridGroup.subgroups() ) {
        descendants.push_back( DescendantEntry<ScalarGridData>{
            parseScalarLeafGrid( childGroup, canonicalAxisIndices ), readRequiredGridPriority( childGroup ),
            childGroup.name() } );
        collectScalarDescendants( childGroup, descendants, canonicalAxisIndices );
    }
}

// Parses one of a scalar_with_time_function layer's own root grid
// subgroups. A root with no subgroups of its own is a plain leaf Grid, and
// must carry no gridPriority (throws if it does - a non-nested grid has no
// nest for that value to mean anything within). A root with subgroups is a
// nest root: must carry gridPriority exactly 0 (throws otherwise), becomes
// a GridParent, and its entire subtree - flattened via
// collectScalarDescendants() and validateAndAddDescendants() - becomes its
// own children_, in ascending gridPriority order.
std::unique_ptr<Grid<ScalarGridData>> parseScalarRootGrid(
    const netcdf::NetcdfGroup &gridGroup, const std::vector<std::size_t> &canonicalAxisIndices )
{
    if( gridGroup.subgroups().empty() ) {
        if( gridGroup.hasAttribute( "gridPriority" ) ) {
            throw std::runtime_error(
                "ggrxf_reader: '" + gridGroup.name() + "' has no child grids but carries gridPriority" );
        }
        return parseScalarLeafGrid( gridGroup, canonicalAxisIndices );
    }

    if( ! gridGroup.hasAttribute( "gridPriority" ) ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "' has child grids but no gridPriority (expected 0)" );
    }
    const std::uint8_t rootPriority = gridGroup.readNumericAttribute<std::uint8_t>( "gridPriority" )[0];
    if( rootPriority != 0 ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "' is a nest root with gridPriority "
            + std::to_string( rootPriority ) + ", expected 0" );
    }

    auto content = parseScalarGridContent( gridGroup, canonicalAxisIndices );
    auto root = std::make_unique<GridParent<ScalarGridData>>( content.geometry, std::move( content.data ) );

    std::vector<DescendantEntry<ScalarGridData>> descendants;
    collectScalarDescendants( gridGroup, descendants, canonicalAxisIndices );
    validateAndAddDescendants( *root, std::move( descendants ) );
    return root;
}

// Reads layerGroup's own timeFunctions (stored as separate, flat,
// dot-named attributes - timeFunctions.count, then timeFunctions.<n>.* per
// entry, not a single compound-type attribute) into layer, one
// TimeFunction per entry. Throws if any entry's own functionType isn't
// linear/step/ramp - the only shapes TimeFunction itself implements.
void parseTimeFunctions( const netcdf::NetcdfGroup &layerGroup, ScalarLayer &layer )
{
    const std::uint8_t count = layerGroup.readNumericAttribute<std::uint8_t>( "timeFunctions.count" )[0];
    for( std::uint8_t index = 0; index < count; ++index ) {
        const std::string prefix = "timeFunctions." + std::to_string( index ) + ".";
        const std::string functionType = layerGroup.readTextAttribute( prefix + "functionType" );

        TimeFunction::Params params;
        if( functionType == "linear" ) {
            params.shape = TimeFunctionShape::Linear;
        } else if( functionType == "step" ) {
            params.shape = TimeFunctionShape::Step;
        } else if( functionType == "ramp" ) {
            params.shape = TimeFunctionShape::Ramp;
        } else {
            throw std::runtime_error(
                "ggrxf_reader: " + prefix + "functionType '" + functionType + "' is not implemented (only linear/"
                "step/ramp are)" );
        }
        params.scaleFactor = layerGroup.readNumericAttribute<double>( prefix + "scaleFactor" )[0];
        // Every date field below is independently optional in the file
        // (which ones a given entry actually needs depends on its own
        // functionType) - TimeFunction's own constructor validates that
        // whichever ones its own shape requires were actually supplied, so
        // this just passes through whatever is present without checking
        // that itself.
        if( layerGroup.hasAttribute( prefix + "functionReferenceDate" ) ) {
            params.functionReferenceDateDays =
                ggrxfDateToDayNumber( layerGroup.readTextAttribute( prefix + "functionReferenceDate" ) );
        }
        if( layerGroup.hasAttribute( prefix + "startDate" ) ) {
            params.startDateDays = ggrxfDateToDayNumber( layerGroup.readTextAttribute( prefix + "startDate" ) );
        }
        if( layerGroup.hasAttribute( prefix + "endDate" ) ) {
            params.endDateDays = ggrxfDateToDayNumber( layerGroup.readTextAttribute( prefix + "endDate" ) );
        }
        if( layerGroup.hasAttribute( prefix + "eventDate" ) ) {
            params.eventDateDays = ggrxfDateToDayNumber( layerGroup.readTextAttribute( prefix + "eventDate" ) );
        }
        layer.addTimeFunction( TimeFunction( params ) );
    }
}

// Parses one scalar_with_time_function layer group: its own timeFunctions,
// its own gridParameters (resolved once here, since it's a layer-level
// attribute every grid under this layer shares - see
// scalarCanonicalAxisIndices() above), then every root grid subgroup
// beneath it, in file order (root grids are resolved by first-match, never
// by gridPriority - see ScalarLayer's own class comment - so no reordering
// happens here, unlike a nest root's own children, see parseScalarRootGrid()
// above).
std::unique_ptr<Layer> parseScalarLayer( const netcdf::NetcdfGroup &layerGroup )
{
    auto layer = std::make_unique<ScalarLayer>();
    parseTimeFunctions( layerGroup, *layer );
    const auto canonicalAxisIndices = scalarCanonicalAxisIndices( layerGroup );
    for( const auto &rootGroup : rootGridGroupsInFileOrder( layerGroup ) ) {
        layer->addRootGrid( parseScalarRootGrid( rootGroup, canonicalAxisIndices ) );
    }
    return layer;
}

// This format always requires segment_count_e/n; segment_count_u is only
// present for a 3-axis model. Throws if even e/n are missing - a
// ragged_segments grid with neither is malformed.
std::size_t raggedAxisCount( const netcdf::NetcdfGroup &gridGroup )
{
    if( ! gridGroup.hasVariable( "segment_count_e" ) || ! gridGroup.hasVariable( "segment_count_n" ) ) {
        throw std::runtime_error( "ggrxf_reader: '" + gridGroup.name() + "' is missing segment_count_e/n" );
    }
    return gridGroup.hasVariable( "segment_count_u" ) ? 3 : 2;
}

// One ragged_segments grid group's own geometry and node data - the ragged
// sibling to ScalarGridContent above, same rationale.
struct RaggedGridContent
{
    GridGeometry geometry;
    RaggedGridData data;
};

// Reads gridGroup's own geometry, its own per-node reference_day/
// has_end_day, and for each displacement axis this grid carries: the
// per-node segment_count offsets (reconstructed once here, not per query),
// the segments_{e,n,u} compound records, and the paired
// function_type_{e,n,u} array (every entry must be linear/code 0 - the only
// ragged segment shape RaggedGridData implements). Every parsed segment is
// combined into a single, already-scaled RaggedSegment (see RaggedSegment's
// own documentation for why startDay is already the combined reference_day
// + day_delta rather than the two kept apart). Never recurses into
// gridGroup's own subgroups - same rationale as ScalarGridContent above.
RaggedGridContent parseRaggedGridContent( const netcdf::NetcdfGroup &gridGroup )
{
    const auto affineCoeffs = readAffineCoeffs( gridGroup );
    const std::uint32_t iNodeCount = gridGroup.readNumericAttribute<std::uint32_t>( "iNodeCount" )[0];
    const std::uint32_t jNodeCount = gridGroup.readNumericAttribute<std::uint32_t>( "jNodeCount" )[0];
    GridGeometry geometry( affineCoeffs, iNodeCount, jNodeCount, kCellTolerance );

    const std::size_t nodeCount = static_cast<std::size_t>( iNodeCount ) * jNodeCount;
    const std::size_t axisCount = raggedAxisCount( gridGroup );

    const auto referenceDay = gridGroup.variable( "reference_day" ).readValues<std::uint32_t>();

    std::vector<bool> hasEndDayPerNode;
    if( gridGroup.hasVariable( "has_end_day" ) ) {
        const auto raw = gridGroup.variable( "has_end_day" ).readValues<std::uint8_t>();
        hasEndDayPerNode.resize( raw.size() );
        for( std::size_t node = 0; node < raw.size(); ++node ) {
            hasEndDayPerNode[node] = raw[node] != 0;
        }
    }

    // One flat, pre-sized std::vector<RaggedSegment> per axis (built below),
    // rather than one per node - see NetcdfRaggedNodeValueSource's own class
    // comment for why (a real, measured heap-fragmentation problem the
    // one-vector-per-node shape had).
    std::vector<std::vector<std::size_t>> segmentOffsetsByAxis( axisCount );
    std::vector<std::vector<RaggedSegment>> segmentsByAxis( axisCount );

    for( std::size_t axis = 0; axis < axisCount; ++axis ) {
        const std::string suffix( 1, kComponentLetters[axis] );
        const auto counts = gridGroup.variable( "segment_count_" + suffix ).readValues<std::uint16_t>();

        // The CSR-style offsets into segments_{suffix}/function_type_{suffix}
        // for each node's own slice - a running sum of counts,
        // reconstructed once per (grid, axis) rather than per query.
        std::vector<std::size_t> offsets( nodeCount + 1, 0 );
        for( std::size_t node = 0; node < nodeCount; ++node ) {
            offsets[node + 1] = offsets[node] + counts[node];
        }

        const auto startValues =
            gridGroup.variable( "segments_" + suffix ).readCompoundField<std::int32_t>( "segment_start_value" );
        const auto dayDeltas = gridGroup.variable( "segments_" + suffix ).readCompoundField<std::uint16_t>( "day_delta" );
        const auto scaleFactors =
            gridGroup.variable( "segments_" + suffix ).readCompoundField<std::int16_t>( "segment_scale_factor" );
        const auto functionTypes = gridGroup.variable( "function_type_" + suffix ).readValues<std::uint8_t>();

        // offsets[nodeCount] is the total segment count for this axis - the
        // whole buffer is allocated once, at that exact size, then filled
        // node by node in place (no per-node allocation, no reallocation).
        std::vector<RaggedSegment> flatSegments( offsets[nodeCount] );
        for( std::size_t node = 0; node < nodeCount; ++node ) {
            for( std::size_t k = offsets[node]; k < offsets[node + 1]; ++k ) {
                if( functionTypes[k] != 0 ) {
                    throw std::runtime_error(
                        "ggrxf_reader: function_type_" + suffix + " code " + std::to_string( functionTypes[k] )
                        + " is not implemented (only linear, code 0, is)" );
                }
                RaggedSegment &segment = flatSegments[k];
                segment.startDay = static_cast<std::int64_t>( referenceDay[node] ) + static_cast<std::int64_t>( dayDeltas[k] );
                segment.startValue = static_cast<double>( startValues[k] ) * kSegmentStartValueScale;
                segment.ratePerDay = static_cast<double>( scaleFactors[k] ) * kSegmentScaleFactorLinearScale;
            }
        }
        segmentOffsetsByAxis[axis] = std::move( offsets );
        segmentsByAxis[axis] = std::move( flatSegments );
    }

    auto valueSource = std::make_unique<NetcdfRaggedNodeValueSource>(
        jNodeCount, axisCount, std::move( hasEndDayPerNode ), std::move( segmentOffsetsByAxis ), std::move( segmentsByAxis ) );
    return RaggedGridContent{ std::move( geometry ), RaggedGridData( std::move( valueSource ) ) };
}

// Parses gridGroup's own geometry/data only, as a plain leaf Grid - the
// ragged sibling to parseScalarLeafGrid() above, same rationale.
std::unique_ptr<Grid<RaggedGridData>> parseRaggedLeafGrid( const netcdf::NetcdfGroup &gridGroup )
{
    auto content = parseRaggedGridContent( gridGroup );
    return std::make_unique<Grid<RaggedGridData>>( content.geometry, std::move( content.data ) );
}

// The ragged sibling to collectScalarDescendants() above, same rationale.
void collectRaggedDescendants(
    const netcdf::NetcdfGroup &gridGroup, std::vector<DescendantEntry<RaggedGridData>> &descendants )
{
    for( const auto &childGroup : gridGroup.subgroups() ) {
        descendants.push_back( DescendantEntry<RaggedGridData>{
            parseRaggedLeafGrid( childGroup ), readRequiredGridPriority( childGroup ), childGroup.name() } );
        collectRaggedDescendants( childGroup, descendants );
    }
}

// The ragged sibling to parseScalarRootGrid() above, same rationale.
std::unique_ptr<Grid<RaggedGridData>> parseRaggedRootGrid( const netcdf::NetcdfGroup &gridGroup )
{
    if( gridGroup.subgroups().empty() ) {
        if( gridGroup.hasAttribute( "gridPriority" ) ) {
            throw std::runtime_error(
                "ggrxf_reader: '" + gridGroup.name() + "' has no child grids but carries gridPriority" );
        }
        return parseRaggedLeafGrid( gridGroup );
    }

    if( ! gridGroup.hasAttribute( "gridPriority" ) ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "' has child grids but no gridPriority (expected 0)" );
    }
    const std::uint8_t rootPriority = gridGroup.readNumericAttribute<std::uint8_t>( "gridPriority" )[0];
    if( rootPriority != 0 ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + gridGroup.name() + "' is a nest root with gridPriority "
            + std::to_string( rootPriority ) + ", expected 0" );
    }

    auto content = parseRaggedGridContent( gridGroup );
    auto root = std::make_unique<GridParent<RaggedGridData>>( content.geometry, std::move( content.data ) );

    std::vector<DescendantEntry<RaggedGridData>> descendants;
    collectRaggedDescendants( gridGroup, descendants );
    validateAndAddDescendants( *root, std::move( descendants ) );
    return root;
}

// Parses one ragged_segments layer group: its own reference_date, then
// every root grid subgroup beneath it - added to the layer sorted by
// descending size (iNodeCount), not file order. Real files never need this
// reordering to matter for anything else (RaggedLayer's own class comment:
// list order plays no role in root-grid resolution) - it exists purely to
// fix a real bug in RaggedLayer's own root-grid index, confirmed directly
// against a real file, explained here since it isn't obvious from
// RaggedLayer's own code alone.
//
// RaggedLayer looks up a root grid of size S at coordinate c via:
//   rawRow = (c - refC0Origin_) / refC0Step_
//   primaryRow0 = floor(rawRow / S) * S
// then looks for a grid stored under (primaryRow0, S) - see
// RaggedLayer::resolveRootGrid(). floor(x / S) * S only ever produces an
// exact multiple of S, so a grid is only ever found if its own row0
// (computed the same way, at index time - see RaggedLayer::indexRootGrid())
// is itself an exact multiple of its own size S. refC0Origin_ is fixed,
// once, to whichever root grid happens to be added to the layer first -
// so which grid gets picked as that reference changes whether every OTHER
// grid's own row0 comes out as a multiple of its own size.
//
// Concrete example: two real, non-overlapping root grids on the same axis
// - grid A, corner at 44.0 (in c0's own units), size 4 steps (covers
// 44.0-48.0); grid B, corner at 128.0, size 128 steps (covers
// 128.0-256.0).
//
// If A is added first (reference = 44.0): A's own row0 = (44.0-44.0)/step
// = 0, a multiple of 4 - fine. B's own row0 = (128.0-44.0)/step = 84, not
// a multiple of 128 - so no query anywhere inside B can ever compute a
// primaryRow0 equal to 84 (only 0, 128, 256, ... are ever produced for
// size 128) - B is never found, for any query point inside it.
//
// If B is added first instead (reference = 128.0): B's own row0 =
// (128.0-128.0)/step = 0, a multiple of 128 - fine. A's own row0 =
// (44.0-128.0)/step = -84, a multiple of 4 - fine too. A query at 150.0
// (inside B) computes primaryRow0 = floor((150.0-128.0)/128)*128 = 0,
// matching B's own stored (0, 128) - found. A query at 45.0 (inside A)
// computes, for size 128, primaryRow0 = floor((45.0-128.0)/128)*128 =
// -128 - nothing stored there, so size 4 is tried next: primaryRow0 =
// floor((45.0-128.0)/4)*4 = -84, matching A's own stored (-84, 4) - found.
//
// So B (the larger grid) works as the reference for both grids; A (the
// smaller one) does not. Sorting root grids descending by size before
// adding them makes the largest one always the reference.
std::unique_ptr<Layer> parseRaggedLayer( const netcdf::NetcdfGroup &layerGroup )
{
    const double referenceDateDays = ggrxfDateToDayNumber( layerGroup.readTextAttribute( "reference_date" ) );
    auto layer = std::make_unique<RaggedLayer>( referenceDateDays );

    std::vector<std::unique_ptr<Grid<RaggedGridData>>> rootGrids;
    for( const auto &rootGroup : rootGridGroupsInFileOrder( layerGroup ) ) {
        rootGrids.push_back( parseRaggedRootGrid( rootGroup ) );
    }
    std::sort( rootGrids.begin(), rootGrids.end(), []( const auto &a, const auto &b ) {
        return a->geometry().iNodeCount() > b->geometry().iNodeCount();
    } );
    for( auto &rootGrid : rootGrids ) {
        layer->addRootGrid( std::move( rootGrid ) );
    }
    return layer;
}

// Parses one top-level layer group: validates interpolationMethod (must be
// bilinear - the only method Grid's own resolution/interpolation
// implements), then dispatches on whether this layer carries a
// reference_date - present only for a ragged_segments layer, absent only
// for a scalar_with_time_function one; a real file's own layers are never
// a mix of both grid types.
std::unique_ptr<Layer> parseLayer( const netcdf::NetcdfGroup &layerGroup )
{
    const std::string interpolationMethod = layerGroup.readTextAttribute( "interpolationMethod" );
    if( interpolationMethod != "bilinear" ) {
        throw std::runtime_error(
            "ggrxf_reader: '" + layerGroup.name() + "'.interpolationMethod '" + interpolationMethod
            + "' is not implemented (only bilinear is)" );
    }

    if( layerGroup.hasAttribute( "reference_date" ) ) {
        return parseRaggedLayer( layerGroup );
    }
    return parseScalarLayer( layerGroup );
}

} // namespace

std::vector<std::unique_ptr<Layer>> parseGgrxfFile( const std::string &path )
{
    netcdf::NetcdfFile file( path );
    std::vector<std::unique_ptr<Layer>> layers;
    for( const auto &layerGroup : file.rootGroup().subgroups() ) {
        layers.push_back( parseLayer( layerGroup ) );
    }
    return layers;
}

} // namespace deformation
