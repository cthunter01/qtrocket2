#include "QtRocket/gui/tree/ComponentLimits.h"

#include <memory>
#include <optional>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/InnerTube.h"
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

/// @p message, or a test failure and an empty message when there is none. (A plain if, which
/// clang-tidy's unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.)
[[nodiscard]] UserPrompts::Message required(const std::optional<UserPrompts::Message>& message)
{
    if (!message.has_value())
    {
        ADD_FAILURE() << "no refusal";
        return {};
    }
    return *message;
}

/// The first sentence of each of @p lines, with its full stop.
[[nodiscard]] QStringList firstSentences(const QStringList& lines)
{
    QStringList sentences;
    for (const QString& line : lines)
    {
        sentences.append(line.left(line.indexOf(QStringLiteral(". ")) + 1));
    }
    return sentences;
}

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

    EXPECT_FALSE(ComponentLimits::refusal(design.document(),
                                          {.components   = DocumentConfig::kMaxComponents - 13,
                                           .assemblies   = DocumentConfig::kMaxAssemblies - 3,
                                           .decalImages  = DocumentConfig::kMaxDecalImages,
                                           .deepestLevel = ComponentHandler::kMaxDepth},
                                          QStringLiteral("Paste"))
                     .has_value());
    EXPECT_FALSE(
        ComponentLimits::refusal(design.document(), {}, QStringLiteral("Paste")).has_value());
}

TEST_F(ComponentLimitsTest, RefusesWhatGoesBeyondABound)
{
    const ProbeDesign design;

    const std::optional<UserPrompts::Message> components = ComponentLimits::refusal(
        design.document(), {.components = DocumentConfig::kMaxComponents - 12},
        QStringLiteral("Paste"));
    const std::optional<UserPrompts::Message> both =
        ComponentLimits::refusal(design.document(),
                                 {.components  = DocumentConfig::kMaxComponents,
                                  .decalImages = DocumentConfig::kMaxDecalImages + 1},
                                 QStringLiteral("Add"));

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
        ComponentLimits::refusal(design.document(), {}, QStringLiteral("Paste")).has_value());
    EXPECT_TRUE(
        ComponentLimits::refusal(design.document(), {.components = 1}, QStringLiteral("Paste"))
            .has_value());
}

// The levels below the Rocket (ComponentHandler's: a stage at 1) and the height of a tree.
TEST_F(ComponentLimitsTest, KnowsTheLevelOfAComponentAndTheHeightOfATree)
{
    const ProbeDesign design;

    EXPECT_EQ(ComponentLimits::levelOf(design.rocket()), 0U);
    EXPECT_EQ(ComponentLimits::levelOf(design.find("Sustainer")), 1U);
    EXPECT_EQ(ComponentLimits::levelOf(design.find("EB")), 4U);
    EXPECT_EQ(ComponentLimits::levelOf(design.find("PBT")), 4U);
    EXPECT_EQ(ComponentLimits::heightOf(design.find("NC")), 0U);
    EXPECT_EQ(ComponentLimits::heightOf(design.find("BT1")), 2U);
    EXPECT_EQ(ComponentLimits::heightOf(design.rocket()), 4U);
    const InnerTube detached;
    EXPECT_EQ(ComponentLimits::levelOf(detached), 0U);
}

TEST_F(ComponentLimitsTest, CountsWhatAPlacementAndAMoveAdd)
{
    const ProbeDesign design;

    // BT1 with IT[EB], CR1 and Fins into the Booster's body tube: EB at level 3 + 2.
    const ComponentLimits::Addition bt1 =
        ComponentLimits::placing(design.find("BT1"), design.find("BBT"));
    // The Booster holds a booster set: two assemblies.
    const ComponentLimits::Addition booster =
        ComponentLimits::placing(design.find("Booster"), design.rocket());
    const ComponentLimits::Addition moved =
        ComponentLimits::moving(design.find("BT1"), design.find("BBT"));

    EXPECT_EQ(bt1.components, 5U);
    EXPECT_EQ(bt1.assemblies, 0U);
    EXPECT_EQ(bt1.decalImages, 0U);
    EXPECT_EQ(bt1.deepestLevel, 5U);
    EXPECT_EQ(booster.components, 4U);
    EXPECT_EQ(booster.assemblies, 2U);
    EXPECT_EQ(booster.deepestLevel, 4U);
    EXPECT_EQ(moved.components, 0U);
    EXPECT_EQ(moved.assemblies, 0U);
    EXPECT_EQ(moved.deepestLevel, 5U);
    ComponentLimits::Addition both = bt1;
    both += booster;
    EXPECT_EQ(both.components, 9U);
    EXPECT_EQ(both.assemblies, 2U);
    EXPECT_EQ(both.deepestLevel, 5U);
}

TEST_F(ComponentLimitsTest, RefusesTooManyAssembliesOrComponentsTooDeep)
{
    const ProbeDesign design;

    // The design has three: Sustainer, Booster and Side boosters.
    const std::optional<UserPrompts::Message> assemblies = ComponentLimits::refusal(
        design.document(), {.components = 1, .assemblies = DocumentConfig::kMaxAssemblies - 2},
        QStringLiteral("Add Stage"));
    const std::optional<UserPrompts::Message> deep = ComponentLimits::refusal(
        design.document(), {.deepestLevel = ComponentHandler::kMaxDepth + 1},
        QStringLiteral("Move component"));
    const std::optional<UserPrompts::Message> all =
        ComponentLimits::refusal(design.document(),
                                 {.components   = DocumentConfig::kMaxComponents,
                                  .assemblies   = DocumentConfig::kMaxAssemblies,
                                  .decalImages  = DocumentConfig::kMaxDecalImages + 1,
                                  .deepestLevel = ComponentHandler::kMaxDepth + 2},
                                 QStringLiteral("Paste components"));

    EXPECT_EQ(required(assemblies).severity, UserPrompts::Severity::WARNING);
    EXPECT_EQ(required(assemblies).title, QStringLiteral("Add Stage"));
    EXPECT_EQ(required(assemblies).lines,
              QStringList{QStringLiteral(
                  "The rocket would have 101 stages, booster sets and pod sets. QtRocket opens at "
                  "most 100 of them in a design and would ignore the ones beyond, with everything "
                  "in them, when it opens this design again.")});
    EXPECT_EQ(required(deep).title, QStringLiteral("Move component"));
    EXPECT_EQ(required(deep).lines,
              QStringList{QStringLiteral(
                  "Components would stand 101 levels below the rocket. QtRocket opens components "
                  "at most 100 levels deep and would ignore the deeper ones, with everything in "
                  "them, when it opens this design again.")});
    // The saver's order: the components, the depth, the assemblies, the images.
    EXPECT_EQ(firstSentences(required(all).lines),
              QStringList({QStringLiteral("The rocket would have 1013 components."),
                           QStringLiteral("Components would stand 102 levels below the rocket."),
                           QStringLiteral("The rocket would have 103 stages, booster sets and pod "
                                          "sets."),
                           QStringLiteral("The design would have 1001 decal images.")}));
}

// Up to the bounds: 100 assemblies, a component at level 100.
TEST_F(ComponentLimitsTest, LetsAssembliesAndLevelsThroughUpToTheBounds)
{
    const ProbeDesign design;

    EXPECT_FALSE(ComponentLimits::refusal(
                     design.document(),
                     {.components = 1, .assemblies = DocumentConfig::kMaxAssemblies - 3},
                     QStringLiteral("Add Stage"))
                     .has_value());
    EXPECT_FALSE(ComponentLimits::refusal(design.document(),
                                          {.deepestLevel = ComponentHandler::kMaxDepth},
                                          QStringLiteral("Move component"))
                     .has_value());
}

// A design with more assemblies than the bound already: adding none is no refusal.
TEST_F(ComponentLimitsTest, RefusesNoAdditionWithoutAssembliesBeyondTheAssemblyBound)
{
    const ProbeDesign design;
    Rocket&           rocket = design.rocket();
    rocket.freeze();
    while (DocumentConfig::assemblyCount(rocket) <= DocumentConfig::kMaxAssemblies)
    {
        rocket.addChild(std::make_unique<AxialStage>());
    }
    rocket.thaw();

    EXPECT_FALSE(ComponentLimits::refusal(
                     design.document(),
                     ComponentLimits::placing(design.find("NC"), design.find("Sustainer")),
                     QStringLiteral("Paste"))
                     .has_value());
    EXPECT_TRUE(ComponentLimits::refusal(design.document(), {.components = 1, .assemblies = 1},
                                         QStringLiteral("Paste"))
                    .has_value());
}

}  // namespace

}  // namespace QtRocket::Gui
