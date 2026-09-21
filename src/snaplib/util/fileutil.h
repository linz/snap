
#ifndef _FILEUTIL_H
#define _FILEUTIL_H
/*
   $Log: fileutil.h,v $
   Revision 1.2  2004/04/22 02:35:25  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:01:26  CHRIS
   Initial revision

*/

/* fileutil.h: routines to assist file management */

#include <time.h>
#include <string>
#include <optional>

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

typedef struct file_context_s
{
   std::string dir;
   std::optional<std::string> reldir;  // Directory relative to parent - used to persist context with context_definition()
   struct file_context_s *parent;
   struct file_context_s *next;  // Used for keeping list of contexts to search/clean.
} file_context;

int path_len( const char *base, int want_name );
int file_exists( const std::string &file );
int is_dir( const std::string &path );
int file_size( const std::string &path );
time_t file_modtime( const std::string &path );

/// Composes dir, name, and dflt_ext into a single path, with no config subdirectory
/// and dir taken literally (never as a filename to extract a directory from).
/// Equivalent to build_config_filespec(dir,false,"",name,dflt_ext) - see there for the
/// full behaviour (path composition and normalization).
///
/// e.g. build_filespec("/a/b","coordsys",".def") returns "/a/b/coordsys.def".
std::string build_filespec(
    const std::string &dir,       ///< directory to prefix the result with, or "" for none
    const std::string &name,      ///< base filename
    const std::string &dflt_ext );///< text (typically an extension, incl. the leading '.')
                                   ///< appended directly after name, or "" for none

/// Composes a file path from a directory, an optional config subdirectory, a name, and
/// a default extension, then normalizes it by lexically collapsing any "./" and "../"
/// segments (a crude, string-only normalization - it never touches the filesystem, so
/// it doesn't resolve symlinks or check the segments it removes actually exist).
///
/// e.g. build_config_filespec("/a/b/c.cfg", true, "config", "coordsys", ".def") returns
/// "/a/b/config/coordsys.def" - pathonly extracts just the directory "/a/b" from the
/// filename-shaped dir argument before the config subdirectory and name are appended.
///
/// e.g. build_config_filespec("/a/b/../c", false, "", "coordsys", ".def") returns
/// "/a/c/coordsys.def" - the "b/../" segment is lexically collapsed away.
std::string build_config_filespec(
    const std::string &dir,    ///< directory to prefix the result with (or, if pathonly is
                                ///< true, a filename whose directory component is used
                                ///< instead), or "" for none
    bool pathonly,              ///< if true, dir is treated as a filename and only its
                                ///< directory component (path_len(dir,0)) is used
    const std::string &config, ///< config subdirectory of dir to insert before name, or
                                ///< "" for none
    const std::string &name,    ///< base filename
    const std::string &dflt_ext );///< text (typically an extension, incl. the leading '.')
                                   ///< appended directly after name, or "" for none


std::optional<std::string> image_path();
std::string image_dir();
std::string image_name();
std::string system_config_dir();
std::optional<std::string> user_config_dir();

/* Reset config directories - use if environment variable is redefined */
void reset_config_dirs();

/* Override the default user environment */
void set_user_config_dir( const std::string &cfgdir );

/* Set the project dir, that can be included in the find_file search.  Supply the name
   of the project file - the path will be extracted .*/

void push_file_context( const std::string &context_dir );
void pop_file_context();
file_context *current_file_context();
file_context *set_file_context( file_context *new_context );
void free_file_contexts();
const char *context_definition(file_context *context);
file_context *recreate_context( const  char *context_def );
/* relative_filename and absolute_filename both operate lexically on the path
   strings - neither requires filepath, relname, or basedir to exist on disk. */

/* Expresses filepath relative to basedir - e.g. relative_filename("/a/b/c","/a/b")
   returns "c". Returns "." if the two paths resolve to the same location. Falls
   back to returning filepath unchanged if the paths can't be compared
   (e.g. on Windows, when they're on different drives). */
std::string relative_filename( const std::string &filepath, const std::string &basedir );

/* Resolves relname to an absolute path. If relname is already absolute, it is
   returned unchanged; otherwise it is composed onto basedir. basedir is
   resolved against the current working directory first if it is itself
   relative (including empty). Falls back to returning relname unchanged if
   resolution fails. */
std::string absolute_filename( const std::string &relname, const std::string &basedir );

/* Returns path with every PATH_SEPARATOR replaced by '/', for writing a path
   into a portable, cross-platform file format. */
std::string portable_path( const std::string &path );

/* Writes path to disk via dump_string_c (util/dstring.h), normalized via
   portable_path first. Use in place of dump_string_c for any path being written
   to a .bin file. */
void dump_filepath( const char *path, FILE *f );
/* std::optional<std::string> overload, for a genuinely-absent path field -
   writes std::nullopt exactly as dump_filepath(nullptr, f) does. */
void dump_filepath( const std::optional<std::string> &path, FILE *f );

/// Searches the user's then the system's configuration directories (each in turn, with
/// and without dflt_ext) for name inside the configdir subdirectory. Returns nullopt if
/// not found in any of them.
///
/// e.g. find_config_file("coordsys","nzgd2000",".def") looks for
/// "<user_config_dir>/coordsys/nzgd2000.def", then (without dflt_ext)
/// "<user_config_dir>/coordsys/nzgd2000", then the same two under
/// "<system_config_dir>/coordsys/...".
std::optional<std::string> find_config_file(
    const std::string &configdir,  ///< config subdirectory to search within, e.g. "coordsys"
    const std::string &name,       ///< base filename to search for
    const std::string &dflt_ext ); ///< extension (incl. the leading '.') to try first, or ""

/// Searches for a file with the given name and extension in the directory containing
/// base (base is treated as a filename, not a directory, unless it doesn't exist as a
/// file - see is_dir/file_exists in build_config_filespec's pathonly), trying with and
/// then without dflt_ext. Returns nullopt if not found.
std::optional<std::string> find_relative_file(
    const std::string &base,       ///< a file (or directory) to search relative to
    const std::string &name,       ///< base filename to search for
    const std::string &dflt_ext ); ///< extension (incl. the leading '.') to try first, or ""

/// General purpose file search, trying up to four strategies in turn until one succeeds:
/// relative to base (via find_relative_file, if base is present), the current project
/// context stack (FF_TRYPROJECT), the current directory (FF_TRYLOCAL), then the
/// configdir subdirectory of the user's/system's config directories (via
/// find_config_file, if configdir is non-empty). Returns nullopt if none succeed.
enum FindFileOption : unsigned
{
    FF_TRYNONE    = 0,
    FF_TRYLOCAL   = 1,
    FF_TRYPROJECT = 2,
    FF_TRYALL     = FF_TRYLOCAL | FF_TRYPROJECT
};

constexpr FindFileOption operator|( FindFileOption a, FindFileOption b )
{
    return static_cast<FindFileOption>( static_cast<unsigned>(a) | static_cast<unsigned>(b) );
}
constexpr FindFileOption operator&( FindFileOption a, FindFileOption b )
{
    return static_cast<FindFileOption>( static_cast<unsigned>(a) & static_cast<unsigned>(b) );
}
constexpr FindFileOption& operator|=( FindFileOption &a, FindFileOption b )
{
    a = a | b;
    return a;
}

std::optional<std::string> find_file(
    const std::string &name,               ///< base filename to search for
    const std::string &dflt_ext,           ///< extension (incl. the leading '.') to try first, or ""
    const std::optional<std::string> &base,///< file/directory to search relative to, or nullopt
                                            ///< to skip that search strategy entirely - nullopt
                                            ///< is not equivalent to a present-but-empty base
                                            ///< (e.g. get_config_directory's "blank" result),
                                            ///< which still triggers a relative-file search
    FindFileOption tryopt,                 ///< bitwise-or of FF_TRYLOCAL/FF_TRYPROJECT, or FF_TRYNONE
    const std::string &configdir );         ///< config subdirectory to search within, or ""
                                             ///< to skip that search strategy entirely

/* Create a temporary file, and set up a handler to delete it when the program terminates */

#ifdef _WIN32
/* The standard tmpfile() creates the file in the root of the current drive,
   which a non-administrator Windows account often cannot write to. Use a
   proper scratch-file implementation based on the actual temp directory
   (TMP/TEMP/USERPROFILE) instead. */
FILE *snaptmpfile();
#else
#define snaptmpfile tmpfile
#endif

/* Skip unicode BOM (byte order marker).  Assumes file pointer is set to beginning of file */
/* Returns 1 if successful (UTF8 or no BOM), 0 if UTF16 BOM at start of file */
/* Returns 1 if file pointer is not set to the beginning of the file */

int skip_utf8_bom(FILE *f);

#endif
