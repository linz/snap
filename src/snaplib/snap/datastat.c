#include "snapconfig.h"
/* Module for calculating data statistics */

/*
   $Log: datastat.c,v $
   Revision 1.1  1996/02/23 16:59:03  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include "util/probfunc.h"
#include "snap/snapglob.h"

#include "snap/datastat.h"

static double mde_value_used = 0.0;
static double mde_value_unused = 0.0;

void set_mde_level( double sig, double power )
{
    sig = (100-sig)/200;
    power = (100-power)/100;
    if( apriori )
    {
        mde_value_used = inv_normal_distn( sig ) + inv_normal_distn( power );
        mde_value_unused = mde_value_used;
    }
    else
    {
        mde_value_used = inv_tau_distn( sig, dof ) + inv_tau_distn( power, dof );
        mde_value_unused = inv_students_t_distn( sig, dof )
                           + inv_students_t_distn( power, dof );
    }
}

double calculate_mde( double sobs, double sres )
{
    if( sres > sobs )     /* Observation is not used */
    {
        return  mde_value_unused * sres;
    }
    else if( sres > 1.0e-10 )
    {
        return  mde_value_used * sobs*sobs / sres;
    }
    else return 0.0;
}

double residual_significance( double sres, int used, int rank )
{
    double prob;
    if( apriori )
    {
        sres *= sres * rank;
        prob = chi2_distn( sres, rank );
    }
    else if( !used )
    {
        sres *= sres;
        prob = f_distn( sres, rank, dof );
    }
    else
    {
        sres *= sres;
        if( dof > rank && dof > rank * sres )
        {
            sres = (dof-rank) * sres / ( dof - rank * sres );
            prob = f_distn( sres, rank, dof-rank );
        }
        else
        {
            prob = -1;
        }
    }

    if( prob >= 0.0 )
    {
        prob = 100 - prob * 100;
    }

    return prob;
}

double prob_of_maximum( double prob, int rank )
{
    if( prob < 1.0 && nobs > 0 ) prob = 1.0 - exp( (log(1.0 - prob)*rank) / nobs );
    else prob = 0.0;
    return prob;
}

