#include "snapconfig.hpp"

// Standalone unit test for the station selection criteria in
// network/networkl.cpp. Failures are reported to stdout and the process exit
// code is the pass/fail signal (0 = all passed), as for the other tests in
// src/test/.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "coordsys/coordsys.h"
#include "network/network.h"
#include "snap/snapglob.h"
#include "util/errdef.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << std::endl;
}

/// Installs a WGS84 coordinate system, which a network needs before it will
/// hold stations, from a minimal definition file written for the purpose.
void install_test_coordsys()
{
    const std::filesystem::path file=std::filesystem::temp_directory_path() / "snap_criteria_test_coordsys.def";
    std::ofstream( file )
        << "[ellipsoids]\n"
        << "WGS84 \"WGS84 ellipsoid\" 6378137 298.257223563\n"
        << "[reference_frames]\n"
        << "WGS84 \"WGS84\" ELLIPSOID WGS84 ITRF96 0 0 0 0 0 0 0\n"
        << "[coordinate_systems]\n"
        << "WGS84 \"WGS84\" REF_FRAME WGS84 GEODETIC\n";
    install_default_projections();
    check( install_crdsys_file( file.generic_string() ) == OK, "install the test coordinate system" );
    std::filesystem::remove( file );
}

/// A small network: stations AAA, AB1, AB2, AB10, BBB and ZZ1, with a
/// classification "grp" that has AAA and AB1 in value "x", AB2 in value "y"
/// and the others unset.
struct TestNetwork
{
    TestNetwork()
    {
        nw=new_network();
        // Stations can only be added to a network that has a coordinate system
        coordsys *cs=load_coordsys( "WGS84" );
        if( cs )
        {
            set_network_coordsys( nw, cs, 0.0, 0, nullptr, 0 );
            delete cs;
        }
        const int grp=nw->class_id( "grp", 1 );
        const int x=nw->class_value_id( grp, "x", 1 );
        const int y=nw->class_value_id( grp, "y", 1 );
        for( const char *code : { "AAA", "AB1", "AB2", "AB10", "BBB", "ZZ1" } )
        {
            station *stn=new_network_station( nw, code, code, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 );
            const std::string c( code );
            if( c == "AAA" || c == "AB1" ) set_station_class( stn, grp, x );
            if( c == "AB2" ) set_station_class( stn, grp, y );
        }
    }
    ~TestNetwork() { delete_network( nw ); }
    network *nw;
};

/// The codes of the stations that match select, in station id order.
/// Returns false in ok if compiling the criteria failed.
std::string matches( TestNetwork &tn, const std::string &select, bool useCache=false, bool *ok=nullptr )
{
    void *psc=new_station_criteria();
    if( useCache ) setup_station_criteria_cache( psc, number_of_stations( tn.nw ) );
    const int sts=compile_station_criteria( psc, tn.nw, select, "" );
    if( ok ) *ok = sts == OK;
    std::string result;
    if( sts == OK )
    {
        // Twice, so that the second pass is answered by the cache
        for( int pass=0; pass < 2; ++pass )
        {
            result.clear();
            for( int id=1; id <= number_of_stations( tn.nw ); ++id )
            {
                station *stn=station_ptr( tn.nw, id );
                if( station_criteria_match( psc, stn ) )
                {
                    if( ! result.empty() ) result += " ";
                    result += stn->Code;
                }
            }
        }
    }
    delete_station_criteria( psc );
    return result;
}

void check_matches( TestNetwork &tn, const std::string &select, const std::string &expected )
{
    for( bool useCache : { false, true } )
    {
        bool ok=false;
        const std::string actual=matches( tn, select, useCache, &ok );
        check( ok && actual == expected,
               "\"" + select + "\"" + (useCache ? " (cached)" : "") + " matches [" + actual + "], expected [" + expected + "]" );
    }
}

void check_no_criteria_matches_nothing()
{
    TestNetwork tn;
    void *psc=new_station_criteria();
    check( ! station_criteria_match( psc, station_ptr( tn.nw, 1 ) ), "an empty criteria list matches nothing" );
    // A list starts with an implied operator that the end of the list cannot follow
    check( compile_station_criteria( psc, tn.nw, "", "" ) != OK, "an empty selection is an error" );
    delete_station_criteria( psc );
}

void check_basic_criteria()
{
    TestNetwork tn;
    check_matches( tn, "AAA", "AAA" );
    check_matches( tn, "aaa bbb", "AAA BBB" );
    check_matches( tn, "all", "AAA AB1 AB2 AB10 BBB ZZ1" );
    check_matches( tn, "AB*", "AB1 AB2 AB10" );
    check_matches( tn, "AB1-AB10", "AB1 AB2 AB10" );
    check_matches( tn, "AB2-BBB", "AB2 AB10 BBB" );
    check_matches( tn, "grp=x", "AAA AB1" );
    check_matches( tn, "grp=x/y", "AAA AB1 AB2" );
    check_matches( tn, "grp=unknown", "" );
}

void check_operators()
{
    TestNetwork tn;
    check_matches( tn, "AB* except AB2", "AB1 AB10" );
    check_matches( tn, "all except AB* BBB", "AAA ZZ1" );
    check_matches( tn, "AB* and grp=x", "AB1" );
    check_matches( tn, "AAA or BBB", "AAA BBB" );
    check_matches( tn, "AB* and grp=x/y", "AB1 AB2" );
}

void check_missing_stations()
{
    TestNetwork tn;
    void *psc=new_station_criteria();
    check( compile_station_criteria( psc, tn.nw, "AAA QQQ", "" ) == OK, "compile with a missing station" );
    check( check_station_criteria_codes( psc, tn.nw ) == INVALID_DATA, "a missing station fails by default" );
    delete_station_criteria( psc );

    psc=new_station_criteria();
    compile_station_criteria( psc, tn.nw, "ignore_missing AAA QQQ", "" );
    check( check_station_criteria_codes( psc, tn.nw ) == OK, "ignore_missing ignores a missing station" );
    delete_station_criteria( psc );

    psc=new_station_criteria();
    compile_station_criteria( psc, tn.nw, "warn_missing AAA QQQ", "" );
    check( check_station_criteria_codes( psc, tn.nw ) == INFO_ERROR, "warn_missing reports a missing station as info" );
    delete_station_criteria( psc );
}

void check_cache_ids_beyond_the_network()
{
    // Station ids beyond the size the cache was set up with, and the id the
    // old cache never initialised, must still give the right answer
    TestNetwork tn;
    void *psc=new_station_criteria();
    setup_station_criteria_cache( psc, 2 );
    compile_station_criteria( psc, tn.nw, "AAA ZZ1", "" );
    std::string result;
    for( int id=number_of_stations( tn.nw ); id >= 1; --id )
    {
        if( station_criteria_match( psc, station_ptr( tn.nw, id ) ) ) result += station_ptr( tn.nw, id )->Code;
    }
    check( result == "ZZ1AAA", "cache grown past its initial size, got [" + result + "]" );
    delete_station_criteria( psc );
}

/// Files for the @file tests, in a directory removed when the object goes.
struct ListFiles
{
    ListFiles()
    {
        dir=std::filesystem::temp_directory_path() / "snap_criteria_test";
        std::filesystem::create_directories( dir );
    }
    ~ListFiles() { std::filesystem::remove_all( dir ); }
    std::string write( const std::string &name, const std::string &text )
    {
        const std::filesystem::path path=dir / name;
        std::ofstream( path ) << text;
        return path.generic_string();
    }
    std::filesystem::path dir;
};

void check_station_list_files()
{
    TestNetwork tn;
    ListFiles files;
    const std::string a=files.write( "a.lst", "AAA\nAB1 ! not a comment on the same line\n" );
    check_matches( tn, "@" + a, "AAA AB1" );
    const std::string b=files.write( "b.lst", "BBB\n@" + a + "\n" );
    check_matches( tn, "ZZ1 @" + b, "AAA AB1 BBB ZZ1" );
    check_matches( tn, "all except @" + b, "AB2 AB10 ZZ1" );
    // Items are alternatives unless another operator is written, files included
    check_matches( tn, "@" + b + " ZZ1", "AAA AB1 BBB ZZ1" );
    check_matches( tn, "ZZ1 or @" + a, "AAA AB1 ZZ1" );
    check_matches( tn, "AB* and @" + a, "AB1" );
    check_matches( tn, "AB* except @" + a, "AB2 AB10" );
    const std::string comments=files.write( "comments.lst", "! a comment\n\nAB2\n" );
    check_matches( tn, "@" + comments, "AB2" );
}

void check_station_list_file_problems()
{
    TestNetwork tn;
    ListFiles files;
    void *psc=new_station_criteria();
    check( compile_station_criteria( psc, tn.nw, "@" + (files.dir / "missing.lst").generic_string(), "" ) != OK,
           "a station list file that does not exist is an error" );
    delete_station_criteria( psc );

    // A file that includes itself
    const std::string self=(files.dir / "self.lst").generic_string();
    files.write( "self.lst", "AAA\n@" + self + "\n" );
    psc=new_station_criteria();
    check( compile_station_criteria( psc, tn.nw, "@" + self, "" ) != OK, "a station list file that uses itself is an error" );
    delete_station_criteria( psc );
}

void check_missing_option_does_not_leak_out_of_nested_files()
{
    // c.lst is read under ignore_missing, set on the line in b.lst that
    // includes it. That must not leak back out to the lines of a.lst after
    // the @b.lst line, where a missing station is still an error.
    TestNetwork tn;
    ListFiles files;
    const std::string c=files.write( "c.lst", "AAA\n" );
    const std::string b=files.write( "b.lst", "ignore_missing @" + c + "\n" );
    const std::string a=files.write( "a.lst", "@" + b + "\nQQQ\n" );
    void *psc=new_station_criteria();
    check( compile_station_criteria( psc, tn.nw, "@" + a, "" ) == OK, "compile nested files" );
    check( check_station_criteria_codes( psc, tn.nw ) == INVALID_DATA,
           "a missing station after a nested ignore_missing is still an error" );
    delete_station_criteria( psc );

    // And an option on the including line is inherited by the whole file
    const std::string d=files.write( "d.lst", "QQQ\n" );
    psc=new_station_criteria();
    compile_station_criteria( psc, tn.nw, "ignore_missing @" + d, "" );
    check( check_station_criteria_codes( psc, tn.nw ) == OK, "ignore_missing is inherited by the station list file" );
    delete_station_criteria( psc );
}

void check_criteria_errors()
{
    TestNetwork tn;
    for( const char *select : { "and", "AAA or", "except", "AAA and and BBB", "AB1-" } )
    {
        void *psc=new_station_criteria();
        const bool failed=compile_station_criteria( psc, tn.nw, select, "" ) != OK;
        // "AB1-" is a range with an empty end, not an error
        check( failed == ( std::string( select ) != "AB1-" ), std::string( "compile \"" ) + select + "\"" );
        delete_station_criteria( psc );
    }
}

void check_empty_station_list_file()
{
#ifndef _WIN32
    // A frame with nothing in it used to loop for ever in the matcher, so
    // fail the test rather than hang if it does
    alarm( 20 );
#endif
    TestNetwork tn;
    ListFiles files;
    const std::string empty=files.write( "empty.lst", "! nothing here\n" );
    check_matches( tn, "@" + empty, "" );
    check_matches( tn, "AAA @" + empty, "AAA" );
    check_matches( tn, "AAA @" + empty + " BBB", "AAA BBB" );
#ifndef _WIN32
    alarm( 0 );
#endif
}

}  // namespace

int main()
{
    init_snap_globals();
    install_test_coordsys();

    check_no_criteria_matches_nothing();
    check_basic_criteria();
    check_operators();
    check_missing_stations();
    check_cache_ids_beyond_the_network();
    check_station_list_files();
    check_station_list_file_problems();
    check_missing_option_does_not_leak_out_of_nested_files();
    check_criteria_errors();
    check_empty_station_list_file();

    if( failures == 0 )
    {
        std::cout << "All criteria tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " criteria test(s) failed\n";
    return EXIT_FAILURE;
}
