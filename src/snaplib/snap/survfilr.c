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
#include "snap/survfilr.h"
#include "snapdata/snapdata.h"
#include "snapdata/loaddata.h"
#include "snapdata/geoddata.h"
#include "snapdata/snapcsvobs.h"
#include "util/progress.h"
#include "util/errdef.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/xprintf.h"

#define PLURAL(d) ( ((d)>1) ? "s" : "" )

// #pragma warning ( disable : 4100 )

static int datafile_progress( DATAFILE *df )
{
    update_file_display();
    return 1;
}
// #pragma warning ( default : 4100 )

long read_data_files( char *base_dir, FILE *lst )
{
    DATAFILE *d;
    survey_data_file *sd;
    int i, c, nfile, nch;
    long file_errors, total_errors;
    char *fname;

    total_errors = 0;

    nfile = survey_data_file_count();

    fname = NULL;
    nch = 0;

    if( base_dir )
    {
        for (i = 0; i < nfile; i++ )
        {
            c = strlen( survey_data_file_name( i ));
            if( c > nch ) nch = c;
        }
        nch += 1 + strlen( base_dir );
        fname = (char *) check_malloc( nch );
    }

    for( i = 0; i < nfile; i++ )
    {
        char *filename;
        sd = survey_data_file_ptr(i);
        filename = sd->name;

        if( base_dir )
        {
            build_filespec( fname, nch, base_dir, filename, NULL );
            if( file_exists( fname )) filename = fname;
        }

        d = df_open_data_file( filename, "survey data file" );
        if( !d )
        {
            xprintf("\n   Unable to open data file %s\n",sd->name);
            total_errors++;
            continue;
        }

        for( c = 0; c < NOBSTYPE; c++ ) sd->obscount[c] = 0;

        xprintf("\n   Reading data from %s\n",sd->name);
        if( lst )
        {
            fprintf(lst,"\nData file %d: %s\n",(int) (i+1),sd->name);
            if( sd->errfct != 1.0 )
                fprintf(lst,"        Errors multiplied by %.2lf\n",sd->errfct);
        }

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
        default:
            handle_error( INTERNAL_ERROR, "Program error: Invalid file format",
                          "Occurred in read_data_files");
            break;
        }
        end_file_display();

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


        file_errors = df_data_file_errcount( d );
        if( file_errors )
        {
            total_errors += file_errors;
            xprintf("   *** %ld error%s reading the file\n",file_errors,
                    PLURAL(file_errors) );

            if( lst )
                fprintf(lst,"   *** %ld error%s reading the file\n",file_errors,
                        PLURAL(file_errors) );
        }

        df_close_data_file( d );
    }

    if( fname ) check_free( fname );
    return total_errors;
}

// #pragma warning(disable: 4100)

void count_obs( int type, int ifile, char unused )
{
    survey_data_file_ptr(ifile)->obscount[type]++;
    obstypecount[type]++;
}
