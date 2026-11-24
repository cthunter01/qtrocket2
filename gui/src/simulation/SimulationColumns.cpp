#include "QtRocket/gui/simulation/SimulationColumns.h"

#include <compare>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <QChar>
#include <QLatin1String>
#include <QPalette>
#include <QString>

#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusColors.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/SimulationAbort.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/SimulationStepperMethod.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/unit/Value.h"
#include "QtRocket/util/AlphanumComparator.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/JavaCollator.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Gui
{

namespace
{

using Column = SimulationColumns::Column;

/// The unit group of a value column; NONE for the others (Java's Column.getUnits()).
[[nodiscard]] UnitGroupId unitGroupId(Column column) noexcept
{
    switch (column)
    {
        case Column::LAUNCH_ROD_VELOCITY:
        case Column::DEPLOYMENT_VELOCITY:
        case Column::MAX_VELOCITY:
        case Column::GROUND_HIT_VELOCITY:
            return UnitGroupId::VELOCITY;
        case Column::APOGEE:
            return UnitGroupId::DISTANCE;
        case Column::OPTIMUM_COAST_TIME:
            return UnitGroupId::SHORT_TIME;
        case Column::MAX_ACCELERATION:
            return UnitGroupId::ACCELERATION;
        case Column::TIME_TO_APOGEE:
        case Column::FLIGHT_TIME:
            return UnitGroupId::LONG_TIME;
        case Column::STATUS:
        case Column::WARNINGS:
        case Column::NAME:
        case Column::CONFIGURATION:
        case Column::SIMULATION_STEPPER:
            break;
    }
    return UnitGroupId::NONE;
}

/// The text of a value that holds one; a BugError for another kind.
[[nodiscard]] const std::string& textOf(Column column, const SimulationColumns::CellValue& value)
{
    const auto* text = std::get_if<std::string>(&value);
    if (text == nullptr)
    {
        bug("The simulations table's column " + std::string(SimulationColumns::id(column)) +
            " has a value that is no text");
    }
    return *text;
}

/// A value of a value column or a warnings column as an optional (nothing for std::monostate);
/// a BugError for another kind.
template <class T>
[[nodiscard]] std::optional<T> optionalOf(Column column, const SimulationColumns::CellValue& value)
{
    if (std::holds_alternative<std::monostate>(value))
    {
        return std::nullopt;
    }
    const T* held = std::get_if<T>(&value);
    if (held == nullptr)
    {
        bug("The simulations table's column " + std::string(SimulationColumns::id(column)) +
            " has a value of another kind");
    }
    return *held;
}

/// "<font color=\"#rrggbb\">text</font>" in @p key's colour for @p palette.
[[nodiscard]] QString colored(ThemeColors::Key key, const QPalette& palette, const QString& text)
{
    return ColorConversion::formatHtmlColor(ThemeColors::color(key, palette), text);
}

/// The text of a cause of an abort as Java's Message.toString() gives it, translated: the cause
/// and, when the abort names sources, ":  " and their quoted names.
[[nodiscard]] QString abortText(const SimulationAbort& abort)
{
    // Message.addSourcesToMessageText() (Message::toString()), over the translated cause.
    QString text = enumText(abort.cause());
    if (abort.sources().empty())
    {
        return text;
    }
    text += QLatin1String(":  ");
    bool first = true;
    for (const MessageSource& source : abort.sources())
    {
        if (!first)
        {
            text += QLatin1String(", ");
        }
        first = false;
        text += QLatin1Char('"') + QtStrings::fromUtf8(source.name) + QLatin1Char('"');
    }
    return text;
}

/// "<br>" and each warning of @p warnings with Warning::toString(), after the group's title in
/// @p key's colour (getSimulationWarningsToolTip()'s loop of one priority).
void appendWarnings(QString& tip, const std::vector<const Warning*>& warnings, const QString& title,
                    ThemeColors::Key key, const QPalette& palette)
{
    if (warnings.empty())
    {
        return;
    }
    tip += QLatin1String("<br><b>") + colored(key, palette, title) + QLatin1String("</b>");
    for (const Warning* warning : warnings)
    {
        tip += QLatin1String("<br>") + QtStrings::fromUtf8(warning->toString());
    }
}

}  // namespace

// ------------------------------------------------------------------------------ the columns

SimulationColumns::Column SimulationColumns::columnAt(int column)
{
    if (column < 0 || column >= kColumnCount)
    {
        bug("The simulations table has no column " + std::to_string(column));
    }
    return kAllColumns.at(static_cast<std::size_t>(column));
}

int SimulationColumns::indexOf(Column column) noexcept
{
    return static_cast<int>(column);
}

std::string_view SimulationColumns::id(Column column) noexcept
{
    switch (column)
    {
        case Column::STATUS:
            return "status";
        case Column::WARNINGS:
            return "warnings";
        case Column::NAME:
            return "name";
        case Column::CONFIGURATION:
            return "configuration";
        case Column::SIMULATION_STEPPER:
            return "simulationStepper";
        case Column::LAUNCH_ROD_VELOCITY:
            return "launchRodVelocity";
        case Column::APOGEE:
            return "apogee";
        case Column::DEPLOYMENT_VELOCITY:
            return "deploymentVelocity";
        case Column::OPTIMUM_COAST_TIME:
            return "optimumCoastTime";
        case Column::MAX_VELOCITY:
            return "maxVelocity";
        case Column::MAX_ACCELERATION:
            return "maxAcceleration";
        case Column::TIME_TO_APOGEE:
            return "timeToApogee";
        case Column::FLIGHT_TIME:
            return "flightTime";
        case Column::GROUND_HIT_VELOCITY:
            return "groundHitVelocity";
    }
    return {};
}

std::optional<SimulationColumns::Column> SimulationColumns::fromId(std::string_view id) noexcept
{
    for (const Column column : kAllColumns)
    {
        if (SimulationColumns::id(column) == id)
        {
            return column;
        }
    }
    return std::nullopt;
}

QString SimulationColumns::headerText(Column column)
{
    switch (column)
    {
        case Column::STATUS:
            return {};
        case Column::WARNINGS:
            //: simpanel.col.Warnings
            return tr("Warnings");
        case Column::NAME:
            //: simpanel.col.Name
            return tr("Name");
        case Column::CONFIGURATION:
            //: simpanel.col.Configuration
            return tr("Configuration");
        case Column::SIMULATION_STEPPER:
            //: simpanel.col.SimStepper
            return tr("Sim. Stepper");
        case Column::LAUNCH_ROD_VELOCITY:
            //: simpanel.col.Velocityoffrod
            return tr("Velocity off rod");
        case Column::APOGEE:
            //: simpanel.col.Apogee
            return tr("Apogee");
        case Column::DEPLOYMENT_VELOCITY:
            //: simpanel.col.Velocityatdeploy
            return tr("Velocity at deployment");
        case Column::OPTIMUM_COAST_TIME:
            //: simpanel.col.OptimumCoastTime
            return tr("Optimum delay");
        case Column::MAX_VELOCITY:
            //: simpanel.col.Maxvelocity
            return tr("Max. velocity");
        case Column::MAX_ACCELERATION:
            //: simpanel.col.Maxacceleration
            return tr("Max. acceleration");
        case Column::TIME_TO_APOGEE:
            //: simpanel.col.Timetoapogee
            return tr("Time to apogee");
        case Column::FLIGHT_TIME:
            //: simpanel.col.Flighttime
            return tr("Flight time");
        case Column::GROUND_HIT_VELOCITY:
            //: simpanel.col.Groundhitvelocity
            return tr("Ground hit velocity");
    }
    return {};
}

QString SimulationColumns::chooserText(Column column)
{
    if (column == Column::STATUS)
    {
        //: simpanel.col.Status
        return tr("Status");
    }
    return headerText(column);
}

QString SimulationColumns::headerToolTip(Column column)
{
    switch (column)
    {
        case Column::SIMULATION_STEPPER:
            //: simpanel.col.SimStepper.ttip
            return tr("Simulation Stepper Method");
        case Column::OPTIMUM_COAST_TIME:
            //: simpanel.col.OptimumCoastTime.ttip
            return tr("The time between last motor burnout and maximum possible altitude.");
        default:
            return headerText(column);
    }
}

bool SimulationColumns::isHiddenByDefault(Column column) noexcept
{
    return column == Column::SIMULATION_STEPPER;
}

bool SimulationColumns::isValueColumn(Column column) noexcept
{
    return unitGroupId(column) != UnitGroupId::NONE;
}

const UnitGroup& SimulationColumns::units(Column column)
{
    return unitGroup(unitGroupId(column));
}

int SimulationColumns::defaultWidth(Column column) noexcept
{
    switch (column)
    {
        case Column::WARNINGS:
            return 40;
        case Column::NAME:
        case Column::CONFIGURATION:
            return 125;
        default:
            return 100;
    }
}

int SimulationColumns::exactWidth(Column column) noexcept
{
    return column == Column::STATUS ? kStatusColumnWidth : 0;
}

bool SimulationColumns::isAutoSize(Column column) noexcept
{
    return column == Column::WARNINGS;
}

bool SimulationColumns::includesHeaderInAutoSize(Column column) noexcept
{
    return column != Column::WARNINGS;
}

// ------------------------------------------------------------------------------ the cells

std::optional<double> SimulationColumns::summaryValue(Column column, const Simulation& simulation)
{
    if (!isValueColumn(column))
    {
        bug("The simulations table's column " + std::string(id(column)) + " is no value column");
    }
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        return std::nullopt;
    }
    switch (column)
    {
        case Column::LAUNCH_ROD_VELOCITY:
            return data->getLaunchRodVelocity();
        case Column::APOGEE:
            return data->getMaxAltitude();
        case Column::DEPLOYMENT_VELOCITY:
            return data->getDeploymentVelocity();
        case Column::OPTIMUM_COAST_TIME:
            return data->getOptimumDelay();
        case Column::MAX_VELOCITY:
            return data->getMaxVelocity();
        case Column::MAX_ACCELERATION:
            return data->getMaxAcceleration();
        case Column::TIME_TO_APOGEE:
            return data->getTimeToApogee();
        case Column::FLIGHT_TIME:
            return data->getFlightTime();
        case Column::GROUND_HIT_VELOCITY:
            return data->getGroundHitVelocity();
        default:
            break;
    }
    QTROCKET_UNREACHABLE();
}

SimulationColumns::CellValue SimulationColumns::cellValue(Column             column,
                                                          const Simulation&  simulation,
                                                          Simulation::Status status,
                                                          const Preferences& preferences)
{
    switch (column)
    {
        case Column::STATUS:
            return QtStrings::toUtf8(statusSortText(status, simulation));
        case Column::WARNINGS:
        {
            const std::optional<WarningCounts> counts = warningCounts(simulation);
            if (!counts.has_value())
            {
                return std::monostate{};
            }
            return *counts;
        }
        case Column::NAME:
            return simulation.getName();
        case Column::CONFIGURATION:
            // Java: RocketDescriptor.format(rocket, id), the configuration's name with its
            // substitutions.
            return simulation.getRocket()
                .getFlightConfiguration(simulation.getId())
                .getName(preferences);
        case Column::SIMULATION_STEPPER:
        {
            const SimulationStepperMethod method =
                simulation.getOptions().getSimulationStepperMethodChoice();
            return QtStrings::toUtf8(
                translatedEnumText(shortNameKey(method), getShortName(method)));
        }
        default:
            break;
    }
    const std::optional<double> value = summaryValue(column, simulation);
    if (!value.has_value())
    {
        return std::monostate{};
    }
    return Value(*value, units(column));
}

QString SimulationColumns::cellText(Column column, const CellValue& value)
{
    if (column == Column::STATUS || column == Column::WARNINGS)
    {
        return {};
    }
    if (const auto* text = std::get_if<std::string>(&value); text != nullptr)
    {
        return QtStrings::fromUtf8(*text);
    }
    if (const auto* number = std::get_if<Value>(&value); number != nullptr)
    {
        return QtStrings::fromUtf8(number->toString());
    }
    return {};
}

std::weak_ordering SimulationColumns::compare(Column column, const CellValue& a, const CellValue& b)
{
    switch (column)
    {
        case Column::STATUS:
        case Column::CONFIGURATION:
        case Column::SIMULATION_STEPPER:
            // TableRowSorter: the toString() of the values, with Collator.getInstance().
            return JavaCollator::compare(textOf(column, a), textOf(column, b)) <=> 0;
        case Column::NAME:
            return AlphanumComparator::compare(textOf(column, a), textOf(column, b)) <=> 0;
        case Column::WARNINGS:
            // OpenRocket bug 23, fixed: by the counts (see the class comment).
            return TableSortKeys::compareMissingFirst(
                optionalOf<WarningCounts>(column, a), optionalOf<WarningCounts>(column, b),
                [](const WarningCounts& x, const WarningCounts& y) { return x <=> y; });
        default:
            break;
    }
    return TableSortKeys::compareMissingFirst(
        optionalOf<Value>(column, a), optionalOf<Value>(column, b),
        [](const Value& x, const Value& y) { return x.compareTo(y); });
}

// ------------------------------------------------------------------------------ the status

QString SimulationColumns::statusDescription(Simulation::Status status,
                                             const Simulation&  simulation)
{
    QString statusText = translatedEnumText(descriptionKey(status), QtRocket::description(status));
    if (status != Simulation::Status::ABORTED)
    {
        return statusText;
    }
    // Status.getDescription(): every abort on a line of its own (more than one branch can abort).
    QString                            text;
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        return text;
    }
    for (std::size_t branch = 0; branch < data->getBranchCount(); ++branch)
    {
        const FlightEvent* event =
            data->getBranch(branch).getFirstEvent(FlightEvent::Type::SIM_ABORT);
        if (event == nullptr)
        {
            continue;
        }
        // A SIM_ABORT event always carries its abort (FlightEvent refuses another payload).
        const SimulationAbort* abort = event->getAbort();
        text += statusText + QLatin1String("<i>: ") +
                (abort != nullptr ? abortText(*abort) : QString()) + QLatin1String("</i><br>");
    }
    return text;
}

QString SimulationColumns::statusSortText(Simulation::Status status, const Simulation& simulation)
{
    // StatusLabel.toString(): the tooltip without the name, "<br>" -> "-", the tags removed (the
    // colour plays no part).
    QString tip = statusToolTip(status, simulation, false, QPalette());
    tip.replace(QLatin1String("<br>"), QLatin1String("-"));
    return QtStrings::fromUtf8(Strings::removeHtmlTags(QtStrings::toUtf8(tip)));
}

QString SimulationColumns::statusToolTip(Simulation::Status status, const Simulation& simulation,
                                         bool includeName, const QPalette& palette)
{
    QString tip = QStringLiteral("<html>");
    if (includeName)
    {
        tip += QLatin1String("<b>") + QtStrings::fromUtf8(simulation.getName()) +
               QLatin1String("</b><br>");
    }
    if (simulation.getSimulatedData() == nullptr)
    {
        //: simpanel.ttip.noData
        tip += tr("No simulation data available.");
        return tip;
    }
    tip += ColorConversion::formatHtmlColor(SimulationStatusColors::color(status, palette),
                                            statusDescription(status, simulation)) +
           QLatin1String("<br>");
    return tip;
}

QString SimulationColumns::warningsToolTip(const Simulation& simulation, bool includeName,
                                           const QPalette& palette)
{
    QString tip = QStringLiteral("<html>");
    if (includeName)
    {
        tip += QLatin1String("<b>") + QtStrings::fromUtf8(simulation.getName()) +
               QLatin1String("</b>");
    }
    const std::shared_ptr<FlightData>& data = simulation.getSimulatedData();
    if (data == nullptr)
    {
        //: simpanel.ttip.noData
        tip += QLatin1String("<br>") + tr("No simulation data available.");
        return tip;
    }
    const WarningSet& warnings = data->getWarningSet();
    if (warnings.empty())
    {
        //: simpanel.ttip.noWarnings
        tip += QLatin1String("<br>") +
               colored(ThemeColors::Key::TEXT_DIM, palette, tr("No warnings."));
        return tip;
    }
    //: simpanel.ttip.CriticalWarnings
    appendWarnings(tip, warnings.criticalWarnings(), tr("Critical(s):"), ThemeColors::Key::ERROR,
                   palette);
    //: simpanel.ttip.NormalWarnings
    appendWarnings(tip, warnings.normalWarnings(), tr("Warning(s):"), ThemeColors::Key::WARNING,
                   palette);
    //: simpanel.ttip.InformationalWarnings
    appendWarnings(tip, warnings.informationalWarnings(), tr("Informational(s):"),
                   ThemeColors::Key::INFO, palette);
    return tip;
}

std::optional<SimulationColumns::WarningCounts> SimulationColumns::warningCounts(
    const Simulation& simulation)
{
    const WarningSet* warnings = simulation.getSimulatedWarnings();
    if (warnings == nullptr)
    {
        return std::nullopt;
    }
    return WarningCounts{.critical      = warnings->countCritical(),
                         .normal        = warnings->countNormal(),
                         .informational = warnings->countInformational()};
}

QString SimulationColumns::statusIconText(Simulation::Status status)
{
    switch (status)
    {
        case Simulation::Status::NOT_SIMULATED:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Not simulated");
        case Simulation::Status::CANT_RUN:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Can't run, no motors assigned.");
        case Simulation::Status::UPTODATE:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Up to date");
        case Simulation::Status::LOADED:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Loaded from File");
        case Simulation::Status::OUTDATED:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Out-of-date");
        case Simulation::Status::EXTERNAL:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Imported data");
        case Simulation::Status::ABORTED:
            //: Not translated in OpenRocket (Icons.java): the description of a status icon
            return tr("Simulation run aborted");
    }
    return {};
}

}  // namespace QtRocket::Gui
