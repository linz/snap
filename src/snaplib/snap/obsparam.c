#include "snapconfig.h"

#include <string.h>

#include "snap/snapglob.h"
#include "snap/stnadj.h"
#include "snap/bindata.h"
#include "snap/obsparam.h"
#include "util/chkalloc.h"
#include "util/errdef.h"


typedef struct obs_param_s
{
    int obsid;       /* Id of first obs of referencing observation set */
    int rowno;       /* Row number in obs equations */
    int used;        /* Flag that the parameter is used */
    double value;    /* Calculated value of parameter */
    double covar;    /* Error of calculated value */
    char *prmname;   /* Name of the parameter */
    struct obs_param_s *next;
    struct obs_param_s *prev;
} obs_param;

static int n_obs_param=0;

static obs_param *first_obs_param=0;
static obs_param *last_obs_param=0;
static obs_param **obs_param_index;

static void delete_obs_param_index();
static void create_obs_param_index();
static obs_param *get_obs_param( int oprmid );

void add_survdata_observation_parameters( survdata *sd, int nprm, const char **descriptions )
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
    sd->prmid = n_obs_param+1;
    if( obs_param_index ) delete_obs_param_index();
    int obsid=get_trgtdata(sd,0)->obsid;
    for( int iprm = 0; iprm < nprm; iprm++ )
    {
        char prmname[80];
        sprintf(prmname,"Obs set %d %.40s",obsid,descriptions[iprm]);
        obs_param *oprm=(obs_param *) check_malloc( sizeof(obs_param) + strlen(prmname) + 1 );
        oprm->prmname=((char *)(void *)oprm)+sizeof(obs_param);
        strcpy(oprm->prmname,prmname);
        oprm->obsid=obsid;
        oprm->rowno=0;
        oprm->used=0;
        oprm->value=0.0;
        oprm->covar=0.0;
        oprm->next=0;
        oprm->prev=last_obs_param;
        if( ! first_obs_param )
        {
            first_obs_param=last_obs_param=oprm;
        }
        else
        {
            last_obs_param->next=oprm;
            last_obs_param=oprm;
        }
        n_obs_param++;
    }
}

void flag_obsparam_used( survdata *sd )
{
    int nprm=sd->nprms;
    if( ! nprm ) return;
    int obsid = get_trgtdata(sd,0)->obsid;
    for( obs_param *oprm=last_obs_param; oprm; oprm=oprm->prev )
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
    delete_obs_param_index();
    while( first_obs_param )
    {
        obs_param *oprm=first_obs_param;
        first_obs_param=oprm->next;
        check_free(oprm);
    }
    last_obs_param=0;
}

void init_observation_parameters()
{
    delete_observation_parameters();
}

static void delete_obs_param_index()
{
    if( obs_param_index ) check_free(obs_param_index);
    obs_param_index=0;
}

static void create_obs_param_index()
{
    int noprm;
    if( obs_param_index )
    {
        delete_obs_param_index();
    }
    if( n_obs_param > 0 )
    {
        obs_param_index=(obs_param **) check_malloc( (n_obs_param+1) * sizeof(obs_param *));
        for ( noprm=0; noprm <= n_obs_param; noprm++ ) obs_param_index[noprm]=0;
        noprm=0;
        for( obs_param *oprm=first_obs_param; oprm; oprm=oprm->next )
        {
            noprm++;
            if( nprm > n_obs_param )
            {
            }
            obs_param_index[noprm]=oprm;
        }
    }
}

int get_obs_param_count()
{
    return n_obs_param;
}

static obs_param *get_obs_param( int oprmid )
{
    if( oprmid <= 0 || oprmid > n_obs_param ) return 0;
    if( ! obs_param_index ) create_obs_param_index();
    return obs_param_index[oprmid];
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
    if( oprm ) return oprm->prmname;
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
    if( n_obs_param <= 0 ) return nobsprm;
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
    if( n_obs_param <= 0 ) return;
    int laststn=0;
    int *rownoptr=0;
    int *strn=0;
    int *stno=0;
    int nstn=number_of_stations(net);
    for( obs_param *oprm=first_obs_param; oprm; oprm=oprm->next )
    {
        if( ! oprm->used ) continue;
        int istn=oprm->rowno;
        if( istn <= 0 || istn > nstn )
        {
            laststn=0;
            rownoptr=&nxtprm;
        }
        else 
        {
            if( ! strn )
            {
                strn=(int *) check_malloc(sizeof(int)*(nstn+1)*2);
                stno=strn+nstn+1;
                strn[0]=stno[0]=0;
                for( int ist=0; ist++<nstn; )
                {
                    stn_adjustment *sa=stnadj(stnptr(ist));
                    int rn = sa->hrowno ? sa->hrowno+2 : 0;
                    if( sa->vrowno ) rn=sa->vrowno+1;
                    strn[ist]=rn;
                    stno[ist]=sa->nobsprm;
                }
            }
            rownoptr=strn+istn;
            stno[istn]--;
        }
        if( ! *rownoptr )
        {
            handle_error(FATAL_ERROR,"Invalid row number setting obs prm",
                    "set_obs_prm_row_number");
            return;
        }
        oprm->rowno=(*rownoptr)++;
    }
    if( nxtprm-1 != endobsprm )
    {
        handle_error(FATAL_ERROR,"Mismatch in number of obs parameters set",
                "set_obs_prm_row_number");
    }

    if( strn )
    {
        for( int ist = 0; ist++ < nstn; )
        {
            if( stno[ist] != 0 )
            {
                handle_error(FATAL_ERROR,"Mismatch in number of station obs parameters set",
                    "set_obs_prm_row_number");
            }
        }
        check_free(strn);
    }
}

int find_obsparam_row( int row, char *name, int nlen )
{
    for( obs_param *oprm=first_obs_param; oprm; oprm=oprm->next )
    {
        if( oprm->rowno == row )
        {
            strncpy( name, oprm->prmname, nlen-1 );
            name[nlen-1] = 0;
            return 1;
        }
    }
    return 0;
}

