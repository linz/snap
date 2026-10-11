#include "snapconfig.h"

// Standalone unit test for util/readcfg_internal.h's filter_line()/cap_line() -
// the pure comment/whitespace-filtering and capacity/overrun logic behind
// CFG_FILE::get_config_line(). Awaiting migration to CTest - until then,
// failures are reported to stdout and the process exit code is the pass/fail
// signal (0 = all passed), matching fieldscanner_test/fileutil_test.

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>

#include "util/readcfg_internal.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void check_filter_plain_line()
{
    std::string line = "option = value";
    filter_line( line, CommentRule{ std::nullopt, false } );
    check( line == "option = value", "filter_line: plain line with no comment rule is unchanged" );
}

void check_filter_strips_cr_and_ctrlz()
{
    std::string line = "abc\rdef\x1Aghi";
    filter_line( line, CommentRule{ std::nullopt, false } );
    check( line == "abcdefghi", "filter_line: '\\r' and '\\x1A' are stripped even with no comment rule" );
}

void check_filter_normalises_whitespace()
{
    std::string line = "a\tb  c";
    filter_line( line, CommentRule{ std::nullopt, false } );
    check( line == "a b  c", "filter_line: a tab is normalised to a single space, runs of real spaces are untouched" );
}

void check_filter_comment_anywhere()
{
    std::string line = "value ! comment text";
    filter_line( line, CommentRule{ '!', false } );
    check( line == "value ", "filter_line: comment starting mid-line discards everything from the comment character on" );
}

void check_filter_comment_ignored()
{
    std::string line = "value ! not a comment";
    filter_line( line, CommentRule{ std::nullopt, false } );
    check( line == "value ! not a comment", "filter_line: nullopt comment_char means the character is never treated specially" );
}

void check_filter_whole_line_only_at_start()
{
    std::string line = "! this whole line is a comment";
    filter_line( line, CommentRule{ '!', true } );
    check( line == "", "filter_line: whole_line_only comment at position 0 empties the whole line" );
}

void check_filter_whole_line_only_not_at_start()
{
    std::string line = "value ! not treated as a comment here";
    filter_line( line, CommentRule{ '!', true } );
    check( line == "value ! not treated as a comment here",
           "filter_line: whole_line_only comment character appearing after position 0 is kept literally" );
}

void check_filter_late_comment_scans_to_true_end()
{
    std::string line( 2000, 'x' );
    line += "!trailing comment";
    filter_line( line, CommentRule{ '!', false } );
    check( line == std::string(2000,'x'), "filter_line: a comment far past any reasonable length limit is still found and stripped" );
}

void check_cap_line_fits_exactly()
{
    std::string line = "12345";
    const int overrun = cap_line( line, 5 );
    check( line == "12345" && overrun == 0, "cap_line: content exactly at max_len is untouched, no overrun" );
}

void check_cap_line_shorter_than_limit()
{
    std::string line = "abc";
    const int overrun = cap_line( line, 10 );
    check( line == "abc" && overrun == 0, "cap_line: content shorter than max_len is untouched, no overrun" );
}

void check_cap_line_real_excess()
{
    std::string line = "12345XY";
    const int overrun = cap_line( line, 5 );
    check( line == "12345" && overrun == 2, "cap_line: real excess content past max_len is truncated and fully counted" );
}

void check_cap_line_tolerates_boundary_whitespace()
{
    std::string line = "12345   ";
    const int overrun = cap_line( line, 5 );
    check( line == "12345" && overrun == 0, "cap_line: pure whitespace immediately past max_len is discarded but not counted as overrun" );
}

void check_cap_line_whitespace_then_real_content()
{
    std::string line = "12345  XY";
    const int overrun = cap_line( line, 5 );
    check( line == "12345" && overrun == 2,
           "cap_line: leading whitespace past max_len is skipped, but real content after it is fully counted" );
}

void check_late_comment_cancels_would_be_overrun()
{
    // The scenario that motivated filtering the whole line before capping:
    // filter_line() removes the comment (and everything after it) before
    // cap_line() ever runs, so pure whitespace sitting between max_len and
    // a distant comment is tolerated exactly like any other boundary
    // whitespace - the comment doesn't need special-casing in cap_line at
    // all, it's simply gone already.
    std::string line = std::string(10,'a') + "   !" + std::string(10,'c');
    filter_line( line, CommentRule{ '!', false } );
    const int overrun = cap_line( line, 10 );
    check( line == std::string(10,'a') && overrun == 0,
           "cap_line after filter_line: boundary whitespace before a stripped-away comment is tolerated, no overrun" );
}

} // namespace

int main()
{
    check_filter_plain_line();
    check_filter_strips_cr_and_ctrlz();
    check_filter_normalises_whitespace();
    check_filter_comment_anywhere();
    check_filter_comment_ignored();
    check_filter_whole_line_only_at_start();
    check_filter_whole_line_only_not_at_start();
    check_filter_late_comment_scans_to_true_end();
    check_cap_line_fits_exactly();
    check_cap_line_shorter_than_limit();
    check_cap_line_real_excess();
    check_cap_line_tolerates_boundary_whitespace();
    check_cap_line_whitespace_then_real_content();
    check_late_comment_cancels_would_be_overrun();

    if( failures == 0 )
    {
        std::cout << "All readcfg filter/cap tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " readcfg filter/cap test(s) failed\n";
    return EXIT_FAILURE;
}
