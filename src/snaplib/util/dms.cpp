#include "snapconfig.h"
/*
   $Log: dms.c,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:56:57  CHRIS
   Initial revision

*/

#include <algorithm>
#include <cmath>
#include <string>

#include "util/dms.h"
#include "util/pi.h"
#include "util/textformat.hpp"

/*------------------------------------------------------------------*/
/*  Angle format conversion routines - DMS to radians and           */
/*  vice-versa.  Uses the structure DMS defined in geodetic.h       */
/*  as                                                              */
/*                                                                  */
/*  typedef struct {                                                */
/*              int    deg                                          */
/*              int    min                                          */
/*              double sec                                          */
/*              char   neg   (TRUE = negative angle )               */
/*              } DMS;                                              */
/*                                                                  */
/*  dms_deg     Converts degrees, minutes, seconds to radians       */
/*  deg_dms     Converts radians to degrees, minutes, seconds       */
/*------------------------------------------------------------------*/


double dms_deg( DMS *dms )
{
    double d;
    d = dms->deg + dms->min/60.0 + dms->sec/3600.0;
    if( dms->neg ) d = -d;
    return d;
}

DMS *deg_dms( double d, DMS *dms )
{
    dms->neg = d<0.0;
    if (dms->neg) d = -d;
    d -= (dms->deg = floor(d));
    d *= 60.0;
    d -= (dms->min = floor(d));
    dms->sec = d*60.0;
    return dms;
}

namespace
{

// The most decimal places that can be shown on the last component, by components shown.
constexpr int maximumDecimalPlacesSeconds = 6;
constexpr int maximumDecimalPlacesMinutes = 8;
constexpr int maximumDecimalPlacesDegrees = 10;

constexpr std::size_t maximumDegreeDigits = 4;

}

DmsFormat::Components DmsFormat::_componentsFor( const int flags )
{
    if( flags & DMSF_FMT_DEG ) return Components::Degrees;
    if( flags & DMSF_FMT_DM ) return Components::DegreesMinutes;
    return Components::DegreesMinutesSeconds;
}

int DmsFormat::_decimalPlacesFor( const int decimalPlaces, const Components components )
{
    switch( components )
    {
    case Components::Degrees: return std::clamp( decimalPlaces, 0, maximumDecimalPlacesDegrees );
    case Components::DegreesMinutes: return std::clamp( decimalPlaces, 0, maximumDecimalPlacesMinutes );
    case Components::DegreesMinutesSeconds: break;
    }
    return std::clamp( decimalPlaces, 0, maximumDecimalPlacesSeconds );
}

double DmsFormat::_roundingOffsetFor( const int decimalPlaces, const Components components )
{
    if( components == Components::Degrees ) return 0.0;
    double offset = 0.5;
    for( int i = decimalPlaces; i--; ) offset /= 10;
    return offset / (components == Components::DegreesMinutes ? 60.0 : 3600.0);
}

DmsFormat::SignPlacement DmsFormat::_signPlacementFor(
    const int flags,
    const std::optional<std::string_view> &positiveSign,
    const std::optional<std::string_view> &negativeSign )
{
    if( ! (flags & DMSF_FMT_PREFIX_HEM) ) return SignPlacement::Suffix;
    if( positiveSign.value_or( "" ).empty() && negativeSign.value_or( "" ) == "-" ) return SignPlacement::LeadingMinus;
    return SignPlacement::Prefix;
}

DmsFormat::DmsFormat(
    const std::size_t degreeDigits,
    const int decimalPlaces,
    const int flags,
    const std::optional<std::string_view> &afterDegrees,
    const std::optional<std::string_view> &afterMinutes,
    const std::optional<std::string_view> &afterSeconds,
    const std::optional<std::string_view> &positiveSign,
    const std::optional<std::string_view> &negativeSign )
    : _degreeDigits( std::min( degreeDigits, maximumDegreeDigits ) ),
      _components( _componentsFor( flags ) ),
      _decimalPlaces( _decimalPlacesFor( decimalPlaces, _components ) ),
      _roundingOffset( _roundingOffsetFor( _decimalPlaces, _components ) ),
      _inputRadians( (flags & DMSF_FMT_INPUT_RADIANS) != 0 ),
      _signPlacement( _signPlacementFor( flags, positiveSign, negativeSign ) ),
      _afterDegrees( afterDegrees.value_or( " " ) ),
      _afterMinutes( afterMinutes.value_or( _components == Components::DegreesMinutesSeconds ? " " : "" ) ),
      _afterSeconds( afterSeconds.value_or( "" ) ),
      _positiveSign( positiveSign.value_or( "" ) ),
      _negativeSign( _signPlacement == SignPlacement::LeadingMinus ? "" : negativeSign.value_or( "" ) )
{
}

std::string DmsFormat::format( const double angle ) const
{
    const double signedDegrees = _inputRadians ? angle * RTOD : angle;
    const bool negative = signedDegrees < 0;
    const double degreesValue = negative ? -signedDegrees : signedDegrees;
    const std::string &sign = negative ? _negativeSign : _positiveSign;
    const bool leadingMinus = negative && _signPlacement == SignPlacement::LeadingMinus;

    std::string text;
    if( _signPlacement == SignPlacement::Prefix ) text += sign;

    if( _components == Components::Degrees )
    {
        text += format_fixed( leadingMinus ? -degreesValue : degreesValue, _decimalPlaces );
    }
    else
    {
        const bool showsMinutes = _components == Components::DegreesMinutesSeconds;
        const double roundedDegrees = degreesValue + _roundingOffset;
        const int degrees = static_cast<int>( std::floor( roundedDegrees ) );
        const double fractionOfDegree = roundedDegrees - degrees;
        const double minutesAndFraction = fractionOfDegree * 60.0;
        const int minutes = showsMinutes ? static_cast<int>( std::floor( minutesAndFraction ) ) : 0;
        const double lastFraction = showsMinutes ? minutesAndFraction - minutes : fractionOfDegree;
        const double lastOffset = showsMinutes ? _roundingOffset * 60.0 * 60.0 : _roundingOffset * 60.0;
        const double lastComponent = std::max( lastFraction * 60.0 - lastOffset, 0.0 );

        std::string degreesText = (leadingMinus ? "-" : "") + std::to_string( degrees );
        if( degreesText.size() < _degreeDigits ) degreesText.insert( 0, _degreeDigits - degreesText.size(), ' ' );
        text += degreesText + _afterDegrees;
        if( showsMinutes )
        {
            text += format_fixed( minutes, 0, 2, '0' ) + _afterMinutes;
        }
        const int lastWidth = _decimalPlaces ? _decimalPlaces + 3 : _decimalPlaces + 2;
        text += format_fixed( lastComponent, _decimalPlaces, lastWidth, '0' ) + (showsMinutes ? _afterSeconds : _afterMinutes);
    }

    if( _signPlacement == SignPlacement::Suffix ) text += sign;
    return text;
}

std::string dms_string( const double angle, const DmsFormat &format )
{
    return format.format( angle );
}
