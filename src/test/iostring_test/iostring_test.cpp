#include "snapconfig.hpp"

// Standalone unit test for util/iostring.cpp's input_string_def-based
// functions. Awaiting migration to CTest - until then, failures are
// reported to stdout and the process exit code is the pass/fail signal
// (0 = all passed), matching fieldscanner_test's own convention.

#include <cstdlib>
#include <iostream>
#include <string>

#include "util/errdef.h"
#include "util/iostring.h"

namespace
{

int failures=0;

void check( bool condition, const std::string &description )
{
    if( condition ) return;
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

void check_next_string_field()
{
    static const std::string text = "abc \"quoted value\" toolongfield";
    input_string_def is( text );
    std::string field;

    check( next_string_field(is.scanner,field,7) == OK, "next_string_field: plain field returns OK" );
    check( field=="abc", "next_string_field: plain field value" );

    check( next_string_field(is.scanner,field,7) == OK, "next_string_field: quoted field returns OK" );
    check( field=="quoted ", "next_string_field: quoted field truncates without error (7 of 12 chars, verbatim)" );

    check( next_string_field(is.scanner,field,7) == OK, "next_string_field: trailing plain field" );
    check( field=="toolong", "next_string_field: truncates to maxlength" );

    check( next_string_field(is.scanner,field,7) == NO_MORE_DATA, "next_string_field: NO_MORE_DATA at end of input" );
    check( field=="toolong", "next_string_field: field unchanged when nothing is read" );
}

void check_next_string_field_missing_data()
{
    static const std::string text = "\"unterminated";
    input_string_def is( text );
    std::string field;
    check( next_string_field(is.scanner,field,19) == MISSING_DATA, "next_string_field: MISSING_DATA on malformed quote" );
}

void check_double_from_string()
{
    static const std::string text = "1.5 abc \"2.5\" \"bad";
    input_string_def is( text );
    double value=0.0;

    check( double_from_string(is.scanner,&value) == OK && value==1.5, "double_from_string: plain valid value" );
    check( double_from_string(is.scanner,&value) == INVALID_DATA, "double_from_string: non-numeric field" );
    check( double_from_string(is.scanner,&value) == OK && value==2.5, "double_from_string: quoted valid value" );
    check( double_from_string(is.scanner,&value) == MISSING_DATA, "double_from_string: malformed quote" );
    check( double_from_string(is.scanner,&value) == NO_MORE_DATA, "double_from_string: NO_MORE_DATA at end of input" );
}

void check_test_next_string_field()
{
    static const std::string text = "abc def";
    input_string_def is( text );
    check( ! test_next_string_field(is.scanner,"xyz"), "test_next_string_field: mismatch returns false and restores" );
    check( test_next_string_field(is.scanner,"ABC"), "test_next_string_field: case-insensitive match consumes" );
    check( test_next_string_field(is.scanner,"def"), "test_next_string_field: matches and consumes the next field" );
    check( ! test_next_string_field(is.scanner,"anything"), "test_next_string_field: false at end of input" );
}

void check_unread_string()
{
    static const std::string text = "abc  def";
    input_string_def is( text );
    is.scanner.next();
    check( unread_string(is)=="  def", "unread_string: verbatim remainder after consuming one field" );
}

int lastStatus=0;
std::string lastMessage;
int recordError( void *, int status, std::string_view message )
{
    lastStatus=status;
    lastMessage=message;
    return status;
}

void check_report_string_error()
{
    static const std::string text = "abc";
    input_string_def is( text );
    is.report_error = recordError;
    report_string_error( is, INVALID_DATA, "test message" );
    check( lastStatus==INVALID_DATA && lastMessage=="test message", "report_string_error: invokes report_error with status/message" );

    lastStatus=0;
    input_string_def is2( text );
    report_string_error( is2, INVALID_DATA, "should not fire" );
    check( lastStatus==0, "report_string_error: no-op when report_error is null" );
}

} // namespace

int main()
{
    check_next_string_field();
    check_next_string_field_missing_data();
    check_double_from_string();
    check_test_next_string_field();
    check_unread_string();
    check_report_string_error();

    if( failures == 0 )
    {
        std::cout << "All iostring tests passed\n";
        return EXIT_SUCCESS;
    }
    std::cout << failures << " iostring test(s) failed\n";
    return EXIT_FAILURE;
}
