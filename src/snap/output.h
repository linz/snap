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

#include <string>
#include <string_view>

#include "util/errdef.h"
#include "util/readcfg.h"
#include "util/writecsv.h"

#define PLURAL(d) ( ((d)>1) ? "s" : "" )

SCOPE bool output_command_file;
SCOPE bool output_input_data;
SCOPE bool output_stn_recode;
SCOPE bool output_file_summary;
SCOPE bool output_problem_definition;
SCOPE bool output_observation_equations;
SCOPE bool output_normal_equations;
SCOPE bool output_deformation;
SCOPE bool output_station_adjustments;
SCOPE bool output_iteration_summary;
SCOPE bool output_ls_summary;
SCOPE bool output_residuals;
SCOPE bool output_file_locations;
SCOPE bool output_distance_ratio_scales;
SCOPE bool output_error_summary;
SCOPE bool output_worst_residuals;
SCOPE bool output_station_coordinates;
SCOPE bool output_station_offsets;
SCOPE bool output_floated_stations;
SCOPE bool output_sort_by_type;
SCOPE bool output_rejected_stations;
SCOPE bool output_rejected_coordinates;
SCOPE bool output_parameters;
SCOPE bool output_reference_frames;
SCOPE bool output_reffrm_topo;
SCOPE bool output_reffrm_geo;
SCOPE bool output_reffrm_iers;
SCOPE bool output_form_feeds;
SCOPE bool output_coordinate_file;
SCOPE bool output_binary_file;
SCOPE bool output_decomposition;
SCOPE bool output_relative_covariances;
SCOPE bool output_all_covariances;
SCOPE bool output_sorted_stations;
SCOPE bool output_xyz_vector_residuals;
SCOPE bool output_notes;
SCOPE bool output_covariance;
SCOPE bool output_covariance_json;
SCOPE bool output_solution_json;
SCOPE bool output_sinex;
SCOPE bool output_full_covariance;
SCOPE bool output_noruntime;
SCOPE bool output_debug_reordering;

SCOPE bool output_csv_shape;
SCOPE bool output_csv_veccomp;
SCOPE bool output_csv_vecsum;
SCOPE bool output_csv_vecinline;
SCOPE bool output_csv_vecenu;
SCOPE bool output_csv_correlations;
SCOPE bool output_csv_stations;
SCOPE bool output_csv_filelist;
SCOPE bool output_csv_metadata;
SCOPE bool output_csv_allfiles;
SCOPE bool output_csv_obs;
SCOPE bool output_csv_tab;

#define LIST_OPTIONS 0
#define CSV_OPTIONS 1

#ifndef OUTPUT_C

SCOPE std::string lst_name;
SCOPE std::string err_name;
SCOPE FILE *lst;
SCOPE FILE *err;

#else
std::string lst_name;
std::string err_name;
FILE *lst = 0;
FILE *err = 0;

#endif

#define REJECTED_OBS_FLAG   '*'
#define REJECTED_STN_FLAG   '#'
#define LOW_REDUNDANCY_FLAG '@'
#define FLAG1 "?"
#define FLAG2 "???"

int read_output_options( CFG_FILE *cfg, std::string_view string, void *value, int len, int code );

int open_output_files( );
void close_output_files( error_message mess1, error_message mess2 );
void init_output_options( void );
void eliminate_inconsistent_outputs( void );
void print_report_header( FILE *out );
void print_section_header( FILE *out, std::string_view heading );
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

void print_json_start( FILE *out, std::string_view name );
void print_json_end( FILE *out, std::string_view name );
void print_json_params( FILE *lst, int nprefix );
void print_solution_json_file();

std::unique_ptr<output_csv> open_snap_output_csv( std::string_view type );

int add_requested_covariance_connections();
void delete_requested_covariance_connections();

#undef SCOPE


#endif
