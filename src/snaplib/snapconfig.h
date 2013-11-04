#ifndef _SNAPCONFIG_H
#define _SNAPCONFIG_H

#ifndef UNIX
#if !defined(_WIN32) && !defined(_MSC_VER)
#define UNIX
#endif
#endif


#if defined(UNIX)
#include <ctype.h>
#define _fileno fileno
#define _unlink unlink
#define _setmode setmode
#define _access access
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _isatty isatty
#define _hypot hypot
#define _tempnam tempnam
#define _strupr(x) {char *c=(x);while(*c){ *c=(char) toupper((int)*c); c++; }}
#define _strlwr(x) {char *c=(x);while(*c){ *c=(char) tolower((int)*c); c++; }}
#define _strdup strdup
#endif



/* Default epoch for coordinate conversions between geoid coordinate system and 
   other coordinate systems.  Use this as don't want to be stopped from converting
   when there is a dynamic relationship (14 param BW or deformation), as this 
   conversion doesn't need to be that accurate... */

#define DEFAULT_GEOID_CRDSYS_EPOCH 2000.0

#endif
