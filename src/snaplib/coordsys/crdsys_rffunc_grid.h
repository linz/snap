#ifndef _CRDSYSR5_H
#define _CRDSYSR5_H

/* crdsysr5.c:  Grid reference frame function header
*/

/*
   $Log: crdsysr5.h,v $
   Revision 1.1  2003/11/28 01:59:26  ccrook
   Updated to be able to use grid transformation for datum changes (ie to
   support official NZGD49-NZGD2000 conversion)


*/

#ifndef _COORDSYS_H
#include "coordsys/coordsys.h"
#endif

ref_frame_func *create_rf_grid_func( const char *type, const char *filename, char *description );

#endif
