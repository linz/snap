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
    // directory (path_len) - a nonexistent filename is used as a literal
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

} // namespace

int main()
{
    check_build_filespec_basic();
    check_build_config_filespec_basic();
    check_normalization();
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
