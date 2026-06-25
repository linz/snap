#include "snapconfig.h"
/*
   $Log: crdsysr2.c,v $
   Revision 1.3  2004/04/22 02:34:21  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.2  2003/11/28 01:59:25  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)

   Revision 1.1  1995/12/22 16:48:20  CHRIS
   Initial revision

*/

#include <stdio.h>
#include "coordsys/coordsys.h"
#include "util/errdef.h"
#include "util/pi.h"

/* Routines to convert to and from the standard reference frame used to
   define reference frame position, orientation, and scale */

int xyz_to_std( ref_frame *rf, double xyz[3], double date )
{
    int axis[] = { 1, 2, 0, 1 };
    int i;
    double x, y, cs, sn;

    if( rf->func && rf->func->xyz_to_std_func )
    {
        return (*(rf->func->xyz_to_std_func))( rf, xyz, date );
    }

    if( rf->use_rates && rf->calcdate != date ) init_ref_frame( rf, date );

    for( i=0; i<3; i++ )
    {
        cs = rf->csrot[i];
        sn = rf->snrot[i];
        x = xyz[axis[i]];
        y = xyz[axis[i+1]];
        xyz[axis[i]] = x*cs + y*sn;
        xyz[axis[i+1]] = -x*sn + y*cs;
    }

    for( i=0; i<3; i++ )
    {
        xyz[i] = xyz[i] * rf->sclfct + rf->trans[i];
    }
    return OK;
}



int std_to_xyz( ref_frame *rf, double xyz[3], double date )
{
    int axis[] = { 1, 2, 0, 1 };
    int i;
    double x, y, cs, sn;

    if( rf->func && rf->func->std_to_xyz_func )
    {
        return (*(rf->func->std_to_xyz_func))( rf, xyz, date );
    }

    if( rf->use_rates && rf->calcdate != date ) init_ref_frame( rf, date );

    for( i=0; i<3; i++ )
    {
        xyz[i] = (xyz[i] - rf->trans[i]) / rf->sclfct;
    }

    for( i=3; i--;  )
    {
        cs = rf->csrot[i];
        sn = rf->snrot[i];
        x = xyz[axis[i]];
        y = xyz[axis[i+1]];
        xyz[axis[i]] = x*cs - y*sn;
        xyz[axis[i+1]] = x*sn + y*cs;
    }
    return OK;
}


int ref_deformation_at_epoch( ref_frame *rf, double llh[3],
                              double epoch, double denu[3] )
{
    if( ! rf->def ) { denu[0] = denu[1] = denu[2] = 0.0; return OK; }
    return (*(rf->def->calc_denu))(rf, llh[CRD_LON], llh[CRD_LAT], epoch, denu );

}


int apply_ref_deformation_llh( ref_frame *rf, double llh[3], double epochfrom, double epochto )
{
    ref_deformation *def = rf->def;
    if( ! def ) return OK;

    if( def->apply_llh )
    {
        return def->apply_llh(rf, llh, epochfrom, epochto );
    }
    else
    {
        int sts;
        double denu[3] = {0,0,0};
        double denu1[3] = {0,0,0};
        double lat = llh[CRD_LAT];
        double lon = llh[CRD_LON];
        sts = OK;
        if( epochfrom != 0 ) sts = (*(rf->def->calc_denu))(rf, lon, lat, epochfrom, denu1 );
        if( sts == OK && epochto != 0) sts = (*(rf->def->calc_denu))( rf, lon, lat, epochto, denu );
        if( sts != OK ) return sts;
        denu[0] -= denu1[0];
        denu[1] -= denu1[1];
        denu[2] -= denu1[2];
        return rf_apply_enu_deformation_to_llh( rf, llh, denu );
    }
}


int rf_apply_enu_deformation_to_llh( ref_frame *rf, double llh[3], double denu[3] )
{
    int sts = OK;
    if( denu[0] != 0 || denu[1] != 0 )
    {
        double xyz[3], dEdLn, dNdLt;
        xyz[CRD_LAT] = llh[CRD_LAT];
        xyz[CRD_LON] = llh[CRD_LON];
        xyz[CRD_HGT] = 0.0;
        llh_to_xyz( rf->el, xyz, xyz, &dEdLn, &dNdLt );
        /* Need to set status to error of dxdy are 0.0 */
        if( dEdLn == 0.0 || dNdLt == 0.0 )
        {
            sts = INVALID_DATA;
        }
        else
        {
            llh[CRD_LON] += denu[CRD_EAST]/dEdLn;
            llh[CRD_LAT] += denu[CRD_NORTH]/dNdLt;
        }
    }
    if( sts == OK ) llh[CRD_HGT] += denu[CRD_HGT];
    return sts;
}
