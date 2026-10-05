#include "snapconfig.h"
/* Routines used with input_string_def objects */

/* iostring.c:  Code for parsing character strings to extract quoted strings,
   and numeric fields.  */

/*
   $Log: iostring.c,v $
   Revision 1.4  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:50:26  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <boost/algorithm/string/predicate.hpp>
#include "util/snapctype.h"

#include "util/pi.h"
#include "util/errdef.h"
#include "util/iostring.h"

/// Reads the next field (quote-transparent) from scanner into field.
/// \return OK with field set, NO_MORE_DATA if nothing non-whitespace is
///         left (checked via a throwaway copy, since the real read must go
///         through checkAndRecoverQuotedValue() for quote-transparency, not
///         a plain next()), or MISSING_DATA on a malformed quote.
static int read_next_field_status( FieldScanner &scanner, std::string_view &field )
{
    {
        FieldScanner probe = scanner;
        if( ! probe.next() ) return NO_MORE_DATA;
    }
    auto f = scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    if( ! f ) return MISSING_DATA;
    field = *f;
    return OK;
}

int next_string_field( FieldScanner &scanner, std::string &field, const size_t maxlength )
{
    std::string_view text;
    const int sts = read_next_field_status( scanner, text );
    if( sts == OK ) field.assign( text.substr( 0, maxlength ) );
    return sts;
}

int test_next_string_field( FieldScanner &scanner, std::string_view test )
{
    auto saved = scanner.remainder();
    std::string_view field;
    if( read_next_field_status( scanner, field ) != OK ) return 0;   // read failed - position already correctly left advanced (or unchanged), don't restore
    if( boost::algorithm::iequals(field,test) ) return 1;
    scanner = FieldScanner(saved);   // read fine but didn't match - restore
    return 0;
}

int double_from_string( FieldScanner &scanner, void *value )
{
    std::string_view field;
    int sts = read_next_field_status( scanner, field );
    if( sts != OK ) return sts;
    auto parsed = parse_double( field );
    if( ! parsed ) return INVALID_DATA;
    *(double *)value = *parsed;
    return OK;
}

std::string_view unread_string( input_string_def &def )
{
    return def.scanner.remainder();
}

void report_string_error( input_string_def &def, int status, std::string_view message )
{
    if( def.report_error )
    {
        (*def.report_error)( def.source, status, message );
    }
}

/*================================================================*/

int write_output_string( output_string_def *os, std::string_view s )
{
    if( os->write ) return (*os->write)( s, os->sink );
    return FILE_WRITE_ERROR;
}

int write_output_string2( output_string_def *os, std::string_view s, int options, std::string_view prefix )
{
    if( ! os->write ) return FILE_WRITE_ERROR;
    const bool triml = options & OSW_TRIML;
    const bool trimr = options & OSW_TRIMR;
    const bool skipblank = options & OSW_SKIPBLANK;
    size_t pos = 0;
    while( pos < s.size() )
    {
        size_t start = pos;
        size_t nch = 0;
        if( triml ) while( start < s.size() && s[start] != '\n' && ISSPACE(s[start]) ) start++;
        if( start >= s.size() ) break;
        if( s[start] == '\n' )
        {
            pos = start;
        }
        else
        {
            size_t end = start;
            while( end < s.size() && s[end] != '\n' )
            {
                if( ! ISSPACE(s[end]) ) nch = end - start + 1;
                end++;
            }
            if( ! trimr ) nch = end - start;
            pos = end;
        }
        if( nch > 0 || ! skipblank )
        {
            if( nch > 0 )
            {
                write_output_string( os, prefix );
                write_output_string( os, s.substr( start, nch ) );
            }
            write_output_string( os, "\n" );
        }
        if( pos < s.size() ) pos++;
    }
    return 0;
}

static int sfputs( std::string_view s, void *f )
{
    return fwrite( s.data(), 1, s.size(), static_cast<FILE *>( f ) ) == s.size() ? 0 : FILE_WRITE_ERROR;
}

void output_string_to_file( output_string_def *os, FILE *f )
{
    os->sink = f;
    os->write = sfputs;
}

