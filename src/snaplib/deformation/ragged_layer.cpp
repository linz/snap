#include "deformation/ragged_layer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>

namespace deformation {

namespace {

// Combines `value`'s own hash into `seed` - the standard
// boost::hash_combine idiom; std::hash has no built-in way to combine
// several values' own hashes into one.
void hashCombine( std::size_t &seed, std::int64_t value )
{
    seed ^= std::hash<std::int64_t>{}( value ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

// How far (c0 - originC0)/stepC0 (or the c1 equivalent) may fall from an
// exact integer before RaggedLayer::indexRootGrid() concludes a root
// grid's own affine transform isn't really lattice-aligned to the shared
// reference at all, rather than just carrying ordinary floating-point
// noise. Tighter than GridGeometry's own cellTolerance (1e-5 in the same
// units), since this is a data-integrity check, not a boundary-slop
// allowance.
constexpr double kLatticeAlignmentEpsilon = 1e-7;

} // namespace

bool RaggedLayer::RootKey::operator==( const RootKey &other ) const
{
    return row0 == other.row0 && col0 == other.col0 && sizeSteps == other.sizeSteps;
}

std::size_t RaggedLayer::RootKeyHash::operator()( const RootKey &key ) const
{
    std::size_t seed = 0;
    hashCombine( seed, key.row0 );
    hashCombine( seed, key.col0 );
    hashCombine( seed, key.sizeSteps );
    return seed;
}

RaggedLayer::RaggedLayer( double referenceDateDays )
    : referenceDateDays_( referenceDateDays )
{
}

void RaggedLayer::addRootGrid( std::unique_ptr<Grid<RaggedGridData>> grid )
{
    if( ! grid ) {
        throw std::invalid_argument( "RaggedLayer::addRootGrid: grid must not be null" );
    }
    if( ! rootGrids_.empty() && grid->axisCount() != axisCount() ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid axisCount (" + std::to_string( grid->axisCount() )
            + ") does not match layer's own (" + std::to_string( *axisCount() ) + ")" );
    }
    indexRootGrid( *grid );
    rootGrids_.push_back( std::move( grid ) );
}

void RaggedLayer::indexRootGrid( const Grid<RaggedGridData> &grid )
{
    const GridGeometry &geometry = grid.geometry();
    const std::array<double, 6> &affineCoeffs = geometry.affineCoeffs();

    if( affineCoeffs[2] != 0.0 || affineCoeffs[4] != 0.0 ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid's own affine transform is not axis-aligned "
            "(affineCoeffs[2]=" + std::to_string( affineCoeffs[2] ) + ", affineCoeffs[4]="
            + std::to_string( affineCoeffs[4] ) + ") - the root-grid index requires it" );
    }
    if( geometry.iNodeCount() != geometry.jNodeCount() ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid is not square (iNodeCount="
            + std::to_string( geometry.iNodeCount() ) + ", jNodeCount="
            + std::to_string( geometry.jNodeCount() ) + ") - the root-grid index requires it" );
    }

    const double c0Step = affineCoeffs[1];
    const double c1Step = affineCoeffs[5];

    if( ! haveReference_ ) {
        refC0Origin_ = affineCoeffs[0];
        refC0Step_ = c0Step;
        refC1Origin_ = affineCoeffs[3];
        refC1Step_ = c1Step;
        cellTolerance_ = geometry.cellTolerance();
        haveReference_ = true;
    } else if( c0Step != refC0Step_ || c1Step != refC1Step_ ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid's own lattice step (" + std::to_string( c0Step ) + ", "
            + std::to_string( c1Step ) + ") does not match this layer's own first root grid's own ("
            + std::to_string( refC0Step_ ) + ", " + std::to_string( refC1Step_ ) + ")" );
    } else if( geometry.cellTolerance() != cellTolerance_ ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid's own cellTolerance (" + std::to_string( geometry.cellTolerance() )
            + ") does not match this layer's own first root grid's own (" + std::to_string( cellTolerance_ ) + ")" );
    }

    const double rawRow0 = ( affineCoeffs[0] - refC0Origin_ ) / refC0Step_;
    const double rawCol0 = ( affineCoeffs[3] - refC1Origin_ ) / refC1Step_;
    const std::int64_t row0 = static_cast<std::int64_t>( std::llround( rawRow0 ) );
    const std::int64_t col0 = static_cast<std::int64_t>( std::llround( rawCol0 ) );
    if( std::abs( rawRow0 - static_cast<double>( row0 ) ) > kLatticeAlignmentEpsilon
     || std::abs( rawCol0 - static_cast<double>( col0 ) ) > kLatticeAlignmentEpsilon ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: grid's own origin does not align to this layer's own "
            "shared lattice (raw row0=" + std::to_string( rawRow0 ) + ", raw col0="
            + std::to_string( rawCol0 ) + ")" );
    }

    const std::int64_t sizeSteps = static_cast<std::int64_t>( geometry.iNodeCount() ) - 1;
    const RootKey key{ row0, col0, sizeSteps };
    if( rootIndex_.find( key ) != rootIndex_.end() ) {
        throw std::invalid_argument(
            "RaggedLayer::addRootGrid: a grid is already indexed at row0=" + std::to_string( row0 )
            + ", col0=" + std::to_string( col0 ) + ", sizeSteps=" + std::to_string( sizeSteps ) );
    }
    rootIndex_.emplace( key, grid );
    if( std::find( distinctSizeSteps_.begin(), distinctSizeSteps_.end(), sizeSteps ) == distinctSizeSteps_.end() ) {
        distinctSizeSteps_.push_back( sizeSteps );
    }
}

std::optional<std::size_t> RaggedLayer::axisCount() const
{
    if( rootGrids_.empty() ) {
        return std::nullopt;
    }
    return rootGrids_.front()->axisCount();
}

std::optional<Resolution<RaggedGridData>> RaggedLayer::resolveRootGrid( double c0, double c1 ) const
{
    if( ! haveReference_ ) {
        return std::nullopt;
    }

    const double rawRow = ( c0 - refC0Origin_ ) / refC0Step_;
    const double rawCol = ( c1 - refC1Origin_ ) / refC1Step_;

    for( const std::int64_t size : distinctSizeSteps_ ) {
        const auto floorToSize = []( double raw, std::int64_t s ) {
            return static_cast<std::int64_t>( std::floor( raw / static_cast<double>( s ) ) ) * s;
        };
        const std::int64_t primaryRow0 = floorToSize( rawRow, size );
        const std::int64_t primaryCol0 = floorToSize( rawCol, size );

        // Same-size neighbours worth trying too, only when (c0, c1) sits
        // within cellTolerance_ of the relevant edge - see this class's
        // own resolveRootGrid() doc comment for why a plain single
        // floor-division candidate isn't always enough.
        std::vector<std::int64_t> rowCandidates{ primaryRow0 };
        if( rawRow - static_cast<double>( primaryRow0 ) <= cellTolerance_ ) {
            rowCandidates.push_back( primaryRow0 - size );
        }
        if( static_cast<double>( primaryRow0 + size ) - rawRow <= cellTolerance_ ) {
            rowCandidates.push_back( primaryRow0 + size );
        }
        std::vector<std::int64_t> colCandidates{ primaryCol0 };
        if( rawCol - static_cast<double>( primaryCol0 ) <= cellTolerance_ ) {
            colCandidates.push_back( primaryCol0 - size );
        }
        if( static_cast<double>( primaryCol0 + size ) - rawCol <= cellTolerance_ ) {
            colCandidates.push_back( primaryCol0 + size );
        }

        for( const std::int64_t row0 : rowCandidates ) {
            for( const std::int64_t col0 : colCandidates ) {
                const auto found = rootIndex_.find( RootKey{ row0, col0, size } );
                if( found == rootIndex_.end() ) {
                    continue;
                }
                if( const auto cell = found->second.get().locate( c0, c1 ) ) {
                    return Resolution<RaggedGridData>{ found->second, *cell };
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<std::array<double, 3>> RaggedLayer::evaluate( double c0, double c1, double queryDateDays ) const
{
    auto resolution = resolveRootGrid( c0, c1 );
    if( ! resolution ) {
        return std::nullopt;
    }
    resolution = descendToLeaf( *resolution, c0, c1 );

    const std::int64_t queryDayCount = static_cast<std::int64_t>( queryDateDays - referenceDateDays_ );
    return resolution->grid.get().data().evaluate( resolution->cell, queryDayCount );
}

} // namespace deformation
