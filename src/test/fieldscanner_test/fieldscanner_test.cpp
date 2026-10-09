#include "snapconfig.hpp"

// Standalone unit test for util/fieldscanner.hpp. Awaiting migration to
// CTest - until then, failures are reported to stdout and the process exit
// code is the pass/fail signal (0 = all passed), matching how other
// test-only tools in src/test/ are checked.

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "util/fieldscanner.hpp"
#include "util/pi.h"

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

void check_at_end()
{
    FieldScanner scanner( "abc  def  " );
    check( ! scanner.atEnd(), "atEnd: false before any field is read" );
    scanner.next();
    check( ! scanner.atEnd(), "atEnd: false with a field still to read" );
    check( scanner.remainder() == "  def  ", "atEnd: does not consume anything" );
    scanner.next();
    check( scanner.atEnd(), "atEnd: true when only trailing whitespace is left" );
    FieldScanner empty( "" );
    check( empty.atEnd(), "atEnd: true on an empty input" );
}

void check_skip_if_next()
{
    FieldScanner scanner( "  *12 #cmd" );
    check( ! scanner.skipIfNext( '#' ), "skipIfNext: false when a different character is next" );
    check( scanner.remainder() == "  *12 #cmd", "skipIfNext: nothing consumed on a mismatch" );
    check( scanner.skipIfNext( '*' ), "skipIfNext: true when the character is next after whitespace" );
    check( scanner.next() == "12", "skipIfNext: the rest of the field is left to read" );
    check( scanner.skipIfNext( '#' ), "skipIfNext: character at the start of a field" );
    check( scanner.next() == "cmd", "skipIfNext: the field after the character" );
    check( ! scanner.skipIfNext( '#' ), "skipIfNext: false at the end of the input" );
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

void check_next_until()
{
    // Adjacent terminators give an empty field, and the terminator that ended
    // each field is reported and consumed.
    {
        FieldScanner scanner( "a,,b c" );
        char terminator='x';
        check( scanner.nextUntil( ", ", terminator ) == "a" && terminator == ',', "nextUntil: first field ends at a comma" );
        check( scanner.nextUntil( ", ", terminator ) == "" && terminator == ',', "nextUntil: empty field between adjacent terminators" );
        check( scanner.nextUntil( ", ", terminator ) == "b" && terminator == ' ', "nextUntil: third field ends at a space" );
        check( scanner.nextUntil( ", ", terminator ) == "c" && terminator == 0, "nextUntil: the rest of the text is the last field, with terminator 0" );
        check( ! scanner.nextUntil( ", ", terminator ) && terminator == 0, "nextUntil: nullopt once nothing is left" );
    }
    // Nothing is skipped before the field, so leading terminators and
    // whitespace are part of the result.
    {
        FieldScanner scanner( ",  a, b" );
        char terminator=0;
        check( scanner.nextUntil( ",", terminator ) == "", "nextUntil: a leading terminator gives an empty first field" );
        check( scanner.nextUntil( ",", terminator ) == "  a", "nextUntil: whitespace before a field is kept" );
        check( scanner.nextUntil( ",", terminator ) == " b" && terminator == 0, "nextUntil: whitespace is kept in the last field" );
    }
    // A trailing terminator ends the last field, and leaves nothing after it.
    {
        FieldScanner scanner( "a," );
        char terminator=0;
        check( scanner.nextUntil( ",", terminator ) == "a" && terminator == ',', "nextUntil: field before a trailing terminator" );
        check( ! scanner.nextUntil( ",", terminator ), "nextUntil: no empty field after a trailing terminator" );
        check( scanner.remainder() == "", "nextUntil: nothing left after a trailing terminator" );
    }
    // An input with no terminator is one field, and empty input is nullopt.
    {
        FieldScanner scanner( "abc" );
        char terminator='x';
        check( scanner.nextUntil( ",", terminator ) == "abc" && terminator == 0, "nextUntil: no terminator returns all the text" );
        FieldScanner empty( "" );
        check( ! empty.nextUntil( ",", terminator ), "nextUntil: nullopt on empty input" );
    }
    // The position is left after the terminator.
    {
        FieldScanner scanner( "a;b;c" );
        char terminator=0;
        scanner.nextUntil( ";", terminator );
        check( scanner.remainder() == "b;c", "nextUntil: position left after the terminator" );
    }
}

void check_next_token()
{
    // Runs of delimiters separate fields as a single one does, and the last
    // field needs no delimiter after it.
    {
        FieldScanner scanner( "a,,b,c" );
        check( scanner.nextToken(',') == "a", "nextToken: first field" );
        check( scanner.nextToken(',') == "b", "nextToken: empty field between adjacent delimiters skipped" );
        check( scanner.nextToken(',') == "c", "nextToken: last field with no delimiter after it" );
        check( ! scanner.nextToken(','), "nextToken: nullopt after the last field" );
    }
    // Leading and trailing delimiters.
    {
        FieldScanner scanner( ",,a,b,," );
        check( scanner.nextToken(',') == "a", "nextToken: leading delimiters skipped" );
        check( scanner.nextToken(',') == "b", "nextToken: second field" );
        check( ! scanner.nextToken(','), "nextToken: trailing delimiters are not a field" );
    }
    // The same fields however many delimiters separate them.
    {
        FieldScanner many( "###a##b##c" );
        FieldScanner one( "#a#b#c" );
        for( const char *field : { "a", "b", "c" } )
        {
            check( many.nextToken('#') == field, std::string( "nextToken: " ) + field + " after repeated delimiters" );
            check( one.nextToken('#') == field, std::string( "nextToken: " ) + field + " after single delimiters" );
        }
    }
    // Position is left at the delimiter after the field.
    {
        FieldScanner scanner( "a,b" );
        scanner.nextToken(',');
        check( scanner.remainder() == ",b", "nextToken: position left at the delimiter after the field" );
    }
    // Only delimiters, or nothing at all.
    {
        FieldScanner delimiters( ",,," );
        check( ! delimiters.nextToken(','), "nextToken: nullopt when there are only delimiters" );
        check( delimiters.remainder() == "", "nextToken: only delimiters are all consumed" );
        FieldScanner empty( "" );
        check( ! empty.nextToken(','), "nextToken: nullopt on empty input" );
    }
    // Whitespace is not special: only the delimiter separates fields.
    {
        FieldScanner scanner( ";a b; c ;" );
        check( scanner.nextToken(';') == "a b", "nextToken: whitespace inside a field kept" );
        check( scanner.nextToken(';') == " c ", "nextToken: whitespace around a field kept" );
        check( ! scanner.nextToken(';'), "nextToken: nullopt after the last field" );
    }
    // A set of delimiters, as strtok takes: any one of them separates fields.
    {
        FieldScanner scanner( " \t a\t\tb \r\n c\n" );
        check( scanner.nextToken(" \t\r\n") == "a", "nextToken set: first field after mixed leading delimiters" );
        check( scanner.nextToken(" \t\r\n") == "b", "nextToken set: tabs and spaces together separate fields" );
        check( scanner.nextToken(" \t\r\n") == "c", "nextToken set: carriage return and newline separate fields" );
        check( ! scanner.nextToken(" \t\r\n"), "nextToken set: nullopt after trailing delimiters" );
    }
    // Unlike next(), only the characters given separate fields: a vertical
    // tab or form feed is not one of strtok's " \t\r\n".
    {
        FieldScanner scanner( "a\vb\fc d" );
        check( scanner.nextToken(" \t\r\n") == "a\vb\fc", "nextToken set: vertical tab and form feed are not delimiters" );
        check( scanner.nextToken(" \t\r\n") == "d", "nextToken set: space still separates" );
        FieldScanner nextScanner( "a\vb" );
        check( nextScanner.next() == "a", "next: vertical tab separates fields, unlike the set above" );
    }
    // A single character set behaves as the char version does.
    {
        FieldScanner scanner( "#a##b" );
        check( scanner.nextToken("#") == "a", "nextToken set: single character, first field" );
        check( scanner.nextToken("#") == "b", "nextToken set: single character, repeated delimiters" );
        check( ! scanner.nextToken("#"), "nextToken set: single character, nullopt at the end" );
        FieldScanner empty( "" );
        check( ! empty.nextToken(" \t\r\n"), "nextToken set: nullopt on empty input" );
    }
    // snaplist's angle_format line, as read from its configuration file
    // (backslash escapes are still raw text here, decoded later).
    {
        FieldScanner scanner( "###\\xC2\\xB0##'##\"" );
        check( scanner.nextToken('#') == "\\xC2\\xB0", "nextToken: angle format, text after the degrees" );
        check( scanner.nextToken('#') == "'", "nextToken: angle format, text after the minutes" );
        check( scanner.nextToken('#') == "\"", "nextToken: angle format, text after the seconds" );
        check( ! scanner.nextToken('#'), "nextToken: angle format, nothing after that" );
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

void check_parse_int()
{
    auto value = parse_int("42");
    check( value.has_value() && *value==42, "parse_int: plain digits" );
    value = parse_int("-7");
    check( value.has_value() && *value==-7, "parse_int: accepts a minus sign" );
    value = parse_int("+7");
    check( value.has_value() && *value==7, "parse_int: accepts a plus sign" );
    check( ! parse_int("42abc"), "parse_int: rejects trailing garbage" );
    check( ! parse_int("4.5"), "parse_int: rejects a decimal point" );
    check( ! parse_int("abc"), "parse_int: rejects non-numeric text" );
    check( ! parse_int(""), "parse_int: rejects an empty field" );
    check( ! parse_int("\"42\""), "parse_int: does not accept a quoted value" );
    check( ! parse_int("99999999999"), "parse_int: rejects a value out of range" );
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

    // The length limited form compares only the first maxLength characters
    check( compare_ignoring_case("#Layer name","#layer",6) == 0, "compare_ignoring_case: limited compare ignores the tail" );
    check( compare_ignoring_case("#layer","#LAYER",6) == 0, "compare_ignoring_case: limited compare ignores case" );
    check( compare_ignoring_case("#layx","#layer",6) == 1, "compare_ignoring_case: limited compare sorts a short string by its content" );
    check( compare_ignoring_case("#lay","#layer",6) == -1, "compare_ignoring_case: limited compare of a short prefix sorts first" );
    check( compare_ignoring_case("#lay","#layer",3) == 0, "compare_ignoring_case: limit shorter than both strings" );
    check( compare_ignoring_case("abc","abd",0) == 0, "compare_ignoring_case: zero limit always matches" );
}

void check_is_name_match()
{
    check( is_name_match("abc","abc"), "is_name_match: identical names" );
    check( is_name_match("abc","ABC"), "is_name_match: equal apart from case" );
    check( is_name_match("",""), "is_name_match: two empty names" );
    check( ! is_name_match("abc","abd"), "is_name_match: different letters" );
    check( ! is_name_match("","a"), "is_name_match: empty and non-empty" );
    check( ! is_name_match("ab","abc"), "is_name_match: a prefix is not a match" );
    check( ! is_name_match("abc","ab"), "is_name_match: a longer name is not a match" );
    check( is_name_match("Day Shift","day_shift"), "is_name_match: space matches underscore" );
    check( is_name_match("DAY_SHIFT","day shift"), "is_name_match: case and space together" );
    check( is_name_match("a\tb","a b"), "is_name_match: a control character matches a space" );
    check( is_name_match("a\xE9","a_"), "is_name_match: a character outside ASCII matches underscore" );
    check( is_name_match("a\xE9","a\xE8"), "is_name_match: two characters outside ASCII match each other" );
    check( ! is_name_match("a-b","a_b"), "is_name_match: hyphen is not treated as underscore" );
    check( ! is_name_match("a.b","a_b"), "is_name_match: full stop is not treated as underscore" );
    check( ! is_name_match("a1","a2"), "is_name_match: different digits" );
}

void check_read_remaining_text()
{
    std::string value = "unchanged";
    FieldScanner scanner( "first   the rest\r of  it\x1a " );
    scanner.next();
    check( read_remaining_text( scanner, value, 100 ) == FieldResult::Ok && value == "the rest of  it ",
           "read_remaining_text: skips leading whitespace, drops carriage return and Ctrl-Z, keeps interior spacing" );
    check( scanner.atEnd() && scanner.remainder().empty(), "read_remaining_text: leaves the scanner at the end" );
    check( read_remaining_text( scanner, value, 100 ) == FieldResult::NoMoreData && value == "the rest of  it ",
           "read_remaining_text: NoMoreData at the end, value unchanged" );

    FieldScanner longer( "abcdefgh" );
    check( read_remaining_text( longer, value, 4 ) == FieldResult::Ok && value == "abcd", "read_remaining_text: cut short without error" );

    FieldScanner blank( "   " );
    check( read_remaining_text( blank, value, 100 ) == FieldResult::NoMoreData, "read_remaining_text: only whitespace is NoMoreData" );
}

void check_read_whole_number_fields()
{
    FieldScanner scanner( "12 -7 +5 1.5 abc 99999999999 \"4\" 123456 " );
    int i = 0;
    long l = 0;
    check( read_int_field( scanner, i ) == FieldResult::Ok && i == 12, "read_int_field: plain value" );
    check( read_int_field( scanner, i ) == FieldResult::Ok && i == -7, "read_int_field: negative value" );
    check( read_int_field( scanner, i ) == FieldResult::Ok && i == 5, "read_int_field: leading plus" );
    check( read_int_field( scanner, i ) == FieldResult::InvalidValue && i == 5, "read_int_field: a decimal is not a whole number, value unchanged" );
    check( read_int_field( scanner, i ) == FieldResult::InvalidValue, "read_int_field: not a number" );
    check( read_int_field( scanner, i ) == FieldResult::InvalidValue && i == 5, "read_int_field: value too big for an int" );
    check( read_int_field( scanner, i ) == FieldResult::Ok && i == 4, "read_int_field: quoted value" );
    check( read_long_field( scanner, l ) == FieldResult::Ok && l == 123456L, "read_long_field: plain value" );
    check( read_long_field( scanner, l ) == FieldResult::NoMoreData && l == 123456L, "read_long_field: NoMoreData at the end" );
}

bool near( double value, double expected )
{
    return std::fabs( value - expected ) < 1.0e-12;
}

void check_read_angle_fields()
{
    double radians = 0.0;

    FieldScanner deg( "45.5 x" );
    check( read_degree_angle_field( deg, radians ) == FieldResult::Ok && near( radians, 45.5*DTOR ), "read_degree_angle_field: degrees to radians" );
    check( read_degree_angle_field( deg, radians ) == FieldResult::InvalidValue, "read_degree_angle_field: not a number" );

    FieldScanner dms( "12 30 15.5 -10 15 0 12 x 5" );
    check( read_dms_angle_field( dms, radians ) == FieldResult::Ok
           && near( radians, (12 + 30/60.0 + 15.5/3600.0)*DTOR ), "read_dms_angle_field: degrees, minutes and seconds" );
    check( read_dms_angle_field( dms, radians ) == FieldResult::Ok
           && near( radians, (-10 + 15/60.0)*DTOR ), "read_dms_angle_field: a negative degrees field is added to the positive minutes" );
    check( read_dms_angle_field( dms, radians ) == FieldResult::InvalidValue, "read_dms_angle_field: invalid minutes" );
    FieldScanner dmsShort( "12 30" );
    check( read_dms_angle_field( dmsShort, radians ) == FieldResult::NoMoreData, "read_dms_angle_field: seconds missing" );

    FieldScanner hp( "12.3015 12.30155 .3045 12 -1.3000 1.30 1.3a00 1.30000z" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::Ok
           && near( radians, (12 + 30/60.0 + 15/3600.0)*DTOR ), "read_hp_angle_field: ddd.mmss" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::Ok
           && near( radians, (12 + 30/60.0 + 15.5/3600.0)*DTOR ), "read_hp_angle_field: decimal fraction of a second" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::Ok
           && near( radians, (30/60.0 + 45/3600.0)*DTOR ), "read_hp_angle_field: degrees omitted" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::InvalidValue, "read_hp_angle_field: no decimal point" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::InvalidValue, "read_hp_angle_field: negative angle" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::InvalidValue, "read_hp_angle_field: seconds missing" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::InvalidValue, "read_hp_angle_field: non-digit in the fraction" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::InvalidValue, "read_hp_angle_field: non-digit after the seconds" );
    check( read_hp_angle_field( hp, radians ) == FieldResult::NoMoreData, "read_hp_angle_field: NoMoreData at the end" );
}

} // namespace

int main()
{
    check_next_basic();
    check_next_empty();
    check_remainder();
    check_at_end();
    check_skip_if_next();
    check_next_delimiter();
    check_next_token();
    check_next_until();
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
    check_parse_int();
    check_parse_positive_double();
    check_parse_leading_long();
    check_compare_ignoring_case();
    check_is_name_match();
    check_read_remaining_text();
    check_read_whole_number_fields();
    check_read_angle_fields();

    if( failures == 0 )
    {
        std::cout << "All FieldScanner tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " FieldScanner test(s) failed\n";
    return EXIT_FAILURE;
}
