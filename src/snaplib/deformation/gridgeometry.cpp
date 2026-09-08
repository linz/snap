#include "deformation/gridgeometry.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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

GridGeometry::GridGeometry( const std::array<double, 6> &affineCoeffs,
                             std::uint32_t iNodeCount,
                             std::uint32_t jNodeCount,
                             double cellTolerance )
    : affineCoeffs_( affineCoeffs ),
      iNodeCount_( iNodeCount ),
      jNodeCount_( jNodeCount ),
      cellTolerance_( cellTolerance )
{
    if( iNodeCount_ < 2 || jNodeCount_ < 2 ) {
        throw std::invalid_argument( "GridGeometry: iNodeCount and jNodeCount must each be >= 2" );
    }
}

std::optional<Cell> GridGeometry::locate( double c0, double c1 ) const
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
    // give -1 or iNodeCount-1. fi/fj (in GridData::interpolate()/
    // RaggedNodeData::evaluate()) are the resulting remainder i-bi/j-bj,
    // deliberately left unclamped themselves - see the Cell comment in
    // gridgeometry.h for the exact range that produces.
    const std::uint32_t bi = static_cast<std::uint32_t>(
        std::clamp( std::floor( i ), 0.0, static_cast<double>( iNodeCount_ - 2 ) ) );
    const std::uint32_t bj = static_cast<std::uint32_t>(
        std::clamp( std::floor( j ), 0.0, static_cast<double>( jNodeCount_ - 2 ) ) );

    return Cell{ bi, bj, i - static_cast<double>( bi ), j - static_cast<double>( bj ) };
}

bool GridGeometry::containsExtentOf( const GridGeometry &other ) const
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

std::uint32_t GridGeometry::iNodeCount() const
{
    return iNodeCount_;
}

std::uint32_t GridGeometry::jNodeCount() const
{
    return jNodeCount_;
}

const std::array<double, 6> &GridGeometry::affineCoeffs() const
{
    return affineCoeffs_;
}

double GridGeometry::cellTolerance() const
{
    return cellTolerance_;
}

} // namespace deformation
