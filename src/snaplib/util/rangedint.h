#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

/// An integer that is always from Minimum to Maximum, whatever it is made from, so that a value of it needs no
/// checking where it is used. Its limits are part of its type, so a month and an hour are different types, and one
/// can not be passed for the other.
///
/// There is no default value, because a range that does not include 0 has no obvious one. It has no comparison
/// operators; compare value. It can not be assigned to, only made and copied.
template <typename Value, Value Minimum, Value Maximum>
class RangedInt {
    static_assert(std::is_integral_v<Value>, "RangedInt holds an integer");
    static_assert(Minimum <= Maximum, "the minimum of a RangedInt can not be more than its maximum");

 public:
    /// The lowest value that it can have.
    static constexpr Value kMinimum = Minimum;

    /// The highest value that it can have.
    static constexpr Value kMaximum = Maximum;

    /// \throws std::out_of_range if number is not from kMinimum to kMaximum.
    explicit RangedInt(const long long number) : value(_checked(number)) {}

    /// Makes one without throwing, for a number that is not known to be in range.
    /// \return the number, or nothing if it is not from kMinimum to kMaximum.
    [[nodiscard]] static std::optional<RangedInt> from(const long long number) {
        if (!_isInRange(number)) {
            return std::nullopt;
        }
        return RangedInt(static_cast<Value>(number), _Unchecked());
    }

    const Value value;  ///< the number

 private:
    /// Marks the construction of a number that has already been checked.
    struct _Unchecked {};

    RangedInt(const Value number, _Unchecked) : value(number) {}

    static constexpr bool _isInRange(const long long number) {
        return number >= static_cast<long long>(Minimum) && number <= static_cast<long long>(Maximum);
    }

    static Value _checked(const long long number) {
        if (!_isInRange(number)) {
            throw std::out_of_range(std::to_string(number) + " is not from " + std::to_string(Minimum) + " to " +
                                    std::to_string(Maximum));
        }
        return static_cast<Value>(number);
    }
};
