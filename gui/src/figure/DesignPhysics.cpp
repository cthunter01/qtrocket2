#include "QtRocket/gui/figure/DesignPhysics.h"

#include <cmath>
#include <limits>
#include <utility>

#include <QObject>
#include <QString>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the reports of Guard.
[[nodiscard]] QString guardContext()
{
    return QStringLiteral("Design physics");
}

/// Whether a CP parameter @p value is the one already set, @p current (RocketPanel's setters:
/// MathUtil.equals(), or both NaN).
[[nodiscard]] bool sameParameter(double value, double current)
{
    return MathUtil::equals(value, current) || (std::isnan(value) && std::isnan(current));
}

}  // namespace

DesignPhysics::Physics DesignPhysics::computePhysics(const FlightConfiguration& configuration,
                                                     FlightConditions&          conditions,
                                                     WarningSet&                warnings,
                                                     AerodynamicCalculator&     calculator,
                                                     bool useWorstCp, RocketInfo& info)
{
    Physics physics;
    physics.length = configuration.getLength();

    double diameter = std::numeric_limits<double>::quiet_NaN();
    for (const RocketComponent* component : configuration.getCoreComponents())
    {
        if (const auto* symmetric = dynamic_cast<const SymmetricComponent*>(component))
        {
            diameter = MathUtil::max(diameter, symmetric->getForeRadius() * 2,
                                     symmetric->getAftRadius() * 2);
        }
    }
    physics.diameter = diameter;

    physics.cp = calculateCp(configuration, conditions, warnings, calculator, useWorstCp);
    physics.cg = MassCalculator::calculateLaunch(configuration).getCM();
    physics.massWithoutMotors = MassCalculator::calculateStructure(configuration).getMass();
    physics.warnings          = warnings;

    info.setLength(physics.length);
    info.setDiameter(physics.diameter);
    applyCgAndCp(info, physics.cg, physics.cp);
    info.setMassWithMotors(physics.cg.weight);
    info.setMassWithoutMotors(physics.massWithoutMotors);
    info.setWarnings(warnings);
    return physics;
}

void DesignPhysics::applyCgAndCp(RocketInfo& info, const Coordinate& cg, const Coordinate& cp)
{
    info.setCG(cg.weight > MassCalculator::kMinMass ? cg.x : 0);
    info.setCP(cp.weight > MathUtil::kEpsilon ? cp.x : 0);
}

Coordinate DesignPhysics::calculateCp(const FlightConfiguration& configuration,
                                      FlightConditions& conditions, WarningSet& warnings,
                                      AerodynamicCalculator& calculator, bool useWorstCp)
{
    warnings.clear();
    // Simulations check geometry separately; the design HUD needs those warnings here.
    calculator.checkGeometry(configuration, configuration.getRocket(), &warnings);
    if (useWorstCp)
    {
        return calculator.getWorstCP(configuration, conditions, &warnings);
    }
    return calculator.getCP(configuration, conditions, &warnings);
}

DesignPhysics::CaretPositions DesignPhysics::caretPositions(const Physics& physics, FigureView view,
                                                            double rotation)
{
    // Only in the side and top views of a design with a length (not a NaN one).
    const bool shown = physics.length > 0 && (view == FigureView::TOP || view == FigureView::SIDE);
    if (!shown)
    {
        return {};
    }
    CaretPositions positions;
    if (physics.cp.weight > MathUtil::kEpsilon)
    {
        positions.cpX = physics.cp.x;
        // map the 3D value into the 2D Display Panel
        positions.cpY = (physics.cp.y * std::cos(rotation)) + (physics.cp.z * std::sin(rotation));
    }
    if (physics.cg.weight > MassCalculator::kMinMass)
    {
        positions.cgX = physics.cg.x;
        // map the 3D value into the 2D Display Panel
        positions.cgY = (physics.cg.y * std::cos(rotation)) + (physics.cg.z * std::sin(rotation));
    }
    // We need to flip the y coordinate if we are in top view
    if (view == FigureView::TOP)
    {
        positions.cgY = -positions.cgY;
    }
    return positions;
}

DesignPhysics::DesignPhysics(RocketFigure& figure, const Preferences& preferences, QObject* parent)
  : QObject(parent),
    m_figure(&figure),
    m_preferences(&preferences),
    m_info(std::as_const(figure.rocket()).getSelectedConfiguration(), preferences)
{
    for (const QObject* ancestor = parent; ancestor != nullptr; ancestor = ancestor->parent())
    {
        if (ancestor == &figure)
        {
            bug("DesignPhysics: the object must not be a child of its figure");
        }
    }
    update();

    figure.addRelativeExtra(m_cpCaret);
    figure.addRelativeExtra(m_cgCaret);
    figure.addAbsoluteExtra(m_info);
    connect(&figure, &RocketFigure::rocketChanged, this,
            guarded([this] { update(); }, guardContext()));
    connect(&figure, &ScaleFigure::changed, this,
            guarded([this] { figureChanged(); }, guardContext()));
}

DesignPhysics::~DesignPhysics()
{
    if (RocketFigure* figure = m_figure.data(); figure != nullptr)
    {
        figure->removeRelativeExtra(m_cpCaret);
        figure->removeRelativeExtra(m_cgCaret);
        figure->removeAbsoluteExtra(m_info);
        figure->update();
    }
}

void DesignPhysics::setCPAOA(double aoa)
{
    if (sameParameter(aoa, m_cpAOA))
    {
        return;
    }
    m_cpAOA = aoa;
    update();
}

void DesignPhysics::setCPTheta(double theta)
{
    if (sameParameter(theta, m_cpTheta))
    {
        return;
    }
    m_cpTheta = theta;
    if (RocketFigure* figure = m_figure.data(); figure != nullptr && !std::isnan(theta))
    {
        figure->setRotation(theta);
    }
    update();
}

void DesignPhysics::setCPMach(double mach)
{
    if (sameParameter(mach, m_cpMach))
    {
        return;
    }
    m_cpMach = mach;
    update();
}

void DesignPhysics::setCPRoll(double roll)
{
    if (sameParameter(roll, m_cpRoll))
    {
        return;
    }
    m_cpRoll = roll;
    update();
}

void DesignPhysics::setShowWarnings(bool showWarnings)
{
    m_info.setShowWarnings(showWarnings);
    if (RocketFigure* figure = m_figure.data(); figure != nullptr)
    {
        figure->update();
    }
}

void DesignPhysics::update()
{
    RocketFigure* figure = m_figure.data();
    if (figure == nullptr)
    {
        return;
    }
    const FlightConfiguration& configuration =
        std::as_const(figure->rocket()).getSelectedConfiguration();
    FlightConditions conditions(configuration);

    m_info.setCurrentConfig(configuration);

    const double mach = std::isnan(m_cpMach) ? m_preferences->getDefaultMach() : m_cpMach;
    conditions.setMach(mach);
    m_info.setMach(mach);

    conditions.setAOA(std::isnan(m_cpAOA) ? 0.0 : m_cpAOA);
    m_info.setAOA(m_cpAOA);

    conditions.setRollRate(std::isnan(m_cpRoll) ? 0.0 : m_cpRoll);

    if (!std::isnan(m_cpTheta))
    {
        conditions.setTheta(m_cpTheta);
    }
    m_info.setTheta(m_cpTheta);
    m_physics = computePhysics(configuration, conditions, m_warnings, m_calculator,
                               std::isnan(m_cpTheta), m_info);

    placeCarets(*figure);
    Q_EMIT updated();
    figure->update();
}

void DesignPhysics::figureChanged()
{
    RocketFigure* figure = m_figure.data();
    if (figure == nullptr ||
        (figure->view() == m_caretView && figure->rotation() == m_caretRotation))
    {
        return;
    }
    // when switching from side view to back view, need to clear CP & CG markers
    placeCarets(*figure);
    figure->update();
}

void DesignPhysics::placeCarets(const RocketFigure& figure)
{
    m_caretView                    = figure.view();
    m_caretRotation                = figure.rotation();
    const CaretPositions positions = caretPositions(m_physics, m_caretView, figure.rotation(true));
    m_cgCaret.setPosition(positions.cgX, positions.cgY);
    m_cpCaret.setPosition(positions.cpX, positions.cpY);
}

}  // namespace QtRocket::Gui
