#include "snapconfig.hpp"

#include "util/fieldscanner.hpp"
#include "util/snapctype.h"
#include "util/pi.h"
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

std::optional<std::string_view> FieldScanner::nextToken( const char delimiter )
{
    _pos = std::find_if( _pos, _text.end(), [delimiter]( const char c ){ return c != delimiter; } );
    if( _pos == _text.end() ) return std::nullopt;
    const auto start = _pos;
    _pos = std::find( _pos, _text.end(), delimiter );
    return _span( start, _pos );
}

std::string_view FieldScanner::remainder() const
{
    return _span( _pos, _text.end() );
}

bool FieldScanner::skipIfNext( const char c )
{
    const auto next = std::find_if( _pos, _text.end(), []( const char ch ){ return ! ISSPACE(ch); } );
    if( next == _text.end() || *next != c ) return false;
    _pos = next + 1;
    return true;
}

bool FieldScanner::atEnd() const
{
    return std::all_of( _pos, _text.end(), []( const char c ){ return ISSPACE(c); } );
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

int compare_ignoring_case( std::string_view string1, std::string_view string2, size_t maxLength )
{
    return compare_ignoring_case( string1.substr( 0, maxLength ), string2.substr( 0, maxLength ) );
}

/// Reduces a character to the form used when comparing names. Letters become
/// lower case, and spaces, control characters, DEL and characters outside
/// ASCII all become an underscore. Other characters are unchanged.
static char normalise_name_char(
    unsigned char ch )  ///< the character to reduce
{
    if( ch <= ' ' || ch >= 127 ) return '_';
    return static_cast<char>( std::tolower( ch ) );
}

/// Two names match when they are the same length and every pair of characters
/// is the same after reducing it with normalise_name_char.
bool is_name_match( std::string_view string1, std::string_view string2 )
{
    return std::equal( string1.begin(), string1.end(), string2.begin(), string2.end(),
        []( unsigned char a, unsigned char b ) { return normalise_name_char( a ) == normalise_name_char( b ); } );
}

void copy_field( std::string_view field, char *buf, int nbuf )
{
    int length = (int) field.size();
    if( length >= nbuf ) length = nbuf-1;
    memcpy( buf, field.data(), length );
    buf[length] = 0;
}

/// Reads the next field from scanner, treating a quoted value as one field.
/// checkAndRecoverQuotedValue gives nullopt both at the end of the text and
/// for a malformed quote, so a copy of the scanner is used to tell which.
/// The scanner is unchanged if nothing is left to read, and is advanced past
/// a malformed quote.
static FieldResult read_field(
    FieldScanner &scanner,      ///< the scanner to read from
    std::string_view &field )   ///< set to the field when the result is Ok
{
    FieldScanner probe = scanner;
    if( ! probe.next() ) return FieldResult::NoMoreData;
    const std::optional<std::string_view> result = scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    if( ! result ) return FieldResult::MalformedQuote;
    field = *result;
    return FieldResult::Ok;
}

FieldResult read_string_field( FieldScanner &scanner, std::string &value, size_t maxlength )
{
    std::string_view field;
    const FieldResult result = read_field( scanner, field );
    if( result == FieldResult::Ok ) value.assign( field.substr( 0, maxlength ) );
    return result;
}

FieldResult read_double_field( FieldScanner &scanner, double &value )
{
    std::string_view field;
    const FieldResult result = read_field( scanner, field );
    if( result != FieldResult::Ok ) return result;
    const std::optional<double> parsed = parse_double( field );
    if( ! parsed ) return FieldResult::InvalidValue;
    value = *parsed;
    return FieldResult::Ok;
}

FieldResult read_remaining_text( FieldScanner &scanner, std::string &value, size_t maxlength )
{
    if( scanner.atEnd() ) return FieldResult::NoMoreData;
    std::string_view text = scanner.remainder();
    while( scanner.next() ) {}
    text.remove_prefix( std::find_if( text.begin(), text.end(), []( char c ) { return ! ISSPACE(c); } ) - text.begin() );
    value.clear();
    for( const char c : text )
    {
        if( value.size() >= maxlength ) break;
        if( c != '\r' && c != '\x1a' ) value += c;
    }
    return FieldResult::Ok;
}

/// Reads the next field from scanner as a whole number of type T.
template <typename T>
static FieldResult read_whole_number_field(
    FieldScanner &scanner,  ///< the scanner to read from
    T &value )              ///< set to the number when the result is Ok
{
    std::string_view field;
    const FieldResult result = read_field( scanner, field );
    if( result != FieldResult::Ok ) return result;
    const std::optional<ParsedField<T>> parsed = parse_leading_field<T>( field );
    if( ! parsed || parsed->result.ptr != field.data() + field.size() ) return FieldResult::InvalidValue;
    value = parsed->value;
    return FieldResult::Ok;
}

FieldResult read_int_field( FieldScanner &scanner, int &value )
{
    return read_whole_number_field( scanner, value );
}

FieldResult read_long_field( FieldScanner &scanner, long &value )
{
    return read_whole_number_field( scanner, value );
}

FieldResult read_degree_angle_field( FieldScanner &scanner, double &radians )
{
    double degrees = 0.0;
    const FieldResult result = read_double_field( scanner, degrees );
    if( result == FieldResult::Ok ) radians = degrees * (PI/180.0);
    return result;
}

FieldResult read_dms_angle_field( FieldScanner &scanner, double &radians )
{
    int degrees = 0;
    int minutes = 0;
    double seconds = 0.0;
    FieldResult result = read_int_field( scanner, degrees );
    if( result == FieldResult::Ok ) result = read_int_field( scanner, minutes );
    if( result == FieldResult::Ok ) result = read_double_field( scanner, seconds );
    if( result == FieldResult::Ok ) radians = (degrees + minutes/60.0 + seconds/3600.0) * PI/180.0;
    return result;
}

FieldResult read_hp_angle_field( FieldScanner &scanner, double &radians )
{
    std::string_view field;
    const FieldResult result = read_field( scanner, field );
    if( result != FieldResult::Ok ) return result;

    const auto isDigit = []( char c ) { return std::isdigit( static_cast<unsigned char>( c ) ) != 0; };
    const size_t point = field.find( '.' );
    if( point == std::string_view::npos ) return FieldResult::InvalidValue;
    const std::string_view whole = field.substr( 0, point );
    const std::string_view fraction = field.substr( point + 1 );
    if( ! std::all_of( whole.begin(), whole.end(), isDigit ) ) return FieldResult::InvalidValue;
    if( fraction.size() < 4 || ! std::all_of( fraction.begin(), fraction.end(), isDigit ) ) return FieldResult::InvalidValue;

    int degrees = 0;
    for( const char c : whole ) degrees = degrees*10 + (c - '0');
    const int minutes = (fraction[0] - '0')*10 + (fraction[1] - '0');
    double seconds = (fraction[2] - '0')*10 + (fraction[3] - '0');
    double place = 0.1;
    for( const char c : fraction.substr( 4 ) )
    {
        seconds += (c - '0')*place;
        place *= 0.1;
    }
    radians = (degrees + minutes/60.0 + seconds/3600.0) * PI/180.0;
    return FieldResult::Ok;
}
