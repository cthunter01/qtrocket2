#include "QtRocket/plot/SimulationPlotConfiguration.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/LineStyle.h"
#include "plot/PlotBranchSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The presets are pinned to what OpenRocket's DEFAULT_CONFIGURATIONS hold (the "PRESET" lines of
// tools/openrocket-probes/plot-configuration's expected.txt), the copies to its "SHARE" lines.

namespace
{

using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::LineStyle;
using QtRocket::SimulationPlotConfiguration;
using QtRocket::UnitGroupId;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::plotFlight;
using QtRocket::Test::plotSeries;
using Id   = QtRocket::FlightDataTypeId;
using Type = QtRocket::FlightEvent::Type;

[[nodiscard]] const FlightDataType& builtin(Id id)
{
    return FlightDataType::builtin(id);
}

/// A row of a preset as the probe printed it: the type, its unit's name and its axis (no colour
/// and a solid line in every preset).
struct RowPin
{
    RowPin(Id rowType, std::string_view rowUnit, int rowAxis)
      : type(rowType), unit(rowUnit), axis(rowAxis)
    {
    }

    Id               type;
    std::string_view unit;
    int              axis;
};

/// A preset as the probe printed it.
struct PresetPin
{
    PresetPin(std::string_view presetName, std::string_view presetKey, Id domainType,
              std::string_view domainTypeUnit, std::vector<RowPin> presetRows,
              std::vector<Type> presetEvents)
      : name(presetName),
        key(presetKey),
        domain(domainType),
        domainUnit(domainTypeUnit),
        rows(std::move(presetRows)),
        events(std::move(presetEvents))
    {
    }

    std::string_view    name;
    std::string_view    key;
    Id                  domain;
    std::string_view    domainUnit;
    std::vector<RowPin> rows;
    std::vector<Type>   events;
};

/// The events of every preset but the ground track, the drag coefficients and the roll
/// characteristics, in declaration order.
[[nodiscard]] std::vector<Type> usualEvents()
{
    return {Type::IGNITION,
            Type::BURNOUT,
            Type::STAGE_SEPARATION,
            Type::APOGEE,
            Type::RECOVERY_DEVICE_DEPLOYMENT,
            Type::GROUND_HIT,
            Type::TUMBLE,
            Type::SIM_WARN,
            Type::SIM_ABORT,
            Type::EXCEPTION};
}

constexpr std::string_view kNoUnit          = "\xE2\x80\x8B";  // Unit.NOUNIT, a zero-width space
constexpr std::string_view kAcceleration    = "m/s\xC2\xB2";
constexpr std::string_view kDegree          = "\xC2\xB0";
constexpr std::string_view kDegreePerSecond = "\xC2\xB0/s";

[[nodiscard]] std::vector<PresetPin> presetPins()
{
    return {
        {"Vertical motion vs. time",
         "PlotConfiguration.Verticalmotion",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_ALTITUDE, "m", 0},
          {Id::TYPE_VELOCITY_Z, "m/s", -1},
          {Id::TYPE_ACCELERATION_Z, kAcceleration, -1}},
         usualEvents()},
        {"Total motion vs. time",
         "PlotConfiguration.Totalmotion",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_ALTITUDE, "m", 0},
          {Id::TYPE_VELOCITY_TOTAL, "m/s", -1},
          {Id::TYPE_ACCELERATION_TOTAL, kAcceleration, -1}},
         usualEvents()},
        {"Flight side profile",
         "PlotConfiguration.Flightside",
         Id::TYPE_POSITION_X,
         "m",
         {{Id::TYPE_ALTITUDE, "m", -1}},
         usualEvents()},
        {"Ground track",
         "PlotConfiguration.Groundtrack",
         Id::TYPE_POSITION_X,
         "m",
         {{Id::TYPE_POSITION_Y, "m", 0}, {Id::TYPE_ALTITUDE, "m", 1}},
         {Type::IGNITION, Type::BURNOUT, Type::APOGEE, Type::RECOVERY_DEVICE_DEPLOYMENT,
          Type::GROUND_HIT, Type::SIM_WARN, Type::SIM_ABORT, Type::EXCEPTION}},
        {"Stability vs. time",
         "PlotConfiguration.Stability",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_STABILITY, kNoUnit, 0},
          {Id::TYPE_CP_LOCATION, "cm", 1},
          {Id::TYPE_CG_LOCATION, "cm", 1}},
         usualEvents()},
        {"Drag coefficients vs. Mach number",
         "PlotConfiguration.Dragcoef",
         Id::TYPE_MACH_NUMBER,
         kNoUnit,
         {{Id::TYPE_DRAG_COEFF, kNoUnit, 0},
          {Id::TYPE_FRICTION_DRAG_COEFF, kNoUnit, 0},
          {Id::TYPE_BASE_DRAG_COEFF, kNoUnit, 0},
          {Id::TYPE_PRESSURE_DRAG_COEFF, kNoUnit, 0}},
         {Type::SIM_WARN, Type::SIM_ABORT, Type::EXCEPTION}},
        {"Roll characteristics",
         "PlotConfiguration.Rollcharacteristics",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_ROLL_RATE, kDegreePerSecond, 0},
          {Id::TYPE_ROLL_MOMENT_COEFF, kNoUnit, 1},
          {Id::TYPE_ROLL_FORCING_COEFF, kNoUnit, 1},
          {Id::TYPE_ROLL_DAMPING_COEFF, kNoUnit, 1}},
         {Type::IGNITION, Type::LAUNCHROD, Type::BURNOUT, Type::STAGE_SEPARATION, Type::APOGEE,
          Type::RECOVERY_DEVICE_DEPLOYMENT, Type::GROUND_HIT, Type::TUMBLE, Type::SIM_WARN,
          Type::SIM_ABORT, Type::EXCEPTION}},
        {"Angle of attack and orientation vs. time",
         "PlotConfiguration.Angleofattack",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_AOA, kDegree, 0},
          {Id::TYPE_ORIENTATION_PHI, kDegree, -1},
          {Id::TYPE_ORIENTATION_THETA, kDegree, -1}},
         usualEvents()},
        {"Simulation time step and computation time",
         "PlotConfiguration.Simulationtime",
         Id::TYPE_TIME,
         "s",
         {{Id::TYPE_TIME_STEP, "s", -1}, {Id::TYPE_COMPUTATION_TIME, "s", -1}},
         usualEvents()},
    };
}

/// Whether @p row of @p preset is @p pin.
[[nodiscard]] ::testing::AssertionResult rowAsPinned(const SimulationPlotConfiguration& preset,
                                                     std::size_t row, const RowPin& pin)
{
    if (&preset.getType(row) != &builtin(pin.type))
    {
        return ::testing::AssertionFailure()
               << "row " << row << ": " << preset.getType(row).getName();
    }
    if (preset.getUnit(row).getUnit() != pin.unit || preset.getAxis(row) != pin.axis)
    {
        return ::testing::AssertionFailure()
               << "row " << row << ": unit '" << preset.getUnit(row).getUnit() << "' axis "
               << preset.getAxis(row);
    }
    if (preset.getPlotDataColor(row).has_value() ||
        preset.getPlotDataLineStyle(row) != LineStyle::SOLID)
    {
        return ::testing::AssertionFailure() << "row " << row << " has an appearance";
    }
    return ::testing::AssertionSuccess();
}

/// Whether @p preset is @p pin.
[[nodiscard]] ::testing::AssertionResult presetAsPinned(const SimulationPlotConfiguration& preset,
                                                        const PresetPin&                   pin)
{
    if (preset.getName() != pin.name || preset.getNameKey() != pin.key)
    {
        return ::testing::AssertionFailure()
               << "name '" << preset.getName() << "' key '" << preset.getNameKey() << "'";
    }
    if (&preset.getDomainAxisType() != &builtin(pin.domain) ||
        preset.getDomainAxisUnit().getUnit() != pin.domainUnit)
    {
        return ::testing::AssertionFailure()
               << pin.name << ": domain " << preset.getDomainAxisType().getName() << " '"
               << preset.getDomainAxisUnit().getUnit() << "'";
    }
    if (preset.getDataCount() != pin.rows.size())
    {
        return ::testing::AssertionFailure()
               << pin.name << ": " << preset.getDataCount() << " rows";
    }
    for (std::size_t i = 0; i < pin.rows.size(); ++i)
    {
        const ::testing::AssertionResult row = rowAsPinned(preset, i, pin.rows[i]);
        if (!row)
        {
            return ::testing::AssertionFailure() << pin.name << ": " << row.message();
        }
    }
    if (preset.getActiveEvents() != pin.events)
    {
        return ::testing::AssertionFailure() << pin.name << ": other events";
    }
    return ::testing::AssertionSuccess();
}

TEST(SimulationPlotConfiguration, TheNinePresetsAreOpenRocketsInItsOrder)
{
    const DefaultUnitsGuard                        units;
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    const std::vector<PresetPin> pins = presetPins();
    ASSERT_EQ(presets.size(), 9U);
    ASSERT_EQ(pins.size(), 9U);
    for (std::size_t i = 0; i < pins.size(); ++i)
    {
        EXPECT_TRUE(presetAsPinned(presets[i], pins[i])) << i;
    }
}

TEST(SimulationPlotConfiguration, ThePresetsComeInTheDefaultUnitsOfTheMoment)
{
    // OpenRocket's presets are made once, in the units of the start; its plot panel resets the
    // units of the one chosen (clone().resetUnits()).
    const DefaultUnitsGuard units;
    ASSERT_TRUE(QtRocket::unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    ASSERT_EQ(presets.size(), 9U);
    EXPECT_EQ(presets[0].getUnit(0).getUnit(), "ft");
    EXPECT_EQ(presets[2].getDomainAxisUnit().getUnit(), "ft");
    EXPECT_EQ(presets[3].getUnit(1).getUnit(), "ft");
}

TEST(SimulationPlotConfiguration, EveryCallMakesNewPresets)
{
    std::vector<SimulationPlotConfiguration> first =
        SimulationPlotConfiguration::defaultConfigurations();
    ASSERT_FALSE(first.empty());
    first[0].removePlotDataType(0);
    first[0].setEvent(Type::LAUNCH, true);
    first[0].setName("changed");

    const std::vector<SimulationPlotConfiguration> second =
        SimulationPlotConfiguration::defaultConfigurations();
    ASSERT_FALSE(second.empty());
    EXPECT_EQ(second[0].getDataCount(), 3U);
    EXPECT_FALSE(second[0].isEventActive(Type::LAUNCH));
    EXPECT_EQ(second[0].getName(), "Vertical motion vs. time");
}

TEST(SimulationPlotConfiguration, TheCustomConfigurationIsEmpty)
{
    const DefaultUnitsGuard           units;
    const SimulationPlotConfiguration custom = SimulationPlotConfiguration::customConfiguration();
    EXPECT_EQ(custom.getName(), "Custom");
    EXPECT_EQ(custom.getNameKey(), "simplotpanel.CUSTOM");
    EXPECT_EQ(custom.getName(), SimulationPlotConfiguration::kCustomName);
    EXPECT_EQ(custom.getNameKey(), SimulationPlotConfiguration::kCustomNameKey);
    EXPECT_EQ(&custom.getDomainAxisType(), &builtin(Id::TYPE_TIME));
    EXPECT_EQ(custom.getDomainAxisUnit().getUnit(), "s");
    EXPECT_EQ(custom.getDataCount(), 0U);
    EXPECT_TRUE(custom.getActiveEvents().empty());
}

TEST(SimulationPlotConfiguration, ANewConfigurationIsOverTimeWithoutEvents)
{
    const SimulationPlotConfiguration unnamed;
    EXPECT_EQ(unnamed.getName(), "");
    EXPECT_EQ(&unnamed.getDomainAxisType(), &builtin(Id::TYPE_TIME));
    EXPECT_TRUE(unnamed.getActiveEvents().empty());

    const SimulationPlotConfiguration named("named");
    EXPECT_EQ(named.getName(), "named");
    EXPECT_EQ(named.getNameKey(), "");
    EXPECT_EQ(&named.getDomainAxisType(), &builtin(Id::TYPE_TIME));

    const SimulationPlotConfiguration overMach("over Mach", builtin(Id::TYPE_MACH_NUMBER));
    EXPECT_EQ(&overMach.getDomainAxisType(), &builtin(Id::TYPE_MACH_NUMBER));

    const SimulationPlotConfiguration withKey("Key", "A.key", builtin(Id::TYPE_POSITION_X));
    EXPECT_EQ(withKey.getNameKey(), "A.key");
    EXPECT_EQ(&withKey.getDomainAxisType(), &builtin(Id::TYPE_POSITION_X));
}

TEST(SimulationPlotConfiguration, ListsItsEventsInDeclarationOrder)
{
    SimulationPlotConfiguration config("events");
    config.setEvent(Type::EXCEPTION, true);
    config.setEvent(Type::LAUNCH, true);
    config.setEvent(Type::APOGEE, true);
    config.setEvent(Type::APOGEE, true);
    EXPECT_TRUE(config.isEventActive(Type::APOGEE));
    EXPECT_FALSE(config.isEventActive(Type::ALTITUDE));
    EXPECT_EQ(config.getActiveEvents(), (std::vector{Type::LAUNCH, Type::APOGEE, Type::EXCEPTION}));

    config.setEvent(Type::LAUNCH, false);
    config.setEvent(Type::GROUND_HIT, false);
    EXPECT_FALSE(config.isEventActive(Type::LAUNCH));
    EXPECT_EQ(config.getActiveEvents(), (std::vector{Type::APOGEE, Type::EXCEPTION}));
}

TEST(SimulationPlotConfiguration, EveryCopyHasEventsOfItsOwn)
{
    // "SHARE clone(): source has LAUNCH=false", while Java's cloneConfiguration() and so
    // fillAutoAxes() share the events with the source ("SHARE cloneConfiguration(): source has
    // APOGEE=true", "SHARE fillAutoAxes(): source has GROUND_HIT=true"); here every copy has its
    // own.
    SimulationPlotConfiguration source("source");
    source.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 0);
    source.setEvent(Type::IGNITION, true);

    SimulationPlotConfiguration copy = source;
    copy.setEvent(Type::LAUNCH, true);
    EXPECT_FALSE(source.isEventActive(Type::LAUNCH));
    EXPECT_TRUE(copy.isEventActive(Type::IGNITION));

    const std::unique_ptr<SimulationPlotConfiguration::Base> clone = source.cloneConfiguration();
    auto* cloned = dynamic_cast<SimulationPlotConfiguration*>(clone.get());
    ASSERT_NE(cloned, nullptr);
    EXPECT_TRUE(cloned->isEventActive(Type::IGNITION));
    cloned->setEvent(Type::APOGEE, true);
    EXPECT_FALSE(source.isEventActive(Type::APOGEE));

    const FlightDataBranch data = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {1, 2});
    const std::unique_ptr<SimulationPlotConfiguration::Base> filled = source.fillAutoAxes(data);
    auto* filledSimulation = dynamic_cast<SimulationPlotConfiguration*>(filled.get());
    ASSERT_NE(filledSimulation, nullptr);
    EXPECT_TRUE(filledSimulation->isEventActive(Type::IGNITION));
    filledSimulation->setEvent(Type::GROUND_HIT, true);
    EXPECT_FALSE(source.isEventActive(Type::GROUND_HIT));
}

TEST(SimulationPlotConfiguration, TheFirstPresetPutsTheAccelerationOnTheRight)
{
    // The rows of "AUTO vertical-motion": the first preset chooses the axes that
    // PlotConfiguration's tests pin for them, and the result is a SimulationPlotConfiguration with
    // the preset's events.
    const DefaultUnitsGuard                        units;
    const std::vector<SimulationPlotConfiguration> presets =
        SimulationPlotConfiguration::defaultConfigurations();
    ASSERT_FALSE(presets.empty());
    const FlightDataBranch data = plotFlight();

    const std::unique_ptr<SimulationPlotConfiguration::Base> filled = presets[0].fillAutoAxes(data);
    ASSERT_EQ(filled->getDataCount(), 3U);
    EXPECT_EQ(filled->getAxis(0), 0);
    EXPECT_EQ(filled->getAxis(1), 0);
    EXPECT_EQ(filled->getAxis(2), 1);
    EXPECT_EQ(filled->getAllAxes()[1].getMaxValue(), 31.4);
    const auto* filledSimulation = dynamic_cast<const SimulationPlotConfiguration*>(filled.get());
    ASSERT_NE(filledSimulation, nullptr);
    EXPECT_EQ(filledSimulation->getActiveEvents(), presets[0].getActiveEvents());
    EXPECT_EQ(filledSimulation->getName(), "Vertical motion vs. time");
}

}  // namespace
