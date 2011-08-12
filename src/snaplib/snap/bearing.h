#ifndef _BEARING_H
#define _BEARING_H

/*
   $Log: bearing.h,v $
   Revision 1.1  2004/04/20 00:58:37  ccrook
   These files were ommitted when they were first built

   Revision 1.1  1995/12/22 17:47:10  CHRIS
   Initial revision

*/

#ifndef BEARING_H_RCSID
#define BEARING_H_RCSID "$Id: bearing.h,v 1.1 2004/04/20 00:58:37 ccrook Exp $"
#endif

#ifndef _COORDSYS_H
#include "coordsys/coordsys.h"
#endif

#ifndef _NETWORK_H
#include "network/network.h"
#endif

typedef struct
{
    char *name;           /* The name of the reference frame     */
    coordsys *prjsys;     /* The coordinate system for the projection */
    coord_conversion prjconv;  /* The conversion from the network geodetic
                             coordinate system to the bearing projection system */
} brngProjection;

#define REFFRAMELEN 20

int get_bproj( const char *name ) ;
int bproj_count( void );
brngProjection *bproj_from_id( int id );
const char *bproj_name( int id );
void clear_bproj_list( void );


int calc_prj_azimuth2( int bproj_id,
                       station *st1, double hgt1, station *st2, double hgt2,
                       double *angle, vector3 dst1, vector3 dst2 );

#endif
