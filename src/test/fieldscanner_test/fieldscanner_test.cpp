#include "snapconfig.hpp"

// Standalone unit test for util/fieldscanner.hpp. Awaiting migration to
// CTest - until then, failures are reported to stdout and the process exit
// code is the pass/fail signal (0 = all passed), matching how other
// test-only tools in src/test/ are checked.

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util/fieldscanner.hpp"

namespace
{

int failures=0;

const std::optional<std::vector<QuoteFollowOption>> whitespaceOrEnd{
    std::vector<QuoteFollowOption>{QuoteFollowOption::Whitespace,QuoteFollowOption::End} };

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void check_next_basic()
{
    FieldScanner scanner( "  abc  def\tghi " );
    check( scanner.next() == "abc", "next: first field, skipping leading whitespace" );
    check( scanner.next() == "def", "next: second field, across a multi-space run" );
    check( scanner.next() == "ghi", "next: third field, across a tab" );
    check( ! scanner.next(), "next: nullopt after the last field" );
    check( ! scanner.next(), "next: still nullopt on a second call past the end" );
}

void check_next_empty()
{
    FieldScanner scanner( "   " );
    check( ! scanner.next(), "next: nullopt on an all-whitespace input" );
    FieldScanner empty( "" );
    check( ! empty.next(), "next: nullopt on an empty input" );
}

void check_remainder()
{
    FieldScanner scanner( "abc  def" );
    scanner.next();
    check( scanner.remainder() == "  def", "remainder: starts at the trailing whitespace, not past it" );
}

void check_next_delimiter()
{
    // Zero occurrences: fails without consuming, remainder() gives everything.
    {
        FieldScanner scanner( "abc" );
        check( ! scanner.next(','), "next(delim): nullopt when the delimiter never occurs" );
        check( scanner.remainder() == "abc", "next(delim): position unchanged after a failed match" );
    }
    // One occurrence: the field after it is only reachable via remainder(),
    // not a second next(delim) call - the "bounded split" pattern this
    // overload exists for.
    {
        FieldScanner scanner( "a,bc" );
        check( scanner.next(',') == "a", "next(delim): field up to the delimiter" );
        check( ! scanner.next(','), "next(delim): nullopt on the second call, no second delimiter" );
        check( scanner.remainder() == "bc", "next(delim): remainder still gives the rest after a failed second call" );
    }
    // Consecutive delimiters: an empty field between them, not collapsed
    // (the real behavioral difference from the no-argument next()).
    {
        FieldScanner scanner( "a,,c" );
        check( scanner.next(',') == "a", "next(delim): field before the first delimiter" );
        check( scanner.next(',') == "", "next(delim): empty field between two adjacent delimiters, not skipped" );
        check( scanner.remainder() == "c", "next(delim): remainder after both delimiters consumed" );
    }
    // Empty input.
    {
        FieldScanner scanner( "" );
        check( ! scanner.next(','), "next(delim): nullopt on empty input" );
    }
}

void check_span_preserves_multiple_spaces()
{
    FieldScanner scanner( "one  two   three" );
    auto start = scanner.remainder();
    scanner.next();
    scanner.next();
    auto stop = scanner.remainder();
    std::string captured( start.data(), stop.data() - start.data() );
    check( captured == "one  two", "remainder(): verbatim text, multi-space run untouched" );
}

void check_quoted_value_single_field()
{
    std::string text="key=\"short\" next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;  // past '=' and the opening quote
    auto value=scanner.quotedValue( openQuote, '"', whitespaceOrEnd );
    check( value.has_value() && *value == "short", "quotedValue: single-field quoted value" );
    check( scanner.next() == "next", "quotedValue: scanner resumes correctly after a single-field value" );
}

void check_quoted_value_spans_fields()
{
    std::string text="key=\"a  b\" next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;
    auto value=scanner.quotedValue( openQuote, '"', whitespaceOrEnd );
    check( value.has_value() && *value == "a  b", "quotedValue: spans multiple next()-delimited fields, preserving internal spacing" );
    check( scanner.next() == "next", "quotedValue: scanner resumes correctly after a multi-field value" );
}

void check_quoted_value_unterminated()
{
    std::string text="key=\"never closes";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;
    auto value=scanner.quotedValue( openQuote, '"', whitespaceOrEnd );
    check( ! value.has_value(), "quotedValue: nullopt when the closing quote never appears" );
}

void check_quoted_value_not_followed_by_whitespace()
{
    std::string text="key=\"abc\"trailing next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;
    auto value=scanner.quotedValue( openQuote, '"', whitespaceOrEnd );
    check( ! value.has_value(), "quotedValue: nullopt when the closing quote isn't followed by whitespace or end of input" );
}

void check_quoted_value_lenient()
{
    std::string text="key=\"abc\"trailing next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;
    auto value=scanner.quotedValue( openQuote, '"', std::nullopt );
    check( value.has_value() && *value == "abc", "quotedValue: with nullopt followOptions, a closing quote not followed by whitespace still counts" );
    check( scanner.next() == "trailing", "quotedValue: with nullopt followOptions, scanner resumes right after the closing quote" );
}

void check_is_quoted()
{
    std::string_view doubleQuoted="\"abc";
    check( FieldScanner::isQuoted(doubleQuoted,false) == doubleQuoted.begin(), "isQuoted: double-quoted field" );

    std::string_view singleQuoted="'abc";
    check( FieldScanner::isQuoted(singleQuoted,false) == singleQuoted.begin(), "isQuoted: single-quoted field, both quote characters accepted" );
    check( FieldScanner::isQuoted(singleQuoted,true) == singleQuoted.end(), "isQuoted: single quote rejected when onlyDoubleQuote is set" );

    std::string_view plain="abc";
    check( FieldScanner::isQuoted(plain,false) == plain.end(), "isQuoted: plain field, not quoted" );

    std::string_view empty="";
    check( FieldScanner::isQuoted(empty,false) == empty.end(), "isQuoted: empty field, not quoted" );
}

void check_and_recover_quoted_value()
{
    std::string text="ANS \"Australian National Spheroid (ANS)\" 6378160 298.25";
    FieldScanner scanner( text );
    auto first=scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    check( first.has_value() && *first == "ANS", "checkAndRecoverQuotedValue: plain field, read via next()" );
    auto second=scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    check( second.has_value() && *second == "Australian National Spheroid (ANS)",
        "checkAndRecoverQuotedValue: quoted field spanning whitespace, read via quotedValue()" );
    auto third=scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    check( third.has_value() && *third == "6378160", "checkAndRecoverQuotedValue: plain field after a quoted one" );
    auto fourth=scanner.checkAndRecoverQuotedValue( true, std::nullopt );
    check( fourth.has_value() && *fourth == "298.25", "checkAndRecoverQuotedValue: last plain field" );
    check( ! scanner.checkAndRecoverQuotedValue( true, std::nullopt ), "checkAndRecoverQuotedValue: nullopt at end of input" );
}

void check_and_recover_quoted_double_value()
{
    FieldScanner scanner( "6378160 \"298.25\" abc" );
    auto first=scanner.checkAndRecoverQuotedDoubleValue( true, std::nullopt );
    check( first.has_value() && *first==6378160, "checkAndRecoverQuotedDoubleValue: plain field, read via next()" );
    auto second=scanner.checkAndRecoverQuotedDoubleValue( true, std::nullopt );
    check( second.has_value() && *second==298.25, "checkAndRecoverQuotedDoubleValue: quoted numeric field" );
    check( ! scanner.checkAndRecoverQuotedDoubleValue( true, std::nullopt ), "checkAndRecoverQuotedDoubleValue: rejects non-numeric text" );
    check( ! scanner.checkAndRecoverQuotedDoubleValue( true, std::nullopt ), "checkAndRecoverQuotedDoubleValue: nullopt at end of input" );
}

void check_parse_double()
{
    auto value = parse_double("1.5");
    check( value.has_value() && *value==1.5, "parse_double: plain valid value" );
    check( ! parse_double("abc"), "parse_double: rejects non-numeric text" );
    check( ! parse_double("1.5x"), "parse_double: rejects trailing garbage" );
    value = parse_double("-1.5");
    check( value.has_value() && *value==-1.5, "parse_double: accepts a negative value" );
}

void check_parse_positive_double()
{
    auto value = parse_positive_double("1.5");
    check( value.has_value() && *value==1.5, "parse_positive_double: plain valid value" );
    check( ! parse_positive_double("abc"), "parse_positive_double: rejects non-numeric text" );
    check( ! parse_positive_double("1.5x"), "parse_positive_double: rejects trailing garbage" );
    check( ! parse_positive_double("-1.5"), "parse_positive_double: rejects a negative value" );
    check( ! parse_positive_double("0"), "parse_positive_double: rejects zero (must be strictly positive)" );
}

void check_parse_leading_long()
{
    auto value = parse_leading<long>("123");
    check( value.has_value() && *value==123, "parse_leading<long>: plain digits" );
    value = parse_leading<long>("42abc");
    check( value.has_value() && *value==42, "parse_leading<long>: stops at the first non-digit" );
    value = parse_leading<long>("+7");
    check( value.has_value() && *value==7, "parse_leading<long>: accepts a leading plus sign" );
    value = parse_leading<long>("-7");
    check( value.has_value() && *value==-7, "parse_leading<long>: accepts a leading minus sign" );
    value = parse_leading<long>("007");
    check( value.has_value() && *value==7, "parse_leading<long>: ignores leading zeros" );
    check( ! parse_leading<long>("abc"), "parse_leading<long>: rejects text that isn't a number" );
    check( ! parse_leading<long>(""), "parse_leading<long>: rejects an empty field" );
}

void check_compare_ignoring_case()
{
    check( compare_ignoring_case("abc","abc") == 0, "compare_ignoring_case: identical strings" );
    check( compare_ignoring_case("abc","ABC") == 0, "compare_ignoring_case: equal apart from case" );
    check( compare_ignoring_case("","") == 0, "compare_ignoring_case: two empty strings" );
    check( compare_ignoring_case("abc","abd") == -1, "compare_ignoring_case: first sorts before second" );
    check( compare_ignoring_case("abd","abc") == 1, "compare_ignoring_case: first sorts after second" );
    check( compare_ignoring_case("ab","abc") == -1, "compare_ignoring_case: a prefix sorts before the longer string" );
    check( compare_ignoring_case("abc","ab") == 1, "compare_ignoring_case: a longer string sorts after its prefix" );
    check( compare_ignoring_case("","a") == -1, "compare_ignoring_case: empty sorts before non-empty" );
    check( compare_ignoring_case("B","a") == 1, "compare_ignoring_case: compares by letter, not by case" );
    // Folding is to lower case, so '_' (between 'Z' and 'a') sorts before any letter
    check( compare_ignoring_case("_","a") == -1, "compare_ignoring_case: underscore sorts before letters" );
    check( compare_ignoring_case("_","A") == -1, "compare_ignoring_case: underscore sorts before upper case letters" );
    // A byte above 127 must not be treated as a negative char
    check( compare_ignoring_case("\xE9","a") == 1, "compare_ignoring_case: high bytes sort after ASCII" );
}

void check_copy_field()
{
    char buf[8];
    copy_field( "abc", buf, sizeof(buf) );
    check( std::string(buf)=="abc", "copy_field: fits with room to spare" );
    copy_field( "abcdefgh", buf, sizeof(buf) );
    check( std::string(buf)=="abcdefg", "copy_field: truncates without error when it doesn't fit" );
    copy_field( "", buf, sizeof(buf) );
    check( std::string(buf)=="", "copy_field: empty field" );
}

} // namespace

int main()
{
    check_next_basic();
    check_next_empty();
    check_remainder();
    check_next_delimiter();
    check_span_preserves_multiple_spaces();
    check_quoted_value_single_field();
    check_quoted_value_spans_fields();
    check_quoted_value_unterminated();
    check_quoted_value_not_followed_by_whitespace();
    check_quoted_value_lenient();
    check_is_quoted();
    check_and_recover_quoted_value();
    check_and_recover_quoted_double_value();
    check_parse_double();
    check_parse_positive_double();
    check_parse_leading_long();
    check_compare_ignoring_case();
    check_copy_field();

    if( failures == 0 )
    {
        std::cout << "All FieldScanner tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " FieldScanner test(s) failed\n";
    return EXIT_FAILURE;
}
