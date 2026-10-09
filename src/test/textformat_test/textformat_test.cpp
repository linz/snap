// Standalone unit test for util/textformat.hpp. Awaiting migration to CTest -
// until then, failures are reported to stdout and the process exit code is the
// pass/fail signal (0 = all passed), matching how other test-only tools in
// src/test/ are checked.
//
// Where printf has an equivalent format, the expected strings are what it gives.

#include <iostream>
#include <string>

#include "util/textformat.hpp"

namespace
{

int failures=0;

void check_equal( const std::string &actual, const std::string &expected, const std::string &description )
{
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected [" << expected << "] got [" << actual << "]\n";
}

void check_format_fixed()
{
    check_equal( format_fixed( 3.14159, 2 ), "3.14", "fixed: decimal places" );
    check_equal( format_fixed( 2.6, 0 ), "3", "fixed: no decimal places rounds and has no point" );
    check_equal( format_fixed( 123.456, 0 ), "123", "fixed: no decimal places, rounding down" );
    check_equal( format_fixed( 0.0, 2 ), "0.00", "fixed: zero" );
    check_equal( format_fixed( -45.5126, 3 ), "-45.513", "fixed: negative" );
    check_equal( format_fixed( 1e-7, 3 ), "0.000", "fixed: smaller than the last digit" );
    check_equal( format_fixed( 1234.5678, 1, 12 ), "      1234.6", "fixed: padded with spaces to a width" );
    check_equal( format_fixed( 123456.789, 1, 3 ), "123456.8", "fixed: wider than the width" );
    check_equal( format_fixed( 7.5, 2, 8, '0' ), "00007.50", "fixed: zero fill" );
    check_equal( format_fixed( -2.5, 1, 6, '0' ), "-002.5", "fixed: zero fill goes after the sign" );
    check_equal( format_fixed( 5.0, 0, 2, '0' ), "05", "fixed: zero fill of a whole number" );
    check_equal( format_fixed( -2.5, 1, 6 ), "  -2.5", "fixed: space fill goes before the sign" );
    check_equal( format_fixed( -2.5, 1, 6, '*' ), "-**2.5", "fixed: any other fill goes after the sign" );
    check_equal( format_fixed( 2.5, 1, 6, '*' ), "***2.5", "fixed: any other fill, positive" );
}

void check_format_scientific()
{
    check_equal( format_scientific( 12345.6789, 8 ), "1.23456789e+04", "scientific: eight decimal places" );
    check_equal( format_scientific( 0.00012, 2 ), "1.20e-04", "scientific: negative exponent" );
    check_equal( format_scientific( -5.0, 1 ), "-5.0e+00", "scientific: negative value" );
    check_equal( format_scientific( 0.0, 2 ), "0.00e+00", "scientific: zero" );
}

void check_padding()
{
    check_equal( pad_right( "To", 6 ), "To    ", "pad_right: shorter than the width" );
    check_equal( pad_right( "Length", 6 ), "Length", "pad_right: exactly the width" );
    check_equal( pad_right( "Observations", 6 ), "Observations", "pad_right: wider than the width is not truncated" );
    check_equal( pad_right( "", 3 ), "   ", "pad_right: empty text" );
    check_equal( pad_left( "To", 6 ), "    To", "pad_left: shorter than the width" );
    check_equal( pad_left( "Length", 6 ), "Length", "pad_left: exactly the width" );
    check_equal( pad_left( "Observations", 6 ), "Observations", "pad_left: wider than the width is not truncated" );
    check_equal( pad_left( "", 3 ), "   ", "pad_left: empty text" );
}

}

int main()
{
    check_format_fixed();
    check_format_scientific();
    check_padding();

    if( failures )
    {
        std::cout << failures << " textformat_test check(s) failed\n";
        return 1;
    }
    std::cout << "textformat_test: all checks passed\n";
    return 0;
}
