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
#include <ctype.h>

#include "util/pi.h"
#include "util/errdef.h"
#include "util/iostring.h"

static char rcsid[]="$Id: iostring.c,v 1.4 2004/04/22 02:35:25 ccrook Exp $";

void set_input_string_def( input_string_def *is, char *string )
{
    is->ptr = string;
    is->buffer = string;
    is->sourcename = 0;
    is->source = NULL;
    is->report_error = (int (*)(void *,int,char*)) 0;
}

static int find_next_field( input_string_def *is )
{
    if( is->ptr ) while( isspace(*is->ptr)) is->ptr++;
    return is->ptr && *is->ptr ? OK : NO_MORE_DATA;
}

static int read_next_field( input_string_def *is, char **start, int *length )
{
    char *s;
    int nxt;
    int sts;

    /* Skip over white space */

    sts = find_next_field(is);
    if( sts != OK ) return sts;

    /* Is it a quoted string */

    *length = 0;

    s = is->ptr;
    nxt = 0;

    if( s[nxt] == '\"' )
    {
        s++;
        *start = s;
        while( s[nxt] != '\"' && s[nxt] ) nxt++;
        if( s[nxt] == '\"' )
        {
            *length = nxt;
            is->ptr = s+nxt+1;
            sts = OK;
        }
        else
        {
            *length = nxt-1;
            is->ptr = s+nxt;
            sts = MISSING_DATA;
        }
    }
    else
    {
        *start = s;
        while( s[nxt] && !isspace(s[nxt]) ) nxt++;
        *length = nxt;
        is->ptr = s+nxt;
        sts = OK;
    }

    return sts;
}


int next_string_field( input_string_def *is, char *buf, int nbuf )
{
    int length;
    char *start;
    int sts;

    sts = read_next_field( is, &start, &length );
    if( sts != OK ) return sts;
    if( length >= nbuf ) length = nbuf-1;
    memcpy( buf, start, length );

    buf[length] = 0;
    return OK;
}

int test_next_string_field( input_string_def *is, const char *test )
{
    int length;
    char *start;
    double loc;
    int sts;

    loc = get_string_loc(is);
    sts = read_next_field( is, &start, &length );
    if( sts != OK ) return 0;
    if( strlen(test) == length && _strnicmp(test,start,length)==0 ) return 1;
    set_string_loc(is,loc);
    return 0;
}

int skip_string_field( input_string_def *is )
{
    char *start;
    int length;
    return read_next_field( is, &start, &length );
}


static int parse_number( input_string_def *is, char *fmt, void *value )
{
    int length, nfld, sts;
    char *fld, save, garbage;
    sts = read_next_field( is, &fld, &length );
    if( sts != OK )  return sts;

    /* Put a NULL terminator at the end of the string */

    save = fld[length];
    fld[length] = 0;

    nfld = sscanf( fld, fmt, value, &garbage );

    fld[length] = save;

    /* Determine the return status */

    return nfld == 1 ? OK : INVALID_DATA;
}


int double_from_string( input_string_def *is, void *value )
{
    return parse_number( is, "%lf%1s", value );
}

int float_string( input_string_def *is, void *value )
{
    return parse_number( is, "%f%1s", value );
}

int long_from_string( input_string_def *is, void *value )
{
    return parse_number( is, "%ld%1s", value );
}

int int_from_string( input_string_def *is, void *value )
{
    int ival;
    int sts;
    sts = parse_number( is, "%d%1s", &ival );
    (*(int *)value) = ival;
    return sts;
}

int short_from_string( input_string_def *is, void *value )
{
    int ival;
    int sts;
    sts = parse_number( is, "%hd%1s", &ival );
    (*(short *)value) = ival;
    return sts;
}

int character_from_string( input_string_def *is, void *cp )
{
    char *c = (char *) cp;
    *c = is->ptr ? *(is->ptr) : 0;
    if(*c) is->ptr++;
    return *c ? 1 : 0;
}

long get_string_loc( input_string_def *is )
{
    return is->ptr - is->buffer;
}

void set_string_loc( input_string_def *is, long loc )
{
    if( loc >= 0 && loc <= (long) strlen(is->buffer) )
    {
        is->ptr = is->buffer + loc;
    }
}

int end_of_string( input_string_def *is )
{
    return find_next_field( is ) ?  0 : 1;
}

char *unread_string( input_string_def *is )
{
    return is->ptr;
}

void report_string_error( input_string_def *is, int status, char *message )
{
    if( is->report_error )
    {
        (*is->report_error)( is->source, status, message );
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
        if( triml ) while( *start && *start != '\n' && isspace(*start)) start++;
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
                if( ! isspace(*ptre) ) nch=ptre-start+1;
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

