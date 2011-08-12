#ifndef _LOADSNAP_H
#define _LOADSNAP_H

/*
   $Log: loadsnap.h,v $
   Revision 1.2  1998/05/21 04:02:02  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.1  1996/01/03 22:00:10  CHRIS
   Initial revision

*/

#ifndef LOADSNAP_H_RCSID
#define LOADSNAP_H_RCSID "$Id: loadsnap.h,v 1.2 1998/05/21 04:02:02 ccrook Exp $"
#endif

void set_convert_ratios_to_distance( int option );
void set_ignore_missing_stations( int option );
void set_require_obs_date( int option );
void init_load_snap( void );
int term_load_snap( void );

#endif
