#include "snapconfig.h"
/* fileutil.c:  Provides some simple file management routines */

/*
   $Log: fileutil.c,v $
   Revision 1.3  2004/04/22 02:35:24  ccrook
   Setting up to support linux compilation (x86 architecture)

   Revision 1.1  1995/12/22 19:01:10  CHRIS
   Initial revision

*/

#include <sys/types.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <utility>
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
#include <algorithm>
#include <filesystem>
#include <forward_list>
#include <system_error>
#include <vector>
#include "util/fileutil.h"
#include "util/dstring.h"
#include "util/errdef.h"

static std::optional<std::string> usercfg;
static std::optional<std::string> syscfg;
static std::optional<std::string> imgpath;
static std::optional<std::string> imgdir;
static std::optional<std::string> imgname;

static std::optional<std::forward_list<std::string>> config_dir_list;

static file_context* current_context = nullptr;
static file_context* context_list = nullptr;

#define SNAPTMP_TEMPLATE "SNAP_TMP_XXXXXX"

std::string native_path(const std::string_view path) {
    std::string result(path);
    for (char& c : result) {
        if (c == PATH_SEPARATOR2) {
            c = PATH_SEPARATOR;
        }
    }
    return result;
}

int path_exists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) ? 1 : 0;
}

int is_dir(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) ? 1 : 0;
}

/* std::filesystem::file_time_type doesn't convert portably to time_t
   pre-C++20 (std::chrono::clock_cast is a C++20 addition, and this
   codebase targets C++17) - stays on _stat's st_mtime rather than
   std::filesystem::last_write_time. */
std::time_t file_modtime(const std::string& path) {
    struct _stat info;
    if (_stat(path.c_str(), &info) != 0) {
        return 0;
    }
    return info.st_mtime;
}

int file_size(const std::string& path) {
    std::error_code ec;
    auto sz = std::filesystem::file_size(path, ec);
    return ec ? 0 : static_cast<int>(sz);
}

/* Lexically collapses "." and ".." segments in path, treating either
   PATH_SEPARATOR or PATH_SEPARATOR2 as a segment boundary. Never touches
   the filesystem - doesn't resolve symlinks or check that a collapsed
   ".." segment's target actually exists. Every separator in the result is
   PATH_SEPARATOR, and the number of leading separators is kept (an absolute
   path stays absolute, and the two leading separators of a Windows UNC path
   are kept). A ".." with no preceding real
   segment to cancel (either because there isn't one, or because the
   preceding segment is itself "." or ".." or otherwise dot-led) is kept
   literally, since there's nothing lexically valid to collapse it against. */
static std::string normalize_path(const std::string& path) {
    const std::string separators{PATH_SEPARATOR, PATH_SEPARATOR2};
    const std::size_t leading = std::min(path.find_first_not_of(separators), path.size());

    std::vector<std::string> segments;
    std::size_t pos = leading;
    while (pos <= path.size()) {
        std::size_t next = pos;
        while (next < path.size() && path[next] != PATH_SEPARATOR && path[next] != PATH_SEPARATOR2) {
            next++;
        }
        std::string segment = path.substr(pos, next - pos);
        if (segment == ".") {
            /* drop */
        } else if (segment == ".." && !segments.empty() && segments.back()[0] != '.') {
            segments.pop_back();
        } else if (!segment.empty()) {
            segments.push_back(std::move(segment));
        }
        if (next >= path.size()) {
            break;
        }
        pos = next + 1;
    }

    std::string result(leading, PATH_SEPARATOR);
    for (std::size_t i = 0; i < segments.size(); i++) {
        if (i > 0) {
            result += PATH_SEPARATOR;
        }
        result += segments[i];
    }
    return result;
}

std::string build_config_filespec(const std::string& dir, const bool pathonly, const std::string& config,
                                  const std::string& name, const std::string& dflt_ext) {
    std::string dirpart = pathonly ? std::filesystem::path(native_path(dir)).parent_path().string() : dir;
    if (!dirpart.empty() && (dirpart.back() == PATH_SEPARATOR || dirpart.back() == PATH_SEPARATOR2)) {
        dirpart.pop_back();
    }

    std::string spec;
    if (!dirpart.empty()) {
        spec += dirpart;
        spec += PATH_SEPARATOR;
    }
    if (!config.empty()) {
        spec += config;
        spec += PATH_SEPARATOR;
    }
    if (!name.empty()) {
        spec += name;
    }
    if (!dflt_ext.empty()) {
        spec += dflt_ext;
    }

    return normalize_path(spec);
}

std::string build_filespec(const std::string& dir, const std::string& name, const std::string& dflt_ext) {
    return build_config_filespec(dir, false, "", name, dflt_ext);
}

/* Routine looks for the image file corresponding to the argument supplied */

#ifdef UNIX

std::optional<std::string> image_path() {
    if (imgpath) {
        return imgpath;
    }
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
    const ssize_t len = readlink(link.c_str(), proc.data(), proc.size());
    if (len != -1) {
        proc.resize(len);
        imgpath = proc;
    }
    return imgpath;
}

std::optional<std::string> user_config_dir() {
    if (usercfg) {
        return usercfg;
    }
    const char* homedir = std::getenv("HOME");
    if (!homedir) {
        return std::nullopt;
    }
    usercfg = std::string(homedir) + PATH_SEPARATOR + "." + USER_CONFIG_BASE;
    return usercfg;
}

#else

std::optional<std::string> image_path() {
    if (imgpath) {
        return imgpath;
    }
    // A path that does not fit in the buffer is cut short, and the length that is returned is then the size of
    // the buffer, so the buffer is doubled until the path leaves room in it.
    std::string path(MAX_PATH, '\0');
    while (true) {
        const DWORD length = GetModuleFileNameA(nullptr, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) {
            path.clear();
            break;
        }
        if (length < path.size()) {
            path.resize(length);
            break;
        }
        path.resize(path.size() * 2);
    }
    imgpath = path;
    return imgpath;
}

std::optional<std::string> user_config_dir() {
    if (usercfg)
        return usercfg;
    const char* appdata = std::getenv("APPDATA");
    if (!appdata)
        return std::nullopt;
    usercfg = std::string(appdata) + PATH_SEPARATOR + USER_CONFIG_BASE;
    return usercfg;
}

#endif

std::string image_name() {
    if (imgname) {
        return *imgname;
    }
    const std::filesystem::path path(image_path().value_or(""));
    imgname = path.stem().string();
    return *imgname;
}

std::string image_dir() {
    if (imgdir) {
        return *imgdir;
    }
    const std::filesystem::path path(image_path().value_or(""));
    imgdir = path.parent_path().string();
    return *imgdir;
}

std::string system_config_dir() {
    if (syscfg) {
        return *syscfg;
    }
    syscfg = image_dir() + PATH_SEPARATOR + SYS_CONFIG_BASE;
    return *syscfg;
}

/// The directories searched for config files, in search order: the SNAPENV entries, then the
/// user's config directory, then the system's. Only those that exist when the list is first
/// built are kept. Built on the first call and kept until reset_config_dirs().
static const std::forward_list<std::string>& config_dirs() {
    if (config_dir_list) {
        return *config_dir_list;
    }
    config_dir_list.emplace();
    auto last = config_dir_list->before_begin();

    if (const char* snapenv = std::getenv(SNAPENV)) {
        std::string_view remaining(snapenv);
        while (!remaining.empty()) {
            const std::size_t end = std::min(remaining.find(PATHENV_SEP), remaining.size());
            const std::string path(remaining.substr(0, end));
            if (!path.empty() && path_exists(path)) {
                last = config_dir_list->insert_after(last, path);
            }
            remaining.remove_prefix(std::min(end + 1, remaining.size()));
        }
    }
    if (const auto userdir = user_config_dir(); userdir && path_exists(*userdir)) {
        last = config_dir_list->insert_after(last, *userdir);
    }
    if (const std::string systemdir = system_config_dir(); path_exists(systemdir)) {
        config_dir_list->insert_after(last, systemdir);
    }
    return *config_dir_list;
}

void reset_config_dirs() {
    config_dir_list.reset();
}

void push_file_context(const std::string& context_dir) {
    file_context* context;
    for (context = context_list; context; context = context->next) {
        if (context->parent == current_context && context->dir == context_dir) {
            current_context = context;
            return;
        }
    }
    context = new file_context{context_dir, std::nullopt, current_context, context_list};
    context_list = context;
    current_context = context;
}

void pop_file_context() {
    // Don't simply delete current context as it may be referenced later, eg in
    if (current_context) {
        current_context = current_context->parent;
    }
}

file_context* current_file_context() {
    return current_context;
}

file_context* set_file_context(file_context* new_context) {
    file_context* saved = current_context;
    current_context = new_context;
    return saved;
}

void free_file_contexts() {
    while (context_list) {
        file_context* next = context_list->next;
        delete context_list;
        context_list = next;
    }
}

std::string portable_path(const std::string& path) {
    std::string result = path;
    for (char& c : result) {
        if (c == PATH_SEPARATOR) {
            c = '/';
        }
    }
    return result;
}

void dump_filepath(const std::string& path, std::FILE* f) {
    dump_string(portable_path(path), f);
}

void dump_filepath(const std::optional<std::string>& path, std::FILE* f) {
    if (!path) {
        dump_string(std::nullopt, f);
        return;
    }
    dump_string(portable_path(*path), f);
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
std::string context_definition(file_context* context) {
    std::string context_def;
    for (file_context* child = context; child->parent; child = child->parent) {
        if (!child->reldir) {
            child->reldir = portable_path(relative_filename(child->dir, child->parent->dir));
        }
        context_def.insert(0, "//" + *child->reldir);
    }
    return context_def;
}

file_context* recreate_context(const std::string_view context_def) {
    file_context* saved_context = current_context;
    file_context* context = current_context;
    if (!context) {
        return nullptr;
    }
    while (context->parent) {
        context = context->parent;
    }
    if (context_def.empty()) {
        return context;
    }
    set_file_context(context);
    std::size_t start = 0;
    while (start < context_def.size()) {
        const std::size_t end = std::min(context_def.find("//", start), context_def.size());
        const std::string reldir(context_def.substr(start, end - start));
        const std::string absdir = absolute_filename(reldir, context->dir);
        push_file_context(absdir);
        context = current_context;
        if (!context->reldir) {
            context->reldir = reldir;
        }
        start = end + 2;
    }
    set_file_context(saved_context);
    return context;
}

std::string relative_filename(const std::string& filepath, const std::string& basedir) {
    try {
        const std::filesystem::path fp(filepath);
        const std::filesystem::path bp(basedir);
        // boost::filesystem::relative returns an empty path when either
        // input is empty; std::filesystem::relative instead returns "."
        // when the two (both-empty) paths compare equal. context_definition
        // relies on an empty reldir producing no characters, so that
        // difference must be preserved here.
        if (fp.empty() || bp.empty()) {
            return "";
        }
        auto relpath = std::filesystem::relative(fp, bp);
        return relpath.string();
    } catch (...) {
        return filepath;
    }
}

std::string absolute_filename(const std::string& relname, const std::string& basedir) {
    try {
        const std::filesystem::path rp(relname);
        std::filesystem::path bp(basedir);
        // std::filesystem::absolute has no (path, base) overload - it only
        // resolves against the process's current directory - so resolve
        // basedir against the current directory ourselves when it isn't
        // already absolute. std::filesystem::absolute() throws on an empty
        // path rather than treating it as the current directory, so that
        // case is handled explicitly too.
        if (bp.empty()) {
            bp = std::filesystem::current_path();
        } else if (!bp.is_absolute()) {
            bp = std::filesystem::absolute(bp);
        }
        std::filesystem::path relpath;
        if (rp.empty()) {
            relpath = bp;
        } else if (rp.is_absolute()) {
            relpath = rp;
        } else {
            relpath = bp / rp;
        }
        return relpath.string();
    } catch (...) {
        return relname;
    }
}

std::optional<std::string> find_config_file(const std::string& config, const std::string& name,
                                            const std::string& dflt_ext) {
    for (const std::string& dir : config_dirs()) {
        std::string spec = build_config_filespec(dir, false, config, name, dflt_ext);
        if (path_exists(spec)) {
            return spec;
        }
        if (!dflt_ext.empty()) {
            spec = build_config_filespec(dir, false, config, name, "");
            if (path_exists(spec)) {
                return spec;
            }
        }
    }
    return std::nullopt;
}

std::optional<std::string> find_relative_file(const std::string& base, const std::string& name,
                                              const std::string& dflt_ext) {
    const bool pathonly = path_exists(base) && !is_dir(base);

    std::string spec = build_config_filespec(base, pathonly, "", name, dflt_ext);
    if (path_exists(spec)) {
        return spec;
    }

    if (!dflt_ext.empty()) {
        spec = build_config_filespec(base, pathonly, "", name, "");
        if (path_exists(spec)) {
            return spec;
        }
    }

    return std::nullopt;
}

std::optional<std::string> find_file(const std::string& name, const std::string& dflt_ext,
                                     const std::optional<std::string>& base, const FindFileOption tryopt,
                                     const std::string& config) {
    std::optional<std::string> spec;
    if (base) {
        spec = find_relative_file(*base, name, dflt_ext);
    }
    if (!spec && current_context && (tryopt & FF_TRYPROJECT)) {
        for (file_context* context = current_context; context && !spec; context = context->parent) {
            std::string trySpec = build_filespec(context->dir, name, dflt_ext);
            if (!dflt_ext.empty() && !path_exists(trySpec)) {
                trySpec = build_filespec(context->dir, name, "");
            }
            if (path_exists(trySpec)) {
                spec = trySpec;
            }
        }
    }
    if (!spec && (tryopt & FF_TRYLOCAL)) {
        std::string trySpec = build_filespec("", name, dflt_ext);
        if (!dflt_ext.empty() && !path_exists(trySpec)) {
            trySpec = build_filespec("", name, "");
        }
        if (path_exists(trySpec)) {
            spec = trySpec;
        }
    }
    if (!spec && !config.empty()) {
        spec = find_config_file(config, name, dflt_ext);
    }
    return spec;
}

#ifdef _WIN32
std::FILE* snaptmpfile() {
    char tmpdir[MAX_PATH];
    char tmpname[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmpdir))
        return nullptr;
    if (!GetTempFileNameA(tmpdir, "snp", 0, tmpname))
        return nullptr;
    int fd =
        _sopen(tmpname, _O_RDWR | _O_CREAT | _O_TRUNC | _O_BINARY | _O_TEMPORARY, _SH_DENYRW, _S_IREAD | _S_IWRITE);
    if (fd == -1)
        return nullptr;
    return _fdopen(fd, "w+b");
}
#endif

/// What the first bytes of a file are, as found by find_byte_order_mark().
enum class ByteOrderMark {
    None,   ///< not a byte order mark
    Utf8,   ///< a complete UTF-8 mark
    Utf16,  ///< a UTF-16 mark, big or little endian
};

/// Finds whether bytes start with a byte order mark. A prefix of a UTF-8 mark that the file ends in, with fewer than
/// three bytes, is not a mark.
static ByteOrderMark find_byte_order_mark(const unsigned char (&bytes)[3], const std::size_t count) {
    if (count >= 2 && ((bytes[0] == 0xFE && bytes[1] == 0xFF) || (bytes[0] == 0xFF && bytes[1] == 0xFE))) {
        return ByteOrderMark::Utf16;
    }
    if (count == 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return ByteOrderMark::Utf8;
    }
    return ByteOrderMark::None;
}

int skip_utf8_bom(std::FILE* f) {
    if (ftell64(f) != 0) {
        return 1;
    }
    unsigned char bytes[3] = {0, 0, 0};
    const std::size_t count = std::fread(bytes, 1, 3, f);
    const ByteOrderMark mark = find_byte_order_mark(bytes, count);
    if (mark == ByteOrderMark::Utf16) {
        return 0;
    }
    if (mark != ByteOrderMark::Utf8) {
        std::fseek(f, 0L, SEEK_SET);
    }
    return 1;
}

int skip_utf8_bom(std::istream& f) {
    // A first character that cannot start a byte order mark is left unread, so the stream is not read and
    // rewound. Rewinding does nothing for some streams, such as standard input in a MinGW program.
    const int first = f.peek();
    f.clear();
    if (first != 0xEF && first != 0xFE && first != 0xFF) {
        return 1;
    }
    if (f.tellg() != std::streampos(0)) {
        return 1;
    }
    unsigned char bytes[3] = {0, 0, 0};
    f.read(reinterpret_cast<char*>(bytes), 3);
    const auto count = static_cast<std::size_t>(f.gcount());
    f.clear();
    const ByteOrderMark mark = find_byte_order_mark(bytes, count);
    if (mark == ByteOrderMark::Utf16) {
        return 0;
    }
    if (mark != ByteOrderMark::Utf8) {
        f.seekg(0);
    }
    return 1;
}

StdinType stdin_type() {
    struct stat info;
    if (fstat(0, &info) != 0) {
        return StdinType::Unknown;
    }
    return (info.st_mode & S_IFMT) == S_IFREG ? StdinType::File : StdinType::NotFile;
}
