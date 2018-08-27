#ifndef _OUTPUT_H
#define _OUTPUT_H

/*
   $Log: output.h,v $
   Revision 1.4  2003/11/23 23:05:18  ccrook
   Updated iteration output to display number of stations exceeding adjustment
   tolerance

   Revision 1.3  2003/05/16 01:20:41  ccrook
   Added option to store all relative covariances in binary file.

   Revision 1.2  1996/01/10 19:47:20  CHRIS
   Added an extra parameter output_full_covariance, and changed command
   string for outputting covariance matrix file to covariance_matrix_file
   instead of just covariance_matrix.

   Revision 1.1  1996/01/03 22:03:56  CHRIS
   Initial revision

*/

/* Output options - each options is defined as a character variable
   which defines whether the corresponding item is to be output.  A
   structure defines a name for each options (used in the control
   file, a default value, and possibly some output modes with which it
   is not compatible. */

#ifdef OUTPUT_C
#define SCOPE
#else
#define SCOPE extern
#endif

#include "util/readcfg.h"
#include "util/writecsv.h"

#define PLURAL(d) ( ((d)>1) ? "s" : "" )

#define MAX_INCOMPATIBLE_MODES 4
#define MAX_FOLLOWING_COMMANDS 4

SCOPE char output_command_file;
SCOPE char output_input_data;
SCOPE char output_stn_recode;
SCOPE char output_file_summary;
SCOPE char output_problem_definition;
SCOPE char output_observation_equations;
SCOPE char output_normal_equations;
SCOPE char output_deformation;
SCOPE char output_station_adjustments;
SCOPE char output_iteration_summary;
SCOPE char output_ls_summary;
SCOPE char output_residuals;
SCOPE char output_file_locations;
SCOPE char output_distance_ratio_scales;
SCOPE char output_error_summary;
SCOPE char output_worst_residuals;
SCOPE char output_station_coordinates;
SCOPE char output_station_offsets;
SCOPE char output_floated_stations;
SCOPE char output_sort_by_type;
SCOPE char output_rejected_stations;
SCOPE char output_rejected_coordinates;
SCOPE char output_parameters;
SCOPE char output_reference_frames;
SCOPE char output_reffrm_topo;
SCOPE char output_reffrm_geo;
SCOPE char output_reffrm_iers;
SCOPE char output_form_feeds;
SCOPE char output_coordinate_file;
SCOPE char output_binary_file;
SCOPE char output_decomposition;
SCOPE char output_relative_covariances;
SCOPE char output_all_covariances;
SCOPE char output_sorted_stations;
SCOPE char output_xyz_vector_residuals;
SCOPE char output_notes;
SCOPE char output_covariance;
SCOPE char output_covariance_json;
SCOPE char output_solution_json;
SCOPE char output_sinex;
SCOPE char output_full_covariance;
SCOPE char output_runtime;
SCOPE char output_noruntime;
SCOPE char output_debug_reordering;

SCOPE char output_csv_shape;
SCOPE char output_csv_veccomp;
SCOPE char output_csv_vecsum;
SCOPE char output_csv_vecinline;
SCOPE char output_csv_vecenu;
SCOPE char output_csv_correlations;
SCOPE char output_csv_stations;
SCOPE char output_csv_filelist;
SCOPE char output_csv_metadata;
SCOPE char output_csv_allfiles;
SCOPE char output_csv_obs;
SCOPE char output_csv_tab;

#define LIST_OPTIONS 0
#define CSV_OPTIONS 1

#ifndef OUTPUT_C

SCOPE char *lst_name;
SCOPE char *err_name;
SCOPE FILE *lst;
SCOPE FILE *err;

#else
char *lst_name = 0;
char *err_name = 0;
FILE *lst = 0;
FILE *err = 0;

#endif

#define REJECTED_OBS_FLAG   '*'
#define REJECTED_STN_FLAG   '#'
#define LOW_REDUNDANCY_FLAG '@'
#define FLAG1 "?"
#define FLAG2 "???"

int read_output_options( CFG_FILE *cfg, char *string, void *value, int len, int code );

int open_output_files( );
void close_output_files( const char *mess1, const char *mess2 );
void init_output_options( void );
void eliminate_inconsistent_outputs( void );
void print_report_header( FILE *out );
void print_section_header( FILE *out, const char *heading );
void print_section_footer( FILE *out );
void print_report_footer( FILE *out );
void print_control_options( FILE *out );
void handle_singularity( int sts );
void print_zero_inverse_warning( FILE *out );
void print_convergence_warning( FILE *out );
void print_iteration_header( int iteration );
void print_iteration_update( int iteration, double maxadj,
                             int maxstn, int nstnadj );
void print_iteration_footer();
void print_problem_summary( FILE *out );
void print_ls_summary( FILE *out );
void xprint_ls_summary();
void print_solution_summary( FILE *out );
void print_bandwidth_reduction( FILE *out );

void print_json_start( FILE *out, const char *name );
void print_json_end( FILE *out, const char *name );
void print_json_params( FILE *lst, int nprefix );
void print_solution_json_file();

output_csv *open_snap_output_csv( const char *type );

int add_requested_covariance_connections();
void delete_requested_covariance_connections();

#undef SCOPE


#endif
