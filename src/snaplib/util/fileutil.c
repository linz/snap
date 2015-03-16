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

static char *usercfg=NULL;
static char *syscfg=NULL;
static char *imgpath=NULL;
static char *imgdir=NULL;
static char *projdir=NULL;
static char *filename=NULL;
static int filenamelen=0;

typedef struct config_path_def_s
{
    struct config_path_def_s *next;
    char *path;
} config_path_def;

static config_path_def *config_dir_list=0;
static int config_dirs_set=0;

static char *filenameptr( int reqlen )
{
    if( ! filename || reqlen > filenamelen )
    {
        if( filename ) check_free(filename);
        reqlen = reqlen*2;
        if( reqlen < MAX_FILENAME_LEN ) reqlen=MAX_FILENAME_LEN;
        filename = (char *) check_malloc(reqlen+1);
        filenamelen = reqlen;
    }
    return filename;
}

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

char *build_config_filespec( char *spec, int nspec,
                             const char *dir, int pathonly, const char *config,
                             const char *name, const char *dflt_ext )
{
    int nch = 0;
    int dirlen = 0;
    char *end;
    if( dir ) 
    {
        dirlen=pathonly ? path_len(dir,0) : strlen(dir);
        if( dirlen > 0 && (dir[dirlen-1]==PATH_SEPARATOR || dir[dirlen-1]==PATH_SEPARATOR2) ) dirlen--;
        if( dirlen > 0 ) nch += dirlen + 1;
    }
    if( config ) nch += strlen(config) + 1;
    if( name ) nch += strlen(name);
    if( dflt_ext ) nch += strlen(dflt_ext);
    nch++;

    if( spec && nch > nspec ) { spec[0]=0; return spec; }
    if( ! spec )
    {
        spec=filenameptr(nch);
    }

    *spec=0;
    end=spec;
    if( dirlen > 0 )
    {
        strncpy(end,dir,dirlen);
        end += dirlen;
        *end=PATH_SEPARATOR;
        end++;
        *end=0;
    }
    if( config ) { strcpy(end,config); end += strlen(config); *end=PATH_SEPARATOR; end++; *end=0; }
    if( name ) { strcpy(end,name); }
    if( dflt_ext ) strcat(end,dflt_ext);

    return spec;
}

char *build_filespec( char *spec, int nspec,
                      const char *dir, const char *name, const char *dflt_ext )
{
    return build_config_filespec(spec,nspec,dir,0,0,name,dflt_ext);
}



/* Routine looks for the image file corresponding to the argument supplied */

#ifdef UNIX

const char *image_path()
{
    char _link[20];
    char buf[10];
    if( imgpath ) return imgpath;
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
    if ( len != -1 )
    {
        proc[len] = '\0';
        imgpath = (char *) check_malloc(strlen(proc)+1 );
        strcpy( imgpath, proc );
    }
    return imgpath;
}

const char *user_config_dir()
{
    char *homedir;
    int len;
    if( usercfg ) return usercfg;
    homedir = getenv("HOME");
    if( ! homedir ) return NULL;
    len = strlen(homedir);
    usercfg = (char *) check_malloc( len + strlen(USER_CONFIG_BASE) + 3);
    strcpy(usercfg,homedir);
    usercfg[len] = PATH_SEPARATOR;
    usercfg[len+1]='.';
    strcpy(usercfg+len+2,USER_CONFIG_BASE);
    return usercfg;
}

#else

const char *image_path()
{

    char *path=NULL;
    if( imgpath ) return imgpath;
    _get_pgmptr(&path);
    if( path ) imgpath=copy_string(path);
    else imgpath=copy_string("");
    return imgpath;
}

const char *user_config_dir()
{

    char *appdata;
    int len;
    if( usercfg ) return usercfg;
    appdata = getenv("APPDATA");
    if( ! appdata ) return NULL;
    len = strlen(appdata);
    usercfg = (char *) check_malloc( len + strlen(USER_CONFIG_BASE) + 2);
    strcpy(usercfg,appdata);
    usercfg[len] = PATH_SEPARATOR;
    strcpy(usercfg+len+1,USER_CONFIG_BASE);
    return usercfg;
}

#endif

const char *image_dir()
{
    int plen;
    const char *imgpath;
    if( imgdir ) return imgdir;
    imgpath = image_path();
    plen = path_len(imgpath,0);
    if( plen ) plen--;
    imgdir = copy_string_nch(imgpath,plen);
    return imgdir;
}

const char *system_config_dir()
{
    int len;
    const char *imgdir;
    if( syscfg ) return syscfg;
    imgdir=image_dir();
    len = strlen(imgdir) + strlen(SYS_CONFIG_BASE) + 2;
    syscfg = (char *) check_malloc(len);
    strcpy(syscfg,imgdir);
    len=strlen(syscfg);
    syscfg[len]=PATH_SEPARATOR;
    strcpy(syscfg+len+1,SYS_CONFIG_BASE);
    return syscfg;
}

const char *project_dir()
{
    return projdir;
}

static config_path_def *config_dirs()
{
    const char *snapenv;
    const char *start, *end;
    config_path_def **nextpath;
    if( config_dirs_set ) return config_dir_list;
    config_dirs_set = 1;
    nextpath = &config_dir_list;
    snapenv =  getenv(SNAPENV);
    if( snapenv )
    {
        start=snapenv;
        while( *start )
        {
            int nch;
            end=start;
            while(*end && *end != PATHENV_SEP) end++;
            nch=end-start;
            if( nch > 0 )
            {
                char *path = copy_string_nch(start,nch);
                if( file_exists(path) )
                {
                    config_path_def *psub=(config_path_def *) check_malloc( sizeof(config_path_def) );
                    psub->next = 0;
                    psub->path = path;
                    *nextpath=psub;
                    nextpath = &(psub->next);
                }
                else
                {
                    check_free(path);
                }
            }
            start = end;
            if( *start ) start++;
        }
    }
    if( file_exists(user_config_dir()))
    {
        config_path_def *psub=(config_path_def *) check_malloc( sizeof(config_path_def) );
        psub->next = 0;
        psub->path = copy_string(user_config_dir());
        *nextpath=psub;
        nextpath = &(psub->next);
    }
    if( file_exists(system_config_dir()))
    {
        config_path_def *psub=(config_path_def *) check_malloc( sizeof(config_path_def) );
        psub->next = 0;
        psub->path = copy_string(system_config_dir());
        *nextpath=psub;
        nextpath = &(psub->next);
    }
    return config_dir_list;
}

void reset_config_dirs()
{
    config_path_def *cpd;
    while( config_dir_list )
    {
        cpd=config_dir_list;
        config_dir_list=cpd->next;
        check_free(cpd);
    }
    config_dirs_set=0;
}

void set_user_config_dir( const char *cfgdir )
{
    if( usercfg ) check_free(usercfg);
    usercfg = copy_string(cfgdir);
}


void set_project_dir( const char *project_dir )
{
    if( projdir ) check_free(projdir);
    projdir=0;
    if( project_dir ) projdir=copy_string(project_dir);
}

const char *find_config_file( const char *config, const char *name, const char *dflt_ext )
{
    const char *spec;
    const char *cfg;
    config_path_def *cpd;

    for( cpd=config_dirs(); cpd; cpd=cpd->next )
    {
        cfg=cpd->path;
        spec=build_config_filespec( 0, 0, cfg, 0, config, name, dflt_ext);
        if( file_exists(spec) ) return spec;
        if( dflt_ext )
        {
            spec=build_config_filespec( 0, 0, cfg, 0, config, name, 0);
            if( file_exists(spec) ) return spec;
        }
    }

    return NULL;
}

const char *find_relative_file( const char *base, const char *name, const char *dflt_ext )
{
    const char *spec;
    spec=build_config_filespec( 0, 0, base, 1, 0, name, dflt_ext);
    if( file_exists(spec) ) return spec;

    if( dflt_ext )
    {
        spec=build_config_filespec( 0, 0, base, 1, 0, name, 0);
        if( file_exists(spec) ) return spec;
    }

    return NULL;
}

const char *find_file( const char *name, const char *dflt_ext, const char *relative, int tryopt, const char *config )
{
    const char *spec=0;
    const char *projdir = project_dir();
    if( relative )
    {
        spec = find_relative_file( relative, name, dflt_ext );
    }
    if( ! spec && projdir && (tryopt && FF_TRYPROJECT) )
    {
        spec = build_filespec(0,0,projdir,name,dflt_ext);
        if( dflt_ext && ! file_exists(spec)) spec = build_filespec(0,0,projdir,name,0);
        if( ! file_exists(spec)) spec = 0;
    }
    if( ! spec && (tryopt && FF_TRYLOCAL) )
    {
        spec = build_filespec(0,0,0,name,dflt_ext);
        if( dflt_ext && ! file_exists(spec)) spec = build_filespec(0,0,0,name,0);
        if( ! file_exists(spec)) spec = 0;
    }
    if( ! spec && config )
    {
        spec = find_config_file( config, name, dflt_ext );
    }
    return spec;
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
    if( ! f )
    {
        check_free(name);
        return NULL;
    }
    def = (tmpfile_def *) check_malloc( sizeof(tmpfile_def) );
    if( ! def )
    {
        fclose(f);
        check_free(name);
        return NULL;
    }
    if( ! tmpfile_list ) atexit( delete_temp_files );
    def->handle = f;
    def->name = name;
    def->next = tmpfile_list;
    tmpfile_list = def;

    return f;
}

