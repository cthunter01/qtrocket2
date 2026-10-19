// The design view's stage buttons (StageSelector) driving its figure and its numbers
// (RocketFigure, DesignPhysics): a selection of stages that OpenRocket's force analysis cannot
// handle is made by clicks, shown with OpenRocket's numbers, and answered by the aerodynamics with
// an ordinary Error, never an exception (decision O17).

#include <QCoreApplication>
#include <QEvent>
#include <QToolButton>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/StageSelector.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "RecordingGuardHook.h"
#include "aero/BarrowmanTestRockets.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using StageToggleTest = Test::GuiTest;

/// The figure's coalesced answer to the rocket's events (RocketFigure::rocketChanged(), on which
/// DesignPhysics computes).
void deliverRocketChanges()
{
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
}

TEST_F(StageToggleTest, AStageBelowInactiveStagesIsShownAndRefusedOnlyByTheForceAnalysis)
{
    const Gui::Test::RecordingGuardHook                hook;
    const QtRocket::Test::TestBoostersOnBoostersRocket nested;
    Rocket&                                            rocket = *nested.rocket;
    ASSERT_TRUE(settleAutomaticDimensions(rocket).has_value());
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, 96.0, nullptr);
    const DesignPhysics       design(figure, preferences);
    const StageSelector       selector(rocket, nullptr);
    ASSERT_EQ(selector.buttonCount(), 4U);

    // RibbonProbe's sequence (special:boosters-on-boosters): the payload off, the core off (its
    // boosters, and theirs, with it), the inner boosters on.
    selector.button(0)->click();
    deliverRocketChanges();
    selector.button(1)->click();
    deliverRocketChanges();
    selector.button(3)->click();
    deliverRocketChanges();
    const FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    ASSERT_FALSE(configuration.isStageActive(QtRocket::Test::TestFalcon9Heavy::kCoreStageNumber));
    ASSERT_TRUE(configuration.isStageActive(
        QtRocket::Test::TestBoostersOnBoostersRocket::kInnerStageNumber));
    EXPECT_FALSE(selector.button(3)->isEnabled());

    // Nothing threw on the way, and the design view shows OpenRocket's numbers after the same
    // selection and event (RocketInfoProbe, special:boosters-on-boosters-toggled).
    EXPECT_TRUE(hook.failures().empty());
    EXPECT_NEAR(design.physics().cp.x, 0x1.4f88d4ca4a81bp-1, 1e-9);
    EXPECT_NEAR(design.physics().cg.x, 0x1.3c8dccdd0d79fp-1, 1e-9);
    EXPECT_EQ(QtStrings::toUtf8(design.info().getStabilityCombined()), "1.24 cal / 14.8 %");
    EXPECT_EQ(QtStrings::toUtf8(design.info().getCp()), "65.5 cm");
    EXPECT_EQ(QtStrings::toUtf8(design.info().getCg()), "61.8 cm");

    // The force analysis and the component drag refuse the selection with an ordinary Error.
    BarrowmanCalculator    calculator;
    const FlightConditions conditions(configuration);
    const Result<ForceMap> analysis =
        calculator.getForceAnalysis(configuration, conditions, nullptr);
    ASSERT_FALSE(analysis.has_value());
    EXPECT_EQ(analysis.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_EQ(analysis.error().message, BarrowmanCalculator::unsupportedStageSelectionText(
                                            "Inner Boosters", "Booster Stage", "Core Stage"));
    const Result<double> drag = ComponentDrag::getOverrideCD(*nested.innerFins, 0.3);
    ASSERT_FALSE(drag.has_value());
    EXPECT_EQ(drag.error().code, ErrorCode::INVALID_ARGUMENT);

    // Back to every stage: the analysis is made again.
    selector.button(0)->click();
    selector.button(1)->click();
    deliverRocketChanges();
    EXPECT_TRUE(calculator.getForceAnalysis(configuration, FlightConditions(configuration), nullptr)
                    .has_value());
    EXPECT_TRUE(hook.failures().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
