#ifndef _SNAPCONFIG_H
#define _SNAPCONFIG_H

#ifndef UNIX
#if !defined(_WIN32) && !defined(_MSC_VER)
#define UNIX
#endif
#endif


#if defined(UNIX)
// unix version of functions
#include <ctype.h>
#include <unistd.h>
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
#define _set_output_format(x)
#define _TWO_DIGIT_EXPONENT
#define _strdup strdup

// std::regex not fully supported by g++ at 4.8
#define REGEX_BOOST 1
#endif




#endif
