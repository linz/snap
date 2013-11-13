#ifndef _DSTRING_H
#define _DSTRING_H

/*
   $Log: dstring.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:57:57  CHRIS
   Initial revision

*/

#ifndef DSTRING_H_RCSID
#define DSTRING_H_RCSID "$Id: dstring.h,v 1.2 2004/04/22 02:35:24 ccrook Exp $"
#endif

char *copy_string( const char *string);
char *copy_string_nch( const char *string, int nch );
void dump_string( const char *string, FILE *f );
char *reload_string( FILE *f );
/* Case insensitive and underscore/whitespace insensitive match */
int ismatch( const char *string1, const char *string2 );

#endif
