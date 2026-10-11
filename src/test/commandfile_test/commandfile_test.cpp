// Standalone unit test for CommandFile in snap/snapglob.h. Awaiting migration
// to CTest - until then, failures are reported to stdout and the process exit
// code is the pass/fail signal (0 = all passed), matching how other test-only
// tools in src/test/ are checked.
//
// Each case creates the files it needs in a scratch directory, builds a
// CommandFile and compares path(), dir() and root() with values written out by
// hand from the documented behaviour of the class: the name as given is used if
// it exists, otherwise the extensions .cmd, .snp and .snap are tried in that
// order, otherwise the name is kept unchanged.

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "snap/snapglob.h"

namespace
{

int failures = 0;

void check_equal( const std::string &actual, const std::string &expected, const std::string &description )
{
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected [" << expected << "] got [" << actual << "]\n";
}

const std::filesystem::path test_dir = std::filesystem::temp_directory_path() / "snap_commandfile_test";

/// The scratch directory name as CommandFile reports it as a directory, with its trailing separator
std::string test_dir_with_separator()
{
    return (test_dir / "").string();
}

/// Creates an empty file in the scratch directory and returns its full name
std::string make_file( const std::string &name )
{
    const std::filesystem::path file = test_dir / name;
    std::filesystem::create_directories( file.parent_path() );
    std::ofstream( file ).put( '\n' );
    return file.string();
}

/// The full name of a file in the scratch directory, which is not created
std::string file_name( const std::string &name )
{
    return (test_dir / name).string();
}

void check_command_file( const CommandFile &command, const std::string &path, const std::string &dir,
                         const std::string &root, const std::string &description )
{
    check_equal( command.path, path, description + " path" );
    check_equal( command.dir, dir, description + " dir" );
    check_equal( command.root, root, description + " root" );
}

void test_name_as_given_exists()
{
    // A file called exactly the name given wins over one with an extension added
    const std::string exact = make_file( "exact" );
    make_file( "exact.cmd" );
    check_command_file( CommandFile( exact ), exact, test_dir_with_separator(), exact, "exact name preferred" );

    const std::string with_extension = make_file( "given.snp" );
    check_command_file( CommandFile( with_extension ), with_extension, test_dir_with_separator(),
                        file_name( "given" ), "name with extension" );
}

void test_extension_probing()
{
    const std::string cmd = make_file( "only_cmd.cmd" );
    check_command_file( CommandFile( file_name( "only_cmd" ) ), cmd, test_dir_with_separator(),
                        file_name( "only_cmd" ), ".cmd added" );

    const std::string snp = make_file( "only_snp.snp" );
    check_command_file( CommandFile( file_name( "only_snp" ) ), snp, test_dir_with_separator(),
                        file_name( "only_snp" ), ".snp added" );

    const std::string snap = make_file( "only_snap.snap" );
    check_command_file( CommandFile( file_name( "only_snap" ) ), snap, test_dir_with_separator(),
                        file_name( "only_snap" ), ".snap added" );
}

void test_extension_order()
{
    const std::string first = make_file( "order_a.cmd" );
    make_file( "order_a.snp" );
    make_file( "order_a.snap" );
    check_equal( CommandFile( file_name( "order_a" ) ).path, first, ".cmd before .snp and .snap" );

    const std::string second = make_file( "order_b.snp" );
    make_file( "order_b.snap" );
    check_equal( CommandFile( file_name( "order_b" ) ).path, second, ".snp before .snap" );
}

void test_not_found()
{
    const std::string missing = file_name( "does_not_exist" );
    check_command_file( CommandFile( missing ), missing, test_dir_with_separator(), missing, "missing file" );
}

void test_no_directory()
{
    // A relative name with no directory part. The name is chosen so that it is not found
    check_command_file( CommandFile( "snap_commandfile_test_missing" ), "snap_commandfile_test_missing", "",
                        "snap_commandfile_test_missing", "no directory" );
}

void test_dotted_names()
{
    // Only the final extension is removed, and dots in the directory are not an extension
    const std::string dotted = make_file( "dir.d/job.v2.cmd" );
    const std::string dir = (test_dir / "dir.d" / "").string();
    check_command_file( CommandFile( dotted ), dotted, dir, file_name( "dir.d/job.v2" ), "dotted names" );

    const std::string no_extension = make_file( "dir.d/plain" );
    check_command_file( CommandFile( no_extension ), no_extension, dir, no_extension, "dotted directory only" );
}

} // namespace

int main()
{
    std::filesystem::remove_all( test_dir );
    std::filesystem::create_directories( test_dir );

    test_name_as_given_exists();
    test_extension_probing();
    test_extension_order();
    test_not_found();
    test_no_directory();
    test_dotted_names();

    std::filesystem::remove_all( test_dir );

    if( failures )
    {
        std::cout << failures << " failure(s)\n";
        return 1;
    }
    std::cout << "commandfile_test: all passed\n";
    return 0;
}
