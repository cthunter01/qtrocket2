#include "QtRocket/mass/CMAnalysisEntry.h"

#include <cmath>
#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::CMAnalysisEntry;
using QtRocket::Coordinate;
using QtRocket::Motor;

TEST(CMAnalysisEntry, ComponentRowStartsWithoutMass)
{
    BodyTube component;
    component.setName("Body");
    const CMAnalysisEntry entry{component};

    EXPECT_EQ(entry.name, "Body");
    EXPECT_EQ(entry.getComponent(), &component);
    EXPECT_EQ(entry.getMotor(), nullptr);
    EXPECT_TRUE(std::isnan(entry.eachMass));
    EXPECT_TRUE(entry.totalCM.isNaN());
}

TEST(CMAnalysisEntry, ComponentRowWithoutNameUsesTheComponentName)
{
    const BodyTube        component;
    const CMAnalysisEntry entry{component};
    EXPECT_EQ(entry.name, component.getName());
    EXPECT_EQ(entry.name, "Body Tube");
}

TEST(CMAnalysisEntry, MotorRowIsNamedAfterTheDesignation)
{
    const std::shared_ptr<const Motor> motor = QtRocket::Test::motorG77();
    const CMAnalysisEntry              entry{motor};

    EXPECT_EQ(entry.name, "G77");
    EXPECT_EQ(entry.getMotor(), motor.get());
    EXPECT_EQ(entry.getComponent(), nullptr);
    EXPECT_TRUE(std::isnan(entry.eachMass));
    EXPECT_TRUE(entry.totalCM.isNaN());
}

TEST(CMAnalysisEntry, NullMotorIsABug)
{
    EXPECT_THROW(static_cast<void>(CMAnalysisEntry{std::shared_ptr<const Motor>{}}), BugError);
}

TEST(CMAnalysisEntry, KeysAreJavasHashCodes)
{
    const BodyTube component;
    EXPECT_EQ(CMAnalysisEntry::keyOf(component), component.hashCode());

    const std::shared_ptr<const Motor> motor = QtRocket::Test::motorM1350();
    EXPECT_EQ(CMAnalysisEntry::keyOf(*motor), QtRocket::Strings::javaHashCode("M1350"));
    // "M1350".hashCode() in Java.
    EXPECT_EQ(CMAnalysisEntry::keyOf(*motor), 72621578);
}

TEST(CMAnalysisEntry, EachMassIsTheFirstValueOnly)
{
    const BodyTube  component;
    CMAnalysisEntry entry{component};

    entry.updateEachMass(0.25);
    EXPECT_EQ(entry.eachMass, 0.25);
    entry.updateEachMass(0.5);
    EXPECT_EQ(entry.eachMass, 0.25);
}

TEST(CMAnalysisEntry, AverageCMStartsAtTheFirstValueThenAverages)
{
    const BodyTube  component;
    CMAnalysisEntry entry{component};

    entry.updateAverageCM(Coordinate{1.0, 0.0, 0.0, 2.0});
    EXPECT_TRUE(entry.totalCM.exactlyEquals(Coordinate{1.0, 0.0, 0.0, 2.0}));

    entry.updateAverageCM(Coordinate{4.0, 3.0, 0.0, 1.0});
    // (1 * 2 + 4 * 1) / 3 = 2, (0 * 2 + 3 * 1) / 3 = 1, weight 3.
    EXPECT_DOUBLE_EQ(entry.totalCM.x, 2.0);
    EXPECT_DOUBLE_EQ(entry.totalCM.y, 1.0);
    EXPECT_DOUBLE_EQ(entry.totalCM.z, 0.0);
    EXPECT_DOUBLE_EQ(entry.totalCM.weight, 3.0);
}

TEST(CMAnalysisEntry, AverageCMRestartsFromAPartlyNaNTotal)
{
    const BodyTube  component;
    CMAnalysisEntry entry{component};
    entry.totalCM = Coordinate{1.0, std::nan(""), 0.0, 1.0};

    entry.updateAverageCM(Coordinate{5.0, 0.0, 0.0, 1.0});
    EXPECT_TRUE(entry.totalCM.exactlyEquals(Coordinate{5.0, 0.0, 0.0, 1.0}));
}

TEST(CMAnalysisEntry, AssemblyMassIsTheAggregatePerInstance)
{
    const BodyTube  component;
    CMAnalysisEntry entry{component};

    // The structure pass of a two-instance assembly...
    entry.updateAssemblyMass(Coordinate{1.0, 0.0, 0.0, 4.0}, 2);
    EXPECT_DOUBLE_EQ(entry.eachMass, 2.0);
    EXPECT_DOUBLE_EQ(entry.totalCM.weight, 4.0);

    // ...then a motor pass adds its clusters: eachMass follows the total, unlike
    // updateEachMass().
    entry.updateAssemblyMass(Coordinate{3.0, 0.0, 0.0, 2.0}, 2);
    EXPECT_DOUBLE_EQ(entry.totalCM.weight, 6.0);
    EXPECT_DOUBLE_EQ(entry.totalCM.x, ((1.0 * 4.0) + (3.0 * 2.0)) / 6.0);
    EXPECT_DOUBLE_EQ(entry.eachMass, 3.0);
}

}  // namespace
