#include "snapconfig.h"
/*
   $Log: netcalcs.c,v $
   Revision 1.2  2001/11/22 00:31:36  ccrook
   Added test for zero length lines in calculation of projection azimuth

   Revision 1.1  1995/12/22 17:28:04  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>

#include "network/network.h"

static char rcsid[]="$Id: netcalcs.c,v 1.2 2001/11/22 00:31:36 ccrook Exp $";

/*
   Procedure for computing observations equations of basic geodetic
   quantities involving one or two stations.  In each case we return
   the observation equations in terms of the East, North, and Up
   coordinates at the station(s) (in a topographic system).

   Generally the equations are generated in the geocentric xyz system
   and then rotated into the XYZ system.

   Currently the gravimetric corrections are assumed to be fixed at a
   station - they are not included in the adjustment.

*/

/* Minimum distance squared on which azimuths are calculated */

#define DIST_TOL 1.0e-5
#define DIST_TOL_2 (DIST_TOL*DIST_TOL)

/* Single station observations - latitude, longitude, orthometric height */
/* And X, Y, Z                                                           */

double calc_lat( station *st, vector3 dst )
{
    if( dst )
    {
        dst[0] = 0.0;
        dst[1] = 1.0/st->dNdLt;
        dst[2] = 0.0;
    }
    return st->ELat;
}

double calc_lon( station *st, vector3 dst )
{
    if( dst )
    {
        dst[0] = 1.0/st->dEdLn;
        dst[1] = 0.0;
        dst[2] = 0.0;
    }
    return st->ELon;
}

double calc_ohgt( station *st, double hgt, vector3 dst )
{
    if( dst )
    {
        dst[0] = dst[1] = 0.0;
        dst[2] = 1.0;
    }
    return st->OHgt+hgt;
}

double calc_ehgt( station *st, double hgt, vector3 dst )
{
    if( dst )
    {
        dst[0] = dst[1] = 0.0;
        dst[2] = 1.0;
    }
    return st->OHgt+st->GUnd+hgt;
}


double calc_x( station *st, vector3 dst )
{
    if( dst )
    {
        dst[0] = 1.0;
        dst[1] = dst[2] = 0.0;
        rotvec( dst, &st->rTopo, dst );
    }
    return st->XYZ[0];
}

double calc_y( station *st, vector3 dst )
{
    if( dst )
    {
        dst[1] = 1.0;
        dst[0] = dst[2] = 0.0;
        rotvec( dst, &st->rTopo, dst );
    }
    return st->XYZ[1];
}

double calc_z( station *st, vector3 dst )
{
    if( dst )
    {
        dst[2] = 1.0;
        dst[0] = dst[1] = 0.0;
        rotvec( dst, &st->rTopo, dst );
    }
    return st->XYZ[2];
}

/* Two station observations - distance, azimuth, zenith distance. These
   all use instrument and target stations and heights.  In each case the
   observations are from the first station to the second.

   The topographic vertical is being used to correct station heights, as
   this allows the distance calculation to be done for horizontal distances,
   by setting the station heights to half way between the stations, and
   sea level distances to half way between the stations. */


static void calc_inst_xyz( station *st, double hgt, vector3 xyz )
{
    vector3 vrt;

    rot_vertical( &st->rTopo, vrt );         /* Get the vertical vector */
    vecadd2( st->XYZ, 1.0, vrt, hgt, xyz );  /* Add to the geocentric coords */
}


static void calc_inst_dif( station *st1, double hgt1, station *st2, double hgt2,
                           vector3 dif )
{
    vector3 inst1, inst2;

    calc_inst_xyz( st1, hgt1, inst1 );
    calc_inst_xyz( st2, hgt2, inst2 );

    vecdif( inst2, inst1, dif );
}



double calc_distance( station *st1, double hgt1, station *st2, double hgt2,
                      vector3 dst1, vector3 dst2 )
{
    double dist;
    vector3 dif;

    calc_inst_dif( st1, hgt1, st2, hgt2, dif );

    dist = veclen( dif );

    if( dst1 )
    {
        if( dist > DIST_TOL )
        {
            scalevec( dif, -1.0/dist );
            rotvec( dif, &st1->rTopo, dst1 );
            scalevec( dif, -1.0 );
            rotvec( dif, &st2->rTopo, dst2 );
        }
        else
        {
            dst1[0] = dst1[1] = dst1[2] = 0.0;
            dst2[0] = dst2[1] = dst2[2] = 0.0;
        }
    }

    return dist;
}

double calc_horizontal_distance( station *st1, station *st2, vector3 dst1, vector3 dst2 )
{
    double dist, h1, h2;
    vector3 dif;

    h1 = st1->OHgt+st1->GUnd;
    h2 = st2->OHgt+st2->GUnd;

    calc_inst_dif( st1, (h2-h1)/2.0, st2, (h1-h2)/2.0, dif );

    dist = veclen( dif );

    if( dst1 )
    {
        if( dist > DIST_TOL )
        {
            scalevec( dif, -1.0/dist );
            rotvec( dif, &st1->rTopo, dst1 );
            scalevec( dif, -1.0 );
            rotvec( dif, &st2->rTopo, dst2 );
        }
        else
        {
            dst1[0] = dst1[1] = dst1[2] = 0.0;
            dst2[0] = dst2[1] = dst2[2] = 0.0;
        }
    }

    return dist;
}

double calc_ellipsoidal_distance( station *st1, station *st2,
                                  vector3 dst1, vector3 dst2 )
{
    double edist;

    edist = calc_distance( st1, -st1->OHgt-st1->GUnd, st2, -st2->OHgt-st2->GUnd, dst1, dst2 );
    edist *= ellipsoidal_distance_correction( st1, st2 );

    if( dst1 )
    {
        dst1[2] = dst2[2] = 0.0;
    }

    return edist;
}

double calc_msl_distance( station *st1, station *st2,
                          vector3 dst1, vector3 dst2 )
{
    double edist;
    double gdiff = (st1->GUnd - st2->GUnd)/2.0;

    edist = calc_distance( st1, -st1->OHgt-gdiff, st2, -st2->OHgt+gdiff, dst1, dst2 );
    edist *= ellipsoidal_distance_correction( st1, st2 );

    if( dst1 )
    {
        dst1[2] = dst2[2] = 0.0;
    }

    return edist;
}

double calc_azimuth( station *st1, double hgt1, station *st2, double hgt2,
                     int usegrav, vector3 dst1, vector3 dst2 )
{
    vector3 dif;
    double angle, dist2, tmp;

    calc_inst_dif( st1, hgt1, st2, hgt2, dif );

    /* Convert the vector to the topocentric reference frame */

    if( usegrav )
    {
        rotvec( dif, &st1->rGrav, dif );
    }
    else
    {
        rotvec( dif, &st1->rTopo, dif );
    }
    dif[2] = 0.0;
    dist2 = vecdot(dif,dif);

    if( dist2 > DIST_TOL_2 )
    {
        angle = atan2( dif[0], dif[1] );
    }
    else
    {
        angle = 0.0;
    }

    if( dst1 )
    {

        /* Reduce dif to a horizontal vector, scale down by distance
        squared, and rotate 90 degrees, and return to XYZ system
         to get differential vector */

        if( dist2 > DIST_TOL_2 )
        {
            scalevec( dif, 1.0/dist2 );
            tmp = dif[0]; dif[0] = -dif[1]; dif[1] = tmp;
            if( usegrav )
            {
                unrotvec( dif, &st1->rGrav, dif );
            }
            else
            {
                unrotvec( dif, &st1->rTopo, dif );
            }

            /* Convert to the the topocentric systems at the two stations */

            rotvec( dif, &st1->rTopo, dst1 );
            scalevec( dif, -1.0 );
            rotvec( dif, &st2->rTopo, dst2 );
        }
        else
        {
            dst1[0] = dst1[1] = dst1[2] = 0.0;
            dst1[0] = dst1[1] = dst1[2] = 0.0;
        }
    }

    return angle;
}



double calc_prj_azimuth( network *net, station *st1, double hgt1, station *st2, double hgt2,
                         vector3 dst1, vector3 dst2 )
{
    double angle, e1, n1, e2, n2, de, dn;

    if( !is_projection(net->crdsys) || dst1 || dst2 )
    {
        angle = calc_azimuth(st1, hgt1, st2, hgt2, 0, dst1, dst2 );
        if( !is_projection(net->crdsys ) ) return angle;
    }


    geog_to_proj( net->crdsys->prj, st1->ELon, st1->ELat, &e1, &n1 );
    geog_to_proj( net->crdsys->prj, st2->ELon, st2->ELat, &e2, &n2 );

    de = e2-e1;
    dn = n2-n1;
    if( de*de + dn*dn < DIST_TOL_2 )
    {
        angle = 0.0;
    }
    else
    {
        angle = atan2( e2-e1, n2-n1 );
    }

    return angle;
}


double calc_zenith_dist( station *st1, double hgt1, station *st2, double hgt2,
                         vector3 dst1, vector3 dst2 )
{

    vector3 dif;
    double angle, dist, tmp;

    calc_inst_dif( st1, hgt1, st2, hgt2, dif );

    rotvec( dif, &st1->rGrav, dif );   /* Convert to the gravitational frame */

    dist = veclen( dif );
    if( dist > DIST_TOL )
    {
        angle = acos( dif[2]/dist );
    }
    else
    {
        angle = 0.0;
    }

    if( dst1 )
    {

        if( dist > DIST_TOL )
        {
            scalevec( dif, 1.0/(dist*dist) );

            tmp = -dif[2];
            dif[2] = sqrt(dif[0]*dif[0]+dif[1]*dif[1]);
            tmp /= dif[2];
            dif[0] *= tmp;
            dif[1] *= tmp;
            unrotvec( dif, &st1->rGrav, dif );

            /* Convert to the the topocentric systems at the two stations */

            rotvec( dif, &st1->rTopo, dst1 );
            scalevec( dif, -1.0 );
            rotvec( dif, &st2->rTopo, dst2 );
        }
        else
        {
            dst1[0] = dst1[1] = dst1[2] = 0.0;
            dst1[0] = dst1[1] = dst1[2] = 0.0;
        }
    }

    return angle;

}


double calc_hgt_diff( station *st1, double hgt1, station *st2, double hgt2,
                      vector3 dst1, vector3 dst2 )
{

    double diff;

    diff = st2->OHgt + hgt2 - st1->OHgt - hgt1;

    if( dst1 )
    {
        dst1[0] = dst1[1] = dst2[0] = dst2[1] = 0.0;
        dst1[2] = -1.0;
        dst2[2] = 1.0;
    }

    return diff;
}


void calc_vec_dif( station *st1, double hgt1, station *st2, double hgt2 ,
                   vector3 dif, vector3 dst1[3], vector3 dst2[3] )
{

    vector3 unit;
    int i;

    calc_inst_dif( st1, hgt1, st2, hgt2, dif );

    if( dst1 )
    {
        unit[0] = unit[1] = unit[2] = 0.0;

        for( i = 0; i<3; i++ )
        {
            unit[i] = -1.0;
            rotvec( unit, &st1->rTopo, dst1[i] );
            unit[i] = 1.0;
            rotvec( unit, &st2->rTopo, dst2[i] );
            unit[i] = 0.0;
        }
    }
}


void calc_xyz( station *st1, double hgt1, vector3 xyz, vector3 dst1[3] )
{

    vector3 unit;
    int i;

    calc_inst_xyz( st1, hgt1, xyz );

    if( dst1 )
    {
        unit[0] = unit[1] = unit[2] = 0.0;

        for( i = 0; i<3; i++ )
        {
            unit[i] = 1.0;
            rotvec( unit, &st1->rTopo, dst1[i] );
            unit[i] = 0.0;
        }
    }
}


double ellipsoidal_distance_correction( station *st1, station *st2 )
{
    vector3 v1, v2;
    double corr;

    rot_vertical( &st1->rTopo, v1 );
    rot_vertical( &st2->rTopo, v2 );
    corr = vecdot( v1, v2 );
    corr = 1.0 + (1.0 - corr)/12.0;

    return corr;
}

