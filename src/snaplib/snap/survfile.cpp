#include "snapconfig.h"
/* Code to manage the list of survey data files */

/*
   $Log: survfile.c,v $
   Revision 1.1  1995/12/22 18:36:41  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <string_view>

#include "util/dstring.h"
#include "util/binfile.h"
#include "snap/survfile.h"
#include "snapdata/obsmod.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/dateutil.h"
#include <boost/algorithm/string/predicate.hpp>

static survey_data_file **sdindx = nullptr;
static int nsdindx = 0;
static int maxsdindx = 0;

#define SDINDX_INC 10


static int add_data_file_nocopy( const std::string &name, int format, const std::optional<std::string> &subtype, const std::optional<std::string> &recode, file_context *context )
{
    survey_data_file *sd;
    int i;

    sd = new survey_data_file();
    if( nsdindx >= maxsdindx )
    {
        maxsdindx = nsdindx + SDINDX_INC;
        survey_data_file **newindx = new survey_data_file *[maxsdindx];
        std::copy( sdindx, sdindx + nsdindx, newindx );
        delete [] sdindx;
        sdindx = newindx;
    }
    sdindx[nsdindx] = sd;
    nsdindx++;

    sd->name = name;
    sd->format = format;
    sd->subtype = subtype;
    sd->recodefile = recode;
    sd->context=context;
    sd->mindate=UNDEFINED_DATE;
    sd->maxdate=UNDEFINED_DATE;
    sd->nnodate=0;
    for( i=0; i < NOBSTYPE; i++ ) sd->obscount[i]=0;
    sd->usage = 0;
    sd->recode=0;
    return nsdindx-1;

}

int add_data_file( const std::string &name, int format, const std::optional<std::string> &subtype, const std::optional<std::string> &recode, file_context *context )
{
    std::string resolved_name = name;

    /* If context is not null */

    if( context )
    {
        std::string filename = build_filespec(context->dir,name,"");
        if( path_exists(filename) ) resolved_name = filename;
    }

    return add_data_file_nocopy( resolved_name, format, subtype, recode, context );
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
        delete sd;
        sdindx[i] = 0;
    }
    delete [] sdindx;
    sdindx=nullptr;
    nsdindx=0;
    maxsdindx=0;
}

survey_data_file *survey_data_file_ptr( int  ifile )
{
    return sdindx[ifile];
}

std::string survey_data_file_name( int ifile )
{
    return sdindx[ifile]->name;
}

int survey_data_file_id( std::string_view name, file_context *context )
{
    int i;
    int matchid=-1;
    std::string matchName(name);

    /* If context is not null then try looking for a matching file */

    if( context )
    {
        std::string filename = build_filespec(context->dir,std::string(name),"");
        if( path_exists(filename) ) matchName=filename;
    }

    /* Case sensitive match - not checking for ambiguity */
    for( i = 0; i < nsdindx; i++ )
    {
        if( matchName == sdindx[i]->name ) { matchid=i; break; }
    }

    /* Case insensitive match - not checking for ambiguity */
    if( matchid < 0 )
    {
        for( i = 0; i < nsdindx; i++ )
        {
            if( boost::algorithm::iequals( matchName, sdindx[i]->name ) ) { matchid=i; break; }
        }
    }

    /* Path insensitive match (but path delimiter character sensitive) */
    if( matchid < 0 )
    {
        size_t matchlen=matchName.size();
        for( i=0; i < nsdindx; i++ )
        {
            const std::string &dfname=sdindx[i]->name;
            if( dfname.size() > matchlen )
            {
                size_t offset=dfname.size()-matchlen;
                if( boost::algorithm::iequals( dfname.substr(offset), matchName ) &&
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
        fwrite( &sdindx[i]->format, sizeof(sdindx[i]->format), 1, b->f );
        dump_filepath( sdindx[i]->name, b->f );
        dump_string( sdindx[i]->subtype, b->f );
        dump_filepath( sdindx[i]->recodefile, b->f );
        dump_string( context_definition(sdindx[i]->context), b->f );
    }
    end_section( b );
}


int reload_filenames( BINARY_FILE *b )
{
    int i, fmt;
    std::string name;
    std::optional<std::string> subtype;
    std::optional<std::string> recodefile;

    if( find_section(b,"DATA_FILES") != OK ) return MISSING_DATA;
    fread( &i, sizeof(i), 1, b->f );
    while( i-- > 0 )
    {
        fread( &fmt, sizeof(fmt), 1, b->f );
        name = reload_string( b->f );
        subtype = reload_optional_string( b->f );
        recodefile = reload_optional_string( b->f );
        // Note: flawed implementation of restoring context.  
        // 
        const std::string context_def = reload_string( b->f );
        file_context *context=recreate_context(context_def);
        add_data_file_nocopy( name, fmt, subtype, recodefile, context );
    }
    return check_end_section( b );
}
