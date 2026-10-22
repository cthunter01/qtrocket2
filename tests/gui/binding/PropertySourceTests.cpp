// The accessors of a property, built from lambdas (Java's DoubleModel and its kin find them by
// reflection: get<Name> required, set<Name> optional, is<Name>Automatic and set<Name>Automatic
// both or none, is<Name>Enabled optional).

#include "QtRocket/gui/binding/PropertySource.h"

#include <array>
#include <tuple>

#include <gtest/gtest.h>

#include "BindingTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

using PropertySourceTest = Test::GuiTest;

/// Accessors of a body tube's length, for the accessor checks.
[[nodiscard]] double tubeLength(const BodyTube& tube)
{
    return tube.getLength();
}

[[nodiscard]] Result<double> checkedTubeLength(const BodyTube& tube)
{
    return tube.getLength();
}

[[nodiscard]] Result<double> noTubeLength(const BodyTube& /*tube*/)
{
    return fail(ErrorCode::INVALID_ARGUMENT, "no value here");
}

[[nodiscard]] bool alwaysAutomatic(const BodyTube& /*tube*/)
{
    return true;
}

void ignoreAutomatic(BodyTube& /*tube*/, bool /*automatic*/) { }
using Test::bodyTubeLength;
using Test::bodyTubeOuterRadius;
using Test::TwoTubeRocket;

}  // namespace

TEST_F(PropertySourceTest, AGetterReadsTheMainComponent)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 1> others{design.tubeB};
    const ComponentSelection              selection(*design.tubeA, others);
    const DoubleSource                    length = bodyTubeLength();
    const Result<double>                  value  = length.get(selection);
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 0.3);
    EXPECT_TRUE(length.isWritable());
    EXPECT_FALSE(length.isAutomaticAvailable());
    EXPECT_FALSE(length.isAutomatic(selection));
    EXPECT_TRUE(length.isEnabled(selection));
}

TEST_F(PropertySourceTest, ASetterReachesEverySelectedComponentOfItsClass)
{
    const TwoTubeRocket                   design;
    const std::array<RocketComponent*, 2> others{design.nose, design.tubeB};
    ComponentSelection                    selection(*design.tubeA, others);
    bodyTubeLength().set(selection, 0.42);
    EXPECT_EQ(design.tubeA->getLength(), 0.42);
    EXPECT_EQ(design.tubeB->getLength(), 0.42);

    const DoubleSource radius = bodyTubeOuterRadius();
    ASSERT_TRUE(radius.isAutomaticAvailable());
    radius.setAutomatic(selection, true);
    EXPECT_TRUE(design.tubeA->isOuterRadiusAutomatic());
    EXPECT_TRUE(design.tubeB->isOuterRadiusAutomatic());
    EXPECT_TRUE(radius.isAutomatic(selection));
}

TEST_F(PropertySourceTest, GetCheckedGivesTheProblemAsAnError)
{
    const TwoTubeRocket      design;
    const ComponentSelection selection(*design.tubeA);
    const DoubleSource       failing = DoubleSource::of<BodyTube>({.getChecked = noTubeLength});
    const Result<double>     value   = failing.get(selection);
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(value.error().message, "no value here");
    EXPECT_FALSE(failing.isWritable());
    const Result<double> checked =
        DoubleSource::of<BodyTube>({.getChecked = checkedTubeLength}).get(selection);
    EXPECT_EQ(checked.value_or(0.0), 0.3);
}

TEST_F(PropertySourceTest, GetValueIsForSourcesWithoutAProblem)
{
    // The bindings that show no problem text (all but DoubleBinding) read with getValue().
    const TwoTubeRocket      design;
    const ComponentSelection selection(*design.tubeA);
    EXPECT_EQ(bodyTubeLength().getValue(selection), 0.3);
    EXPECT_EQ(DoubleSource::of<BodyTube>({.getChecked = checkedTubeLength}).getValue(selection),
              0.3);
    EXPECT_THROW(
        std::ignore = DoubleSource::of<BodyTube>({.getChecked = noTubeLength}).getValue(selection),
        BugError);
}

TEST_F(PropertySourceTest, TheAccessorsMustBeComplete)
{
    using Accessors = DoubleSource::Accessors<BodyTube>;
    // Neither or both getters.
    EXPECT_THROW(std::ignore = DoubleSource::of<BodyTube>(Accessors{}), BugError);
    EXPECT_THROW(std::ignore = DoubleSource::of<BodyTube>(
                     {.get = tubeLength, .getChecked = checkedTubeLength}),
                 BugError);
    // One automatic accessor alone (Java uses them only when both exist).
    EXPECT_THROW(std::ignore = DoubleSource::of<BodyTube>(
                     {.get = tubeLength, .isAutomatic = alwaysAutomatic}),
                 BugError);
    EXPECT_THROW(std::ignore = DoubleSource::of<BodyTube>(
                     {.get = tubeLength, .setAutomatic = ignoreAutomatic}),
                 BugError);
    EXPECT_NO_THROW(
        std::ignore = DoubleSource::of<BodyTube>(
            {.get = tubeLength, .isAutomatic = alwaysAutomatic, .setAutomatic = ignoreAutomatic}));
}

TEST_F(PropertySourceTest, AReadOnlySourceOrOneWithoutAnAutomaticStateCannotSetIt)
{
    const TwoTubeRocket design;
    ComponentSelection  selection(*design.tubeA);
    const DoubleSource  readOnly = DoubleSource::of<BodyTube>({.get = tubeLength});
    EXPECT_FALSE(readOnly.isWritable());
    EXPECT_THROW(readOnly.set(selection, 1.0), BugError);
    EXPECT_THROW(readOnly.setAutomatic(selection, true), BugError);
    EXPECT_EQ(design.tubeA->getLength(), 0.3);
}

TEST_F(PropertySourceTest, TheMainComponentMustBeOfTheSourcesClass)
{
    const TwoTubeRocket      design;
    const ComponentSelection selection(*design.nose);
    EXPECT_THROW(std::ignore = bodyTubeLength().get(selection), BugError);
    EXPECT_THROW(std::ignore = bodyTubeOuterRadius().isAutomatic(selection), BugError);
}

}  // namespace QtRocket::Gui
