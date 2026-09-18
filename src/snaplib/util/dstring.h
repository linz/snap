#ifndef _DSTRING_H
#define _DSTRING_H

/*
   $Log: dstring.h,v $
   Revision 1.2  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 18:57:57  CHRIS
   Initial revision

*/

#include <string>

char *copy_string( const char *string);
char *copy_string_nch( const char *string, int nch );
void dump_string_c( const char *string, FILE *f );
char *reload_string_c( FILE *f );
/* std::string-based equivalents of dump_string_c/reload_string_c, sharing the
 * same on-disk length-prefixed format. Unlike the char* versions, there is no
 * null-vs-empty distinction - callers with a genuine optional string need
 * std::optional<std::string> around this, not a sentinel value. */
void dump_string( const std::string &string, FILE *f );
std::string reload_string( FILE *f );
/* Case insensitive and underscore/whitespace insensitive match */
int ismatch( const char *string1, const char *string2 );
/* Next field - skips whitespace, puts zero delimiter at end of next 
 * field in string, and returns pointer to field if found, 0 otherwise.
 * String pointer is updated to start of next search.
 */
char *next_field( char **start );

#endif
