#include "snapconfig.h"

/*
   $Log: datatype.c,v $
   Revision 1.1  1995/12/22 18:41:13  CHRIS
   Initial revision

*/
#include <stdio.h>
#include "snapdata/datatype.h"
#include "util/errdef.h"
#include "string.h"

/* Definition of survey datatypes. */

datatypedef datatype[] =
{
    /*
      P  point observation (ie no target station)
      V  vector observation
      A  components are read as angles
      F  uses a reference frame def
      T  defines a reference frame translation
      Pj uses a projection
      C  uses a refraction coefficient
      Ds  true if is a distance
      D  uses a distance scale factor
      B  uses a bearing orientation error
      N  single observation is meaningless - needs group
      J  affected by combining in observation set
      D  the order if inst/target stations is significant

      Formatting info
      Wid   Default width of observation
      Ewid  Default width of error
      Rwid  Default width of residual
      Ndp   Default number of decimal places

      NOTE: The elements in this array should correspond with the enumerated
      types in datatype.h

        Code   Name                       P  V  A  F  T  Pj C  Ds D  B  N  J  D Wid Ewid Rwid Ndp*/
    {GB, "GB", "GPS baseline",           0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 12,  6,   7,  3},
    {GX, "GX", "GPS point coord",        1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 12,  6,   7,  3},
    {SD, "SD", "slope distance",         0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,  9,  6,   7,  4},
    {HD, "HD", "horizontal distance",    0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,  9,  6,   7,  4},
    {ED, "ED", "ellipsoidal distance",   0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,  9,  6,   7,  4},
    {MD, "MD", "mean sea level distance",0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0,  9,  6,   7,  4},
    {DR, "DR", "distance ratio",         0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0,  9,  6,   7,  4},
    {HA, "HA", "horizontal angle",       0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 11,  4,   7,  1},
    {AZ, "AZ", "azimuth",                0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 11,  4,   7,  1},
    {PB, "PB", "projection bearing",     0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 11,  4,   7,  1},
    {ZD, "ZD", "zenith distance",        0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 11,  4,   7,  1},
    {LV, "LV", "height difference",      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,  6,   7,  4},
    {OH, "OH", "orthometric height",     1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,  6,   7,  4},
    {EH, "EH", "ellipsoidal height",     1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 10,  6,   7,  4},
    {LT, "LT", "latitude",               1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14,  4,   7,  2},
    {LN, "LN", "longitude",              1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 14,  4,   7,  2},
    {EP, "EP", "ellipsoidal point crd",  1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 14,  4,   7,  2},
};

/* SEE NOTE ABOVE BEFORE MODIFYING THIS */

/*===============================================================*/
/* Get observation snap type from its code                       */

int datatype_from_code( const char *code )
{
    int i;
    if( _stricmp( code, "GPS" ) == 0 ) code = "GB";
    for( i=0; i < NOBSTYPE; i++ )
    {
        if( _stricmp(datatype[i].code, code) == 0 )
        {
            if( datatype[i].id != i ) handle_error( INTERNAL_ERROR, "Data type id inconsistent in datatype.c",NO_MESSAGE);
            return i;
        }
    }
    return NOBSTYPE;
}

datatypedef *datatypedef_from_code( const char *code )
{
    int i = datatype_from_code(code);
    if( i < NOBSTYPE ) return &(datatype[i]);
    return 0;
}

datatypedef *datatypedef_from_id( int idtype )
{
    return &(datatype[idtype]);
}
