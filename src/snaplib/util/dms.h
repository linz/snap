#ifndef _DMS_H
#define _DMS_H

/*
   $Log: dms.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:57:21  CHRIS
   Initial revision

*/

/* dms.h - header file for dms.c, degrees, minutes, seconds conversion
   routines */

struct DMS
{
    int    deg;
    int    min;
    double sec;
    char   neg;          /*  TRUE = negative, FALSE = positive */
};

double dms_deg( DMS *dms );
DMS *deg_dms( double d, DMS *dms );

#define DMSF_FMT_PREFIX_HEM 1
#define DMSF_FMT_SUFFIX_HEM 0
#define DMSF_FMT_DMS 0
#define DMSF_FMT_DM  2
#define DMSF_FMT_DEG 4
#define DMSF_FMT_INPUT_RADIANS 8

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

/// How an angle in degrees is written as text, and the writing of it.
///
/// A text argument that is not supplied takes its default, which is not the
/// same as supplying an empty string: the text after the degrees defaults to
/// a blank, and so does the text after the minutes for a degrees, minutes and
/// seconds format (it defaults to nothing for the other formats).
class DmsFormat
{
public:
    DmsFormat(
        std::size_t degreeDigits,                          ///< width the degrees are right-justified to, 0 to 4
        int decimalPlaces,                                 ///< decimal places on the last component shown
        int flags = 0,                                     ///< DMSF_FMT_ values combined with |
        const std::optional<std::string_view> &afterDegrees = std::nullopt,
        const std::optional<std::string_view> &afterMinutes = std::nullopt,
        const std::optional<std::string_view> &afterSeconds = std::nullopt,
        const std::optional<std::string_view> &positiveSign = std::nullopt,  ///< hemisphere text for a positive angle
        const std::optional<std::string_view> &negativeSign = std::nullopt   ///< hemisphere text for a negative angle
        );

    /// Writes the angle, which is in degrees unless the format was created
    /// with DMSF_FMT_INPUT_RADIANS.
    std::string format( double angle ) const;

private:
    /// Which components are shown, from the DMSF_FMT_DM and DMSF_FMT_DEG flags
    enum class Components { DegreesMinutesSeconds, DegreesMinutes, Degrees };
    /// Where the positive/negative text goes. LeadingMinus is the case where
    /// the positive text is empty and the negative text is "-" with the
    /// prefix flag set, which writes the minus sign in front of the degrees.
    enum class SignPlacement { Suffix, Prefix, LeadingMinus };

    /// Components shown for the flags. DMSF_FMT_DEG takes precedence over DMSF_FMT_DM.
    static Components _componentsFor( int flags );
    /// The requested decimal places, limited to the most the components can usefully show.
    static int _decimalPlacesFor( int decimalPlaces, Components components );
    /// Half of the last digit shown, in degrees: added to the angle so that it rounds rather than truncates.
    static double _roundingOffsetFor( int decimalPlaces, Components components );
    /// Where the sign text goes for the flags. This is LeadingMinus when it is written before the
    /// degrees as a bare "-": the prefix flag, no positive text, and "-" as the negative text.
    static SignPlacement _signPlacementFor(
        int flags,
        const std::optional<std::string_view> &positiveSign,
        const std::optional<std::string_view> &negativeSign );

    // Declared in the order they depend on each other, as they are initialised in this order.
    const std::size_t _degreeDigits;    ///< width the degrees are right-justified to
    const Components _components;       ///< which of degrees, minutes and seconds are shown
    const int _decimalPlaces;           ///< decimal places on the last component shown
    const double _roundingOffset;       ///< added to the angle before splitting, so the last digit rounds rather than truncates
    const bool _inputRadians;           ///< the angle passed to format() is in radians
    const SignPlacement _signPlacement; ///< where _positiveSign/_negativeSign are written
    const std::string _afterDegrees;    ///< text between degrees and minutes
    const std::string _afterMinutes;    ///< text between minutes and seconds (after the minutes for DegreesMinutes)
    const std::string _afterSeconds;    ///< text after the seconds
    const std::string _positiveSign;    ///< hemisphere text for a positive angle
    const std::string _negativeSign;    ///< hemisphere text for a negative angle, empty for LeadingMinus
};

/// Writes the angle as format.format() does.
std::string dms_string( double angle, const DmsFormat &format );

#endif
