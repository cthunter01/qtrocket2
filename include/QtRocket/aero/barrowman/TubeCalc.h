#pragma once

#include "QtRocket/aero/barrowman/RocketComponentCalc.h"

namespace QtRocket
{

class FlightConditions;
class Tube;
class WarningSet;

/// The pressure drag of an open tube in the airflow (OpenRocket's
/// aerodynamics/barrowman/TubeCalc): the abstract base of LaunchLugCalc and TubeFinSetCalc. The
/// air flowing through the tube loses pressure to the friction on its inner wall (the
/// Swamee-Jain friction factor in the Darcy-Weisbach equation, with the Reynolds number of the
/// inner diameter), and the tube's wall has the stagnation and base drag of its frontal area.
///
/// The tube's length, radii and surface roughness are copied at construction, as in Java, so the
/// calculator neither keeps the tube nor sees later changes to it.
///
/// Deviations from OpenRocket:
/// - Java keeps the tube in a field it never reads after the constructor, and its total area in
///   another; neither is kept here.
/// - Java's protected field innerArea is the protected getter getInnerArea().
class TubeCalc : public RocketComponentCalc
{
public:
    /// 0 at a velocity below MathUtil::kEpsilon. Otherwise
    /// (tubeCD * innerArea + 0.7 * (@p stagnationCD + @p baseCD) * frontalArea) / reference area,
    /// with frontalArea the area of the tube's wall and tubeCD the drag coefficient of the
    /// pressure drop through the tube, 2 * deltaP / (rho v^2), where
    /// deltaP = f * length * rho * v^2 / (2 * diameter) and
    /// f = 0.25 / log10(roughness / (3.7 * diameter) + 5.74 / Re^0.9)^2. A tube whose inner area
    /// is not above MathUtil::kEpsilon (a launch lug that stands for a rail guide) has no flow
    /// through it: tubeCD is 0.
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;

protected:
    /// A calculation for @p tube, whose dimensions and roughness are copied (see the class
    /// comment).
    explicit TubeCalc(const Tube& tube);

    /// The cross-section area of the inside of the tube, pi * inner radius^2 (Java: the
    /// protected field innerArea).
    [[nodiscard]] double getInnerArea() const noexcept { return m_innerArea; }

private:
    double m_length;       ///< the tube's length
    double m_diameter;     ///< the inner diameter
    double m_innerArea;    ///< pi * inner radius^2
    double m_frontalArea;  ///< the area of the wall: pi * outer radius^2 less the inner area
    double m_epsilon;      ///< the surface roughness, also used for the inside of the tube
};

}  // namespace QtRocket
