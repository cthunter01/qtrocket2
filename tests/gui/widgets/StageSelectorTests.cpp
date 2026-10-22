#include "QtRocket/gui/widgets/StageSelector.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QChar>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QLayout>
#include <QMargins>
#include <QPointer>
#include <QRect>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringList>
#include <QTest>
#include <QToolButton>
#include <Qt>
#include <QtContainerFwd>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"
#include "RibbonTestSupport.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using StageSelectorTest = Test::GuiTest;
using Test::ChangeEventTypes;

/// Deletes what was handed to deleteLater() (the buttons a selector no longer needs).
void deleteLaterObjects()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// The stages of @p rocket that are active in its selected configuration, as '1' and '0' by
/// stage number.
[[nodiscard]] std::string activeStages(const Rocket& rocket)
{
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    std::string                flags;
    for (std::size_t n = 0; n < rocket.getStageCount(); ++n)
    {
        flags += configuration.isStageActive(static_cast<int>(n)) ? '1' : '0';
    }
    return flags;
}

/// The texts of @p selector's buttons, separated by '|'.
[[nodiscard]] QString buttonTexts(const StageSelector& selector)
{
    QStringList texts;
    for (std::size_t i = 0; i < selector.buttonCount(); ++i)
    {
        texts.append(selector.button(i)->text());
    }
    return texts.join(QLatin1Char('|'));
}

/// Whether every button of @p selector is a checkable button with its text beside the icon and
/// at least as high as every other one's preferred height (Java's size group).
void expectToggleButtonsOfEqualHeight(const StageSelector& selector)
{
    int highest = 0;
    for (std::size_t i = 0; i < selector.buttonCount(); ++i)
    {
        highest = std::max(highest, selector.button(i)->sizeHint().height());
    }
    for (std::size_t i = 0; i < selector.buttonCount(); ++i)
    {
        const QToolButton& button = *selector.button(i);
        EXPECT_TRUE(button.isCheckable()) << i;
        EXPECT_EQ(button.toolButtonStyle(), Qt::ToolButtonTextBesideIcon) << i;
        EXPECT_EQ(button.minimumHeight(), highest) << i;
    }
}

/// A rocket of two stages, each with a body tube, its events left disabled (a rocket that is
/// not a document's: its changes reach no listener).
struct SilentTwoStages
{
    SilentTwoStages()
    {
        AxialStage& sustainer = rocket.addChild(std::make_unique<AxialStage>());
        sustainer.setName("Sustainer");
        sustainer.addChild(std::make_unique<BodyTube>());
        AxialStage& booster = rocket.addChild(std::make_unique<AxialStage>());
        booster.setName("Booster");
        booster.addChild(std::make_unique<BodyTube>());
    }

    Rocket rocket;
};

TEST_F(StageSelectorTest, MakesAToggleButtonPerStageInTreeOrder)
{
    // The payload, the core and the booster set; the boosters are a ParallelStage.
    const QtRocket::Test::TestFalcon9Heavy falcon;
    const StageSelector                    selector(*falcon.rocket, nullptr);
    ASSERT_EQ(selector.buttonCount(), 3U);
    EXPECT_EQ(selector.stageId(0), falcon.payloadStage->getId());
    EXPECT_EQ(selector.stageId(1), falcon.coreStage->getId());
    EXPECT_EQ(selector.stageId(2), falcon.boosterStage->getId());
    EXPECT_EQ(buttonTexts(selector),
              QStringLiteral("Payload Fairing Stage|Core Stage|Booster Stage"));
    expectToggleButtonsOfEqualHeight(selector);
    // MigLayout "insets 0".
    ASSERT_NE(selector.layout(), nullptr);
    EXPECT_EQ(selector.layout()->contentsMargins(), QMargins());

    // A pod set is an assembly, but not a stage.
    const QtRocket::Test::TestEstesAlphaIIIWithPods pods;
    const StageSelector                             single(*pods.rocket, nullptr);
    EXPECT_EQ(single.buttonCount(), 1U);
}

/// The geometries of @p selector's buttons, in its coordinates.
[[nodiscard]] std::vector<QRect> buttonGeometries(const StageSelector& selector)
{
    std::vector<QRect> geometries;
    geometries.reserve(selector.buttonCount());
    for (std::size_t i = 0; i < selector.buttonCount(); ++i)
    {
        geometries.push_back(selector.button(i)->geometry());
    }
    return geometries;
}

TEST_F(StageSelectorTest, KeepsItsButtonsPackedAtTheLeft)
{
    // Java's ribbon puts the selector in a "pushx" cell: the cell takes the room to spare, the
    // selector keeps its preferred size, and its buttons stand together ("gap rel!").
    const QtRocket::Test::TestFalcon9Heavy falcon;
    StageSelector                          selector(*falcon.rocket, nullptr);
    ASSERT_EQ(selector.buttonCount(), 3U);
    EXPECT_EQ(selector.sizePolicy().horizontalPolicy(), QSizePolicy::Maximum);
    const QSize hint = selector.sizeHint();
    selector.resize(hint);
    selector.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&selector));
    const std::vector<QRect> packed = buttonGeometries(selector);
    EXPECT_EQ(packed.front().x(), 0);
    EXPECT_LT(packed.back().right(), hint.width());

    // Three times as wide: the buttons stay where they are, with the room to their right.
    selector.resize(3 * hint.width(), hint.height());
    QCoreApplication::processEvents();
    EXPECT_EQ(selector.width(), 3 * hint.width());
    EXPECT_EQ(buttonGeometries(selector), packed);
}

TEST_F(StageSelectorTest, AClickTogglesTheStageAndFiresTheToggleEvent)
{
    const QtRocket::Test::TestSimple2Stage simple;
    Rocket&                                rocket = *simple.rocket;
    const StageSelector                    selector(rocket, nullptr);
    const ChangeEventTypes                 events(rocket);
    ASSERT_EQ(activeStages(rocket), "11");

    selector.button(1)->click();
    EXPECT_EQ(activeStages(rocket), "10");
    ASSERT_EQ(events.types().size(), 1U);
    EXPECT_EQ(events.types()[0], StageSelector::kToggleChange);
    EXPECT_EQ(StageSelector::kToggleChange, 550);
    EXPECT_TRUE(events.allFromRocket());
    // The buttons followed the event: the booster is inactive, the sustainer the last active.
    EXPECT_FALSE(selector.button(1)->isChecked());
    EXPECT_FALSE(selector.button(0)->isEnabled());
    EXPECT_TRUE(selector.button(0)->font().italic());
}

TEST_F(StageSelectorTest, AToggleAddsNoUndoPosition)
{
    QtRocket::Test::TestSimple2Stage          simple;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(simple.rocket));
    Rocket&                              rocket = document->getRocket();
    const StageSelector                  selector(rocket, nullptr);
    const OpenRocketDocument::UndoDetail before = document->getUndoDetail();
    ASSERT_FALSE(document->isUndoAvailable());

    selector.button(0)->click();
    const OpenRocketDocument::UndoDetail after = document->getUndoDetail();
    EXPECT_EQ(after.descriptions, before.descriptions);
    EXPECT_EQ(after.position, before.position);
    EXPECT_EQ(after.nextDescription, before.nextDescription);
    // The change makes the document differ from its state, as in OpenRocket: an undo is
    // available (it brings the stage back), and it has no description.
    EXPECT_FALSE(after.clean);
    EXPECT_TRUE(document->isUndoAvailable());
    EXPECT_EQ(document->getUndoDescription(), std::nullopt);
    EXPECT_EQ(activeStages(rocket), "01");
}

TEST_F(StageSelectorTest, KeepsWorkingWhenAnUndoReplacesTheStages)
{
    QtRocket::Test::TestSimple2Stage          simple;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(simple.rocket));
    Rocket&             rocket = document->getRocket();
    const StageSelector selector(rocket, nullptr);
    const Uuid          boosterId = selector.stageId(1);

    selector.button(1)->click();
    ASSERT_EQ(activeStages(rocket), "10");
    // The undo loads the state before the toggle: every stage object is a new one.
    const AxialStage* oldBooster = rocket.getStage(boosterId);
    document->undo();
    EXPECT_EQ(activeStages(rocket), "11");
    ASSERT_NE(rocket.getStage(boosterId), nullptr);
    EXPECT_NE(rocket.getStage(boosterId), oldBooster);
    EXPECT_TRUE(selector.button(1)->isChecked());
    EXPECT_TRUE(selector.button(0)->isEnabled());

    // The buttons find the new objects by id.
    selector.button(1)->click();
    EXPECT_EQ(activeStages(rocket), "10");
    EXPECT_FALSE(selector.button(1)->isChecked());
}

TEST_F(StageSelectorTest, FollowsTheRocketsChanges)
{
    const QtRocket::Test::TestSimple2Stage simple;
    Rocket&                                rocket = *simple.rocket;
    const StageSelector                    selector(rocket, nullptr);

    simple.boosterStage->setName("First stage");
    EXPECT_EQ(buttonTexts(selector), QStringLiteral("Sustainer Stage|First stage"));

    // A third stage, with a body tube.
    AxialStage& third = rocket.addChild(std::make_unique<AxialStage>());
    third.setName("Third");
    EXPECT_EQ(selector.buttonCount(), 3U);
    // Without children it is inactive and cannot be toggled.
    EXPECT_FALSE(selector.button(2)->isEnabled());
    EXPECT_FALSE(selector.button(2)->isChecked());
    third.addChild(std::make_unique<BodyTube>());
    EXPECT_TRUE(selector.button(2)->isEnabled());
    EXPECT_TRUE(selector.button(2)->isChecked());

    // Removed again: the button goes once its events are delivered.
    const QPointer<QToolButton>            surplus = selector.button(2);
    const std::unique_ptr<RocketComponent> removed = rocket.removeChild(&third);
    EXPECT_EQ(selector.buttonCount(), 2U);
    ASSERT_FALSE(surplus.isNull());
    EXPECT_EQ(selector.layout()->indexOf(surplus.data()), -1);
    deleteLaterObjects();
    EXPECT_TRUE(surplus.isNull());
}

TEST_F(StageSelectorTest, ShowsTheSelectedConfiguration)
{
    const QtRocket::Test::TestBeta beta;
    Rocket&                        rocket = *beta.rocket;
    const StageSelector            selector(rocket, nullptr);
    ASSERT_EQ(activeStages(rocket), "11");

    // Another configuration in which only the sustainer flies.
    const FlightConfigurationId other = QtRocket::Test::testFcid(2);
    rocket.getFlightConfiguration(other).setOnlyStage(0);
    rocket.setSelectedConfiguration(other);
    EXPECT_TRUE(selector.button(0)->isChecked());
    EXPECT_FALSE(selector.button(1)->isChecked());
    EXPECT_FALSE(selector.button(0)->isEnabled());
    EXPECT_FALSE(selector.button(1)->icon().isNull());
}

TEST_F(StageSelectorTest, ShowsAnAmpersandOfAStageNameAsItself)
{
    const QtRocket::Test::TestSimple2Stage simple;
    simple.boosterStage->setName("Booster & Sustainer");
    const StageSelector selector(*simple.rocket, nullptr);
    // A QToolButton takes '&' for a mnemonic; "&&" shows one '&'.
    EXPECT_EQ(selector.button(1)->text(), QStringLiteral("Booster && Sustainer"));
}

TEST_F(StageSelectorTest, AClickOnAStageThatIsGoneTogglesNothing)
{
    SilentTwoStages silent;
    Rocket&         rocket = silent.rocket;
    ASSERT_FALSE(rocket.isEventsEnabled());
    const StageSelector selector(rocket, nullptr);
    ASSERT_EQ(selector.buttonCount(), 2U);

    // The booster leaves the rocket without an event, so the selector does not know.
    const std::unique_ptr<RocketComponent> booster = rocket.removeChild(1);
    ASSERT_EQ(selector.buttonCount(), 2U);
    const QPointer<QToolButton> stale  = selector.button(1);
    const std::string           before = activeStages(rocket);
    stale->setEnabled(true);
    stale->click();
    EXPECT_EQ(activeStages(rocket), before);
    EXPECT_EQ(selector.buttonCount(), 1U);
    deleteLaterObjects();
    EXPECT_TRUE(stale.isNull());
}

TEST_F(StageSelectorTest, TogglesAStageOfTheSelectedConfigurationWithItsSubStages)
{
    // The core stage holds the booster set: toggling the core toggles the boosters with it.
    const QtRocket::Test::TestFalcon9Heavy falcon;
    Rocket&                                rocket = *falcon.rocket;
    const ChangeEventTypes                 events(rocket);
    StageSelector::toggleStage(rocket, QtRocket::Test::TestFalcon9Heavy::kCoreStageNumber);
    EXPECT_EQ(activeStages(rocket), "100");
    EXPECT_EQ(events.types(), std::vector<int>{StageSelector::kToggleChange});

    // An unknown stage number toggles nothing, but the event is fired, as in Java.
    StageSelector::toggleStage(rocket, 17);
    EXPECT_EQ(activeStages(rocket), "100");
    EXPECT_EQ(events.types().size(), 2U);
}

}  // namespace

}  // namespace QtRocket::Gui
