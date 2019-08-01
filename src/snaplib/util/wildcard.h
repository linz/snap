#ifndef WILDCARD_H
#define WILDCARD_H

bool has_wildcard( const char *pattern );
bool wildcard_match( const char *pattern, const char *s );
bool filename_wildcard_match( const char *pattern, const char *filename );

#endif
