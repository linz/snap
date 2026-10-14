#pragma once

#include <array>
#include <optional>

namespace deformation {

// One GGRXF-style layer: one summed, independent contribution to total
// deformation (one GGRXF netCDF group) - see each concrete subclass
// (ScalarLayer, RaggedLayer) for how it computes its own contribution.
// Every layer's own contribution is computed independently and simply
// added to every other layer's (GGXF's own "every layer's own
// contribution... summed independently") - the not-yet-built top-level
// container above this abstraction sums whichever mix of layers a file
// has, without caring which kind each one is.
class Layer {
 public:
    virtual ~Layer() = default;

    // Evaluates this layer's own contribution at (c0, c1, queryDateDays) -
    // only the first axisCount() entries of the result are meaningful.
    // Returns std::nullopt if this layer contributes nothing at this point
    // (GGRXF spec: "this layer contributes exactly 0 at this point") - a
    // caller summing multiple layers treats a std::nullopt the same as an
    // all-zero contribution.
    [[nodiscard]] virtual std::optional<std::array<double, 3>> evaluate(
        double c0,  // coordinate on this layer's first axis (same units/order as every root grid's own affineCoeffs c0)
        double c1,  // coordinate on this layer's second axis
        double queryDateDays  // query date, as a day count in the same epoch as every TimeFunction::Params field
    ) const = 0;
};

}  // namespace deformation
