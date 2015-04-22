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

#ifndef OUTPUT_H_RCSID
#define OUTPUT_H_RCSID "$Id: output.h,v 1.4 2003/11/23 23:05:18 ccrook Exp $"
#endif

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


#define PLURAL(d) ( ((d)>1) ? "s" : "" )

#define MAX_INCOMPATIBLE_MODES 4

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
SCOPE char output_notes;
SCOPE char output_covariance;
SCOPE char output_sinex;
SCOPE char output_full_covariance;
SCOPE char output_runtime;
SCOPE char output_noruntime;

SCOPE char output_csv_shape;
SCOPE char output_csv_veccomp;
SCOPE char output_csv_vecsum;
SCOPE char output_csv_vecinline;
SCOPE char output_csv_vecenu;
SCOPE char output_csv_correlations;
SCOPE char output_csv_stations;
SCOPE char output_csv_metadata;
SCOPE char output_csv_allfiles;
SCOPE char output_csv_obs;
SCOPE char output_csv_tab;

typedef struct
{
    const char *name;
    char *status;
    char dflt;
    char incompatible[MAX_INCOMPATIBLE_MODES];
} output_option;

typedef struct
{
    char *filename;
    FILE *f;
    char tab;
    char first;
    char delim;
    char *delimrep;
    char quote;
    char *quoterep;
    char *newlinerep;
    char charbuf[20];
} output_csv;

#define LIST_OPTIONS 0
#define CSV_OPTIONS 1

#ifndef OUTPUT_C

SCOPE output_option output[];
SCOPE output_option csvopt[];
SCOPE char *lst_name;
SCOPE char *err_name;
SCOPE FILE *lst;
SCOPE FILE *err;

#else
char *lst_name = 0;
char *err_name = 0;
FILE *lst = 0;
FILE *err = 0;

output_option output[] =
{
    {"command_file",&output_command_file,0,{0}},
    {"input_data",&output_input_data,0,{0}},
    {"station_recoding",&output_stn_recode,0,{0}},
    {"file_summary",&output_file_summary,1,{0}},
    {"problem_definition",&output_problem_definition,0,{0}},
    {"observation_equations",&output_observation_equations,0,{0}},
    {"normal_equations",&output_normal_equations,0,{0}},
    {"observation_deformation",&output_deformation,0,{0}},
    {"station_adjustments",&output_station_adjustments,0,{ PREANALYSIS, 0}},
    {"iteration_summary",&output_iteration_summary,1,{ PREANALYSIS, 0}},
    {"solution_summary",&output_ls_summary,1,{ 0 }},
    {"residuals",&output_residuals,1,{ PREANALYSIS, 0}},
    {"worst_residuals",&output_worst_residuals,1,{0}},
    {"error_summary",&output_error_summary,1,{0}},
    {"grouped_data_by_type",&output_sort_by_type,1,{0}},
    {
        "station_coordinates",&output_station_coordinates,1,
        {DATA_CONSISTENCY, 0}
    },
    {
        "floated_stations",&output_floated_stations,1,
        {DATA_CONSISTENCY, DATA_CHECK, 0}
    },
    {"station_offsets",&output_station_offsets,1,{0}},
    {"rejected_stations",&output_rejected_stations,1,{0}},
    {"rejected_station_coordinates",&output_rejected_coordinates,1,{0}},
    {"reference_frames",&output_reference_frames,1,{DATA_CONSISTENCY, 0}},
    {"topocentric_ref_frame",&output_reffrm_topo,0,{0}},
    {"geocentric_ref_frame",&output_reffrm_geo,0,{0}},
    {"iers_ref_frame",&output_reffrm_iers,0,{0}},
    {"parameters",&output_parameters,1,{DATA_CONSISTENCY, 0}},
    {"form_feeds",&output_form_feeds,0,{0}},
    {
        "coordinate_file",&output_coordinate_file,1,
        {DATA_CONSISTENCY,DATA_CHECK,PREANALYSIS,0}
    },
    {"binary_file",&output_binary_file,1,{0}},
    {"decomposition",&output_decomposition,0,{0}},
    {"relative_covariances",&output_relative_covariances,1,{0}},
    {"all_relative_covariances",&output_all_covariances,0,{0}},
    {"full_covariance_matrix",&output_full_covariance,0,{0}},
    {"sort_stations",&output_sorted_stations,0,{0}},
    {"notes",&output_notes,1,{0}},
    {"covariance_matrix_file",&output_covariance,0,{0}},
    {"sinex",&output_sinex,0,{0}},
    {NULL,NULL,0,{0}}
};

output_option csvopt[] =
{
    {"wkt_shape",&output_csv_shape,0,{0}},
    {"vector_components",&output_csv_veccomp,1,{0}},
    {"vector_summary",&output_csv_vecsum,1,{0}},
    {"vectors_inline",&output_csv_vecinline,1,{0}},
    {"enu_residuals",&output_csv_vecenu,1,{0}},
    {"correlations",&output_csv_correlations,0,{0}},
    {"stations",&output_csv_stations,0,{0}},
    {"observations",&output_csv_obs,0,{0}},
    {"metadata",&output_csv_metadata,0,{0}},
    {"all",&output_csv_allfiles,0,{0}},
    {"tab_delimited",&output_csv_tab,0,{0}},
    {NULL,NULL,0,{0}}
};

#endif


#define REJECTED_OBS_FLAG   '*'
#define REJECTED_STN_FLAG   '#'
#define LOW_REDUNDANCY_FLAG '@'
#define FLAG1 "?"
#define FLAG2 "???"


int open_output_files( );
void close_output_files( const char *mess1, const char *mess2 );
void init_output_options( void );
void eliminate_inconsistent_outputs( void );
void print_header( FILE *out );
void print_control_options( FILE *out );
void print_section_heading( FILE *out, const char *heading );
void handle_singularity( int sts );
void print_iteration_header( int iteration );
void print_iteration_update( int iteration, double maxadj,
                             int maxstn, int nstnadj );
void print_problem_summary( FILE *out );
void print_ls_summary( FILE *out );
void xprint_ls_summary();
void print_solution_summary( FILE *out );

void print_json_start( FILE *out, const char *name );
void print_json_end( FILE *out, const char *name );
void print_json_params( FILE *lst, int nprefix );

output_csv *open_output_csv( const char *type);
void close_output_csv( output_csv *csv );
void end_output_csv_record( output_csv *csv );
void write_csv_header( output_csv *csv, const char *fieldname );
void write_csv_string( output_csv *csv, const char *value );
void write_csv_int( output_csv *csv, long value );
void write_csv_double( output_csv *csv, double value, int ndp );
void write_csv_date( output_csv *csv, double date );
void write_csv_null_field( output_csv *csv );

#undef SCOPE


#endif
