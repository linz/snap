#ifndef _SNAPCONFIG_H
#define _SNAPCONFIG_H

#if !defined(_WIN32) && !defined(_MSC_VER)
#define UNIX
#endif


#if defined(UNIX)
#define _fileno fileno
#define _unlink unlink
#define _setmode setmode
#define _access access
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _isatty isatty
#define _hypot hypot
#define _tempnam tempnam
#define _strupr strupr
#define _strlwr strlwr
#define _strdup strdup
#endif

#endif
