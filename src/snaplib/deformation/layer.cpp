#include "deformation/layer.h"

#include <stdexcept>
#include <string>

namespace deformation {

void Layer::addRootGrid( std::unique_ptr<Grid> grid )
{
    if( ! grid ) {
        throw std::invalid_argument( "Layer::addRootGrid: grid must not be null" );
    }
    if( ! rootGrids_.empty() && grid->axisCount() != axisCount() ) {
        throw std::invalid_argument(
            "Layer::addRootGrid: grid axisCount (" + std::to_string( grid->axisCount() )
            + ") does not match layer's own (" + std::to_string( *axisCount() ) + ")" );
    }
    rootGrids_.push_back( std::move( grid ) );
}

void Layer::addTimeFunction( TimeFunction timeFunction )
{
    timeFunctions_.push_back( std::move( timeFunction ) );
}

std::optional<std::size_t> Layer::axisCount() const
{
    if( rootGrids_.empty() ) {
        return std::nullopt;
    }
    return rootGrids_.front()->axisCount();
}

std::optional<std::array<double, 3>> Layer::evaluate( double c0, double c1, double queryDateDays ) const
{
    auto resolution = Grid::selectAmongSiblings( rootGrids_, c0, c1 );
    if( ! resolution ) {
        return std::nullopt;
    }
    while( auto childResolution = resolution->grid->resolveChild( c0, c1 ) ) {
        resolution = childResolution;
    }

    auto values = resolution->grid->interpolate( resolution->cell );
    if( ! values ) {
        return std::nullopt;
    }

    double factor = 0.0;
    for( const auto &timeFunction : timeFunctions_ ) {
        factor += timeFunction.evaluate( queryDateDays );
    }

    const std::size_t n = resolution->grid->axisCount();
    for( std::size_t a = 0; a < n; ++a ) {
        (*values)[a] *= factor;
    }
    return values;
}

} // namespace deformation
