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

#include "output.h"
#include "adjparam.h"
#include "reorder.h"
#include "snap/deform.h"
#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snapmain.h"
#include "stnobseq.h"
#include "util/chkalloc.h"
#include "util/dateutil.h"
#include "util/dms.h"
#include "util/dstring.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/leastsqu.h"
#include "util/license.h"
#include "util/pi.h"
#include "util/probfunc.h"
#include "util/xprintf.h"
#include "version.h"

#define MAX_SUBCOMMANDS 10

typedef struct
{
    const char *name;
    config_store_func store;
    int code;
} output_subcommand;

typedef struct
{
    const char *name;
    char *status;
    char dflt;
    char incompatible[MAX_INCOMPATIBLE_MODES];
    output_subcommand (*subcommands)[MAX_SUBCOMMANDS];
} output_option;

typedef struct relcvr_opt_s
{
    relcvr_opt_s *next;
    double maxlen;
    char *stnlist;
} relcvr_opt;

static int read_cvr_connections( CFG_FILE *cfg, char *string, void *value, int len, int code );

#define RELCVR_CMD_UNDER 0
#define RELCVR_CMD_BETWEEN 1

static output_subcommand relcvr_subcommands[MAX_SUBCOMMANDS]=
{
    {"under",read_cvr_connections,RELCVR_CMD_UNDER},
    {"between",read_cvr_connections,RELCVR_CMD_BETWEEN},
    {0,0,0}
};

static output_option output[] =
{
    {"command_file",&output_command_file,0,{0},0},
    {"input_data",&output_input_data,0,{0},0},
    {"station_recoding",&output_stn_recode,0,{0},0},
    {"file_summary",&output_file_summary,1,{0},0},
    {"problem_definition",&output_problem_definition,0,{0},0},
    {"observation_equations",&output_observation_equations,0,{0},0},
    {"normal_equations",&output_normal_equations,0,{0},0},
    {"observation_deformation",&output_deformation,0,{0},0},
    {"station_adjustments",&output_station_adjustments,0,{ PREANALYSIS, 0},0},
    {"iteration_summary",&output_iteration_summary,1,{ PREANALYSIS, 0},0},
    {"solution_summary",&output_ls_summary,1,{ 0 },0},
    {"residuals",&output_residuals,1,{ PREANALYSIS, 0},0},
    {"file_locations",&output_file_locations,1,{ PREANALYSIS, 0},0},
    {"distance_ratio_scales",&output_distance_ratio_scales,1,{ PREANALYSIS, 0},0},
    {"worst_residuals",&output_worst_residuals,1,{0},0},
    {"error_summary",&output_error_summary,1,{0},0},
    {"grouped_data_by_type",&output_sort_by_type,1,{0},0},
    {
        "station_coordinates",&output_station_coordinates,1,
        {DATA_CONSISTENCY, 0}
    ,0},
    {
        "floated_stations",&output_floated_stations,1,
        {DATA_CONSISTENCY, DATA_CHECK, 0}
    ,0},
    {"station_offsets",&output_station_offsets,1,{0},0},
    {"rejected_stations",&output_rejected_stations,1,{0},0},
    {"rejected_station_coordinates",&output_rejected_coordinates,1,{0},0},
    {"reference_frames",&output_reference_frames,1,{DATA_CONSISTENCY, 0},0},
    {"topocentric_ref_frame",&output_reffrm_topo,0,{0},0},
    {"geocentric_ref_frame",&output_reffrm_geo,0,{0},0},
    {"iers_ref_frame",&output_reffrm_iers,0,{0},0},
    {"parameters",&output_parameters,1,{DATA_CONSISTENCY, 0},0},
    {"form_feeds",&output_form_feeds,0,{0},0},
    {
        "coordinate_file",&output_coordinate_file,1,
        {DATA_CONSISTENCY,DATA_CHECK,PREANALYSIS,0}
    ,0},
    {"binary_file",&output_binary_file,1,{0},0},
    {"decomposition",&output_decomposition,0,{0},0},
    {"relative_covariances",&output_relative_covariances,1,{0},&relcvr_subcommands},
    {"all_relative_covariances",&output_all_covariances,0,{0},0},
    {"full_covariance_matrix",&output_full_covariance,0,{0},0},
    {"sort_stations",&output_sorted_stations,0,{0},0},
    {"notes",&output_notes,1,{0},0},
    {"covariance_matrix_file",&output_covariance,0,{0},0},
    {"covariance_json",&output_covariance_json,0,{0},0},
    {"solution_json",&output_solution_json,0,{0},0},
    {"sinex",&output_sinex,0,{0},0},
    {"debug_reordering",&output_debug_reordering,0,{0},0},
    {NULL,NULL,0,{0},0}
};

static output_option csvopt[] =
{
    {"wkt_shape",&output_csv_shape,0,{0},0},
    {"vector_components",&output_csv_veccomp,1,{0},0},
    {"vector_summary",&output_csv_vecsum,1,{0},0},
    {"vectors_inline",&output_csv_vecinline,1,{0},0},
    {"enu_residuals",&output_csv_vecenu,1,{0},0},
    {"correlations",&output_csv_correlations,0,{0},0},
    {"stations",&output_csv_stations,0,{0},0},
    {"observations",&output_csv_obs,0,{0},0},
    {"metadata",&output_csv_metadata,0,{0},0},
    {"all",&output_csv_allfiles,0,{0},0},
    {"tab_delimited",&output_csv_tab,0,{0},0},
    {NULL,NULL,0,{0},0}
};

static int print_err( int sts, const char *mess1, const char *mess2 );
static int errcount = 0;
static int page_width = 80;
static char *divider = NULL;
static relcvr_opt *relcvr_opts = NULL;

int read_output_options( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    output_option *output_set;
    output_option *o=0;
    char *st;
    char set;
    char errmess[80];

    output_set = code == CSV_OPTIONS ? csvopt : output;

    for( st = strtok(string," "); st; st=strtok(NULL," "))
    {
        // If the last output command has a matched subcommand, then execute its store
        // function on the remainder of the string and return 
        if( o && o->subcommands )
        {
            output_subcommand *sc;
            for( sc=*(o->subcommands); sc->name; sc++ )
            {
                if( _stricmp(sc->name, st) == 0 ) break;
            }
            if( sc->name )
            {
                st=strtok(NULL,"\n");
                if( ! st )
                {
                    sprintf(errmess,"Incomplete output option %.30s", o->name );
                    send_config_error( cfg, INVALID_DATA, errmess );
                    return OK;
                }
                else
                {
                    return (sc->store)(cfg,st,0,0,sc->code);
                }
            }
        }

        if( _stricmp( st, "everything") == 0 )
        {
            for( o = output_set; o->name; o++ ) *(o->status) = 1;
            continue;
        }
        if( _strnicmp(st,"no_",3) == 0 )
        {
            set = 0;
            st += 3;
        }
        else
        {
            set = 1;
        }

        for( o = output_set; o->name; o++ )
        {
            if( _stricmp( st, o->name ) == 0 )
            {
                *(o->status) = set;
                break;
            }
        }
        if( !o->name )
        {
            sprintf(errmess,"Invalid output option %.30s", st );
            send_config_error( cfg, INVALID_DATA, errmess );
        }
        if( ! set || ! o->name ) o=0;
    }
    return OK;
}

static int read_cvr_connections( CFG_FILE *cfg, char *string, void *value, int len, int code )
{
    char errmess[80];
    char *st;
    char *stnlist=NULL;
    double maxlen=0.0;
    int listlen;
    relcvr_opt *rco;

    if( ! net )
    {
        send_config_error( cfg, INVALID_DATA, "Cannot specify output relative_covariances before coordinate_file" );
        return OK;
    }
    if( code == RELCVR_CMD_UNDER )
    {
        st=strtok(string," ");
        if( ! st || sscanf(st,"%lf",&maxlen) != 1 )
        {
            sprintf(errmess,"Invalid relative covariance max length %.30s",st);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
        st=strtok(NULL," ");
        if( st && _stricmp( st, "between") == 0 )
        {
            code=RELCVR_CMD_BETWEEN;
            string=strtok(NULL,"\n");
        }
        else if( st )
        {
            sprintf(errmess,"Invalid option %.30s in output relative_ covariance",st);
            send_config_error( cfg, INVALID_DATA, errmess );
            return OK;
        }
    }
    if( code == RELCVR_CMD_BETWEEN )
    {
        stnlist=string;
    }

    listlen=stnlist ? strlen(stnlist)+1 : 0;
    rco=(relcvr_opt *)check_malloc(sizeof(relcvr_opt)+listlen);
    rco->next=relcvr_opts;
    relcvr_opts=rco;
    rco->maxlen=maxlen;
    rco->stnlist=0;
    if( stnlist )
    {
        rco->stnlist=((char *)(void *)rco)+sizeof(relcvr_opt);
        strcpy(rco->stnlist,stnlist);
    }
    return OK;
}

static void set_usenode( station *st, void *data )
{
    char *usenode=(char *) data;
    if( st->id >= 0 ) usenode[st->id]=1;
}

void delete_requested_covariance_connections()
{
    relcvr_opt *rco;
    while( relcvr_opts )
    {
        rco=relcvr_opts;
        relcvr_opts=relcvr_opts->next;
        check_free(rco);
    }
}

int add_requested_covariance_connections()
{
    relcvr_opt *rco;
    char *usenode;
    station *stnf, *stnt;
    int nnode=number_of_stations(net);
    int sts=OK;

    if( ! relcvr_opts ) return sts;

    // Initiallize list of ids
    usenode=(char *)check_malloc(sizeof(char)*(nnode+1));
    
    for( rco=relcvr_opts; rco; rco=rco->next )
    {
        for( int i=0; i <= nnode; i++ )
        {
            usenode[i]=0;
        }
        if( rco->stnlist )
        {
            int errcount=get_error_count();
            process_selected_stations( net, rco->stnlist, command_file, (void *)usenode, set_usenode );
            if( get_error_count() > errcount )
            {
                handle_error(sts,"Error in relative_covariance station list",rco->stnlist);
                sts=INVALID_DATA;
                break;
            }
        }
        else
        {
            for( reset_station_list(net,1); (stnf=next_station(net)); )
            {
                if( stnf->id >= 0 ) usenode[stnf->id]=1;
            }
        }
        for( int idf=0; idf<nnode; idf++ )
        {
            if( ! usenode[idf] ) continue;
            stnf=station_ptr( net, idf);
            for( int idt=idf+1; idt<=nnode; idt++ )
            {
                if( ! usenode[idt] ) continue;
                if( rco->maxlen > 0 )
                {
                    stnt=station_ptr( net, idt );
                    double distance=calc_distance(stnf,0.0,stnt,0.0,NULL,NULL);
                    if( distance > rco->maxlen ) continue;
                }
                add_connection( idf, idt );
            }

        }
    }
    check_free(usenode);
    return sts;
}

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

    if( ! output_noruntime ) print_report_header( lst );

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

    if( ! output_noruntime ) print_report_header( err );
    print_section_header( err, "ERROR SUMMARY" );
    errcount = 0;
    set_error_handler( print_err );
    return 1;
}


static void close_listing_file( void )
{
    if( ! lst ) return;
    if( errcount > 0 )
    {
        int ierrname=path_len(err_name,0);
        print_section_header( lst, "ERRORS" );
        fprintf(lst,"\nNote: %d errors reported in %s\n",errcount,err_name+ierrname);
        print_section_footer( lst );
    }
    if( ! output_noruntime ) print_report_footer(lst);
    if( lst ) fclose( lst );
    lst = 0;
    xprintf("\n\n****************************************************\n\n");
    xprintf("The results are in file %s\n\n",lst_name);
}


static void close_error_file( const char *mess1, const char *mess2 )
{
    set_error_handler( DEFAULT_ERROR_HANDLER );
    
    if( err ) 
    {
        print_section_footer(err);
        if( ! output_noruntime ) print_report_footer( err );
        fclose( err );
    }
    err = 0;
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
    fprintf(err,"\n%s: %s\n", INFO_ERROR_CONDITION(sts) ? "Warning" : "Error", mess1 );
    if( mess2 ) fprintf(err,"       %s\n",mess2);
    errcount++;
    if( FATAL_ERROR_CONDITION(sts) )
    {
        close_output_files( mess1, mess2 );
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

static void print_centred( FILE *out, const char *heading )
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


void print_report_header( FILE *out )
{
    char heading[100];

    print_line( out );
    skip_line( out );
    sprintf(heading,"PROGRAM %.20s  Version %.20s",PROGRAM,version_number());
    print_centred( out, heading );
    skip_line( out );
    print_centred( out, "Survey Network Adjustment Program" );
    skip_line( out );
    print_centred( out, "Copyright: Land Information New Zealand" );
    print_centred( out, "Author: Chris Crook" );
    sprintf(heading,"Version date: %.20s",version_date() );
    print_centred( out, heading );
    skip_line( out );
    print_line( out );
    sprintf(heading,"Run at %.20s",run_time);
    if( snap_user )
    {
        sprintf(heading+strlen(heading)," by %.40s",snap_user);
    }
    print_centred( out, heading );
    skip_line( out );
}

void print_report_footer( FILE *out )
{
}

void print_control_options( FILE *lst )
{
    fprintf( lst, "\n\nProgram options read from %s\n\n",command_file + path_len(command_file,0) );
    fprintf( lst, "Job: %s\n\n", job_title );
    print_solution_type( lst );
}


void print_section_header( FILE *out, const char *heading )
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

void print_section_footer( FILE *out )
{
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

    print_section_header( lst, "SINGULARITY REPORT" );
    fprintf( lst, "The least squares equations cannot be solved\n");
    sprintf(errmess,"A singularity was detected at %s",paramname);
    if(stno)
    {
        sprintf(errmess+strlen(errmess)," of station %s",station_code(stno));
    }

    fprintf( lst, "%s\n\n", errmess);

    handle_error(INVALID_DATA,"Normal equations are singular",errmess);
    print_section_footer( lst );
}



void print_iteration_header( int iteration )
{
    char heading[35];
    if( output_observation_equations || output_station_adjustments )
    {
        sprintf(heading,"ITERATION NUMBER %d",(int) iteration);
        print_section_header( lst, heading );
    }
    else if( iteration == 1 && output_iteration_summary )
    {
        print_section_header( lst, "ITERATION_SUMMARY" );
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

void print_iteration_footer()
{
    print_section_footer( lst );
}



void print_problem_summary( FILE *lst )
{
    char havefloat;
    char haveauto;
    char reject;
    station *st;
    char adj;
    int row;
    int i;

    print_section_header( lst, "DEFINITION OF PROBLEM" );

    print_solution_type( lst );

    havefloat = 0;
    haveauto = 0;
    reject = 0;
    for( reset_station_list(net,(int)output_sorted_stations);
            NULL != (st = next_station(net)); )
    {
        stn_adjustment *sa=stnadj(st);

        if( sa->flag.float_h || sa->flag.float_v ) { havefloat = 1; }
        if( sa->flag.rejected ) { reject = 1; }
        if( sa->flag.auto_h && sa->flag.auto_v ) { haveauto |= 4; }
        else if( sa->flag.auto_h ) { haveauto |= 2; }
        else if( sa->flag.auto_v ) { haveauto |= 1; }
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

    /* Print out auto constrained stations */
    for( i=4; i >= 1; i >>= 1 )
    {
        int auto_h=1;
        int auto_v=1;
        if( ! (haveauto & i) ) continue;
        switch( i )
        {
            case 4:
                break;
            case 2:
                auto_v=0;
                break;
            case 1:
                auto_h=0;
                break;
        }
        
        fprintf(lst,"\n\nThe following stations have been automatically fixed %s\n",
                i==4 ? "in 3 dimensions" : i==2 ? "horizontally" : "vertically");

        row=0;
        for( reset_station_list(net,(int)output_sorted_stations);
                NULL != (st = next_station(net)); )
        {
            if( stnadj(st)->flag.rejected ) continue;
            stn_adjustment *sa=stnadj(st);
            if( sa->flag.auto_h==auto_h && sa->flag.auto_v==auto_v )
            {
                if( row >= 80 ) { fputs("\n",lst); row=0; }
                fprintf(lst," %-*s", stn_name_width,st->Code );
                row += stn_name_width+1;
            }
        }
    }

    list_calculated_parameters( lst );

    if( deformation )
    {
        fputs("\n\nThe following deformation has been applied\n",lst);
        print_deformation_model( deformation, lst,"");
    }

    print_section_footer( lst );
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

    print_section_header( lst, "SOLUTION SUMMARY" );

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
    print_bandwidth_reduction( lst );
    print_section_footer( lst );
}


void print_bandwidth_reduction( FILE *lst )
{
    long nsave, nfull;
    /* Report effectiveness of bandwidth reduction */
    if( nprm )
    {
        nfull = ((long) nprm * (nprm+1))/2;
        nsave = nfull - lsq_normal_matrix()->nelement;
        if( nsave > 0 )
        {
            fprintf(lst,"\nStation reordering has reduced the matrix size by %.1lf%%\n",(nsave*100.0)/nfull);
        }
    }
}

void print_json_start( FILE *out, const char *name )
{
    fprintf( out, "\nBEGIN_JSON %s\n",name);
}

void print_json_end( FILE *out, const char *name )
{
    fprintf( out, "\nEND_JSON %s\n",name);
}

output_csv *open_output_csv(const char *type)
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
    if( date == UNDEFINED_DATE ) { write_csv_null_field( csv ); return; }
    write_csv_string( csv, date_as_string(date,0,0) );
}

void print_json_params( FILE *lst, int nprefix )
{
    fprintf(lst,"%*s\"nparam\":%d",nprefix,"",nprm);
    if( nprm )
    {
        fprintf(lst,",\n%*s\"parameters\": [",nprefix,"");
        for( int i = 0; i++ < nprm; )
        {
            int stno=0;
            char paramname[40];
            if( ! find_param_row(i,paramname,40) && 
                    !(stno=find_station_row(i,paramname,40)))
            {
                sprintf(paramname,"Parameter %d",i);
            }
            fprintf(lst,"%s\n%*s\"%s%s%s\"", 
                    i > 1 ? "," : "", 
                    nprefix+2,"",
                    stno ? station_code(stno) : "",
                    stno ? ": " : "",
                    paramname );
        }
        fprintf(lst,"\n%*s]",nprefix,"");
    }
}

void print_solution_json_file()
{
    int nch;
    char *bfn;
    FILE *f;
    bltmatrix *invnorm;

    nch = strlen( root_name ) + strlen(SOLNFILE_EXT)+strlen( JSONFILE_EXT ) + 1;
    bfn = ( char * ) check_malloc( nch );
    strcpy( bfn, root_name );
    strcat( bfn, SOLNFILE_EXT );
    strcat( bfn, JSONFILE_EXT );

    f = fopen( bfn, "w" );
    if( !f )
    {
        handle_error( FILE_OPEN_ERROR,"Unable to open JSON solution file", bfn );
    }
    else
    {
        xprintf("\nCreating the JSON solution file %s\n",bfn);
    }
    check_free( bfn );
    if( !f ) return;

    fprintf(f,"{\n");
    fprintf(f,"  \"nparam\": %d,\n",(int)nprm);
    fprintf(f,"  \"nimplicit_param\": %ld,\n",(long)(nschp));
    fprintf(f,"  \"nobs\": %ld,\n",(long)(nobs+nschp));
    fprintf(f,"  \"nconstraint\": %ld,\n",(long)(ncon));
    fprintf(f,"  \"dof\": %ld,\n",(long) dof);
    fprintf(f,"  \"ssr\": %.5lf,\n",ssr);
    fprintf(f,"  \"seu\": %.5lf,\n",seu);
    print_json_params( f, 2 );
    fprintf(f,",\n  \"covariance\":\n  ");
    invnorm = lsq_normal_matrix();
    print_bltmatrix_json( invnorm, f, 4, BLT_JSON_FULL | BLT_JSON_MATRIX_ONLY, "%.8le");
    fprintf(f,"\n}\n");
    fclose(f);
}
