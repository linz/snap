#include "snapconfig.hpp"

// Standalone unit test for the StringLimited class in util/stringlimited.hpp.
// Awaiting migration to CTest - until then, failures are reported to stdout and
// the process exit code is the pass/fail signal (0 = all passed), matching how
// other test-only tools in src/test/ are checked.

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

#include "util/stringlimited.hpp"

namespace
{

int failures = 0;

void check( const bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void check_equal( const std::string_view actual, const std::string_view expected, const std::string &description )
{
    if( actual == expected ) return;
    ++failures;
    std::cout << "FAIL: " << description << ": expected [" << expected << "] got [" << actual << "]\n";
}

void test_default_is_empty()
{
    const StringLimited<4> text;
    check( text.empty(), "default string is empty" );
    check( text.size() == 0, "default string has size 0" );
    check_equal( text, "", "default string as a string_view" );
    check_equal( text.c_str(), "", "default string as a C string" );
}

void test_construct()
{
    const StringLimited<4> text( "ABC" );
    check( ! text.empty(), "constructed string is not empty" );
    check( text.size() == 3, "constructed string has size 3" );
    check_equal( text, "ABC", "constructed string as a string_view" );
    check_equal( text.c_str(), "ABC", "constructed string is NUL terminated" );
}

void test_construct_truncates_to_the_limit()
{
    const StringLimited<4> exact( "ABCD" );
    check_equal( exact, "ABCD", "a string of exactly the limit is kept" );

    const StringLimited<4> longer( "ABCDEFG" );
    check( longer.size() == 4, "a longer string is truncated to the limit" );
    check_equal( longer, "ABCD", "a longer string keeps its first characters" );
    check_equal( longer.c_str(), "ABCD", "a truncated string is NUL terminated" );
}

void test_assign_truncates_to_the_limit()
{
    StringLimited<4> text;
    text.assign( "ABCD" );
    check_equal( text, "ABCD", "assigning a string of exactly the limit keeps it" );

    text.assign( "WXYZ12" );
    check( text.size() == 4, "assigning a longer string truncates it to the limit" );
    check_equal( text, "WXYZ", "assigning a longer string keeps its first characters" );
}

void test_assign_replaces_the_whole_string()
{
    StringLimited<4> text( "ABCD" );
    text.assign( "AB" );
    check_equal( text, "AB", "a shorter string replaces a longer one" );
    check_equal( text.c_str(), "AB", "nothing of the longer string is left behind" );

    text.assign( std::string_view() );
    check( text.empty(), "assigning an empty string empties it" );
    check( text.size() == 0, "assigning an empty string gives size 0" );
}

void test_str_copies_the_text()
{
    StringLimited<4> text( "ABCDEFG" );
    const std::string copy = text.str();
    check_equal( copy, "ABCD", "str gives the truncated text" );

    text.assign( "XY" );
    check_equal( copy, "ABCD", "a copy from str is not changed by a later assign" );
    check_equal( text.str(), "XY", "str gives the current text" );
}

void test_limit_of_zero()
{
    const StringLimited<0> text( "ABC" );
    check( text.empty(), "a string limited to no characters is always empty" );
}

void test_limit_is_part_of_the_type()
{
    static_assert( ! std::is_convertible<StringLimited<4>, std::string>::value,
                   "a StringLimited does not convert to std::string, whose operations could break the limit" );
    static_assert( ! std::is_convertible<std::string_view, StringLimited<4>>::value,
                   "a string_view does not convert to a StringLimited implicitly" );
    static_assert( ! std::is_convertible<StringLimited<4>, StringLimited<5>>::value,
                   "strings with different limits are different types" );
    check( true, "limit is part of the type" );
}

}  // namespace

int main()
{
    test_default_is_empty();
    test_construct();
    test_construct_truncates_to_the_limit();
    test_assign_truncates_to_the_limit();
    test_assign_replaces_the_whole_string();
    test_str_copies_the_text();
    test_limit_of_zero();
    test_limit_is_part_of_the_type();

    if( failures == 0 )
    {
        std::cout << "All StringLimited tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " StringLimited test(s) failed\n";
    return EXIT_FAILURE;
}
