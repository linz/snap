#ifndef SNAPCTYPE_H
#define SNAPCTYPE_H

#include <ctype.h>

// ISALNUM can be replaced by std::isalnum(static_cast<unsigned char>(c)) from
// <cctype> if/when the repo migrates to that. The cast to unsigned char is
// needed because isalnum is undefined for negative values such as the bytes
// of a UTF-8 character.
#define ISALNUM(c) (isalnum((unsigned char)(c)))
#define ISDIGIT(c) (isdigit((unsigned char)(c)))
#define ISPRINT(c) (isprint((unsigned char)(c)))
#define ISSPACE(c) (isspace((unsigned char)(c)))
#define ISXDIGIT(c) (isxdigit((unsigned char)(c)))
#define TOLOWER(c) (((unsigned char)(c) & 0x80) ? c : tolower((unsigned char)(c)))
#define TOUPPER(c) (((unsigned char)(c) & 0x80) ? c : toupper((unsigned char)(c)))

#endif
