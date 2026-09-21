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
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <share.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#ifdef UNIX
#define _stat stat
#include <unistd.h>
#endif
#include <filesystem>
#include <system_error>
#include <vector>


#include "util/fileutil.h"
#include "util/chkalloc.h"
#include "util/dstring.h"
#include "util/errdef.h"

static std::optional<std::string> usercfg;
static std::optional<std::string> syscfg;
static std::optional<std::string> imgpath;
static std::optional<std::string> imgdir;
static std::optional<std::string> imgname;

typedef struct config_path_def_s
{
    struct config_path_def_s *next;
    std::string path;
} config_path_def;

static config_path_def *config_dir_list=0;
static int config_dirs_set=0;

static file_context *current_context = 0;
static file_context *context_list = 0;

#define SNAPTMP_TEMPLATE "SNAP_TMP_XXXXXX"

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

/* Check whether a file exists */

int file_exists( const std::string &file )
{
    std::error_code ec;
    return std::filesystem::exists(file, ec) ? 1 : 0;
}

int is_dir(const std::string &path)
{
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) ? 1 : 0;
}

/* std::filesystem::file_time_type doesn't convert portably to time_t
   pre-C++20 (std::chrono::clock_cast is a C++20 addition, and this
   codebase targets C++17) - stays on _stat's st_mtime rather than
   std::filesystem::last_write_time. */
time_t  file_modtime(const std::string &path)
{
    struct _stat info;
    if(_stat( path.c_str(), &info ) != 0)
        return 0;
    return info.st_mtime;
}

int  file_size(const std::string &path)
{
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<int>(sz);
}

/* Lexically collapses "." and ".." segments in path, treating either
   PATH_SEPARATOR or PATH_SEPARATOR2 as a segment boundary. Never touches
   the filesystem - doesn't resolve symlinks or check that a collapsed
   ".." segment's target actually exists. Preserves a leading separator
   (an absolute path stays absolute). A ".." with no preceding real
   segment to cancel (either because there isn't one, or because the
   preceding segment is itself "." or ".." or otherwise dot-led) is kept
   literally, since there's nothing lexically valid to collapse it against. */
static std::string normalize_path( const std::string &path )
{
    bool absolute = ! path.empty() &&
        (path.front()==PATH_SEPARATOR || path.front()==PATH_SEPARATOR2);

    std::vector<std::string> segments;
    size_t pos = 0;
    while( pos <= path.size() )
    {
        size_t next = pos;
        while( next < path.size() && path[next] != PATH_SEPARATOR && path[next] != PATH_SEPARATOR2 ) next++;
        std::string segment = path.substr(pos,next-pos);
        if( segment == "." )
        {
            /* drop */
        }
        else if( segment == ".." && ! segments.empty() && segments.back()[0] != '.' )
        {
            segments.pop_back();
        }
        else if( ! segment.empty() )
        {
            segments.push_back(std::move(segment));
        }
        if( next >= path.size() ) break;
        pos = next+1;
    }

    std::string result;
    if( absolute ) result += PATH_SEPARATOR;
    for( size_t i=0; i<segments.size(); i++ )
    {
        if( i>0 ) result += PATH_SEPARATOR;
        result += segments[i];
    }
    return result;
}

std::string build_config_filespec( const std::string &dir, const bool pathonly, const std::string &config,
                                   const std::string &name, const std::string &dflt_ext )
{
    std::string dirpart;
    if( ! dir.empty() )
    {
        int dirlen = pathonly ? path_len(dir.c_str(),0) : (int) dir.size();
        if( dirlen > 0 && (dir[dirlen-1]==PATH_SEPARATOR || dir[dirlen-1]==PATH_SEPARATOR2) ) dirlen--;
        if( dirlen > 0 ) dirpart = dir.substr(0,dirlen);
    }

    std::string spec;
    if( ! dirpart.empty() ) { spec += dirpart; spec += PATH_SEPARATOR; }
    if( ! config.empty() ) { spec += config; spec += PATH_SEPARATOR; }
    if( ! name.empty() ) spec += name;
    if( ! dflt_ext.empty() ) spec += dflt_ext;

    return normalize_path(spec);
}

std::string build_filespec( const std::string &dir, const std::string &name, const std::string &dflt_ext )
{
    return build_config_filespec(dir,false,"",name,dflt_ext);
}




/* Routine looks for the image file corresponding to the argument supplied */

#ifdef UNIX

std::optional<std::string> image_path()
{
    if( imgpath ) return imgpath;
    std::string link = "/proc/" + std::to_string(getpid());
#if defined(__linux) || defined(linux)
    link += "/exe";
#endif
#if defined(sun) || defined(__sun)
    link += "/path/a.out";
#endif
#if defined(__bsdi__)
    link += "/file";
#endif
    std::string proc(512, '\0');
    ssize_t len = readlink( link.c_str(), proc.data(), proc.size() );
    if ( len != -1 )
    {
        proc.resize(len);
        imgpath = proc;
    }
    return imgpath;
}

std::optional<std::string> user_config_dir()
{
    if( usercfg ) return usercfg;
    const char *homedir = getenv("HOME");
    if( ! homedir ) return std::nullopt;
    usercfg = std::string(homedir) + PATH_SEPARATOR + "." + USER_CONFIG_BASE;
    return usercfg;
}

#else

std::optional<std::string> image_path()
{
    if( imgpath ) return imgpath;
    char *path=NULL;
    _get_pgmptr(&path);
    imgpath = path ? std::string(path) : std::string();
    return imgpath;
}

std::optional<std::string> user_config_dir()
{
    if( usercfg ) return usercfg;
    const char *appdata = getenv("APPDATA");
    if( ! appdata ) return std::nullopt;
    usercfg = std::string(appdata) + PATH_SEPARATOR + USER_CONFIG_BASE;
    return usercfg;
}

#endif

std::string image_name()
{
    if( imgname ) return *imgname;
    std::filesystem::path path( image_path().value_or("") );
    imgname = path.stem().string();
    return *imgname;
}

std::string image_dir()
{
    if( imgdir ) return *imgdir;
    std::filesystem::path path( image_path().value_or("") );
    imgdir = path.parent_path().string();
    return *imgdir;
}

std::string system_config_dir()
{
    if( syscfg ) return *syscfg;
    syscfg = image_dir() + PATH_SEPARATOR + SYS_CONFIG_BASE;
    return *syscfg;
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
                std::string path(start,end-start);
                if( ! path.empty() && file_exists(path) )
                {
                    config_path_def *psub = new config_path_def{ nullptr, path };
                    *nextpath=psub;
                    nextpath = &(psub->next);
                }
            }
            start = end;
            if( *start ) start++;
        }
    }
    if( auto userdir = user_config_dir(); userdir && file_exists(*userdir))
    {
        config_path_def *psub = new config_path_def{ nullptr, *userdir };
        *nextpath=psub;
        nextpath = &(psub->next);
    }
    if( file_exists(system_config_dir()))
    {
        config_path_def *psub = new config_path_def{ nullptr, system_config_dir() };
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
        delete cpd;
    }
    config_dirs_set=0;
}

void set_user_config_dir( const std::string &cfgdir )
{
    usercfg = cfgdir;
}

void push_file_context( const std::string &context_dir )
{
    file_context *context;
    for( context=context_list; context; context=context->next )
    {
        if( context->parent == current_context && context->dir == context_dir )
        {
            current_context=context;
            return;
        }
    }
    context = new file_context{ context_dir, std::nullopt, current_context, context_list };
    context_list=context;
    current_context = context;
}

void pop_file_context()
{
    // Don't simply delete current context as it may be referenced later, eg in 
    if( current_context )
    {
        current_context = current_context->parent;
    }
}

file_context *current_file_context()
{
    return current_context;
}

file_context *set_file_context( file_context *new_context )
{
    file_context *saved = current_context;
    current_context = new_context;
    return saved;
}

void free_file_contexts()
{
    while( context_list )
    {
        file_context *next = context_list->next;
        delete context_list;
        context_list = next;
    }
}

std::string portable_path( const std::string &path )
{
    std::string result = path;
    for( char &c : result )
    {
        if( c == PATH_SEPARATOR ) c = '/';
    }
    return result;
}

void dump_filepath( const char *path, FILE *f )
{
    if( ! path )
    {
        dump_string_c( nullptr, f );
        return;
    }
    dump_string_c( portable_path(path).c_str(), f );
}

void dump_filepath( const std::optional<std::string> &path, FILE *f )
{
    if( ! path )
    {
        dump_string( std::nullopt, f );
        return;
    }
    dump_string( portable_path(*path), f );
}

// context_definition/recreate_context serialize a chain of relative directory
// paths to and from a single string. A doubled separator marks the boundary
// between path segments - a real relative path never contains two consecutive
// separators, so that's a safe, unambiguous marker.
//
// This used to double up PATH_SEPARATOR for that marker: '\\' on Windows,
// '/' on Linux. relative_filename's result also carries PATH_SEPARATOR
// internally (std::filesystem::path::string() renders using the platform's
// native separator). Both make a context string written on one platform
// unparseable on the other. Worse, it's not even a loud failure: POSIX
// treats '\\' as a literal filename character, not a separator, so a
// Windows-written reldir silently resolves to the wrong path on Linux instead
// of failing.
//
// Fixed by always using '/' here, on both platforms, for the marker and for
// every separator within each reldir's own content (via portable_path).
// std::filesystem::path (used by relative_filename/absolute_filename)
// accepts '/' as a valid separator on both platforms - unlike '\\' on POSIX.
const char *context_definition(file_context *context)
{
    int nch=1;
    for( file_context *child=context; child->parent; child=child->parent )
    {
        if( ! child->reldir )
        {
            child->reldir = portable_path( relative_filename(child->dir,child->parent->dir) );
        }
        nch += child->reldir->size()+2;
    }
    char *context_def = (char *) check_malloc(nch);
    char *endptr=context_def+nch-1;
    *endptr = 0;
    for( file_context *child=context; child->parent; child=child->parent )
    {
        nch=child->reldir->size();
        endptr -= (nch+2);
        *endptr='/';
        *(endptr+1)='/';
        strncpy(endptr+2,child->reldir->c_str(),nch);
    }
    return context_def;
}


file_context *recreate_context(  const char *context_def )
{
    file_context *saved_context=current_context;
    file_context *context=current_context;
    if( ! context )
    {
        return 0;
    }
    while( context->parent ) context=context->parent;
    if( ! context_def || ! *context_def ) return context;
    set_file_context( context );
    while( *context_def )
    {
        const char *start=context_def;
        const char *end=context_def;
        while( *end && ! (*end == '/' && *(end+1) == '/')) end++;
        std::string reldir(start, end-start);
        std::string absdir = absolute_filename(reldir,context->dir);
        push_file_context(absdir);
        context=current_context;
        if( ! context->reldir ) context->reldir=reldir;
        context_def = end;
        if( *context_def == '/' ) context_def += 2;
    }
    set_file_context(saved_context);
    return context;
}

std::string relative_filename( const std::string &filepath, const std::string &basedir )
{
    try
    {
        std::filesystem::path fp(filepath);
        std::filesystem::path bp(basedir);
        // boost::filesystem::relative returns an empty path when either
        // input is empty; std::filesystem::relative instead returns "."
        // when the two (both-empty) paths compare equal. context_definition
        // relies on an empty reldir producing no characters, so that
        // difference must be preserved here.
        if( fp.empty() || bp.empty() ) {
            return "";
        }
        auto relpath=std::filesystem::relative( fp, bp );
        return relpath.string();
    }
    catch (...)
    {
        return filepath;
    }
}

std::string absolute_filename( const std::string &relname, const std::string &basedir )
{
    try
    {
        std::filesystem::path rp(relname);
        std::filesystem::path bp(basedir);
        // std::filesystem::absolute has no (path, base) overload - it only
        // resolves against the process's current directory - so resolve
        // basedir against the current directory ourselves when it isn't
        // already absolute. std::filesystem::absolute() throws on an empty
        // path rather than treating it as the current directory, so that
        // case is handled explicitly too.
        if( bp.empty() ) {
            bp = std::filesystem::current_path();
        }
        else if( ! bp.is_absolute() ) {
            bp = std::filesystem::absolute(bp);
        }
        std::filesystem::path relpath;
        if( rp.empty() ) {
            relpath = bp;
        }
        else if( rp.is_absolute() ) {
            relpath = rp;
        }
        else {
            relpath = bp / rp;
        }
        return relpath.string();
    }
    catch (...)
    {
        return relname;
    }
}

std::optional<std::string> find_config_file( const std::string &config, const std::string &name, const std::string &dflt_ext )
{
    for( config_path_def *cpd=config_dirs(); cpd; cpd=cpd->next )
    {
        std::string spec=build_config_filespec( cpd->path, false, config, name, dflt_ext);
        if( file_exists(spec) ) return spec;
        if( ! dflt_ext.empty() )
        {
            spec=build_config_filespec( cpd->path, false, config, name, "");
            if( file_exists(spec) ) return spec;
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_relative_file( const std::string &base, const std::string &name, const std::string &dflt_ext )
{
    bool pathonly = file_exists(base) && ! is_dir(base);

    std::string spec=build_config_filespec( base, pathonly, "", name, dflt_ext);
    if( file_exists(spec) ) return spec;

    if( ! dflt_ext.empty() )
    {
        spec=build_config_filespec( base, pathonly, "", name, "");
        if( file_exists(spec) ) return spec;
    }

    return std::nullopt;
}

std::optional<std::string> find_file( const std::string &name, const std::string &dflt_ext, const std::optional<std::string> &base, const FindFileOption tryopt, const std::string &config )
{
    std::optional<std::string> spec;
    if( base )
    {
        spec = find_relative_file( *base, name, dflt_ext );
    }
    if( ! spec && current_context && (tryopt & FF_TRYPROJECT) )
    {
        for( file_context *context = current_context; context && ! spec; context=context->parent )
        {
            std::string trySpec = build_filespec(context->dir,name,dflt_ext);
            if( ! dflt_ext.empty() && ! file_exists(trySpec)) trySpec = build_filespec(context->dir,name,"");
            if( file_exists(trySpec)) spec = trySpec;
        }
    }
    if( ! spec && (tryopt & FF_TRYLOCAL) )
    {
        std::string trySpec = build_filespec("",name,dflt_ext);
        if( ! dflt_ext.empty() && ! file_exists(trySpec)) trySpec = build_filespec("",name,"");
        if( file_exists(trySpec)) spec = trySpec;
    }
    if( ! spec && ! config.empty() )
    {
        spec = find_config_file( config, name, dflt_ext );
    }
    return spec;
}

#ifdef _WIN32
FILE *snaptmpfile()
{
    char tmpdir[MAX_PATH];
    char tmpname[MAX_PATH];
    if( ! GetTempPathA( MAX_PATH, tmpdir ) ) return NULL;
    if( ! GetTempFileNameA( tmpdir, "snp", 0, tmpname ) ) return NULL;
    int fd = _sopen( tmpname, _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY | _O_TEMPORARY,
                      _SH_DENYRW, _S_IREAD | _S_IWRITE );
    if( fd == -1 ) return NULL;
    return _fdopen( fd, "w+b" );
}
#endif

int skip_utf8_bom( FILE *f )
{
    unsigned char bom[3];
    int nchar;
    if( ftell64(f) != 0 ) return 1;
    nchar=fread(bom,1,3,f);
    if( nchar >= 2 && ( (bom[0] == '\xFE' && bom[1] == '\xFF') || (bom[0] == '\xFF' && bom[1] == '\xFE') ) )
    {
        return 0;
    }
    else if ( nchar < 3 || bom[0] != '\xEF' || bom[1] != '\xBB' || bom[2] != '\xBF' )
    {
        fseek(f,0L,SEEK_SET);
    }
    return 1;
}

