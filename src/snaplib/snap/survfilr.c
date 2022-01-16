#include "snapconfig.h"
/* Code to manage the list of survey data files */

/*
   $Log: survfilr.c,v $
   Revision 1.3  2004/04/22 02:34:48  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  1998/06/15 02:26:26  ccrook
   Modified to handle long integer number of observations

   Revision 1.1  1995/12/22 20:06:47  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 18:37:51  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>

#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/survfilr.h"
#include "snapdata/snapdata.h"
#include "snapdata/loaddata.h"
#include "snapdata/geoddata.h"
#include "snapdata/snapcsvobs.h"
#include "snapdata/sinexdata.h"
#include "snapdata/stnrecodefile.h"
#include "util/progress.h"
#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/fileutil.h"
#include "util/xprintf.h"

#define PLURAL(d) ( ((d)>1) ? "s" : "" )

// #pragma warning ( disable : 4100 )

static int datafile_progress( DATAFILE * )
{
    update_file_display();
    return 1;
}
// #pragma warning ( default : 4100 )

long read_data_files( FILE *lst )
{
    DATAFILE *d=0;
    survey_data_file *sd;
    int i, c, nfile, nch, sts;
    long file_errors, total_errors, misc_errors;
    char *fname;
    stn_recode_data recodedata;
    file_context *saved_context = current_file_context();

    nfile = survey_data_file_count();
    if( nfile <= 0 ) return 0;

    recodedata.global_map=stnrecode;
    recodedata.net=net;

    fname = NULL;
    nch = 0;
    total_errors=0;

    for (i = 0; i < nfile; i++ )
    {
        sd = survey_data_file_ptr(i);
        c = strlen( sd->name )+1;
        if( c > nch ) nch = c;
    }
    fname = (char *) check_malloc( nch );

    for( i = 0; i < nfile; i++ )
    {
        char *filename;

        if( obsmod_ignore_datafile( obs_modifications, i )) continue;

        sd = survey_data_file_ptr(i);

        set_file_context( sd->context );

        filename = sd->name;

        if( d ) 
        {
            df_close_data_file( d );
            d = 0;
        }

        d = df_open_data_file( filename, "survey data file" );
        if( !d )
        {
            xprintf("\n   Unable to open data file %s\n",sd->name);
            continue;
        }

        if( sd->recodefile && ! sd->recode )
        {
            sd->recode=create_stn_recode_map( net );
            sts = read_station_recode_file( sd->recode, sd->recodefile, filename );
            if( sts != OK )
            {
                xprintf("\n   Unable to read station recode file %s\n",sd->recodefile);
                total_errors++;
                continue;
            }
        }

        recodedata.file_map=sd->recode;
        if( recodedata.global_map || recodedata.file_map )
        {
            set_stn_recode_func( recoded_network_station, &recodedata );
        }
        else
        {
            set_stn_recode_func( 0, 0 );
        }

        for( c = 0; c < NOBSTYPE; c++ ) sd->obscount[c] = 0;
        sd->mindate=sd->maxdate=UNDEFINED_DATE;
        sd->nnodate=0;

        xprintf("\n   Reading data from %s\n",sd->name);
        if( lst )
        {
            fprintf(lst,"\nData file %d: %s\n",(int) (i+1),sd->name);
        }

        misc_errors=get_error_count();
        ldt_init_obs_modifications( obs_modifications );
        ldt_file( i );
        init_file_display( d->f );
        switch( sd->format )
        {
        case GB_FORMAT:
            read_gb_data( d, datafile_progress);
            break;
        case SNAP_FORMAT:
            read_snap_data( d, datafile_progress );
            break;
        case CSV_FORMAT:
            load_snap_csv_obs( sd->subtype, d, datafile_progress );
            break;
        case SINEX_FORMAT:
            load_sinex_obs( sd->subtype, d, datafile_progress );
            break;
        default:
            handle_error( INTERNAL_ERROR, "Program error: Invalid file format",
                          "Occurred in read_data_files");
            break;
        }
        end_file_display();

        if( lst )
        {
            if( sd->mindate != UNDEFINED_DATE )
            {
                fprintf(lst,"    Observations between %s",date_as_string(sd->mindate,"DT?",0));
                fprintf(lst," and %s\n",date_as_string(sd->maxdate,"DT?",0));
            }
            if( sd->nnodate > 0 )
            {
                fprintf(lst,"    %4ld observations do not have a date\n",sd->nnodate);
            }
            for( c = 0; c < NOBSTYPE; c++ )
            {
                if( sd->obscount[c] )
                {
                    xprintf("        %4ld %s%s\n",sd->obscount[c],
                            datatype[c].name,PLURAL(sd->obscount[c]) );
                    if( lst )
                        fprintf(lst,"    %4ld %s%s\n",sd->obscount[c],
                                datatype[c].name,PLURAL(sd->obscount[c]) );
                }
            }
            
            if( sd->recode && recodes_used( sd->recode ) )
            {
                fprintf(lst,"    Recoding stations:\n");
                print_stn_recode_list( lst, sd->recode, 1, stn_name_width, "    ");
            }
        }

        file_errors = df_data_file_errcount( d );
        misc_errors=get_error_count()-misc_errors;
        if( misc_errors < 0 ) misc_errors=0;
        total_errors += file_errors = misc_errors;
        if( file_errors )
        {
            xprintf("   *** %ld error%s reading the file\n",file_errors,
                    PLURAL(file_errors) );

            if( lst )
                fprintf(lst,"   *** %ld error%s reading the file\n",file_errors,
                        PLURAL(file_errors) );
        }

        if( sd->recode ) 
        {
            delete_stn_recode_map( sd->recode );
            sd->recode = 0;
        }

        df_close_data_file( d );
        d=0;
    }
    set_file_context( saved_context );

    set_stn_recode_func( 0, 0 );
    if( d ) df_close_data_file( d );
    if( fname ) check_free( fname );
    
    sts=check_obsmod_station_criteria_codes( obs_modifications, net );
    if( sts >= WARNING_ERROR )
    {
        total_errors++;
        if( lst )
        {
            fprintf(lst,"\nErrors found in station lists in reweight/reject/ignore observations\n");
        }
    }
    return total_errors;
}

// #pragma warning(disable: 4100)

void count_obs( int type, int ifile, double date, char )
{
    survey_data_file *sd=survey_data_file_ptr(ifile);
    sd->obscount[type]++;
    if( date == UNDEFINED_DATE )
    {
        sd->nnodate++;
    }
    else if( sd->mindate == UNDEFINED_DATE )
    {
        sd->mindate=sd->maxdate=date;
    }
    else if( date < sd->mindate )
    {
        sd->mindate=date;
    }
    else if( date > sd->maxdate )
    {
        sd->maxdate=date;
    }
    obstypecount[type]++;
}
