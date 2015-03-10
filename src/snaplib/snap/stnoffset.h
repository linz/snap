#ifndef _STNOFFSET_H
#define _STNOFFSET_H

/*
   $Log: deform.h,v $
   Revision 1.1  1998/06/04 02:24:53  ccrook
   Initial revision


*/

#ifndef _NETWORK_H
#include "network/network.h"
#endif

typedef struct
{
    double date;
    double enu[3];
} stn_tspoint;

typedef struct 
{
    int mode;
    int ordinates;
    stn_tspoint basepoint;
    stn_tspoint *tspoints;
} stn_offset_comp;

typedef struct
{
    int deformation;
    int ncomp;
    stn_offset_comp *components;

} stn_offset;


#endif
