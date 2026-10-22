// Standalone unit test for util/textformat.hpp.
//
// Where printf has an equivalent format, the expected strings are what it gives.

#include <string>

#include "test_support.h"
#include "util/textformat.hpp"

namespace {

using snap_test::checkEqual;

void check_format_fixed() {
    checkEqual(format_fixed(3.14159, 2), "3.14", "fixed: decimal places");
    checkEqual(format_fixed(2.6, 0), "3", "fixed: no decimal places rounds and has no point");
    checkEqual(format_fixed(123.456, 0), "123", "fixed: no decimal places, rounding down");
    checkEqual(format_fixed(0.0, 2), "0.00", "fixed: zero");
    checkEqual(format_fixed(-45.5126, 3), "-45.513", "fixed: negative");
    checkEqual(format_fixed(1e-7, 3), "0.000", "fixed: smaller than the last digit");
    checkEqual(format_fixed(1234.5678, 1, 12), "      1234.6", "fixed: padded with spaces to a width");
    checkEqual(format_fixed(123456.789, 1, 3), "123456.8", "fixed: wider than the width");
    checkEqual(format_fixed(7.5, 2, 8, '0'), "00007.50", "fixed: zero fill");
    checkEqual(format_fixed(-2.5, 1, 6, '0'), "-002.5", "fixed: zero fill goes after the sign");
    checkEqual(format_fixed(5.0, 0, 2, '0'), "05", "fixed: zero fill of a whole number");
    checkEqual(format_fixed(-2.5, 1, 6), "  -2.5", "fixed: space fill goes before the sign");
    checkEqual(format_fixed(-2.5, 1, 6, '*'), "-**2.5", "fixed: any other fill goes after the sign");
    checkEqual(format_fixed(2.5, 1, 6, '*'), "***2.5", "fixed: any other fill, positive");
}

void check_format_scientific() {
    checkEqual(format_scientific(12345.6789, 8), "1.23456789e+04", "scientific: eight decimal places");
    checkEqual(format_scientific(0.00012, 2), "1.20e-04", "scientific: negative exponent");
    checkEqual(format_scientific(-5.0, 1), "-5.0e+00", "scientific: negative value");
    checkEqual(format_scientific(0.0, 2), "0.00e+00", "scientific: zero");
}

void check_padding() {
    checkEqual(pad_right("To", 6), "To    ", "pad_right: shorter than the width");
    checkEqual(pad_right("Length", 6), "Length", "pad_right: exactly the width");
    checkEqual(pad_right("Observations", 6), "Observations", "pad_right: wider than the width is not truncated");
    checkEqual(pad_right("", 3), "   ", "pad_right: empty text");
    checkEqual(pad_left("To", 6), "    To", "pad_left: shorter than the width");
    checkEqual(pad_left("Length", 6), "Length", "pad_left: exactly the width");
    checkEqual(pad_left("Observations", 6), "Observations", "pad_left: wider than the width is not truncated");
    checkEqual(pad_left("", 3), "   ", "pad_left: empty text");
}

}  // namespace

int main() {
    check_format_fixed();
    check_format_scientific();
    check_padding();

    return snap_test::finish("textformat_test");
}
