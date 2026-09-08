#include "deformation/scalarlayer.h"

#include <stdexcept>
#include <string>

namespace deformation {

void ScalarLayer::addRootGrid( std::unique_ptr<Grid<ScalarGridData>> grid )
{
    if( ! grid ) {
        throw std::invalid_argument( "ScalarLayer::addRootGrid: grid must not be null" );
    }
    if( ! rootGrids_.empty() && grid->axisCount() != axisCount() ) {
        throw std::invalid_argument(
            "ScalarLayer::addRootGrid: grid axisCount (" + std::to_string( grid->axisCount() )
            + ") does not match layer's own (" + std::to_string( *axisCount() ) + ")" );
    }
    rootGrids_.push_back( std::move( grid ) );
}

void ScalarLayer::addTimeFunction( const TimeFunction &timeFunction )
{
    timeFunctions_.push_back( timeFunction );
}

std::optional<std::size_t> ScalarLayer::axisCount() const
{
    if( rootGrids_.empty() ) {
        return std::nullopt;
    }
    return rootGrids_.front()->axisCount();
}

std::optional<std::array<double, 3>> ScalarLayer::evaluate( double c0, double c1, double queryDateDays ) const
{
    auto resolution = Grid<ScalarGridData>::selectAmongSiblings( rootGrids_, c0, c1 );
    if( ! resolution ) {
        return std::nullopt;
    }
    while( auto childResolution = resolution->grid.get().resolveChild( c0, c1 ) ) {
        resolution = childResolution;
    }

    auto values = resolution->grid.get().data().interpolate( resolution->cell );
    if( ! values ) {
        return std::nullopt;
    }

    double factor = 0.0;
    for( const auto &timeFunction : timeFunctions_ ) {
        factor += timeFunction.evaluate( queryDateDays );
    }

    const std::size_t n = resolution->grid.get().axisCount();
    for( std::size_t a = 0; a < n; ++a ) {
        (*values)[a] *= factor;
    }
    return values;
}

} // namespace deformation
