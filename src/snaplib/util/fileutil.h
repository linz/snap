
#ifndef _FILEUTIL_H
#define _FILEUTIL_H
/*
   $Log: fileutil.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:01:26  CHRIS
   Initial revision

*/

#ifndef FILEUTIL_H_RCSID
#define FILEUTIL_H_RCSID "$Id: fileutil.h,v 1.2 2004/04/22 02:35:25 ccrook Exp $"
#endif
/* fileutil.h: routines to assist file management */

#ifndef UNIX
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR2 '/'
#define DRIVE_SEPARATOR ':'
#define EXTENSION_SEPARATOR '.'
#define PATHENV_SEP ';'

#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR2 '\\'
#define DRIVE_SEPARATOR '\0'
#define EXTENSION_SEPARATOR '.'
#define PATHENV_SEP ':'

/* #define SYS_CONFIG_BASE "/usr/local/share" */
#endif
#define SNAPENV              "SNAPDIR"
#define SYS_CONFIG_BASE      "config"
#define USER_CONFIG_BASE     "linz"

#define MAX_FILENAME_LEN 256

int path_len( const char *base, int want_name );
int file_exists( const char *file );

/* Compile a filename.  If spec is null then it will return a static variable which
   may be modified by other calls to these routines.

   build_config_filespec adds the ability to extract the directory from dir, assuming
   it is a filename, and to include a config subdirectory of dir
   */

char *build_filespec( char *spec, int nspec,
                      const char *dir, const char *name, const char *dflt_ext );
char *build_config_filespec( char *spec, int nspec,
                             const char *dir, int pathonly, const char *config,
                             const char *name, const char *dflt_ext );


const char *image_path();
const char *image_dir();
const char *system_config_dir();
const char *user_config_dir();
const char *project_dir();

/* Reset config directories - use if environment variable is redefined */
void reset_config_dirs();

/* Override the default user environment */
void set_user_config_dir( const char *cfgdir );

/* Set the project dir, that can be included in the find_file search.  Supply the name
   of the project file - the path will be extracted .*/

void set_project_dir( const char *project_dir );

/* Note: find..file return a static character string.  The result should be used
   straight away or copied */

/* Searches for a file with a given name and extension in configuration files, first the user
   and then the system, in each case with and without the supplied extension. Returns NULL
   if not found */

const char *find_config_file( const char *configdir, const char *name, const char *dflt_ext );

/* Searches for a file with a given name relative to the specified base file (ie the directory
   component of the file first with and then without the supplied extension. Returns NULL
   if not found  */

const char *find_relative_file( const char *base, const char *name, const char *dflt_ext );

/* General purpose find file ... can include a base file, which will use find_relative_file,
   try_local, which will try then name with and without the extension if true,
   and a config section to try the user and system configuration directories */

#define FF_TRYLOCAL      1
#define FF_TRYPROJECT    2
#define FF_TRYALL        3

const char *find_file( const char *name, const char *dflt_ext, const char *base, int tryopt, const char *configdir );

/* Create a temporary file, and set up a handler to delete it when the program terminates */

FILE *snaptmpfile();

#endif
