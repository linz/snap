#pragma once

#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "gridgeometry.h"

namespace deformation {

template <typename NodeData>
class Grid;

// One grid a query point resolved to, together with the cell within it -
// what Grid's resolution methods below return, so a caller can pass the
// cell straight to the grid's own NodeData (ScalarGridData::interpolate(),
// RaggedGridData::evaluate()) without relocating it.
template <typename NodeData>
struct Resolution
{
    const Grid<NodeData> *grid;
    Cell cell;
};

// One node in a grid hierarchy: one GridGeometry, one NodeData, plus
// whichever child Grids describe the same physical contribution at finer
// resolution (GGRXF spec: "A grid may itself contain further grid
// subgroups nested inside it"). NodeData is a template parameter, not a
// base class - C++17 has no concepts to spell this out as an enforced,
// compiler-checked contract, so it's stated here instead: NodeData must
// provide `std::size_t axisCount() const`, the only operation Grid itself
// ever calls on it (see addChild()). The two real instantiations are
// ScalarGridData (scalar_with_time_function) and RaggedGridData
// (ragged_segments) - see scalargriddata.h/raggedgriddata.h for each one's
// own further value-computation method (ScalarGridData::interpolate(),
// RaggedGridData::evaluate()), which Grid itself never calls at all: a
// caller gets the
// resolved node data via Resolution::grid->data() and invokes whichever
// type-specific method fits, since the two differ in signature (an extra
// query-date parameter, optional vs. plain return) in a way a single
// shared method on Grid couldn't paper over without being misleading for
// one side or the other. Grid resolution
// is confirmed identical for both grid types (spec: "Grid resolution
// within a layer" resolves any grid the same way, dispatching to
// grid-type-specific evaluation only afterward) - this is genuinely one
// shared template, not a scalar-only mechanism ragged happens to also use.
// Each child's own extent must be entirely contained within this grid's
// own (enforced by addChild(), see below) - nesting is how spatial
// *resolution* is expressed; it is a separate concept from spatial
// *coverage*, below.
//
// Picking a grid follows one rule, applied identically whether choosing
// among a Layer's own root grids or one Grid's own children(): the first
// entry in the list whose extent contains the query point wins - first
// come, first served. Entries may cover the same area (in which case list
// order matters) or different areas (in which case it doesn't) - either
// way, list order alone decides; see selectAmongSiblings() below. GGRXF's
// own gridPriority attribute, where a file has one, is simply translated
// into that order by whoever builds the hierarchy (a GGRXF parser; LINZDEF
// has no equivalent attribute at all) - Grid itself never stores or reads
// a priority number. (A RaggedLayer's own root-level selection, at
// real-world scale, replaces this rule with a spatial index instead of a
// linear scan, but still resolves to the same answer - see RaggedLayer;
// Grid's own children() lists, and a ScalarLayer's typically-small root
// list, stay on this direct rule.)
//
// Once a grid is picked by coverage, resolveChild() keeps descending into
// its own children - each one finer than the last - stopping as soon as
// none of the current grid's own children cover the query point. The
// current grid's own value is then used. Grid has no parent pointer: this
// only ever walks top-down and never needs to go back up.
template <typename NodeData>
class Grid
{
    // The closest C++17 stand-in for a concept constraint (C++20 isn't
    // available - see the class comment above): NodeData must provide
    // `std::size_t axisCount() const`, the only operation Grid itself ever
    // calls on it. Without this, a violation would only surface as a deep,
    // hard-to-read error from the explicit instantiations at the bottom of
    // grid.cpp, rather than this deliberate, readable message.
    static_assert( std::is_convertible_v<decltype( std::declval<const NodeData &>().axisCount() ), std::size_t>,
                   "NodeData must provide std::size_t axisCount() const" );

public:
    Grid( GridGeometry geometry, NodeData data );   // this grid's own geometry and node data; children are added afterward via addChild()

    const GridGeometry &geometry() const;
    const NodeData &data() const;

    // Forwards to geometry().locate() - see GridGeometry.
    std::optional<Cell> locate( double c0, double c1 ) const;

    // Forwards to data().axisCount() - see ScalarGridData/RaggedGridData. Kept
    // here too since addChild()'s own validation needs it, and a caller
    // holding a Grid often wants it without going via data() just to ask.
    std::size_t axisCount() const;

    // Adds a child grid. Ownership transfers to this Grid. If this grid
    // has multiple children that could both cover the same point, add
    // them in the order they should be tried - first come, first served
    // (see the class comment above); children covering different areas
    // can be added in any order.
    //
    // Two invariants are checked and enforced here: every child's own
    // axisCount() must equal this grid's own (GGRXF's gridParameters is
    // declared once per layer, so every grid nested anywhere under it
    // necessarily agrees), and the child's own extent must be entirely
    // contained within this grid's own (see GridGeometry::containsExtentOf()).
    // A file that violates either is malformed (or this is a parser bug),
    // so this throws rather than silently accepting bad data - same
    // reasoning as TimeFunction's own constructor validation.
    void addChild( std::unique_ptr<Grid<NodeData>> child );   // the child grid to add; throws std::invalid_argument if child is null, child->axisCount() != this->axisCount(), or child's own extent is not contained within this grid's own

    // This grid's own child grids, in the order they were added (see
    // addChild()). Never contains null entries.
    const std::vector<std::unique_ptr<Grid<NodeData>>> &children() const;

    // True if this grid has no children.
    bool isLeaf() const;

    // Picks among this grid's own children() - equivalent to
    // selectAmongSiblings(children(), c0, c1), for the common case of
    // choosing one of a specific grid's own children rather than a
    // Layer's root grids (which no single Grid owns, so must go through
    // the static selectAmongSiblings() form directly).
    std::optional<Resolution<NodeData>> resolveChild( double c0, double c1 ) const;

    // Picks a single grid among `siblings` that contains (c0, c1) - the
    // first-come-first-served rule from the class comment above, used
    // identically for a Layer's own root grids and for any Grid's own
    // children(). Returns std::nullopt if none of `siblings` contains the
    // point. This only ever inspects `siblings` itself, never anything
    // deeper - choosing further into the picked sibling's own children is
    // the caller's job (see resolveChild()).
    static std::optional<Resolution<NodeData>> selectAmongSiblings(
        const std::vector<std::unique_ptr<Grid<NodeData>>> &siblings,   // a Layer's own root grids, or one Grid's own children()
        double c0,                                                       // coordinate on the siblings' shared first axis
        double c1                                                        // coordinate on the siblings' shared second axis
    );

private:
    GridGeometry geometry_;
    NodeData data_;
    std::vector<std::unique_ptr<Grid<NodeData>>> children_;   // this grid's own child grids, in the order they were added
};

} // namespace deformation
