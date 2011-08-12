#include "snapconfig.h"
/* Output file maintenance .. */

/*
   $Log: output.c,v $
   Revision 1.8  2004/04/22 02:35:44  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.7  2003/11/27 00:15:15  ccrook
   Modified to ensure that version number is always compiled into SNAP

   Revision 1.6  2003/11/25 01:29:59  ccrook
   Updated SNAP to allow calculation of projection bearings in coordinate
   systems other than that of the coordinate file

   Revision 1.5  2003/11/23 23:05:18  ccrook
   Updated iteration output to display number of stations exceeding adjustment
   tolerance

   Revision 1.4  1999/05/20 10:42:59  ccrook
   Changed header from DOSLI to LINZ, and removed licence information

   Revision 1.3  1998/06/15 02:23:53  ccrook
   Modified to handle long integer count of observations

   Revision 1.2  1998/05/21 04:01:56  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.1  1996/01/03 22:03:29  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define OUTPUT_C
#include "snapmain.h"
#include "util/chkalloc.h"
#include "util/fileutil.h"
#include "util/dateutil.h"
#include "util/dstring.h"
#include "output.h"
#include "util/errdef.h"
#include "snap/stnadj.h"
#include "stnobseq.h"
#include "adjparam.h"
#include "util/probfunc.h"
#include "version.h"
#include "util/license.h"
#include "util/leastsqu.h"
#include "snap/deform.h"
#include "util/dms.h"
#include "util/pi.h"
#include "util/xprintf.h"

#define RTOD (180.0/PI)


static char rcsid[]="$Id: output.c,v 1.8 2004/04/22 02:35:44 ccrook Exp $";

static int print_err( int sts, const char *mess1, const char *mess2 );
static int errcount = 0;
static int page_width = 80;
static char *divider = NULL;


int open_output_files( )
{
    char errmess[80];
    int rlen;

    rlen = strlen( root_name );

    lst_name = (char *) check_malloc( rlen + strlen( LISTINGFILE_EXT ) + 1);
    strcpy( lst_name, root_name );
    strcpy( lst_name+rlen, LISTINGFILE_EXT );
    lst = fopen( lst_name, "w" );
    if( !lst )
    {
        sprintf(errmess,"Unable to open listing file %.40s",lst_name);
        handle_error( FILE_OPEN_ERROR, errmess,"Aborting program");
        return 0;
    }

    if( ! output_noruntime ) print_header( lst );

    err_name = (char *) check_malloc( rlen + strlen( ERRORFILE_EXT ) + 1);
    strcpy( err_name, root_name );
    strcpy( err_name+rlen, ERRORFILE_EXT );
    err = fopen( err_name, "w" );
    if( !err )
    {
        sprintf(errmess,"Unable to open error file %.40s",err_name);
        handle_error( FILE_OPEN_ERROR, errmess,"Aborting program");
        return 0;
    }

    if( ! output_noruntime ) print_header( err );
    print_section_heading( err, "ERROR SUMMARY" );
    errcount = 0;
    set_error_handler( print_err );
    return 1;
}


static void close_listing_file( void )
{
    if( lst ) fclose( lst );
    xprintf("\n\n****************************************************\n\n");
    xprintf("The results are in file %s\n\n",lst_name);
}


static void close_error_file( const char *mess1, const char *mess2 )
{
    if( err ) fclose( err );
    if( errcount <= 0 )
    {
        _unlink( err_name );
    }
    else
    {
        fprintf(stderr,"\n\n****************************************************\n\n");
        fprintf(stderr,"See the errors reported in %s\n",err_name);
        if( mess1 || mess2 )
        {
            fprintf(stderr,"\nThe program stopped with the following error:\n");
            if(mess1) fprintf(stderr,"%s\n",mess1);
            if(mess2) fprintf(stderr,"%s\n",mess2);
        }
        fprintf(stderr,"\n****************************************************\n\n");
    }
}


static int print_err( int sts, const char *mess1, const char *mess2 )
{
    fprintf(err,"\n%s: %s\n", sts == WARNING_ERROR ? "Warning" : "Error", mess1 );
    if( mess2 ) fprintf(err,"       %s\n",mess2);
    errcount++;
    if( FATAL_ERROR_CONDITION(sts) )
    {
        close_output_files( mess1, mess2 );
        set_error_handler( DEFAULT_ERROR_HANDLER );
    }
    return sts;
}

void close_output_files( const char *mess1, const char *mess2 )
{
    close_listing_file();
    close_error_file( mess1, mess2 );
}



/* Cancel any outputs which are inconsistent with the program mode */
/* Also checks that allocated space for station codes is adequate  */

void init_output_options( void )
{
    output_option *o;
    for( o = output; o->name; o++ ) *(o->status) = o->dflt;
    for( o = csvopt; o->name; o++ ) *(o->status) = o->dflt;
}


void eliminate_inconsistent_outputs( void )
{
    output_option *o;
    int i;
    int nc;

    for( o = output; o->name; o++ )
    {
        for( i=0; i<MAX_INCOMPATIBLE_MODES; i++ )
        {
            if( o->incompatible[i] == 0 ) break;
            if( o->incompatible[i] == program_mode ) { o->status = 0; break; }
        }
    }

    for( i=0; i++<number_of_stations( net ); )
    {
        nc = strlen(station_code(i));
        if( nc > stn_name_width ) stn_name_width = nc;
    }

}

static void new_page( FILE *out )
{
    fprintf(out,"\n%c\n",output_form_feeds ? '\f' : '\n');
}

static void print_line( FILE *out )
{
    int i;
    if( !divider )
    {
        divider = (char *) check_malloc( page_width + 2 );
        for( i=0; i<page_width; i++ ) divider[i] = '=';
        divider[i] = '\n';
        divider[i+1] = 0;
    }
    fputs( divider, out );
}

static void print_centred( FILE *out, char *heading )
{
    int pos;
    pos = (page_width - strlen(heading))/2;
    if( pos < 0 ) pos = 0;
    fprintf(out,"%*s%s\n",pos,"",heading);
}

static void skip_line( FILE *out )
{
    fputc('\n',out);
}


void print_solution_type( FILE *lst )
{

    fputs("\nSolution type: ",lst);
    switch (dimension)
    {
    case 1: fputs("Vertical ",lst); break;
    case 2: fputs("Horizontal ",lst); break;
    case 3: fputs("3d ",lst); break;
    }
    switch (program_mode)
    {
    case ADJUST:           fputs("coordinate adjustment\n",lst); break;
    case PREANALYSIS:      fputs("network preanalysis\n",lst); break;
    case DATA_CHECK:       fputs("data checking\n",lst); break;
    case DATA_CONSISTENCY: fputs("data internal consistency check\n",lst); break;
    }
}


void print_header( FILE *out )
{
    char heading[100];

    print_line( out );
    skip_line( out );
    sprintf(heading,"PROGRAM %s  Version %s",PROGRAM,version_number());
    print_centred( out, heading );
    skip_line( out );
    print_centred( out, "Survey Network Adjustment Program" );
    skip_line( out );
    print_centred( out, "Copyright: Land Information New Zealand" );
    print_centred( out, "Author: Chris Crook" );
    sprintf(heading,"Version date: %s",version_date() );
    print_centred( out, heading );
    skip_line( out );
    print_line( out );
    sprintf(heading,"Run at %s",run_time);
    print_centred( out, heading );
    skip_line( out );
}


void print_control_options( FILE *lst )
{
    fprintf( lst, "\n\nProgram options read from %s\n\n",command_file + path_len(command_file,0) );
    fprintf( lst, "Job: %s\n\n", job_title );
    print_solution_type( lst );
}



void print_section_heading( FILE *out, char *heading )
{
    int rtl;

    new_page( out );
    print_line( out );
    rtl = strlen( run_time );
    fprintf(out,"%-*s   %s\n\n",page_width - rtl - 3, job_title,
            output_noruntime ? "" : run_time );

    rtl = (page_width - strlen( heading ))/2;
    if( rtl < 0 ) rtl = 0;

    print_centred(out,heading);
    fputc('\n',out);
    print_line( out );
}


void handle_singularity( int sts )
{
    char paramname[40];
    char errmess[120];
    int stno;

    stno = 0;
    if( !find_param_row( sts, paramname, 40 ) &&
            ((stno = find_station_row( sts, paramname, 40 )) == 0) )
    {
        sprintf(paramname,"Parameter %d", (int) sts );
    }

    print_section_heading( lst, "SINGULARITY REPORT" );
    fprintf( lst, "The least squares equations cannot be solved\n");
    sprintf(errmess,"A singularity was detected at %s",paramname);
    if(stno)
    {
        sprintf(errmess+strlen(errmess)," of station %s",station_code(stno));
    }

    fprintf( lst, "%s\n\n", errmess);

    handle_error(INVALID_DATA,"Normal equations are singular",errmess);
}



void print_iteration_header( int iteration )
{
    char heading[35];
    if( output_observation_equations || output_station_adjustments )
    {
        sprintf(heading,"ITERATION NUMBER %d",(int) iteration);
        print_section_heading( lst, heading );
    }
    else if( iteration == 1 && output_iteration_summary )
    {
        print_section_heading( lst, "ITERATION_SUMMARY" );
    }
}


void print_iteration_update( int iteration, double maxadj,
                             int maxstn, int nstnadj )
{
    if( output_station_adjustments )
    {
        fprintf(lst,"\n");
        print_coordinate_changes( lst );
    }

    if( output_iteration_summary )
    {
        fprintf(lst,"\nIteration %d: Maximum change is %.4lf metres at station %s\n",
                (int)iteration, maxadj, station_code(maxstn));
        fprintf(lst,"             %d station adjustments exceed convergence criteria\n",
                (int)nstnadj );
    }
}



void print_problem_summary( FILE *lst )
{
    char havefloat;
    char reject;
    station *st;
    char adj;
    int row;

    print_section_heading( lst, "DEFINITION OF PROBLEM" );

    print_solution_type( lst );

    havefloat = 0;
    reject = 0;
    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        if( stnadj(st)->flag.float_h || stnadj(st)->flag.float_v ) { havefloat = 1; }
        if( stnadj(st)->flag.rejected ) { reject = 1; }
    }

    fputs("\n\nThe following table lists the stations included in the adjustment.\n",lst);
    fputs("Adjustment is signified by F (fixed), ",lst);
    if( dimension == 3 )
    {
        fputs("H (horizontal), V (vertical) or 3 (3d).\n",lst);
    }
    else
    {
        fputs("A (adjusted).\n",lst);
    }
    if( havefloat )
    {
        fputs("Where errors are specified the adjustment is constrained by giving the\n",lst);
        fputs("initial station coordinates the specified apriori expected error\n",lst);
    }

    fprintf(lst,"\n\n%-*s  Adj",stn_name_width+2,"Station");
    if( havefloat )
    {
        if( dimension != 3 )
        {
            fputs("   Error",lst);
        }
        else
        {
            fputs("      Error     ",lst);
        }
    }
    fputs("  Row",lst);
    fputs("  Name\n",lst);
    if(havefloat && dimension == 3) fputs("                Hor     Ver\n",lst);
    fputs("\n",lst);

    for( reset_station_list(net, (int) output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        if( stnadj(st)->flag.rejected ) continue;

        if( stnadj(st)->flag.adj_h || stnadj(st)->flag.adj_v )
        {
            if(dimension < 3 )
            {
                adj = 'A';
            }
            else
            {
                adj = '3';
                if( !stnadj(st)->flag.adj_h ) adj = 'V';
                if( !stnadj(st)->flag.adj_v ) adj = 'H';
            }
        }
        else
        {
            adj = 'F';
        }

        fprintf( lst,"%-*s    %c ",stn_name_width,st->Code, adj );
        if( havefloat )
        {
            if( dimension != 1 )
            {
                if( stnadj(st)->flag.float_h )
                {
                    fprintf(lst,"  %6.3f",stnadj(st)->herror);
                }
                else
                {
                    fputs("    -   ",lst);
                }
            }
            if( dimension != 2 )
            {
                if( stnadj(st)->flag.float_v )
                {
                    fprintf(lst,"  %6.3f",stnadj(st)->verror);
                }
                else
                {
                    fputs("    -   ",lst);
                }
            }
        }
        row = -1;
        if( stnadj(st)->flag.adj_h ) row = stnadj(st)->hrowno;
        else if( stnadj(st)->flag.adj_v ) row = stnadj(st)->vrowno;
        if( row < 0 ) { fprintf(lst,"    -");}
        else { fprintf(lst," %4d",row); }
        fprintf(lst,"  %s\n",st->Name);
    }

    if( reject )
    {
        fputs("\n\n\nThe following stations have been rejected.  Those denoted with '*'\n",lst);
        fputs("have been rejected automatically because there is insufficient data\n",lst);
        fputs("to locate them.\n\n",lst);
        fprintf(lst,"%-*s  Name\n\n",stn_name_width+2,"Station");
    }

    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        if( !stnadj(st)->flag.rejected ) continue;
        fprintf(lst,"%c%-*s   %s\n",stnadj(st)->flag.autoreject ? '*' : ' ',
                stn_name_width,st->Code, st->Name );
    }

    list_calculated_parameters( lst );

    if( deformation )
    {
        fputs("\n\nThe following deformation has been applied\n",lst);
        print_deformation_model( deformation, lst,"");
    }


}



/* Print out a summary of statistics from the least squares adjustment */

void print_ls_summary( FILE *lst )
{
    fprintf(lst,"\nDegrees of freedom:               %5ld\n",(long) dof);
    fprintf(lst,"Sum of squared residuals:         %11.5lf\n",ssr);
    fprintf(lst,"Standard error of unit weight:    %11.5lf\n",seu);
}

void xprint_ls_summary()
{
    xprintf("\nDegrees of freedom:               %5ld\n",(long) dof);
    xprintf("Sum of squared residuals:         %11.5lf\n",ssr);
    xprintf("Standard error of unit weight:    %11.5lf\n",seu);
}

void print_solution_summary( FILE *lst )
{

    double c2sig;
    long nsave, nfull;

    print_section_heading( lst, "SOLUTION SUMMARY" );

    print_solution_type( lst );

    fprintf(lst,"\n\nNumber of observations:           %5ld",(long)(nobs + nschp));

    fprintf(lst,"\nNumber of parameters:             %5d\n",(int)nprm);
    if(nschp) fprintf(lst,"Number of implicit parameters:    %5ld\n",(long)nschp);
    if(ncon) fprintf(lst,"Number of arbitrary constraints:  %5ld\n",(long)ncon);

    print_ls_summary( lst );

    if( program_mode == PREANALYSIS || dof <= 0 ) return;

    c2sig = chi2_distn( ssr, dof );

    if( c2sig > 0.5 )
    {
        c2sig = 100.0 * (1.0 - c2sig);
        fprintf(lst,"\nThe probability of an SSR this low is %.3lf%% (from Chi squared (%ld))\n",
                c2sig,(long) dof);
        if( c2sig < 2.5 ) fprintf(lst,"\nYou may have over-estimated the errors of the data.\n");
    }
    else
    {
        c2sig = 100.0 * c2sig;
        fprintf(lst,"\nThe probability of an SSR this high is %.3lf%% (from Chi squared (%ld))\n",
                c2sig,(long) dof);
        if( c2sig < 2.5 )
        {
            fprintf(lst,"\nYou may have under-estimated the errors of the data,\n");
            fprintf(lst,"or there may be gross errors in the data\n");
            if( program_mode != DATA_CONSISTENCY )
                fprintf(lst,"or the fixed stations may be incorrectly positioned\n");
        }
    }


    /* Report effectiveness of bandwidth reduction */

    if( nprm )
    {
        nfull = ((long) nprm * (nprm+1))/2;
        nsave = nfull - lsq_normal_matrix()->nelement;
        if( nsave > 0 )
        {
            fprintf(lst,"\nStation reordering has reduced the matrix size by %ld%%\n",(nsave*100)/nfull);
        }
    }
}


output_csv *open_output_csv(char *type)
{
    int rlen;
    output_csv *csv;
    char *filename;
    FILE *f;

    rlen = strlen( root_name );

    filename = (char *) check_malloc( rlen + strlen(type) + 6);
    strcpy(filename,root_name);
    strcpy(filename+rlen,"-");
    strcpy(filename+rlen+1,type);
    if( output_csv_tab ) { strcat(filename,".txt"); }
    else { strcat(filename,".csv"); }

    f = fopen(filename,"w");
    if( !f )
    {
        char errmess[120];
        sprintf(errmess,"Unable to open listing file %.80s",filename);
        handle_error( FILE_OPEN_ERROR, errmess,"Aborting program");
        check_free(filename);
        return 0;
    }

    csv = (output_csv *) check_malloc( sizeof(output_csv));
    csv->filename = filename;
    csv->f = f;
    csv->delim = output_csv_tab ? '\t' : ',';
    csv->delimrep = strcpy( csv->charbuf, output_csv_tab ? " " : "," );
    csv->tab = output_csv_tab;
    csv->quote = output_csv_tab ? 0 : '"';
    csv->quoterep = strcpy(csv->charbuf+3,output_csv_tab ? "\"" : "\"\"");
    csv->newlinerep = strcpy(csv->charbuf+6, output_csv_tab ? " " : "\n");
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
        if( ! isalnum(ch) ) ch = '_';
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
    char datestr[32];
    int year, month, day, hour, min, sec;
    if( date == 0 ) { write_csv_null_field( csv ); return; }

    date_as_ymdhms( date, &year, &month, &day, &hour, &min, &sec );
    sprintf( datestr,"%04d-%02d-%02d %02d:%02d:%02d",year,month,day,hour,min,sec);
    write_csv_string( csv, datestr );
}