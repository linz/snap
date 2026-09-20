#include "snapconfig.hpp"

#include "util/fieldscanner.hpp"
#include "util/snapctype.h"
#include <charconv>

std::optional<std::string_view> FieldScanner::next()
{
    while( _pos != _text.end() && ISSPACE(*_pos) ) ++_pos;
    if( _pos == _text.end() ) return std::nullopt;
    auto start = _pos;
    while( _pos != _text.end() && ! ISSPACE(*_pos) ) ++_pos;
    return span( start, _pos );
}

std::string_view FieldScanner::remainder() const
{
    return span( _pos, _text.end() );
}

std::string_view FieldScanner::span( std::string_view::const_iterator start, std::string_view::const_iterator end ) const
{
    /* Computed via iterator subtraction and pointer arithmetic on
       _text.data(), not by dereferencing start/end - either may legitimately
       be _text.end(), and dereferencing a non-dereferenceable iterator (even
       just to take its address) is undefined behavior. */
    return std::string_view( _text.data() + std::distance( _text.begin(), start ),
                              std::distance( start, end ) );
}

bool parse_positive_double( std::string_view field, double &value )
{
    const char *begin = field.data();
    const char *end = begin + field.size();
    auto result = std::from_chars( begin, end, value );
    return result.ec == std::errc() && result.ptr == end && value > 0.0;
}
