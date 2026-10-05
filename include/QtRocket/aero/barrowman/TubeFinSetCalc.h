#pragma once

#include <numbers>

#include "QtRocket/aero/barrowman/TubeCalc.h"
#include "QtRocket/logging/WarningSet.h"

namespace QtRocket
{

class AerodynamicForces;
class FlightConditions;
class Transformation;
class TubeFinSet;

/// The aerodynamic calculation of one tube of a tube fin set (OpenRocket's
/// aerodynamics/barrowman/TubeFinSetCalc, "preliminary computation of tube fin aerodynamics").
/// The Barrowman calculators iterate over the tubes (the instances of the tube fin set), so
/// everything here is the geometry of a single tube; the interference between the tubes is not
/// considered.
///
/// - Non-axial forces: CNa is Ribner's ring airfoil value ("The ring airfoil in nonaxial flow",
///   Journal of the Aeronautical Sciences 14(9), 1947, equation 5),
///   2 (AR' / (1 + AR')) pi^2 inner radius * chord / reference area with AR' = 2 AR / pi and the
///   aspect ratio AR = 2 inner radius / chord; CN is CNa times the angle of attack, held at the
///   stall angle of 20 degrees. The CP is along the chord at the fraction of
///   RocketComponentCalc's CP helpers: the quarter chord up to Mach 0.5, supersonicCPPos(AR beta)
///   from Mach 2, transonicCPPos() between. The roll forcing is 0 (a tube's cant angle is
///   always 0) and the roll damping follows the roll rate. No side force and no yaw moment.
/// - Friction drag: the outer surface of the tube, less the part of it and of the body that the
///   contact between the two masks (the inner surface counts in the pressure drag instead).
/// - Pressure drag: TubeCalc's, plus the stagnation and base drag of the interstice between the
///   tube and the body.
///
/// Every call of calculateNonaxialForces() and calculatePressureCD() adds the warnings about the
/// geometry, each with the tube fin set as its source: Warning::kTubeIsolated for a single tube,
/// else Warning::kTubeSeparation when the tubes are more than MathUtil::kEpsilon apart, else
/// Warning::kTubeOverlap when they overlap by more than that.
///
/// Everything is copied from the tube fin set at construction, as in Java, so the calculator
/// neither keeps the tube fin set nor sees later changes to it.
///
/// Deviations from OpenRocket:
/// - The class is final (nothing in OpenRocket extends it), and Java's protected field
///   geometryWarnings is read through getGeometryWarnings().
/// - Java keeps the tube fin set, the inner radius, the tube count and the base rotation in
///   fields it never reads after the constructor; they are not kept here. Its cant angle field,
///   always 0, is the constant kCantAngle.
/// - The source of a geometry warning is the tube fin set's id and its name at construction
///   (see MessageSource).
class TubeFinSetCalc final : public TubeCalc
{
public:
    /// A calculation for one tube of @p tubes, whose geometry is copied (see the class comment).
    explicit TubeFinSetCalc(const TubeFinSet& tubes);

    /// The forces of one tube into @p forces (see the class comment); every coefficient is 0,
    /// with the CP at the origin, for a tube of outer radius below 1 mm. Adds the geometry
    /// warnings to @p warnings.
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// @p componentCf times the wetted area of one tube over the reference area.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;

    /// TubeCalc's pressure drag plus (@p stagnationCD + @p baseCD) times the interstice area
    /// over the reference area. Adds the geometry warnings to @p warnings.
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;

    /// The warnings about the geometry, found at construction (Java: the protected field
    /// geometryWarnings).
    [[nodiscard]] const WarningSet& getGeometryWarnings() const noexcept
    {
        return m_geometryWarnings;
    }

private:
    /// The angle of attack beyond which a tube stalls (STALL_ANGLE): 20 degrees.
    static constexpr double kStallAngle = 20 * std::numbers::pi / 180;

    /// At present tubes are only allowed a cant angle of 0 (Java: the field cantAngle).
    static constexpr double kCantAngle = 0;

    /// The position of the CP along the chord, as a fraction of it (calculateCPPos()).
    [[nodiscard]] double calculateCPPos(const FlightConditions& conditions) const noexcept;

    // Parameters straight from the tube fin set, copied at construction.
    double m_bodyRadius;
    double m_chord;
    double m_outerRadius;

    // Values precomputed at construction.
    double m_ar{0};              ///< the aspect ratio 2 * inner radius / chord
    double m_intersticeArea{0};  ///< the area between one tube and the body
    double m_wettedArea{0};      ///< the wetted area of one tube, for the friction drag
    double m_cnaconst{0};        ///< CNa times the reference area

    WarningSet m_geometryWarnings;
};

}  // namespace QtRocket
