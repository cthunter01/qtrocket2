#pragma once

#include <array>

#include "QtRocket/simulation/AbstractEulerStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The stepper of a tumbling rocket or stage (OpenRocket's simulation/BasicTumbleStepper): an
/// AbstractEulerStepper whose drag coefficient comes from the projected areas of the fins and
/// of the body, with the constants of Sampo Niskanen's experiments as documented in
/// OpenRocket's technical documentation (techdoc.pdf).
///
/// Deviation from OpenRocket: the active instances are summed in tree order (Java: the hash
/// order of the instance map), see InstanceMap.
class BasicTumbleStepper : public AbstractEulerStepper
{
public:
    /// The drag coefficient of the fins of a tumbling rocket (cDFin).
    static constexpr double kCdFin = 1.42;

    /// The drag coefficient of the body of a tumbling rocket (cDBt).
    static constexpr double kCdBt = 0.56;

    /// The fin efficiency by the number of fins of a set (finEff): the entry 0 is arbitrary and
    /// offsets the indices, so that the entry 1 is the coefficient of one fin in the table of
    /// the technical documentation. A set with more fins than the table has uses its last
    /// entry.
    static constexpr std::array<double, 8> kFinEfficiency{0.0,  0.5,  1.0,  1.41,
                                                          1.81, 1.73, 1.90, 1.85};

    BasicTumbleStepper()                                     = default;
    BasicTumbleStepper(const BasicTumbleStepper&)            = default;
    BasicTumbleStepper(BasicTumbleStepper&&)                 = default;
    BasicTumbleStepper& operator=(const BasicTumbleStepper&) = delete;
    BasicTumbleStepper& operator=(BasicTumbleStepper&&)      = delete;
    ~BasicTumbleStepper() override                           = default;

    /// (kCdFin * aFins + kCdBt * aBt) / the reference area of the configuration, summed over
    /// every active instance of every aerodynamic component of @p status's configuration: a
    /// fin set adds its planform area * the fin efficiency of its fin count / its fin count to
    /// aFins, and a symmetric component (nose cone, body tube, transition) its planform area
    /// to aBt.
    [[nodiscard]] double computeCD(const SimulationStatus& status) override;
};

}  // namespace QtRocket
