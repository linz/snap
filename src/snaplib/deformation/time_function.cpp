#include "deformation/time_function.h"

#include <stdexcept>
#include <string>

namespace deformation {

namespace {

// Names a TimeFunctionShape for an exception message - "unknown" is
// unreachable for a valid enumerator, but avoids undefined behaviour if the
// underlying byte is ever something else.
const char *shapeName( TimeFunctionShape shape )
{
    switch( shape ) {
    case TimeFunctionShape::Linear: return "Linear";
    case TimeFunctionShape::Step: return "Step";
    case TimeFunctionShape::Ramp: return "Ramp";
    case TimeFunctionShape::Quadratic: return "Quadratic";
    case TimeFunctionShape::Exponential: return "Exponential";
    case TimeFunctionShape::LogBaseE: return "LogBaseE";
    case TimeFunctionShape::LogBase10: return "LogBase10";
    case TimeFunctionShape::Cyclic: return "Cyclic";
    case TimeFunctionShape::HyperbolicTangent: return "HyperbolicTangent";
    }
    return "unknown";
}

} // namespace

TimeFunction::TimeFunction( const Params &params )
    : params_( params )
{
    switch( params_.shape ) {
    case TimeFunctionShape::Linear:
        if( ! params_.functionReferenceDateDays ) {
            throw std::invalid_argument( "TimeFunction: Linear requires functionReferenceDateDays" );
        }
        break;
    case TimeFunctionShape::Step:
        if( ! params_.eventDateDays ) {
            throw std::invalid_argument( "TimeFunction: Step requires eventDateDays" );
        }
        break;
    case TimeFunctionShape::Ramp:
        if( ! params_.startDateDays || ! params_.endDateDays ) {
            throw std::invalid_argument( "TimeFunction: Ramp requires startDateDays and endDateDays" );
        }
        break;
    default:
        throw std::invalid_argument(
            "TimeFunction: shape " + std::string( shapeName( params_.shape ) )
            + " is not implemented (only Linear/Step/Ramp are)" );
    }
}

double TimeFunction::deltaAt( double dateDays ) const
{
    switch( params_.shape ) {
    case TimeFunctionShape::Linear:
        return dateDays - *params_.functionReferenceDateDays;
    case TimeFunctionShape::Step:
        return dateDays - *params_.eventDateDays;
    case TimeFunctionShape::Ramp:
        return dateDays - *params_.startDateDays;
    default:
        // Unreachable - the constructor already rejected every other shape.
        throw std::logic_error(
            "TimeFunction::deltaAt: shape " + std::string( shapeName( params_.shape ) ) + " is not implemented" );
    }
}

double TimeFunction::refFunc( double delta ) const
{
    switch( params_.shape ) {
    case TimeFunctionShape::Linear:
        if( params_.startDateDays && delta < 0.0 ) {
            return 0.0;
        }
        return delta;
    case TimeFunctionShape::Step:
        return delta >= 0.0 ? 1.0 : 0.0;
    case TimeFunctionShape::Ramp: {
        const double deltaEnd = *params_.endDateDays - *params_.startDateDays;
        if( delta < 0.0 ) {
            return 0.0;
        }
        if( delta >= deltaEnd ) {
            return 1.0;
        }
        return delta / deltaEnd;
    }
    default:
        // Unreachable - the constructor already rejected every other shape.
        throw std::logic_error(
            "TimeFunction::refFunc: shape " + std::string( shapeName( params_.shape ) ) + " is not implemented" );
    }
}

double TimeFunction::evaluate( double queryDateDays ) const
{
    double result = refFunc( deltaAt( queryDateDays ) );
    if( params_.functionReferenceDateDays ) {
        result -= refFunc( deltaAt( *params_.functionReferenceDateDays ) );
    }
    return result * params_.scaleFactor;
}

} // namespace deformation
