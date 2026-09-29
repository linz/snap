#include "snapconfig.hpp"

#include "util/fieldscanner.hpp"
#include "util/snapctype.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>

std::optional<std::string_view> FieldScanner::next()
{
    _pos = std::find_if( _pos, _text.end(), []( const char c ){ return ! ISSPACE(c); } );
    if( _pos == _text.end() ) return std::nullopt;
    auto start = _pos;
    _pos = std::find_if( _pos, _text.end(), []( const char c ){ return ISSPACE(c); } );
    return _span( start, _pos );
}

std::optional<std::string_view> FieldScanner::next( const char delimiter )
{
    auto found = std::find( _pos, _text.end(), delimiter );
    if( found == _text.end() ) return std::nullopt;
    auto field = _span( _pos, found );
    _pos = found + 1;
    return field;
}

std::string_view FieldScanner::remainder() const
{
    return _span( _pos, _text.end() );
}

std::string_view FieldScanner::_span( std::string_view::const_iterator start, std::string_view::const_iterator end ) const
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
                return _span( start, pos );
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

std::optional<double> FieldScanner::checkAndRecoverQuotedDoubleValue( const bool onlyDoubleQuote,
    const std::optional<std::vector<QuoteFollowOption>> &followOptions )
{
    auto field = checkAndRecoverQuotedValue( onlyDoubleQuote, followOptions );
    if( ! field ) return std::nullopt;
    return parse_double( *field );
}

template <typename T>
std::optional<ParsedField<T>> parse_leading_field( std::string_view field )
{
    // std::from_chars doesn't accept a leading '+' - matches the same
    // handling in readcfg.cpp's store_numeric_config_value.
    if( ! field.empty() && field.front() == '+' ) field.remove_prefix(1);
    ParsedField<T> r{};
    r.result = std::from_chars( field.data(), field.data() + field.size(), r.value );
    if( r.result.ec != std::errc() ) return std::nullopt;
    return r;
}
template std::optional<ParsedField<int>> parse_leading_field<int>( std::string_view );
template std::optional<ParsedField<long>> parse_leading_field<long>( std::string_view );
template std::optional<ParsedField<double>> parse_leading_field<double>( std::string_view );

template <typename T>
std::optional<T> parse_leading( std::string_view field )
{
    const auto r = parse_leading_field<T>( field );
    if( ! r ) return std::nullopt;
    return r->value;
}
template std::optional<int> parse_leading<int>( std::string_view );
template std::optional<long> parse_leading<long>( std::string_view );
template std::optional<double> parse_leading<double>( std::string_view );

std::optional<double> parse_double( std::string_view field )
{
    const auto r = parse_leading_field<double>( field );
    if( ! r || r->result.ptr != field.data() + field.size() ) return std::nullopt;
    return r->value;
}

std::optional<double> parse_positive_double( std::string_view field )
{
    auto value = parse_double( field );
    if( value && *value <= 0.0 ) return std::nullopt;
    return value;
}

int compare_ignoring_case( std::string_view string1, std::string_view string2 )
{
    const auto foldedLess = []( unsigned char a, unsigned char b ) { return std::tolower(a) < std::tolower(b); };
    if( std::lexicographical_compare( string1.begin(), string1.end(), string2.begin(), string2.end(), foldedLess ) ) return -1;
    return std::lexicographical_compare( string2.begin(), string2.end(), string1.begin(), string1.end(), foldedLess ) ? 1 : 0;
}

void copy_field( std::string_view field, char *buf, int nbuf )
{
    int length = (int) field.size();
    if( length >= nbuf ) length = nbuf-1;
    memcpy( buf, field.data(), length );
    buf[length] = 0;
}
