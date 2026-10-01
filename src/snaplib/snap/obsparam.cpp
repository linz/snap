#include "snapconfig.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <boost/numeric/conversion/cast.hpp>
using boost::numeric_cast;

#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/bindata.h"
#include "snap/obsparam.h"
#include "util/errdef.h"


/// A calculated parameter of an observation set
class obs_param
{
public:
    /// Creates an unused parameter with zero value, referenced by the observation set with id \p obsid
    obs_param( int obsid, std::string prmname )
        : obsid( obsid ), prmname_( std::move( prmname ) ) {}

    /// Returns the name of the parameter
    const std::string &prmname() const { return prmname_; }

    const int obsid;      ///< Id of first obs of referencing observation set
    int rowno = 0;        ///< Row number in obs equations
    int used = 0;         ///< Flag that the parameter is used
    double value = 0.0;   ///< Calculated value of parameter
    double covar = 0.0;   ///< Error of calculated value

private:
    std::string prmname_; ///< Name of the parameter
};

/// All observation parameters in order of creation.  Parameter id n is element n-1.
static std::vector<obs_param> obs_params;

static obs_param *get_obs_param( int oprmid );

void add_survdata_observation_parameters( survdata *sd, int nprm, const std::array<std::string_view,3> &descriptions )
{
    if( nprm == 0 ) return;
    if( sd->nprms > 0 )
    {
        handle_error(FATAL_ERROR,
                "Cannot handle multiple add_observation_parameters for same obs set",
                "add_observation_parameters");
        return;
    }
    sd->nprms = nprm;
    sd->prmid = get_obs_param_count()+1;
    const int obsid=get_trgtdata(sd,0)->obsid;
    for( int iprm = 0; iprm < nprm; iprm++ )
    {
        const std::string description( descriptions[iprm].substr( 0, 40 ) );
        obs_params.emplace_back( obsid, "Obs set " + std::to_string(obsid) + " " + description );
    }
}

void flag_obsparam_used( survdata *sd )
{
    int nprm=sd->nprms;
    if( ! nprm ) return;
    const int obsid = get_trgtdata(sd,0)->obsid;
    for( auto oprm=obs_params.rbegin(); oprm != obs_params.rend(); ++oprm )
    {
        if( oprm->obsid == obsid )
        {
            oprm->used=1;
            nprm--;
            if( nprm == 0 ) break;
        }
    }
}

void delete_observation_parameters()
{
    obs_params.clear();
}

void init_observation_parameters()
{
    delete_observation_parameters();
}

int get_obs_param_count()
{
    return numeric_cast<int>( obs_params.size() );
}

/// Returns the parameter with the 1-based id \p oprmid, or null if there is none.
/// The pointer is valid until the next parameter is added.
static obs_param *get_obs_param( int oprmid )
{
    if( oprmid <= 0 || oprmid > get_obs_param_count() ) return nullptr;
    return &obs_params[oprmid-1];
}

double get_obs_param_value( int prmid )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) return oprm->value;
    return 0.0;
}

double get_obs_param_covar( int prmid )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) return oprm->covar;
    return 0.0;
}

const char *get_obs_param_name( int prmid )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) return oprm->prmname().c_str();
    return "";
}

void update_obs_param_value( int prmid, double value, double covar )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) 
    {
        oprm->value=value;
        oprm->covar=covar;
    }
}

int get_obs_param_rowno( int prmid, double *value )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) 
    {
        if( value ) *value = oprm->value;
        return oprm->rowno;
    }
    if( value ) *value=0.0;
    return 0;
}

int get_obs_param_used( int prmid )
{
    obs_param *oprm = get_obs_param(prmid);
    if( oprm ) 
    {
        return oprm->used;
    }
    return 0;
}

int get_survdata_obs_param_rowno( survdata *sd, int prmno, double *value )
{
    int oprmid=sd->prmid+prmno;
    obs_param *oprm = get_obs_param(oprmid);
    if( oprm ) 
    {
        if( value ) *value = oprm->value;
        return oprm->rowno;
    }
    if( value ) *value=0.0;
    return 0;
}

/* First pass at allocating observation parameters to stations to improve bandwidth.
 * This assigns observations parameters to the first adjusted station in the observation
 * (at this stage just setting the count of parameters)
 */

int assign_obs_param_to_stations( int *pnstnobs )
{
    int nstnobs=0;
    int nobsprm=0;
    if( pnstnobs ) *pnstnobs = nstnobs;
    if( get_obs_param_count() <= 0 ) return nobsprm;
    int nstn=number_of_stations(net);
    for( int istn = 0; istn++ < nstn; )
    {
        stnadj(stnptr(istn))->nobsprm=0;
    }
    bindata *b=create_bindata();
    init_get_bindata(0L);
    for(;;)
    {
        if( get_bindata( SURVDATA, b ) != OK ) break;
        survdata *sd = (survdata *) b->data;
        int nprm=sd->nprms;
        int prmid=sd->prmid;
        if( nprm <= 0 || prmid <= 0 ) continue;
        if( ! get_obs_param(prmid)->used ) continue;
        int istno=0;
        int istn=sd->from;
        for( int iobs = -1; iobs < sd->nobs; iobs++ )
        {
            if( iobs >= 0 )
            {
                trgtdata *tgt=get_trgtdata(sd,iobs);
                if( tgt->unused ) continue;
                istn=tgt->to;
            }
            station *st=stnptr(istn);
            if( ! st ) continue;
            stn_adjustment *sa=stnadj(st);
            if( sa->hrowno > 0 || sa->vrowno > 0 )
            {
                sa->nobsprm += nprm;
                nstnobs += nprm;
                istno=istn;
                break;
            }
        }
        nobsprm += nprm;
        int obsid=get_trgtdata(sd,0)->obsid;
        for( int iprm=0; iprm < nprm; iprm++ )
        {
            obs_param *oprm=get_obs_param(prmid+iprm);
            if( ! oprm || oprm->obsid != obsid )
            {
                handle_error(FATAL_ERROR,"Mismatch in obs param count",
                        "assign_obs_param_to_stations");
                return 0;
            }
            oprm->rowno=istno;
        }
    }
    delete_bindata( b );
    if( pnstnobs ) *pnstnobs = nstnobs;
    return nobsprm;
}

/* Second pass at allocating observation parameters to stations to improve bandwidth.
 * This sets the parameter row number for observation parameters.  The row number is
 * set based on the first station adjusted nobsprm value, which is assumed to have 
 * been set to the row number of the first obs prm for the station.  For obs
 * not allocated to stations takes the number from the nxtprm value.
 */

void set_obs_prm_row_number( int nxtprm, int endobsprm )
{
    if( get_obs_param_count() <= 0 ) return;
    int *rownoptr=nullptr;
    std::vector<int> strn;
    std::vector<int> stno;
    int nstn=number_of_stations(net);
    for( obs_param &oprm : obs_params )
    {
        if( ! oprm.used ) continue;
        int istn=oprm.rowno;
        if( istn <= 0 || istn > nstn )
        {
            rownoptr=&nxtprm;
        }
        else 
        {
            if( strn.empty() )
            {
                strn.assign( nstn+1, 0 );
                stno.assign( nstn+1, 0 );
                for( int ist=1; ist<=nstn; ist++ )
                {
                    const stn_adjustment *sa=stnadj(stnptr(ist));
                    int rn = sa->hrowno ? sa->hrowno+2 : 0;
                    if( sa->vrowno ) rn=sa->vrowno+1;
                    strn[ist]=rn;
                    stno[ist]=sa->nobsprm;
                }
            }
            rownoptr=&strn[istn];
            stno[istn]--;
        }
        if( ! *rownoptr )
        {
            handle_error(FATAL_ERROR,"Invalid row number setting obs prm",
                    "set_obs_prm_row_number");
            return;
        }
        oprm.rowno=(*rownoptr)++;
    }
    if( nxtprm-1 != endobsprm )
    {
        handle_error(FATAL_ERROR,"Mismatch in number of obs parameters set",
                "set_obs_prm_row_number");
    }

    for( int ist = 1; ist <= nstn && ! stno.empty(); ist++ )
    {
        if( stno[ist] != 0 )
        {
            handle_error(FATAL_ERROR,"Mismatch in number of station obs parameters set",
                "set_obs_prm_row_number");
        }
    }
}

std::optional<std::string> find_obsparam_row( const int row )
{
    const auto oprm = std::find_if( obs_params.begin(), obs_params.end(),
        [row]( const obs_param &candidate ) { return candidate.rowno == row; } );
    if( oprm == obs_params.end() ) return std::nullopt;
    return oprm->prmname();
}

