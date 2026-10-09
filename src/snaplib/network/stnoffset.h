#ifndef _STNOFFSET_H
#define _STNOFFSET_H

#include <cstddef>
#include <vector>

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

struct stn_offset_comp
{
    int mode = STN_TS_STEP;            ///< STN_TS_STEP, STN_TS_SERIES or STN_TS_VELOCITY
    int isxyz = STN_TS_ENU;            ///< STN_TS_ENU or STN_TS_XYZ
    stn_tspoint basepoint{};           ///< The step or velocity, or the base of a time series
    std::vector<stn_tspoint> tspoints; ///< The points of a time series, in date order

    /// Creates a component with room for the given number of time series points
    stn_offset_comp( const int tsmode, const int tsxyz, const size_t npoints )
        : mode( tsmode ), isxyz( tsxyz ), tspoints( npoints ) {}
};

struct stn_offset
{
    int isdeformation = 0;                   ///< Non-zero if the offsets define a deformation
    std::vector<stn_offset_comp> components; ///< The components of the offset, in the order added
};

void add_stn_offset_comp_to_station( station *st, stn_offset_comp comp, int isdeformation );
void delete_station_offset( station *st );

#endif /* STNOFFSET_H not defined */



