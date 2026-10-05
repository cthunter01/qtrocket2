#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <span>

#include "QtRocket/aero/barrowman/Naca1307FinBodyInterference.h"
#include "QtRocket/aero/barrowman/RocketComponentCalc.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/FinSet.h"

namespace QtRocket
{

class AerodynamicForces;
class BodyTube;
class Coordinate;
class FlightConditions;
class Transformation;

/// The aerodynamic calculation of a set of planar fins for the extended Barrowman method
/// (OpenRocket's aerodynamics/barrowman/FinSetCalc): a trapezoidal, elliptical or freeform fin
/// set on any symmetric body component.
///
/// At construction it copies the fin set's thickness, body radius, fin count, cant angle, span,
/// planform area and cross-section, and derives from the fin outline, cut into kDivisions chords
/// from root to tip: the mean aerodynamic chord (length, leading edge and spanwise position), the
/// aspect ratio 2 span^2 / area (the two fins of a pair joined), the mean cosines of the midchord
/// and leading-edge sweep, and the roll damping sum; the number of fins that interfere with each
/// other (the fins of every fin set on the same parent that overlaps this one axially); and, for
/// a trapezoidal fin set on a body tube, the NACA Report 1307 fin-body interference model.
/// Nothing of the fin set is read later, and no reference to it is kept: a change of the fin set
/// needs a new calculator (the Barrowman calculators drop theirs when the rocket's tree or
/// aerodynamics change).
///
/// calculateNonaxialForces() is called once per fin, with the fin's instance transformation: the
/// normal force of a fin is the single-fin CNa times sin^2 of the angle between the lateral
/// airflow (FlightConditions::getTheta()) and the fin (the transformation's x rotation), so that
/// the fins of a set add up to the set's CNa. The side force and yaw moment are always 0, as in
/// OpenRocket.
///
/// One calculator must not be used from two threads at once (the NACA model caches its last
/// results); a calculator belongs to one simulation.
///
/// Kept from OpenRocket, on purpose:
/// - The area and the span are the fin set's own (FinSet::getPlanformArea(), getSpan()), while
///   the MAC, the sweep cosines and the roll damping sum come from the chords. An area that is
///   NaN (a canted fin on a body radius of 0) is not "no area": the forces are then NaN.
/// - A canted trapezoidal fin set that ends flush with its body tube gets no NACA model: the
///   cant moves the fin's absolute front back a little, so the tube ends before the root does.
/// - The roll damping sums the chords as if they were kDivisions strips where the fin tips are
///   stalled (roll rate * tip radius / velocity above 15 degrees), and kDivisions - 1 elsewhere;
///   between Mach 0.9 and 1.5 it interpolates the values at Mach 0.89 and 1.51.
/// - A chord division that the outline crosses in a point of its own may count that point twice.
/// - tau = body radius / (span + body radius) is reset to 0 when it is NaN or infinite. That is a
///   defence only, which no fin set reaches: a NaN or infinite body radius throws at
///   construction, a NaN span leaves no chord and a span of 0 no area (both are "no forces"
///   before tau is formed), and span + body radius = 0 needs a negative span, whose area is
///   below the zero-area limit.
///
/// Deviations from OpenRocket:
/// - A fin set without a parent throws BugError (Java: IllegalStateException), and so does a
///   count of interfering fins below the fin set's own, which a NaN root chord or position gives
///   (Java: BugException); both are thrown by the constructor.
/// - A parent without an instance location, which no component tree has, throws BugError from
///   the constructor too (Java: ArrayIndexOutOfBoundsException from toRelative(...)[0]).
/// - Java's static K1, K2 and K3 interpolators and its transonic CNa PolyInterpolator are
///   function-local statics, built at first use from the same expressions.
/// - Java's UnsupportedOperationException for a cross-section that is none of the three cannot
///   be reached: FinCrossSection has no other value (BugError all the same).
/// - Public here, for the tests: calculateFinCNa1() and calculateCPPos() (Java: protected,
///   reached through a subclass by FinSetCalcTest), calculateCylindricalAfterbodyEnd(),
///   calculateBodyFinInterferenceFactor() and calculateNacaApplicabilityWeight() (Java:
///   package-private, called by FinBodyInterferenceTest), and getters for the protected fields.
///   usesNacaInterference() is an addition (Java: a private field).
/// - calculateFinGeometry() is private and takes no part in overriding (Java: protected; only
///   the constructor calls it).
/// - A geometry warning names its fin set by a snapshot of the id and name (see MessageSource),
///   taken at construction. ComponentCalcMap makes the calculation of a renamed fin set anew, so
///   a calculator's warnings carry the current name, as Java's do.
class FinSetCalc final : public RocketComponentCalc
{
public:
    /// The number of divisions in the fin chords (DIVISIONS).
    static constexpr int kDivisions = 48;

    /// A calculation for the fin set @p component, which must have a parent.
    /// @throws BugError when @p component has no parent, when the parent has no instance location,
    ///         or when fewer fins than its own are counted as interfering (see the class
    ///         comment); and as FinSet::getFinFront() when the parent is not a
    ///         SymmetricComponent.
    explicit FinSetCalc(const FinSet& component);

    /// The non-axial forces of one fin (normal force, pitch and roll moments, CP and CNa; side
    /// force and yaw moment 0) into @p forces, and the geometry warnings into @p warnings on
    /// every call. A fin of no area or no MAC span gives zeros.
    ///
    /// CNa is the single-fin CNa (calculateFinCNa1()) times sin^2(theta - the x rotation of
    /// @p transform), times the fin-fin interference factor of the interfering fin count (1 up to
    /// four fins, 0.948, 0.913, 0.854 and 0.81 for five to eight, 0.75 with the PARALLEL_FINS
    /// warning beyond), times the body-fin interference: the NACA 1307 model where it applies
    /// (which also moves the CP by the load the fin carries over to the body), faded by
    /// calculateNacaApplicabilityWeight() into the classical factor
    /// calculateBodyFinInterferenceFactor(), which is used alone for every other geometry.
    /// CN is CNa times the angle of attack up to the 20 degree stall angle. The roll forcing
    /// coefficient is (MAC span + body radius) * single-fin CNa * wing incidence factor * cant
    /// angle / reference length, reduced linearly to 0 between 20 and 30 degrees; the roll
    /// damping coefficient follows the roll rate (0 below 0.1 rad/s).
    void calculateNonaxialForces(const FlightConditions& conditions,
                                 const Transformation& transform, AerodynamicForces& forces,
                                 WarningSet& warnings) override;

    /// @p componentCf * (1 + 2 thickness / MAC length) * 2 fin area / reference area; 0 for a
    /// fin of no area or no MAC length.
    [[nodiscard]] double calculateFrictionCD(const FlightConditions& conditions, double componentCf,
                                             WarningSet& warnings) override;

    /// The leading-edge pressure drag: for a rounded or airfoil cross-section the blunt-edge
    /// coefficient of the Mach number (three regimes: below 0.9, below 1, from 1), for a square
    /// one @p stagnationCD; times cos^2 of the leading-edge sweep and span * thickness /
    /// reference area. 0 for a fin of no area. The trailing edge is calculateComponentBaseCD().
    [[nodiscard]] double calculatePressureCD(const FlightConditions& conditions,
                                             double stagnationCD, double baseCD,
                                             WarningSet& warnings) override;

    /// The trailing-edge base drag: @p baseCD for a square cross-section, half of it for a
    /// rounded one and none for an airfoil; times span * thickness / reference area. 0 for a fin
    /// of no area.
    [[nodiscard]] double calculateComponentBaseCD(const FlightConditions& conditions, double baseCD,
                                                  WarningSet& warnings) override;

    /// The length of the mean aerodynamic chord, which the friction drag needs.
    [[nodiscard]] double getMACLength() const noexcept { return m_macLength; }

    /// The middle of the mean aerodynamic chord, from the leading edge of the root.
    [[nodiscard]] double getMidchordPos() const noexcept { return m_macLead + (0.5 * m_macLength); }

    // ---- public for the tests (Java: protected or package-private; see the class comment)

    /// The CNa of one fin without interference, based on the reference area: the subsonic
    /// formula up to Mach 0.9, the third-order supersonic expansion in the angle of attack (at
    /// most the stall angle) from Mach 1.5 with the coefficients K1, K2 and K3 tabulated up to
    /// Mach 5, and between the two the polynomial that matches both values and derivatives. 0
    /// for a fin of no area, span or midchord sweep cosine.
    [[nodiscard]] double calculateFinCNa1(const FlightConditions& conditions) const;

    /// The CP along the mean aerodynamic chord as a fraction of it: the quarter chord up to Mach
    /// 0.5, supersonicCPPos(aspect ratio * beta) from Mach 2, and transonicCPPos() between.
    [[nodiscard]] double calculateCPPos(const FlightConditions& conditions) const noexcept;

    /// The end of the constant-radius body behind the front of @p finSet's root, measured from
    /// it, for the NACA pressure integration: the end of @p bodyTube and of every flush body tube
    /// of the same radius that follows it in the same parent. The walk stops at a stage or
    /// assembly boundary and at every transition, gap or radius change.
    [[nodiscard]] static double calculateCylindricalAfterbodyEnd(const FinSet&   finSet,
                                                                 const BodyTube& bodyTube);

    /// The classical combined fin-in-body and body-in-fin normal force multiplier for
    /// @p tau = body radius / (span + body radius): (1 + tau)^2 up to Mach 0.9 (equations 14 and
    /// 21 of NACA Report 1307 for slender configurations), 1 + tau from Mach 1.5, and between the
    /// two the body's part tau * (1 + tau) blended out linearly.
    [[nodiscard]] static double calculateBodyFinInterferenceFactor(double tau,
                                                                   double mach) noexcept;

    /// The weight of the NACA model at the unsigned angle of attack @p angleOfAttack (radians): 1
    /// up to 10 degrees, a smooth step down to 0 at the 20 degree stall angle, and 0 beyond
    /// (reverse flow included), for a negative angle and for NaN or infinity.
    [[nodiscard]] static double calculateNacaApplicabilityWeight(double angleOfAttack) noexcept;

    /// The leading edge of the mean aerodynamic chord, from the leading edge of the root.
    [[nodiscard]] double getMACLead() const noexcept { return m_macLead; }
    /// The spanwise position of the mean aerodynamic chord, from the root.
    [[nodiscard]] double getMACSpan() const noexcept { return m_macSpan; }
    /// The planform area of one fin.
    [[nodiscard]] double getFinArea() const noexcept { return m_finArea; }
    /// The aspect ratio 2 span^2 / area; 0 for a fin of no area.
    [[nodiscard]] double getAspectRatio() const noexcept { return m_ar; }
    /// The span of one fin.
    [[nodiscard]] double getSpan() const noexcept { return m_span; }
    /// The mean cosine of the midchord sweep angle.
    [[nodiscard]] double getCosGamma() const noexcept { return m_cosGamma; }
    /// The mean cosine of the leading-edge sweep angle.
    [[nodiscard]] double getCosGammaLead() const noexcept { return m_cosGammaLead; }
    /// The roll damping sum: the integral of chord length * (body radius + y)^2 over the span.
    [[nodiscard]] double getRollSum() const noexcept { return m_rollSum; }
    /// The number of fins in interference.
    [[nodiscard]] int getInterferenceFinCount() const noexcept { return m_interferenceFinCount; }
    /// The x of the leading edge at each chord division, root to tip.
    [[nodiscard]] std::span<const double, kDivisions> getChordLead() const noexcept
    {
        return m_chordLead;
    }
    /// The x of the trailing edge at each chord division.
    [[nodiscard]] std::span<const double, kDivisions> getChordTrail() const noexcept
    {
        return m_chordTrail;
    }
    /// The length of fin material at each chord division (less than trailing - leading edge
    /// where the outline is concave).
    [[nodiscard]] std::span<const double, kDivisions> getChordLength() const noexcept
    {
        return m_chordLength;
    }
    /// The geometry warnings collected at construction (zero area, jagged edge, thick fin).
    [[nodiscard]] const WarningSet& getGeometryWarnings() const noexcept
    {
        return m_geometryWarnings;
    }
    /// Whether the NACA Report 1307 model is used: a trapezoidal fin set on a body tube, not
    /// ahead of the tube's front, whose geometry the report covers.
    [[nodiscard]] bool usesNacaInterference() const noexcept
    {
        return m_bodyFinInterference.has_value();
    }

private:
    static constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

    /// Pre-calculates the fin geometry values (calculateFinGeometry()).
    void calculateFinGeometry(const FinSet& component);

    /// The part of calculateFinGeometry() that checks the outline: the JAGGED_EDGED_FIN warning
    /// for an outline that rises by more than 1 mm after having fallen by more than 1 mm (the
    /// root is left out), and THICK_FIN for a thickness above half the body radius.
    void checkOutline(const FinSet& component);

    /// The part of calculateFinGeometry() that fills the chord lead and trail positions and
    /// lengths from the closed outline @p points (the root included).
    void calculateChords(std::span<const Coordinate> points);

    /// The contribution of the outline segment (@p x1, @p y1) - (@p x2, @p y2) to the chord
    /// division @p i.
    void addChordPoint(std::size_t i, double x1, double y1, double x2, double y2);

    /// The part of calculateFinGeometry() that checks and corrects inconsistent chords.
    void correctChords();

    /// The part of calculateFinGeometry() that sums the chords into the MAC, the sweep cosines
    /// and the roll damping sum, for the body radius @p radius at the front of the root.
    void calculateFinProperties(double radius);

    /// Counts the fins in interference (calculateInterferenceFinCount()).
    void calculateInterferenceFinCount(const FinSet& component);

    /// The complete NACA model, only for the constant-radius trapezoidal geometries covered by
    /// Report 1307; nullopt for every other fin and parent shape, which use the scalar fallback
    /// (createBodyFinInterferenceModel()).
    [[nodiscard]] std::optional<Naca1307FinBodyInterference> createBodyFinInterferenceModel(
        const FinSet& finSet) const;

    /// The roll damping moment coefficient (calculateDampingMoment()).
    [[nodiscard]] double calculateDampingMoment(const FlightConditions& conditions) const;

    // Copied from the fin set at construction, in Java's order.
    double               m_thickness;
    double               m_bodyRadius;
    int                  m_finCount;
    double               m_cantAngle;
    double               m_span;     ///< Fin span
    double               m_finArea;  ///< Fin area
    FinSet::CrossSection m_crossSection;
    bool                 m_rectangularPlanform;

    // Derived by calculateFinGeometry(); NaN until then, as in Java.
    double m_macLength{kNaN};     ///< MAC length
    double m_macLead{kNaN};       ///< MAC leading edge position
    double m_macSpan{kNaN};       ///< MAC spanwise position
    double m_ar{kNaN};            ///< Fin aspect ratio
    double m_cosGamma{kNaN};      ///< Cosine of midchord sweep angle
    double m_cosGammaLead{kNaN};  ///< Cosine of leading edge sweep angle
    double m_rollSum{kNaN};       ///< Roll damping sum term

    int m_interferenceFinCount{-1};  ///< No. of fins in interference

    std::array<double, kDivisions> m_chordLead{};
    std::array<double, kDivisions> m_chordTrail{};
    std::array<double, kDivisions> m_chordLength{};

    WarningSet m_geometryWarnings;

    std::optional<Naca1307FinBodyInterference> m_bodyFinInterference;
};

}  // namespace QtRocket
