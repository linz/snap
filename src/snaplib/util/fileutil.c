#include "snapconfig.h"
/* fileutil.c:  Provides some simple file management routines */

/*
   $Log: fileutil.c,v $
   Revision 1.3  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:01:10  CHRIS
   Initial revision

*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <io.h>
#endif
#ifdef UNIX
#include <sys/types.h>
#include <unistd.h>
#endif

#include "util/fileutil.h"
#include "util/chkalloc.h"
#include "util/dstring.h"

static char rcsid[]="$Id: fileutil.c,v 1.3 2004/04/22 02:35:24 ccrook Exp $";

int path_len( const char *base, int want_name )
{
    const char *c;
    int i, idot, ipath;

    idot = -2;
    ipath = -1;
    for( c = base, i=0; *c; c++, i++ )
    {
        if( *c == DRIVE_SEPARATOR ||
                *c == PATH_SEPARATOR  ||
                *c == PATH_SEPARATOR2 ) ipath = i;
        else if( *c == EXTENSION_SEPARATOR ) idot = i;
    }
    if( idot < ipath ) idot = i;
    return want_name ? idot : ipath+1;
}

/* Check whether a file exists by trying to open it for reading */

int file_exists( const char *file )
{
    if( ! file ) return 0;
    return _access( file, 04 ) == 0 ? 1 : 0;
}


char *build_filespec( char *spec, int nspec,
                      const char *dir, const char *name, const char *dflt_ext )
{
    int nch;

    if( dir )
    {
        nch = strlen( dir );
        strncpy( spec, dir, nspec - 1 );
    }
    else
    {
        nch = 0;
    }

    if( nch < nspec-1 )
    {
        strncpy( spec+nch, name, nspec-nch-1);
    }
    spec[nspec-1] = 0;

    nch = strlen( spec );
    if( dflt_ext && nch < nspec-1 && nch == path_len(spec,1) )
    {
        strncpy( spec+nch, dflt_ext, nspec-nch-1);
        spec[nspec-1] = 0;
    }

    /* Ensure that the path separator is compatible with the operating system */

    for( nch = strlen(spec); nch--; )
    {
        if( spec[nch] == PATH_SEPARATOR2 ) spec[nch] = PATH_SEPARATOR;
    }

    return spec;
}

/* Routine looks for the image file corresponding to the argument supplied */

#ifdef UNIX

char *find_image( const char *argv0 )
{
    char* path;
    char _link[20];
    char buf[10];
    pid_t pid = getpid();
    sprintf( buf,"%d", pid );
    strcpy( _link, "/proc/" );
    strcat( _link, buf );
#if defined(__linux) || defined(linux)
    strcat( _link, "/exe" );
#endif
#if defined(sun) || defined(__sun)
    strcat( _link, "/path/a.out" );
#endif
#if defined(__bsdi__)
    strcat( _link, "/file" );
#endif
    char proc[512];
    ssize_t len = readlink( _link, proc, 512);
    path = 0;
    if ( len != -1 )
    {
      proc[len] = '\0';
      path = check_malloc(strlen(proc)+1 );
      path = strcpy( path, proc );
    }
    return path;
}

#if 0
char *find_image( const char *argv0 )
{
    char *image;
    char *path = getenv("PATH");
    char *p;
    int maxlen;
    int len;

    /* Find the longest string in the path */

    for( p = path, len = 1; *p; p++, len++ )
    {
        if( *p == ';' || *p == ':' ) len = 0;
        if( len > maxlen ) maxlen = len;
    }

    /* Allocate a string big enough for the path plus the argv[0] name */

    image = (char *)  check_malloc( maxlen + strlen(argv0) + 3);
    if( _access(argv0,X_OK)==0 )
    {
        strcpy(image,argv0);
    }
    else
    {
        p = path;
        image[0] = 0;
        while( *p )
        {
            char *e = p;
            while( *e && *e != ';' && *e != ':' ) e++;
            len = e-p;
            if( len > 0 ) strncpy( image, p, len );
            image[len] = PATH_SEPARATOR;
            strcpy( image+len+1, argv0 );
            if( _access(image,X_OK) == 0 ) break;
            image[0] = 0;
            p = e;
            if( *p ) p++;
        }
    }
    return image;
}
#endif

#else

char *find_image( const char *argv0 )
{

    char *path=NULL;

    _get_pgmptr(&path);
    if( path ) { return copy_string(path); }
    return copy_string("");

}

#endif


/*======================================================================*/
/* Support for configuration and other file search paths...             */

static char *base_dir = NULL;
static char *prog_dir = NULL;
static char *home_dir = NULL;

static char spec[MAX_FILENAME_LEN];

void set_find_file_directories( const char *progname, const char *basedir, const char *homeenv )
{
    if( progname )
    {
        set_find_file_prog_dir( progname );
    }
    if( basedir )
    {
        set_find_file_base_dir( basedir );
    }
    if( homeenv ) 
    {
        char *homedir = getenv( homeenv );
        if( homedir )
        {
            set_find_file_home_dir( homedir );
        }
    }
}

void set_find_file_base_dir( const char *basefile )
{
    int l;
    l = path_len( basefile, 0 );
    if( base_dir ) check_free( base_dir);
    base_dir = (char *) check_malloc( l + 1 );
    strncpy( base_dir, basefile, l );
    base_dir[l] = 0;
}

void set_find_file_home_dir( const char *homedir )
{
    int l;
    if( home_dir ) check_free( home_dir );
    l = strlen(homedir);
    home_dir = (char *) check_malloc( l+2 );
    strcpy( home_dir, homedir );
    if( home_dir[l-1] != PATH_SEPARATOR )
    {
        home_dir[l] = PATH_SEPARATOR;
        home_dir[l+1] = '\0';
    }
}

void set_find_file_prog_dir( const char *progname )
{
    int l;
    l = path_len( progname, 0 );
    if( prog_dir ) check_free( prog_dir );
    prog_dir = (char *) check_malloc( l + 1 );
    strncpy( prog_dir, progname, l );
    prog_dir[l] = 0;
}


char *find_file( const char *name, const char *dflt_ext, int options )
{

    if( (options & FF_TRYBASEDIR) && file_exists(
                build_filespec( spec, MAX_FILENAME_LEN, base_dir, name, dflt_ext )) )
    {
        return spec;
    }
    else if( (options & FF_TRYCURDIR) && file_exists (
                 build_filespec( spec, MAX_FILENAME_LEN, NULL, name, dflt_ext )) )
    {
        return spec;
    }
    else if( (options & FF_TRYHOMEDIR) && home_dir && file_exists (
                 build_filespec( spec, MAX_FILENAME_LEN, home_dir, name, dflt_ext )) )
    {
        return spec;
    }
    else if( (options & FF_TRYPROGDIR) && prog_dir && file_exists (
                 build_filespec( spec, MAX_FILENAME_LEN, prog_dir, name, dflt_ext )) )
    {
        return spec;
    }
    return NULL;
}

char *find_file_from_base( const char *base, const char *name, const char *dflt_ext )
{
    char base_dir[MAX_FILENAME_LEN];
    int len = 0;

    base_dir[0] = 0;
    if( base ) len = path_len(base,0);
    if( len > 0 && len < MAX_FILENAME_LEN )
    {
        strncpy(base_dir,base,len);
        base_dir[len] = 0;
    }

    if( file_exists(
                build_filespec( spec, MAX_FILENAME_LEN, base_dir, name, dflt_ext )) )
    {
        return spec;
    }
    return NULL;
}

typedef struct s_tmpfile_def
{
    char *name;
    FILE *handle;
    struct s_tmpfile_def *next;
} tmpfile_def;

static tmpfile_def *tmpfile_list = 0;

static void delete_temp_files( void )
{
    tmpfile_def *del = tmpfile_list;
    while( tmpfile_list )
    {
        del = tmpfile_list;
        tmpfile_list = tmpfile_list->next;
        if( _unlink( del->name ) != 0 )
        {
            fclose(del->handle);
            _unlink(del->name);
        }
        check_free(del->name);
        check_free(del);
    }
}

FILE *snaptmpfile()
{
    FILE *f;
    char *name;
    tmpfile_def *def;

    name = _strdup(_tempnam("/tmp","snaptmp.") );
    if( ! name ) return NULL;
    f = fopen(name,"w+b");
    if( ! f ) { check_free(name); return NULL; }
    def = (tmpfile_def *) check_malloc( sizeof(tmpfile_def) );
    if( ! def ) { fclose(f); check_free(name); return NULL; }
    if( ! tmpfile_list ) atexit( delete_temp_files );
    def->handle = f;
    def->name = name;
    def->next = tmpfile_list;
    tmpfile_list = def;

    return f;
}

