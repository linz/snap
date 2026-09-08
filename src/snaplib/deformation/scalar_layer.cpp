#include "deformation/scalar_layer.h"

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
    // Plain linear scan for the root grid containing (c0, c1) - the first
    // one found wins; see rootGrids_'s own doc comment above for why list
    // order, not this method, is what makes that meaningful when two roots
    // do overlap.
    std::optional<Resolution<ScalarGridData>> resolution;
    for( const auto &root : rootGrids_ ) {
        if( const auto cell = root->locate( c0, c1 ) ) {
            resolution = Resolution<ScalarGridData>{ *root, *cell };
            break;
        }
    }
    if( ! resolution ) {
        return std::nullopt;
    }
    resolution = descendToLeaf( *resolution, c0, c1 );

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
