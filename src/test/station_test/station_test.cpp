#include "snapconfig.hpp"

// Standalone unit test for the classification functions of the station class
// in network/network.h. Awaiting migration to CTest - until then, failures are
// reported to stdout and the process exit code is the pass/fail signal (0 =
// all passed), matching how other test-only tools in src/test/ are checked.
//
// A counting error handler is installed with set_error_handler so that the
// reports made for an invalid class id can be checked. The constructor and the
// coordinate functions need an ellipsoid and are covered by the binroundtrip and
// regression tests.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

#include "network/network.h"
#include "util/errdef.h"

namespace
{

int failures = 0;

void check_equal( const int actual, const int expected, const std::string &description )
{
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected " << expected << " got " << actual << "\n";
}

int errorCount = 0;
int lastStatus = 0;

int counting_handler( const int status, const std::string_view, const error_message )
{
    ++errorCount;
    lastStatus = status;
    return status;
}

/// The values of the first count classifications of a station, 1 to count
std::string class_values( const station &stn, const int count )
{
    std::string values;
    for( int classId = 1; classId <= count; classId++ )
    {
        if( classId > 1 ) values += ",";
        values += std::to_string( stn.get_class( classId ) );
    }
    return values;
}

void check_values( const station &stn, const int count, const std::string &expected, const std::string &description )
{
    const std::string actual = class_values( stn, count );
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected [" << expected << "] got [" << actual << "]\n";
}

void test_new_station_has_no_classes()
{
    const station stn;
    check_equal( stn.get_class( 1 ), 0, "class 1 of a station with no classes" );
    check_equal( stn.get_class( 0 ), 0, "class 0" );
    check_equal( stn.get_class( -1 ), 0, "class -1" );
    check_equal( static_cast<int>( stn.classval.size() ), 0, "number of class values" );
}

void test_class_count_zero_fills()
{
    station stn;
    stn.set_class_count( 3 );
    check_equal( static_cast<int>( stn.classval.size() ), 3, "number of class values after set_class_count(3)" );
    check_values( stn, 3, "0,0,0", "new class values are zero" );
}

void test_set_and_get_class()
{
    station stn;
    stn.set_class_count( 3 );
    stn.set_class( 1, 5 );
    stn.set_class( 2, 7 );
    stn.set_class( 3, 9 );
    check_values( stn, 3, "5,7,9", "values set by class id" );
}

void test_growing_keeps_existing_values()
{
    station stn;
    stn.set_class_count( 3 );
    stn.set_class( 1, 5 );
    stn.set_class( 2, 7 );
    stn.set_class( 3, 9 );
    stn.set_class_count( 5 );
    check_values( stn, 5, "5,7,9,0,0", "existing values kept and new ones zero when growing" );
}

void test_shrinking_drops_the_end()
{
    station stn;
    stn.set_class_count( 3 );
    stn.set_class( 1, 5 );
    stn.set_class( 2, 7 );
    stn.set_class( 3, 9 );
    stn.set_class_count( 2 );
    check_values( stn, 2, "5,7", "values kept when shrinking" );
    check_equal( stn.get_class( 3 ), 0, "dropped class reads as 0" );
}

void test_zero_or_negative_count_removes_all_classes()
{
    station stn;
    stn.set_class_count( 2 );
    stn.set_class( 1, 4 );
    stn.set_class_count( 0 );
    check_equal( static_cast<int>( stn.classval.size() ), 0, "set_class_count(0)" );

    stn.set_class_count( 2 );
    stn.set_class_count( -3 );
    check_equal( static_cast<int>( stn.classval.size() ), 0, "a negative count" );
}

void test_invalid_class_id_is_reported_on_set_only()
{
    station stn;
    stn.set_class_count( 2 );
    stn.set_class( 1, 5 );
    stn.set_class( 2, 6 );

    const errhandler_type oldHandler = set_error_handler( counting_handler );

    errorCount = 0;
    stn.set_class( 0, 99 );
    check_equal( errorCount, 1, "errors reported for class id 0" );
    check_equal( lastStatus, INCONSISTENT_DATA, "status reported for class id 0" );
    stn.set_class( 3, 99 );
    check_equal( errorCount, 2, "errors reported for a class id past the end" );
    stn.set_class( -1, 99 );
    check_equal( errorCount, 3, "errors reported for a negative class id" );
    check_values( stn, 2, "5,6", "values unchanged by invalid class ids" );

    errorCount = 0;
    stn.get_class( 0 );
    stn.get_class( 3 );
    check_equal( errorCount, 0, "errors reported when getting an invalid class id" );

    set_error_handler( oldHandler );
}

void test_copy_has_its_own_class_values()
{
    // snaplist copies stations by value
    station original;
    original.set_class_count( 2 );
    original.set_class( 1, 5 );
    original.set_class( 2, 6 );

    station copy = original;
    copy.set_class( 1, 99 );
    check_values( original, 2, "5,6", "the original is unchanged by changes to a copy" );
    check_values( copy, 2, "99,6", "the copy has the changed value" );
}

void test_set_class_count_is_independent_per_station()
{
    station first;
    station second;
    first.set_class_count( 2 );
    first.set_class( 1, 1 );
    second.set_class_count( 4 );
    check_equal( static_cast<int>( first.classval.size() ), 2, "first station class count" );
    check_equal( static_cast<int>( second.classval.size() ), 4, "second station class count" );
    check_values( first, 2, "1,0", "first station values" );
}

}  // namespace

int main()
{
    test_new_station_has_no_classes();
    test_class_count_zero_fills();
    test_set_and_get_class();
    test_growing_keeps_existing_values();
    test_shrinking_drops_the_end();
    test_zero_or_negative_count_removes_all_classes();
    test_invalid_class_id_is_reported_on_set_only();
    test_copy_has_its_own_class_values();
    test_set_class_count_is_independent_per_station();

    if( failures == 0 )
    {
        std::cout << "All station tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " station test(s) failed\n";
    return EXIT_FAILURE;
}
