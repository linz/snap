#ifndef _OBSPARAM_H
#define _OBSPARAM_H

#include <array>
#include <optional>
#include <string>
#include <string_view>

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

/* Record observation parameters.  nprm is number of parameters.  description is
 * name of observation parameters
 */

void init_observation_parameters();
void delete_observation_parameters();

void add_survdata_observation_parameters( survdata *sd, int nprm, const std::array<std::string_view,3> &descriptions );
int get_survdata_obs_param_rowno( survdata *sd, int prmno, double *value );
void flag_obsparam_used( survdata *sd );

int get_obs_param_rowno( int prmid, double *value );
int get_obs_param_used( int prmid );
int get_obs_param_count();
double get_obs_param_value( int prmid );
double get_obs_param_covar( int prmid );
/// Returns the name of an observation parameter, or an empty string if there is no such parameter.
const std::string &get_obs_param_name( int prmid ///< Observation parameter id
);
void update_obs_param_value( int prmid, double value, double covar );
int assign_obs_param_to_stations( int *pnstnobs );
void set_obs_prm_row_number( int nxtprm, int endobsprm );
/// Returns the name of the observation parameter solved at equation row \p row, if there is one.
std::optional<std::string> find_obsparam_row( int row );

#endif
