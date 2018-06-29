#ifndef GETVERSION_H
#define GETVERSION_H

const char *getProgramName();
const char *getProgramVersion(const char *version);

/* Programs using this need to define one module which
 * includes this with DEFINE_PROGRAM_DATE set.
 * This should be recompiled every time the program is built
 */

#ifdef GETVERSION_SET_PROGRAM_DATE
const char *programDate=__DATE__ " " __TIME__;
#else
extern const char *programDate;
#endif

#include "snapversion.h"
#define PROGRAM_NAME getProgramName()
#define PROGRAM_VERSION getProgramVersion(SNAPVERSION)
#define PROGRAM_DATE programDate


#endif
