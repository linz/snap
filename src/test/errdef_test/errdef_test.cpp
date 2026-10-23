// Standalone unit test for handle_error in util/errdef.h.
//
// A recording handler is installed with set_error_handler, so each case checks
// exactly what handle_error passes on: the status, the message (a default text
// for the status when none is given), and the second message (the error
// location when none is given). Expected texts are written out by hand from the
// documented behaviour. Fatal errors are not tested because they exit.

#include <optional>
#include <string>
#include <string_view>

#include "test_support.h"
#include "util/errdef.h"

namespace {

using snap_test::check;
using snap_test::checkEqual;

/// What the recording handler was last called with
struct Recorded {
    int calls = 0;
    int status = 0;
    std::string first;
    std::optional<std::string> second;
};

Recorded recorded;

int recording_handler(const int status, const std::string_view first, const error_message second) {
    ++recorded.calls;
    recorded.status = status;
    recorded.first = std::string(first);
    recorded.second = second ? std::optional<std::string>(std::string(*second)) : std::nullopt;
    return status;
}

/// Calls handle_error with the recording handler and returns what it recorded
Recorded report(const int status, const error_message first, const error_message second) {
    recorded = Recorded();
    handle_error(status, first, second);
    return recorded;
}

/// Text of the second message, or a marker when it is absent
std::string second_text(const Recorded& result) {
    return result.second ? "[" + *result.second + "]" : "absent";
}

void test_default_messages() {
    struct Case {
        int status;
        const char* text;
    };
    const Case cases[] = {
        {FILE_OPEN_ERROR, "Error opening file"},
        {FILE_READ_ERROR, "Error reading file"},
        {FILE_WRITE_ERROR, "Error writing file"},
        {UNEXPECTED_EOF, "End of file encountered"},
        {SYNTAX_ERROR, "Syntax error"},
        {INVALID_DATA, "Invalid data error"},
        {MISSING_DATA, "Missing data"},
        {INCONSISTENT_DATA, "Inconsistent data"},
        {TOO_MUCH_DATA, "Too much data"},
        {OPERATION_ABORTED, "Aborted by user"},
        {INFO_ERROR, "Notice"},
        {WARNING_ERROR, "Undefined error"},
    };
    for (const Case& testCase : cases) {
        const Recorded result = report(testCase.status, NO_MESSAGE, NO_MESSAGE);
        checkEqual(result.first, testCase.text, "default message for status " + std::to_string(testCase.status));
        check(result.status == testCase.status, "status passed on for " + std::to_string(testCase.status));
    }
}

void test_given_message_is_kept() {
    checkEqual(report(INVALID_DATA, "my message", NO_MESSAGE).first, "my message", "given message");

    // An empty message is a message, not an absent one, so no default is used
    const Recorded empty = report(INVALID_DATA, std::string_view(), NO_MESSAGE);
    checkEqual(empty.first, "", "empty message is kept");
}

void test_second_message() {
    set_error_location(NO_MESSAGE);
    checkEqual(second_text(report(INVALID_DATA, "m", NO_MESSAGE)), "absent", "no second message, no location");
    checkEqual(second_text(report(INVALID_DATA, "m", "detail")), "[detail]", "given second message");
    checkEqual(second_text(report(INVALID_DATA, "m", std::string_view())), "[]", "empty second message, no location");
}

void test_location() {
    set_error_location("in file x line 3");
    checkEqual(second_text(report(INVALID_DATA, "m", NO_MESSAGE)), "[in file x line 3]",
               "location replaces an absent second message");
    checkEqual(second_text(report(INVALID_DATA, "m", "detail")), "[detail]", "given second message beats the location");
    checkEqual(second_text(report(INVALID_DATA, "m", std::string_view())), "[]",
               "empty second message beats the location");

    set_error_location(NO_MESSAGE);
    checkEqual(second_text(report(INVALID_DATA, "m", NO_MESSAGE)), "absent", "location cleared");

    // An empty location is the same as none
    set_error_location(std::string_view());
    checkEqual(second_text(report(INVALID_DATA, "m", NO_MESSAGE)), "absent", "empty location");

    // Longer than the 255 characters that an earlier version truncated to
    const std::string longLocation(300, 'x');
    set_error_location(longLocation);
    checkEqual(second_text(report(INVALID_DATA, "m", NO_MESSAGE)), "[" + longLocation + "]", "long location");
    set_error_location(NO_MESSAGE);
}

void test_reporting_conditions() {
    check(report(OK, "m", NO_MESSAGE).calls == 0, "OK is not reported");
    check(report(NO_MORE_DATA, "m", NO_MESSAGE).calls == 0, "NO_MORE_DATA is not reported");
    check(report(INVALID_DATA, "m", NO_MESSAGE).calls == 1, "a warning is reported");

    const int oldLevel = set_error_level(WARNING_ERROR);
    check(report(INFO_ERROR, "m", NO_MESSAGE).calls == 0, "info is below the reporting level");
    check(report(INVALID_DATA, "m", NO_MESSAGE).calls == 1, "a warning meets the reporting level");
    set_error_level(oldLevel);
}

void test_error_count() {
    const int before = get_error_count();
    report(INFO_ERROR, "m", NO_MESSAGE);
    check(get_error_count() == before, "info is not counted");
    report(INVALID_DATA, "m", NO_MESSAGE);
    check(get_error_count() == before + 1, "a warning is counted");
}

}  // namespace

int main() {
    set_error_handler(recording_handler);
    set_error_level(0);

    test_default_messages();
    test_given_message_is_kept();
    test_second_message();
    test_location();
    test_reporting_conditions();
    test_error_count();

    set_error_handler(DEFAULT_ERROR_HANDLER);

    return snap_test::finish("errdef_test");
}
