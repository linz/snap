#include "snapconfig.h"
/* Adjustment program main section */

/*
	$Log: snapmain.c,v $
	Revision 1.10  2004/04/22 02:35:44  ccrook
	Setting up to support linux compilation (x86 architecture)

	Revision 1.9  2003/11/28 01:56:47  ccrook
	Updated to defer formulation of full normal equation matrix to calculation
	of inverse when it is required, rather than bypassing station reordering.

	Revision 1.8  2003/11/27 00:15:15  ccrook
	Modified to ensure that version number is always compiled into SNAP

	Revision 1.7  2003/11/23 23:05:19  ccrook
	Updated iteration output to display number of stations exceeding adjustment
	tolerance

	Revision 1.6  2003/05/16 01:20:42  ccrook
	Added option to store all relative covariances in binary file.

	Revision 1.5  1999/05/20 10:45:42  ccrook
	Added call to test relative accuracies of stations.

	Revision 1.4  1998/05/21 04:01:55  ccrook
	Added support for deformation model to be applied in the adjustment.

	Revision 1.3  1998/03/06 15:59:12  CHRIS
	Changed output for DATA_CONSISTENCY mode adjustment to include dumping
	the variance covariance information.  This may not be the most sensible
	option?

	Revision 1.2  1996/01/10 19:51:13  CHRIS
	Added function to allow binary dump of full covariance matrix.
	This does a row by row choleski inversion to get the covariance
	matrix, so it could be slow.  It doesn't require the full matrix
	to be stored however.

	Revision 1.1  1996/01/03 22:09:11  CHRIS
	Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "util/binfile.h"
#include "util/chkalloc.h"
#include "util/progress.h"
#include "util/fileutil.h"
#include "util/errdef.h"
#include "util/get_date.h"
#include "util/dstring.h"
#include "util/xprintf.h"

#define _SNAPMAIN_C
#include "snapmain.h"

#include "adjparam.h"
#include "bindata2.h"
#include "control.h"
#include "coordsys/coordsys.h"
#include "csdeform.h"
#include "cvrfile.h"
#include "loadsnap.h"
#include "network/network.h"
#include "output.h"
#include "relerror.h"
#include "reorder.h"
#include "residual.h"
#include "ressumry.h"
#include "rftrnadj.h"
#include "snap/bindata.h"
#include "snap/deform.h"
#include "snap/rftrans.h"
#include "snap/rftrndmp.h"
#include "snap/stnadj.h"
#include "snap/survfilr.h"
#include "snapdata/survdata.h"
#include "sortobs.h"
#include "stnobseq.h"
#include "testspec.h"
#include "util/classify.h"
#include "util/leastsqu.h"
#include "version.h"

static void print_help( void );
static void print_command_file( void );
static int read_parameters( int argc, char *argv[] );
static void update_station_file( void );
static BINARY_FILE *open_dump_file( void );
static void dump_binary_data( BINARY_FILE *b );
static void dump_choleski_decomposition( BINARY_FILE *b );
static void dump_covariance_matrix( BINARY_FILE *b );
static void summarize_data_modifications( FILE *lst );
static void write_metadata_csv();

int snap_main( int argc, char *argv[] )
{

    int show_iterations;
    int sts;
    int maxstn;
    int nstnadj;
    int read_errors;
    double maxadj;
    char errmess[256];
    BINARY_FILE *dump;

    /* MS VC compatibility */
    _set_output_format(_TWO_DIGIT_EXPONENT);

    get_date( run_time );

    init_snap_globals();

    if( read_parameters( argc, argv ) != OK )
    {
        print_help();
        return DEFAULT_RETURN_STATUS;
    }

    /* Load the coordinate system definition file */

    install_default_crdsys_file();

    /* Check that the command file exists */

    if( config_file && !file_exists(config_file) )
    {
        xprintf("\nCannot open configuration file %s\n", config_file );
        return DEFAULT_RETURN_STATUS;
    }

    if( !file_exists(command_file) )
    {
        xprintf("\nCannot open command file %s\n", command_file );
        return DEFAULT_RETURN_STATUS;
    }

    /* Initiallize the program */

    xprintf("\nInitiallizing the program\n");
    init_output_options();
    if( ! open_output_files( ) )
    {
        xprintf("\nUnable to create output files\n");
        return DEFAULT_RETURN_STATUS;
    }

    /* Read the configuration file - also loads the station coordinates */

    process_default_configuration();

    if( config_file )
    {
        xprintf("\nReading the configuration file %s\n", config_file);
        if( read_configuration_file( config_file ) != OK )
        {
            xprintf("\nErrors loading configuration file %s\n", config_file );
            handle_error( INVALID_DATA, "The configuration file is not correct", NO_MESSAGE );
            close_output_files(0,0);
            return DEFAULT_RETURN_STATUS;
        }
    }

    /* Read the command file */

    xprintf("\nReading the command file %s\n", command_file );

    /* Set station initiallization for reading the station file before it is loaded by read_command_file */
    set_stnadj_init_network();

    if( read_command_file( command_file ) != OK )
    {
        xprintf("\nErrors loading command file %s\n", command_file );
        handle_error(INVALID_DATA, "The command file is not correct",NO_MESSAGE);
        close_output_files(0,0);
        return DEFAULT_RETURN_STATUS;
    }

    /* Tidy up selected options */

    eliminate_inconsistent_outputs();
    if( output_all_covariances )
    {
        output_relative_covariances = 1;
    }

    /* If the command file is to be echoed to the output - do it now */

    if( output_command_file )
    {
        print_command_file();
        if( output_file_summary ) print_section_heading( lst, "SUMMARY OF INPUT FILES" );
    }

    /* If the coordinate system has a deformation model, install it now */

    if( has_deformation_model( net->crdsys ) )
    {
        if( deformation )
        {
            /*
            handle_error( WARNING_ERROR,
                          "The adjustment is using deformation model which overrides that of the coordinate system",
                          NO_MESSAGE);
            */
        }
        else if( ignore_deformation )
        {
            /*
            handle_error( WARNING_ERROR,
                          "The adjustment is not using the deformation model of the coordinate system",
                          NO_MESSAGE);
            */
        }
        else
        {
            create_crdsys_deformation( &deformation );
        }
    }

    /* Print out summary of input options */

    print_control_options( lst );

    /* Open the binary dump file ready to hold the data */

    dump = NULL;
    if( output_binary_file )
    {
        dump = open_dump_file();
        if( dump == NULL )
        {
            xprintf("\nUnable to create binary file\n");
            close_output_files(0,0);
            return DEFAULT_RETURN_STATUS;
        }
        create_section( dump, "OBSERVATIONS" );
    }

    init_bindata( dump ? dump->f : NULL);

    /* Read the data defined in the control file into the binary data file */

    if( output_file_summary )
    {
        fprintf(lst,"\n\nCoordinates file %s\n    %4d stations read\n",
                station_filename, (int) number_of_stations( net ));
    }

    if( geoid_file )
    {
        if( !(net->options & NW_GEOID_HEIGHTS) || overwrite_geoid )
        {
            sts = set_network_geoid( net, geoid_file );
            if( sts != OK ) 
            {
                close_output_files(0,0);
                return DEFAULT_RETURN_STATUS;
            }
        }
    }

    xprintf("\nReading the data files...\n");
    init_connections( number_of_stations( net ));
    {
        double lt, ln;
        get_network_topocentre( net, &lt, &ln );
        setup_rftrans_list( lt, ln );
    }

    init_load_snap();
    read_errors = read_data_files( cmd_dir, output_file_summary ? lst : NULL );
    sts = term_load_snap();
    end_bindata();
    if( dump ) end_section( dump );


    /* Report information from loading the data before stopping as this 
       could help understand what went wrong.

       Annotate the file summary section of the list file with those
       observations that will have error factors applied, or are being
       rejected or ignored */

    summarize_data_modifications( lst );
    if( output_input_data ) print_input_data( lst );

    if( read_errors )
    {
        xprintf( "\n%d errors reported reading the data files\n", (int) read_errors);
        sprintf( errmess, "%d errors reported reading the data files", (int) read_errors);
        handle_error(INVALID_DATA, errmess, NO_MESSAGE );
        close_output_files(0,0);
        return DEFAULT_RETURN_STATUS;
    }
    else if ( sts != OK )
    {
        xprintf("\nErrors encountered reading the data files\n");
        handle_error( INVALID_DATA, "Errors encountered reading the data files", NO_MESSAGE );
        close_output_files(0,0);
        return DEFAULT_RETURN_STATUS;
    }

    /* Mark the end of the binary data */


    /* Echo the input data if required */

    if( output_stn_recode ) print_station_recoding( lst );

    if( sort_obs ) sort_observation_list();


    xprintf("\nReading the station constraint commands %s\n", command_file );
    if( read_command_file_constraints( command_file ) != OK )
    {
        xprintf("\nErrors loading command file %s\n", command_file );
        handle_error(INVALID_DATA, "The command file is not correct",NO_MESSAGE);
        close_output_files(0,0);
        return DEFAULT_RETURN_STATUS;
    }

    /* Set up parameters - sorts out which stations are to be used, creates
       station ordering, numbers station coordinate and other parameters,
       and allocates the least squares matrices */

    init_rftrans_prms_list();
    nprm = setup_parameters();

    if( deformation && init_deformation( deformation ) != OK )
    {
        xprintf("\nUnable to initiallize the deformation model\n");
        handle_error( INVALID_DATA, "Unable to initiallized deformation model", NO_MESSAGE );
        close_output_files(0,0);
        return DEFAULT_RETURN_STATUS;
    }

    if( output_problem_definition ) print_problem_summary( lst );

    /* Main adjustment loop */

    iterations = 0;
    show_iterations = program_mode == ADJUST || program_mode == DATA_CONSISTENCY;

    for(;;)
    {

        iterations++;

        if( show_iterations )
        {
            xprintf("\nIteration %d\n",(int)iterations);
            print_iteration_header( iterations );
        }

        /* Initiallize the least squares equations */

        lsq_init();

        /* Sum the observations */

        xprintf("   Summing the data\n");

        sts=sum_bindata( iterations );
        if( sts != OK )
        {
            xprintf("\nThe adjustment cannot be solved - observations cannot be summed\n");
            close_output_files(0,0);
            return DEFAULT_RETURN_STATUS;
        }

        sum_floating_stations(iterations);

        /* Solve the equations - if we are doing preanalysis we want to
        ignore any information from the input observations - we are
         only interested in the covariance of the derived parameters */

        if( output_normal_equations )
        {
            char header[30];
            sprintf(header,"normal_equations_%d",iterations);
            print_section_heading( lst, "NORMAL EQUATIONS" );
            print_json_start(lst,header);
            fprintf(lst,"{\n");
            print_json_params(lst,2);
            fprintf(lst,",\n\"normal_equations\": ");
            lsq_print_normal_equations_json( lst, 2 );
            fprintf(lst,"\n}");
            print_json_end(lst,header);
        }

        xprintf("   Solving the equations\n");

        if( program_mode == PREANALYSIS ) lsq_zero_b_vector();

        if( program_mode == DATA_CONSISTENCY )
        {
            ncon = lsq_solve_equations(1);
            sts = 0;
        }
        else
        {
            ncon = 0;
            sts = lsq_solve_equations(0);
        }
        if( sts != 0 )
        {
            handle_singularity( sts );
            xprintf("\nThe adjustment cannot be solved - equations are singular\n");
            close_output_files(0,0);
            return DEFAULT_RETURN_STATUS;
        }

        if( output_normal_equations )
        {
            char header[30];
            sprintf(header,"solution_vector_%d",iterations);
            print_json_start(lst,header);
            lsq_print_solution_vector_json( lst, 0, 0 );
            print_json_end(lst,header);
        }

        /* Find the maximum adjustment and document the iterations */

        max_station_adjustment( convergence_tol, &maxstn, &maxadj, &nstnadj );
        if( maxstn && show_iterations )
        {

            xprintf("   Maximum adjustment is %.4lf at station %s\n",
                    maxadj,station_code(maxstn));
            xprintf("   %d station adjustments exceed convergence criteria\n",
                    nstnadj );
        }

        if( maxstn && show_iterations )
        {
            print_iteration_update( iterations, maxadj, maxstn, nstnadj );
        }

        /* Update station coordinates and parameters, but do not
           get parameter covariance matrix (don't want to invert the
           matrix yet */

        update_station_coords();
        update_params( 0 );
        update_rftrans_prms_list( 0 );

        /* Check whether we have converged or run out of iterations */

        if( iterations >= min_iterations && maxadj < convergence_tol ) break;

        if( iterations >= max_iterations  )
        {
            sprintf(errmess,"The adjustment has not converged after %d iterations",
                    (int)iterations);
            handle_error(WARNING_ERROR,errmess,NO_MESSAGE);
            break;
        }

        if( maxadj > max_adjustment )
        {
            sprintf(errmess,"Adjustment %.4lf at station %s greater than allowable maximum",
                    maxadj, station_code(maxstn) );
            handle_error( WARNING_ERROR, errmess, NO_MESSAGE );
            break;
        }
    }

    /* Grab the least squares statistics */

    lsq_get_stats( &nobs, &nprm, &nschp, &dof, &ssr, &seu );

    if( program_mode != PREANALYSIS ) xprint_ls_summary();

    /* Dump the choleski decomposition if required */

    if( output_decomposition && dump )
    {
        dump_choleski_decomposition( dump );
    }

    /* Zero the b vector... we have already applied any updates, so we
       want to avoid applying them again when we calculate residuals */

    lsq_zero_b_vector();

    /* Expand the matrix to a full matrix if required */

    if( do_accuracy_tests || 
            output_covariance ||
            output_covariance_json ||
            output_sinex ||
            output_all_covariances || 
            (dump && output_full_covariance) )
    {
        expand_bltmatrix_to_full(lsq_normal_matrix());
    }

    /* Dump the covariance matrix if required */

    if( dump && output_full_covariance )
    {
        dump_covariance_matrix( dump );
    }

    /* Invert the normal equations and calculate the residuals */

    xprintf("\n\nInverting the normal equations\n");
    lsq_invert_normal_equations();

    /* Now get parameter covariances */

    update_params( 1 );
    update_rftrans_prms_list( 1 );

    xprintf("Calculating observations residuals\n");
    calc_residuals();

    /* Report the results */

    xprintf("\n\nWriting the results...\n");

    if( output_ls_summary ) print_solution_summary( lst );

    if( output_station_coordinates )
    {
        xprintf("   Listing coordinates\n");
        print_adjusted_coordinates( lst );
    }

    if( output_floated_stations )
    {
        print_floated_stations( lst );
    }

    if( output_station_offsets )
    {
        xprintf("   Listing station offsets\n");
        print_station_offsets( lst );
    }

    if( output_reference_frames )
    {
        xprintf("   Listing reference frames\n");
        print_rftrans_list( lst );
    }

    if( output_parameters )
    {
        xprintf("   Listing other parameters\n");
        print_adjusted_parameters( lst );
    }

    if( output_residuals )
    {
        xprintf("   Listing residuals\n");
        print_residuals( lst );
    }


    if( output_error_summary )
    {
        xprintf("   Listing error summary\n");
        print_error_summary( lst );
    }

    if( do_accuracy_tests )
    {
        xprintf("\n\nTesting accuracy specifications\n");
        test_specifications();
    }

    if( output_worst_residuals ) print_worst_residuals( lst );

    /* CSV outputs */

    if( output_csv_stations || output_csv_allfiles )
    {
        xprintf("   Writing station csv file\n");
        write_station_csv();
    }

    if( output_csv_obs || output_csv_allfiles )
    {
        xprintf("   Writing observation csv file\n");
        write_observation_csv();
    }

    if( output_csv_metadata || output_csv_allfiles )
    {
        xprintf("   Writing metadata csv file\n");
        write_metadata_csv();
    }

    /* Update the station file */

    if( output_coordinate_file ) update_station_file();

    if( dump )
    {
        dump_binary_data( dump );
        close_binary_file( dump );
    }

    /* Drop allocated bits */

    if( deformation ) delete_deformation( deformation );

    /* Output covariance products if required */

    if( output_covariance ) print_coord_covariance();
    if( output_covariance_json ) print_coord_covariance_json();
    if( output_solution_json ) print_solution_json_file();
    if( output_sinex ) print_coord_sinex();

    /* If using debug version of memory allocator then list outstanding
       allocations */
    list_memory_allocations( lst );

    close_output_files( 0, 0 );
    return DEFAULT_RETURN_STATUS;
}


static int read_parameters( int argc, char *argv[] )
{
    char *arg;
    int sts;
    char *cfg_file;
    char *cmd_file;

    sts = OK;
    cfg_file = NULL;
    cmd_file = NULL;

    output_noruntime = 0;

    for( argc--, argv++; sts==OK && argc; argc--, argv++ )
    {
        arg = argv[0];
        if( arg[0] == '-' || arg[0] == '/' )
        {
            switch( arg[1] )
            {
            case 'c':
            case 'C': if( arg[2] )
                {
                    cfg_file = arg+2;
                }
                else if( argc )
                {
                    cfg_file = *++argv;
                    argc--;
                }
                else
                {
                    xprintf("\nConfiguration file name missing after -c switch\n");
                    sts = MISSING_DATA;
                }
                break;

                /* Option for testing so that output doesn't contain run time specific info */
            case 'q':
            case 'Q':
                output_noruntime = 1;
                break;

            default: xprintf("\nInvalid switch %c on command line\n",arg[1]);
                sts = INVALID_DATA;
                break;

            }
        }
        else if( !cmd_file )
        {
            cmd_file = arg;
        }
        else
        {
            xprintf("\nCommand line option %s is not understood\n",arg);
            sts = INVALID_DATA;
        }
    }

    if( !cmd_file )
    {
        xprintf("\nCommand file must be specified on the command line\n");
        sts = MISSING_DATA;
    }

    if( sts != OK ) return sts;

    set_snap_command_file( cmd_file );

    if( cfg_file )
    {
        const char *cf;
        cf = find_configuration_file( cfg_file );
        if( cf )
        {
            set_snap_config_file( copy_string( cf ));
        }
    }

    return sts;
}

static void print_help( void )
{
    xprintf("\n\nTo run %s you must specify a command file to use\n",PROGRAM);
    xprintf("e.g\n          %s  myjob[.cmd]\n\n",PROGRAM);
}

static void print_command_file( void )
{
    FILE *cmd;
    char inrec[256];
    cmd = fopen( command_file, "r" );
    if( !cmd ) return;
    fprintf(lst,"\nThe command file %s contains:\n", command_file+path_len(command_file,0));
    while( fgets(inrec,256,cmd)) fprintf(lst,"     %s",inrec);
    fprintf(lst,"\n");
    fclose( cmd );

    if( !config_file )  return;

    cmd = fopen( config_file, "r" );
    if( !cmd ) return;
    fprintf(lst,"\nAdditional configuration commands were read from %s\n",
            config_file);
    while( fgets(inrec,256,cmd)) fprintf(lst,"     %s",inrec);
    fprintf(lst,"\n");
    fclose( cmd );

}


static void write_metadata_csv()
{
    char buffer[128];
    output_csv *csv = open_output_csv("metadata");
    if( ! csv ) return;
    write_csv_header(csv,"code");
    write_csv_header(csv,"value");
    write_csv_header(csv,"comment");
    end_output_csv_record(csv);

    write_csv_string(csv,"SNAPVER");
    write_csv_string(csv,version_number());
    write_csv_string(csv,"SNAP version");
    end_output_csv_record(csv);

    write_csv_string(csv,"RUNTIME");
    write_csv_string(csv,run_time);
    write_csv_string(csv,"Run time");
    end_output_csv_record(csv);

    write_csv_string(csv,"TITLE");
    write_csv_string(csv,job_title);
    write_csv_string(csv,"Job title");
    end_output_csv_record(csv);

    write_csv_string(csv,"CRDSYS");
    write_csv_string(csv,net->crdsys->code);
    write_csv_string(csv,net->crdsys->name);
    end_output_csv_record(csv);

    if( has_deformation_model(net->crdsys) && deformation_model_epoch(net->crdsys) > 0 )
    {
        write_csv_string(csv,"CRDSYSEPOCH");
        write_csv_double(csv,deformation_model_epoch(net->crdsys),1);
        write_csv_string(csv,"Coordinate system epoch");
        end_output_csv_record(csv);
    }

    write_csv_string(csv,"NOBS");
    write_csv_int(csv,nobs+nschp);
    write_csv_string(csv,"Number of observations");
    end_output_csv_record(csv);

    write_csv_string(csv,"NPRM");
    write_csv_int(csv,nprm);
    write_csv_string(csv,"Number of parameters");
    end_output_csv_record(csv);

    write_csv_string(csv,"NIMP");
    write_csv_int(csv,nschp);
    write_csv_string(csv,"Number of implicit parameters");
    end_output_csv_record(csv);

    write_csv_string(csv,"NCON");
    write_csv_int(csv,ncon);
    write_csv_string(csv,"Number of arbitrary constraints");
    end_output_csv_record(csv);

    write_csv_string(csv,"NDOF");
    write_csv_int(csv,dof);
    write_csv_string(csv,"Degrees of freedom");
    end_output_csv_record(csv);

    write_csv_string(csv,"SSR");
    write_csv_double(csv,ssr,-1);
    write_csv_string(csv,"Sum of squared residuals");
    end_output_csv_record(csv);

    write_csv_string(csv,"SEU");
    write_csv_double(csv,seu,5);
    write_csv_string(csv,"Standard error of unit weight");
    end_output_csv_record(csv);

    write_csv_string(csv,"CONVERGED");
    write_csv_string(csv,converged ? "Y" : "N");
    write_csv_string(csv,"Adjustment met convergence criteria");
    end_output_csv_record(csv);

    write_csv_string(csv,"ERRTYPE");
    write_csv_string(csv,apriori ? "apriori" : "aposteriori");
    write_csv_string(csv,"Errors presented as apriori or aposteriori");
    end_output_csv_record(csv);

    if( got_vector_data() )
    {
        buffer[0] = buffer[1] = 0;
        if( output_csv_veccomp ) strcat(buffer,"-components");
        if( output_csv_vecsum ) strcat(buffer,"-summary");
        if( output_csv_vecinline ) strcat(buffer,"-inline");
        if( output_csv_correlations ) strcat(buffer,"-correlations");
        write_csv_string(csv,"VECFORMAT");
        write_csv_string(csv,buffer+1);
        write_csv_string(csv,"Vector format");
        end_output_csv_record(csv);

        if( output_csv_veccomp )
        {
            write_csv_string(csv,"VECERRTYPE");
            write_csv_string(csv,output_csv_vecenu ? "ENU" : "XYZ");
            write_csv_string(csv,"Vector errors/residual components");
            end_output_csv_record(csv);
        }
    }
    close_output_csv( csv );
}

static void update_station_file()
{
    int nch;
    char *nsfname;

    nch = path_len( station_filespec, 1 );
    nsfname = (char *) check_malloc( nch + strlen( NEWSTNFILE_EXT ) + 1);
    memcpy( nsfname, station_filespec, nch );
    strcpy( nsfname+nch, NEWSTNFILE_EXT );

    if( write_station_file( nsfname, PROGRAM, version_number(), run_time,
                            coord_precision, output_rejected_coordinates ) == OK )
    {
        xprintf("\nNew station coordinates have been written to %s\n",nsfname);
    }
    else
    {
        handle_error( FILE_OPEN_ERROR, "Unable to create updated station coordinate file",
                      nsfname);
    }

    check_free( nsfname );
}



BINARY_FILE *open_dump_file( void )
{
    int nch;
    char *bfn;
    BINARY_FILE *b;

    nch = strlen( root_name ) + strlen( BINFILE_EXT ) + 1;
    bfn = (char *) check_malloc( nch );
    strcpy( bfn, root_name );
    strcat( bfn, BINFILE_EXT );

    b = create_binary_file( bfn, BINFILE_SIGNATURE );
    if( !b )
    {
        handle_error( FILE_OPEN_ERROR, "Unable to open binary file", bfn );
    }

    check_free( bfn );
    return b;
}


void dump_covariance_matrix( BINARY_FILE *b )
{
    int i;
    int nprm;
    bltmatrix *blt = lsq_get_covariance_matrix();
    if( ! blt ) return;
    create_section( b, "FULL_COVARIANCE");
    nprm = blt->nrow;
    fwrite(&nprm,sizeof(nprm),1,b->f );
    xprintf("\nDumping full covariance matrix to binary file..\n");
    init_progress_meter( nprm );
    for( i = 0; i < nprm; i++ )
    {
        double *row;
        update_progress_meter( i );
        row = blt_get_row_data(blt,i);
        if( row ) fwrite( row, sizeof(double), i+1, b->f );
    }
    end_progress_meter();
    end_section(b);
}

void dump_choleski_decomposition( BINARY_FILE *b )
{
    bltmatrix *blt = lsq_get_decomposition();
    if( blt == NULL ) return;
    create_section( b, "CHOLESKI_DECOMPOSITION" );
    dump_bltmatrix(blt,b->f);
    end_section(b);
}


void dump_binary_data( BINARY_FILE *b )
{

    xprintf("\nCreating a binary dump file...\n");

    dump_snap_globals(b);
    dump_stations(b);
    if( program_mode != DATA_CHECK )
    {
        dump_station_covariances(b);
        if( output_relative_covariances )
            dump_relative_covariances(b,output_all_covariances);
    }
    dump_filenames( b );
    dump_obs_classes( b );
    dump_rftransformations( b );
    dump_parameters( b );
}


void summarize_data_modifications( FILE *lst )
{
    int first, id, class_id;
    first = 1;

    for( id = 0; id<NOBSTYPE; id++ )
    {
        if( obs_usage[id] || obs_errfct[id] != 1.0 )
        {
            if( first )
            {
                fprintf(lst,"\n\nModifications for different data types:\n");
                first = 0;
            }
            fprintf(lst,"   Observations of %s data",datatype[id].name);

            if( obs_usage[id] & IGNORE_OBS_BIT )
            {
                fprintf(lst," are ignored");
            }
            else
            {
                if( obs_usage[id] & REJECT_OBS_BIT )
                {
                    fprintf(lst," are rejected");
                }
                if( obs_errfct[id] != 1.0 )
                {
                    fprintf(lst,", errors scaled by %.2lf",obs_errfct[id]);
                }
            }
            fprintf(lst,"\n");
        }
    }

    for( class_id = 0; class_id++ < classification_count( &obs_classes);  )
    {
        first = 1;
        for( id = 0; id<class_value_count( &obs_classes, class_id ); id++)
        {
            double errfct;
            unsigned char usage;

            errfct = get_class_errfct( &obs_classes, class_id, id );
            usage = get_class_usage( &obs_classes, class_id, id );
            if( usage || errfct != 1.0 )
            {
                if( first )
                {
                    fprintf(lst,"\n\nModifications based on %s classification\n",
                            classification_name( &obs_classes, class_id ) );
                    first = 0;
                }
                fprintf(lst,"   Observations classified as %s",
                        class_value_name( &obs_classes, class_id, id ) );

                if( usage & IGNORE_OBS_BIT )
                {
                    fprintf(lst," are ignored");
                }
                else
                {
                    if( usage & REJECT_OBS_BIT )
                    {
                        fprintf(lst," are rejected");
                    }
                    if( errfct != 1.0 )
                    {
                        fprintf(lst,", errors scaled by %.2lf",errfct);
                    }
                }
                fprintf(lst,"\n");
            }
        }
    }
}
