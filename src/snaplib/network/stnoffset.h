#ifndef _STNOFFSET_H
#define _STNOFFSET_H

/* Need definitions of a coordinate system and of basic geodetic functions */

#include "util/geodetic.h"


#define STN_TS_STEP 0
#define STN_TS_SERIES 1
#define STN_TS_VELOCITY 2

#define STN_TS_ENU 0
#define STN_TS_XYZ 1

struct stn_tspoint
{
    double date;
    vector3 denu;
};

/* Note: tspoints is allocated in same allocation as stn_offset_comp
 * if it is required  */

struct stn_offset_comp
{
    int mode;
    int isxyz;
    int ntspoints;
    stn_tspoint basepoint;
    stn_tspoint *tspoints;
    struct stn_offset_comp *next;
};

struct stn_offset
{
    int isdeformation;
    stn_offset_comp *components;

};

stn_offset_comp *create_stn_offset_comp( int mode, int isxyz, int ntspoints );
void add_stn_offset_comp_to_station( station *st, stn_offset_comp *comp, int isdeformation );
void delete_station_offset( station *st );

#endif /* STNOFFSET_H not defined */



