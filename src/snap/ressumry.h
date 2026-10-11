#ifndef _RESSUMRY_H
#define _RESSUMRY_H

/*
   $Log: ressumry.h,v $
   Revision 1.1  1996/01/03 22:08:21  CHRIS
   Initial revision

*/

#include <string>

int define_error_summary( const std::string &definition );
void print_error_summary( FILE *lst );

#endif
