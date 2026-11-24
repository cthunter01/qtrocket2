#include "QtRocket/plot/PlotConfiguration.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/plot/PlotAxisBounds.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/LineStyle.h"
#include "plot/PlotBranchSupport.h"
#include "unit/DefaultUnitsGuard.h"

// The expected values are what OpenRocket's PlotConfiguration gives for the same data: the probe
// tools/openrocket-probes/plot-configuration (PlotConfigurationProbe, expected.txt) prints them,
// every double with Double.toString (which reads back as the same double) and in hex. Where
// QtRocket fixes one of OpenRocket's bugs (45 and 46 of plan 11.4, and fillAutoAxes() without
// rows), the test expects QtRocket's value and names Java's beside it.

namespace
{

using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::FlightDataBranch;
using QtRocket::FlightDataType;
using QtRocket::LineStyle;
using QtRocket::PlotAxisBounds;
using QtRocket::SimulationPlotConfiguration;
using QtRocket::Unit;
using QtRocket::UnitGroupId;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::plotFlight;
using QtRocket::Test::plotLinear;
using QtRocket::Test::plotSeries;
using Id     = QtRocket::FlightDataTypeId;
using Config = QtRocket::PlotConfiguration<FlightDataType, FlightDataBranch>;

constexpr double kNaN      = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

[[nodiscard]] const FlightDataType& builtin(Id id)
{
    return FlightDataType::builtin(id);
}

[[nodiscard]] const Unit& unitNamed(UnitGroupId group, std::string_view name)
{
    const Unit* unit = QtRocket::unitGroup(group).getUnit(name);
    if (unit == nullptr)
    {
        ADD_FAILURE() << "no unit " << name;
        return QtRocket::unitGroup(group).getDefaultUnit();
    }
    return *unit;
}

/// A row of a configuration made by configOf(): a type on an axis (negative: automatic).
struct RowSpec
{
    RowSpec(Id rowType, int rowAxis) : type(rowType), axis(rowAxis) { }

    Id  type;
    int axis;
};

/// A configuration with @p rows, added as the probe's config() adds them.
[[nodiscard]] SimulationPlotConfiguration configOf(std::initializer_list<RowSpec> rows)
{
    SimulationPlotConfiguration config("probe");
    for (const RowSpec& row : rows)
    {
        if (row.axis < 0)
        {
            config.addPlotDataType(builtin(row.type));
        }
        else
        {
            config.addPlotDataType(builtin(row.type), row.axis);
        }
    }
    return config;
}

/// The rows' axes.
[[nodiscard]] std::vector<int> axesOf(const Config& config)
{
    std::vector<int> axes;
    axes.reserve(config.getDataCount());
    for (std::size_t i = 0; i < config.getDataCount(); ++i)
    {
        axes.push_back(config.getAxis(i));
    }
    return axes;
}

/// Whether @p actual is @p expected bit for bit (any NaN for a NaN).
[[nodiscard]] bool sameDouble(double actual, double expected)
{
    if (std::isnan(expected))
    {
        return std::isnan(actual);
    }
    return std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected);
}

/// The two ranges of @p bounds as "left [min, max] right [min, max]", each double in hex.
[[nodiscard]] std::string rangesText(std::span<const PlotAxisBounds> bounds)
{
    return std::format("left [{:a}, {:a}] right [{:a}, {:a}]", bounds[0].getMinValue(),
                       bounds[0].getMaxValue(), bounds[1].getMinValue(), bounds[1].getMaxValue());
}

/// Whether @p bounds (the left and the right axis) are exactly the given ranges.
[[nodiscard]] ::testing::AssertionResult rangesAre(std::span<const PlotAxisBounds> bounds,
                                                   double leftMin, double leftMax, double rightMin,
                                                   double rightMax)
{
    if (sameDouble(bounds[0].getMinValue(), leftMin) &&
        sameDouble(bounds[0].getMaxValue(), leftMax) &&
        sameDouble(bounds[1].getMinValue(), rightMin) &&
        sameDouble(bounds[1].getMaxValue(), rightMax))
    {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << rangesText(bounds) << ", expected "
                                         << std::format("left [{:a}, {:a}] right [{:a}, {:a}]",
                                                        leftMin, leftMax, rightMin, rightMax);
}

/// rangesAre() over a configuration's axes.
[[nodiscard]] ::testing::AssertionResult axesAre(const Config& config, double leftMin,
                                                 double leftMax, double rightMin, double rightMax)
{
    return rangesAre(config.getAllAxes(), leftMin, leftMax, rightMin, rightMax);
}

/// fitAxes() over @p branches.
void fit(Config& config, std::initializer_list<const FlightDataBranch*> branches)
{
    const std::vector<const FlightDataBranch*> data(branches);
    config.fitAxes(data);
}

/// One candidate fillAutoAxes() scored: its axes, its score and the ranges it was scored on.
struct Candidate
{
    std::vector<int>              axes;
    double                        goodness{};
    std::array<PlotAxisBounds, 2> ranges{};
};

/// A configuration that records every candidate its getGoodnessValue() scores (the probe's
/// LoggedConfiguration); its copies record into the same list.
class RecordingConfiguration final : public SimulationPlotConfiguration
{
public:
    explicit RecordingConfiguration(std::vector<Candidate>& log)
      : SimulationPlotConfiguration("logged"), m_log(&log)
    {
    }

    [[nodiscard]] std::unique_ptr<Base> cloneConfiguration() const override
    {
        return std::make_unique<RecordingConfiguration>(*this);
    }

protected:
    [[nodiscard]] double getGoodnessValue(const FlightDataBranch& data) override
    {
        const double goodness = SimulationPlotConfiguration::getGoodnessValue(data);
        m_log->push_back(
            Candidate{.axes = axesOf(*this), .goodness = goodness, .ranges = getAllAxes()});
        return goodness;
    }

private:
    std::vector<Candidate>* m_log;
};

/// A configuration whose candidates score what @p score gives for their axes (the probe's
/// ScoredConfiguration); its copies share the function and record the axes they were asked for.
class ScoredConfiguration final : public SimulationPlotConfiguration
{
public:
    using Score = std::function<double(const std::vector<int>&)>;

    ScoredConfiguration(Score score, std::vector<std::vector<int>>& asked)
      : SimulationPlotConfiguration("scored"), m_score(std::move(score)), m_asked(&asked)
    {
    }

    [[nodiscard]] std::unique_ptr<Base> cloneConfiguration() const override
    {
        return std::make_unique<ScoredConfiguration>(*this);
    }

protected:
    [[nodiscard]] double getGoodnessValue(const FlightDataBranch& /*data*/) override
    {
        m_asked->push_back(axesOf(*this));
        return m_score(axesOf(*this));
    }

private:
    Score                          m_score;
    std::vector<std::vector<int>>* m_asked;
};

/// Port of PlotConfigurationTest.NaNGoodnessSimulationPlotConfiguration: every candidate scores
/// NaN.
class NaNGoodnessSimulationPlotConfiguration final : public SimulationPlotConfiguration
{
public:
    explicit NaNGoodnessSimulationPlotConfiguration(std::string name)
      : SimulationPlotConfiguration(std::move(name))
    {
    }

    [[nodiscard]] std::unique_ptr<Base> cloneConfiguration() const override
    {
        return std::make_unique<NaNGoodnessSimulationPlotConfiguration>(*this);
    }

protected:
    [[nodiscard]] double getGoodnessValue(const FlightDataBranch& /*data*/) override
    {
        return kNaN;
    }
};

/// The candidates fillAutoAxes() scored for @p rows over @p mainBranch, and the configuration it
/// chose.
struct AutoAxes
{
    std::vector<Candidate>  candidates;
    std::unique_ptr<Config> chosen;
    std::vector<int>        sourceAxes;
};

[[nodiscard]] AutoAxes fillAuto(std::initializer_list<RowSpec> rows,
                                const FlightDataBranch&        mainBranch)
{
    AutoAxes               result;
    RecordingConfiguration config(result.candidates);
    for (const RowSpec& row : rows)
    {
        if (row.axis < 0)
        {
            config.addPlotDataType(builtin(row.type));
        }
        else
        {
            config.addPlotDataType(builtin(row.type), row.axis);
        }
    }
    result.chosen     = config.fillAutoAxes(mainBranch);
    result.sourceAxes = axesOf(config);
    return result;
}

/// The axes fillAutoAxes() chose with a score function over the automatic rows altitude,
/// vertical velocity and vertical acceleration (as many as @p autoRows), and the axes it asked
/// the function for, in order.
struct ScoredChoice
{
    std::vector<std::vector<int>> asked;
    std::unique_ptr<Config>       chosen;
};

[[nodiscard]] ScoredChoice chooseByScore(std::size_t autoRows, ScoredConfiguration::Score score)
{
    ScoredChoice        result;
    ScoredConfiguration config(std::move(score), result.asked);
    const std::array    types{Id::TYPE_ALTITUDE, Id::TYPE_VELOCITY_Z, Id::TYPE_ACCELERATION_Z};
    for (std::size_t i = 0; i < autoRows; ++i)
    {
        config.addPlotDataType(builtin(types.at(i)));
    }
    const FlightDataBranch mainBranch = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {1, 2});
    result.chosen                     = config.fillAutoAxes(mainBranch);
    return result;
}

/// The axes of every candidate of @p result, in the order they were scored.
[[nodiscard]] std::vector<std::vector<int>> candidateAxes(const AutoAxes& result)
{
    std::vector<std::vector<int>> axes;
    axes.reserve(result.candidates.size());
    for (const Candidate& candidate : result.candidates)
    {
        axes.push_back(candidate.axes);
    }
    return axes;
}

/// The score of every candidate of @p result, in the order they were scored.
[[nodiscard]] std::vector<double> candidateScores(const AutoAxes& result)
{
    std::vector<double> scores;
    scores.reserve(result.candidates.size());
    for (const Candidate& candidate : result.candidates)
    {
        scores.push_back(candidate.goodness);
    }
    return scores;
}

// The score functions of the probe's SCORED cases, by the candidates' axes.

[[nodiscard]] double constantScore(const std::vector<int>& /*axes*/)
{
    return 5.0;
}

[[nodiscard]] double nanScore(const std::vector<int>& /*axes*/)
{
    return kNaN;
}

[[nodiscard]] double lastBestScore(const std::vector<int>& axes)
{
    return axes == std::vector{1, 1, 1} ? 2.0 : 1.0;
}

[[nodiscard]] double negativeScore(const std::vector<int>& axes)
{
    return axes == std::vector{1, 0} ? -1.0 : -2.0;
}

/// NaN, 3, Infinity, 3.
[[nodiscard]] double nonFiniteScore(const std::vector<int>& axes)
{
    if (axes == std::vector{0, 0})
    {
        return kNaN;
    }
    return axes == std::vector{1, 0} ? kInfinity : 3.0;
}

/// NaN, -Infinity, -Infinity, 7.
[[nodiscard]] double laterBestScore(const std::vector<int>& axes)
{
    if (axes == std::vector{0, 0})
    {
        return kNaN;
    }
    return axes == std::vector{1, 1} ? 7.0 : -kInfinity;
}

//// The rows and the domain

TEST(PlotConfiguration, ANewConfigurationHasNoRowsAndNoRanges)
{
    // "EDITS new: domain=Time 's' rows=0 events=[]"
    const DefaultUnitsGuard units;
    const Config            config("A name", builtin(Id::TYPE_TIME));
    EXPECT_EQ(config.getName(), "A name");
    EXPECT_EQ(config.getNameKey(), "");
    EXPECT_EQ(&config.getDomainAxisType(), &builtin(Id::TYPE_TIME));
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "s");
    EXPECT_EQ(config.getDataCount(), 0U);
    EXPECT_TRUE(axesAre(config, kNaN, kNaN, kNaN, kNaN));
}

TEST(PlotConfiguration, ARowStartsInItsDefaultUnitWithoutAColorOnASolidLine)
{
    const DefaultUnitsGuard units;
    Config                  config("rows", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE));
    config.addPlotDataType(builtin(Id::TYPE_VELOCITY_Z), 1);
    config.addPlotDataType(builtin(Id::TYPE_ACCELERATION_Z), 0);

    ASSERT_EQ(config.getDataCount(), 3U);
    EXPECT_EQ(&config.getType(0), &builtin(Id::TYPE_ALTITUDE));
    EXPECT_EQ(config.getUnit(0).getUnit(), "m");
    EXPECT_EQ(config.getAxis(0), Config::kAutoAxis);
    EXPECT_EQ(config.getAxis(0), -1);
    EXPECT_FALSE(config.getPlotDataColor(0).has_value());
    EXPECT_EQ(config.getPlotDataLineStyle(0), LineStyle::SOLID);
    EXPECT_EQ(&config.getType(1), &builtin(Id::TYPE_VELOCITY_Z));
    EXPECT_EQ(config.getUnit(1).getUnit(), "m/s");
    EXPECT_EQ(config.getAxis(1), 1);
    EXPECT_EQ(config.getAxis(2), 0);
}

TEST(PlotConfiguration, TheDefaultUnitIsTheOneOfTheMoment)
{
    const DefaultUnitsGuard units;
    ASSERT_TRUE(QtRocket::unitGroup(UnitGroupId::DISTANCE).setDefaultUnit("ft"));
    Config config("feet", builtin(Id::TYPE_POSITION_X));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 0);
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "ft");
    EXPECT_EQ(config.getUnit(0).getUnit(), "ft");
}

TEST(PlotConfiguration, AnAxisAboveTheRightOneIsABug)
{
    // "EDITS setPlotDataAxis(2): IllegalArgumentException: Axis index too large" and
    // "EDITS setPlotDataAxis(-2): axis=-2": any negative axis is an automatic one.
    Config config("axes", builtin(Id::TYPE_TIME));
    EXPECT_THROW(config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 2), BugError);
    EXPECT_EQ(config.getDataCount(), 0U);

    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 1);
    EXPECT_THROW(config.setPlotDataAxis(0, 2), BugError);
    EXPECT_EQ(config.getAxis(0), 1);
    config.setPlotDataAxis(0, -2);
    EXPECT_EQ(config.getAxis(0), -2);
    config.setPlotDataAxis(0, 0);
    EXPECT_EQ(config.getAxis(0), 0);
}

TEST(PlotConfiguration, ANewTypeKeepsTheUnitOfItsGroup)
{
    // "EDITS same group: Position East of launch 'ft'", "EDITS other group: Vertical velocity
    // 'm/s'", "EDITS unit of another group: IllegalArgumentException"
    const DefaultUnitsGuard units;
    Config                  config("edits", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE));
    config.setPlotDataUnit(0, unitNamed(UnitGroupId::DISTANCE, "ft"));
    config.setPlotDataType(0, builtin(Id::TYPE_POSITION_X));
    EXPECT_EQ(&config.getType(0), &builtin(Id::TYPE_POSITION_X));
    EXPECT_EQ(config.getUnit(0).getUnit(), "ft");

    config.setPlotDataType(0, builtin(Id::TYPE_VELOCITY_Z));
    EXPECT_EQ(&config.getType(0), &builtin(Id::TYPE_VELOCITY_Z));
    EXPECT_EQ(config.getUnit(0).getUnit(), "m/s");

    EXPECT_THROW(config.setPlotDataUnit(0, unitNamed(UnitGroupId::DISTANCE, "ft")), BugError);
    EXPECT_EQ(config.getUnit(0).getUnit(), "m/s");
}

TEST(PlotConfiguration, SetsATypeAUnitAndAnAxisAtOnce)
{
    const DefaultUnitsGuard units;
    Config                  config("edits", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE));
    const Unit& feetPerSecond = unitNamed(UnitGroupId::VELOCITY, "ft/s");
    config.setPlotDataType(0, builtin(Id::TYPE_VELOCITY_Z), feetPerSecond, 1);
    EXPECT_EQ(&config.getType(0), &builtin(Id::TYPE_VELOCITY_Z));
    EXPECT_EQ(&config.getUnit(0), &feetPerSecond);
    EXPECT_EQ(config.getAxis(0), 1);
    EXPECT_THROW(config.setPlotDataType(0, builtin(Id::TYPE_ALTITUDE), feetPerSecond, 2), BugError);
}

TEST(PlotConfiguration, ANewDomainKeepsTheUnitOfItsGroup)
{
    // "EDITS domain time step: Simulation time step 's' group same=false", "EDITS domain mach:
    // Mach number", "EDITS domain same group: Position East of launch 'ft'"
    const DefaultUnitsGuard units;
    Config                  config("edits", builtin(Id::TYPE_TIME));
    config.setDomainAxisUnit(unitNamed(UnitGroupId::LONG_TIME, "min"));
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "min");
    config.setDomainAxisType(builtin(Id::TYPE_TIME_STEP));
    EXPECT_EQ(&config.getDomainAxisType(), &builtin(Id::TYPE_TIME_STEP));
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "s");

    config.setDomainAxisType(builtin(Id::TYPE_MACH_NUMBER));
    EXPECT_EQ(&config.getDomainAxisUnit(),
              &builtin(Id::TYPE_MACH_NUMBER).getUnitGroup().getDefaultUnit());
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), QtRocket::Chars::kZwsp);

    config.setDomainAxisType(builtin(Id::TYPE_ALTITUDE));
    config.setDomainAxisUnit(unitNamed(UnitGroupId::DISTANCE, "ft"));
    config.setDomainAxisType(builtin(Id::TYPE_POSITION_X));
    EXPECT_EQ(&config.getDomainAxisType(), &builtin(Id::TYPE_POSITION_X));
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "ft");

    EXPECT_THROW(config.setDomainAxisUnit(unitNamed(UnitGroupId::VELOCITY, "m/s")), BugError);
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "ft");
}

TEST(PlotConfiguration, ResetUnitsResetsTheRowsButNotTheDomain)
{
    // "EDITS resetUnits: domain 'ft' row 'm/s'"
    const DefaultUnitsGuard units;
    Config                  config("edits", builtin(Id::TYPE_POSITION_X));
    config.setDomainAxisUnit(unitNamed(UnitGroupId::DISTANCE, "ft"));
    config.addPlotDataType(builtin(Id::TYPE_VELOCITY_Z));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 1);
    config.setPlotDataUnit(0, unitNamed(UnitGroupId::VELOCITY, "ft/s"));
    config.setPlotDataUnit(1, unitNamed(UnitGroupId::DISTANCE, "km"));

    const Config& reset = config.resetUnits();
    EXPECT_EQ(&reset, &config);
    EXPECT_EQ(config.getDomainAxisUnit().getUnit(), "ft");
    EXPECT_EQ(config.getUnit(0).getUnit(), "m/s");
    EXPECT_EQ(config.getUnit(1).getUnit(), "m");
    EXPECT_EQ(config.getAxis(1), 1);
}

TEST(PlotConfiguration, RemovesRowsAndClearsTheirAppearances)
{
    Config config("rows", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 0);
    config.addPlotDataType(builtin(Id::TYPE_VELOCITY_Z), 1);
    config.addPlotDataType(builtin(Id::TYPE_ACCELERATION_Z));
    config.setPlotDataColor(0, Color{1, 2, 3});
    config.setPlotDataLineStyle(0, LineStyle::DASHED);
    config.setPlotDataColor(2, Color{4, 5, 6, 7});
    config.setPlotDataLineStyle(2, LineStyle::DASHDOT);
    EXPECT_EQ(config.getPlotDataColor(0), (Color{1, 2, 3}));
    EXPECT_EQ(config.getPlotDataLineStyle(0), LineStyle::DASHED);

    config.removePlotDataType(1);
    ASSERT_EQ(config.getDataCount(), 2U);
    EXPECT_EQ(&config.getType(1), &builtin(Id::TYPE_ACCELERATION_Z));
    EXPECT_EQ(config.getAxis(1), Config::kAutoAxis);
    EXPECT_EQ(config.getPlotDataColor(1), (Color{4, 5, 6, 7}));
    EXPECT_EQ(config.getPlotDataLineStyle(1), LineStyle::DASHDOT);

    config.setPlotDataColor(1, std::nullopt);
    EXPECT_FALSE(config.getPlotDataColor(1).has_value());

    config.clearPlotAppearances();
    EXPECT_FALSE(config.getPlotDataColor(0).has_value());
    EXPECT_EQ(config.getPlotDataLineStyle(0), LineStyle::SOLID);
    EXPECT_FALSE(config.getPlotDataColor(1).has_value());
    EXPECT_EQ(config.getPlotDataLineStyle(1), LineStyle::SOLID);
}

TEST(PlotConfiguration, ARowBeyondTheLastIsABug)
{
    Config config("rows", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE));
    EXPECT_THROW(static_cast<void>(config.getType(1)), BugError);
    EXPECT_THROW(static_cast<void>(config.getUnit(1)), BugError);
    EXPECT_THROW(static_cast<void>(config.getAxis(1)), BugError);
    EXPECT_THROW(static_cast<void>(config.getPlotDataColor(1)), BugError);
    EXPECT_THROW(static_cast<void>(config.getPlotDataLineStyle(1)), BugError);
    EXPECT_THROW(config.setPlotDataType(1, builtin(Id::TYPE_ALTITUDE)), BugError);
    EXPECT_THROW(config.setPlotDataUnit(1, config.getUnit(0)), BugError);
    EXPECT_THROW(config.setPlotDataAxis(1, 0), BugError);
    EXPECT_THROW(config.setPlotDataColor(1, Color{1, 2, 3}), BugError);
    EXPECT_THROW(config.setPlotDataLineStyle(1, LineStyle::DOTTED), BugError);
    EXPECT_THROW(config.removePlotDataType(1), BugError);
    EXPECT_EQ(config.getDataCount(), 1U);
}

TEST(PlotConfiguration, IsRenamedWithOrWithoutAKey)
{
    Config config("A name", "A.key", builtin(Id::TYPE_TIME));
    EXPECT_EQ(config.getName(), "A name");
    EXPECT_EQ(config.getNameKey(), "A.key");
    config.setName("Custom", "simplotpanel.CUSTOM");
    EXPECT_EQ(config.getName(), "Custom");
    EXPECT_EQ(config.getNameKey(), "simplotpanel.CUSTOM");
    config.setName("Mine");
    EXPECT_EQ(config.getName(), "Mine");
    EXPECT_EQ(config.getNameKey(), "");
}

TEST(PlotConfiguration, ACopyIsAConfigurationOfItsOwn)
{
    const DefaultUnitsGuard units;
    Config                  config("source", builtin(Id::TYPE_TIME));
    config.addPlotDataType(builtin(Id::TYPE_ALTITUDE), 0);
    const FlightDataBranch data = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {10, 20});
    fit(config, {&data});

    const std::unique_ptr<Config> clone = config.cloneConfiguration();
    const Config                  copy  = config;
    config.setPlotDataAxis(0, 1);
    config.setName("changed");
    fit(config, {&data});

    EXPECT_EQ(clone->getName(), "source");
    EXPECT_EQ(clone->getAxis(0), 0);
    EXPECT_TRUE(axesAre(*clone, 9.7, 20.3, kNaN, kNaN));
    EXPECT_EQ(copy.getName(), "source");
    EXPECT_EQ(copy.getAxis(0), 0);
    EXPECT_TRUE(axesAre(copy, 9.7, 20.3, kNaN, kNaN));
    EXPECT_TRUE(axesAre(config, kNaN, kNaN, 9.7, 20.3));
}

//// fitAxes()

TEST(PlotConfiguration, AFlatRangeIsWidenedByOneOnEachSide)
{
    // "FIT flat", "FIT single-point", "FIT flat-zero" and "FIT tiny" (equal within
    // MathUtil.equals' 1e-8)
    const DefaultUnitsGuard units;
    const FlightDataType&   altitude = builtin(Id::TYPE_ALTITUDE);

    SimulationPlotConfiguration flat       = configOf({{Id::TYPE_ALTITUDE, 0}});
    const FlightDataBranch      sevenTwice = plotSeries("b", altitude, {7, 7});
    fit(flat, {&sevenTwice});
    EXPECT_TRUE(axesAre(flat, 5.94, 8.06, kNaN, kNaN));

    const FlightDataBranch seven = plotSeries("b", altitude, {7});
    fit(flat, {&seven});
    EXPECT_TRUE(axesAre(flat, 5.94, 8.06, kNaN, kNaN));

    const FlightDataBranch zero = plotSeries("b", altitude, {0, 0});
    fit(flat, {&zero});
    EXPECT_TRUE(axesAre(flat, -1.06, 1.06, kNaN, kNaN));

    const FlightDataBranch tiny = plotSeries("b", altitude, {1e-7, 1.00000001e-7});
    fit(flat, {&tiny});
    EXPECT_TRUE(axesAre(flat, -1.0599999, 1.060000100000001, kNaN, kNaN));
}

TEST(PlotConfiguration, ThreePercentOfTheRangeIsAddedOnEachSide)
{
    // "FIT margins", "FIT spans-zero" and "FIT right-only"
    const DefaultUnitsGuard     units;
    const FlightDataType&       altitude    = builtin(Id::TYPE_ALTITUDE);
    SimulationPlotConfiguration left        = configOf({{Id::TYPE_ALTITUDE, 0}});
    const FlightDataBranch      tenToTwenty = plotSeries("b", altitude, {10, 20});
    fit(left, {&tenToTwenty});
    EXPECT_TRUE(axesAre(left, 9.7, 20.3, kNaN, kNaN));

    const FlightDataBranch spanning = plotSeries("b", altitude, {-3, 7});
    fit(left, {&spanning});
    EXPECT_TRUE(axesAre(left, -3.3, 7.3, kNaN, kNaN));

    SimulationPlotConfiguration right = configOf({{Id::TYPE_ALTITUDE, 1}});
    fit(right, {&tenToTwenty});
    EXPECT_TRUE(axesAre(right, kNaN, kNaN, 9.7, 20.3));
}

TEST(PlotConfiguration, ZeroIsIncludedWithinThirtyPercentOfTheRange)
{
    // "FIT zero-near-low", "FIT zero-just-in", "FIT zero-just-out" and "FIT zero-near-high":
    // from 3.4 to 13.4 the lower end, 3.1 after the margin, is within 0.3 * 10.6 of zero; from 3.5
    // to 13.5 it is not.
    const DefaultUnitsGuard     units;
    const FlightDataType&       altitude = builtin(Id::TYPE_ALTITUDE);
    SimulationPlotConfiguration config   = configOf({{Id::TYPE_ALTITUDE, 0}});

    const FlightDataBranch nearLow = plotSeries("b", altitude, {2, 12});
    fit(config, {&nearLow});
    EXPECT_TRUE(axesAre(config, 0.0, 12.3, kNaN, kNaN));

    const FlightDataBranch justIn = plotSeries("b", altitude, {3.4, 13.4});
    fit(config, {&justIn});
    EXPECT_TRUE(axesAre(config, 0.0, 13.700000000000001, kNaN, kNaN));

    const FlightDataBranch justOut = plotSeries("b", altitude, {3.5, 13.5});
    fit(config, {&justOut});
    EXPECT_TRUE(axesAre(config, 3.2, 13.8, kNaN, kNaN));

    const FlightDataBranch nearHigh = plotSeries("b", altitude, {-12, -2});
    fit(config, {&nearHigh});
    EXPECT_TRUE(axesAre(config, -12.3, 0.0, kNaN, kNaN));
}

TEST(PlotConfiguration, TheRangesAreInTheRowsUnits)
{
    // "FIT in-feet"
    const DefaultUnitsGuard     units;
    SimulationPlotConfiguration config = configOf({{Id::TYPE_ALTITUDE, 0}});
    config.setPlotDataUnit(0, unitNamed(UnitGroupId::DISTANCE, "ft"));
    const FlightDataBranch data = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {10, 20});
    fit(config, {&data});
    EXPECT_TRUE(axesAre(config, 31.824146981627297, 66.60104986876641, kNaN, kNaN));
}

TEST(PlotConfiguration, TheRangesHoldEveryBranchButTheEmptyOnes)
{
    // "FIT branches"
    const DefaultUnitsGuard     units;
    const FlightDataType&       altitude = builtin(Id::TYPE_ALTITUDE);
    SimulationPlotConfiguration config   = configOf({{Id::TYPE_ALTITUDE, 0}});
    const FlightDataBranch      first    = plotSeries("b0", altitude, {0, 50});
    const FlightDataBranch      second   = plotSeries("b1", altitude, {-5, 80});
    const FlightDataBranch      empty    = plotSeries("empty", altitude, {});
    fit(config, {&first, &second, &empty});
    EXPECT_TRUE(axesAre(config, -7.55, 82.55, kNaN, kNaN));
}

TEST(PlotConfiguration, ABranchWithoutTheRowsTypeIsIgnored)
{
    // Bug 46: Java gives NaN on the left axis in all four ("FIT other-branch-without-type", "FIT
    // main-branch-without-type", "FIT nan-column", "FIT branch-0-empty"); QtRocket the range of
    // the branches that have the type (the "FIT branches" range for the last).
    const DefaultUnitsGuard     units;
    const FlightDataType&       altitude      = builtin(Id::TYPE_ALTITUDE);
    SimulationPlotConfiguration config        = configOf({{Id::TYPE_ALTITUDE, 0}});
    const FlightDataBranch      fiveToHundred = plotSeries("b0", altitude, {5, 100});
    FlightDataBranch            timeOnly("t", {builtin(Id::TYPE_TIME)});
    timeOnly.addPoint();
    timeOnly.setValue(builtin(Id::TYPE_TIME), 0);

    fit(config, {&fiveToHundred, &timeOnly});
    EXPECT_TRUE(axesAre(config, 0.0, 102.85, kNaN, kNaN));

    fit(config, {&timeOnly, &fiveToHundred});
    EXPECT_TRUE(axesAre(config, 0.0, 102.85, kNaN, kNaN));

    const FlightDataBranch nanColumn = plotSeries("b0", altitude, {kNaN, kNaN});
    fit(config, {&nanColumn, &fiveToHundred});
    EXPECT_TRUE(axesAre(config, 0.0, 102.85, kNaN, kNaN));

    const FlightDataBranch empty  = plotSeries("empty", altitude, {});
    const FlightDataBranch second = plotSeries("b1", altitude, {-5, 80});
    fit(config, {&empty, &second});
    EXPECT_TRUE(axesAre(config, -7.55, 82.55, kNaN, kNaN));

    // Only branches without the type: the axis has no range.
    fit(config, {&timeOnly});
    EXPECT_TRUE(axesAre(config, kNaN, kNaN, kNaN, kNaN));
}

TEST(PlotConfiguration, TwoAxesThatHoldZeroShareIt)
{
    // "FIT common-zero-lowers-right-min" (the left zero is higher: the right minimum goes down),
    // "FIT common-zero-raises-right-max" (it is lower: the right maximum goes up),
    // "FIT common-zero-equal", "FIT no-common-zero-right-positive" and "FIT right-starts-at-zero"
    const DefaultUnitsGuard     units;
    SimulationPlotConfiguration config =
        configOf({{Id::TYPE_ALTITUDE, 0}, {Id::TYPE_VELOCITY_Z, 1}});

    const FlightDataBranch lowersMin = plotLinear(-10, 30, -1, 100);
    fit(config, {&lowersMin});
    EXPECT_TRUE(axesAre(config, -11.2, 31.2, -36.9851282051282, 103.03));

    const FlightDataBranch raisesMax = plotLinear(-1, 100, -10, 30);
    fit(config, {&raisesMax});
    EXPECT_TRUE(axesAre(config, -4.029999999999999, 103.03, -11.2, 286.33647642679904));

    const FlightDataBranch equal = plotLinear(-10, 30, -10, 30);
    fit(config, {&equal});
    EXPECT_TRUE(axesAre(config, -11.2, 31.2, -11.2, 31.2));

    const FlightDataBranch rightPositive = plotLinear(-10, 30, 50, 100);
    fit(config, {&rightPositive});
    EXPECT_TRUE(axesAre(config, -11.2, 31.2, 48.5, 101.5));

    const FlightDataBranch rightFromZero = plotLinear(-10, 10, 5, 100);
    fit(config, {&rightFromZero});
    EXPECT_TRUE(axesAre(config, -10.6, 10.6, -102.85, 102.85));
}

TEST(PlotConfiguration, ALeftRangeEndingAtZeroLeavesTheRightOneFinite)
{
    // Bug 45: Java gives the right axis [-Infinity, 10.6] in both ("FIT left-starts-at-zero",
    // "FIT left-ends-at-zero"); QtRocket leaves out the common zero, so the right axis keeps its
    // range with the margins.
    const DefaultUnitsGuard     units;
    SimulationPlotConfiguration config =
        configOf({{Id::TYPE_ALTITUDE, 0}, {Id::TYPE_VELOCITY_Z, 1}});

    const FlightDataBranch startsAtZero = plotLinear(5, 100, -10, 10);
    fit(config, {&startsAtZero});
    EXPECT_TRUE(axesAre(config, 0.0, 102.85, -10.6, 10.6));

    const FlightDataBranch endsAtZero = plotLinear(-100, -5, -10, 10);
    fit(config, {&endsAtZero});
    EXPECT_TRUE(axesAre(config, -102.85, 0.0, -10.6, 10.6));
}

TEST(PlotConfiguration, FittingAnAutomaticRowOrNoBranchIsABug)
{
    // "EMPTY fitAxes(no branches)": without rows, no branch is needed.
    SimulationPlotConfiguration empty("empty");
    fit(empty, {});
    EXPECT_TRUE(axesAre(empty, kNaN, kNaN, kNaN, kNaN));

    SimulationPlotConfiguration config = configOf({{Id::TYPE_ALTITUDE, 0}});
    EXPECT_THROW(fit(config, {}), BugError);
    const FlightDataBranch data = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {1, 2});
    EXPECT_THROW(fit(config, {&data, nullptr}), BugError);

    config.addPlotDataType(builtin(Id::TYPE_VELOCITY_Z));
    EXPECT_THROW(fit(config, {&data}), BugError);
}

//// fillAutoAxes()

TEST(PlotConfiguration, ScoresEveryAssignmentOfTheAutomaticRowsInOrder)
{
    // "AUTO vertical-motion" (the rows of the first preset): the second automatic row varies
    // fastest; the best score wins.
    const DefaultUnitsGuard units;
    const FlightDataBranch  mainBranch = plotFlight();
    const AutoAxes          result =
        fillAuto({{Id::TYPE_ALTITUDE, 0}, {Id::TYPE_VELOCITY_Z, -1}, {Id::TYPE_ACCELERATION_Z, -1}},
                 mainBranch);

    ASSERT_EQ(result.candidates.size(), 4U);
    EXPECT_EQ(result.candidates[0].axes, (std::vector{0, 0, 0}));
    EXPECT_EQ(result.candidates[0].goodness, 254.73819299406614);
    EXPECT_TRUE(rangesAre(result.candidates[0].ranges, -53.265, 132.765, kNaN, kNaN));
    EXPECT_EQ(result.candidates[1].axes, (std::vector{0, 0, 1}));
    EXPECT_EQ(result.candidates[1].goodness, 293.71689698822547);
    EXPECT_TRUE(
        rangesAre(result.candidates[1].ranges, -53.265, 132.765, -12.597604790419165, 31.4));
    EXPECT_EQ(result.candidates[2].axes, (std::vector{0, 1, 0}));
    EXPECT_EQ(result.candidates[2].goodness, 231.9177546157083);
    EXPECT_TRUE(
        rangesAre(result.candidates[2].ranges, -13.919, 131.619, -50.94, 481.6920655219484));
    EXPECT_EQ(result.candidates[3].axes, (std::vector{0, 1, 1}));
    EXPECT_EQ(result.candidates[3].goodness, 178.37031416456875);
    EXPECT_TRUE(rangesAre(result.candidates[3].ranges, -3.8249999999999997, 131.325, -50.94,
                          1748.9399999999996));

    // "CHOSEN [0 0 1] ... class=LoggedConfiguration source still [0 -1 -1]"
    ASSERT_NE(result.chosen, nullptr);
    EXPECT_EQ(axesOf(*result.chosen), (std::vector{0, 0, 1}));
    EXPECT_TRUE(axesAre(*result.chosen, -53.265, 132.765, -12.597604790419165, 31.4));
    EXPECT_NE(dynamic_cast<const RecordingConfiguration*>(result.chosen.get()), nullptr);
    EXPECT_EQ(result.sourceAxes, (std::vector{0, -1, -1}));
}

TEST(PlotConfiguration, TheFirstAutomaticRowIsTheMostSignificant)
{
    // "AUTO all-auto"
    const DefaultUnitsGuard units;
    const FlightDataBranch  mainBranch = plotFlight();
    const AutoAxes          result     = fillAuto(
        {{Id::TYPE_ALTITUDE, -1}, {Id::TYPE_VELOCITY_Z, -1}, {Id::TYPE_ACCELERATION_Z, -1}},
        mainBranch);

    ASSERT_EQ(result.candidates.size(), 8U);
    EXPECT_EQ(candidateAxes(result), (std::vector<std::vector<int>>{{0, 0, 0},
                                                                    {0, 0, 1},
                                                                    {0, 1, 0},
                                                                    {0, 1, 1},
                                                                    {1, 0, 0},
                                                                    {1, 0, 1},
                                                                    {1, 1, 0},
                                                                    {1, 1, 1}}));
    EXPECT_EQ(candidateScores(result),
              (std::vector{254.73819299406614, 293.71689698822547, 231.9177546157083,
                           178.37031416456875, 271.5225771070281, 248.74550333573387,
                           287.0209112407035, 251.73819299406614}));
    EXPECT_TRUE(
        rangesAre(result.candidates[4].ranges, -50.94, 52.94, -126.36372308273515, 131.325));
    EXPECT_TRUE(
        rangesAre(result.candidates[5].ranges, -50.94, 52.94, -126.64661616924819, 131.619));
    EXPECT_TRUE(rangesAre(result.candidates[6].ranges, -11.0, 31.4, -53.265, 152.04736363636363));
    EXPECT_TRUE(rangesAre(result.candidates[7].ranges, kNaN, kNaN, -53.265, 132.765));

    EXPECT_EQ(axesOf(*result.chosen), (std::vector{0, 0, 1}));
    EXPECT_TRUE(axesAre(*result.chosen, -53.265, 132.765, -12.597604790419165, 31.4));
    EXPECT_EQ(result.sourceAxes, (std::vector{-1, -1, -1}));
}

TEST(PlotConfiguration, OneAutomaticRowOrNone)
{
    // "AUTO one-auto" (the bonus of row 0 on the left axis decides) and "AUTO none-auto" (the
    // only candidate)
    const DefaultUnitsGuard units;
    const FlightDataBranch  mainBranch = plotFlight();

    const AutoAxes one = fillAuto({{Id::TYPE_ALTITUDE, -1}}, mainBranch);
    ASSERT_EQ(one.candidates.size(), 2U);
    EXPECT_EQ(one.candidates[0].goodness, 150.1285862357264);
    EXPECT_EQ(one.candidates[1].goodness, 147.1285862357264);
    EXPECT_TRUE(rangesAre(one.candidates[1].ranges, kNaN, kNaN, -3.8249999999999997, 131.325));
    EXPECT_EQ(axesOf(*one.chosen), (std::vector{0}));
    EXPECT_TRUE(axesAre(*one.chosen, -3.8249999999999997, 131.325, kNaN, kNaN));

    const AutoAxes none =
        fillAuto({{Id::TYPE_ALTITUDE, 0}, {Id::TYPE_VELOCITY_Z, 1}, {Id::TYPE_ACCELERATION_Z, 0}},
                 mainBranch);
    ASSERT_EQ(none.candidates.size(), 1U);
    EXPECT_EQ(none.candidates[0].goodness, 231.9177546157083);
    EXPECT_EQ(axesOf(*none.chosen), (std::vector{0, 1, 0}));
    EXPECT_TRUE(axesAre(*none.chosen, -13.919, 131.619, -50.94, 481.6920655219484));
}

TEST(PlotConfiguration, AFlatOrMissingRowScoresNothing)
{
    // "AUTO flat-row" (a constant altitude) and "AUTO missing-type" (no Mach number in the branch)
    const DefaultUnitsGuard units;
    const FlightDataBranch  flat = plotLinear(3, 3, -10, 10);
    const AutoAxes flatRow = fillAuto({{Id::TYPE_ALTITUDE, -1}, {Id::TYPE_VELOCITY_Z, -1}}, flat);
    ASSERT_EQ(flatRow.candidates.size(), 4U);
    EXPECT_EQ(flatRow.candidates[0].goodness, 150.1285862357264);
    EXPECT_EQ(flatRow.candidates[1].goodness, 98.12858623572642);
    EXPECT_TRUE(rangesAre(flatRow.candidates[1].ranges, 1.94, 4.06, -10.6, 10.6));
    EXPECT_EQ(flatRow.candidates[2].goodness, 99.12858623572642);
    EXPECT_EQ(flatRow.candidates[3].goodness, 147.1285862357264);
    EXPECT_EQ(axesOf(*flatRow.chosen), (std::vector{0, 0}));
    EXPECT_TRUE(axesAre(*flatRow.chosen, -10.6, 10.6, kNaN, kNaN));

    const FlightDataBranch withoutMach = plotLinear(0, 10, 0, 1);
    const AutoAxes         missing =
        fillAuto({{Id::TYPE_MACH_NUMBER, -1}, {Id::TYPE_ALTITUDE, -1}}, withoutMach);
    ASSERT_EQ(missing.candidates.size(), 4U);
    EXPECT_EQ(missing.candidates[0].goodness, 150.1285862357264);
    EXPECT_EQ(missing.candidates[1].goodness, 148.1285862357264);
    EXPECT_EQ(missing.candidates[2].goodness, 149.1285862357264);
    EXPECT_EQ(missing.candidates[3].goodness, 147.1285862357264);
    EXPECT_EQ(axesOf(*missing.chosen), (std::vector{0, 0}));
    EXPECT_TRUE(axesAre(*missing.chosen, -0.3, 10.3, kNaN, kNaN));
}

TEST(PlotConfiguration, TheCandidatesOfALeftRangeAtZeroAreScoredOnFiniteRanges)
{
    // Bug 45 in the search ("AUTO left-starts-at-zero"): Java scores [0 1] on a right axis of
    // [-Infinity, 10.6], which gives the velocity nothing (139.10802543107718), and chooses
    // [0 0]; QtRocket scores it on [-10.6, 10.6] and chooses it.
    const DefaultUnitsGuard units;
    const FlightDataBranch  mainBranch = plotLinear(5, 100, -10, 10);
    const AutoAxes          result =
        fillAuto({{Id::TYPE_ALTITUDE, 0}, {Id::TYPE_VELOCITY_Z, -1}}, mainBranch);
    ASSERT_EQ(result.candidates.size(), 2U);
    EXPECT_EQ(result.candidates[0].goodness, 184.67934273383275);
    EXPECT_TRUE(rangesAre(result.candidates[0].ranges, -13.3, 103.3, kNaN, kNaN));
    EXPECT_EQ(result.candidates[1].goodness, 236.23661166680358);
    EXPECT_TRUE(rangesAre(result.candidates[1].ranges, 0.0, 102.85, -10.6, 10.6));
    EXPECT_EQ(axesOf(*result.chosen), (std::vector{0, 1}));
    EXPECT_TRUE(axesAre(*result.chosen, 0.0, 102.85, -10.6, 10.6));
}

TEST(PlotConfiguration, TheFirstOfEqualScoresIsChosen)
{
    // "SCORED constant", "SCORED last-best" and "SCORED negative": the final ranges are fitted
    // to the main branch (altitude 1 to 2) on the chosen axes.
    const DefaultUnitsGuard units;
    const ScoredChoice      constant = chooseByScore(2, constantScore);
    EXPECT_EQ(constant.asked, (std::vector<std::vector<int>>{{0, 0}, {0, 1}, {1, 0}, {1, 1}}));
    EXPECT_EQ(axesOf(*constant.chosen), (std::vector{0, 0}));
    EXPECT_TRUE(axesAre(*constant.chosen, 0.97, 2.03, kNaN, kNaN));

    const ScoredChoice lastBest = chooseByScore(3, lastBestScore);
    EXPECT_EQ(lastBest.asked.size(), 8U);
    EXPECT_EQ(axesOf(*lastBest.chosen), (std::vector{1, 1, 1}));
    EXPECT_TRUE(axesAre(*lastBest.chosen, kNaN, kNaN, 0.97, 2.03));

    const ScoredChoice negative = chooseByScore(2, negativeScore);
    EXPECT_EQ(axesOf(*negative.chosen), (std::vector{1, 0}));
    EXPECT_TRUE(axesAre(*negative.chosen, kNaN, kNaN, 0.97, 2.03));
}

TEST(PlotConfiguration, ANonFiniteScoreNeverReplacesAnother)
{
    // "SCORED all-nan" (the first candidate), "SCORED non-finite" (NaN, 3, Infinity, 3: the first
    // 3) and "SCORED first-nan-later-best" (NaN, -Infinity, -Infinity, 7)
    const DefaultUnitsGuard units;
    const ScoredChoice      allNaN = chooseByScore(2, nanScore);
    EXPECT_EQ(allNaN.asked.size(), 4U);
    EXPECT_EQ(axesOf(*allNaN.chosen), (std::vector{0, 0}));
    EXPECT_TRUE(axesAre(*allNaN.chosen, 0.97, 2.03, kNaN, kNaN));

    const ScoredChoice nonFinite = chooseByScore(2, nonFiniteScore);
    EXPECT_EQ(axesOf(*nonFinite.chosen), (std::vector{0, 1}));
    EXPECT_TRUE(axesAre(*nonFinite.chosen, 0.97, 2.03, kNaN, kNaN));

    const ScoredChoice laterBest = chooseByScore(2, laterBestScore);
    EXPECT_EQ(axesOf(*laterBest.chosen), (std::vector{1, 1}));
    EXPECT_TRUE(axesAre(*laterBest.chosen, kNaN, kNaN, 0.97, 2.03));
}

// Port of PlotConfigurationTest.testFillAutoAxesFallsBackWhenGoodnessIsNaN: a custom expression
// can yield only non-finite values for the auto-axis scorer; the recursive chooser must still
// return a configuration.
TEST(PlotConfiguration, FillAutoAxesFallsBackWhenGoodnessIsNaN)
{
    FlightDataBranch branch("test", {builtin(Id::TYPE_TIME), builtin(Id::TYPE_ALTITUDE)});
    branch.addPoint();
    branch.setValue(builtin(Id::TYPE_TIME), 0.0);
    branch.setValue(builtin(Id::TYPE_ALTITUDE), 10.0);

    NaNGoodnessSimulationPlotConfiguration configuration("NaN goodness test");
    configuration.addPlotDataType(builtin(Id::TYPE_ALTITUDE));

    std::unique_ptr<Config> filled;
    ASSERT_NO_THROW(filled = configuration.fillAutoAxes(branch))
        << "Auto-axis selection should not fail when every candidate scores as NaN";

    ASSERT_NE(filled, nullptr) << "Auto-axis selection must always return a configuration";
    EXPECT_GE(filled->getAxis(0), 0) << "The plotted type should be assigned to a concrete axis";
}

TEST(PlotConfiguration, AConfigurationWithoutRowsGetsNoRanges)
{
    // Java fails ("EMPTY fillAutoAxes: IndexOutOfBoundsException: Index 0 out of bounds for length
    // 0"); QtRocket gives a configuration without ranges.
    SimulationPlotConfiguration config("empty");
    const FlightDataBranch      mainBranch = plotSeries("b", builtin(Id::TYPE_ALTITUDE), {1, 2});
    std::unique_ptr<Config>     filled;
    ASSERT_NO_THROW(filled = config.fillAutoAxes(mainBranch));
    ASSERT_NE(filled, nullptr);
    EXPECT_EQ(filled->getDataCount(), 0U);
    EXPECT_TRUE(axesAre(*filled, kNaN, kNaN, kNaN, kNaN));
}

}  // namespace
