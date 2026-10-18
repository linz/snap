// Standalone unit test for RangedInt (util/rangedint.h): that a value is only made within its limits, whatever it is
// made from, including a number that would wrap round to a value within them if it were first narrowed to the type that
// holds the value. Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all passed),
// as for the other test-only tools in src/test/.

#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "util/rangedint.h"

namespace {

/// Held in a byte, so that a number made into one can wrap.
using Month = RangedInt<std::uint8_t, 1, 12>;

/// Has negative limits.
using Offset = RangedInt<std::int16_t, -1439, 1439>;

/// Has limits that are not near 0.
using Year = RangedInt<std::uint16_t, 1000, 4000>;

/// Has one value.
using Only = RangedInt<int, 7, 7>;

static_assert(Month::kMinimum == 1 && Month::kMaximum == 12, "a RangedInt has its limits as constants");
static_assert(Offset::kMinimum == -1439 && Offset::kMaximum == 1439, "a RangedInt has its limits as constants");
static_assert(!std::is_default_constructible_v<Month>, "a RangedInt has no default value");
static_assert(std::is_copy_constructible_v<Month>, "a RangedInt can be copied");
static_assert(!std::is_copy_assignable_v<Month>, "a RangedInt can not be assigned to");
static_assert(!std::is_convertible_v<int, Month>, "a number must be made into a RangedInt explicitly");
static_assert(!std::is_same_v<Month, RangedInt<std::uint8_t, 0, 59>>, "RangedInts with different limits differ");

int failures = 0;

void check(const bool condition, const std::string& description) {
    if (condition) {
        return;
    }
    ++failures;
    std::cout << "FAIL: " << description << "\n";
}

/// \return whether making a Ranged from the number throws an std::out_of_range.
template <typename Ranged>
bool constructionThrows(const long long number) {
    try {
        const Ranged ranged(number);
        return false;
    } catch (const std::out_of_range&) {
        return true;
    }
}

/// Checks that the number is accepted by both ways of making a Ranged, and that its value is the number.
template <typename Ranged>
void checkAccepted(const std::string& type, const long long number) {
    const std::string description = type + " from " + std::to_string(number);
    if (constructionThrows<Ranged>(number)) {
        check(false, description + " threw");
    } else {
        check(Ranged(number).value == number, description + " does not have the number as its value");
    }
    const std::optional<Ranged> made = Ranged::from(number);
    check(made.has_value(), description + " by from() gave nothing");
    if (made) {
        check(made->value == number, description + " by from() does not have the number as its value");
    }
}

/// Checks that the number is refused by both ways of making a Ranged.
template <typename Ranged>
void checkRefused(const std::string& type, const long long number) {
    const std::string description = type + " from " + std::to_string(number);
    check(constructionThrows<Ranged>(number), description + " did not throw");
    check(!Ranged::from(number).has_value(), description + " by from() gave a value");
}

void checkLimits() {
    // Each limit and the numbers either side of it.
    for (const long long number : {1, 2, 11, 12}) {
        checkAccepted<Month>("Month", number);
    }
    for (const long long number :
         {0LL, 13LL, -1LL, std::numeric_limits<long long>::min(), std::numeric_limits<long long>::max()}) {
        checkRefused<Month>("Month", number);
    }
    for (const long long number : {-1439, -1, 0, 1, 1439}) {
        checkAccepted<Offset>("Offset", number);
    }
    for (const long long number : {-1440, 1440, -100000, 100000}) {
        checkRefused<Offset>("Offset", number);
    }
    for (const long long number : {1000, 1001, 3999, 4000}) {
        checkAccepted<Year>("Year", number);
    }
    for (const long long number : {0, 999, 4001, 65536}) {
        checkRefused<Year>("Year", number);
    }
    checkAccepted<Only>("Only", 7);
    checkRefused<Only>("Only", 6);
    checkRefused<Only>("Only", 8);
}

/// A number that is too big for the type that holds the value must be refused, not wrapped round into the limits.
void checkNoWrapping() {
    // 256 + 5 is 5 in a byte, which is a month.
    checkRefused<Month>("Month", 261);
    // 256 is 0, and 300 is 44, in a byte.
    checkRefused<Month>("Month", 256);
    checkRefused<Month>("Month", 300);
    // 65536 + 2000 is 2000 in 16 bits, which is a year.
    checkRefused<Year>("Year", 65536 + 2000);
    // 65536 + 100 is 100 in 16 bits, which is an offset.
    checkRefused<Offset>("Offset", 65536 + 100);
    // 2^32 + 7 is 7 in 32 bits.
    checkRefused<Only>("Only", (1LL << 32) + 7);
}

void checkMessage() {
    try {
        const Month month(300);
        check(false, "Month from 300 did not throw, and gave " + std::to_string(month.value));
    } catch (const std::out_of_range& error) {
        const std::string message = error.what();
        check(message.find("300") != std::string::npos,
              "the message for Month from 300 does not contain 300: " + message);
        check(message.find('1') != std::string::npos && message.find("12") != std::string::npos,
              "the message for Month from 300 does not contain its limits 1 and 12: " + message);
    }
    try {
        const Offset offset(-5000);
        check(false, "Offset from -5000 did not throw, and gave " + std::to_string(offset.value));
    } catch (const std::out_of_range& error) {
        const std::string message = error.what();
        check(message.find("-5000") != std::string::npos,
              "the message for Offset from -5000 does not contain -5000: " + message);
        check(message.find("-1439") != std::string::npos && message.find("1439") != std::string::npos,
              "the message for Offset from -5000 does not contain its limits -1439 and 1439: " + message);
    }
}

void checkCopy() {
    const Month original(7);
    const Month copy(original);
    check(copy.value == 7, "a copy of a Month does not have the value of the original");
    check(original.value == 7, "the original of a copy of a Month has changed");
}

}  // namespace

int main() {
    checkLimits();
    checkNoWrapping();
    checkMessage();
    checkCopy();

    if (failures > 0) {
        std::cout << failures << " rangedint_test check(s) failed\n";
        return 1;
    }
    std::cout << "rangedint_test: all checks passed\n";
    return 0;
}
