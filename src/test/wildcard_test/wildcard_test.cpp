#include "snapconfig.hpp"

// Standalone unit test for has_wildcard, wildcard_match and
// filename_wildcard_match in util/wildcard.cpp. Failures are reported to
// stdout and the process exit code is the pass/fail signal (0 = all passed),
// matching how other test-only tools in src/test/ are checked.

#include <cstdlib>
#include <iostream>
#include <string>

#include "util/wildcard.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// Checks that pattern matches text with wildcard_match.
void check_matches( const std::string &pattern, const std::string &text )
{
    check( wildcard_match(pattern,text), "wildcard_match: \"" + pattern + "\" matches \"" + text + "\"" );
}

/// Checks that pattern does not match text with wildcard_match.
void check_no_match( const std::string &pattern, const std::string &text )
{
    check( ! wildcard_match(pattern,text), "wildcard_match: \"" + pattern + "\" does not match \"" + text + "\"" );
}

/// Checks that pattern matches filename with filename_wildcard_match.
void check_file_matches( const std::string &pattern, const std::string &filename )
{
    check( filename_wildcard_match(pattern,filename),
           "filename_wildcard_match: \"" + pattern + "\" matches \"" + filename + "\"" );
}

/// Checks that pattern does not match filename with filename_wildcard_match.
void check_file_no_match( const std::string &pattern, const std::string &filename )
{
    check( ! filename_wildcard_match(pattern,filename),
           "filename_wildcard_match: \"" + pattern + "\" does not match \"" + filename + "\"" );
}

void check_has_wildcard()
{
    check( has_wildcard("a*"), "has_wildcard: star" );
    check( has_wildcard("a?c"), "has_wildcard: question mark" );
    check( has_wildcard("*"), "has_wildcard: star alone" );
    check( ! has_wildcard("abc"), "has_wildcard: plain text" );
    check( ! has_wildcard(""), "has_wildcard: empty" );
}

void check_literal_matching()
{
    check_matches( "abc", "abc" );
    check_matches( "abc", "ABC" );
    check_matches( "AbC", "aBc" );
    check_matches( "", "" );
    check_no_match( "abc", "abd" );
    check_no_match( "abc", "ab" );
    check_no_match( "ab", "abc" );
    check_no_match( "", "a" );
    check_no_match( "a", "" );
}

void check_question_mark()
{
    check_matches( "a?c", "abc" );
    check_matches( "?", "x" );
    check_matches( "???", "abc" );
    check_no_match( "a?c", "ac" );
    check_no_match( "a?c", "abbc" );
    check_no_match( "?", "" );
    check_no_match( "??", "a" );
}

void check_star()
{
    check_matches( "*", "" );
    check_matches( "*", "anything" );
    check_matches( "a*", "a" );
    check_matches( "a*", "abc" );
    check_matches( "*c", "abc" );
    check_matches( "a*c", "ac" );
    check_matches( "a*c", "abbbc" );
    check_matches( "*b*", "abc" );
    check_matches( "*b*", "b" );
    check_matches( "a*b*c", "aXXbYYc" );
    check_matches( "**", "abc" );
    check_matches( "a**c", "abc" );
    check_matches( "a*?c", "abc" );
    check_no_match( "a*c", "ab" );
    check_no_match( "*c", "abcd" );
    check_no_match( "a*", "ba" );
    check_no_match( "*b*", "ac" );
    check_no_match( "a*b*c", "aXXcYYb" );
}

void check_star_backtracks()
{
    // The first "ab" is not followed by "c", so the star has to extend past it
    check_matches( "*abc", "ababc" );
    check_matches( "a*ab", "aaab" );
    check_no_match( "*abc", "ababd" );
}

void check_filename_matching()
{
    check_file_matches( "a.txt", "a.txt" );
    check_file_matches( "A.TXT", "a.txt" );
    check_file_matches( "*.txt", "a.txt" );
    check_file_matches( "a?txt", "a.txt" );
    check_file_no_match( "*.txt", "a.dat" );
    check_file_no_match( "", "a.txt" );
    check_file_no_match( "a.txt", "" );
}

void check_filename_matches_any_path_component()
{
    // The pattern may start at the beginning of the name or after any / or \.
    check_file_matches( "b.txt", "a/b.txt" );
    check_file_matches( "b.txt", "a\\b.txt" );
    check_file_matches( "b.txt", "a/b/c/b.txt" );
    check_file_matches( "*.txt", "dir/a.txt" );
    check_file_matches( "b/c.txt", "a/b/c.txt" );
    check_file_no_match( "b.txt", "ab.txt" );
    check_file_no_match( "b.txt", "a/b.txt/c" );
}

void check_filename_wildcards_stop_at_separators()
{
    // A single star or a question mark does not cross a / or \.
    check_file_matches( "dir/*.txt", "dir/a.txt" );
    check_file_no_match( "dir*a.txt", "dir/a.txt" );
    check_file_no_match( "dir*a.txt", "dir\\a.txt" );
    check_file_no_match( "dir?a.txt", "dir/a.txt" );
    check_file_no_match( "a*", "a/b" );
    // A run of stars does cross them.
    check_file_matches( "dir**a.txt", "dir/a.txt" );
    check_file_matches( "dir**a.txt", "dir\\sub\\a.txt" );
}

}  // namespace

int main()
{
    check_has_wildcard();
    check_literal_matching();
    check_question_mark();
    check_star();
    check_star_backtracks();
    check_filename_matching();
    check_filename_matches_any_path_component();
    check_filename_wildcards_stop_at_separators();

    if( failures == 0 )
    {
        std::cout << "All wildcard tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " wildcard test(s) failed\n";
    return EXIT_FAILURE;
}
