#ifndef _SNAPGLOB_H
#define _SNAPGLOB_H

/*
   $Log: snapglob.h,v $
   Revision 1.5  2003/11/24 01:34:14  ccrook
   Updated to allow .snp as command file name

   Revision 1.4  1998/06/15 02:27:08  ccrook
   Modified to handle long integer number of observations

   Revision 1.3  1998/06/03 22:01:18  ccrook
   Modified to support deformation

   Revision 1.2  1996/02/23 16:58:24  CHRIS
   Adding mde_power to global variables

   Revision 1.1  1995/12/22 20:03:49  CHRIS
   Initial revision

   Revision 1.1  1995/12/22 17:48:40  CHRIS
   Initial revision

*/

/* Snap global data - mainly for programs which use the SNAP binary file */

#ifndef _GET_DATA_H
#include "util/get_date.h"  /* For definition of GETDATELEN  */
#endif

#ifndef _DATATYPE_H
#include "snapdata/datatype.h"  /* For definition of NOBSTYPE    */
#endif

#ifndef _DEFORM_H
#include "snap/deform.h"
#endif

#ifndef _CLASSIFY_H
#include "snap/classify.h"
#endif

#ifndef _FILENAMES_H
#include "snap/filenames.h"
#endif

#ifndef _OBSMOD_H
#include "snapdata/obsmod.h"
#endif

#ifdef _SNAPGLOB_C
#define SCOPE
#else
#define SCOPE extern
#endif

/* The program output files */

SCOPE char *command_file;
SCOPE char *config_file;
SCOPE char *root_name;
SCOPE char *cmd_dir;   /* drive/directory of the command file */
SCOPE char *snap_user;  /* User id running SNAP */

/* Program modes */

enum { ADJUST=1, PREANALYSIS, DATA_CHECK, DATA_CONSISTENCY };

/* Basic data relating to the adjustment */

#define JOBTITLELEN 80

SCOPE char job_title[JOBTITLELEN+1];
SCOPE char run_time[GETDATELEN];
SCOPE int dimension;
SCOPE int program_mode;
SCOPE int max_iterations;
SCOPE int min_iterations;
SCOPE double max_adjustment;
SCOPE double convergence_tol;
SCOPE long nobs, nschp, ncon, dof;
SCOPE int nprm;
SCOPE double ssr, seu;
SCOPE int iterations, converged;
SCOPE double last_iteration_max_adjustment;

/* Other miscellaneous data */

SCOPE int have_obs_ids;
SCOPE unsigned char obs_usage[ NOBSTYPE ];
SCOPE double   obs_errfct[ NOBSTYPE ];
SCOPE long     obstypecount[ NOBSTYPE ];
SCOPE int  maxworst;
SCOPE unsigned char use_distance_ratios;
SCOPE int ignore_deformation; /* Ignore the coordinate system deformation */
SCOPE deformation_model *deformation;
SCOPE classifications obs_classes;
SCOPE void *obs_modifications;

/* General output options */

SCOPE int coord_precision;
SCOPE int  file_location_frequency;
SCOPE int  stn_name_width;
SCOPE int  obs_precision[ NOBSTYPE ];

/* Information relating to statistics from the program */

/* errconflim is 0 or 1 depending on whether coordinate errors are presented as
 * confidence limits or multiples of standard error. errconfval is either a multiple
 * of standard errors or a percentage confidence */

SCOPE char apriori;
SCOPE char errconflim;
SCOPE double errconfval;
SCOPE double flag_level[2];
SCOPE char taumax[2];
SCOPE double mde_power;
SCOPE double redundancy_flag_level;

void init_snap_globals();
void set_snap_command_file( char *cmd_file );
void set_snap_config_file( char *cfg_file );
void *snap_obs_modifications( bool create );

#undef SCOPE

#endif
