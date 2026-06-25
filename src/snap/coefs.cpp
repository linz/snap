#include "snapconfig.h"
/*
   $Log: coefs.c,v $
   Revision 1.2  2001/01/18 00:01:43  ccrook
   Fixed error assigning value past end of coefname buffer.

   Revision 1.1  1996/01/03 21:57:37  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "adjparam.h"
#include "coefs.h"
#include "snapdata/loaddata.h"
#include "snap/snapglob.h"
#include "snap/genparam.h"
#include "network/network.h"
#include "util/geodetic.h"
#include "util/pi.h"

int refcoef_prm( const char *refcoef )
{
    return get_param( PRM_REFCOEF, refcoef, 1 );
}

const char *refcoef_name( int rc )
{
    return param_type_name( PRM_REFCOEF, rc);
}


double zd_ref_correction( int p, station *st1, station *st2, void *hA, int irow )
{
    vector3 vrt1, vrt2;
    double angle;

    /* Calculate the angle subtended by the verticals at the two stations */

    rot_vertical( &st1->rTopo, vrt1 );
    rot_vertical( &st2->rTopo, vrt2 );

    angle = vecdot( vrt1, vrt2 );
    angle = 1.0 - angle*angle;
    if( angle > 0.0 ) angle = sqrt(angle);

    /* Effect on zenith distance is negative */

    angle = -angle;
    set_param_obseq( p, hA, irow, angle );

    return angle * param_value( p );
}

/*=============================================================*/
/* Distance scale err specific bits                            */

int distsf_prm( const char *distsf )
{
    return get_param( PRM_DISTSF, distsf, 1 );
}

const char *distsf_name( int ds )
{
    return param_type_name(PRM_DISTSF, ds);
}


double distsf_correction( int p, double dist, void *hA, int irow )
{
    if( !p ) return 0;
    dist *= 1.0e-6;
    set_param_obseq( p, hA, irow, dist );
    return param_value( p ) * dist;
}

/*==============================================================*/
/* Azimuth bias parameter                                       */

int brngref_prm( const char *brngref )
{
    return get_param( PRM_BRNGREF, brngref, 1 );
}

const char *brngref_name( int br )
{
    return param_type_name(PRM_BRNGREF, br);
}

double brngref_correction( int br, void *hA, int irow )
{
    if( !br ) return 0;
    set_param_obseq( br, hA, irow, STOR );
    return param_value( br ) * STOR;
}


/*==============================================================*/
/* Systematic error parameter                                   */

int syserr_prm( const char *syserr )
{
    return get_param( PRM_SYSERR, syserr, 1 );
}

const char *syserr_name( int se )
{
    return param_type_name(PRM_SYSERR, se);
}

double syserr_correction( int se, double influence, void *hA, int irow )
{
    if( !se ) return 0;
    set_param_obseq( se, hA, irow, influence );
    return param_value( se ) * influence;
}

