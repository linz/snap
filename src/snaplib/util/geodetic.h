
/*
   $Log: geodetic.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:48:35  CHRIS
   Initial revision

*/

/* Basic data types used in geodetic routines                */
/* A 3d vector/coordinate                                    */

#ifndef _GEODETIC_H
#define _GEODETIC_H

typedef double vector3[3];            /* The basic 3d vector */

/// A rotation between the geocentric axes and the axes of a local system at a
/// point, held in compressed form as the cosine and sine of the latitude and
/// longitude that define the local system.  The members are public because
/// station's on-disk layout (STATION_DISK_FIELDS) writes them as four doubles.
struct rotmat
{
    // The defaults give the identity rotation: the local axes coincide with
    // the geocentric axes (latitude +90 degrees, longitude -90 degrees).
    double cslt = 0.0;
    double snlt = 1.0;
    double csln = 0.0;
    double snln = -1.0;

    rotmat() = default;

    /// The topocentric system at a point.
    rotmat( double Lat,   ///< Latitude (radians)
            double Lon ); ///< Longitude (radians)

    /// The gravimetric system at a point: the topocentric system at the
    /// latitude Lat+Xi and the longitude Lon+Eta/cos(Lat+Xi).
    static rotmat gravimetric( double Lat,   ///< Latitude (radians)
                               double Lon,   ///< Longitude (radians)
                               double Xi,    ///< Added to the latitude
                               double Eta ); ///< Divided by cos(latitude) and added to the longitude

    /// Rotates a vector from the geocentric axes to the local axes.  The
    /// input and output may be the same vector.
    void rotvec( vector3 in, vector3 out ) const;

    /// Rotates a vector from the local axes to the geocentric axes.  The
    /// input and output may be the same vector.
    void unrotvec( vector3 in, vector3 out ) const;

    /// The local vertical as a unit vector in the geocentric axes.
    void rot_vertical( vector3 vrt ) const;
};

static_assert( sizeof(rotmat) == 4*sizeof(double), "rotmat is written to disk as four doubles" );

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
