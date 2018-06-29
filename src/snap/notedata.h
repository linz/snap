#ifndef _NOTEDATA_H
#define _NOTEDATA_H

/*
   $Log: notedata.h,v $
   Revision 1.1  1996/01/03 22:02:21  CHRIS
   Initial revision

*/

long save_note( const char *note, int continued );
void list_note( FILE *out, long loc );

#endif
