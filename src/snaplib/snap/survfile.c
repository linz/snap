#include "snapconfig.h"
/* Code to manage the list of survey data files */

/*
   $Log: survfile.c,v $
   Revision 1.1  1995/12/22 18:36:41  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/binfile.h"
#include "snap/survfile.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/dateutil.h"

static survey_data_file **sdindx = NULL;
static int nsdindx = 0;
static int maxsdindx = 0;

#define SDINDX_INC 10


static int add_data_file_nocopy( char *name, int format, char *subtype, double errfct, char *recode )
{
    survey_data_file *sd;
    int i;

    sd = (survey_data_file *) check_malloc( sizeof( survey_data_file ) );
    if( nsdindx >= maxsdindx )
    {
        maxsdindx = nsdindx + SDINDX_INC;
        sdindx = (survey_data_file **) check_realloc(
                     sdindx, maxsdindx * sizeof(survey_data_file *));
    }
    sdindx[nsdindx] = sd;
    nsdindx++;

    sd->name = name;
    sd->format = format;
    sd->subtype = subtype;
    sd->errfct = errfct;
    sd->recodefile=recode;
    sd->mindate=UNDEFINED_DATE;
    sd->maxdate=UNDEFINED_DATE;
    sd->nnodate=0;
    for( i=0; i < NOBSTYPE; i++ ) sd->obscount[i]=0;
    sd->usage = 0;
    sd->recode=0;
    return OK;
}

int add_data_file( char *name, int format, char *subtype, double errfct, char *recode )
{
    name = copy_string( name );
    subtype = copy_string( subtype );
    recode = copy_string( recode );
    return add_data_file_nocopy( name, format, subtype, errfct, recode );
}

void delete_survey_data_file_recodes()
{
    int i;
    for( i=0; i<nsdindx; i++ )
    {
        survey_data_file *sd=sdindx[i];
        if( sd->recode ) delete_stn_recode_map( sd->recode );
        sd->recode=0;
    }
}

void delete_survey_file_list()
{
    int i;
    for( i=0; i<nsdindx; i++ )
    {
        survey_data_file *sd=sdindx[i];
        if( sd->recode ) delete_stn_recode_map( sd->recode );
        if( sd->name ) check_free( sd->name );
        if( sd->subtype ) check_free( sd->subtype );
        if( sd->recodefile ) check_free( sd->recodefile );
        check_free( sd );
        sdindx[i] = 0;
    }
    check_free( sdindx );
    sdindx=0;
    nsdindx=0;
}

survey_data_file *survey_data_file_ptr( int  ifile )
{
    return sdindx[ifile];
}

char *survey_data_file_name( int ifile )
{
    return sdindx[ifile]->name;
}

int survey_data_file_id( char *name )
{
    int i;
    for( i = 0; i < nsdindx; i++ )
    {
        if( _stricmp( name, sdindx[i]->name ) == 0 ) return i;
    }
    return -1;
}

int survey_data_file_count( void )
{
    return nsdindx;
}

double survey_data_file_errfct( int ifile )
{
    return sdindx[ifile]->errfct;
}


void dump_filenames( BINARY_FILE *b )
{
    int i;
    create_section( b, "DATA_FILES" );
    fwrite( &nsdindx, sizeof(nsdindx), 1, b->f );
    for( i=0; i<nsdindx; i++ )
    {
        fwrite( &sdindx[i]->format, sizeof(sdindx[i]->format), 1, b->f );
        fwrite( &sdindx[i]->errfct, sizeof(sdindx[i]->errfct), 1, b->f );
        dump_string( sdindx[i]->name, b->f );
        dump_string( sdindx[i]->subtype, b->f );
        dump_string( sdindx[i]->recodefile, b->f );
    }
    end_section( b );
}


int reload_filenames( BINARY_FILE *b )
{
    int i, fmt;
    double errfct;
    char *name;
    char *subtype;
    char *recodefile;

    if( find_section(b,"DATA_FILES") != OK ) return MISSING_DATA;
    fread( &i, sizeof(i), 1, b->f );
    while( i-- > 0 )
    {
        fread( &fmt, sizeof(fmt), 1, b->f );
        fread( &errfct, sizeof(errfct), 1, b->f );
        name = reload_string( b->f );
        subtype = reload_string( b->f );
        recodefile = reload_string( b->f );

        add_data_file_nocopy( name, fmt, subtype, errfct, recodefile );
    }
    return check_end_section( b );
}
