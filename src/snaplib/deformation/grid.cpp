#include "deformation/grid.h"

#include <stdexcept>
#include <string>

#include "deformation/scalargriddata.h"
#include "deformation/raggedgriddata.h"

namespace deformation {

template <typename NodeData>
Grid<NodeData>::Grid( const GridGeometry &geometry, NodeData data )
    : geometry_( geometry ),
      data_( std::move( data ) )
{
}

template <typename NodeData>
const GridGeometry &Grid<NodeData>::geometry() const
{
    return geometry_;
}

template <typename NodeData>
const NodeData &Grid<NodeData>::data() const
{
    return data_;
}

template <typename NodeData>
std::optional<Cell> Grid<NodeData>::locate( double c0, double c1 ) const
{
    return geometry_.locate( c0, c1 );
}

template <typename NodeData>
std::size_t Grid<NodeData>::axisCount() const
{
    return data_.axisCount();
}

template <typename NodeData>
void Grid<NodeData>::addChild( std::unique_ptr<Grid<NodeData>> child )
{
    if( ! child ) {
        throw std::invalid_argument( "Grid::addChild: child must not be null" );
    }
    if( child->axisCount() != axisCount() ) {
        throw std::invalid_argument(
            "Grid::addChild: child axisCount (" + std::to_string( child->axisCount() )
            + ") does not match parent's own (" + std::to_string( axisCount() ) + ")" );
    }
    if( ! geometry_.containsExtentOf( child->geometry_ ) ) {
        throw std::invalid_argument( "Grid::addChild: child extent is not contained within parent's own extent" );
    }
    children_.push_back( std::move( child ) );
}

template <typename NodeData>
const std::vector<std::unique_ptr<Grid<NodeData>>> &Grid<NodeData>::children() const
{
    return children_;
}

template <typename NodeData>
bool Grid<NodeData>::isLeaf() const
{
    return children_.empty();
}

template <typename NodeData>
std::optional<Resolution<NodeData>> Grid<NodeData>::resolveChild( double c0, double c1 ) const
{
    return selectAmongSiblings( children_, c0, c1 );
}

template <typename NodeData>
std::optional<Resolution<NodeData>> Grid<NodeData>::selectAmongSiblings(
    const std::vector<std::unique_ptr<Grid<NodeData>>> &siblings, double c0, double c1 )
{
    for( const auto &sibling : siblings ) {
        if( const auto cell = sibling->locate( c0, c1 ) ) {
            return Resolution<NodeData>{ *sibling, *cell };
        }
    }
    return std::nullopt;
}

// Explicit instantiation - Grid is only ever used with these two node-data
// types (ScalarGridData for scalar_with_time_function, RaggedGridData for
// ragged_segments), so the definitions above live here rather than in the
// header, matching this module's own declaration/definition split.
template class Grid<ScalarGridData>;
template class Grid<RaggedGridData>;

} // namespace deformation
