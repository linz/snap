// Shared by the unit tests in src/test/: counting the checks that fail, and the exit code that says whether any did.
// A test calls check() and the other functions here, and ends its main() with
//
//     return snap_test::finish("name_of_the_test");
//
// A failed check is reported to stdout, and the process exit code is the pass/fail signal (0 = all passed).

#pragma once

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace snap_test {

namespace detail {

/// How many checks have failed so far in this program.
inline int failureCount = 0;

/// \return the value as text for a failure message: a floating-point value with every digit that it has, so that two
/// values that differ in a late digit are not shown as the same, and a bool as true or false. It is written to a stream
/// of its own, not std::cout, as the precision of a stream stays set.
template <typename Value>
std::string format(const Value& value) {
    std::ostringstream text;
    text.precision(std::numeric_limits<double>::max_digits10);
    text << std::boolalpha << value;
    return text.str();
}

}  // namespace detail

/// Counts a failure, and prints the message, if the condition is false.
inline void check(const bool condition,
                  const std::string& messageOnError) {  ///< printed if the check fails, and says what was checked
    if (condition) {
        return;
    }
    ++detail::failureCount;
    std::cout << "FAIL: " << messageOnError << "\n";
}

/// Counts a failure, and prints the message with both values, if the values are not equal. The values can be of any
/// types that can be compared with == and written to a stream.
template <typename Actual, typename Expected>
void checkEqual(const Actual& actual,
                const Expected& expected,
                const std::string& messageOnError) {  ///< printed with the values if the check fails
    if (actual == expected) {
        return;
    }
    ++detail::failureCount;
    std::cout << "FAIL: " << messageOnError << ": expected '" << detail::format(expected) << "', got '"
              << detail::format(actual) << "'\n";
}

/// Counts a failure, and prints the message with both values, if the values differ by more than the tolerance.
inline void checkEqual(const double actual,
                       const double expected,
                       const double tolerance,               ///< the most that the values may differ by
                       const std::string& messageOnError) {  ///< printed with the values if the check fails
    if (std::fabs(actual - expected) <= tolerance) {
        return;
    }
    ++detail::failureCount;
    std::cout << "FAIL: " << messageOnError << ": expected '" << detail::format(expected) << "' within '"
              << detail::format(tolerance) << "', got '" << detail::format(actual) << "'\n";
}

/// Checks that calling the function throws the exception type, with a message that contains the text. An exception of
/// another type is not caught.
template <typename Exception, typename Function>
void checkThrows(Function function,                    ///< what to call
                 const std::string& messageText,       ///< text that the exception's message must contain
                 const std::string& messageOnError) {  ///< printed if the check fails, and says what was checked
    try {
        function();
    } catch (const Exception& error) {
        check(std::string(error.what()).find(messageText) != std::string::npos,
              messageOnError + ": the message '" + error.what() + "' does not contain '" + messageText + "'");
        return;
    }
    check(false, messageOnError + ": expected an exception, but there was none");
}

/// Prints whether every check passed, and \return the exit code for the program: 0 if all of them did, 1 if not.
inline int finish(const std::string& testName) {
    if (detail::failureCount > 0) {
        std::cout << detail::failureCount << " " << testName << " check(s) failed\n";
        return 1;
    }
    std::cout << testName << ": all checks passed\n";
    return 0;
}

}  // namespace snap_test
