#ifndef _SNAPDATA_H
#define _SNAPDATA_H

class DATAFILE;
/*
   $Log: snapdata.h,v $
   Revision 1.1  1995/12/22 18:48:39  CHRIS
   Initial revision

*/

int read_snap_data(DATAFILE& d, bool (*check_progress)(DATAFILE& d));

#endif
