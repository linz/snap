#include "snapconfig.h"

#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include <utility>
#include "coordsys/coordsys.h"
#include "coordsys/crdsys_rfdef_ggrxf.h"
#include "deformation/ggrxf_reader.h"
#include "deformation/layer.h"
#include "util/dateutil.h"
#include "util/errdef.h"
#include "util/fileutil.h"
#include "util/pi.h"
#include "iostring.h"

// Matches crdsys_rfdef_linzdef.cpp's own VERSIONLEN - both files parse the
// same trailing version token syntax on a DEFORMATION line.
#define VERSIONLEN 8

namespace {

// GGRXF displacement values are always stored in millimetres
// (GGRXF_DEFORMATION_FORMAT.md: "unitName = millimetre and unitSiRatio =
// 0.001... this format's own existing millimetre-scale storage"), but
// calc_denu's own denu[] output is added directly to a geodetic position
// in metres (rf_apply_enu_deformation_to_llh applies it with no further
// scaling) - so every summed layer contribution needs this conversion
// applied once, here, not inside the deformation:: module itself, which
// stays agnostic to any particular caller's own unit convention.
constexpr double kMillimetresToMetres = 0.001;

// One GGRXF deformation model, as stored in a ref_deformation's own opaque
// data pointer - allocated/freed with new/delete (not check_malloc/
// check_free, unlike this file's sibling rfdef parsers) since it owns real
// C++ containers, not a plain-old-data struct.
struct GgrxfModel {
    std::string path;
    std::string version;                // empty: no version declared in coordsys.def
    std::optional<bool> loadSucceeded;  // nullopt: not yet attempted
    std::string loadError;              // set alongside loadSucceeded==false, for rf_ggrxf_describe
    std::vector<std::unique_ptr<deformation::Layer>> layers;
};

// Casts a ref_deformation's own opaque data pointer back to GgrxfModel -
// the inverse of the new GgrxfModel this file always stores there
// (rfdef_parse_ggrxf()/rf_ggrxf_copy()).
GgrxfModel* asGgrxfModel(void* data) {
    return static_cast<GgrxfModel*>(data);
}

// Parses model's own file on first use, caching the outcome - same
// lazy-load-once contract as this file's sibling parsers' own load
// functions (e.g. rf_linzdef_load), so a failed load isn't retried on
// every calc_denu call. Only catches the exception types the deformation::
// module actually documents throwing for a malformed/unsupported file
// (std::logic_error, which std::invalid_argument derives from; and
// std::runtime_error, which NetcdfError derives from) - deliberately not a
// bare std::exception, so an unexpected failure like std::bad_alloc
// propagates and fails loudly instead of being silently reported as
// "invalid data".
int loadGgrxfModel(GgrxfModel& model) {
    if (model.loadSucceeded) {
        return *model.loadSucceeded ? OK : INVALID_DATA;
    }
    try {
        deformation::GgrxfFile file = deformation::parseGgrxfFile(model.path);

        // An empty model.version means no version was declared in
        // coordsys.def, so there's nothing to check against - matches
        // rfdef_parse_linzdef's own optional-version semantics. A missing
        // modelVersion attribute compares unequal to any declared version
        // (std::optional's own != against a bare value), so it's treated
        // the same as a real mismatch, not as "nothing to check".
        if (!model.version.empty() && file.modelVersion != model.version) {
            model.loadError = "GGRXF file's modelVersion (" +
                              (file.modelVersion ? "\"" + *file.modelVersion + "\"" : std::string("none")) +
                              ") does not match the version declared in coordsys.def (\"" + model.version + "\")";
            model.loadSucceeded = false;
            return INVALID_DATA;
        }

        model.layers = std::move(file.layers);
        model.loadSucceeded = true;
        return OK;
    } catch (const std::logic_error&) {
        model.loadSucceeded = false;
        return INVALID_DATA;
    } catch (const std::runtime_error&) {
        model.loadSucceeded = false;
        return INVALID_DATA;
    }
}

// Frees a GgrxfModel allocated by rfdef_parse_ggrxf()/rf_ggrxf_copy() -
// this ref_deformation's own delete_func.
void rf_ggrxf_delete(void* data) {
    delete asGgrxfModel(data);
}

// A fresh, not-yet-loaded copy - same lazy-load contract as this file's
// sibling parsers' own copy functions, discarding any already-loaded state
// rather than sharing it (each ref_deformation instance owns its own copy,
// matching copy_ref_deformation's own by-value semantics elsewhere).
void* rf_ggrxf_copy(void* data) {
    auto copy = std::make_unique<GgrxfModel>();
    copy->path = asGgrxfModel(data)->path;
    copy->version = asGgrxfModel(data)->version;
    return copy.release();
}

// True if two GgrxfModels reference the same underlying file - same
// identity contract as this file's sibling parsers' own identical
// functions (e.g. rf_linzdef_identical), comparing only the file path, not
// any loaded state.
int rf_ggrxf_identical(void* data1, void* data2) {
    return asGgrxfModel(data1)->path == asGgrxfModel(data2)->path ? 1 : 0;
}

// Describes this deformation model for output - the GGRXF sibling to
// rf_linzdef_describe. Deliberately minimal: unlike LINZDEF's own compiled
// binary, GGRXF has no single per-file title/version to extract, only a
// per-layer description - so this reports only the file path, whether it
// loaded successfully, and (on failure) the specific reason if one was
// recorded, e.g. a modelVersion mismatch (see loadGgrxfModel()).
int rf_ggrxf_describe(ref_frame* rf, output_string_def* os) {
    ref_deformation* def = rf->def;
    GgrxfModel* model = asGgrxfModel(def->data);
    if (!model) {
        return OK;
    }

    write_output_string(os, "GGRXF deformation model\n");
    if (loadGgrxfModel(*model) != OK) {
        if (!model->loadError.empty()) {
            write_output_string(os, "   ");
            write_output_string(os, model->loadError.c_str());
            write_output_string(os, "\n");
        } else {
            write_output_string(os, "   Cannot load from file ");
            write_output_string(os, model->path.c_str());
            write_output_string(os, "\n");
        }
    } else {
        write_output_string(os, "   ");
        write_output_string(os, model->path.c_str());
        write_output_string(os, "\n");
    }
    return OK;
}

// Called for each observation to determine the east, north, and vertical
// offset the model predicts for a specific time - the GGRXF sibling to
// rf_linzdef_calc.
int rf_ggrxf_calc(ref_frame* rf, double lon, double lat, double epoch, double denu[3]) {
    denu[0] = denu[1] = denu[2] = 0.0;
    if (epoch == 0) {
        return OK;
    }

    ref_deformation* def = rf->def;
    GgrxfModel* model = asGgrxfModel(def->data);
    const int sts = loadGgrxfModel(*model);
    if (sts != OK) {
        return sts;
    }

    // epoch is a decimal year here (confirmed at its real call sites, e.g.
    // csdeform.cpp's own "year = date_as_year(date)"), not a day count -
    // year_as_snapdate is the exact inverse of date_as_year, giving a day
    // count in the same epoch ggrxfDateToDayNumber's own ISO-8601 parsing
    // already uses.
    const double queryDateDays = year_as_snapdate(epoch);
    const double latDeg = lat * RTOD;

    // GGRXF_DEFORMATION_FORMAT.md documents this project's own producer as
    // always storing longitude in [0, 360) - the nationwide file's own
    // coarse grid spans the antimeridian to cover the Chatham Islands
    // alongside the mainland in one grid, and every other, non-crossing
    // grid's own longitude is already non-negative regardless (all real NZ
    // territory bar the Chathams lies east of Greenwich, under 180), so
    // rebasing into that same [0, 360) convention here is a no-op for every
    // grid except where SNAP's own signed convention would otherwise place
    // a Chatham Islands query on the wrong side of every grid's own extent.
    //
    // lon itself is not guaranteed to already be within (-180, 180] -
    // stations read from a decimal-degrees or DMS station file carry
    // whatever raw value the file declared, with no range check applied
    // anywhere before this point (confirmed directly against networkr.cpp's
    // own df_read_double()/df_read_dmsangle() station readers) - so this
    // rebases via a real modulo, not a single one-directional shift that
    // would only be exhaustive over an already-bounded input.
    const double lonDegMod360 = std::fmod(lon * RTOD, 360.0);
    const double lonDeg = lonDegMod360 < 0.0 ? lonDegMod360 + 360.0 : lonDegMod360;

    try {
        double totalE = 0.0;
        double totalN = 0.0;
        double totalU = 0.0;
        bool anyLayerMatched = false;
        for (const auto& layer : model->layers) {
            const auto contribution = layer->evaluate(latDeg, lonDeg, queryDateDays);
            if (!contribution) {
                continue;
            }
            anyLayerMatched = true;
            totalE += (*contribution)[0];
            totalN += (*contribution)[1];
            totalU += (*contribution)[2];
        }
        if (!anyLayerMatched) {
            throw std::runtime_error("GGRXF model " + model->path +
                                     " has no layer covering lat=" + std::to_string(latDeg) +
                                     " lon=" + std::to_string(lonDeg) + " date=" + std::to_string(queryDateDays));
        }
        denu[0] = totalE * kMillimetresToMetres;
        denu[1] = totalN * kMillimetresToMetres;
        denu[2] = totalU * kMillimetresToMetres;
    } catch (const std::logic_error&) {
        return INVALID_DATA;
    } catch (const std::runtime_error&) {
        return INVALID_DATA;
    }
    return OK;
}

}  // namespace

// Parses a "DEFORMATION GGRXF <filename> [version]" config clause - the
// GGRXF sibling to rfdef_parse_linzdef. Only records the file path and the
// optional declared version here; the file itself is opened lazily on
// first real calc_denu()/describe_func() call (see loadGgrxfModel()).
ref_deformation* rfdef_parse_ggrxf(input_string_def& is) {
    std::string filename;
    const int sts = next_string_field(is.scanner, filename, MAX_FILENAME_LEN);
    if (sts != OK) {
        report_string_error(is, sts, "Missing filename for GGRXF deformation");
        return nullptr;
    }

    const std::optional<std::string> ggrxfFile = find_relative_file(is.sourcename, filename, ".nc");
    if (!ggrxfFile) {
        report_string_error(is, FILE_OPEN_ERROR, "Cannot open GGRXF deformation file " + filename);
        return nullptr;
    }

    // The trailing version token is optional, the same as
    // rfdef_parse_linzdef's own version argument - absence means "don't
    // check the file's own modelVersion" (see loadGgrxfModel()).
    std::string version;
    if (next_string_field(is.scanner, version, VERSIONLEN) != OK) {
        version.clear();
    }

    auto model = std::make_unique<GgrxfModel>();
    model->path = *ggrxfFile;
    model->version = version;
    return new ref_deformation("GGRXF", model.release(), rf_ggrxf_delete, rf_ggrxf_copy, rf_ggrxf_identical,
                               rf_ggrxf_describe, rf_ggrxf_calc, nullptr);
}
