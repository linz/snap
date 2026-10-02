#ifndef _TEXTFORMAT_HPP
#define _TEXTFORMAT_HPP

#include <iomanip>
#include <sstream>
#include <string>

/// Functions that write numbers as text, replacing sprintf into a fixed buffer.
/// They are written with streams because the project builds as C++17. From C++20
/// std::format does the same job, and these can be replaced by calls to it.

/// Writes a number with a fixed number of decimal places, as printf's "%.*f"
/// does. Padding with a space goes before the sign, as in printf. Padding
/// with any other fill goes between the sign and the digits, as printf's 0
/// flag does for '0'.
inline std::string format_fixed(
    const double value,             ///< the number to write
    const int decimalPlaces,        ///< digits after the decimal point, none and no point if 0
    const int minimumWidth = 0,     ///< the result is padded on the left to at least this many characters
    const char fill = ' ' )         ///< the character used for padding
{
    std::ostringstream text;
    text << std::fixed << std::setprecision( decimalPlaces ) << std::setfill( fill );
    if( fill != ' ' ) text << std::internal;
    text << std::setw( minimumWidth ) << value;
    return text.str();
}

/// Writes a number in scientific notation with a fixed number of decimal
/// places, as printf's "%.*e" does.
inline std::string format_scientific(
    const double value,             ///< the number to write
    const int decimalPlaces )       ///< digits after the decimal point in the mantissa
{
    std::ostringstream text;
    text << std::scientific << std::setprecision( decimalPlaces ) << value;
    return text.str();
}

#endif
