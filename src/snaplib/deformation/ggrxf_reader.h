#pragma once

#include <memory>
#include <string>
#include <vector>

#include "layer.h"

namespace deformation {

// Parses a GGRXF netCDF-4 file into its query-ready Layer objects -
// ScalarLayer for scalar_with_time_function groups, RaggedLayer for
// ragged_segments groups.
//
// Scoped to exactly what a real producer's own files carry and what a real
// reference reader actually reads back, confirmed directly against both
// rather than the full GGRXF spec: no container/root-level parsing at all
// (Conventions/content/CRS WKT/root parameters.* are never read);
// interpolationMethod must be "bilinear" (throws otherwise); only linear/
// step/ramp time functions and only linear (function_type code 0) ragged
// segments are supported, matching TimeFunction's/RaggedGridData's own
// scope; has_end_day is read and passed through to ScalarGridData's/
// RaggedGridData's own has_end_day handling (which throws if it's ever
// true - not implemented, and no real file sets it, see ScalarGridData's
// own has_end_day comment).
//
// Every returned Layer's own evaluate(c0, c1, ...) expects (c0, c1) in
// whichever axis order this file's own affineCoeffs use - assumed to be
// (latitude, longitude), matching this project's own fixed interpolation
// CRS, rather than parsed from interpolationCrsWkt generically.
std::vector<std::unique_ptr<Layer>> parseGgrxfFile( const std::string &path );

} // namespace deformation
