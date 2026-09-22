#include "snapconfig.hpp"

#include "util/fieldscanner.hpp"
#include "util/snapctype.h"
#include <algorithm>
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

std::optional<std::string_view> FieldScanner::quotedValue( const std::string_view::const_iterator start, const char quoteChar,
    const std::optional<std::vector<QuoteFollowOption>> &followOptions )
{
    auto pos=start;
    while( pos != _text.end() )
    {
        if( *pos == quoteChar )
        {
            auto afterQuote=pos+1;
            bool followOk = ! followOptions.has_value() || std::any_of( followOptions->begin(), followOptions->end(),
                [&]( const QuoteFollowOption opt )
                {
                    return ( opt == QuoteFollowOption::End && afterQuote == _text.end() )
                        || ( opt == QuoteFollowOption::Whitespace && afterQuote != _text.end() && ISSPACE(*afterQuote) );
                } );
            if( followOk )
            {
                _pos=afterQuote;
                return span( start, pos );
            }
            break;
        }
        ++pos;
    }
    _pos = ( pos == _text.end() ) ? pos : pos+1;
    return std::nullopt;
}

std::string_view::const_iterator FieldScanner::isQuoted( const std::string_view field, const bool onlyDoubleQuote )
{
    if( ! field.empty() && ( field.front() == '"' || ( !onlyDoubleQuote && field.front() == '\'' ) ) )
    {
        return field.begin();
    }
    return field.end();
}

std::optional<std::string_view> FieldScanner::checkAndRecoverQuotedValue( const bool onlyDoubleQuote,
    const std::optional<std::vector<QuoteFollowOption>> &followOptions )
{
    while( _pos != _text.end() && ISSPACE(*_pos) ) ++_pos;
    if( _pos == _text.end() ) return std::nullopt;

    const std::string_view upcoming = remainder();
    auto quoteStart = isQuoted( upcoming, onlyDoubleQuote );
    if( quoteStart != upcoming.end() )
    {
        return quotedValue( quoteStart+1, *quoteStart, followOptions );
    }
    return next();
}

bool parse_positive_double( std::string_view field, double &value )
{
    const char *begin = field.data();
    const char *end = begin + field.size();
    auto result = std::from_chars( begin, end, value );
    return result.ec == std::errc() && result.ptr == end && value > 0.0;
}
