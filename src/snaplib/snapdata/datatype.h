
/*
   $Log: datatype.h,v $
   Revision 1.1  1995/12/22 18:44:20  CHRIS
   Initial revision

*/

/* Definition of valid data types - datatype array is implemented in module
   datatype.c */

#ifndef _DATATYPE_H
#define _DATATYPE_H


/* Data types
   GB = GPS baseline
   GX = GPS point obs
   SD = Slope distance
   HD = Horizontal distance
   ED = Ellipsoidal distance
   MD = Mean sea level distance
   DR = Distance ratios
   HA = Horizontal angle
   AZ = Azimuth
   ZD = Zenith distance
   LV = Levelling
   LT = Latitude observation
   LN = Longitude observation
   PB = Projection bearing
   OH = Othometric height
   EH = Ellipsoidal height
   EP = Ellipoidal point obs
*/

enum
{
    GB,
    GX,
    SD,
    HD,
    ED,
    MD,
    DR,
    HA,
    AZ,
    PB,
    ZD,
    LV,
    OH,
    EH,
    LT,
    LN,
    EP,
    NOBSTYPE
};

struct datatypedef
{
    int    id;
    const char   *code;
    const char   *name;
    bool   ispoint;    /* True if is a point observation (ie no target station) */
    bool   isvector;   /* True if multiple components of observation */
    bool   isangle;    /* True if components are read as angles */
    bool   reffrm;     /* True if uses a reference frame def */
    bool   reftrns;    /* True if defines a reference frame translation */
    bool   projctn;    /* True if uses a projection */
    bool   refcoef;    /* True if uses a refraction coefficient */
    bool   isdistance; /* True if this is a distance type */
    bool   distsf;     /* True if uses a distance scale factor */
    bool   brngref;    /* True if uses a bearing orientation error */
    bool   needsgroup; /* True if single observation is meaningless - needs group */
    bool   joinsgroup; /* True if affected by combining in observation set */
    bool   isdirectional; /* True if the order if inst/target stations is significant */
    int    dfltwidth;      /* Default width and no decimal places */
    int    dflterrwid;
    int    dfltreswid;
    int    dfltndp;
};

extern datatypedef datatype[];

/* Returns NOBSTYPE if code is not valid */
int datatype_from_code( const char *code );

datatypedef* datatypedef_from_code( const char *code );
datatypedef *datatypedef_from_id( int idtype );

#endif  /* DATATYPE_H not defined */
