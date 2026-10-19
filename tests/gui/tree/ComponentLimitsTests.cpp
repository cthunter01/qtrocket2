#include "QtRocket/gui/tree/ComponentLimits.h"

#include <memory>
#include <optional>

#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "tree/ProbeDesign.h"

namespace QtRocket::Gui
{

namespace
{

using Test::ProbeDesign;
using ComponentLimitsTest = Test::GuiTest;

TEST_F(ComponentLimitsTest, CountsATreeWithItsRoot)
{
    const ProbeDesign design;

    EXPECT_EQ(ComponentLimits::treeSize(design.find("BT1")), 5U);
    EXPECT_EQ(ComponentLimits::treeSize(design.find("NC")), 1U);
    // The Rocket counted too, where DocumentConfig::componentCount() leaves it out.
    EXPECT_EQ(ComponentLimits::treeSize(design.rocket()), 14U);
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), 13U);
}

TEST_F(ComponentLimitsTest, LetsEverythingThroughUpToTheBounds)
{
    const ProbeDesign design;

    EXPECT_FALSE(ComponentLimits::refusal(design.document(), DocumentConfig::kMaxComponents - 13,
                                          DocumentConfig::kMaxDecalImages, QStringLiteral("Paste"))
                     .has_value());
    EXPECT_FALSE(
        ComponentLimits::refusal(design.document(), 0, 0, QStringLiteral("Paste")).has_value());
}

TEST_F(ComponentLimitsTest, RefusesWhatGoesBeyondABound)
{
    const ProbeDesign design;

    const std::optional<UserPrompts::Message> components = ComponentLimits::refusal(
        design.document(), DocumentConfig::kMaxComponents - 12, 0, QStringLiteral("Paste"));
    const std::optional<UserPrompts::Message> both =
        ComponentLimits::refusal(design.document(), DocumentConfig::kMaxComponents,
                                 DocumentConfig::kMaxDecalImages + 1, QStringLiteral("Add"));

    // A plain if: clang-tidy's unchecked-optional-access check cannot see through ASSERT_TRUE.
    if (!components.has_value() || !both.has_value())
    {
        FAIL() << "no refusal";
    }
    EXPECT_EQ(components->severity, UserPrompts::Severity::WARNING);
    EXPECT_EQ(components->title, QStringLiteral("Paste"));
    EXPECT_EQ(
        components->lines,
        QStringList{QStringLiteral(
            "The rocket would have 1001 components. QtRocket opens at most 1000 components "
            "of a design and would ignore the ones beyond when it opens this design again.")});
    EXPECT_EQ(both->title, QStringLiteral("Add"));
    EXPECT_EQ(both->lines,
              QStringList({QStringLiteral("The rocket would have 1013 components. QtRocket opens "
                                          "at most 1000 components of a design and would ignore "
                                          "the ones beyond when it opens this design again."),
                           QStringLiteral("The design would have 1001 decal images. QtRocket "
                                          "opens at most 1000 decal images of a design and would "
                                          "ignore the decals of the ones beyond when it opens "
                                          "this design again.")}));
}

// A design beyond a bound already (no file gives one, but code can make one): adding nothing
// is no refusal, adding anything is.
TEST_F(ComponentLimitsTest, RefusesEveryAdditionBeyondTheBound)
{
    const ProbeDesign design;
    Rocket&           rocket = design.rocket();
    RocketComponent&  tube   = design.find("BT2");
    rocket.freeze();
    while (DocumentConfig::componentCount(rocket) <= DocumentConfig::kMaxComponents)
    {
        tube.addChild(std::make_unique<MassComponent>());
    }
    rocket.thaw();

    EXPECT_FALSE(
        ComponentLimits::refusal(design.document(), 0, 0, QStringLiteral("Paste")).has_value());
    EXPECT_TRUE(
        ComponentLimits::refusal(design.document(), 1, 0, QStringLiteral("Paste")).has_value());
}

}  // namespace

}  // namespace QtRocket::Gui
