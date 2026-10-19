#pragma once

#include <limits>

#include <QObject>
#include <QPointer>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/CGCaret.h"
#include "QtRocket/gui/figure/CPCaret.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Coordinate.h"

namespace QtRocket
{
class AerodynamicCalculator;
class FlightConditions;
class FlightConfiguration;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class RocketFigure;

/// The numbers of the design view and the figure elements that show them (OpenRocket's
/// RocketInfoContextHelper and the part of RocketPanel.updateExtras() before the background
/// simulation): the length, the largest diameter, the masses, the CG, the CP with the warnings
/// of the geometry check, the CG and CP carets of the figure and the information overlay
/// (RocketInfo).
///
/// The computation (computePhysics(), for the rocket's selected configuration): FlightConditions
/// of the configuration at the Mach number cpMach(), or Preferences::getDefaultMach() when it is
/// NaN (the default), the angle of attack cpAOA() or 0, the roll rate cpRoll() or 0, and the wind
/// direction cpTheta() when it is set; the warnings are cleared, the whole rocket's geometry is
/// checked (checkGeometry()), and the CP is the worst over every wind direction (getWorstCP()),
/// or the one at cpTheta() when that is set (getCP()). The CG is that of the launch mass
/// (MassCalculator::calculateLaunch()), the mass without motors the structure's; the diameter
/// the largest fore or aft diameter of the symmetric core components (NaN without one). The
/// overlay shows a CG of 0 when the mass is not above MassCalculator::kMinMass and a CP of 0 when
/// its CNa is not above MathUtil::kEpsilon (applyCgAndCp(): getWorstCP() gives a sentinel at
/// Double.MAX_VALUE for a design without lift).
///
/// The carets (caretPositions()): in the side and top views of a design with a length, at the
/// CG and the CP projected on the figure's plane at the figure's rotation (rotation(true): less
/// PI/2 in the top view), the CG's height turned over in the top view (the CP's is not, as in
/// Java); a CG without mass or a CP without lift, the back view and a design without length put
/// them at NaN, where they are not drawn.
///
/// The object belongs to one RocketFigure, which shows its elements: the constructor adds the CP
/// and the CG caret as relative extras and the overlay as an absolute one (RocketPanel's order),
/// the destructor removes them when the figure still exists. It computes at once and then
/// whenever the figure has caught up with the rocket's change events (RocketFigure::
/// rocketChanged(): once per pass of the event loop however many events came, the rocket's own
/// listener being the figure's), synchronously on the GUI thread, and when a CP parameter
/// changes; a new view or rotation of the figure moves the carets without computing again (the
/// numbers do not depend on them). Each computation ends with updated() and a repaint of the
/// figure. Once the figure is gone, nothing is computed: the document's rocket, which must
/// outlive the figure, may be gone too.
///
/// A selection of stages the force analysis cannot handle (an active stage below two inactive
/// ones) is no error here: getWorstCP(), getCP(), checkGeometry() and the mass calculations work
/// for it, and the numbers are shown.
///
/// Deviations from OpenRocket:
/// - Java computes on every change event of the rocket, at once; here once per pass of the event
///   loop, after the figure (a burst of events, a slider drag, gives one computation).
/// - A new rotation or view moves the carets from the numbers already computed: Java computes
///   everything again (the same numbers).
/// - The object must not be a child of its figure (a child is destroyed in the middle of the
///   figure's destructor, when the figure can no longer take its elements back): BugError.
class DesignPhysics final : public QObject
{
    Q_OBJECT

public:
    /// RocketInfoContextHelper.RocketPhysics with the other numbers computePhysics() gives the
    /// overlay.
    struct Physics
    {
        /// The CP in absolute coordinates, with CNa as its weight (getWorstCP()'s sentinel at
        /// Double.MAX_VALUE with the weight 0 for a design without lift).
        Coordinate cp;
        /// The launch CG, with the launch mass as its weight.
        Coordinate cg;
        double     length{0.0};
        /// The largest fore or aft diameter of the symmetric core components; NaN without one.
        double diameter{std::numeric_limits<double>::quiet_NaN()};
        /// The mass of the structure (calculateStructure()).
        double massWithoutMotors{0.0};
        /// The warnings of the geometry check and the CP.
        WarningSet warnings;
    };

    /// The carets' positions in the figure's metres (NaN: not drawn).
    struct CaretPositions
    {
        double cgX{std::numeric_limits<double>::quiet_NaN()};
        double cgY{std::numeric_limits<double>::quiet_NaN()};
        double cpX{std::numeric_limits<double>::quiet_NaN()};
        double cpY{std::numeric_limits<double>::quiet_NaN()};
    };

    /// RocketInfoContextHelper.computePhysics(): the numbers of @p configuration in
    /// @p conditions, the CP the worst (@p useWorstCp) or that of the conditions' theta, with
    /// the warnings in @p warnings (cleared first); they are applied to @p info (its length,
    /// diameter, CG, CP, masses and a copy of the warnings).
    [[nodiscard]] static Physics computePhysics(const FlightConfiguration& configuration,
                                                FlightConditions& conditions, WarningSet& warnings,
                                                AerodynamicCalculator& calculator, bool useWorstCp,
                                                RocketInfo& info);

    /// RocketInfoContextHelper.applyCgAndCp(): the x of @p cg when its weight is above
    /// MassCalculator::kMinMass, else 0, and the x of @p cp when its weight is above
    /// MathUtil::kEpsilon, else 0, set on @p info.
    static void applyCgAndCp(RocketInfo& info, const Coordinate& cg, const Coordinate& cp);

    /// RocketInfoContextHelper.calculateCp(): clears @p warnings, checks the geometry of the
    /// whole rocket, then the worst CP (@p useWorstCp) or the CP at the conditions' theta.
    [[nodiscard]] static Coordinate calculateCp(const FlightConfiguration& configuration,
                                                FlightConditions& conditions, WarningSet& warnings,
                                                AerodynamicCalculator& calculator, bool useWorstCp);

    /// Where updateExtras() puts the carets for @p physics in @p view at @p rotation
    /// (RocketFigure::rotation(true)); see the class comment.
    [[nodiscard]] static CaretPositions caretPositions(const Physics& physics, FigureView view,
                                                       double rotation);

    /// The numbers of @p figure's rocket, computed now, and its elements added to it.
    /// @p preferences (the default Mach number, the overlay's preferences) must outlive the
    /// object.
    /// @throws BugError when @p parent is @p figure or one of its descendants
    DesignPhysics(RocketFigure& figure, const Preferences& preferences, QObject* parent = nullptr);
    /// Takes the elements back from the figure, if it still exists.
    ~DesignPhysics() override;

    DesignPhysics(const DesignPhysics&)            = delete;
    DesignPhysics& operator=(const DesignPhysics&) = delete;
    DesignPhysics(DesignPhysics&&)                 = delete;
    DesignPhysics& operator=(DesignPhysics&&)      = delete;

    /// The figure, null once it is destroyed.
    [[nodiscard]] RocketFigure* figure() const noexcept { return m_figure.data(); }

    /// The numbers of the last computation.
    [[nodiscard]] const Physics& physics() const noexcept { return m_physics; }
    /// The elements in the figure (getExtraCG(), getExtraCP(), getExtraText()).
    [[nodiscard]] CGCaret&          cgCaret() noexcept { return m_cgCaret; }
    [[nodiscard]] const CGCaret&    cgCaret() const noexcept { return m_cgCaret; }
    [[nodiscard]] CPCaret&          cpCaret() noexcept { return m_cpCaret; }
    [[nodiscard]] const CPCaret&    cpCaret() const noexcept { return m_cpCaret; }
    [[nodiscard]] RocketInfo&       info() noexcept { return m_info; }
    [[nodiscard]] const RocketInfo& info() const noexcept { return m_info; }

    /// The CP parameters (getCPAOA() ...), NaN (the default) for the defaults: the angle of
    /// attack 0, the worst wind direction, the preferences' Mach number, the roll rate 0. Setting
    /// one computes again when it changes (MathUtil::equals(), or both NaN); a wind direction
    /// that is set also turns the figure to it (setCPTheta()).
    [[nodiscard]] double cpAOA() const noexcept { return m_cpAOA; }
    void                 setCPAOA(double aoa);
    [[nodiscard]] double cpTheta() const noexcept { return m_cpTheta; }
    void                 setCPTheta(double theta);
    [[nodiscard]] double cpMach() const noexcept { return m_cpMach; }
    void                 setCPMach(double mach);
    [[nodiscard]] double cpRoll() const noexcept { return m_cpRoll; }
    void                 setCPRoll(double roll);

    /// Whether the overlay lists the warnings (RocketPanel's "Show warnings"); repaints.
    [[nodiscard]] bool isShowWarnings() const noexcept { return m_info.isShowWarnings(); }
    void               setShowWarnings(bool showWarnings);

    /// Computes now (updateExtras()), moves the carets, emits updated() and repaints the figure;
    /// nothing once the figure is gone. For a change the rocket announced with no event (the
    /// preferences' Mach number, the default units).
    void update();

Q_SIGNALS:
    /// A computation ended: physics(), the carets and the overlay are up to date.
    void updated();

private:
    /// The figure's change: the carets follow a new view or rotation.
    void figureChanged();
    /// Puts the carets where caretPositions() says for the figure's view and rotation now.
    void placeCarets(const RocketFigure& figure);

    QPointer<RocketFigure> m_figure;
    const Preferences*     m_preferences;
    /// One calculator for the figure, kept for its cache (RocketPanel's aerodynamicCalculator).
    BarrowmanCalculator m_calculator;
    /// The warnings of the last computation (RocketPanel's warnings).
    WarningSet m_warnings;
    Physics    m_physics;
    CGCaret    m_cgCaret{0.0, 0.0};
    CPCaret    m_cpCaret{0.0, 0.0};
    RocketInfo m_info;
    double     m_cpAOA{std::numeric_limits<double>::quiet_NaN()};
    double     m_cpTheta{std::numeric_limits<double>::quiet_NaN()};
    double     m_cpMach{std::numeric_limits<double>::quiet_NaN()};
    double     m_cpRoll{std::numeric_limits<double>::quiet_NaN()};
    /// The view and the rotation the carets were placed for.
    FigureView m_caretView{kDefaultFigureView};
    double     m_caretRotation{0.0};
};

}  // namespace QtRocket::Gui
