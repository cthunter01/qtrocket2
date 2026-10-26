// The motor chooser's core logic (motor/chooser/) against OpenRocket's own chooser: the pins of
// MotorChooserPins.h are what OpenRocket's real ThrustCurveMotorSelectionPanel shows and decides,
// driven headless over the bundled motor database by tools/openrocket-motor-chooser (family
// "chooser", ChooserScenarios.java). Each test below replays a group of the probe's steps on a
// ThrustCurveMotorSelection, in the same order and from the same fresh preferences and default
// units, and compares. Where QtRocket fixes one of OpenRocket's bugs (M3's bugs 8, 9 and 11) the
// test says so and expects QtRocket's answer, derived from OpenRocket's.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/MotorChooserInput.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorPreferences.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/motor/chooser/ImpulseClass.h"
#include "QtRocket/motor/chooser/MotorClass.h"
#include "QtRocket/motor/chooser/MotorFilterSliders.h"
#include "QtRocket/motor/chooser/MotorHolder.h"
#include "QtRocket/motor/chooser/MotorRowFilter.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Md5.h"
#include "TestPaths.h"
#include "goldens/GoldenData.h"
#include "motor/TestMotorDatabase.h"
#include "motor/chooser/MotorChooserPins.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::FlightConfigurationId;
using QtRocket::ImpulseClass;
using QtRocket::InMemoryPreferences;
using QtRocket::InnerTube;
using QtRocket::Manufacturer;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::MotorHolder;
using QtRocket::MotorMount;
using QtRocket::MotorTableSorter;
using QtRocket::Preferences;
using QtRocket::Rocket;
using QtRocket::ThrustCurveMotor;
using QtRocket::ThrustCurveMotorColumns;
using QtRocket::ThrustCurveMotorSelection;
using QtRocket::ThrustCurveMotorSet;
using QtRocket::Test::bundledMotorDatabase;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::MotorChooserCurvePin;
using QtRocket::Test::MotorChooserOpenPin;
using QtRocket::Test::MotorChooserRowScenarioPin;
using QtRocket::Test::MotorChooserSelectionPin;
namespace Pins               = QtRocket::Test;
namespace MotorFilterSliders = QtRocket::MotorFilterSliders;
namespace MotorPreferences   = QtRocket::MotorPreferences;

using MotorPtr = std::shared_ptr<const ThrustCurveMotor>;

// ---- the database

/// The set of the bundled database whose manufacturer's display name and designation are
/// @p manufacturer and @p designation.
const ThrustCurveMotorSet& findSet(std::string_view manufacturer, std::string_view designation)
{
    for (const ThrustCurveMotorSet& set : bundledMotorDatabase().getMotorSets())
    {
        if (set.getManufacturer()->getDisplayName() == manufacturer &&
            set.getDesignation() == designation)
        {
            return set;
        }
    }
    ADD_FAILURE() << "no set " << manufacturer << " " << designation;
    return bundledMotorDatabase().getMotorSets().front();
}

/// The index of @p set in the bundled database.
std::size_t setIndex(const ThrustCurveMotorSet* set)
{
    const auto& sets = bundledMotorDatabase().getMotorSets();
    for (std::size_t index = 0; index < sets.size(); index++)
    {
        if (&sets[index] == set)
        {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

/// The index of @p motor in @p set.
std::size_t curveIndex(const ThrustCurveMotorSet& set, const MotorPtr& motor)
{
    const auto& motors = set.getMotors();
    for (std::size_t index = 0; index < motors.size(); index++)
    {
        if (motors[index] == motor)
        {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

// ---- the table

/// The column index and order of each sort key, as the pins write them ("4A 2A 0A").
std::string sortKeysText(const MotorTableSorter& sorter)
{
    std::string text;
    for (const MotorTableSorter::SortKey& key : sorter.getSortKeys())
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += std::format("{}{}", static_cast<int>(key.column),
                            key.sortOrder == MotorTableSorter::SortOrder::ASCENDING ? 'A' : 'D');
    }
    return text;
}

/// The row of @p modelRow as the pins write it: the model row and the six column texts.
std::string rowText(const ThrustCurveMotorSelection& selection, std::size_t modelRow,
                    const Preferences& preferences)
{
    std::string text = std::to_string(modelRow);
    for (const ThrustCurveMotorColumns column : QtRocket::kAllThrustCurveMotorColumns)
    {
        text += '\t';
        text += selection.getModel().getValueText(modelRow, column, preferences);
    }
    return text;
}

/// The pin of the scenario @p name.
const MotorChooserRowScenarioPin* scenarioPin(std::string_view name)
{
    for (const MotorChooserRowScenarioPin& pin : Pins::kMotorChooserRowScenarioPins)
    {
        if (pin.name == name)
        {
            return &pin;
        }
    }
    return nullptr;
}

/// How the table of @p selection differs from OpenRocket's in the scenario @p name ("" when it
/// does not).
std::string tableDifferences(const ThrustCurveMotorSelection& selection,
                             const Preferences& preferences, std::string_view name)
{
    const MotorChooserRowScenarioPin* pin = scenarioPin(name);
    if (pin == nullptr)
    {
        return std::format("no pin for the scenario {}\n", name);
    }
    const std::vector<std::size_t> rows = selection.getViewRows();
    std::string                    all;
    std::string                    differences;
    for (std::size_t view = 0; view < rows.size(); view++)
    {
        const std::string row = rowText(selection, rows[view], preferences);
        all += row + '\n';
        if (view < pin->firstRowCount && row != Pins::kMotorChooserRowPins.at(pin->firstRow + view))
        {
            differences += std::format("{}: row {} is [{}], OpenRocket's [{}]\n", name, view, row,
                                       Pins::kMotorChooserRowPins.at(pin->firstRow + view));
        }
    }
    if (rows.size() != pin->rows)
    {
        differences += std::format("{}: {} rows, OpenRocket {}\n", name, rows.size(), pin->rows);
    }
    if (const std::string keys = sortKeysText(selection.getSorter()); keys != pin->sortKeys)
    {
        differences += std::format("{}: sort keys {}, OpenRocket {}\n", name, keys, pin->sortKeys);
    }
    const std::string md5 = QtRocket::toHex(QtRocket::md5(std::as_bytes(std::span(all))));
    if (md5 != pin->md5)
    {
        differences += std::format("{}: rows MD5 {}, OpenRocket {}\n", name, md5, pin->md5);
    }
    return differences;
}

/// How the filter's limits differ from OpenRocket's after the step @p name.
std::string filterDifferences(const ThrustCurveMotorSelection& selection, std::string_view name)
{
    for (const Pins::MotorChooserFilterPin& pin : Pins::kMotorChooserFilterPins)
    {
        if (pin.name != name)
        {
            continue;
        }
        const QtRocket::MotorRowFilter& filter = selection.getRowFilter();
        const std::optional<double>     minimumDiameter =
            pin.hasMinimumDiameter ? std::optional(pin.minimumDiameter) : std::nullopt;
        const std::optional<double> maximumDiameter =
            pin.hasMaximumDiameter ? std::optional(pin.maximumDiameter) : std::nullopt;
        if (filter.getMinimumDiameter() != minimumDiameter ||
            filter.getMaximumDiameter() != maximumDiameter ||
            filter.getMinimumLength() != pin.minimumLength ||
            filter.getMaximumLength() != pin.maximumLength)
        {
            return std::format("{}: the filter's limits differ from OpenRocket's\n", name);
        }
        return "";
    }
    return std::format("no filter pin for {}\n", name);
}

// ---- the designs

/// A rocket of one stage whose body tube is a motor mount, with configurations of their own.
class BodyTubeMount
{
public:
    BodyTubeMount()
    {
        AxialStage& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_mount           = &stage.addChild(std::make_unique<BodyTube>());
        m_mount->setMotorMount(true);
    }

    [[nodiscard]] MotorMount& mount() noexcept { return *m_mount; }

    /// A new flight configuration whose override gives the mount @p motor with @p delay and
    /// @p nozzle, as the probe's configure() does.
    FlightConfigurationId configure(MotorPtr motor, double delay, double nozzle)
    {
        const FlightConfigurationId fcid;
        m_rocket.createFlightConfiguration(fcid);
        MotorConfiguration configuration(*m_mount, fcid);
        configuration.setMotor(std::move(motor));
        configuration.setEjectionDelay(delay);
        EXPECT_TRUE(configuration.setNozzleExitDiameter(nozzle).has_value());
        m_mount->setMotorConfig(configuration, fcid);
        return fcid;
    }

private:
    Rocket    m_rocket;
    BodyTube* m_mount{nullptr};
};

/// The probe's custom motor: an Estes "<designation>" of 18 x 70 mm with the standard delays
/// @p delays (MotorChooserProbe's motor()).
MotorPtr customMotor(const std::string& designation, std::vector<double> delays)
{
    QtRocket::Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("Estes"))
            .setDesignation(designation)
            .setDiameter(0.018)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 5, 0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.02), Coordinate(0.035, 0, 0, 0.015),
                          Coordinate(0.035, 0, 0, 0.01)})
            .setStandardDelays(std::move(delays))
            .setDigest("digest" + designation)
            .setMotorType(Motor::Type::SINGLE)
            .build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// The curve of tests/data/motors/test1.eng, which no set of the database holds.
MotorPtr test1Motor()
{
    const auto bytes = QtRocket::readFile(QtRocket::Test::testDataDir() / "motors" / "test1.eng");
    if (!bytes)
    {
        ADD_FAILURE() << bytes.error().message;
        return nullptr;
    }
    const QtRocket::GeneralMotorLoader loader;
    auto                               builders = loader.load(*bytes, "test1.eng");
    if (!builders || builders->empty())
    {
        ADD_FAILURE() << "test1.eng holds no motor";
        return nullptr;
    }
    QtRocket::Result<ThrustCurveMotor> motor = builders->front().build();
    if (!motor)
    {
        ADD_FAILURE() << motor.error().message;
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

// ---- the selection panel

/// The curve list's items as the pins write them: the set indices of their curves, joined by ','.
std::string curveListText(ThrustCurveMotorSelection& selection)
{
    std::string text;
    for (const MotorHolder& holder : selection.getCurveItems())
    {
        if (!text.empty())
        {
            text += ',';
        }
        text += std::to_string(curveIndex(*selection.getSelectedMotorSet(), holder.getMotor()));
    }
    return text;
}

/// The index of the selected curve in the curve list.
std::size_t curveSelection(ThrustCurveMotorSelection& selection)
{
    const std::vector<MotorHolder> items = selection.getCurveItems();
    for (std::size_t index = 0; index < items.size(); index++)
    {
        if (items[index].getMotor() == selection.getSelectedMotor())
        {
            return index;
        }
    }
    return std::numeric_limits<std::size_t>::max();
}

/// The delay box's items joined by '|'.
std::string delayItemsText(const ThrustCurveMotorSelection& selection)
{
    std::string text;
    for (const std::string& item : selection.getDelayItems())
    {
        if (!text.empty())
        {
            text += '|';
        }
        text += item;
    }
    return text;
}

/// Whether two delays are the same (NaN equal to NaN, as Double.equals has it).
bool sameDelay(double a, double b)
{
    return a == b || (std::isnan(a) && std::isnan(b));
}

/// How the selection panel differs from OpenRocket's in the step @p label of the selections.
std::string selectionDifferences(ThrustCurveMotorSelection& selection, std::string_view label,
                                 std::size_t occurrence = 0)
{
    for (const MotorChooserSelectionPin& pin : Pins::kMotorChooserSelectionPins)
    {
        if (pin.label != label)
        {
            continue;
        }
        if (occurrence > 0)
        {
            occurrence--;
            continue;
        }
        const ThrustCurveMotorSet* set = selection.getSelectedMotorSet();
        if (set == nullptr || selection.getSelectedMotor() == nullptr)
        {
            return std::format("{}: nothing selected\n", label);
        }
        const bool same = setIndex(set) == pin.setIndex &&
                          curveIndex(*set, selection.getSelectedMotor()) == pin.curve &&
                          selection.getSelectedMotor()->getDigest() == pin.digest &&
                          sameDelay(selection.getSelectedDelay(), pin.delay) &&
                          (selection.getCurveItems().size() > 1) == pin.curvesEnabled &&
                          curveListText(selection) == pin.curves &&
                          curveSelection(selection) == pin.curveSelection &&
                          delayItemsText(selection) == pin.delayItems &&
                          selection.getDelayText() == pin.delayText &&
                          selection.getNozzleExitDiameter() == pin.nozzle;
        return same ? ""
                    : std::format(
                          "{}: set {}, curves [{}], delay {} [{}], OpenRocket set {}, curves "
                          "[{}], delay {} [{}]\n",
                          label, setIndex(set), curveListText(selection),
                          selection.getSelectedDelay(), selection.getDelayText(), pin.setIndex,
                          pin.curves, pin.delay, pin.delayText);
    }
    return std::format("no selection pin for {}\n", label);
}

/// Selects @p set through the table (its model row is its database index: the model is in database
/// order) and compares with the step @p label.
std::string selectThroughTable(ThrustCurveMotorSelection& selection, const ThrustCurveMotorSet& set,
                               std::string_view label, std::size_t occurrence = 0)
{
    const std::optional<std::size_t> row = selection.getModel().getIndex(set);
    if (!row.has_value())
    {
        return std::format("{}: the set is not in the model\n", label);
    }
    selection.selectMotorFromTable(*row);
    return selectionDifferences(selection, label, occurrence);
}

/// The open pin @p label.
const MotorChooserOpenPin* openPin(std::string_view label)
{
    for (const MotorChooserOpenPin& pin : Pins::kMotorChooserOpenPins)
    {
        if (pin.label == label)
        {
            return &pin;
        }
    }
    return nullptr;
}

/// How the panel after setMotorMountAndConfig() for a configuration with the delay @p configured
/// differs from OpenRocket's step @p label. QtRocket keeps a configured delay that is not NaN (M3's
/// bug 8); OpenRocket's delay and text are then expected only for a NaN one.
std::string openDifferences(ThrustCurveMotorSelection& selection, std::string_view label,
                            double configured)
{
    const MotorChooserOpenPin* pin = openPin(label);
    if (pin == nullptr || selection.getSelectedMotor() == nullptr)
    {
        return std::format("{}: no pin, or nothing selected\n", label);
    }
    const double      delay = std::isnan(configured) ? pin->delay : configured;
    const std::string text  = std::isnan(configured)
                                  ? std::string(pin->delayText)
                                  : ThrustCurveMotor::getDelayString(configured, "Plugged");
    const std::optional<std::size_t> setRow =
        selection.getModel().getIndex(*selection.getSelectedMotorSet());
    const bool same =
        selection.getSelectedMotor()->getDigest() == pin->digest &&
        sameDelay(selection.getSelectedDelay(), delay) && selection.getDelayText() == text &&
        selection.getNozzleExitDiameter() == pin->nozzle &&
        curveListText(selection) == pin->curves && delayItemsText(selection) == pin->delayItems &&
        selection.getModel().getRowCount() == pin->modelRows && setRow == pin->setRow;
    return same ? ""
                : std::format(
                      "{}: delay {} [{}], rows {}, OpenRocket delay {} [{}], rows {} (QtRocket "
                      "expects delay {} [{}])\n",
                      label, selection.getSelectedDelay(), selection.getDelayText(),
                      selection.getModel().getRowCount(), pin->delay, pin->delayText,
                      pin->modelRows, delay, text);
}

// ---- the groups of steps

/// The search texts of the probe and their scenarios.
constexpr std::array<std::pair<std::string_view, std::string_view>, 12> kSearches{{
    {"H12", "search [H12]"},
    {"aerotech h", "search [aerotech h]"},
    {"29", "search [29]"},
    {"38 mm", "search [38 mm]"},
    {"estes c6", "search [estes c6]"},
    {"Ns", "search [Ns]"},
    {"blue", "search [blue]"},
    {"0.1", "search [0.1]"},
    {"  ", "search [  ]"},
    {"j", "search [j]"},
    {"rms", "search [rms]"},
    {" \x01"
     "Estes\x02\tC6 ",
     "search [ <U+0001>Estes<U+0002><U+0009>C6 ]"},
}};

/// ChooserScenarios.rows(), up to the mount.
std::string filterAndSortDifferences(ThrustCurveMotorSelection& selection,
                                     const Preferences&         preferences)
{
    std::string differences = tableDifferences(selection, preferences, "default");
    selection.setHideUnavailable(false);
    differences += tableDifferences(selection, preferences, "hideUnavailableOff");
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::CASEINFO);
    differences += tableDifferences(selection, preferences, "sortedByCase");
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::MANUFACTURER);
    differences += tableDifferences(selection, preferences, "sortedByManufacturer");
    selection.getSorter().setSortKeys(MotorTableSorter::kInitialSortKeys);

    std::vector<const Manufacturer*> excluded;
    for (const ThrustCurveMotorSet& set : bundledMotorDatabase().getMotorSets())
    {
        if (std::ranges::find(excluded, set.getManufacturer()) == excluded.end() &&
            set.getManufacturer() != &Manufacturer::getManufacturer("Estes"))
        {
            excluded.push_back(set.getManufacturer());
        }
    }
    selection.getRowFilter().setExcludedManufacturers(excluded);
    differences += tableDifferences(selection, preferences, "onlyEstes");
    selection.getRowFilter().setExcludedManufacturers({});

    selection.getRowFilter().setMinimumImpulse(ImpulseClass::D);
    selection.getRowFilter().setMaximumImpulse(ImpulseClass::F);
    differences += tableDifferences(selection, preferences, "impulseDToF");
    selection.getRowFilter().setMinimumImpulse(ImpulseClass::A);
    selection.getRowFilter().setMaximumImpulse(ImpulseClass::O);
    differences += tableDifferences(selection, preferences, "impulseAToO");
    selection.getRowFilter().setMinimumImpulse(std::nullopt);
    selection.getRowFilter().setMaximumImpulse(std::nullopt);

    for (const auto& [text, name] : kSearches)
    {
        selection.getRowFilter().setSearchText(text);
        differences += tableDifferences(selection, preferences, name);
    }
    selection.getRowFilter().setSearchText("");
    return differences;
}

/// ChooserScenarios.rows() from the mount on: the filter panel's limit boxes, through
/// MotorFilterSliders as the panel moves its sliders.
std::string mountDifferences(ThrustCurveMotorSelection& selection, Preferences& preferences)
{
    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&   body  = stage.addChild(std::make_unique<BodyTube>());
    InnerTube&  mount = body.addChild(std::make_unique<InnerTube>());
    mount.setOuterRadius(0.0145);
    mount.setInnerRadius(0.0145 - 0.0005);
    mount.setLength(0.1);
    mount.setMotorMount(true);
    const FlightConfigurationId fcid;
    rocket.createFlightConfiguration(fcid);
    std::string differences;
    if (mount.getMotorMountDiameter() != Pins::kMotorChooserMountDiameter ||
        mount.getLength() != Pins::kMotorChooserMountLength)
    {
        differences += "the mount's dimensions differ from OpenRocket's\n";
    }

    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(mount, fcid));
    differences += tableDifferences(selection, preferences, "mountNoLimits");
    differences += filterDifferences(selection, "mountNoLimits");

    // "Limit motor diameter to mount diameter": the upper thumb to the largest nominal diameter
    // that fits; the slider's listener sets both diameters from the thumbs.
    MotorPreferences::setLimitDiameter(preferences, true);
    const int stop = MotorFilterSliders::limitDiameterStop(mount.getMotorMountDiameter());
    EXPECT_EQ(stop, 4);
    selection.getRowFilter().setMinimumDiameter(MotorFilterSliders::minimumDiameter(0));
    selection.getRowFilter().setMaximumDiameter(
        MotorFilterSliders::maximumDiameter(static_cast<std::size_t>(stop)));
    differences += tableDifferences(selection, preferences, "mountLimitDiameter");
    differences += filterDifferences(selection, "mountLimitDiameter");

    // "Limit motor length to mount length": the upper thumb to the mount's length in mm, the
    // maximum length to the mount's length (the lower thumb stays at 0).
    MotorPreferences::setLimitLength(preferences, true);
    EXPECT_EQ(MotorFilterSliders::limitLengthStop(mount.getLength()), 100);
    selection.getRowFilter().setMinimumLength(0);
    selection.getRowFilter().setMaximumLength(mount.getLength());
    differences += tableDifferences(selection, preferences, "mountLimitDiameterAndLength");
    differences += filterDifferences(selection, "mountLimitDiameterAndLength");

    // Turning both boxes off keeps the limits (the sliders stay where they are).
    MotorPreferences::setLimitLength(preferences, false);
    MotorPreferences::setLimitDiameter(preferences, false);
    differences += tableDifferences(selection, preferences, "mountLimitsOff");
    differences += filterDifferences(selection, "mountLimitsOff");
    return differences;
}

/// ChooserScenarios.rows(), the header clicks and the name column.
std::string headerClickDifferences(ThrustCurveMotorSelection& selection, Preferences& preferences)
{
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::NAME);
    std::string differences = tableDifferences(selection, preferences, "clickName");
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::NAME);
    differences += tableDifferences(selection, preferences, "clickNameAgain");
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::MANUFACTURER);
    differences += tableDifferences(selection, preferences, "clickManufacturer");
    selection.getSorter().toggleSortOrder(ThrustCurveMotorColumns::CASEINFO);
    differences += tableDifferences(selection, preferences, "clickCase");
    preferences.setMotorNameColumn(false);
    differences += tableDifferences(selection, preferences, "commonNameColumn");
    return differences;
}

/// ChooserScenarios.hideUsed(): the mount's motors, then another mount's. OpenRocket keeps the
/// first mount's motors too (M3's bug 9): QtRocket shows what OpenRocket shows for the second mount
/// alone.
std::string hideUsedDifferences()
{
    InMemoryPreferences         preferences;
    ThrustCurveMotorSelection   selection(bundledMotorDatabase(), preferences);
    BodyTubeMount               first;
    const FlightConfigurationId a = first.configure(findSet("Estes", "C6").getMotors()[1], 5, 0);
    first.configure(findSet("Estes", "D12").getMotors()[0], Motor::kPluggedDelay, 0);
    BodyTubeMount               second;
    const FlightConfigurationId c =
        second.configure(findSet("AeroTech", "H128W").getMotors()[0], 6, 0);

    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(first.mount(), a));
    selection.getRowFilter().setHideUsedMotors(true);
    std::string differences = tableDifferences(selection, preferences, "hideUsed");
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(second.mount(), c));
    differences += tableDifferences(selection, preferences, "hideUsedOnlySecondMount");
    if (tableDifferences(selection, preferences, "hideUsedSecondMount").empty())
    {
        differences += "the second mount hides the first mount's motors, as in OpenRocket\n";
    }

    InMemoryPreferences       otherPreferences;
    ThrustCurveMotorSelection other(bundledMotorDatabase(), otherPreferences);
    other.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(second.mount(), c));
    other.getRowFilter().setHideUsedMotors(true);
    differences += tableDifferences(other, otherPreferences, "hideUsedOnlySecondMount");
    return differences;
}

/// ChooserScenarios.customMotor(): a motor in no set of the database.
std::string customMotorDifferences()
{
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    const MotorPtr            motor = test1Motor();
    if (motor == nullptr)
    {
        return "no custom motor\n";
    }
    BodyTubeMount               mount;
    const FlightConfigurationId fcid = mount.configure(motor, 3.3, 0);
    selection.setMotorMountAndConfig(QtRocket::MotorChooserInput::fromMount(mount.mount(), fcid));
    return openDifferences(selection, "customMotor", 3.3) +
           tableDifferences(selection, preferences, "customMotor");
}

/// ChooserScenarios.selections(), the sets selected through the table one after the other.
std::string tableSelectionDifferences(ThrustCurveMotorSelection& selection)
{
    std::string differences;
    differences +=
        selectThroughTable(selection, findSet("AeroTech", "D24T"), "table AeroTech D24T");
    differences +=
        selectThroughTable(selection, findSet("AeroTech", "E15W"), "table AeroTech E15W");
    differences += selectThroughTable(selection, findSet("Estes", "C6"), "table Estes C6");
    differences +=
        selectThroughTable(selection, findSet("AeroTech", "H128W"), "table AeroTech H128W");
    differences += selectThroughTable(selection, findSet("Cesaroni Technology Inc.", "I205"),
                                      "table Cesaroni Technology Inc. I205");
    differences += selectThroughTable(selection, findSet("Estes", "C6"), "table Estes C6", 1);
    return differences;
}

/// ChooserScenarios.selections(), the typed delays on Estes C6, each after the one before. A typed
/// NaN keeps the delay in QtRocket (M3's bug 11): OpenRocket takes it.
std::string typedDelayDifferences(ThrustCurveMotorSelection& selection)
{
    std::string differences;
    double      previous = selection.getSelectedDelay();
    for (const Pins::MotorChooserDelayPin& pin : Pins::kMotorChooserDelayPins)
    {
        selection.setDelayText(pin.typed);
        const double      expected = std::isnan(pin.delay) ? previous : pin.delay;
        const std::string text     = std::isnan(pin.delay)
                                         ? ThrustCurveMotor::getDelayString(previous, "Plugged")
                                         : std::string(pin.text);
        if (!sameDelay(selection.getSelectedDelay(), expected) || selection.getDelayText() != text)
        {
            differences +=
                std::format("[{}]: delay {} [{}], expected {} [{}] (OpenRocket {} [{}])\n",
                            pin.typed, selection.getSelectedDelay(), selection.getDelayText(),
                            expected, text, pin.delay, pin.text);
        }
        previous = selection.getSelectedDelay();
    }
    return differences;
}

/// ChooserScenarios.selections(), the delay carried to another set.
std::string delayCarryDifferences(ThrustCurveMotorSelection& selection)
{
    selection.setDelayText("4");
    std::string differences =
        selectThroughTable(selection, findSet("Estes", "D12"), "delay 4, then Estes D12");
    selection.setDelayText("Plugged");
    differences += selectThroughTable(selection, findSet("Estes", "C6"), "plugged, then Estes C6");
    differences +=
        selectThroughTable(selection, findSet("AeroTech", "H128W"), "then AeroTech H128W");
    selection.setDelayText("9");
    differences +=
        selectThroughTable(selection, findSet("AeroTech", "H128W"), "delay 9, the same set");
    selection.setDelayText("1e10");
    differences +=
        selectThroughTable(selection, findSet("Estes", "C6"), "delay 1e10, then Estes C6");
    return differences;
}

/// ChooserScenarios.selections(), the curve list of AeroTech E15W.
std::string curveListDifferences(ThrustCurveMotorSelection& selection)
{
    std::string differences =
        selectThroughTable(selection, findSet("AeroTech", "E15W"), "table AeroTech E15W again");
    selection.select(selection.getCurveItems().at(1).getMotor());
    differences += selectionDifferences(selection, "E15W curve item 1");
    selection.setHideSimilar(false);
    differences += selectionDifferences(selection, "E15W hide similar off");
    selection.select(selection.getCurveItems().at(1).getMotor());
    differences += selectionDifferences(selection, "E15W hide similar off, curve item 1");
    selection.setHideSimilar(true);
    differences += selectionDifferences(selection, "E15W hide similar on");
    selection.select(selection.getCurveItems().at(0).getMotor());
    differences += selectionDifferences(selection, "E15W curve item 0");
    return differences;
}

/// A step of ChooserScenarios.opens().
struct OpenStep
{
    std::string_view label;
    /// The motor: a set of the database and a curve index, or a custom motor's designation and
    /// standard delays (an empty set name), or the first custom motor again (custom "again").
    std::string_view manufacturer;
    std::string_view designation;
    std::size_t      curve;
    double           delay;
    double           nozzle;
    bool             fresh;
};

/// The probe's opens().
constexpr std::array<OpenStep, 11> kOpenSteps{{
    {.label        = "C6 curve 0, delay 6, nozzle 0.01",
     .manufacturer = "Estes",
     .designation  = "C6",
     .curve        = 0,
     .delay        = 6,
     .nozzle       = 0.01,
     .fresh        = true},
    {.label        = "C6 curve 0, delay 5.04, nozzle 0.001",
     .manufacturer = "Estes",
     .designation  = "C6",
     .curve        = 0,
     .delay        = 5.04,
     .nozzle       = 0.001,
     .fresh        = true},
    {.label        = "C6 curve 1, delay NaN",
     .manufacturer = "Estes",
     .designation  = "C6",
     .curve        = 1,
     .delay        = std::numeric_limits<double>::quiet_NaN(),
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "C6 curve 0, plugged",
     .manufacturer = "Estes",
     .designation  = "C6",
     .curve        = 0,
     .delay        = Motor::kPluggedDelay,
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "H128W curve 1, plugged",
     .manufacturer = "AeroTech",
     .designation  = "H128W",
     .curve        = 1,
     .delay        = Motor::kPluggedDelay,
     .nozzle       = 0.0005,
     .fresh        = true},
    {.label        = "C6 curve 0, delay 5",
     .manufacturer = "Estes",
     .designation  = "C6",
     .curve        = 0,
     .delay        = 5,
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "custom delays 2.55 and 4.25, delay 3.3",
     .manufacturer = "",
     .designation  = "Z1",
     .curve        = 0,
     .delay        = 3.3,
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "custom without delays, delay 5",
     .manufacturer = "",
     .designation  = "Z2",
     .curve        = 0,
     .delay        = 5,
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "custom delay 7, delay 3",
     .manufacturer = "",
     .designation  = "Z3",
     .curve        = 0,
     .delay        = 3,
     .nozzle       = 0,
     .fresh        = true},
    {.label        = "another custom, delay 3",
     .manufacturer = "",
     .designation  = "Z4",
     .curve        = 0,
     .delay        = 3,
     .nozzle       = 0,
     .fresh        = false},
    {.label        = "the first custom again, delay 7",
     .manufacturer = "",
     .designation  = "again",
     .curve        = 0,
     .delay        = 7,
     .nozzle       = 0,
     .fresh        = false},
}};

/// The motor of @p step; @p firstCustom is the Z3 custom motor made earlier.
MotorPtr openMotor(const OpenStep& step, const MotorPtr& firstCustom)
{
    if (!step.manufacturer.empty())
    {
        return findSet(step.manufacturer, step.designation).getMotors().at(step.curve);
    }
    if (step.designation == "Z1")
    {
        return customMotor("Z1", {2.55, 4.25});
    }
    if (step.designation == "Z2")
    {
        return customMotor("Z2", {});
    }
    if (step.designation == "again")
    {
        return firstCustom;
    }
    return customMotor(std::string(step.designation), {7});
}

/// ChooserScenarios.opens().
std::string openStepDifferences()
{
    std::string                                 differences;
    std::unique_ptr<InMemoryPreferences>        preferences;
    std::unique_ptr<ThrustCurveMotorSelection>  selection;
    std::vector<std::unique_ptr<BodyTubeMount>> mounts;
    MotorPtr                                    firstCustom;
    for (const OpenStep& step : kOpenSteps)
    {
        if (step.fresh)
        {
            selection.reset();
            preferences = std::make_unique<InMemoryPreferences>();
            selection =
                std::make_unique<ThrustCurveMotorSelection>(bundledMotorDatabase(), *preferences);
        }
        const MotorPtr motor = openMotor(step, firstCustom);
        if (step.designation == "Z3")
        {
            firstCustom = motor;
        }
        mounts.push_back(std::make_unique<BodyTubeMount>());
        const FlightConfigurationId fcid = mounts.back()->configure(motor, step.delay, step.nozzle);
        selection->setMotorMountAndConfig(
            QtRocket::MotorChooserInput::fromMount(mounts.back()->mount(), fcid));
        differences += openDifferences(*selection, step.label, step.delay);
    }
    return differences;
}

/// ChooserScenarios.preferred().
std::string preferredDifferences()
{
    InMemoryPreferences preferences;
    std::string         differences;
    {
        ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
        selection.storeSelectedMotor(*findSet("Estes", "C6").getMotors()[1]);
    }
    const Preferences* node =
        preferences.findNode(QtRocket::PreferenceKeys::kPreferredThrustCurveMotorNode);
    std::vector<std::string> stored;
    if (node != nullptr)
    {
        for (const std::string& key : node->keys())
        {
            stored.push_back(key + "=" + node->get(key).value_or(""));
        }
    }
    std::vector<std::string> pinned;
    pinned.reserve(Pins::kMotorChooserPreferredPins.size());
    for (const Pins::MotorChooserPreferredPin& pin : Pins::kMotorChooserPreferredPins)
    {
        pinned.push_back(std::string(pin.digest) + "=" + std::string(pin.value));
    }
    if (stored != pinned)
    {
        differences += "the preferred curves stored differ from OpenRocket's\n";
    }
    ThrustCurveMotorSelection later(bundledMotorDatabase(), preferences);
    differences += selectThroughTable(later, findSet("Estes", "C6"), "preferred C6 curve 1");
    differences += selectThroughTable(later, findSet("Estes", "D12"), "then Estes D12");
    return differences;
}

/// The multi-curve pins of shard @p shard of @p pins.
template <std::size_t N>
std::vector<MotorChooserCurvePin> shardOf(const std::array<MotorChooserCurvePin, N>& pins,
                                          std::size_t shard, std::size_t shards)
{
    std::vector<MotorChooserCurvePin> part;
    for (std::size_t index = shard; index < pins.size(); index += shards)
    {
        part.push_back(pins.at(index));
    }
    return part;
}

/// How the curve choice of the sets of @p pins differs from OpenRocket's, the sets selected
/// through the table of @p selection one after the other.
std::string curveDifferences(ThrustCurveMotorSelection&            selection,
                             std::span<const MotorChooserCurvePin> pins)
{
    std::string differences;
    for (const MotorChooserCurvePin& pin : pins)
    {
        const auto& set = bundledMotorDatabase().getMotorSets().at(pin.setIndex);
        selection.selectMotorFromTable(pin.setIndex);
        const std::size_t selected = curveIndex(set, selection.getSelectedMotor());
        if (selection.getSelectedMotorSet() != &set || selected != pin.selectedCurve ||
            curveListText(selection) != pin.curves)
        {
            differences += std::format(
                "set {}: curve {}, list [{}]; OpenRocket curve {}, list [{}]\n", pin.setIndex,
                selected, curveListText(selection), pin.selectedCurve, pin.curves);
        }
    }
    return differences;
}

/// The number of shards the curve choice runs in (each similarity is a few ms in a debug build,
/// several times that under the sanitizers).
constexpr std::size_t kCurveShards = 8;

/// ChooserScenarios.curves() for the shard @p shard: with "Hide very similar thrust curves" on,
/// then off.
std::string curveShardDifferences(std::size_t shard)
{
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    std::string               differences =
        curveDifferences(selection, shardOf(Pins::kMotorChooserCurvePins, shard, kCurveShards));
    selection.setHideSimilar(false);
    differences +=
        curveDifferences(selection, shardOf(Pins::kMotorChooserAllCurvePins, shard, kCurveShards));
    return differences;
}

/// MotorClass of every pinned impulse, with the default units.
std::string motorClassDifferences()
{
    std::string differences;
    for (const Pins::MotorChooserClassPin& pin : Pins::kMotorChooserClassPins)
    {
        const QtRocket::MotorClass motorClass = QtRocket::getMotorClass(pin.impulse);
        if (QtRocket::enumName(motorClass) != pin.name ||
            QtRocket::getDescription(motorClass, pin.impulse) != pin.description ||
            QtRocket::getClassDescription(motorClass) != pin.classDescription)
        {
            differences += std::format("{}: {} [{}] [{}], OpenRocket {} [{}] [{}]\n", pin.impulse,
                                       QtRocket::enumName(motorClass),
                                       QtRocket::getDescription(motorClass, pin.impulse),
                                       QtRocket::getClassDescription(motorClass), pin.name,
                                       pin.description, pin.classDescription);
        }
    }
    return differences;
}

/// The filter panel's manufacturers, against the pins.
std::string manufacturerDifferences()
{
    InMemoryPreferences             preferences;
    const ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    const auto&                     manufacturers = selection.getManufacturers();
    if (manufacturers.size() != Pins::kMotorChooserManufacturerPins.size())
    {
        return std::format("{} manufacturers, OpenRocket {}\n", manufacturers.size(),
                           Pins::kMotorChooserManufacturerPins.size());
    }
    std::string differences;
    for (std::size_t index = 0; index < manufacturers.size(); index++)
    {
        const Pins::MotorChooserManufacturerPin& pin =
            Pins::kMotorChooserManufacturerPins.at(index);
        std::size_t sets = 0;
        for (const ThrustCurveMotorSet& set : bundledMotorDatabase().getMotorSets())
        {
            sets += set.getManufacturer() == manufacturers[index] ? 1U : 0U;
        }
        if (manufacturers[index]->getSimpleName() != pin.simpleName ||
            manufacturers[index]->getDisplayName() != pin.displayName || sets != pin.sets)
        {
            differences += std::format("manufacturer {}: {}, OpenRocket {}\n", index,
                                       manufacturers[index]->getSimpleName(), pin.simpleName);
        }
    }
    return differences;
}

/// The labels MotorFilterSliders gives, joined by spaces, and the pinned @p labels.
std::pair<std::string, std::string> labelTexts(std::span<const std::string_view> labels)
{
    std::string ours;
    for (const std::string& label : MotorFilterSliders::diameterLabels())
    {
        ours += label + ' ';
    }
    std::string pinned;
    for (const std::string_view label : labels)
    {
        pinned += std::string(label) + ' ';
    }
    return {ours, pinned};
}

// ---- the tests

/// The pins come from the OpenRocket of the goldens, over the bundled database.
TEST(MotorChooserPinsGolden, ComeFromTheOpenRocketOfTheGoldens)
{
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(Pins::kMotorChooserPinsCommit, manifest->openrocketCommit);
    EXPECT_EQ(bundledMotorDatabase().getMotorSets().size(), Pins::kMotorChooserPinsMotorSets);
    EXPECT_EQ(Pins::kMotorChooserCurvePins.size(), 107U);
}

/// The filter panel lists the database's manufacturers in OpenRocket's order.
TEST(MotorChooserPinsGolden, ListsTheManufacturersInOpenRocketsOrder)
{
    EXPECT_EQ(manufacturerDifferences(), "");
}

/// The diameter slider's labels in mm and in inches.
TEST(MotorChooserPinsGolden, LabelsTheDiameterSliderAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    const auto [millimetres, pinnedMillimetres] =
        labelTexts(Pins::kMotorChooserDiameterLabelsMillimetres);
    EXPECT_EQ(millimetres, pinnedMillimetres);
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    const auto [inches, pinnedInches] = labelTexts(Pins::kMotorChooserDiameterLabelsInches);
    EXPECT_EQ(inches, pinnedInches);
}

/// The table under the filters, the searches and the sort keys: the rows and their order equal
/// OpenRocket's, the tertiary collator's Case order included (the scouting's one difference, with
/// the primary collator).
TEST(MotorChooserPinsGolden, FiltersAndSortsTheTableAsOpenRocket)
{
    const DefaultUnitsGuard   guard;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    EXPECT_EQ(filterAndSortDifferences(selection, preferences), "");
    EXPECT_EQ(mountDifferences(selection, preferences), "");
    EXPECT_EQ(headerClickDifferences(selection, preferences), "");
}

/// A motor in no set of the database gets a set of its own, and the model's list is sorted.
TEST(MotorChooserPinsGolden, AddsAMotorOfNoSetAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(customMotorDifferences(), "");
}

/// "Hide motors already used in the mount" hides the mount's motors; for a second mount, QtRocket
/// hides that mount's only (M3's bug 9).
TEST(MotorChooserPinsGolden, HidesTheMotorsOfTheMountOnly)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(hideUsedDifferences(), "");
}

/// The default curve, the curve list and the delays of sets selected through the table, the
/// delays typed into the delay box, the delay carried to another set, and the curve list's
/// choices.
TEST(MotorChooserPinsGolden, SelectsCurvesAndDelaysAsOpenRocket)
{
    const DefaultUnitsGuard   guard;
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    EXPECT_EQ(tableSelectionDifferences(selection), "");
    EXPECT_EQ(typedDelayDifferences(selection), "");
    EXPECT_EQ(delayCarryDifferences(selection), "");
    EXPECT_EQ(curveListDifferences(selection), "");
}

/// Opening the chooser for a configuration selects its motor, nozzle and (unlike OpenRocket, M3's
/// bug 8) its delay.
TEST(MotorChooserPinsGolden, OpensForAConfigurationAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(openStepDifferences(), "");
}

/// OK stores the chosen curve of the set, which a later chooser selects.
TEST(MotorChooserPinsGolden, RemembersThePreferredCurveAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(preferredDifferences(), "");
}

/// The table in inches: the dimension texts and the search in them.
TEST(MotorChooserPinsGolden, ShowsInchesAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::MOTOR_DIMENSIONS).setDefaultUnit("in"));
    InMemoryPreferences       preferences;
    ThrustCurveMotorSelection selection(bundledMotorDatabase(), preferences);
    EXPECT_EQ(tableDifferences(selection, preferences, "inchNoFilter"), "");
    selection.getRowFilter().setSearchText("1.14");
    EXPECT_EQ(tableDifferences(selection, preferences, "inchSearch [1.14]"), "");
}

/// MotorClass's classes and texts.
TEST(MotorChooserPinsGolden, ClassifiesImpulsesAsOpenRocket)
{
    const DefaultUnitsGuard guard;
    EXPECT_EQ(motorClassDifferences(), "");
}

class MotorChooserCurvesGolden : public ::testing::TestWithParam<std::size_t>
{ };

/// Every set of more than one curve of the shard, selected through the table: the default curve
/// and the curve list (MotorCorrelation's "Hide very similar thrust curves", ThrustCurveMotor
/// Comparator's order) equal OpenRocket's, with the box on and off.
TEST_P(MotorChooserCurvesGolden, ChoosesOpenRocketsCurves)
{
    EXPECT_EQ(curveShardDifferences(GetParam()), "");
}

INSTANTIATE_TEST_SUITE_P(Shards, MotorChooserCurvesGolden,
                         ::testing::Range<std::size_t>(0, kCurveShards));

}  // namespace
