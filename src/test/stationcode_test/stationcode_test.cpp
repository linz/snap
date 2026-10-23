#include "snapconfig.hpp"

// Standalone unit test for the StationCode class in network/network.h.
//
// StationCode holds the code in a fixed array of STNCODELEN + 1 characters, and has its own c_str(), which returns that
// array for the printf family and other C interfaces. A C reader stops at the first NUL, so as well as the string_view
// that a StationCode converts to (which finds the first NUL itself), the cases check that the text c_str() gives, up to
// its first NUL, is exactly the expected code: that the array is terminated where it should be, and that nothing of an
// earlier, longer code is left behind.

#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

#include "network/network.h"
#include "test_support.h"

namespace {

using snap_test::check;

/// Compares as string views, so that a type that converts to one can be passed as it is, and so that a const char* is
/// compared by its text up to the first NUL, as a C reader reads it, not by its address.
void check_equal(const std::string_view actual, const std::string_view expected, const std::string& messageOnError) {
    snap_test::checkEqual(actual, expected, messageOnError);
}

void test_default_is_empty() {
    const StationCode code;
    check(code.empty(), "default code is empty");
    const auto size = code.size();
    check(size == 0, "default code has size 0");
    check_equal(code, "", "default code as a string_view");
    check_equal(code.c_str(), "", "default code as a C string");
}

void test_assign() {
    StationCode code;
    code.assign("ABC1");
    check(!code.empty(), "assigned code is not empty");
    check(code.size() == 4, "assigned code has size 4");
    check_equal(code, "ABC1", "assigned code as a string_view");
    check_equal(code.c_str(), "ABC1", "assigned code is NUL terminated");
}

void test_assign_truncates_to_the_code_length() {
    const std::string exact(STNCODELEN, 'X');
    StationCode code;
    code.assign(exact);
    check_equal(code, exact, "a code of exactly STNCODELEN characters is kept");
    check_equal(code.c_str(), exact, "a code of exactly STNCODELEN characters is NUL terminated");

    code.assign(exact + "YZ");
    check_equal(code, exact, "a longer code is truncated to STNCODELEN characters");
    check_equal(code.c_str(), exact, "a truncated code is NUL terminated");
}

void test_assign_replaces_the_whole_code() {
    StationCode code;
    code.assign("ABCDEFGHIJ");
    code.assign("AB");
    check_equal(code, "AB", "a shorter code replaces a longer one");
    check_equal(code.c_str(), "AB", "nothing of the longer code is left behind");

    code.assign(std::string_view());
    check(code.empty(), "assigning an empty code empties it");
    const auto size = code.size();
    check(size == 0, "assigning an empty code gives size 0");
}

void test_unterminated_code_is_bounded() {
    // A code read from disk could fill all of its bytes
    char bytes[STNCODELEN + 1];
    std::memset(bytes, 'Q', sizeof(bytes));
    StationCode code;
    std::memcpy(static_cast<void*>(&code), bytes, sizeof(code));
    check(code.size() == sizeof(bytes), "an unterminated code has the full size");
    check_equal(code, std::string_view(bytes, sizeof(bytes)), "an unterminated code does not read past its bytes");
}

void test_embedded_nul_ends_the_code() {
    char bytes[STNCODELEN + 1];
    std::memset(bytes, 'Q', sizeof(bytes));
    bytes[3] = '\0';
    StationCode code;
    std::memcpy(static_cast<void*>(&code), bytes, sizeof(code));
    check(code.size() == 3, "the code ends at the first NUL");
    check_equal(code, "QQQ", "the code is the characters before the first NUL");
}

void test_layout() {
    // The disk table writes the code as STNCODELEN+1 bytes at the start of a station
    check(sizeof(StationCode) == STNCODELEN + 1, "size of StationCode");
    check(alignof(StationCode) == 1, "alignment of StationCode");
    check(offsetof(station, Code) == 0, "Code is the first member of station");
}

}  // namespace

int main() {
    test_default_is_empty();
    test_assign();
    test_assign_truncates_to_the_code_length();
    test_assign_replaces_the_whole_code();
    test_unterminated_code_is_bounded();
    test_embedded_nul_ends_the_code();
    test_layout();

    return snap_test::finish("stationcode_test");
}
