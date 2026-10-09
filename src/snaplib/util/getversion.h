#ifndef GETVERSION_H
#define GETVERSION_H

#include <string>
#include <string_view>

std::string getProgramName();
/// The program version from the VERSION file, followed by "-" and the VERSIONID file text if
/// there is one. The default version is used if there is no VERSION file.
const std::string &getProgramVersion(std::string_view version);

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
#define PROGRAM_NAME getProgramName().c_str()
#define PROGRAM_VERSION getProgramVersion(SNAPVERSION).c_str()
#define PROGRAM_DATE programDate


#endif
