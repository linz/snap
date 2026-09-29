#include "snapconfig.hpp"

// Standalone unit test for stncodecmp in network/netlist.cpp. Awaiting
// migration to CTest - until then, failures are reported to stdout and the
// process exit code is the pass/fail signal (0 = all passed), matching how
// other test-only tools in src/test/ are checked.

#include <cstdlib>
#include <iostream>
#include <string>

#include "network/network.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// Checks that first sorts before second, and that the reverse comparison
/// is the mirror image.
void check_sorts_before( const std::string &first, const std::string &second )
{
    check( stncodecmp(first,second) < 0, "stncodecmp: " + first + " sorts before " + second );
    check( stncodecmp(second,first) > 0, "stncodecmp: " + second + " sorts after " + first );
}

void check_equal( const std::string &first, const std::string &second )
{
    check( stncodecmp(first,second) == 0, "stncodecmp: " + first + " equals " + second );
    check( stncodecmp(second,first) == 0, "stncodecmp: " + second + " equals " + first );
}

void check_case_insensitive()
{
    check_equal( "ABC", "abc" );
    check_equal( "Ab1x", "aB1X" );
    check_equal( "", "" );
    check_sorts_before( "ABC", "abd" );
}

void check_digit_runs_compare_by_value()
{
    check_sorts_before( "AB9", "AB10" );
    check_sorts_before( "9", "10" );
    check_sorts_before( "A99", "a100" );
    check_sorts_before( "AB2C", "AB10A" );
}

void check_equal_digit_values_fall_back_to_text()
{
    // 10 and 010 have the same value, so the full text decides
    check_sorts_before( "AB010", "AB10" );
    check_sorts_before( "A1B", "A1C" );
    check_equal( "A1B", "a1b" );
}

void check_text_before_digits_decides_first()
{
    check_sorts_before( "AB1", "AC1" );
    check_sorts_before( "AB99", "AC1" );
}

void check_digit_runs_at_different_places()
{
    // The first digit is not in the same place, so the codes compare as plain text
    check_sorts_before( "A9", "AB10" );
    check_sorts_before( "ABC", "ABC1" );
    check_sorts_before( "AB", "AB1" );
}

}  // namespace

int main()
{
    check_case_insensitive();
    check_digit_runs_compare_by_value();
    check_equal_digit_values_fall_back_to_text();
    check_text_before_digits_decides_first();
    check_digit_runs_at_different_places();

    if( failures == 0 )
    {
        std::cout << "All stncodecmp tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " stncodecmp test(s) failed\n";
    return EXIT_FAILURE;
}
