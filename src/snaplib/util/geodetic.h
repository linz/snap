
/*
   $Log: geodetic.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:48:35  CHRIS
   Initial revision

*/

#ifndef GEODETIC_H_RCSID
#define GEODETIC_H_RCSID "$Id: geodetic.h,v 1.2 2004/04/22 02:35:25 ccrook Exp $"
#endif
/* Basic data types used in geodetic routines                */
/* A 3d vector/coordinate                                    */

#ifndef _GEODETIC_H
#define _GEODETIC_H

typedef double vector3[3];            /* The basic 3d vector */

typedef struct                        /* Definition of a rotation matrix */
{
    double cslt;           /* in compressed form.             */
    double snlt;
    double csln;
    double snln;
} rotmat;

/* Definition of topocentric and gravitational coordinate systems */

void init_toprot( double Lat, double Lon, rotmat *topo ) ;
void init_gravrot( double Lat, double Lon, double Xi, double Eta, rotmat *grav ) ;

/* Functions to rotate vectors */

void rotvec ( vector3 in, rotmat *rot, vector3 out ) ;
void unrotvec( vector3 in, rotmat *rot, vector3 out );
void rot_vertical( rotmat *rot, vector3 vrt );

/* Basic vector functions */

double vecdot( vector3 vec1, vector3 vec2 ) ;
double veclen( vector3 vec ) ;
void vecprd( vector3 vec1, vector3 vec2, vector3 prd ) ;
void vecadd2( vector3 vec1, double mult1, vector3 vec2, double mult2,
              vector3 res ) ;
void vecadd( vector3 vec1, vector3 vec2, vector3 sum );
void vecdif( vector3 vec1, vector3 vec2, vector3 dif );
void scalevec( vector3 vec, double mult );
void veccopy( vector3 vec, vector3 copy );

void premult3( double *m1, double *m2, double *mr, int ndim );

#endif /* GEODETIC_H not defined */
