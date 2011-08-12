#ifndef VERSIONINFO_H
#define VERSIONINFO_H

/* One module should include this file and add macros defining program name and version number.
   Build function should ensure that the module is always rebuilt.
*/

typedef struct
{
    char *program;
    char *version;
    char *builddate;
} VersionInfo;

#ifdef VERSIONINFO

#ifndef PROGRAM
#define PROGRAM "Unnamed"
#endif

#ifndef VERSION
#define VERSION "0.0.0"
#endif

VersionInfo ProgramVersion =
{
    PROGRAM,
    VERSION,
    __DATE__ " " __TIME__
};


#else

extern VersionInfo ProgramVersion;
#endif

#define PROGRAM_NAME ProgramVersion.program
#define PROGRAM_VERSION ProgramVersion.version
#define PROGRAM_DATE ProgramVersion.builddate


#endif