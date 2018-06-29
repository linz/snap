#ifndef PLOTBIN_H
#define PLOTBIN_H

/*
   $Log: plotbin.h,v $
   Revision 1.3  1997/04/28 10:59:57  CHRIS
   Added header for function obtaining note data.

   Revision 1.2  1996/07/12 20:32:49  CHRIS
   Modified to support hidden stations.

   Revision 1.1  1996/01/03 22:23:53  CHRIS
   Initial revision

*/

#ifndef _SURVDATA_H
#include "snapdata/survdata.h"
#endif

#ifndef INFOWIN_H
#include "infowin.h"
#endif

int reload_binary_data();
void load_observations_from_binary( void );
void open_data_source( void );
void close_data_source( void );

survdata *get_survdata_from_binary( long loc );
void display_note_text( void *dest, PutTextFunc f, long loc );

#endif

