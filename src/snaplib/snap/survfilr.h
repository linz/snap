#ifndef _SURVFILR_H
#define _SURVFILR_H

/*
   $Log: survfilr.h,v $
   Revision 1.2  1998/06/15 02:26:14  ccrook
   Modified to handle long integer number of observations

   Revision 1.1  1995/12/22 18:38:08  CHRIS
   Initial revision

*/

#ifndef _SURVFILE_H
#include "snap/survfile.h"
#endif

long read_data_files( FILE *lst );
void count_obs( int type, int ifile, double date, char unused );

#endif

