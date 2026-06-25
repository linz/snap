#include "snapconfig.h"
#include <stdio.h>
#include <string.h>
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/snapctype.h"
#include "util/writecsv.h"


output_csv *open_output_csv(const char *filename, int tab_delimited )
{
    output_csv *csv;
    FILE *f = fopen(filename,"w");
    if( !f ) return 0;
    csv = (output_csv *) check_malloc( sizeof(output_csv));
    csv->filename = copy_string(filename);
    csv->f = f;
    csv->delim = tab_delimited ? '\t' : ',';
    csv->delimrep = strcpy( csv->charbuf, tab_delimited ? " " : "," );
    csv->tab = tab_delimited;
    csv->quote = tab_delimited ? 0 : '"';
    csv->quoterep = strcpy(csv->charbuf+3,tab_delimited ? "\"" : "\"\"");
    csv->newlinerep = strcpy(csv->charbuf+6, tab_delimited ? " " : "\n");
    csv->first = 1;
    return csv;
}

void close_output_csv( output_csv *csv )
{
    if( ! csv ) return;
    fclose( csv->f );
    check_free( csv->filename );
    check_free( csv );
}

void end_output_csv_record( output_csv *csv )
{
    fputs("\n",csv->f);
    csv->first = 1;
}

static void start_field( output_csv *csv )
{
    if( csv->first )
    {
        csv->first = 0;
    }
    else
    {
        fputc(csv->delim,csv->f);
    }
}

void write_csv_header( output_csv *csv, const char *fieldname )
{
    char header[33];
    char *c;

    for( c=header; *fieldname; fieldname++ )
    {
        char ch = *fieldname;
        if( ! ISALNUM(ch) ) ch = '_';
        *c++ = ch;
        if( c - header >= 32 ) break;
    }
    *c = 0;
    write_csv_string( csv, header );
}

void write_csv_string( output_csv *csv, const char *value )
{
    const char *c;
    start_field( csv );
    if( ! value ) return;
    if( csv->quote ) { fputc(csv->quote,csv->f); }
    for( c = value; *c; c++ )
    {
        if( *c == csv->quote ) { fputs( csv->quoterep, csv->f ); }
        else if( *c == csv->delim ) { fputs( csv->delimrep, csv->f ); }
        else if( *c == '\n' ) { fputs( csv->newlinerep, csv->f ); }
        else fputc( (int) *c, csv->f );
    }
    if( csv->quote ) { fputc(csv->quote,csv->f); }
}

void write_csv_int( output_csv *csv, long value )
{
    start_field( csv );
    fprintf( csv->f, "%ld", value );
}

void write_csv_double( output_csv *csv, double value, int ndp )
{
    start_field( csv );
    if( ndp  >= 0 )
    {
        fprintf( csv->f, "%.*lf", ndp,value );
    }
    else
    {
        fprintf( csv->f, "%lf", value );
    }
}

void write_csv_null_field( output_csv *csv )
{
    start_field( csv );
}

void write_csv_date( output_csv *csv, double date )
{
    if( date == UNDEFINED_DATE ) { write_csv_null_field( csv ); return; }
    write_csv_string( csv, date_as_string(date,0,0) );
}

