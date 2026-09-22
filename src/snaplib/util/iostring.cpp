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

int next_string_field( FieldScanner &scanner, char *buf, int nbuf )
{
    std::string_view field;
    int sts = read_next_field_status( scanner, field );
    if( sts == OK ) copy_field( field, buf, nbuf );
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

char *unread_string( input_string_def &def )
{
    return const_cast<char *>( def.scanner.remainder().data() );
}

void report_string_error( input_string_def &def, int status, const char *message )
{
    if( def.report_error )
    {
        (*def.report_error)( def.source, status, message );
    }
}

/*================================================================*/

int write_output_string( output_string_def *os, const char *s )
{
    if( os->write ) return (*os->write)( s, os->sink );
    return FILE_WRITE_ERROR;
}

int write_output_string2( output_string_def *os, const char *s, int options, const char *prefix )
{
    if( ! os->write ) return FILE_WRITE_ERROR;
    const char *ptrs;
    const char *ptre;
    int triml = options & OSW_TRIML;
    int trimr = options & OSW_TRIMR;
    int skipblank = options & OSW_SKIPBLANK;
    ptrs = s;
    while( *ptrs )
    {
        const char *start;
        int nch;
        ptre=ptrs;
        start=ptrs;
        if( triml ) while( *start && *start != '\n' && ISSPACE(*start)) start++;
        if( ! *start ) break;
        if( *start == '\n' )
        {
            nch=0;
            ptrs=start;
        }
        else
        {
            nch=0;
            ptre=start;
            while( *ptre && *ptre != '\n' )
            {
                if( ! ISSPACE(*ptre) ) nch=ptre-start+1;
                ptre++;
            }
            if( ! trimr ) nch=ptre-start;
            ptrs = ptre;
        }
        if( nch > 0 || ! skipblank )
        {
            if( nch && prefix ) write_output_string(os,prefix);
            while( nch > 0 )
            {
                char buffer[33];
                int ncopy = nch > 32 ? 32 : nch;
                strncpy( buffer,start,ncopy );
                buffer[ncopy]=0;
                write_output_string(os,buffer);
                start += ncopy;
                nch -= ncopy;
            }
            write_output_string(os,"\n");
        }
        if( *ptrs ) ptrs++;
    }
    return 0;
}

static int sfputs( const char *s, void *f )
{
    return (int) fputs( s, (FILE *) f );
}

void output_string_to_file( output_string_def *os, FILE *f )
{
    os->sink = f;
    os->write = sfputs;
}

