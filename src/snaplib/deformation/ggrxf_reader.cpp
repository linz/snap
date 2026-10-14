#include "deformation/ggrxf_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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
constexpr char kComponentLetters[3] = {'e', 'n', 'u'};

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
constexpr std::array<const char*, 3> kCanonicalParameterNames = {"displacementEast", "displacementNorth",
                                                                 "displacementUp"};

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
std::vector<std::size_t> scalarCanonicalAxisIndices(const netcdf::NetcdfGroup& layerGroup) {
    std::vector<std::string> names;
    if (layerGroup.hasAttribute("gridParameters")) {
        names = layerGroup.readStringListAttribute("gridParameters");
    } else {
        names.assign(kCanonicalParameterNames.begin(), kCanonicalParameterNames.end());
    }

    std::vector<std::size_t> indices;
    indices.reserve(names.size());
    for (const auto& name : names) {
        const auto it = std::find(kCanonicalParameterNames.begin(), kCanonicalParameterNames.end(), name);
        if (it == kCanonicalParameterNames.end()) {
            throw std::runtime_error("ggrxf_reader: '" + layerGroup.name() + "'.gridParameters lists '" + name +
                                     "', expected one of displacementEast/displacementNorth/displacementUp");
        }
        indices.push_back(static_cast<std::size_t>(it - kCanonicalParameterNames.begin()));
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
std::uint64_t gridGroupOrderIndex(const netcdf::NetcdfGroup& gridGroup) {
    const std::string& name = gridGroup.name();
    if (name.compare(0, kGridGroupPrefix.size(), kGridGroupPrefix) != 0) {
        throw std::runtime_error("ggrxf_reader: '" + name + "' does not match the expected '" + kGridGroupPrefix +
                                 "<N>' naming convention");
    }
    return std::stoull(name.substr(kGridGroupPrefix.size()));
}

// A layer's own root grid subgroups, sorted into the file order
// gridGroupOrderIndex() defines - root grids are resolved by first-match in
// that order (see ScalarLayer's/RaggedLayer's own class comments), so this
// order is semantically meaningful, unlike a GridParent's own descendant
// collection (collectScalarDescendants()/collectRaggedDescendants() below),
// which is re-sorted by gridPriority afterward regardless of the order it's
// collected in.
std::vector<netcdf::NetcdfGroup> rootGridGroupsInFileOrder(const netcdf::NetcdfGroup& layerGroup) {
    auto groups = layerGroup.subgroups();
    std::sort(groups.begin(), groups.end(), [](const netcdf::NetcdfGroup& a, const netcdf::NetcdfGroup& b) {
        return gridGroupOrderIndex(a) < gridGroupOrderIndex(b);
    });
    return groups;
}

// Determines whether interpolationCrsWkt (GGRXF_DEFORMATION_FORMAT.md
// "Container": real OGC WKT text for the CRS affineCoeffs coordinates are
// expressed in) declares its geodetic latitude axis before its geodetic
// longitude axis - GGXF's own convention for reading a file's own (c0, c1)
// identity from a CRS WKT's AXIS[...] clause order (GGXF's own
// nodeCoordinateParametersFromCrsWkt), applied here as a plain substring
// search for each axis clause's own literal name, not a general CRS WKT
// grammar parser - this reader only ever needs to distinguish these two
// specific axis names, not parse arbitrary CRS WKT text. Throws if either
// axis name is absent - an unsupported or malformed interpolation CRS, not
// something to guess an order for.
bool wktDeclaresLatitudeFirst(const std::string& interpolationCrsWkt) {
    const auto latitudePos = interpolationCrsWkt.find("geodetic latitude");
    const auto longitudePos = interpolationCrsWkt.find("geodetic longitude");
    if (latitudePos == std::string::npos || longitudePos == std::string::npos) {
        throw std::runtime_error(
            "ggrxf_reader: interpolationCrsWkt does not declare both a geodetic latitude axis and "
            "a geodetic longitude axis");
    }
    return latitudePos < longitudePos;
}

// Reorders one grid's own affineCoeffs so its (c0, c1) is always (latitude,
// longitude), regardless of the axis order interpolationCrsWkt actually
// declares for this file - swaps the two axes' own coefficient rows
// ([0..2], [3..5]) when latitudeFirst is false, otherwise leaves coeffs
// unchanged. Applied once, here, at parse time, rather than carrying axis
// order as a fact every downstream caller (rf_ggrxf_calc, GridGeometry)
// would otherwise have to separately account for.
void canonicalizeAffineCoeffsAxisOrder(std::array<double, 6>& coeffs, bool latitudeFirst) {
    if (latitudeFirst) {
        return;
    }
    for (std::size_t k = 0; k < 3; ++k) {
        std::swap(coeffs[k], coeffs[k + 3]);
    }
}

// Reads and validates one grid group's own affineCoeffs attribute (node
// (i, j) <-> world (c0, c1) - see GridGeometry's own constructor for what
// each element means), canonicalized to (latitude, longitude) axis order
// (see canonicalizeAffineCoeffsAxisOrder() above). Throws if the file's own
// attribute isn't exactly length 6 - a malformed file, or this reader
// misreading it, either way not safe to proceed past silently.
std::array<double, 6> readAffineCoeffs(const netcdf::NetcdfGroup& gridGroup, bool latitudeFirst) {
    const auto raw = gridGroup.readNumericAttribute<double>("affineCoeffs");
    if (raw.size() != 6) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() + "'.affineCoeffs has " +
                                 std::to_string(raw.size()) + " elements, expected 6");
    }
    std::array<double, 6> coeffs{};
    std::copy(raw.begin(), raw.end(), coeffs.begin());
    canonicalizeAffineCoeffsAxisOrder(coeffs, latitudeFirst);
    return coeffs;
}

// One collected descendant of a nest root (see collectScalarDescendants()/
// collectRaggedDescendants() below): the leaf grid itself (its own on-disk
// nesting, if any, already discarded - it was walked, not preserved), the
// gridPriority that placed it here, and its own group name, kept only for
// error messages if validateAndAddDescendants() below rejects it.
template <typename NodeData>
struct DescendantEntry {
    std::unique_ptr<Grid<NodeData>> grid;
    std::uint8_t priority;
    std::string name;
};

// Reads gridGroup's own gridPriority, throwing (naming gridGroup) if it's
// absent - every descendant of a nest root is required to carry one (see
// GGRXF_CPP_READER_PLAN.md's own grid-priority design notes), unlike a
// root's own optional attribute (see the "no subgroups" case in
// parseScalarRootGrid()/parseRaggedRootGrid() below).
std::uint8_t readRequiredGridPriority(const netcdf::NetcdfGroup& gridGroup) {
    if (!gridGroup.hasAttribute("gridPriority")) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() +
                                 "' has no gridPriority - required for every descendant of a nested grid");
    }
    return gridGroup.readNumericAttribute<std::uint8_t>("gridPriority")[0];
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
void validateAndAddDescendants(GridParent<NodeData>& root, std::vector<DescendantEntry<NodeData>> descendants) {
    const std::size_t childCount = descendants.size();
    std::vector<bool> seen(childCount + 1, false);  // index 0 unused - valid priorities here start at 1
    for (const auto& entry : descendants) {
        if (entry.priority == 0 || entry.priority > childCount) {
            throw std::runtime_error("ggrxf_reader: '" + entry.name + "' has gridPriority " +
                                     std::to_string(entry.priority) + ", expected within [1, " +
                                     std::to_string(childCount) + "] for this " + std::to_string(childCount) +
                                     "-descendant nest");
        }
        if (seen[entry.priority]) {
            throw std::runtime_error("ggrxf_reader: '" + entry.name + "' has gridPriority " +
                                     std::to_string(entry.priority) +
                                     ", already used by another descendant of the same nest");
        }
        seen[entry.priority] = true;
    }

    std::sort(descendants.begin(), descendants.end(),
              [](const auto& a, const auto& b) { return a.priority < b.priority; });
    for (auto& entry : descendants) {
        root.addChild(std::move(entry.grid));
    }
}

// One scalar_with_time_function grid group's own geometry and node data -
// everything parseScalarLeafGrid()/parseScalarRootGrid() below need to
// construct either a plain Grid or a GridParent, before either one decides
// what (if anything) to do about this group's own subgroups.
struct ScalarGridContent {
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
ScalarGridContent parseScalarGridContent(const netcdf::NetcdfGroup& gridGroup,
                                         const std::vector<std::size_t>& canonicalAxisIndices, bool latitudeFirst) {
    const auto affineCoeffs = readAffineCoeffs(gridGroup, latitudeFirst);
    const std::uint32_t iNodeCount = gridGroup.readNumericAttribute<std::uint32_t>("iNodeCount")[0];
    const std::uint32_t jNodeCount = gridGroup.readNumericAttribute<std::uint32_t>("jNodeCount")[0];
    GridGeometry geometry(affineCoeffs, iNodeCount, jNodeCount, kCellTolerance);

    auto displacementVar = gridGroup.variable("displacement");
    const auto dims = displacementVar.dimensionLengths();
    if (dims.size() != 3) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() + "'.displacement has " +
                                 std::to_string(dims.size()) + " dimensions, expected 3");
    }
    // This grid's own displacement-axis count comes from the file's own
    // stored array; canonicalAxisIndices comes from this grid's own
    // layer's own gridParameters attribute - a separately-sourced count
    // that must still agree, since NetcdfScalarNodeValueSource pairs raw
    // axis position with canonicalAxisIndices position-for-position.
    const std::size_t axisCount = dims[2];
    if (axisCount != canonicalAxisIndices.size()) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() + "'.displacement has " +
                                 std::to_string(axisCount) + " axes, but this layer's own gridParameters lists " +
                                 std::to_string(canonicalAxisIndices.size()));
    }

    const double scaleFactor = displacementVar.readNumericAttribute<double>("scale_factor")[0];
    const std::optional<std::int32_t> missingValue =
        displacementVar.hasAttribute("missing_value")
            ? std::optional<std::int32_t>(displacementVar.readNumericAttribute<std::int32_t>("missing_value")[0])
            : std::nullopt;
    const bool hasEndDay =
        gridGroup.hasVariable("has_end_day") && gridGroup.variable("has_end_day").readValues<std::uint8_t>()[0] != 0;

    auto valueSource = std::make_unique<NetcdfScalarNodeValueSource>(
        jNodeCount, canonicalAxisIndices, displacementVar.readValues<std::int32_t>(), missingValue, hasEndDay);
    return ScalarGridContent{std::move(geometry), ScalarGridData(scaleFactor, std::move(valueSource))};
}

// Parses gridGroup's own geometry/data only, as a plain leaf Grid - used
// both for a non-nested root grid, and for every descendant collected by
// collectScalarDescendants() below (whose own on-disk subgroups, if any,
// are walked separately by that same function, never preserved as this
// leaf's own children).
std::unique_ptr<Grid<ScalarGridData>> parseScalarLeafGrid(const netcdf::NetcdfGroup& gridGroup,
                                                          const std::vector<std::size_t>& canonicalAxisIndices,
                                                          bool latitudeFirst) {
    auto content = parseScalarGridContent(gridGroup, canonicalAxisIndices, latitudeFirst);
    return std::make_unique<Grid<ScalarGridData>>(content.geometry, std::move(content.data));
}

// Recursively collects every descendant of gridGroup - at every depth, not
// just its own direct subgroups - into `descendants`, each paired with its
// own required gridPriority (see readRequiredGridPriority()). Whatever
// on-disk nesting a descendant itself carries is walked here (so nothing is
// missed) but never preserved on the leaf Grid parseScalarLeafGrid()
// constructs for it - see GGRXF_CPP_READER_PLAN.md's own grid-priority
// design notes for why a nest's whole subtree flattens into one
// GridParent's own children_, not a multi-level Grid<T> tree.
void collectScalarDescendants(const netcdf::NetcdfGroup& gridGroup,
                              std::vector<DescendantEntry<ScalarGridData>>& descendants,
                              const std::vector<std::size_t>& canonicalAxisIndices, bool latitudeFirst) {
    for (const auto& childGroup : gridGroup.subgroups()) {
        descendants.push_back(
            DescendantEntry<ScalarGridData>{parseScalarLeafGrid(childGroup, canonicalAxisIndices, latitudeFirst),
                                            readRequiredGridPriority(childGroup), childGroup.name()});
        collectScalarDescendants(childGroup, descendants, canonicalAxisIndices, latitudeFirst);
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
std::unique_ptr<Grid<ScalarGridData>> parseScalarRootGrid(const netcdf::NetcdfGroup& gridGroup,
                                                          const std::vector<std::size_t>& canonicalAxisIndices,
                                                          bool latitudeFirst) {
    if (gridGroup.subgroups().empty()) {
        if (gridGroup.hasAttribute("gridPriority")) {
            throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() +
                                     "' has no child grids but carries gridPriority");
        }
        return parseScalarLeafGrid(gridGroup, canonicalAxisIndices, latitudeFirst);
    }

    if (!gridGroup.hasAttribute("gridPriority")) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() +
                                 "' has child grids but no gridPriority (expected 0)");
    }
    const std::uint8_t rootPriority = gridGroup.readNumericAttribute<std::uint8_t>("gridPriority")[0];
    if (rootPriority != 0) {
        throw std::runtime_error("ggrxf_reader: '" + gridGroup.name() + "' is a nest root with gridPriority " +
                                 std::to_string(rootPriority) + ", expected 0");
    }

    auto content = parseScalarGridContent(gridGroup, canonicalAxisIndices, latitudeFirst);
    auto root = std::make_unique<GridParent<ScalarGridData>>(content.geometry, std::move(content.data));

    std::vector<DescendantEntry<ScalarGridData>> descendants;
    collectScalarDescendants(gridGroup, descendants, canonicalAxisIndices, latitudeFirst);
    validateAndAddDescendants(*root, std::move(descendants));
    return root;
}

// Reads layerGroup's own timeFunctions (stored as separate, flat,
// dot-named attributes - timeFunctions.count, then timeFunctions.<n>.* per
// entry, not a single compound-type attribute) into layer, one
// TimeFunction per entry. Throws if any entry's own functionType isn't
// linear/step/ramp - the only shapes TimeFunction itself implements.
void parseTimeFunctions(const netcdf::NetcdfGroup& layerGroup, ScalarLayer& layer) {
    const std::uint8_t count = layerGroup.readNumericAttribute<std::uint8_t>("timeFunctions.count")[0];
    for (std::uint8_t index = 0; index < count; ++index) {
        const std::string prefix = "timeFunctions." + std::to_string(index) + ".";
        const std::string functionType = layerGroup.readTextAttribute(prefix + "functionType");

        TimeFunction::Params params;
        if (functionType == "linear") {
            params.shape = TimeFunctionShape::Linear;
        } else if (functionType == "step") {
            params.shape = TimeFunctionShape::Step;
        } else if (functionType == "ramp") {
            params.shape = TimeFunctionShape::Ramp;
        } else {
            throw std::runtime_error("ggrxf_reader: " + prefix + "functionType '" + functionType +
                                     "' is not implemented (only linear/"
                                     "step/ramp are)");
        }
        params.scaleFactor = layerGroup.readNumericAttribute<double>(prefix + "scaleFactor")[0];
        // Every date field below is independently optional in the file
        // (which ones a given entry actually needs depends on its own
        // functionType) - TimeFunction's own constructor validates that
        // whichever ones its own shape requires were actually supplied, so
        // this just passes through whatever is present without checking
        // that itself.
        if (layerGroup.hasAttribute(prefix + "functionReferenceDate")) {
            params.functionReferenceDateDays =
                ggrxfDateToDayNumber(layerGroup.readTextAttribute(prefix + "functionReferenceDate"));
        }
        if (layerGroup.hasAttribute(prefix + "startDate")) {
            params.startDateDays = ggrxfDateToDayNumber(layerGroup.readTextAttribute(prefix + "startDate"));
        }
        if (layerGroup.hasAttribute(prefix + "endDate")) {
            params.endDateDays = ggrxfDateToDayNumber(layerGroup.readTextAttribute(prefix + "endDate"));
        }
        if (layerGroup.hasAttribute(prefix + "eventDate")) {
            params.eventDateDays = ggrxfDateToDayNumber(layerGroup.readTextAttribute(prefix + "eventDate"));
        }
        layer.addTimeFunction(TimeFunction(params));
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
std::unique_ptr<Layer> parseScalarLayer(const netcdf::NetcdfGroup& layerGroup, bool latitudeFirst) {
    auto layer = std::make_unique<ScalarLayer>();
    parseTimeFunctions(layerGroup, *layer);
    const auto canonicalAxisIndices = scalarCanonicalAxisIndices(layerGroup);
    for (const auto& rootGroup : rootGridGroupsInFileOrder(layerGroup)) {
        layer->addRootGrid(parseScalarRootGrid(rootGroup, canonicalAxisIndices, latitudeFirst));
    }
    return layer;
}

// The sentinel `parent_grid_index` value marking a root grid in a
// ragged_segments layer's own consolidated grid arrays - GGRXF_DEFORMATION_
// FORMAT.md's own uint32 max (grid index 0 is an ordinary, valid array
// position, so it can't double as "no parent").
constexpr std::uint32_t kRaggedRootSentinel = std::numeric_limits<std::uint32_t>::max();

// One ragged_segments layer's own consolidated storage, read once in full:
// every grid's own geometry/nesting fields, indexed by a `grid` dimension,
// plus every node's own reference_day/has_end_day and every component's own
// segment records, indexed by a layer-wide `node` dimension spanning every
// grid's nodes concatenated. A per-grid RaggedGridData (see
// parseRaggedGridContent() below) is built by slicing into these arrays,
// never by reading a per-grid netCDF object - this grid type has no grid
// subgroups at all.
struct RaggedLayerArrays {
    std::vector<double> affineCoeffs;            // [grid * 6 + k]
    std::vector<std::uint32_t> iNodeCount;       // [grid]
    std::vector<std::uint32_t> jNodeCount;       // [grid]
    std::vector<std::uint8_t> gridPriority;      // [grid] - meaningless for a root grid, see parseRaggedLayer() below
    std::vector<std::uint32_t> parentGridIndex;  // [grid], kRaggedRootSentinel for a root grid
    std::vector<std::size_t>
        gridNodeOffset;  // [grid + 1]; gridNodeOffset[g] is grid g's own base index into the layer-wide node arrays below, reconstructed (not stored) as a cumulative sum of iNodeCount*jNodeCount

    std::vector<std::uint32_t> referenceDay;  // layer-wide, one entry per node
    std::vector<bool>
        hasEndDayPerNode;  // layer-wide, one entry per node; empty if the file has no has_end_day variable at all

    std::size_t axisCount = 0;
    // Per axis: layer-wide CSR offsets (one entry per node, plus a final
    // sentinel), reconstructed from segment_count_{enu}, and the raw
    // segment fields those offsets index into (each sum(segment_count_{enu})
    // long).
    std::vector<std::vector<std::size_t>> nodeSegmentOffset;
    std::vector<std::vector<std::int32_t>> segmentStartValues;
    std::vector<std::vector<std::uint16_t>> segmentDayDeltas;
    std::vector<std::vector<std::int16_t>> segmentScaleFactors;
    std::vector<std::vector<std::uint8_t>> segmentFunctionTypes;
};

// This format always requires segment_count_e/n; segment_count_u is only
// present for a 3-axis model. Throws if even e/n are missing - a
// ragged_segments layer with neither is malformed.
std::size_t raggedLayerAxisCount(const netcdf::NetcdfGroup& layerGroup) {
    if (!layerGroup.hasVariable("segment_count_e") || !layerGroup.hasVariable("segment_count_n")) {
        throw std::runtime_error("ggrxf_reader: '" + layerGroup.name() + "' is missing segment_count_e/n");
    }
    return layerGroup.hasVariable("segment_count_u") ? 3 : 2;
}

// Reads layerGroup's own consolidated grid/node/segment arrays in full -
// see RaggedLayerArrays' own class comment. Every grid's own affineCoeffs
// entry is canonicalized to (latitude, longitude) axis order in place here
// (see canonicalizeAffineCoeffsAxisOrder()), the ragged sibling to
// readAffineCoeffs()'s own per-grid canonicalization for a
// scalar_with_time_function grid.
RaggedLayerArrays readRaggedLayerArrays(const netcdf::NetcdfGroup& layerGroup, bool latitudeFirst) {
    RaggedLayerArrays arrays;
    arrays.affineCoeffs = layerGroup.variable("affineCoeffs").readValues<double>();
    if (!latitudeFirst) {
        const std::size_t gridCount = arrays.affineCoeffs.size() / 6;
        for (std::size_t grid = 0; grid < gridCount; ++grid) {
            for (std::size_t k = 0; k < 3; ++k) {
                std::swap(arrays.affineCoeffs[grid * 6 + k], arrays.affineCoeffs[grid * 6 + k + 3]);
            }
        }
    }
    arrays.iNodeCount = layerGroup.variable("iNodeCount").readValues<std::uint32_t>();
    arrays.jNodeCount = layerGroup.variable("jNodeCount").readValues<std::uint32_t>();
    arrays.gridPriority = layerGroup.variable("gridPriority").readValues<std::uint8_t>();
    arrays.parentGridIndex = layerGroup.variable("parent_grid_index").readValues<std::uint32_t>();

    const std::size_t gridCount = arrays.iNodeCount.size();
    arrays.gridNodeOffset.assign(gridCount + 1, 0);
    for (std::size_t grid = 0; grid < gridCount; ++grid) {
        arrays.gridNodeOffset[grid + 1] =
            arrays.gridNodeOffset[grid] + static_cast<std::size_t>(arrays.iNodeCount[grid]) * arrays.jNodeCount[grid];
    }

    arrays.referenceDay = layerGroup.variable("reference_day").readValues<std::uint32_t>();
    const std::size_t nodeCount = arrays.referenceDay.size();
    if (layerGroup.hasVariable("has_end_day")) {
        const auto raw = layerGroup.variable("has_end_day").readValues<std::uint8_t>();
        arrays.hasEndDayPerNode.resize(raw.size());
        for (std::size_t node = 0; node < raw.size(); ++node) {
            arrays.hasEndDayPerNode[node] = raw[node] != 0;
        }
    }

    arrays.axisCount = raggedLayerAxisCount(layerGroup);
    arrays.nodeSegmentOffset.resize(arrays.axisCount);
    arrays.segmentStartValues.resize(arrays.axisCount);
    arrays.segmentDayDeltas.resize(arrays.axisCount);
    arrays.segmentScaleFactors.resize(arrays.axisCount);
    arrays.segmentFunctionTypes.resize(arrays.axisCount);
    for (std::size_t axis = 0; axis < arrays.axisCount; ++axis) {
        const std::string suffix(1, kComponentLetters[axis]);
        const auto counts = layerGroup.variable("segment_count_" + suffix).readValues<std::uint16_t>();

        // The CSR-style offsets into segments_{suffix}/function_type_{suffix}
        // for each node's own slice - a running sum of counts,
        // reconstructed once per (layer, axis) rather than per query.
        std::vector<std::size_t> offsets(nodeCount + 1, 0);
        for (std::size_t node = 0; node < nodeCount; ++node) {
            offsets[node + 1] = offsets[node] + counts[node];
        }
        arrays.nodeSegmentOffset[axis] = std::move(offsets);

        arrays.segmentStartValues[axis] =
            layerGroup.variable("segments_" + suffix).readCompoundField<std::int32_t>("segment_start_value");
        arrays.segmentDayDeltas[axis] =
            layerGroup.variable("segments_" + suffix).readCompoundField<std::uint16_t>("day_delta");
        arrays.segmentScaleFactors[axis] =
            layerGroup.variable("segments_" + suffix).readCompoundField<std::int16_t>("segment_scale_factor");
        arrays.segmentFunctionTypes[axis] = layerGroup.variable("function_type_" + suffix).readValues<std::uint8_t>();
    }
    return arrays;
}

// One ragged_segments grid group's own geometry and node data - the ragged
// sibling to ScalarGridContent above, same rationale.
struct RaggedGridContent {
    GridGeometry geometry;
    RaggedGridData data;
};

// Builds gridIndex's own geometry and node data by slicing them out of
// `arrays` (read once, layer-wide, by readRaggedLayerArrays() above) rather
// than reading a per-grid netCDF object - this grid type has no grid
// subgroups at all. For each displacement axis this grid carries: rebases
// that axis's own layer-wide CSR segment offsets to this grid's own node
// range, then copies out its own segments_{e,n,u} compound records and
// paired function_type_{e,n,u} entries (every entry must be linear/code 0 -
// the only ragged segment shape RaggedGridData implements). Every parsed
// segment is combined into a single, already-scaled RaggedSegment (see
// RaggedSegment's own documentation for why startDay is already the
// combined reference_day + day_delta rather than the two kept apart).
RaggedGridContent parseRaggedGridContent(const RaggedLayerArrays& arrays, std::size_t gridIndex) {
    std::array<double, 6> affineCoeffs{};
    std::copy(arrays.affineCoeffs.begin() + static_cast<std::ptrdiff_t>(gridIndex * 6),
              arrays.affineCoeffs.begin() + static_cast<std::ptrdiff_t>(gridIndex * 6 + 6), affineCoeffs.begin());
    const std::uint32_t iNodeCount = arrays.iNodeCount[gridIndex];
    const std::uint32_t jNodeCount = arrays.jNodeCount[gridIndex];
    GridGeometry geometry(affineCoeffs, iNodeCount, jNodeCount, kCellTolerance);

    const std::size_t nodeCount = static_cast<std::size_t>(iNodeCount) * jNodeCount;
    const std::size_t nodeBase = arrays.gridNodeOffset[gridIndex];

    std::vector<bool> hasEndDayPerNode;
    if (!arrays.hasEndDayPerNode.empty()) {
        hasEndDayPerNode.assign(arrays.hasEndDayPerNode.begin() + static_cast<std::ptrdiff_t>(nodeBase),
                                arrays.hasEndDayPerNode.begin() + static_cast<std::ptrdiff_t>(nodeBase + nodeCount));
    }

    // One flat, pre-sized std::vector<RaggedSegment> per axis (built below),
    // rather than one per node - see NetcdfRaggedNodeValueSource's own class
    // comment for why (a real, measured heap-fragmentation problem the
    // one-vector-per-node shape had).
    std::vector<std::vector<std::size_t>> segmentOffsetsByAxis(arrays.axisCount);
    std::vector<std::vector<RaggedSegment>> segmentsByAxis(arrays.axisCount);

    for (std::size_t axis = 0; axis < arrays.axisCount; ++axis) {
        const auto& globalOffsets = arrays.nodeSegmentOffset[axis];

        // This grid's own node-local CSR offsets, rebased from the
        // layer-wide ones above at nodeBase.
        std::vector<std::size_t> offsets(nodeCount + 1, 0);
        for (std::size_t local = 0; local < nodeCount; ++local) {
            offsets[local + 1] =
                offsets[local] + (globalOffsets[nodeBase + local + 1] - globalOffsets[nodeBase + local]);
        }

        std::vector<RaggedSegment> flatSegments(offsets[nodeCount]);
        for (std::size_t local = 0; local < nodeCount; ++local) {
            const std::size_t absNode = nodeBase + local;
            const std::size_t globalStart = globalOffsets[absNode];
            const std::size_t globalEnd = globalOffsets[absNode + 1];
            for (std::size_t k = globalStart; k < globalEnd; ++k) {
                if (arrays.segmentFunctionTypes[axis][k] != 0) {
                    throw std::runtime_error("ggrxf_reader: function_type_" + std::string(1, kComponentLetters[axis]) +
                                             " code " + std::to_string(arrays.segmentFunctionTypes[axis][k]) +
                                             " is not implemented (only linear, code 0, is)");
                }
                RaggedSegment& segment = flatSegments[offsets[local] + (k - globalStart)];
                segment.startDay = static_cast<std::int64_t>(arrays.referenceDay[absNode]) +
                                   static_cast<std::int64_t>(arrays.segmentDayDeltas[axis][k]);
                segment.startValue = static_cast<double>(arrays.segmentStartValues[axis][k]) * kSegmentStartValueScale;
                segment.ratePerDay =
                    static_cast<double>(arrays.segmentScaleFactors[axis][k]) * kSegmentScaleFactorLinearScale;
            }
        }
        segmentOffsetsByAxis[axis] = std::move(offsets);
        segmentsByAxis[axis] = std::move(flatSegments);
    }

    auto valueSource =
        std::make_unique<NetcdfRaggedNodeValueSource>(jNodeCount, arrays.axisCount, std::move(hasEndDayPerNode),
                                                      std::move(segmentOffsetsByAxis), std::move(segmentsByAxis));
    return RaggedGridContent{std::move(geometry), RaggedGridData(std::move(valueSource))};
}

// Builds gridIndex's own geometry/data as a plain leaf Grid - the ragged
// sibling to parseScalarLeafGrid() above, same rationale.
std::unique_ptr<Grid<RaggedGridData>> parseRaggedLeafGrid(const RaggedLayerArrays& arrays, std::size_t gridIndex) {
    auto content = parseRaggedGridContent(arrays, gridIndex);
    return std::make_unique<Grid<RaggedGridData>>(content.geometry, std::move(content.data));
}

// Recursively collects gridIndex's entire descendant subtree - at every
// depth, not just its own direct children - into `descendants`, each paired
// with its own required gridPriority read straight from `arrays` (every
// grid always carries one, see RaggedLayerArrays' own class comment; a
// descendant's own value is validated by validateAndAddDescendants() below,
// not here). The ragged sibling to collectScalarDescendants() above, same
// rationale, but walking childrenOf (parent grid index -> child grid
// indices, built once in parseRaggedLayer() below from parent_grid_index)
// instead of netCDF subgroup nesting.
void collectRaggedDescendants(std::size_t gridIndex, const RaggedLayerArrays& arrays,
                              const std::unordered_map<std::uint32_t, std::vector<std::size_t>>& childrenOf,
                              std::vector<DescendantEntry<RaggedGridData>>& descendants) {
    const auto found = childrenOf.find(static_cast<std::uint32_t>(gridIndex));
    if (found == childrenOf.end()) {
        return;
    }
    for (const std::size_t childIndex : found->second) {
        descendants.push_back(DescendantEntry<RaggedGridData>{parseRaggedLeafGrid(arrays, childIndex),
                                                              arrays.gridPriority[childIndex],
                                                              "grid index " + std::to_string(childIndex)});
        collectRaggedDescendants(childIndex, arrays, childrenOf, descendants);
    }
}

// Parses one ragged_segments layer group: reads its own consolidated grid
// arrays in full (readRaggedLayerArrays() above), builds a parent-grid-index
// -> child-grid-indices adjacency map from parent_grid_index (one pass,
// O(grid count)), then builds one Grid<RaggedGridData> per grid whose own
// parent_grid_index is the root sentinel - a plain leaf Grid if childless,
// or a GridParent whose entire descendant subtree (any depth) is flattened
// via collectRaggedDescendants()/validateAndAddDescendants(), exactly as
// parseScalarRootGrid() does for a scalar_with_time_function grid's own
// subgroup nesting. Root grids are collected in ascending grid-index (file)
// order, then re-sorted descending by size (iNodeCount) before being added
// to the layer - see the comment on that sort below for why.
std::unique_ptr<Layer> parseRaggedLayer(const netcdf::NetcdfGroup& layerGroup, bool latitudeFirst) {
    const double referenceDateDays = ggrxfDateToDayNumber(layerGroup.readTextAttribute("reference_date"));
    auto layer = std::make_unique<RaggedLayer>(referenceDateDays);

    const RaggedLayerArrays arrays = readRaggedLayerArrays(layerGroup, latitudeFirst);
    const std::size_t gridCount = arrays.iNodeCount.size();

    std::unordered_map<std::uint32_t, std::vector<std::size_t>> childrenOf;
    for (std::size_t grid = 0; grid < gridCount; ++grid) {
        childrenOf[arrays.parentGridIndex[grid]].push_back(grid);
    }

    std::vector<std::unique_ptr<Grid<RaggedGridData>>> rootGrids;
    for (std::size_t grid = 0; grid < gridCount; ++grid) {
        if (arrays.parentGridIndex[grid] != kRaggedRootSentinel) {
            continue;  // not a root - collected as a descendant of whichever root it traces back to, below
        }

        const auto childrenFound = childrenOf.find(static_cast<std::uint32_t>(grid));
        if (childrenFound == childrenOf.end() || childrenFound->second.empty()) {
            // A childless root's own gridPriority is meaningless (see
            // RaggedLayerArrays' own class comment) - not read here, unlike
            // a nest root's below.
            rootGrids.push_back(parseRaggedLeafGrid(arrays, grid));
            continue;
        }

        if (arrays.gridPriority[grid] != 0) {
            throw std::runtime_error("ggrxf_reader: '" + layerGroup.name() + "' grid index " + std::to_string(grid) +
                                     " is a nest root with gridPriority " + std::to_string(arrays.gridPriority[grid]) +
                                     ", expected 0");
        }
        auto content = parseRaggedGridContent(arrays, grid);
        auto root = std::make_unique<GridParent<RaggedGridData>>(content.geometry, std::move(content.data));

        std::vector<DescendantEntry<RaggedGridData>> descendants;
        collectRaggedDescendants(grid, arrays, childrenOf, descendants);
        validateAndAddDescendants(*root, std::move(descendants));
        rootGrids.push_back(std::move(root));
    }

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
    std::sort(rootGrids.begin(), rootGrids.end(),
              [](const auto& a, const auto& b) { return a->geometry().iNodeCount() > b->geometry().iNodeCount(); });
    for (auto& rootGrid : rootGrids) {
        layer->addRootGrid(std::move(rootGrid));
    }
    return layer;
}

// Parses one top-level layer group: validates interpolationMethod (must be
// bilinear - the only method Grid's own resolution/interpolation
// implements), then dispatches on this layer's own grid_type attribute -
// every grid within one layer, at every nesting depth, is required to share
// this one value, so a layer's own grid_type fully determines which shape
// (scalar_with_time_function's grid subgroups, or ragged_segments'
// consolidated layer-wide arrays) the rest of this layer group is parsed
// as.
std::unique_ptr<Layer> parseLayer(const netcdf::NetcdfGroup& layerGroup, bool latitudeFirst) {
    const std::string interpolationMethod = layerGroup.readTextAttribute("interpolationMethod");
    if (interpolationMethod != "bilinear") {
        throw std::runtime_error("ggrxf_reader: '" + layerGroup.name() + "'.interpolationMethod '" +
                                 interpolationMethod + "' is not implemented (only bilinear is)");
    }

    const std::string gridType = layerGroup.readTextAttribute("grid_type");
    if (gridType == "ragged_segments") {
        return parseRaggedLayer(layerGroup, latitudeFirst);
    }
    if (gridType == "scalar_with_time_function") {
        return parseScalarLayer(layerGroup, latitudeFirst);
    }
    throw std::runtime_error("ggrxf_reader: '" + layerGroup.name() + "'.grid_type '" + gridType +
                             "' is not implemented (only scalar_with_time_function/ragged_segments are)");
}

}  // namespace

GgrxfFile parseGgrxfFile(const std::string& path) {
    netcdf::NetcdfFile const file(path);
    const auto rootGroup = file.rootGroup();

    // interpolationCrsWkt is required (GGRXF_DEFORMATION_FORMAT.md
    // "Container") - read once here, since every layer's own grids share
    // this file's one interpolation CRS, and used only to determine axis
    // order (see wktDeclaresLatitudeFirst()); nothing else in the WKT text
    // is read.
    const bool latitudeFirst = wktDeclaresLatitudeFirst(rootGroup.readTextAttribute("interpolationCrsWkt"));

    GgrxfFile result;
    for (const auto& layerGroup : rootGroup.subgroups()) {
        result.layers.push_back(parseLayer(layerGroup, latitudeFirst));
    }
    if (rootGroup.hasAttribute("modelVersion")) {
        result.modelVersion = rootGroup.readTextAttribute("modelVersion");
    }
    return result;
}

}  // namespace deformation
