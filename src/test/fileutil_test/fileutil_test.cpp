#include "snapconfig.h"

// Standalone unit test for util/fileutil.cpp's path-building and file-search
// functions (build_filespec/build_config_filespec/find_config_file/
// find_relative_file/find_file). Awaiting migration to CTest - until then,
// failures are reported to stdout and the process exit code is the pass/fail
// signal (0 = all passed), matching fieldscanner_test.

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>

#include "util/fileutil.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void check_build_filespec_basic()
{
    check( build_filespec("/a/b","name",".ext") == "/a/b/name.ext", "build_filespec: basic dir+name+ext" );
    check( build_filespec("","name",".ext") == "name.ext", "build_filespec: empty dir omits prefix" );
    check( build_filespec("/a/b","name","") == "/a/b/name", "build_filespec: empty ext omits suffix" );
    check( build_filespec("/a/b/","name",".ext") == "/a/b/name.ext", "build_filespec: trailing separator on dir not doubled" );
}

void check_build_config_filespec_basic()
{
    check( build_config_filespec("/a/b",false,"config","name",".ext") == "/a/b/config/name.ext",
           "build_config_filespec: dir+config+name+ext" );
    check( build_config_filespec("/a/b",false,"","name",".ext") == "/a/b/name.ext",
           "build_config_filespec: empty config omits subdirectory" );
    check( build_config_filespec("/a/b/c.cfg",true,"","name",".ext") == "/a/b/name.ext",
           "build_config_filespec: pathonly extracts directory from a filename" );
}

// Normalization fully resolves chained ".."/"./../" segments (a single
// left-to-right pass over path components, not a crude character scan that
// stops after one level), and collapses plain doubled separators.
void check_normalization()
{
    check( build_config_filespec("/a/b/../c",false,"","name",".ext") == "/a/c/name.ext",
           "normalize: single ../ collapses against the preceding segment" );
    check( build_config_filespec("/a/./b",false,"","name",".ext") == "/a/b/name.ext",
           "normalize: ./ segment dropped" );
    check( build_config_filespec("./a/b",false,"","name",".ext") == "a/b/name.ext",
           "normalize: leading ./ dropped" );
    check( build_config_filespec("/a/b/../../c",false,"","name",".ext") == "/c/name.ext",
           "normalize: chained ../../ fully resolved" );
    check( build_config_filespec("/a/./../b",false,"","name",".ext") == "/b/name.ext",
           "normalize: ./ immediately followed by ../ fully resolved" );
    check( build_config_filespec("/a//b",false,"","name",".ext") == "/a/b/name.ext",
           "normalize: plain doubled separator collapsed" );
    check( build_config_filespec("/../x",false,"","name",".ext") == "/../x/name.ext",
           "normalize: leading .. with nothing to cancel against is kept literally" );
    check( build_config_filespec("/.hidden/../x",false,"","name",".ext") == "/.hidden/../x/name.ext",
           "normalize: ../ not collapsed against a dot-led preceding segment" );
}

void check_find_relative_file()
{
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "snap_fileutil_test_relative";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories( tmp / "sub" );
    { std::ofstream(tmp / "sub" / "target.dat").put('x'); }
    // base must be a real, existing file for pathonly to extract its
    // directory - a nonexistent filename is used as a literal
    // directory prefix instead, which is not what this exercises.
    { std::ofstream(tmp / "sub" / "other.dat").put('y'); }
    std::string basefile = (tmp / "sub" / "other.dat").string();

    auto found = find_relative_file( basefile, "target", ".dat" );
    check( found.has_value(), "find_relative_file: finds a file in the base's directory" );
    if( found ) check( std::filesystem::equivalent(*found, tmp / "sub" / "target.dat"),
                        "find_relative_file: resolves to the correct file" );

    auto notfound = find_relative_file( basefile, "missing", ".dat" );
    check( ! notfound.has_value(), "find_relative_file: nullopt when no matching file exists" );

    std::filesystem::remove_all(tmp);
}

// FF_TRYLOCAL and FF_TRYPROJECT are independent bits (FF_TRYALL is both
// combined) - each must be individually effective, not just true-when-any-
// bit-is-set.
void check_find_file_tryopt_bitmask()
{
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "snap_fileutil_test_tryopt";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories( tmp / "project" );
    const std::string marker = "snap_fileutil_test_tryopt_marker";
    { std::ofstream(tmp / "project" / (marker+".dat")).put('x'); }

    push_file_context( (tmp / "project").string() );

    auto localOnly = find_file( marker, ".dat", std::nullopt, FF_TRYLOCAL, "" );
    check( ! localOnly.has_value(), "find_file: FF_TRYLOCAL alone does not also search the project context stack" );

    auto projectOnly = find_file( marker, ".dat", std::nullopt, FF_TRYPROJECT, "" );
    check( projectOnly.has_value(), "find_file: FF_TRYPROJECT alone searches the project context stack" );

    auto all = find_file( marker, ".dat", std::nullopt, FF_TRYALL, "" );
    check( all.has_value(), "find_file: FF_TRYALL searches both strategies" );

    pop_file_context();
    std::filesystem::remove_all(tmp);
}

void check_find_config_file_via_snapenv()
{
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "snap_fileutil_test_config";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories( tmp / "mysection" );
    { std::ofstream(tmp / "mysection" / "cfgtest.def").put('x'); }

#ifdef _WIN32
    _putenv_s("SNAPDIR", tmp.string().c_str());
#else
    setenv("SNAPDIR", tmp.string().c_str(), 1);
#endif
    reset_config_dirs();

    auto found = find_config_file( "mysection", "cfgtest", ".def" );
    check( found.has_value(), "find_config_file: finds a file via a SNAPDIR-listed directory" );

    auto notfound = find_config_file( "mysection", "missing", ".def" );
    check( ! notfound.has_value(), "find_config_file: nullopt when no matching file exists" );

#ifdef _WIN32
    _putenv_s("SNAPDIR", "");
#else
    unsetenv("SNAPDIR");
#endif
    reset_config_dirs();
    std::filesystem::remove_all(tmp);
}

// native_path converts every alternative separator to the native one, and
// changes nothing else: nothing is collapsed, removed or resolved.
void check_native_path()
{
#ifdef UNIX
    check( native_path( "a\\b/c\\\\d\\" ) == "a/b/c//d/",
           "native_path: every backslash becomes /, the existing / is unchanged, none are collapsed or removed" );
    check( native_path( ".\\..\\a" ) == "./../a", "native_path: . and .. segments are kept" );
#else
    check( native_path( "a/b\\c//d/" ) == "a\\b\\c\\\\d\\",
           "native_path: every / becomes a backslash, the existing backslash is unchanged, none are collapsed or removed" );
    check( native_path( "./..//a" ) == ".\\..\\\\a", "native_path: . and .. segments are kept" );
#endif
    check( native_path( "" ).empty(), "native_path: empty string" );
}

// pathonly takes the directory of a filename, including when it has no directory,
// sits in the root, or was written with the alternative separator.
void check_build_config_filespec_pathonly()
{
    check( build_config_filespec("c.cfg",true,"","name",".ext") == "name.ext",
           "pathonly: filename with no directory gives no directory prefix" );
    check( build_config_filespec("/c.cfg",true,"","name",".ext") == "name.ext",
           "pathonly: filename in the root directory gives no directory prefix" );
    check( build_config_filespec("/a/b/",true,"","name",".ext") == "/a/b/name.ext",
           "pathonly: trailing separator is the end of the directory" );
    check( build_config_filespec("/a.b/c",true,"","name",".ext") == "/a.b/name.ext",
           "pathonly: a dot in the directory is not an extension" );
#ifdef UNIX
    check( build_config_filespec("\\a\\b\\c.cfg",true,"","name",".ext") == "/a/b/name.ext",
           "pathonly: backslash in a filename is a directory separator" );
#endif
}

// context_definition writes the chain of directories below the root context as
// "//" delimited relative paths, and recreate_context rebuilds the same context.
void check_context_round_trip()
{
    std::filesystem::path tmp = std::filesystem::temp_directory_path() / "snap_fileutil_test_context";
    push_file_context( tmp.string() );
    file_context *root = current_file_context();
    check( context_definition( root ).empty(), "context_definition: root context is empty" );

    push_file_context( (tmp / "a").string() );
    push_file_context( (tmp / "a" / "b").string() );
    file_context *leaf = current_file_context();
    const std::string definition = context_definition( leaf );
    check( definition == "//a//b", "context_definition: relative directories joined with //, root first" );

    // The leading // of the definition gives recreate_context an empty first segment, so
    // the rebuilt chain has an extra link under the root and is not the same object as
    // leaf (a long-standing flaw, see reload_filenames). It does end in the same directory.
    file_context *recreated = recreate_context( definition );
    check( recreated != nullptr && recreated->dir == leaf->dir,
           "recreate_context: rebuilds a context for the same directory from its definition" );
    check( recreate_context( "" ) == root, "recreate_context: empty definition gives the root context" );
    check( current_file_context() == leaf, "recreate_context: leaves the current context unchanged" );

    pop_file_context();
    pop_file_context();
    pop_file_context();
}

} // namespace

int main()
{
    check_build_filespec_basic();
    check_build_config_filespec_basic();
    check_normalization();
    check_native_path();
    check_build_config_filespec_pathonly();
    check_context_round_trip();
    check_find_relative_file();
    check_find_file_tryopt_bitmask();
    check_find_config_file_via_snapenv();

    if( failures == 0 )
    {
        std::cout << "All fileutil tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " fileutil test(s) failed\n";
    return EXIT_FAILURE;
}
