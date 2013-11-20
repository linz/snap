#include "snapconfig.h"
/* Routines for reading a data file.  These routines provide the following
   facilities:

   1) Counting of line numbers
   2) Skipping blank lines
   3) Skipping comments - anything after ! on a line
   4) Reporting errors with a file name and line number
   5) Clearing out control characters which differ between operating systems.
*/


/*
   $Log: datafile.c,v $
   Revision 1.4  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1996/10/25 22:54:34  CHRIS
   Enlarged size of field used to read numbers from 20 characters to 30 chars.

   Revision 1.1  1995/12/22 18:55:56  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#include "util/chkalloc.h"
#include "util/datafile.h"
#include "util/pi.h"
#include "util/errdef.h"

#define CONTINUATION_CHR '&'
#define COMMENT_CHR      '!'
#define QUOTE_CHR        '\"'

static char rcsid[]="$Id: datafile.c,v 1.4 2004/04/22 02:35:24 ccrook Exp $";

static int default_reclen = 256;

#define ISSPACE(x) ((x)==' ' || (x)=='\r' || (x)=='\n' || (x)=='\t' || (x)=='\x1A')

static char *utf8_bom = "\xEF\xBB\xBF";
static char *utf16_bom = "\xFF\xFE\x46";

int df_data_file_default_reclen( int newlen )
{
    int oldlen = default_reclen;
    if( newlen > 80 ) default_reclen = newlen;
    return oldlen;
}

DATAFILE *df_open_data_file( const char *fname, const char *description )
{
    FILE *f;
    DATAFILE *d;
    char msg[80];
    int nch;
    char unicode;
    char binary;
    int i;

    f = fopen( fname, "rb" );
    if( f == NULL )
    {
        if( description )
        {
            sprintf(msg,"Unable to open %.60s",description);
            handle_error(FILE_OPEN_ERROR,msg,fname);
        }
        return NULL;
    }

    nch = fread(msg,1,80,f);
    unicode = 0;
    binary = 0;
    if( nch >= strlen(utf8_bom) &&memcmp(msg,utf8_bom,strlen(utf8_bom))==0) unicode = 1;
    if( nch >= strlen(utf16_bom) &&memcmp(msg,utf16_bom,strlen(utf16_bom))==0) unicode = 2;
    if( ! unicode )
    {
        for( i = 0; i < nch; i++ )
        {
            if( msg[i] == 0 || (msg[i] & '\x80')) { binary=1; break; }
        }
    }
    if( unicode )
    {
        fclose(f);
        sprintf(msg,"Cannot use unicode file - convert to ASCII");
        handle_error(FILE_OPEN_ERROR,msg,fname);
        return NULL;
    }
    if( binary )
    {
        fclose(f);
        sprintf(msg,"File appears to contain binary data");
        handle_error(FILE_OPEN_ERROR,msg,fname);
        return NULL;
    }
    fseek(f,0L,SEEK_SET);


    d = (DATAFILE *) check_malloc( sizeof(DATAFILE) +  strlen(fname) + 1 );
    d->inrec = (char *) check_malloc( default_reclen );

    d->maxreclen = default_reclen;
    d->fname = (char *)d + sizeof(DATAFILE);
    strcpy( d->fname, fname );
    d->f = f;
    d->lineno = d->startlineno = 0;
    d->startloc = 0;
    d->reclineno = 0;
    d->inrec[0] = 0;
    d->inrecptr = d->inrec;
    d->lastrecptr = d->inrec;
    d->unicode = 0;
    d->errcount = 0;
    d->comment_char = COMMENT_CHR;
    d->continuation_char = CONTINUATION_CHR;
    d->quote_char = QUOTE_CHR;
    return d;
}

char *df_file_name( DATAFILE *d )
{
    return d->fname;
}

void  df_set_data_file_comment( DATAFILE *d, char comment )
{
    d->comment_char = comment;
}

void  df_set_data_file_quote( DATAFILE *d, char quote )
{
    d->quote_char = quote;
}

void  df_set_data_file_continuation( DATAFILE *d, char continuation )
{
    d->continuation_char = continuation;
}


void df_close_data_file( DATAFILE *d )
{
    if( d )
    {
        if( d->f ) fclose(d->f);
        check_free(d->inrec);
        check_free(d);
    }
}


int df_skip_to_blank_line( DATAFILE *d )
{
    char blank;
    int c;
    blank = 1;

    while( EOF != (c = fgetc( d->f )) )
    {
        if( c == '\n' )
        {
            d->lineno++;
            if( blank ) return OK;
            blank = 1;
        }
        else if( ! ISSPACE(c) )
        {
            blank = 0;
        }
    }
    return NO_MORE_DATA;
}

static void df_expand_buffer( DATAFILE *d )
{
    d->maxreclen *= 2;
    d->inrec = (char *) check_realloc( d->inrec, d->maxreclen );
}

int df_read_data_file( DATAFILE *d )
{
    int eof = 0;

    d->startlineno = d->lineno;
    d->startloc = ftell( d->f );
    d->inrec[0] = 0;
    d->reclineno = 0;

    // Until we get to a non-blank line...
    while( ! d->inrec[0] && ! eof)
    {
        int offset = 0;
        int continued = 1;
        char *line;
        d->reclineno = 0;

        while ( continued && ! eof )
        {
            int lineoffset = offset;
            while( 1 )
            {
                char *start = d->inrec+offset;
                if( ! fgets( start, d->maxreclen-offset, d->f )) { eof = 1; break; }
                int len = strlen(start);
                if( ! len ) break;
                if( start[len-1] == '\n' ) break;
                df_expand_buffer(d);
                offset += len;
            }
            d->lineno++;
            if( ! d->reclineno ) d->reclineno = d->lineno;
            // Remove everything after a comment character
            line = d->inrec+lineoffset;
            if( d->comment_char )
            {
                char *end = strchr(line,d->comment_char);
                if( end ) *end = 0;
            }
            // Trim whitespace
            int nch = strlen(line);
            while( nch-- && isspace(line[nch])) { line[nch] = 0; }
            offset = lineoffset + nch;
            // Check for line continuation
            if( nch >= 1 && line[nch] == d->continuation_char && isspace(line[nch-1]))
            {
                line[offset] = 0;
            }
            else
            {
                continued = 0;
            }
        }
        // Retrim in case continuation only adds blanks
        line = d->inrec;
        while( offset > 0 && isspace(line[offset])) { line[offset--] = 0; }
    }

    d->inrecptr = d->lastrecptr = d->inrec;
    return d->inrec[0] ? OK : NO_MORE_DATA;
}

char *df_rest_of_line( DATAFILE *d )
{
    return d->inrecptr;
}

#if 0
static int send_datafile_error( void *src, int status, const char *message )
{
    DATAFILE *d = (DATAFILE *) src;
    return df_data_file_error( d, status, message );
}
#endif

input_string_def *df_input_string( DATAFILE *d )
{
    d->instr.buffer = d->inrec;
    d->instr.ptr = d->inrecptr;
    d->instr.sourcename = d->fname;
    d->instr.source = (void *) d;
    d->instr.report_error = (input_string_errfunc) df_data_file_error;
    return &d->instr;
}


/* Routines to extract data from the data file record  */
/* Assumes data is all space delimited                 */
/* All return 1 if found and OK, 0 otherwise, for easy */
/* combination in logical expression                   */

int df_read_field( DATAFILE *d, char *field, int nfld )
{
    char *s, *e;

    s = d->inrecptr;
    while( ISSPACE(*s) ) s++;

    d->lastrecptr = s;

    if ( !*s ) { *field = 0; d->inrecptr = s; return 0; }

    if( *s == d->quote_char )
    {
        e = ++s;
        while( *e && *e != d->quote_char ) e++;
        if( *e )
        {
            d->inrecptr = e+1;
        }
        else
        {
            d->inrecptr = e;
        }
    }

    else
    {
        e = s;
        while( *e && ! ISSPACE(*e) ) e++;
        d->inrecptr = e;
    }


    while( --nfld > 0 && s < e ) *field++ = *s++;
    *field = 0;

    return 1;
}


int df_skip_character( DATAFILE *d )
{
    if ( *d->inrecptr )
    {
        d->inrecptr++;
        return 1;
    }
    else
    {
        return 0;
    }
}


int df_read_code( DATAFILE *d, char *field, int nfld )
{
    int sts;
    sts = df_read_field(d,field,nfld);
    if(sts) _strupr(field);
    return sts;
}



int df_read_int( DATAFILE *d, int *v )
{
    char field[30], chk[2];
    chk[0]=0;
    if( !df_read_field( d, field, 30 ) ) return 0;
    if( sscanf( field, "%d%1s", v, chk ) < 1 || chk[0] ) return 0;
    return 1;
}


int df_read_short( DATAFILE *d, short *v )
{
    char field[30], chk[2];
    short i;
    chk[0]=0;
    if( !df_read_field( d, field, 30 ) ) return 0;
    if( sscanf( field, "%hd%1s", &i, chk ) < 1 || chk[0] ) return 0;
    (*v) = i;
    return 1;
}


int df_read_long( DATAFILE *d, long *v )
{
    char field[30], chk[2];
    chk[0]=0;
    if( !df_read_field( d, field, 30 ) ) return 0;
    if( sscanf( field, "%ld%1s", v, chk ) < 1 || chk[0] ) return 0;
    return 1;
}

int df_read_double( DATAFILE *d, double *v )
{
    char field[30], chk[2];
    chk[0]=0;
    if( !df_read_field( d, field, 30 ) ) return 0;
    if( sscanf( field, "%lf%1s", v, chk ) < 1 || chk[0] ) return 0;
    return 1;
}

int df_read_degangle( DATAFILE *d, double *v )
{
    int ok;
    ok = df_read_double(d, v);
    (*v) *= (PI/180.0);
    return ok;
}

int df_read_dmsangle( DATAFILE *d, double *v )
{
    int deg, min;
    double sec;
    if( df_read_int( d, &deg) && df_read_int( d, &min ) && df_read_double( d, &sec ) )
    {
        *v = (deg + min/60.0 + sec/3600.0 )*PI/180.0;
        return 1;
    }
    else
    {
        return 0;
    }
}

int df_read_hpangle( DATAFILE *d, double *v )
{
    char field[30], *f;
    int deg, min;
    double sec, den;

    if( !df_read_field( d, field, 30 ) ) return 0;

    deg = min = 0; sec = 0.0;

    for( f = field; *f != '.'; f++ )
    {
        if( !isdigit(*f) ) return 0;
        deg = deg*10 + (*f - '0');
    }
    f++;
    if( !isdigit( *f )) return 0;
    min = (*f - '0')*10;
    f++;
    if( !isdigit( *f )) return 0;
    min += (*f - '0' );
    f++;
    if( !isdigit( *f )) return 0;
    sec = (*f - '0')*10;
    f++;
    if( !isdigit( *f )) return 0;
    sec += (*f - '0' );
    f++;
    den = 0.1;
    while( isdigit(*f) )
    {
        sec += (*f - '0')*den;
        den *= 0.1;
        f++;
    }
    if( *f ) return 0;
    *v = (deg + min/60.0 + sec/3600.0 ) * PI/180.0;
    return 1;
}


int df_read_rest( DATAFILE *d, char *line, int nlin )
{
    char *s;
    d->lastrecptr = d->inrecptr;
    s = d->inrecptr;
    while ( ISSPACE(*s) ) s++;
    if( !*s ) { *line = 0; d->inrecptr = s; return 0; }
    nlin--;
    for( ; nlin > 0 && *s ; s++ )
    {
        if( *s != '\r' && *s != '\x1a' )
        {
            *line++ = *s;
            nlin--;
        }
    }
    *line = 0;
    while( *s ) s++;
    d->inrecptr = s;
    return 1;
}


void df_reread_field( DATAFILE *d )
{
    d->inrecptr = d->lastrecptr;
}


long df_line_number( DATAFILE *d )
{
    return d->reclineno;
}

int df_end_of_line( DATAFILE *d )
{
    char *s;
    s = d->inrecptr;
    while( ISSPACE(*s) ) s++;
    d->inrecptr = s;
    return *s ? 0 : 1;
}


int df_data_file_error( DATAFILE *d, int sts, const char *errmsg )
{
    char fmsg[100];
    sprintf(fmsg,"Line: %ld  File: %.60s", d->reclineno, d->fname );
    handle_error(sts,errmsg,fmsg );
    if( sts > WARNING_ERROR ) d->errcount++;
    return sts;
}

int df_data_file_errcount( DATAFILE *d )
{
    return d->errcount;
}

void df_save_data_file_loc( DATAFILE *d, datafile_loc *dl )
{
    dl->line = d->startlineno;
    dl->loc = d->startloc;
}

void df_reset_data_file_loc( DATAFILE *d, datafile_loc *dl )
{
    d->lineno = dl->line;
    fseek( d->f, dl->loc, SEEK_SET );
    df_read_data_file( d );
}


#ifdef TESTDF

int main( int argc, char *argv[] )
{
    DATAFILE *in;
    char field[20], *f, data[20], rest[80];
    int sts,;
    int ival;
    long lval;
    double dval;

    if( argc < 2 || (in = open_data_file( argv[1],"test file")) == NULL )
    {
        printf("Invalid or missing file name parameter\n");
        return 0;
    }

    while( read_data_file(in) == OK )
    {
        df_read_field(in,field,20);
        printf("\n\nLine %ld: fields %s\n",line_number(in),field);
        _strupr( field );
        for( f=field; *f; f++ )
        {
            sts = 1;
            switch(*f)
            {
            case 'F': sts = df_read_field( in, data, 20 );
                printf("Status %d: Field %s\n",(int)sts,data);
                break;

            case 'I': sts = read_int( in, &ival );
                printf("Status %d: Integer %d\n",(int) sts, ival);
                break;

            case 'L': sts = read_long( in, &lval );
                printf("Status %d: Long %ld\n",(int) sts, lval);
                break;

            case 'D': sts = read_double( in, &dval );
                printf("Status %d: Double %lf\n",(int) sts,dval);
                break;

            case 'A': sts = read_dmsangle( in, &dval );
                printf("Status %d: DMS %lf\n",(int) sts,dval);
                break;

            case 'H': sts = read_hpangle( in, &dval );
                printf("Status %d: HP %lf\n",(int) sts,dval);
                break;

            case 'R': sts = read_rest( in, rest, 80 );
                printf("Status %d: Rest %s\n",(int) sts,rest);
                break;

            case 'B': reread_field( in );
                break;

            default:
                data_file_error( in, INVALID_DATA,
                                 "Invalid data in field specifier");
                break;

            }
            if( !sts )
            {
                data_file_error( in, INVALID_DATA, "Invalid data read from field");
            }
        }

    }
    close_data_file( in );
    return 0;
}



#endif
