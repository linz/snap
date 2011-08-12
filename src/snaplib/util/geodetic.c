#include "snapconfig.h"
/*
   $Log: geodetic.c,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:47:00  CHRIS
   Initial revision

*/

#include <math.h>
#include "util/geodetic.h"

static char rcsid[]="$Id: geodetic.c,v 1.2 2004/04/22 02:35:25 ccrook Exp $";

/*----------------------------------------------------------------*/
/*  Procedure to compute the rotations relating the topocentric   */
/*  and gravimetric coordinate systems to the geocentric system   */
/*                                                                */
/*----------------------------------------------------------------*/

void init_toprot( double Lat, double Lon, rotmat *topo )
{
    topo->cslt = cos(Lat);
    topo->snlt = sin(Lat);
    topo->csln = cos(Lon);
    topo->snln = sin(Lon);
}


void init_gravrot( double Lat, double Lon, double Xi, double Eta, rotmat *grav )
{
    Lat += Xi;
    Lon += Eta/cos(Lat);
    init_toprot( Lat, Lon, grav );
}


#define Rslt rot->snlt
#define Rsln rot->snln
#define Rclt rot->cslt
#define Rcln rot->csln

void rotvec ( vector3 in, rotmat *rot, vector3 out )
{
    vector3 tmp;

    tmp[0] = -in[0]*Rsln      + in[1]*Rcln;
    tmp[1] = -in[0]*Rslt*Rcln - in[1]*Rslt*Rsln + in[2]*Rclt;
    tmp[2] =  in[0]*Rclt*Rcln + in[1]*Rclt*Rsln + in[2]*Rslt;
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

void unrotvec( vector3 in, rotmat *rot, vector3 out )
{
    vector3 tmp;

    tmp[0] = -in[0]*Rsln      - in[1]*Rslt*Rcln + in[2]*Rclt*Rcln;
    tmp[1] =  in[0]*Rcln      - in[1]*Rslt*Rsln + in[2]*Rclt*Rsln;
    tmp[2] =                    in[1]*Rclt      + in[2]*Rslt;
    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

void rot_vertical( rotmat *rot, vector3 vrt )
{
    vrt[0] = Rclt*Rcln;
    vrt[1] = Rclt*Rsln;
    vrt[2] = Rslt;
}

#undef Rslt
#undef Rsln
#undef Rclt
#undef Rcln


/*----------------------------------------------------------------*/
/*   Some vector processing routines..                            */
/*                                                                */
/*   vecdot	Forms dot product of two vectors                  */
/*   veclen     Returns the length of a vector                    */
/*   vecprd     Forms the vector product of two vectors           */
/*   vecadd     Adds two scaled vectors                           */
/*----------------------------------------------------------------*/

double vecdot( vector3 vec1, vector3 vec2 )
{
    return vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2];
}

double veclen( vector3 vec )
{
    return sqrt( vecdot( vec, vec ));
}

void veccopy( vector3 vec, vector3 copy )
{
    copy[0] = vec[0];
    copy[1] = vec[1];
    copy[2] = vec[2];
}

void vecprd( vector3 vec1, vector3 vec2, vector3 prd )
{
    double tmp[3];
    tmp[0] = vec1[1]*vec2[2] - vec1[2]*vec2[1];
    tmp[1] = vec1[2]*vec2[0] - vec1[0]*vec2[2];
    tmp[2] = vec1[0]*vec2[1] - vec1[1]*vec2[0];
    prd[0] = tmp[0];
    prd[1] = tmp[1];
    prd[2] = tmp[2];
}

void vecadd2( vector3 vec1, double mult1, vector3 vec2, double mult2,
              vector3 res )
{
    int i;
    for (i=0; i<3; i++) res[i] = vec1[i]*mult1 + vec2[i]*mult2;
}

void vecadd( vector3 vec1, vector3 vec2, vector3 dif )
{
    dif[0] = vec1[0] + vec2[0];
    dif[1] = vec1[1] + vec2[1];
    dif[2] = vec1[2] + vec2[2];
}

void vecdif( vector3 vec1, vector3 vec2, vector3 dif )
{
    dif[0] = vec1[0] - vec2[0];
    dif[1] = vec1[1] - vec2[1];
    dif[2] = vec1[2] - vec2[2];
}

void scalevec( vector3 vec, double mult )
{
    vec[0] *= mult;
    vec[1] *= mult;
    vec[2] *= mult;
}


/* Put product m1.m2 in mr, where mr and m2 can be the same */

void premult3( double *m1, double *m2, double *mr, int ndim )
{
    double col[3], sum;
    int i, j, k, ij, jk;

    for( i = 0; i < ndim; i++ )
    {
        ij = i;
        for( j = 0; j < 3; j++ )
        {
            col[j] = m2[ij];
            ij += ndim;
        }
        ij = i; jk = 0;
        for( j = 0; j < 3; j++ )
        {
            sum = 0.0;
            for( k = 0; k < 3; k++ ) sum += m1[jk++]*col[k];
            mr[ij] = sum;
            ij += ndim;
        }
    }
}
