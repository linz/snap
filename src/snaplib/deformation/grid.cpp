#include "deformation/grid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace deformation {

namespace {

// Inverts the affine transform's 2x2 linear part to map (c0, c1) to a
// fractional node index (i, j) - see "Grid spatial indexing attributes".
std::array<double, 2> fractionalIndex( const std::array<double, 6> &affineCoeffs, double c0, double c1 )
{
    const double det = affineCoeffs[1] * affineCoeffs[5] - affineCoeffs[2] * affineCoeffs[4];
    const double dc0 = c0 - affineCoeffs[0];
    const double dc1 = c1 - affineCoeffs[3];
    const double i = ( affineCoeffs[5] * dc0 - affineCoeffs[2] * dc1 ) / det;
    const double j = ( affineCoeffs[1] * dc1 - affineCoeffs[4] * dc0 ) / det;
    return { i, j };
}

// The forward affine transform: node index (i, j) -> world coordinates
// (c0, c1) - the inverse of fractionalIndex() above.
std::array<double, 2> nodeToWorld( const std::array<double, 6> &affineCoeffs, double i, double j )
{
    const double c0 = affineCoeffs[0] + affineCoeffs[1] * i + affineCoeffs[2] * j;
    const double c1 = affineCoeffs[3] + affineCoeffs[4] * i + affineCoeffs[5] * j;
    return { c0, c1 };
}

} // namespace

GridData::GridData( std::array<double, 6> affineCoeffs,
                     std::uint32_t iNodeCount,
                     std::uint32_t jNodeCount,
                     double cellTolerance,
                     double scaleFactor,
                     std::unique_ptr<NodeValueSource> values )
    : affineCoeffs_( affineCoeffs ),
      iNodeCount_( iNodeCount ),
      jNodeCount_( jNodeCount ),
      cellTolerance_( cellTolerance ),
      scaleFactor_( scaleFactor ),
      values_( std::move( values ) )
{
    if( ! values_ ) {
        throw std::invalid_argument( "GridData: values must not be null" );
    }
    if( iNodeCount_ < 2 || jNodeCount_ < 2 ) {
        throw std::invalid_argument( "GridData: iNodeCount and jNodeCount must each be >= 2" );
    }
}

std::optional<Cell> GridData::locate( double c0, double c1 ) const
{
    const auto [i, j] = fractionalIndex( affineCoeffs_, c0, c1 );
    const bool inRange = i >= -cellTolerance_ && i <= static_cast<double>( iNodeCount_ - 1 ) + cellTolerance_
                       && j >= -cellTolerance_ && j <= static_cast<double>( jNodeCount_ - 1 ) + cellTolerance_;
    if( ! inRange ) {
        return std::nullopt;
    }

    // bi/bj are floor(i)/floor(j), clamped to [0, iNodeCount-2]/
    // [0, jNodeCount-2] so the opposite corner (bi+1, bj+1) never reads a
    // node index past the last valid one - needed both for a point exactly
    // on the grid's last row/column, and for a point in the tolerance band
    // just beyond it in either direction, where an unclamped floor() would
    // give -1 or iNodeCount-1. fi/fj (in interpolate()) are the resulting
    // remainder i-bi/j-bj, deliberately left unclamped themselves - see the
    // Cell comment in grid.h for the exact range that produces.
    const std::uint32_t bi = static_cast<std::uint32_t>(
        std::clamp( std::floor( i ), 0.0, static_cast<double>( iNodeCount_ - 2 ) ) );
    const std::uint32_t bj = static_cast<std::uint32_t>(
        std::clamp( std::floor( j ), 0.0, static_cast<double>( jNodeCount_ - 2 ) ) );

    return Cell{ bi, bj, i - static_cast<double>( bi ), j - static_cast<double>( bj ) };
}

std::optional<std::array<double, 3>> GridData::interpolate( const Cell &cell ) const
{
    const auto v00 = values_->nodeValues( cell.bi, cell.bj );
    const auto v10 = values_->nodeValues( cell.bi + 1, cell.bj );
    const auto v01 = values_->nodeValues( cell.bi, cell.bj + 1 );
    const auto v11 = values_->nodeValues( cell.bi + 1, cell.bj + 1 );
    if( ! v00 || ! v10 || ! v01 || ! v11 ) {
        return std::nullopt;
    }

    const double w00 = ( 1.0 - cell.fi ) * ( 1.0 - cell.fj );
    const double w10 = cell.fi * ( 1.0 - cell.fj );
    const double w01 = ( 1.0 - cell.fi ) * cell.fj;
    const double w11 = cell.fi * cell.fj;

    std::array<double, 3> result{};   // value-initialized: every entry starts at 0.0, including any beyond axisCount()
    const std::size_t n = axisCount();
    for( std::size_t a = 0; a < n; ++a ) {
        result[a] = ( w00 * (*v00)[a] + w10 * (*v10)[a] + w01 * (*v01)[a] + w11 * (*v11)[a] ) * scaleFactor_;
    }
    return result;
}

std::size_t GridData::axisCount() const
{
    return values_->axisCount();
}

bool GridData::containsExtentOf( const GridData &other ) const
{
    const double corners[4][2] = {
        { 0.0, 0.0 },
        { static_cast<double>( other.iNodeCount_ - 1 ), 0.0 },
        { 0.0, static_cast<double>( other.jNodeCount_ - 1 ) },
        { static_cast<double>( other.iNodeCount_ - 1 ), static_cast<double>( other.jNodeCount_ - 1 ) }
    };
    for( const auto &corner : corners ) {
        const auto [c0, c1] = nodeToWorld( other.affineCoeffs_, corner[0], corner[1] );
        if( ! locate( c0, c1 ) ) {
            return false;
        }
    }
    return true;
}

Grid::Grid( GridData data )
    : data_( std::move( data ) )
{
}

const GridData &Grid::data() const
{
    return data_;
}

std::optional<Cell> Grid::locate( double c0, double c1 ) const
{
    return data_.locate( c0, c1 );
}

std::optional<std::array<double, 3>> Grid::interpolate( const Cell &cell ) const
{
    return data_.interpolate( cell );
}

std::size_t Grid::axisCount() const
{
    return data_.axisCount();
}

void Grid::addChild( std::unique_ptr<Grid> child )
{
    if( ! child ) {
        throw std::invalid_argument( "Grid::addChild: child must not be null" );
    }
    if( child->axisCount() != axisCount() ) {
        throw std::invalid_argument(
            "Grid::addChild: child axisCount (" + std::to_string( child->axisCount() )
            + ") does not match parent's own (" + std::to_string( axisCount() ) + ")" );
    }
    if( ! data_.containsExtentOf( child->data_ ) ) {
        throw std::invalid_argument( "Grid::addChild: child extent is not contained within parent's own extent" );
    }
    children_.push_back( std::move( child ) );
}

const std::vector<std::unique_ptr<Grid>> &Grid::children() const
{
    return children_;
}

bool Grid::isLeaf() const
{
    return children_.empty();
}

std::optional<Resolution> Grid::resolveChild( double c0, double c1 ) const
{
    return selectAmongSiblings( children_, c0, c1 );
}

std::optional<Resolution> Grid::selectAmongSiblings( const std::vector<std::unique_ptr<Grid>> &siblings, double c0, double c1 )
{
    for( const auto &sibling : siblings ) {
        if( const auto cell = sibling->locate( c0, c1 ) ) {
            return Resolution{ sibling.get(), *cell };
        }
    }
    return std::nullopt;
}

} // namespace deformation
