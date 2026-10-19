// Tests of figure/DesignPhysics.h: the ports of swing RocketInfoContextHelperTest, the numbers
// against those of the command-line tool (InfoCommand, which prints what the design window shows),
// and how the object follows its figure and the rocket: one computation per pass of the event loop,
// the carets, the CP parameters, the stage selections the force analysis refuses, undo, and nothing
// once the figure or the document is gone. OpenRocket's numbers for the examples and the test
// rockets are compared in rocket_info_golden_tests.cpp.

#include "QtRocket/gui/figure/DesignPhysics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QObject>
#include <QPoint>
#include <QRegion>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/InfoCommand.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "RecordingGuardHook.h"
#include "RecordingPaintDevice.h"
#include "aero/BarrowmanTestRockets.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using DesignPhysicsTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;

constexpr double kDotsPerInch = 96.0;

/// The answer to the rocket's change events of this pass of the event loop (the figure's queued
/// update, which DesignPhysics follows).
void deliverRocketChanges(RocketFigure& figure)
{
    QCoreApplication::sendPostedEvents(&figure, QEvent::MetaCall);
}

/// The texts @p figure paints (its overlay's: the rocket draws none).
[[nodiscard]] std::vector<std::string> paintedTexts(RocketFigure& figure)
{
    RecordingPaintDevice device(figure.size());
    figure.render(&device, QPoint(), QRegion(figure.rect()), QWidget::RenderFlags());
    std::vector<std::string> texts;
    for (const RecordedPaint& record : Test::joinTextRuns(device.records()))
    {
        if (record.kind == RecordedPaint::Kind::TEXT)
        {
            texts.push_back(QtStrings::toUtf8(record.text));
        }
    }
    return texts;
}

/// What 'info' printed, by label.
using PrintedNumbers = std::map<std::string, std::string, std::less<>>;

/// What 'info' prints of the configuration @p rocket has selected: "length" -> "0.4254 m", ...
[[nodiscard]] PrintedNumbers printedNumbers(Rocket& rocket, const Preferences& preferences)
{
    int index = -1;
    for (int candidate = 0; candidate <= rocket.getConfigurationCount(); ++candidate)
    {
        if (&rocket.getFlightConfigurationByIndex(candidate, true) ==
            &rocket.getSelectedConfiguration())
        {
            index = candidate;
        }
    }
    EXPECT_GE(index, 0);
    CliOutput output;
    InfoCommand::printConfiguration(rocket, index, preferences.getDefaultMach(), preferences,
                                    output);
    PrintedNumbers   numbers;
    std::string_view text = output.out();
    while (!text.empty())
    {
        const std::size_t      end  = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        const std::size_t colon = line.find(": ");
        if (line.starts_with("  ") && !line.starts_with("    ") && colon != std::string_view::npos)
        {
            numbers.emplace(line.substr(2, colon - 2), line.substr(colon + 2));
        }
    }
    return numbers;
}

/// The number of @p printed before its unit ("0.4254 m": 0.4254); NaN for "none (...)".
[[nodiscard]] double printedValue(std::string_view printed)
{
    const std::optional<double> value = Strings::parseDouble(printed.substr(0, printed.find(' ')));
    return value.value_or(std::numeric_limits<double>::quiet_NaN());
}

/// Compares the CG of @p physics with @p printed's: "none (no mass)" there for a CG without mass,
/// which the overlay shows as 0.
void expectPrintedCg(const DesignPhysics::Physics& physics, const PrintedNumbers& printed)
{
    if (physics.cg.weight > MassCalculator::kMinMass)
    {
        EXPECT_EQ(physics.cg.x, printedValue(printed.at("CG")));
    }
    else
    {
        EXPECT_EQ(printed.at("CG"), "none (no mass)");
    }
}

/// Compares the worst CP of @p physics with @p printed's: "none (no lift)" there for a CP
/// without lift, which the overlay shows as 0.
void expectPrintedCp(const DesignPhysics::Physics& physics, const PrintedNumbers& printed)
{
    const std::string worst = "worst CP at Mach 0.3";
    if (physics.cp.weight > MathUtil::kEpsilon)
    {
        EXPECT_EQ(physics.cp.x, printedValue(printed.at(worst)));
    }
    else
    {
        EXPECT_EQ(printed.at(worst), "none (no lift)");
    }
}

/// Compares whether @p physics has warnings with @p printed: "warnings: none", or "warnings:"
/// with a line for each.
void expectPrintedWarnings(const DesignPhysics::Physics& physics, const PrintedNumbers& printed)
{
    const auto warnings = printed.find("warnings");
    const bool none     = warnings != printed.end() && warnings->second == "none";
    EXPECT_EQ(physics.warnings.empty(), none);
}

/// Compares DesignPhysics' numbers for @p rocket with what 'info' prints of it.
void expectNumbersOfTheCommandLineTool(Rocket& rocket)
{
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    const DesignPhysics       design(figure, preferences);
    const PrintedNumbers      printed = printedNumbers(rocket, preferences);
    ASSERT_TRUE(printed.contains("length"));
    ASSERT_TRUE(printed.contains("CG"));
    ASSERT_TRUE(printed.contains("worst CP at Mach 0.3"));
    const DesignPhysics::Physics& physics = design.physics();
    EXPECT_EQ(physics.length, printedValue(printed.at("length")));
    EXPECT_EQ(physics.cg.weight, printedValue(printed.at("mass with motors")));
    EXPECT_EQ(physics.massWithoutMotors, printedValue(printed.at("mass without motors")));
    expectPrintedCg(physics, printed);
    expectPrintedCp(physics, printed);
    expectPrintedWarnings(physics, printed);
}

// Port of RocketInfoContextHelperTest.displayPhysicsIncludesGeometryWarnings.
TEST_F(DesignPhysicsTest, DisplayPhysicsIncludesGeometryWarnings)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    alpha.nose->setAftRadius(alpha.body->getOuterRadius() * 1.25);

    const FlightConfiguration& configuration = alpha.rocket->getSelectedConfiguration();
    WarningSet                 warnings;
    FlightConditions           conditions(configuration);
    BarrowmanCalculator        calculator;
    std::ignore =
        DesignPhysics::calculateCp(configuration, conditions, warnings, calculator, false);

    bool found = false;
    for (const Warning& warning : warnings)
    {
        found = found || warning.messageDescription() ==
                             Warning::kDiameterDiscontinuity.messageDescription();
    }
    EXPECT_TRUE(found) << "rocket display calculations should include geometry warnings: "
                       << warnings.size();
}

// Port of RocketInfoContextHelperTest.bodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews. The
// second overlay is Java's threeDimensionalInfo without set3DView(true), which only chooses the
// 3D view's text colour (Milestone 6).
TEST_F(DesignPhysicsTest, BodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews)
{
    ASSERT_TRUE(unitGroup(UnitGroupId::STABILITY).setDefaultUnit("cal"));

    Rocket      rocket;
    AxialStage& stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<BodyTube>(1.0, 0.125));

    FlightConfiguration configuration(rocket);
    configuration.setAllStages();
    FlightConditions          conditions(configuration);
    const InMemoryPreferences preferences;
    RocketInfo                twoDimensionalInfo(configuration, preferences);

    WarningSet                   warnings;
    BarrowmanCalculator          calculator;
    const DesignPhysics::Physics physics = DesignPhysics::computePhysics(
        configuration, conditions, warnings, calculator, true, twoDimensionalInfo);

    EXPECT_LE(physics.cp.weight, MathUtil::kEpsilon)
        << "a body tube alone should not have a usable CP";

    RocketInfo threeDimensionalInfo(configuration, preferences);
    DesignPhysics::applyCgAndCp(threeDimensionalInfo, physics.cg, physics.cp);

    const Unit* metres = unitGroup(UnitGroupId::LENGTH).getUnit("m");
    ASSERT_NE(metres, nullptr);
    EXPECT_EQ(QtStrings::toUtf8(twoDimensionalInfo.getCp(*metres)), "0 m");
    EXPECT_EQ(twoDimensionalInfo.getCp(*metres), threeDimensionalInfo.getCp(*metres));
    const QString stability = twoDimensionalInfo.getStabilityCombined();
    EXPECT_TRUE(stability.startsWith(QStringLiteral("-2 cal")))
        << "unexpected stability: " << QtStrings::toUtf8(stability);
    EXPECT_EQ(twoDimensionalInfo.getStabilityCombined(),
              threeDimensionalInfo.getStabilityCombined());
    ASSERT_TRUE(unitGroup(UnitGroupId::STABILITY).setDefaultUnit("cal"));
}

class DesignPhysicsOfExample : public Test::GuiTest,
                               public ::testing::WithParamInterface<QtRocket::Test::GoldenExample>
{ };

TEST_P(DesignPhysicsOfExample, ComputesWhatTheCommandLineToolPrints)
{
    Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
        GetParam(), QtRocket::Test::ExampleSource::ORIGINAL,
        QtRocket::Test::DesignFileEnvironment::Presets::NONE);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    expectNumbersOfTheCommandLineTool(loaded->document->getRocket());
}

INSTANTIATE_TEST_SUITE_P(Examples, DesignPhysicsOfExample,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

TEST_F(DesignPhysicsTest, ComputesWhatTheCommandLineToolPrintsOfTheTestRockets)
{
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        SCOPED_TRACE(maker.method);
        const std::unique_ptr<Rocket> rocket = maker.make();
        ASSERT_TRUE(settleAutomaticDimensions(*rocket).has_value());
        expectNumbersOfTheCommandLineTool(*rocket);
    }
}

TEST_F(DesignPhysicsTest, ShowsItsElementsInTheFigureUntilItIsDestroyed)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    figure.resize(1000, 700);
    auto design = std::make_unique<DesignPhysics>(figure, preferences);
    // Computed at once: the overlay shows the rocket's name and the carets stand at the CG and
    // the CP.
    const std::vector<std::string> texts = paintedTexts(figure);
    ASSERT_FALSE(texts.empty());
    EXPECT_EQ(texts.front(), beta.rocket->getName());
    EXPECT_EQ(design->cgCaret().x(), design->physics().cg.x);
    EXPECT_EQ(design->cpCaret().x(), design->physics().cp.x);
    EXPECT_EQ(design->figure(), &figure);

    // The figure no longer paints them once the object is gone.
    design.reset();
    EXPECT_TRUE(paintedTexts(figure).empty());
}

TEST_F(DesignPhysicsTest, ComputesOncePerPassOfTheEventLoop)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    const DesignPhysics            design(figure, preferences);
    const QSignalSpy               updated(&design, &DesignPhysics::updated);
    const double                   length = design.physics().length;

    // Several events: nothing until the figure has caught up, then one computation.
    beta.body->setLength(beta.body->getLength() + 0.1);
    beta.body->setLength(beta.body->getLength() + 0.1);
    beta.nose->setLength(beta.nose->getLength() + 0.1);
    EXPECT_EQ(updated.count(), 0);
    EXPECT_EQ(design.physics().length, length);

    deliverRocketChanges(figure);
    EXPECT_EQ(updated.count(), 1);
    EXPECT_NEAR(design.physics().length, length + 0.3, 1e-12);
    EXPECT_NEAR(design.info().length(), length + 0.3, 1e-12);

    deliverRocketChanges(figure);
    EXPECT_EQ(updated.count(), 1);
}

TEST_F(DesignPhysicsTest, MovesTheCaretsWithTheViewAndTheRotationWithoutComputing)
{
    const QtRocket::Test::TestEstesAlphaIIIWithPods pods;
    const InMemoryPreferences                       preferences;
    RocketFigure        figure(*pods.rocket, preferences, kDotsPerInch, nullptr);
    const DesignPhysics design(figure, preferences);
    const QSignalSpy    updated(&design, &DesignPhysics::updated);
    const Coordinate    cg = design.physics().cg;
    const Coordinate    cp = design.physics().cp;
    ASSERT_GT(cg.weight, MassCalculator::kMinMass);
    ASSERT_GT(cp.weight, MathUtil::kEpsilon);

    // The side view: the projection on the plane at the rotation.
    EXPECT_EQ(design.cgCaret().x(), cg.x);
    EXPECT_EQ(design.cgCaret().y(), cg.y);
    EXPECT_EQ(design.cpCaret().x(), cp.x);
    EXPECT_EQ(design.cpCaret().y(), cp.y);
    figure.setRotation(0.5);
    EXPECT_DOUBLE_EQ(design.cgCaret().y(), (cg.y * std::cos(0.5)) + (cg.z * std::sin(0.5)));
    EXPECT_DOUBLE_EQ(design.cpCaret().y(), (cp.y * std::cos(0.5)) + (cp.z * std::sin(0.5)));

    // The top view: the rotation less PI/2, the CG's height turned over, the CP's not.
    figure.setView(FigureView::TOP);
    const double top = 0.5 - (std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(design.cgCaret().y(), -((cg.y * std::cos(top)) + (cg.z * std::sin(top))));
    EXPECT_DOUBLE_EQ(design.cpCaret().y(), (cp.y * std::cos(top)) + (cp.z * std::sin(top)));
    EXPECT_EQ(design.cgCaret().x(), cg.x);

    // The back view: no carets.
    figure.setView(FigureView::BACK);
    EXPECT_TRUE(std::isnan(design.cgCaret().x()));
    EXPECT_TRUE(std::isnan(design.cgCaret().y()));
    EXPECT_TRUE(std::isnan(design.cpCaret().x()));
    EXPECT_TRUE(std::isnan(design.cpCaret().y()));
    figure.setView(FigureView::SIDE);
    EXPECT_EQ(design.cpCaret().x(), cp.x);

    // A zoom or a selection changes neither.
    figure.setSelection({});
    std::ignore = figure.scaleTo(2.0, QSize(1000, 700));
    EXPECT_EQ(updated.count(), 0);
}

TEST_F(DesignPhysicsTest, PlacesNoCaretsWithoutLengthMassOrLift)
{
    // A new design: no length, no mass, no lift (the CP is getWorstCP()'s sentinel).
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    RocketFigure              figure(document->getRocket(), preferences, kDotsPerInch, nullptr);
    const DesignPhysics       design(figure, preferences);
    EXPECT_EQ(design.physics().length, 0.0);
    EXPECT_EQ(design.physics().cp.x, std::numeric_limits<double>::max());
    EXPECT_TRUE(std::isnan(design.physics().diameter));
    EXPECT_TRUE(std::isnan(design.cgCaret().x()));
    EXPECT_TRUE(std::isnan(design.cpCaret().x()));
    EXPECT_EQ(design.info().cp(), 0.0);
    EXPECT_EQ(design.info().cg(), 0.0);

    // A body tube alone: a CG, but no CP caret.
    document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>(1.0, 0.125));
    deliverRocketChanges(figure);
    EXPECT_EQ(design.physics().length, 1.0);
    EXPECT_EQ(design.cgCaret().x(), 0.5);
    EXPECT_TRUE(std::isnan(design.cpCaret().x()));
    EXPECT_EQ(design.info().cp(), 0.0);
    EXPECT_EQ(design.info().cg(), 0.5);

    const DesignPhysics::Physics        withoutLength;
    const DesignPhysics::CaretPositions none =
        DesignPhysics::caretPositions(withoutLength, FigureView::SIDE, 0.0);
    EXPECT_TRUE(std::isnan(none.cgX));
    EXPECT_TRUE(std::isnan(none.cpY));
}

TEST_F(DesignPhysicsTest, TakesTheCpParametersOfTheComponentAnalysis)
{
    const QtRocket::Test::TestBeta beta;
    InMemoryPreferences            preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    DesignPhysics                  design(figure, preferences);
    const QSignalSpy               updated(&design, &DesignPhysics::updated);
    EXPECT_TRUE(std::isnan(design.cpMach()));
    EXPECT_TRUE(std::isnan(design.cpAOA()));
    EXPECT_TRUE(std::isnan(design.cpTheta()));
    EXPECT_TRUE(std::isnan(design.cpRoll()));
    EXPECT_EQ(design.info().mach(), 0.3);
    const double worst = design.physics().cp.x;

    // The Mach number: the overlay's too; the same value again computes nothing.
    design.setCPMach(0.8);
    EXPECT_EQ(updated.count(), 1);
    EXPECT_EQ(design.info().mach(), 0.8);
    design.setCPMach(0.8 + 1e-12);
    design.setCPMach(0.8);
    EXPECT_EQ(updated.count(), 1);
    design.setCPMach(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(updated.count(), 2);
    EXPECT_EQ(design.info().mach(), 0.3);
    EXPECT_EQ(design.physics().cp.x, worst);

    // The preferences' Mach number, read at each computation.
    preferences.setDefaultMach(0.5);
    design.update();
    EXPECT_EQ(design.info().mach(), 0.5);
    EXPECT_EQ(updated.count(), 3);

    // The wind direction: the figure turns to it and the CP is that direction's (getCP()).
    design.setCPTheta(0.4);
    EXPECT_EQ(figure.rotation(), 0.4);
    EXPECT_EQ(design.info().theta(), 0.4);
    design.setCPTheta(std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(std::isnan(design.info().theta()));

    design.setCPAOA(0.1);
    EXPECT_EQ(design.info().aoa(), 0.1);
    design.setCPRoll(1.0);
    EXPECT_EQ(design.cpRoll(), 1.0);
    EXPECT_EQ(updated.count(), 7);
}

TEST_F(DesignPhysicsTest, ShowsOrCountsTheWarnings)
{
    const QtRocket::Test::TestEstesAlphaIII alpha;
    alpha.nose->setAftRadius(alpha.body->getOuterRadius() * 1.25);
    const InMemoryPreferences preferences;
    RocketFigure              figure(*alpha.rocket, preferences, kDotsPerInch, nullptr);
    figure.resize(1000, 700);
    DesignPhysics    design(figure, preferences);
    const QSignalSpy updated(&design, &DesignPhysics::updated);
    ASSERT_EQ(design.physics().warnings.size(), 1U);
    EXPECT_TRUE(design.isShowWarnings());
    std::vector<std::string> texts = paintedTexts(figure);
    ASSERT_GE(texts.size(), 2U);
    EXPECT_EQ(texts.at(texts.size() - 2), "Warning:");

    design.setShowWarnings(false);
    EXPECT_FALSE(design.info().isShowWarnings());
    texts = paintedTexts(figure);
    EXPECT_EQ(texts.back(), "1 warning(s)");
    EXPECT_EQ(updated.count(), 0);
}

TEST_F(DesignPhysicsTest, ShowsAStageBelowInactiveStagesWhichTheForceAnalysisRefuses)
{
    const Gui::Test::RecordingGuardHook                hook;
    const QtRocket::Test::TestBoostersOnBoostersRocket nested;
    Rocket&                                            rocket = *nested.rocket;
    const InMemoryPreferences                          preferences;
    RocketFigure        figure(rocket, preferences, kDotsPerInch, nullptr);
    const DesignPhysics design(figure, preferences);

    // The stage selector's toggle (StageSelector: toggleStage(), then the rocket's event).
    FlightConfiguration& configuration = rocket.getSelectedConfiguration();
    configuration.setOnlyStage(QtRocket::Test::TestBoostersOnBoostersRocket::kInnerStageNumber);
    rocket.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange |
                                        ComponentChangeEvent::kMotorChange |
                                        ComponentChangeEvent::kTreeChangeChildren,
                                    configuration.getId());
    deliverRocketChanges(figure);

    // The force analysis of the selection is an ordinary Error ...
    BarrowmanCalculator    calculator;
    const FlightConditions conditions(configuration);
    EXPECT_FALSE(calculator.getForceAnalysis(configuration, conditions, nullptr).has_value());
    // ... but the design view's numbers exist, and are shown: OpenRocket's after the same toggle
    // and event (RocketInfoProbe, special:boosters-on-boosters-toggled), CP 0.655 m, CG 0.618 m.
    EXPECT_TRUE(hook.failures().empty());
    EXPECT_NEAR(design.physics().cp.x, 0x1.4f88d4ca4a81bp-1, 1e-9);
    EXPECT_NEAR(design.physics().cg.x, 0x1.3c8dccdd0d79fp-1, 1e-9);
    EXPECT_EQ(QtStrings::toUtf8(design.info().getStabilityCombined()), "1.24 cal / 14.8 %");
    EXPECT_EQ(QtStrings::toUtf8(design.info().getCp()), "65.5 cm");
}

TEST_F(DesignPhysicsTest, FollowsTheSelectedConfiguration)
{
    const QtRocket::Test::GoldenExample* example =
        QtRocket::Test::findGoldenExample("Two stage high power rocket.ork");
    ASSERT_NE(example, nullptr);
    Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
        *example, QtRocket::Test::ExampleSource::ORIGINAL,
        QtRocket::Test::DesignFileEnvironment::Presets::NONE);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    Rocket&                   rocket = loaded->document->getRocket();
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    const DesignPhysics       design(figure, preferences);
    ASSERT_TRUE(design.info().hasMotors());

    // The default configuration has no motors: no mass with motors in the overlay.
    rocket.setSelectedConfiguration(rocket.getFlightConfigurationByIndex(0, true).getId());
    deliverRocketChanges(figure);
    EXPECT_FALSE(design.info().hasMotors());
    EXPECT_EQ(design.physics().cg.weight, design.physics().massWithoutMotors);
}

TEST_F(DesignPhysicsTest, KeepsNoComponentOrConfigurationAcrossAnUndo)
{
    // Under AddressSanitizer this is the proof: Rocket::loadFrom() replaces every component and
    // configuration, and the overlay's stability units find the configuration again by its id.
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    Rocket&          rocket = document->getRocket();
    RocketComponent& stage  = rocket.getChild(0);
    stage.addChild(std::make_unique<BodyTube>(0.3, 0.02));
    const InMemoryPreferences preferences;
    RocketFigure              figure(rocket, preferences, kDotsPerInch, nullptr);
    figure.resize(1000, 700);
    const DesignPhysics design(figure, preferences);
    deliverRocketChanges(figure);
    EXPECT_EQ(design.physics().length, 0.3);

    document->addUndoPosition("Add");
    stage.addChild(std::make_unique<BodyTube>(0.2, 0.02));
    deliverRocketChanges(figure);
    EXPECT_NEAR(design.physics().length, 0.5, 1e-12);
    std::ignore = paintedTexts(figure);

    document->undo();
    // Painted before the figure caught up, and after.
    std::ignore = paintedTexts(figure);
    deliverRocketChanges(figure);
    EXPECT_EQ(design.physics().length, 0.3);
    EXPECT_FALSE(paintedTexts(figure).empty());
}

TEST_F(DesignPhysicsTest, ComputesNothingOnceTheFigureIsGone)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    auto figure = std::make_unique<RocketFigure>(*beta.rocket, preferences, kDotsPerInch, nullptr);
    DesignPhysics    design(*figure, preferences);
    const QSignalSpy updated(&design, &DesignPhysics::updated);

    figure.reset();
    EXPECT_EQ(design.figure(), nullptr);
    design.update();
    design.setCPMach(0.5);
    design.setShowWarnings(false);
    EXPECT_EQ(updated.count(), 0);
}

TEST_F(DesignPhysicsTest, DropsAPendingComputationWithTheDocument)
{
    // The window goes first (the figure and the object), then the document: the computation the
    // rocket's last event asked for never runs.
    auto                      document = OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    auto                      figure =
        std::make_unique<RocketFigure>(document->getRocket(), preferences, kDotsPerInch, nullptr);
    auto design = std::make_unique<DesignPhysics>(*figure, preferences);
    document->getRocket().getChild(0).addChild(std::make_unique<BodyTube>(0.3, 0.02));
    design.reset();
    figure.reset();
    document.reset();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    SUCCEED();
}

TEST_F(DesignPhysicsTest, IsNeverAChildOfItsFigure)
{
    const QtRocket::Test::TestBeta beta;
    const InMemoryPreferences      preferences;
    RocketFigure                   figure(*beta.rocket, preferences, kDotsPerInch, nullptr);
    QObject                        holder(&figure);
    EXPECT_THROW({ const DesignPhysics child(figure, preferences, &figure); }, BugError);
    EXPECT_THROW({ const DesignPhysics grandchild(figure, preferences, &holder); }, BugError);
    // The figure was left as it was: an object of another parent can be made.
    QObject             parent;
    const DesignPhysics design(figure, preferences, &parent);
    EXPECT_EQ(design.parent(), &parent);
    // The failed ones added nothing to the figure: one overlay, one rocket name.
    figure.resize(1000, 700);
    EXPECT_EQ(std::ranges::count(paintedTexts(figure), beta.rocket->getName()), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
