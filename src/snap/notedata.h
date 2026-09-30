#ifndef _NOTEDATA_H
#define _NOTEDATA_H

/*
   $Log: notedata.h,v $
   Revision 1.1  1996/01/03 22:02:21  CHRIS
   Initial revision

*/

#include <stdint.h>
#include <stdio.h>
#include <string_view>

int64_t save_note( std::string_view note, int continued );
void list_note( FILE *out, int64_t loc );

#endif
