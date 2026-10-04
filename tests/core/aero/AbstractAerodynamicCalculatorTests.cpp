#include "QtRocket/aero/AbstractAerodynamicCalculator.h"

#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <numbers>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/aero/AerodynamicCalculator.h"
#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::AbstractAerodynamicCalculator;
using QtRocket::AerodynamicCalculator;
using QtRocket::AerodynamicForces;
using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::ForceMap;
using QtRocket::ModId;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::WarningSet;

constexpr double kPi = std::numbers::pi;

/// A calculator whose CP is a function of theta, counting its calls and cache voids.
class ThetaCalculator final : public AbstractAerodynamicCalculator
{
public:
    using CpFunction = std::function<Coordinate(double)>;

    explicit ThetaCalculator(CpFunction cpForTheta) : m_cpForTheta(std::move(cpForTheta)) { }

    [[nodiscard]] double getStallAngle() const override { return 0.3; }

    [[nodiscard]] Coordinate getCP(const FlightConfiguration& configuration,
                                   const FlightConditions&    conditions,
                                   WarningSet*                warnings) override
    {
        checkCache(configuration);
        ++m_cpCalls;
        actualWarnings(warnings).add("getCP");
        return m_cpForTheta(conditions.getTheta());
    }

    [[nodiscard]] AerodynamicForces getAerodynamicForces(const FlightConfiguration& configuration,
                                                         const FlightConditions& /*conditions*/,
                                                         WarningSet* /*warnings*/) override
    {
        checkCache(configuration);
        return AerodynamicForces{};
    }

    [[nodiscard]] ForceMap getForceAnalysis(const FlightConfiguration& configuration,
                                            const FlightConditions& /*conditions*/,
                                            WarningSet* /*warnings*/) override
    {
        checkCache(configuration);
        return ForceMap{};
    }

    [[nodiscard]] std::unique_ptr<AerodynamicCalculator> newInstance() const override
    {
        return std::make_unique<ThetaCalculator>(m_cpForTheta);
    }

    void checkGeometry(const FlightConfiguration& /*configuration*/,
                       const RocketComponent& /*component*/, WarningSet* /*warnings*/) override
    {
    }

    [[nodiscard]] ModId modId() const override { return ModId::zero(); }

    [[nodiscard]] int cpCalls() const noexcept { return m_cpCalls; }
    [[nodiscard]] int voidCount() const noexcept { return m_voidCount; }

    using AbstractAerodynamicCalculator::ignoreWarningSet;

protected:
    void voidAerodynamicCache() override
    {
        ++m_voidCount;
        AbstractAerodynamicCalculator::voidAerodynamicCache();
    }

private:
    CpFunction m_cpForTheta;
    int        m_cpCalls{0};
    int        m_voidCount{0};
};

/// A rocket with a stage and a body tube (0.5 m, radius 0.025 m), events on, and a configuration
/// of it.
struct TestRocket
{
    Rocket              rocket;
    AxialStage&         stage = rocket.addChild(std::make_unique<AxialStage>());
    BodyTube&           body  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.025));
    FlightConfiguration config{rocket};

    TestRocket() { rocket.enableEvents(); }
};

static_assert(QtRocket::Monitorable<ThetaCalculator>);

TEST(AbstractAerodynamicCalculator, WorstCpIsTheForemostOverEveryWindDirection)
{
    const TestRocket r;
    ThetaCalculator  calculator{
        [](double theta) { return Coordinate{1 + (0.25 * std::cos(theta)), 0, 0, 1}; }};
    FlightConditions conditions;
    conditions.setTheta(0.4);
    int                                        changes = 0;
    const QtRocket::Signal<>::ScopedConnection connection{
        conditions.changed().connect([&changes] { ++changes; })};

    WarningSet       warnings;
    const Coordinate worst = calculator.getWorstCP(r.config, conditions, &warnings);

    EXPECT_EQ(AbstractAerodynamicCalculator::kDivisions, 360);
    EXPECT_EQ(calculator.cpCalls(), 360);
    EXPECT_NEAR(worst.x, 0.75, 1e-15);
    EXPECT_EQ(worst.weight, 1);
    // The theta that gave it (2 pi * 180 / 360) is set in the conditions; nothing else changes.
    EXPECT_EQ(conditions.getTheta(), 2 * kPi * 180 / 360);
    EXPECT_EQ(changes, 1);
}

TEST(AbstractAerodynamicCalculator, WorstCpWorksOnACopyOfTheConditions)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.5, 0, 0, 1}; }};
    FlightConditions conditions;
    conditions.setMach(0.6);
    conditions.setTheta(0.4);
    calculator.getWorstCP(r.config, conditions,
                          nullptr);  // for the theta alone, as OpenRocket does
    EXPECT_EQ(conditions.getMach(), 0.6);
    EXPECT_EQ(conditions.getTheta(), 0.0);  // the first of equal CPs
}

TEST(AbstractAerodynamicCalculator, WorstCpPassesTheWarningsOn)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.5, 0, 0, 1}; }};
    FlightConditions conditions;
    WarningSet       warnings;
    calculator.getWorstCP(r.config, conditions, &warnings);
    EXPECT_EQ(warnings.size(), 1U);  // the warnings went to the caller's set (one kind)
    EXPECT_TRUE(calculator.ignoreWarningSet().empty());
}

TEST(AbstractAerodynamicCalculator, WorstCpSkipsCpsWithoutNormalForce)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double theta) {
        // Only the directions from 1.5 rad on have a CNa above MathUtil::kEpsilon.
        return Coordinate{theta, 0, 0, theta >= 1.5 ? 1.0 : 1e-9};
    }};
    FlightConditions conditions;
    const Coordinate worst = calculator.getWorstCP(r.config, conditions, nullptr);
    const double     first = 2 * kPi * 86 / 360;  // the first direction from 1.5 rad
    EXPECT_EQ(worst.x, first);
    EXPECT_EQ(conditions.getTheta(), first);
    // Without a set, the warnings go to the calculator's own.
    EXPECT_FALSE(calculator.ignoreWarningSet().empty());
}

TEST(AbstractAerodynamicCalculator, WorstCpKeepsTheFirstOfEqualCps)
{
    const TestRocket r;
    ThetaCalculator  calculator{
        [](double theta) { return Coordinate{theta < 1 ? 2.0 : 0.5, 0, 0, 1}; }};
    FlightConditions conditions;
    const Coordinate worst = calculator.getWorstCP(r.config, conditions, nullptr);
    EXPECT_EQ(worst.x, 0.5);
    EXPECT_EQ(conditions.getTheta(), 2 * kPi * 58 / 360);  // the first direction from 1 rad
}

TEST(AbstractAerodynamicCalculator, WorstCpWithoutAnyNormalForceIsFarAft)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.4}; }};
    FlightConditions conditions;
    conditions.setTheta(1.0);
    const Coordinate worst = calculator.getWorstCP(r.config, conditions, nullptr);
    EXPECT_TRUE(worst.exactlyEquals(Coordinate{std::numeric_limits<double>::max()}));
    EXPECT_EQ(conditions.getTheta(), 0.0);
}

TEST(AbstractAerodynamicCalculator, CacheIsVoidedOnTheFirstCallAndAnAerodynamicChange)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.4, 0, 0, 1}; }};
    FlightConditions conditions;

    (void)calculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 1);  // the first call always voids
    (void)calculator.getCP(r.config, conditions, nullptr);
    (void)calculator.getAerodynamicForces(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 1);

    r.body.setFilled(true);  // a mass change leaves the aerodynamics alone
    (void)calculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 1);

    r.body.setLength(0.7);  // an aerodynamic change
    (void)calculator.getForceAnalysis(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 2);
}

TEST(AbstractAerodynamicCalculator, CacheIsVoidedWhenTheTreeOrTheRocketChanges)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.4, 0, 0, 1}; }};
    FlightConditions conditions;
    (void)calculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 1);

    r.stage.addChild(std::make_unique<BodyTube>(0.1, 0.025));  // a tree change
    (void)calculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 2);
    (void)calculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 2);

    // Another rocket has other ids.
    const TestRocket other;
    (void)calculator.getCP(other.config, conditions, nullptr);
    EXPECT_EQ(calculator.voidCount(), 3);
}

TEST(AbstractAerodynamicCalculator, NewInstanceStartsWithAnEmptyCache)
{
    const TestRocket r;
    ThetaCalculator  calculator{[](double /*theta*/) { return Coordinate{0.4, 0, 0, 1}; }};
    FlightConditions conditions;
    (void)calculator.getCP(r.config, conditions, nullptr);

    const std::unique_ptr<AerodynamicCalculator> fresh = calculator.newInstance();
    auto& freshCalculator                              = dynamic_cast<ThetaCalculator&>(*fresh);
    (void)freshCalculator.getCP(r.config, conditions, nullptr);
    EXPECT_EQ(freshCalculator.voidCount(), 1);
    EXPECT_EQ(fresh->getStallAngle(), 0.3);
    EXPECT_EQ(fresh->modId(), ModId::zero());
}

}  // namespace
