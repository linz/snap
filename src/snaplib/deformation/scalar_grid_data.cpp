#include "deformation/scalar_grid_data.h"

#include <stdexcept>

namespace deformation {

ScalarGridData::ScalarGridData( double scaleFactor, std::unique_ptr<ScalarNodeValueSource> values )
    : scaleFactor_( scaleFactor ),
      values_( std::move( values ) )
{
    if( ! values_ ) {
        throw std::invalid_argument( "ScalarGridData: values must not be null" );
    }
    if( values_->hasEndDay() ) {
        throw std::runtime_error(
            "ScalarGridData: has_end_day/own_end_date terminal-node termination is not implemented" );
    }
}

std::optional<std::array<double, 3>> ScalarGridData::interpolate( const Cell &cell ) const
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

std::size_t ScalarGridData::axisCount() const
{
    return values_->axisCount();
}

} // namespace deformation
