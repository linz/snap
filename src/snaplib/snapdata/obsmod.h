#ifndef _OBSMOD_HPP
#define _OBSMOD_HPP

#ifndef _READCFG_H
#include "util/readcfg.h"
#endif

#ifndef _NETWORK_H
#include "network/network.h"
#endif

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

#define OBS_MOD_IGNORE   1
#define OBS_MOD_REJECT   2
#define OBS_MOD_REWEIGHT 4
#define OBS_MOD_REWEIGHT_SET 8

/* Note: network and classifications are not held by the obs_modifications object
 * so must remain valid for as long as the obs_modifications are used.
 */

void *new_obs_modifications( network *nw, classifications *obs_classes );
void delete_obs_modifications( void *obsmod );
void set_obs_modifications_network( void *obsmod, network *nw );

/* Crude fix to avoid snapdata dependency on snap.  Would be nicer with environment pointer.
 * For C++ rewrite! */

typedef int (*fileid_func)(char *filename, char *refpath);
void set_obs_modifications_file_func( void *obsmod, fileid_func idfunc );

/* Add modifications based on a string specifying multiple criteria */
int add_obs_modifications( CFG_FILE *cfg, void *obsmod, char *modifications, int action, double err_factor );

/* Add modifications criteria based on a classification name and list of values */
int add_obs_modifications_classification( CFG_FILE *cfg, void *obsmod, char *classification, char *value, int action, double err_factor, int missing_error );

/* Add modifications for data file error factor */
int add_obs_modifications_datafile_factor( CFG_FILE *cfg, void *obsmod, int fileid, char *filename, double factor );

/* Determine the observation stations based upon the criteria.  Returns a flag specifying 
 * ignoring, rejecting, and reweighting observations, and the error factor
 * err_factor is an additional observation err scale factor.
 * obsmod may be nullptr
 *
 * Returns the number of observations ignored by the modifications, or 0 if none removed
 */
int apply_obs_modifications( void *obsmod, survdata *sd );

/* Check if a data file is to be completely ignored */
bool obsmod_ignore_datafile( void *obsmod, int file_id );

/* Check if there are errors in observation modification station criteria - 
 * should be run after applying observation modifications (once station recoding
 * has already been done. */
int check_obsmod_station_criteria_codes( void *pobsmod, network *nw );

/* Summarize the observation modifications */
void summarize_obs_modifications( void *obsmod, FILE *lst, const char *prefix );

#endif /* _OBSMOD_HPP */
