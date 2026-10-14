#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "layer.h"

namespace deformation {

// The result of parsing one GGRXF file - its query-ready layers, plus its
// own root-level modelVersion attribute (absent if the file doesn't carry
// one - GGRXF's container/root-level attributes are otherwise unparsed, see
// parseGgrxfFile's own comment).
struct GgrxfFile {
    std::vector<std::unique_ptr<Layer>> layers;
    std::optional<std::string> modelVersion;
};

// Parses a GGRXF netCDF-4 file into its query-ready Layer objects -
// ScalarLayer for scalar_with_time_function groups, RaggedLayer for
// ragged_segments groups.
//
// Scoped to exactly what a real producer's own files carry and what a real
// reference reader actually reads back, confirmed directly against both
// rather than the full GGRXF spec: no container/root-level parsing at all
// beyond modelVersion and interpolationCrsWkt (Conventions/content/root
// parameters.* are never read, and interpolationCrsWkt itself is only ever
// read for its AXIS[...] clause order, see wktDeclaresLatitudeFirst() in
// ggrxf_reader.cpp - no other part of its CRS text is parsed);
// interpolationMethod must be "bilinear" (throws otherwise); only
// linear/step/ramp time functions and only linear (function_type code 0)
// ragged segments are supported, matching TimeFunction's/RaggedGridData's
// own scope; has_end_day is read and passed through to ScalarGridData's/
// RaggedGridData's own has_end_day handling (which throws if it's ever
// true - not implemented, and no real file sets it, see ScalarGridData's
// own has_end_day comment).
//
// Every returned Layer's own evaluate(c0, c1, ...) expects (c0, c1) in
// (latitude, longitude) order, regardless of which axis order this file's
// own interpolationCrsWkt actually declares - every grid's own affineCoeffs
// is canonicalized to that order at parse time (throwing if
// interpolationCrsWkt doesn't clearly declare both axes), so a caller never
// needs to know or check a particular file's own declared order itself.
GgrxfFile parseGgrxfFile(const std::string& path);

}  // namespace deformation
