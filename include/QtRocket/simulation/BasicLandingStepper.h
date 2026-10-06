#pragma once

#include "QtRocket/simulation/AbstractEulerStepper.h"

namespace QtRocket
{

class SimulationStatus;

/// The stepper of the descent under recovery devices (OpenRocket's
/// simulation/BasicLandingStepper): an AbstractEulerStepper whose drag is that of the deployed
/// recovery devices.
///
/// Deviation from OpenRocket: the devices are summed in the order they were deployed (Java: the
/// hash order of the set), see MonitorableSet.
class BasicLandingStepper : public AbstractEulerStepper
{
public:
    BasicLandingStepper()                                      = default;
    BasicLandingStepper(const BasicLandingStepper&)            = default;
    BasicLandingStepper(BasicLandingStepper&&)                 = default;
    BasicLandingStepper& operator=(const BasicLandingStepper&) = delete;
    BasicLandingStepper& operator=(BasicLandingStepper&&)      = delete;
    ~BasicLandingStepper() override                            = default;

    /// The sum, over the deployed recovery devices of @p status, of the number of active
    /// instances of the device * its drag coefficient * its area / the reference area of the
    /// configuration. A device that is not among the active instances (its stage has
    /// separated) counts 0 times.
    [[nodiscard]] double computeCD(const SimulationStatus& status) override;
};

}  // namespace QtRocket
