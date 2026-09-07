#include "deformation/raggedgriddata.h"

#include <stdexcept>
#include <string>

namespace deformation {

namespace {

// Throws if node (i, j) has a real own end day - see
// RaggedNodeValueSource::hasEndDay(). Naming the specific node lets a
// caller trace which corner of which grid triggered it, rather than just
// knowing it happened somewhere.
void checkNoEndDay( const RaggedNodeValueSource &values, std::uint32_t i, std::uint32_t j )
{
    if( values.hasEndDay( i, j ) ) {
        throw std::runtime_error(
            "RaggedGridData::evaluate: own_end_day is not implemented (node ("
            + std::to_string( i ) + ", " + std::to_string( j ) + "))" );
    }
}

} // namespace

RaggedGridData::RaggedGridData( std::unique_ptr<RaggedNodeValueSource> values )
    : values_( std::move( values ) )
{
    if( ! values_ ) {
        throw std::invalid_argument( "RaggedGridData: values must not be null" );
    }
}

std::size_t RaggedGridData::axisCount() const
{
    return values_->axisCount();
}

double RaggedGridData::evaluateNode( std::uint32_t i, std::uint32_t j, std::size_t axis, std::int64_t queryDayCount ) const
{
    double value = 0.0;
    for( const auto &segment : values_->segments( i, j, axis ) ) {
        if( segment.startDay > queryDayCount ) {
            break;
        }
        value = segment.startValue + segment.ratePerDay * static_cast<double>( queryDayCount - segment.startDay );
    }
    return value;
}

std::array<double, 3> RaggedGridData::evaluate( const Cell &cell, std::int64_t queryDayCount ) const
{
    checkNoEndDay( *values_, cell.bi, cell.bj );
    checkNoEndDay( *values_, cell.bi + 1, cell.bj );
    checkNoEndDay( *values_, cell.bi, cell.bj + 1 );
    checkNoEndDay( *values_, cell.bi + 1, cell.bj + 1 );

    const double w00 = ( 1.0 - cell.fi ) * ( 1.0 - cell.fj );
    const double w10 = cell.fi * ( 1.0 - cell.fj );
    const double w01 = ( 1.0 - cell.fi ) * cell.fj;
    const double w11 = cell.fi * cell.fj;

    std::array<double, 3> result{};   // value-initialized: every entry starts at 0.0, including any beyond axisCount()
    const std::size_t n = axisCount();
    for( std::size_t a = 0; a < n; ++a ) {
        const double v00 = evaluateNode( cell.bi, cell.bj, a, queryDayCount );
        const double v10 = evaluateNode( cell.bi + 1, cell.bj, a, queryDayCount );
        const double v01 = evaluateNode( cell.bi, cell.bj + 1, a, queryDayCount );
        const double v11 = evaluateNode( cell.bi + 1, cell.bj + 1, a, queryDayCount );
        result[a] = w00 * v00 + w10 * v10 + w01 * v01 + w11 * v11;
    }
    return result;
}

} // namespace deformation
