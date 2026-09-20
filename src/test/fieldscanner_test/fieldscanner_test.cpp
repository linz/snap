#include "snapconfig.hpp"

// Standalone unit test for util/fieldscanner.hpp. Awaiting migration to
// CTest - until then, failures are reported to stdout and the process exit
// code is the pass/fail signal (0 = all passed), matching how other
// test-only tools in src/test/ are checked.

#include <cstdlib>
#include <iostream>
#include <string>

#include "util/fieldscanner.hpp"

namespace
{

int failures=0;

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

void check_span_preserves_multiple_spaces()
{
    FieldScanner scanner( "one  two   three" );
    auto start=scanner.pos();
    scanner.next();
    scanner.next();
    auto end=scanner.pos();
    check( scanner.span(start,end) == "one  two", "span: verbatim text, multi-space run untouched" );
}

void check_quoted_value_single_field()
{
    std::string text="key=\"short\" next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;  // past '=' and the opening quote
    auto value=scanner.quotedValue( openQuote, '"' );
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
    auto value=scanner.quotedValue( openQuote, '"' );
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
    auto value=scanner.quotedValue( openQuote, '"' );
    check( ! value.has_value(), "quotedValue: nullopt when the closing quote never appears" );
}

void check_quoted_value_not_followed_by_whitespace()
{
    std::string text="key=\"abc\"trailing next";
    FieldScanner scanner( text );
    auto field=scanner.next();
    auto equalsPos=field->find('=');
    auto openQuote=field->begin()+equalsPos+2;
    auto value=scanner.quotedValue( openQuote, '"' );
    check( ! value.has_value(), "quotedValue: nullopt when the closing quote isn't followed by whitespace or end of input" );
}

void check_parse_positive_double()
{
    double value=0.0;
    check( parse_positive_double("1.5",value) && value==1.5, "parse_positive_double: plain valid value" );
    check( ! parse_positive_double("abc",value), "parse_positive_double: rejects non-numeric text" );
    check( ! parse_positive_double("1.5x",value), "parse_positive_double: rejects trailing garbage" );
    check( ! parse_positive_double("-1.5",value), "parse_positive_double: rejects a negative value" );
    check( ! parse_positive_double("0",value), "parse_positive_double: rejects zero (must be strictly positive)" );
}

} // namespace

int main()
{
    check_next_basic();
    check_next_empty();
    check_remainder();
    check_span_preserves_multiple_spaces();
    check_quoted_value_single_field();
    check_quoted_value_spans_fields();
    check_quoted_value_unterminated();
    check_quoted_value_not_followed_by_whitespace();
    check_parse_positive_double();

    if( failures == 0 )
    {
        std::cout << "All FieldScanner tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " FieldScanner test(s) failed\n";
    return EXIT_FAILURE;
}
