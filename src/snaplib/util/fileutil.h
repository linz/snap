
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

#ifndef _FILEUTIL_H
#define _FILEUTIL_H

int path_len( const char *base, int want_name );
int file_exists( const char *file );
char *build_filespec( char *spec, int nspec,
                      const char *dir, const char *name, const char *dflt_ext );
char *find_image( const char *argv0 );

#ifndef UNIX
#define PATH_SEPARATOR '\\'
#define PATH_SEPARATOR2 '/'
#define DRIVE_SEPARATOR ':'
#define EXTENSION_SEPARATOR '.'
#else
#define PATH_SEPARATOR '/'
#define PATH_SEPARATOR2 '\\'
#define DRIVE_SEPARATOR '\0'
#define EXTENSION_SEPARATOR '.'
#endif

#define MAX_FILENAME_LEN 256
#define FF_TRYPROGDIR 1
#define FF_TRYHOMEDIR 2
#define FF_TRYBASEDIR 4
#define FF_TRYCURDIR  8
#define FF_TRYALL    15

void set_find_file_directories( const char *progname, const char *basedir, const char *homeenv );
void set_find_file_prog_dir( const char *progname );
void set_find_file_home_dir( const char *homedir );
void set_find_file_base_dir( const char *basefile );
char *find_file( const char *name, const char *dflt_ext, int options );
char *find_file_from_base( const char *base, const char *name, const char *dflt_ext );
FILE *snaptmpfile( void );



#endif
