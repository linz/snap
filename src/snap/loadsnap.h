#ifndef _LOADSNAP_H
#define _LOADSNAP_H

/*
   $Log: loadsnap.h,v $
   Revision 1.2  1998/05/21 04:02:02  ccrook
   Added support for deformation model to be applied in the adjustment.

   Revision 1.1  1996/01/03 22:00:10  CHRIS
   Initial revision

*/

#define NO_IGNORE_MISSING_STATIONS 0
#define IGNORE_MISSING_STATIONS 1
#define REPORT_MISSING_NONE 0
#define REPORT_MISSING_UNLISTED 1
#define REPORT_MISSING_ALL 2

void set_convert_ratios_to_distance( int option );
void set_ignore_missing_stations( int option );
void set_report_missing_stations( int option );
void set_accept_missing_station( const char *code );
void set_require_obs_date( int option );
void init_load_snap( void );
int term_load_snap( void );

#endif
