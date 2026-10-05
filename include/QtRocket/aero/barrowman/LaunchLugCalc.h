#pragma once

#include "QtRocket/aero/barrowman/TubeCalc.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class LaunchLug;
class Transformation;
class WarningSet;

/// The aerodynamic calculation of a launch lug (OpenRocket's
/// aerodynamics/barrowman/LaunchLugCalc): no non-axial forces and no friction drag (a lug adds
/// too little area to matter), and TubeCalc's pressure drag of an open tube.
///
/// The lug's dimensions are copied at construction (see TubeCalc). Deviation from OpenRocket:
/// the class is final; nothing in OpenRocket extends it.
class LaunchLugCalc final : public TubeCalc
{
public:
    /// A calculation for @p lug, whose dimensions and roughness are copied.
    explicit LaunchLugCalc(const LaunchLug& lug);

    /// Leaves @p forces untouched.
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// 0.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;
};

}  // namespace QtRocket
