#ifndef SNAPCTYPE_H
#define SNAPCTYPE_H

#include <ctype.h>

#define ISALNUM(c) (isalnum((unsigned char)(c)))
#define ISDIGIT(c) (isdigit((unsigned char)(c)))
#define ISPRINT(c) (isprint((unsigned char)(c)))
#define ISSPACE(c) (isspace((unsigned char)(c)))
#define ISXDIGIT(c) (isxdigit((unsigned char)(c)))
#define TOLOWER(c) (((unsigned char)(c) & 0x80) ? c : tolower((unsigned char)(c)))
#define TOUPPER(c) (((unsigned char)(c) & 0x80) ? c : toupper((unsigned char)(c)))

#endif
