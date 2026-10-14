#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "grid_geometry.h"

namespace deformation {

template <typename NodeData>
class Grid;

template <typename NodeData>
class GridParent;

// One grid a query point resolved to, together with the cell within it -
// what Grid's/GridParent's resolution methods return, so a caller can pass
// the cell straight to the grid's own NodeData (ScalarGridData::interpolate(),
// RaggedGridData::evaluate()) without relocating it.
template <typename NodeData>
struct Resolution {
    // Non-owning - the resolved grid is (and stays) owned by whichever
    // Layer's rootGrids_/GridParent's children_ it came from; a Resolution
    // never outlives that owner, so this only ever observes it.
    // reference_wrapper rather than a raw pointer: a Resolution is only ever
    // produced already pointing at a real grid (see GridParent::
    // resolveChild() below, and each Layer's own root-grid resolution), so
    // there is no genuine "no grid yet" state for this field to represent -
    // reference_wrapper makes that a compile-time guarantee instead of a
    // convention every future caller has to keep upholding by hand.
    std::reference_wrapper<const Grid<NodeData>> grid;
    Cell cell;
};

// One leaf in a grid hierarchy: one GridGeometry plus one NodeData, nothing
// else - a grid with no children of its own. NodeData is a template
// parameter, not a base class - C++17 has no concepts to spell this out as
// an enforced, compiler-checked contract, so it's stated here instead:
// NodeData must provide `std::size_t axisCount() const`, the only operation
// Grid itself ever calls on it. The two real instantiations are
// ScalarGridData (scalar_with_time_function) and RaggedGridData
// (ragged_segments) - see scalar_grid_data.h/ragged_grid_data.h for each
// one's own further value-computation method (ScalarGridData::interpolate(),
// RaggedGridData::evaluate()), which Grid itself never calls at all: a
// caller gets the resolved node data via Resolution::grid->data() and
// invokes whichever type-specific method fits, since the two differ in
// signature (an extra query-date parameter, optional vs. plain return) in a
// way a single shared method on Grid couldn't paper over without being
// misleading for one side or the other.
//
// A grid may itself have children nested inside it (GGRXF spec: "A grid may
// itself contain further grid subgroups nested inside it") - that capability,
// and any knowledge of a list of grids at all (siblings or children), lives
// entirely on the derived class GridParent below, not here. A plain Grid
// knows nothing about any other Grid; asParent() lets a caller ask, without
// RTTI, whether a given Grid is actually a GridParent carrying children to
// descend into.
template <typename NodeData>
class Grid {
    // The closest C++17 stand-in for a concept constraint (C++20 isn't
    // available - see the class comment above): NodeData must provide
    // `std::size_t axisCount() const`, the only operation Grid itself ever
    // calls on it. Without this, a violation would only surface as a deep,
    // hard-to-read error from the explicit instantiations at the bottom of
    // grid.cpp, rather than this deliberate, readable message.
    static_assert(std::is_convertible_v<decltype(std::declval<const NodeData&>().axisCount()), std::size_t>,
                  "NodeData must provide std::size_t axisCount() const");

 public:
    Grid(
        const GridGeometry& geometry,
        NodeData
            data);  // this grid's own geometry and node data. geometry is copied (it's a trivial, cheap-to-copy aggregate with no move benefit over a copy); data is a sink parameter (NodeData is move-only, owns a unique_ptr) and is moved from

    // Virtual - GridParent (see below) is a real subclass, and a Grid is
    // frequently held/destroyed via std::unique_ptr<Grid<NodeData>> even
    // when it's actually a GridParent underneath.
    virtual ~Grid() = default;

    const GridGeometry& geometry() const;
    const NodeData& data() const;

    // Forwards to geometry().locate() - see GridGeometry.
    std::optional<Cell> locate(double c0, double c1) const;

    // Forwards to data().axisCount() - see ScalarGridData/RaggedGridData. Kept
    // here too since GridParent::addChild()'s own validation needs it, and a
    // caller holding a Grid often wants it without going via data() just to
    // ask.
    std::size_t axisCount() const;

    // A type-safe "does this grid actually have children" check, with no
    // RTTI: returns this grid itself, viewed as a GridParent, if it is one -
    // std::nullptr otherwise (an ordinary leaf Grid). GridParent overrides
    // this to return `this`.
    virtual const GridParent<NodeData>* asParent() const {
        return nullptr;
    }

 private:
    GridGeometry geometry_;
    NodeData data_;
};

// A Grid that has its own nested child grids, each one finer than this grid
// itself (GGRXF spec: "A grid may itself contain further grid subgroups
// nested inside it"). Each child's own extent must be entirely contained
// within this grid's own (enforced by addChild(), see below) - nesting is
// how spatial *resolution* is expressed; it is a separate concept from
// spatial *coverage*.
//
// children_ is held in ascending gridPriority order (lowest priority added
// first) - not because GridParent itself ever reads a numeric priority
// value (it doesn't; see the "why" below), but because that is the order a
// caller must add them in for resolveChild()'s own last-match rule (below)
// to realise "highest priority wins" correctly. Two or more direct children
// of the *same* GridParent always share the same priority in practice
// (GGRXF's own gridPriority attribute only ever distinguishes grids that
// genuinely, spatially overlap - and two children nested under one common
// parent are already resolved relative to each other by that nesting, not
// by a further priority number); the ascending-order convention only
// matters in the corner case where that stops being true.
//
// Priority is otherwise realised by depth alone: within one root's own
// connected chain of descendants, priority strictly increases with each
// level actually descended into. So GridParent never stores or reads a
// priority number at all - a GGRXF parser is responsible for translating
// the file's own gridPriority attribute (where present) into this ordering
// as it calls addChild(), the same way it translates a Layer's own root
// grid order.
template <typename NodeData>
class GridParent : public Grid<NodeData> {
 public:
    GridParent(
        const GridGeometry& geometry,
        NodeData data);  // same parameters as Grid's own constructor; children are added afterward via addChild()

    // Adds a child grid, in ascending gridPriority order (see the class
    // comment above) - if this grid has multiple children that could both
    // cover the same point, add the lower-priority one first. Ownership
    // transfers to this GridParent.
    //
    // Two invariants are checked and enforced here: every child's own
    // axisCount() must equal this grid's own (GGRXF's gridParameters is
    // declared once per layer, so every grid nested anywhere under it
    // necessarily agrees), and the child's own extent must be entirely
    // contained within this grid's own (see GridGeometry::containsExtentOf()).
    // A file that violates either is malformed (or this is a parser bug),
    // so this throws rather than silently accepting bad data - same
    // reasoning as TimeFunction's own constructor validation.
    void addChild(
        std::unique_ptr<Grid<NodeData>>
            child);  // the child grid to add; throws std::invalid_argument if child is null, child->axisCount() != this->axisCount(), or child's own extent is not contained within this grid's own

    // This grid's own child grids, in the order they were added (see
    // addChild()). Never contains null entries.
    const std::vector<std::unique_ptr<Grid<NodeData>>>& children() const;

    // Picks among this grid's own children() at (c0, c1) - a linear scan
    // that returns the *last* covering entry, not the first: children_ is
    // held in ascending priority order (see the class comment above), so
    // the highest-priority covering entry is the one that should win.
    // Returns std::nullopt if none of this grid's own children contains the
    // point.
    std::optional<Resolution<NodeData>> resolveChild(double c0, double c1) const;

    const GridParent<NodeData>* asParent() const override {
        return this;
    }

 private:
    std::vector<std::unique_ptr<Grid<NodeData>>>
        children_;  // this grid's own child grids, in ascending gridPriority order
};

// Descends from `resolution` into successively finer children, for as long
// as the grid currently reached is actually a GridParent (see Grid::
// asParent()) with a child that covers (c0, c1) - stopping at the first
// grid reached that either isn't a GridParent, or has no covering child.
// Shared by ScalarLayer::evaluate()/RaggedLayer::evaluate(), which otherwise
// differ only in how they resolve their own *root* grid - a plain linear
// scan over rootGrids_ for ScalarLayer, a spatial index
// (RaggedLayer::resolveRootGrid()) for RaggedLayer.
template <typename NodeData>
Resolution<NodeData> descendToLeaf(Resolution<NodeData> resolution, double c0, double c1) {
    while (const GridParent<NodeData>* parent = resolution.grid.get().asParent()) {
        auto child = parent->resolveChild(c0, c1);
        if (!child) {
            break;
        }
        resolution = *child;
    }
    return resolution;
}

}  // namespace deformation
