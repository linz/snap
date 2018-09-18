#ifndef _OBSPARAM_H
#define _OBSPARAM_H

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

/* Record observation parameters.  nprm is number of parameters.  description is
 * name of observation parameters 
 */

void init_observation_parameters();
void delete_observation_parameters();

void add_survdata_observation_parameters( survdata *sd, int nprm, const char **descriptions );
int get_survdata_obs_param_rowno( survdata *sd, int prmno, double *value );
void flag_obsparam_used( survdata *sd );

int get_obs_param_rowno( int prmid, double *value );
int get_obs_param_used( int prmid );
int get_obs_param_count();
double get_obs_param_value( int prmid );
double get_obs_param_covar( int prmid );
const char *get_obs_param_name( int prmid );
void update_obs_param_value( int prmid, double value, double covar );
int assign_obs_param_to_stations( int *pnstnobs );
void set_obs_prm_row_number( int nxtprm, int endobsprm );
int find_obsparam_row( int row, char *name, int nlen );

#endif
