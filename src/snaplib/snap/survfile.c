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
#include "snapdata/obsmod.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/dateutil.h"

static survey_data_file **sdindx = NULL;
static int nsdindx = 0;
static int maxsdindx = 0;

#define SDINDX_INC 10


static int add_data_file_nocopy( char *name, int format, char *subtype, char *recode, file_context *context )
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
    sd->recodefile=recode;
    sd->context=context;
    sd->mindate=UNDEFINED_DATE;
    sd->maxdate=UNDEFINED_DATE;
    sd->nnodate=0;
    for( i=0; i < NOBSTYPE; i++ ) sd->obscount[i]=0;
    sd->usage = 0;
    sd->recode=0;
    return nsdindx-1;

}

int add_data_file( char *name, int format, char *subtype, char *recode, file_context *context )
{
    char *buffer=0;

    /* If context is not null */

    if( context )
    {
        const char *refpath = context->dir;
        int nch=strlen(name)+strlen(refpath)+2;
        char *filename;
        buffer= (char *) check_malloc( nch );
        filename=build_filespec(buffer,nch,refpath,name,NULL);
        if( file_exists(filename) ) name=filename;
    }

    name = copy_string( name );
    subtype = copy_string( subtype );
    recode = copy_string( recode );
    if( buffer ) check_free( buffer );
    return add_data_file_nocopy( name, format, subtype, recode, context );
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

int survey_data_file_id( char *name, file_context *context )
{
    int i;
    int matchid=-1;
    int matchlen;
    char *buffer=0;

    /* If context is not null then try looking for a matching file */

    if( context )
    {
        const char *refpath = context->dir;
        int nch=strlen(name)+strlen(refpath)+2;
        char *filename;
        buffer=(char *) check_malloc( nch );
        filename=build_filespec(buffer,nch,refpath,name,NULL);
        if( file_exists(filename) ) name=filename;
    }

    /* Case sensitive match - not checking for ambiguity */
    for( i = 0; i < nsdindx; i++ )
    {
        if( strcmp( name, sdindx[i]->name ) == 0 ) { matchid=i; break; }
    }

    /* Case insensitive match - not checking for ambiguity */
    if( matchid < 0 )
    {
        for( i = 0; i < nsdindx; i++ )
        {
            if( _stricmp( name, sdindx[i]->name ) == 0 ) { matchid=i; break; }
        }
    }

    /* Path insensitive match (but path delimiter character sensitive) */
    if( matchid < 0 )
    {
        matchlen=strlen(name);
        for( i=0; i < nsdindx; i++ )
        {
            char *dfname=sdindx[i]->name;
            int offset=strlen(dfname)-matchlen;
            if( offset > 0 )
            {
                if( _stricmp( dfname+offset, name ) == 0 &&
                    (dfname[offset-1]=='/' || dfname[offset-1]=='\\'))
                {
                    if( matchid < 0 ) matchid=i;
                    else
                    {
                        /* Ambiguous filename match */
                        matchid=-1;
                        break;
                    }
                }
            }

        }
    }

    if( buffer ) check_free( buffer );

    return matchid;
}

int survey_data_file_count( void )
{
    return nsdindx;
}

void survey_data_file_dates( double *mindate, double *maxdate, int *nnodate )
{
    double mindat=UNDEFINED_DATE;
    double maxdat=UNDEFINED_DATE;
    int nnd=0;
    int i;
    for( i=0; i<nsdindx; i++ )
    {
        survey_data_file *sd=sdindx[i];
        if( sd->mindate != UNDEFINED_DATE )
        {
            if( sd->mindate < mindat || mindat == UNDEFINED_DATE ) mindat=sd->mindate;
            if( sd->maxdate > maxdat || maxdat == UNDEFINED_DATE ) maxdat=sd->maxdate;
            nnd += sd->nnodate;
        }
    }
    if( mindate ) *mindate=mindat;
    if( maxdate ) *maxdate=maxdat;
    if( nnodate ) *nnodate=nnd;
}

void dump_filenames( BINARY_FILE *b )
{
    int i;
    create_section( b, "DATA_FILES" );
    fwrite( &nsdindx, sizeof(nsdindx), 1, b->f );
    for( i=0; i<nsdindx; i++ )
    {
        const char *context_def=context_definition(sdindx[i]->context);
        fwrite( &sdindx[i]->format, sizeof(sdindx[i]->format), 1, b->f );
        dump_string( sdindx[i]->name, b->f );
        dump_string( sdindx[i]->subtype, b->f );
        dump_string( sdindx[i]->recodefile, b->f );
        dump_string( context_def, b->f );
        check_free(context_def);
    }
    end_section( b );
}


int reload_filenames( BINARY_FILE *b )
{
    int i, fmt;
    char *name;
    char *subtype;
    char *recodefile;
    char *context_def;

    if( find_section(b,"DATA_FILES") != OK ) return MISSING_DATA;
    fread( &i, sizeof(i), 1, b->f );
    while( i-- > 0 )
    {
        fread( &fmt, sizeof(fmt), 1, b->f );
        name = reload_string( b->f );
        subtype = reload_string( b->f );
        recodefile = reload_string( b->f );
        // Note: flawed implementation of restoring context.  
        // 
        context_def = reload_string( b->f );
        file_context *context=recreate_context(context_def);
        check_free(context_def);
        add_data_file_nocopy( name, fmt, subtype, recodefile, context );
    }
    return check_end_section( b );
}
