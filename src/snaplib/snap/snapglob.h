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

#include <optional>
#include <string>

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

/// The command file a program is run with, and the names derived from it.
class CommandFile
{
public:
    /// Locates the command file, trying the default command file extensions if
    /// the name as given does not exist, and derives its directory and root name.
    explicit CommandFile( const std::string &name );

    const std::string path; ///< The command file name, including any extension that was added
    const std::string dir;  ///< The drive/directory of the command file
    const std::string root; ///< The command file name without its extension, used as the base of the output file names

private:
    static std::string _locate( const std::string &name );
};

/* The program output files */

extern std::optional<CommandFile> command_file;
extern std::optional<std::string> config_file;
extern std::optional<std::string> snap_user;  /* User id running SNAP */

#ifdef _SNAPGLOB_C
#define SCOPE
#else
#define SCOPE extern
#endif

/* Program modes */

enum { ADJUST=1, PREANALYSIS, DATA_CHECK, DATA_CONSISTENCY };

/* Basic data relating to the adjustment */

#define JOBTITLELEN 80

SCOPE char job_title[JOBTITLELEN+1];
SCOPE std::string run_time;
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
void set_snap_command_file( const std::string &cmd_file );
void set_snap_config_file( const std::string &cfg_file );
void *snap_obs_modifications( bool create );

#undef SCOPE

#endif
