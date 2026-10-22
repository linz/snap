// Standalone unit test for the GGRXF deformation model of a reference frame, in coordsys/crdsys_rfdef_ggrxf.cpp: the
// parsing of the text that follows "DEFORMATION GGRXF" in coordsys.def, which is the name of the GGRXF file and
// optionally the version of the model, and the functions that the resulting ref_deformation calls to calculate,
// describe, copy and compare the model. It reads the files that src/test/ggrxf_files/write_ggrxf_files.py writes, whose
// directory is the first argument of the test. The reading of the files themselves is tested by
// ggrxf_reader_scalar_test and ggrxf_reader_ragged_test, so this test checks what the model does with what the reader
// gives: the units of the offsets, the dates and longitudes of the queries, the sum of the layers, the version of the
// model, and the errors. Failures are reported to stdout and the process exit code is the pass/fail signal (0 = all
// passed), as for the other test-only tools in src/test/.
//
// The values of the layers are those that the two reader tests describe. At (10.5, 20.5), after 2000-01-01, the scalar
// layer of the files gives 83, 83.5 and 84 millimetres. The ragged layer of mixed_layers.nc gives, a number of days
// after 2000-01-01, 10 + 365 * 0.01 / 365.2425 * days millimetres east, 20 north before day 100, and 0 up.

#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_ggrxf.h"
#include "deformation/ggrxf_date.h"
#include "test_support.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/iostring.h"
#include "util/pi.h"

namespace {

using snap_test::check;
using snap_test::checkEqual;

/// The most that an offset in metres may differ from the expected one.
constexpr double kToleranceInMetres = 1e-9;

/// The rate a day of the east values of the ragged layer of mixed_layers.nc, in millimetres.
constexpr double kEastRatePerDay = 365 * 0.01 / 365.2425;

/// The directory of the files that the Python script wrote.
std::string filesDirectory;

/// What parsing the text of a model reported through input_string_def::report_error.
struct ReportedError {
    int count = 0;
    int status = OK;
    std::string message;
};

/// Records an error that parsing the text of a model reports.
int recordError(void* source, const int status, const std::string_view message) {
    auto& reported = *static_cast<ReportedError*>(source);
    ++reported.count;
    reported.status = status;
    reported.message = message;
    return OK;
}

/// \return the ref_deformation that describes the GGRXF model of the text, or nullptr if the text is wrong. The text is
/// what follows "DEFORMATION GGRXF" in coordsys.def: the name of the file, which is found in the directory of the
/// files, and optionally the version of the model.
ref_deformation* parseModelText(const std::string& modelText,
                                ReportedError& reported) {  ///< the error that parsing reports, if there is one
    input_string_def is(modelText);
    is.sourcename = filesDirectory;
    is.source = &reported;
    is.report_error = recordError;
    return rfdef_parse_ggrxf(is);
}

/// \return a reference frame that owns the model, and deletes it. The frame has no ellipsoid and no transformation, as
/// the model reads only the def of the frame.
std::unique_ptr<ref_frame> makeFrame(ref_deformation* const model) {
    const double zero[3] = {0.0, 0.0, 0.0};
    double zeroRates[3] = {0.0, 0.0, 0.0};
    return std::make_unique<ref_frame>("TEST", "Test frame", nullptr, std::nullopt, zero, zero, 0.0, 2000.0, zeroRates,
                                       zeroRates, 0.0, nullptr, model);
}

/// \return a reference frame with the GGRXF model of the text, which has to parse.
std::unique_ptr<ref_frame> frameOfModel(const std::string& modelText) {
    ReportedError reported;
    ref_deformation* const model = parseModelText(modelText, reported);
    check(model != nullptr, "the model text '" + modelText + "' parses: " + reported.message);
    return makeFrame(model);
}

/// \return a new reference frame whose model is a copy of the frame's model, made with the model's copy_data function.
std::unique_ptr<ref_frame> frameOfCopy(const ref_frame& frame) {
    const ref_deformation& model = *frame.def;
    return makeFrame(new ref_deformation(model.type, model.copy_data(model.data), model.delete_data, model.copy_data,
                                         model.identical, model.describe_func, model.calc_denu, model.apply_llh));
}

/// What calc_denu gave: its status, and the offsets, which are initialised to numbers that no calculation gives.
struct Calculation {
    int status = OK;
    std::array<double, 3> denu = {-999.0, -999.0, -999.0};
};

/// \return the epoch, as a decimal year, of the start of the day of the ISO 8601 date, or of a time that is later than
/// that by a number of days, which can be part of a day.
double epochOf(const std::string& date, const double daysAfterStartOfDay = 0.0) {
    return date_as_year(deformation::ggrxfDateToDayNumber(date) + daysAfterStartOfDay);
}

/// Calculates the offsets of the model of the frame, at a point given in degrees.
Calculation calculate(ref_frame& frame, const double latitude, const double longitude, const double epoch) {
    Calculation calculation;
    calculation.status =
        frame.def->calc_denu(&frame, longitude * DTOR, latitude * DTOR, epoch, calculation.denu.data());
    return calculation;
}

/// Checks that the calculation succeeded and gave the expected offsets, which are in metres.
void checkOffsets(const Calculation& calculation, const std::array<double, 3>& expected,
                  const std::string& messageOnError) {
    checkEqual(calculation.status, OK, messageOnError + ": status");
    for (std::size_t axis = 0; axis < expected.size(); ++axis) {
        checkEqual(calculation.denu[axis], expected[axis], kToleranceInMetres,
                   messageOnError + ": offset " + std::to_string(axis));
    }
}

/// \return what the model of the frame writes when it describes itself.
std::string describe(ref_frame& frame) {
    std::string text;
    output_string_def os{&text, [](const std::string_view part, void* sink) {
                             static_cast<std::string*>(sink)->append(part);
                             return OK;
                         }};
    checkEqual(frame.def->describe_func(&frame, &os), OK, "describe returns OK");
    return text;
}

/// \return true if text contains toFind.
bool contains(const std::string& text, const std::string& toFind) {
    return text.find(toFind) != std::string::npos;
}

void check_the_model_text_is_parsed() {
    ReportedError reported;
    const std::unique_ptr<ref_frame> frame = makeFrame(parseModelText("scalar_basic 1.2.3", reported));
    check(frame->def != nullptr, "the text of a file and a version parses");
    checkEqual(frame->def->type, std::string("GGRXF"), "the type of the model");
    checkEqual(reported.count, 0, "text that parses reports no error");
}

void check_model_text_that_is_wrong() {
    ReportedError noName;
    check(parseModelText("", noName) == nullptr, "text with no file name gives no model");
    checkEqual(noName.count, 1, "text with no file name reports one error");
    check(contains(noName.message, "Missing filename for GGRXF deformation"),
          "the message for text with no file name: " + noName.message);

    ReportedError notFound;
    check(parseModelText("absent", notFound) == nullptr, "a file that is not there gives no model");
    checkEqual(notFound.count, 1, "a file that is not there reports one error");
    checkEqual(notFound.status, FILE_OPEN_ERROR, "the status for a file that is not there");
    check(contains(notFound.message, "Cannot open GGRXF deformation file absent"),
          "the message for a file that is not there: " + notFound.message);
}

void check_the_extension_of_the_file_is_optional() {
    const std::unique_ptr<ref_frame> withoutExtension = frameOfModel("scalar_basic");
    const std::unique_ptr<ref_frame> withExtension = frameOfModel("scalar_basic.nc");
    check(withoutExtension->def->identical(withoutExtension->def->data, withExtension->def->data) != 0,
          "scalar_basic and scalar_basic.nc are the same file");
    const std::string path = (std::filesystem::path(filesDirectory) / "scalar_basic.nc").lexically_normal().string();
    check(contains(describe(*withoutExtension), path), "the path of a file that is given with no extension");
}

void check_offsets_are_in_metres() {
    // The layer gives 83, 83.5 and 84 millimetres at the point, from 2000-01-01.
    const std::unique_ptr<ref_frame> frame = frameOfModel("scalar_basic");
    checkOffsets(calculate(*frame, 10.5, 20.5, epochOf("2001-01-01")), {0.083, 0.0835, 0.084},
                 "a point in the grid, after the step");
    checkOffsets(calculate(*frame, 10.5, 20.5, epochOf("1999-12-31")), {0.0, 0.0, 0.0},
                 "a point in the grid, before the step");
}

void check_an_epoch_of_zero_gives_no_offsets() {
    // The file cannot be read, so a calculation that loads it gives INVALID_DATA. An epoch of 0 gives OK, which shows
    // that it returns before the file is loaded.
    const std::unique_ptr<ref_frame> frame = frameOfModel("no_wkt");
    checkOffsets(calculate(*frame, 10.5, 20.5, 0.0), {0.0, 0.0, 0.0}, "an epoch of 0");
}

void check_a_point_outside_every_layer() {
    const std::unique_ptr<ref_frame> frame = frameOfModel("scalar_basic");
    const Calculation calculation = calculate(*frame, 50.0, 50.0, epochOf("2001-01-01"));
    checkEqual(calculation.status, INVALID_DATA, "a point outside the grid: status");
    for (const double offset : calculation.denu) {
        checkEqual(offset, 0.0, "a point outside the grid: the offsets are 0");
    }
}

void check_longitudes_are_taken_modulo_360() {
    const std::unique_ptr<ref_frame> frame = frameOfModel("scalar_basic");
    const double epoch = epochOf("2001-01-01");
    checkOffsets(calculate(*frame, 10.5, 20.5 + 360.0, epoch), {0.083, 0.0835, 0.084}, "a longitude of 360 more");
    checkOffsets(calculate(*frame, 10.5, 20.5 - 360.0, epoch), {0.083, 0.0835, 0.084}, "a longitude of 360 less");
    checkOffsets(calculate(*frame, 10.5, 20.5 + 720.0, epoch), {0.083, 0.0835, 0.084}, "a longitude of 720 more");
}

void check_layers_are_summed() {
    // The scalar layer and the ragged layer of the file both cover the point. On 2000-02-20, 50 days after 2000-01-01,
    // the east values are 83 and 10 + 50 * the rate, the north are 83.5 and 20, and the up are 84 and 0, in
    // millimetres.
    const std::unique_ptr<ref_frame> frame = frameOfModel("mixed_layers");
    checkOffsets(calculate(*frame, 10.5, 20.5, epochOf("2000-02-20")),
                 {(83.0 + 10.0 + kEastRatePerDay * 50.0) * 0.001, (83.5 + 20.0) * 0.001, 84.0 * 0.001},
                 "a point that two layers cover");
}

void check_a_query_part_way_through_a_day() {
    // Noon on 2000-02-20 is 50.5 days after 2000-01-01. The east value of the ragged layer rises by the rate each day,
    // so a query that was cut to the start of its day would give 50 days of it, which is 5 micrometres less. The
    // other values do not change in the day.
    const std::unique_ptr<ref_frame> frame = frameOfModel("mixed_layers");
    checkOffsets(calculate(*frame, 10.5, 20.5, epochOf("2000-02-20", 0.5)),
                 {(83.0 + 10.0 + kEastRatePerDay * 50.5) * 0.001, (83.5 + 20.0) * 0.001, 84.0 * 0.001},
                 "a point that two layers cover, at noon");
}

void check_a_file_with_no_layer_covers_no_point() {
    const std::unique_ptr<ref_frame> frame = frameOfModel("no_layers");
    checkEqual(calculate(*frame, 10.5, 20.5, epochOf("2001-01-01")).status, INVALID_DATA, "a file with no layer");
}

void check_the_version_is_checked() {
    const double epoch = epochOf("2001-01-01");
    const std::unique_ptr<ref_frame> matching = frameOfModel("scalar_basic 1.2.3");
    checkOffsets(calculate(*matching, 10.5, 20.5, epoch), {0.083, 0.0835, 0.084}, "the version of the file");

    const std::unique_ptr<ref_frame> none = frameOfModel("scalar_basic");
    checkOffsets(calculate(*none, 10.5, 20.5, epoch), {0.083, 0.0835, 0.084}, "no version declared");

    // The whole version is kept, so a version that starts with the version of the file does not match, nor does a
    // version that is the start of the version of the file.
    for (const std::string version : {"1.2.3-beta", "1.2"}) {
        const std::unique_ptr<ref_frame> mismatched = frameOfModel("scalar_basic " + version);
        checkEqual(calculate(*mismatched, 10.5, 20.5, epoch).status, INVALID_DATA, "the version " + version);
        const std::string text = describe(*mismatched);
        check(contains(text, "modelVersion (\"1.2.3\")"), "the description for the version " + version + ": " + text);
        check(contains(text, "(\"" + version + "\")"), "the description has the declared version " + version);
        check(!contains(text, "Cannot load from file"), "a version that does not match is not a file that cannot load");
    }

    const std::unique_ptr<ref_frame> noModelVersion = frameOfModel("scalar_no_model_version 1.2.3");
    checkEqual(calculate(*noModelVersion, 10.5, 20.5, epoch).status, INVALID_DATA, "a file with no model version");
    check(contains(describe(*noModelVersion), "modelVersion (none)"),
          "the description for a file with no model version");

    const std::unique_ptr<ref_frame> noVersionEither = frameOfModel("scalar_no_model_version");
    checkOffsets(calculate(*noVersionEither, 10.5, 20.5, epoch), {0.083, 0.0835, 0.084},
                 "no version declared, for a file that has none");
}

void check_a_file_that_cannot_be_read() {
    const std::unique_ptr<ref_frame> frame = frameOfModel("no_wkt");
    const double epoch = epochOf("2001-01-01");
    checkEqual(calculate(*frame, 10.5, 20.5, epoch).status, INVALID_DATA, "a file that cannot be read");
    checkEqual(calculate(*frame, 10.5, 20.5, epoch).status, INVALID_DATA, "the same file, again");
    const std::string text = describe(*frame);
    check(contains(text, "GGRXF deformation model\n"), "the description names the model: " + text);
    check(contains(text, "Cannot load from file ") && contains(text, "no_wkt.nc"),
          "the description for a file that cannot be read: " + text);
}

void check_the_description_of_a_file_that_is_read() {
    const std::unique_ptr<ref_frame> frame = frameOfModel("scalar_basic");
    const std::string text = describe(*frame);
    check(text.rfind("GGRXF deformation model\n   ", 0) == 0, "the description starts with the model: " + text);
    check(contains(text, "scalar_basic.nc\n"), "the description has the path: " + text);
    check(!contains(text, "Cannot load"), "the description of a file that is read: " + text);
}

void check_a_copy_gives_the_same_offsets_and_keeps_the_version() {
    // A copy has the path and the version of the original, but not what the original has read from the file, so
    // calculating with the copy loads the file again.
    const double epoch = epochOf("2001-01-01");
    const std::unique_ptr<ref_frame> original = frameOfModel("scalar_basic 1.2.3");
    checkOffsets(calculate(*original, 10.5, 20.5, epoch), {0.083, 0.0835, 0.084}, "the original");

    const std::unique_ptr<ref_frame> copy = frameOfCopy(*original);
    check(original->def->identical(original->def->data, copy->def->data) != 0, "a copy is the same file");
    checkOffsets(calculate(*copy, 10.5, 20.5, epoch), {0.083, 0.0835, 0.084}, "the copy, at a point in the grid");
    checkEqual(calculate(*copy, 50.0, 50.0, epoch).status, INVALID_DATA, "the copy, at a point outside the grid");

    // The version of the copy is the version of the original, so the copy rejects a file that does not have it.
    const std::unique_ptr<ref_frame> mismatched = frameOfModel("scalar_basic 9");
    const std::unique_ptr<ref_frame> mismatchedCopy = frameOfCopy(*mismatched);
    checkEqual(calculate(*mismatchedCopy, 10.5, 20.5, epoch).status, INVALID_DATA, "a copy keeps the version");
}

void check_models_of_different_files_are_not_identical() {
    const std::unique_ptr<ref_frame> first = frameOfModel("scalar_basic");
    const std::unique_ptr<ref_frame> second = frameOfModel("scalar_no_model_version");
    const std::unique_ptr<ref_frame> sameFileAsFirst = frameOfModel("scalar_basic 1.2.3");
    check(first->def->identical(first->def->data, second->def->data) == 0, "different files are not identical");
    check(first->def->identical(first->def->data, sameFileAsFirst->def->data) != 0,
          "the same file is identical, whatever the version that is declared");
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: crdsys_rfdef_ggrxf_test <directory of the GGRXF files>\n";
        return 2;
    }
    filesDirectory = argv[1];

    try {
        check_the_model_text_is_parsed();
        check_model_text_that_is_wrong();
        check_the_extension_of_the_file_is_optional();
        check_offsets_are_in_metres();
        check_an_epoch_of_zero_gives_no_offsets();
        check_a_point_outside_every_layer();
        check_longitudes_are_taken_modulo_360();
        check_layers_are_summed();
        check_a_query_part_way_through_a_day();
        check_a_file_with_no_layer_covers_no_point();
        check_the_version_is_checked();
        check_a_file_that_cannot_be_read();
        check_the_description_of_a_file_that_is_read();
        check_a_copy_gives_the_same_offsets_and_keeps_the_version();
        check_models_of_different_files_are_not_identical();
    } catch (const std::exception& error) {
        check(false, std::string("unexpected exception: ") + error.what());
    }

    return snap_test::finish("crdsys_rfdef_ggrxf_test");
}
