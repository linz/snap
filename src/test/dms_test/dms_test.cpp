// Standalone unit test for util/dms.h.
//
// The expected strings were first confirmed against the C implementation
// (create_dms_format/dms_string) that DmsFormat replaced.

#include <cmath>
#include <optional>
#include <string>

#include "test_support.h"
#include "util/dms.h"

namespace {

using snap_test::checkEqual;

// Writes the angle with dms_string, and checks that DmsFormat::format gives the same.
std::string write(const DmsFormat& format, const double angle) {
    const std::string result = dms_string(angle, format);
    checkEqual(format.format(angle), result, "DmsFormat::format matches dms_string");
    return result;
}

void check_degrees_minutes_seconds() {
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS), 45.5125), " 45 30 45.00", "dms: default separators");
    checkEqual(write(DmsFormat(3, 0, DMSF_FMT_DMS), 45.5125), " 45 30 45", "dms: no decimal places");
    checkEqual(write(DmsFormat(3, 1, DMSF_FMT_DMS, "d", "'", "\""), 45.5125), " 45d30'45.0\"", "dms: separators");
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS, "\xC2\xB0", "'", "\""), 45.5125),
               " 45\xC2\xB0"
               "30'45.00\"",
               "dms: multi-byte degree separator, as in snaplist's config");
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS, "\xC2\xB0", "'", ""), 45.5125),
               " 45\xC2\xB0"
               "30'45.00",
               "dms: empty text after the seconds, as when snaplist's config omits it");
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS), 0.0), "  0 00 00.00", "dms: zero");
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS), 45.99999999), " 46 00 00.00",
               "dms: rounds up through the minutes and seconds");
    checkEqual(write(DmsFormat(7, 2, DMSF_FMT_DMS), 45.5125), "  45 30 45.00", "dms: degree digits above 4 become 4");
    checkEqual(write(DmsFormat(0, 2, DMSF_FMT_DMS), 45.5125), "45 30 45.00", "dms: no degree digits is no padding");
    checkEqual(write(DmsFormat(3, -2, DMSF_FMT_DMS), 45.5125), " 45 30 45", "dms: negative decimal places become 0");
    checkEqual(write(DmsFormat(3, 20, DMSF_FMT_DMS), 45.5125), " 45 30 45.000000",
               "dms: decimal places above 6 become 6");
}

void check_default_and_empty_text() {
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS, "", std::nullopt), 45.5125), " 4530 45.00",
               "dms: empty text after degrees replaces the default blank");
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS, std::nullopt, ""), 45.5125), " 45 3045.00",
               "dms: empty text after minutes replaces the default blank");
    checkEqual(write(DmsFormat(3, 3, DMSF_FMT_DM), 45.5125), " 45 30.750",
               "dm: the default text after the minutes is empty");
    checkEqual(write(DmsFormat(3, 3, DMSF_FMT_DM, std::nullopt, "'"), 45.5125), " 45 30.750'",
               "dm: text after the minutes");
    checkEqual(write(DmsFormat(3, 3, DMSF_FMT_DM, "d"), 45.5125), " 45d30.750", "dm: text after the degrees");
}

void check_degrees_only() {
    checkEqual(write(DmsFormat(3, 4, DMSF_FMT_DEG), 45.5125), "45.5125", "deg: plain");
    checkEqual(write(DmsFormat(3, 20, DMSF_FMT_DEG), 45.5125), "45.5125000000",
               "deg: decimal places above 10 become 10");
    checkEqual(write(DmsFormat(3, -2, DMSF_FMT_DEG), 45.5125), "46", "deg: negative decimal places become 0");
    const DmsFormat hemisphere(0, 4, DMSF_FMT_DEG, std::nullopt, std::nullopt, std::nullopt, "N", "S");
    checkEqual(write(hemisphere, 45.5125), "45.5125N", "deg: positive hemisphere follows");
    checkEqual(write(hemisphere, -45.5125), "45.5125S", "deg: negative hemisphere follows");
    const DmsFormat prefixed(0, 4, DMSF_FMT_DEG | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt, "N",
                             "S");
    checkEqual(write(prefixed, -45.5125), "S45.5125", "deg: hemisphere prefixes");
    const DmsFormat leadingMinus(0, 4, DMSF_FMT_DEG | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt,
                                 std::nullopt, "-");
    checkEqual(write(leadingMinus, -45.5125), "-45.5125", "deg: leading minus");
    checkEqual(write(leadingMinus, 45.5125), "45.5125", "deg: leading minus, positive angle");
}

void check_signs() {
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS), -45.5125), " 45 30 45.00",
               "sign: negative angle with no sign text loses its sign");
    const DmsFormat hemisphere(3, 2, DMSF_FMT_DMS, std::nullopt, std::nullopt, std::nullopt, "N", "S");
    checkEqual(write(hemisphere, 45.5125), " 45 30 45.00N", "sign: positive hemisphere follows");
    checkEqual(write(hemisphere, -45.5125), " 45 30 45.00S", "sign: negative hemisphere follows");
    const DmsFormat blankHemisphere(3, 2, DMSF_FMT_DMS, std::nullopt, std::nullopt, std::nullopt, " N", " S");
    checkEqual(write(blankHemisphere, -45.5125), " 45 30 45.00 S", "sign: hemisphere with a leading blank");
    const DmsFormat prefixed(3, 2, DMSF_FMT_DMS | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt, "N",
                             "S");
    checkEqual(write(prefixed, -45.5125), "S 45 30 45.00", "sign: hemisphere prefixes");
    const DmsFormat plusMinus(3, 2, DMSF_FMT_DMS | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt, "+",
                              "-");
    checkEqual(write(plusMinus, 45.5125), "+ 45 30 45.00", "sign: plus prefixes");
    checkEqual(write(plusMinus, -45.5125), "- 45 30 45.00", "sign: minus prefixes");
    const DmsFormat trailingMinus(3, 2, DMSF_FMT_DMS, std::nullopt, std::nullopt, std::nullopt, std::nullopt, "-");
    checkEqual(write(trailingMinus, -45.5125), " 45 30 45.00-", "sign: minus follows without the prefix flag");
    const DmsFormat leadingMinus(3, 2, DMSF_FMT_DMS | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt,
                                 std::nullopt, "-");
    checkEqual(write(leadingMinus, -45.5125), "-45 30 45.00", "sign: leading minus sits against the degrees");
    checkEqual(write(leadingMinus, 45.5125), " 45 30 45.00", "sign: leading minus, positive angle");
    const DmsFormat wideLeadingMinus(4, 2, DMSF_FMT_DMS | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt,
                                     std::nullopt, "-");
    checkEqual(write(wideLeadingMinus, -45.5125), " -45 30 45.00",
               "sign: leading minus counts towards the degree width");
    const DmsFormat prefixedMinutes(3, 3, DMSF_FMT_DM | DMSF_FMT_PREFIX_HEM, std::nullopt, std::nullopt, std::nullopt,
                                    "N", "S");
    checkEqual(write(prefixedMinutes, -45.5125), "S 45 30.750", "sign: dm hemisphere prefixes");
}

void check_radians() {
    checkEqual(write(DmsFormat(3, 2, DMSF_FMT_DMS | DMSF_FMT_INPUT_RADIANS), std::acos(-1.0) / 4.0), " 45 00 00.00",
               "radians: pi/4");
}

void check_format_reuse() {
    const DmsFormat format(3, 2, DMSF_FMT_DMS, std::nullopt, std::nullopt, std::nullopt, "N", "S");
    checkEqual(write(format, 45.5125), " 45 30 45.00N", "reuse: first angle");
    checkEqual(write(format, -10.25), " 10 15 00.00S", "reuse: second angle");
    checkEqual(write(format, 45.5125), " 45 30 45.00N", "reuse: first angle again");
}

}  // namespace

int main() {
    check_degrees_minutes_seconds();
    check_default_and_empty_text();
    check_degrees_only();
    check_signs();
    check_radians();
    check_format_reuse();

    return snap_test::finish("dms_test");
}
