// OpenRocket's panels of the trapezoidal and the elliptical fin set (TrapezoidFinSetConfig and
// EllipticalFinSetConfig with their base FinSetConfig, OpenRocket 5f164fd0e) made by FinPanelProbe
// (tools/openrocket-panel-pins) under Xvfb, against QtRocket's in the same state. The probe
// printed, and this test replays:
// - "panel": the panel of every trapezoidal and elliptical fin set of the 16 examples (loaded
//   without presets) and the 13 test rockets, in tree order, each after the design was settled
//   (settleAutomaticDimensions()): the titles and tooltips of the tabs General and Fin tabs, their
//   labels (with their tooltips), spin box texts (the counts too), unit selectors, slider
//   positions and combo boxes (the material panel and the fillet material left out), the button
//   "Calculate automatically" and the buttons "Split fins" and "Convert to freeform" of the button
//   row, and the fin set's values (the sweep exactly, as read from the design);
// - "edit": on designs of the probe's own (a body tube 30 cm long with a fixed radius of 2.1 cm
//   holding the fin set, motor mounts and centering rings), edits through the panel, as a user
//   makes them: the text a spin box commits (in its unit), a count, a combo box item, the button
//   "Calculate automatically"; after each the tabs' widgets but the labels, the buttons and the
//   fin set's values (the sweep in micrometres: an edit of the sweep angle makes it with a
//   tangent);
// - "multi" and "multi edit": a panel with a second fin set selected (Java: the config listener of
//   the first) and edits through it, after each the values of both;
// - "compute": FinSetConfig.computeFinTabLength() on FinSetConfigTest's inputs (and one more),
//   the length it returns and the tab offset it gives its model (FinTabCalculatorTests);
// - "dialog": "Convert to freeform", "Split fins" and "Calculate automatically" through the
//   configuration dialog itself (ComponentConfigDialog), after each step the dialog then open,
//   the rocket's tree and the undo history.
// The lines of each panel section are sorted, a dialog's are in the order printed. The pins are
// FinPanelPins.h, generated from the probe's output.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "FinPanelPins.h"
#include "GuiTestSupport.h"
#include "PanelProbeLines.h"
#include "PanelReplay.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/FinSetConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

using FinPanelBuiltGolden  = Test::GuiTest;
using FinDialogFlowsGolden = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FinDialogPin;
using Test::FinPanelPin;
using Test::javaDouble;
using Test::kFinDialogPins;
using Test::kFinPanelPins;
using Test::kFinPanelPinsCommit;
using Test::onBodyTube;
using Test::PanelEdit;
using Test::PanelReplay;
using Test::pinnedDesignDocument;
using Test::pinnedLineDifferences;
using Test::quoted;
using Test::quotedOrNull;
using Test::withDifference;

// ------------------------------------------------------------------------------- the lines

/// Whether @p component is one of the kinds the probe opens a panel for.
[[nodiscard]] bool isFinPanelKind(const RocketComponent& component)
{
    return component.kind() == ComponentKind::TRAPEZOID_FIN_SET ||
           component.kind() == ComponentKind::ELLIPTICAL_FIN_SET;
}

/// The class name the probe prints for @p component (Java's getSimpleName()).
[[nodiscard]] std::string_view probeClassName(const RocketComponent& component)
{
    switch (component.kind())
    {
        case ComponentKind::ROCKET:
            return "Rocket";
        case ComponentKind::AXIAL_STAGE:
            return "AxialStage";
        case ComponentKind::BODY_TUBE:
            return "BodyTube";
        case ComponentKind::INNER_TUBE:
            return "InnerTube";
        case ComponentKind::CENTERING_RING:
            return "CenteringRing";
        case ComponentKind::TRAPEZOID_FIN_SET:
            return "TrapezoidFinSet";
        case ComponentKind::ELLIPTICAL_FIN_SET:
            return "EllipticalFinSet";
        case ComponentKind::FREEFORM_FIN_SET:
            return "FreeformFinSet";
        default:
            return "?";
    }
}

/// Java's Math.round() of @p value (floor(value + 0.5)).
[[nodiscard]] long long javaRound(double value)
{
    return static_cast<long long>(std::floor(value + 0.5));
}

/// The probe's state() of the outline of a trapezoidal or elliptical fin set.
void addOutlineState(const FinSet& fins, std::string_view prefix, bool exactSweep,
                     std::vector<std::string>& lines)
{
    if (const auto* trapezoid = dynamic_cast<const TrapezoidFinSet*>(&fins))
    {
        lines.push_back(
            std::format("state {}RootChord={}", prefix, javaDouble(trapezoid->getRootChord())));
        lines.push_back(
            std::format("state {}TipChord={}", prefix, javaDouble(trapezoid->getTipChord())));
        lines.push_back(
            std::format("state {}Height={}", prefix, javaDouble(trapezoid->getHeight())));
        if (exactSweep)
        {
            lines.push_back(
                std::format("state {}Sweep={}", prefix, javaDouble(trapezoid->getSweep())));
        }
        else
        {
            lines.push_back(
                std::format("state {}SweepUm={}", prefix, javaRound(trapezoid->getSweep() * 1e6)));
        }
    }
    else if (const auto* ellipse = dynamic_cast<const EllipticalFinSet*>(&fins))
    {
        lines.push_back(std::format("state {}Length={}", prefix, javaDouble(ellipse->getLength())));
        lines.push_back(std::format("state {}Height={}", prefix, javaDouble(ellipse->getHeight())));
    }
}

/// The probe's state(): the values of a fin set (the largest tab height only on a body tube).
void addFinSetState(const RocketComponent& component, std::string_view prefix, bool exactSweep,
                    std::vector<std::string>& lines)
{
    const auto* fins = dynamic_cast<const FinSet*>(&component);
    if (fins == nullptr)
    {
        return;
    }
    lines.push_back(std::format("state {}FinCount={}", prefix, fins->getFinCount()));
    lines.push_back(std::format("state {}CantAngle={}", prefix, javaDouble(fins->getCantAngle())));
    lines.push_back(std::format("state {}Thickness={}", prefix, javaDouble(fins->getThickness())));
    lines.push_back(std::format("state {}CrossSection={}", prefix,
                                finCrossSectionName(fins->getCrossSection())));
    lines.push_back(
        std::format("state {}BaseRotation={}", prefix, javaDouble(fins->getBaseRotation())));
    lines.push_back(std::format("state {}TabHeight={}", prefix, javaDouble(fins->getTabHeight())));
    lines.push_back(std::format("state {}TabLength={}", prefix, javaDouble(fins->getTabLength())));
    lines.push_back(std::format("state {}TabOffset={}", prefix, javaDouble(fins->getTabOffset())));
    lines.push_back(std::format("state {}TabOffsetMethod={}", prefix,
                                axialMethodName(fins->getTabOffsetMethod())));
    lines.push_back(
        std::format("state {}TabFrontEdge={}", prefix, javaDouble(fins->getTabFrontEdge())));
    lines.push_back(
        std::format("state {}FilletRadius={}", prefix, javaDouble(fins->getFilletRadius())));
    lines.push_back(
        std::format("state {}FilletMaterial={}", prefix, fins->getFilletMaterial().getName()));
    if (dynamic_cast<const BodyTube*>(fins->getParent()) != nullptr)
    {
        lines.push_back(
            std::format("state {}MaxTabHeight={}", prefix, javaDouble(fins->getMaxTabHeight())));
    }
    addOutlineState(*fins, prefix, exactSweep, lines);
}

/// The probe's buttonLine(): "button "<text>" enabled=<b> tip=<tooltip>".
[[nodiscard]] std::string buttonLine(const QPushButton& button)
{
    return std::format("button {} enabled={} tip={}", quoted(button.text()),
                       button.isEnabled() ? "true" : "false", quotedOrNull(button.toolTip()));
}

/// The lines of the widgets of the tab @p index of @p tabs, each after @p prefix.
void addPrefixedTabLines(const QTabWidget& tabs, int index, std::string_view prefix,
                         bool withLabels, std::vector<std::string>& lines)
{
    std::vector<std::string> tabLines;
    Test::addTabWidgetLines(*tabs.widget(index), {.labels = withLabels, .labelTips = true},
                            tabLines);
    for (const std::string& line : tabLines)
    {
        lines.push_back(std::string(prefix) + line);
    }
}

/// The probe's tabsLines(): the tabs General and Fin tabs (their titles with the labels) and the
/// buttons of the button row.
void addFinPanelLines(const RocketComponentConfig& config, bool withLabels,
                      std::vector<std::string>& lines)
{
    const auto&       panel = dynamic_cast<const FinSetConfig&>(config);
    const QTabWidget& tabs  = panel.tabs();
    if (withLabels)
    {
        lines.push_back(
            std::format("tab0 {} tip={}", quoted(tabs.tabText(0)), quoted(tabs.tabToolTip(0))));
        lines.push_back(
            std::format("tab1 {} tip={}", quoted(tabs.tabText(1)), quoted(tabs.tabToolTip(1))));
    }
    addPrefixedTabLines(tabs, 0, "general ", withLabels, lines);
    addPrefixedTabLines(tabs, 1, "fintabs ", withLabels, lines);
    lines.push_back("fintabs " + buttonLine(panel.autoCalcButton()));
    for (const QPushButton* button : {panel.splitButton(), panel.convertButton()})
    {
        if (button != nullptr)
        {
            lines.push_back("row " + buttonLine(*button));
        }
    }
}

/// What the replay prints of the trapezoidal and elliptical fin sets (FinPanelProbe's).
[[nodiscard]] Test::PanelReplayLines finLines()
{
    return {
        .isPanelKind = isFinPanelKind,
        .className   = probeClassName,
        .panel       = addFinPanelLines,
        .state       = addFinSetState,
    };
}

/// The pins of the examples and the test rockets.
[[nodiscard]] std::vector<FinPanelPin> designPins()
{
    std::vector<FinPanelPin> pins;
    std::ranges::copy_if(kFinPanelPins, std::back_inserter(pins),
                         [](const FinPanelPin& pin) { return pin.source != "built"; });
    return pins;
}

/// The lines of the pin of the probe's own design @p design.
[[nodiscard]] std::span<const std::string_view> builtLines(std::string_view design)
{
    const std::span pins(kFinPanelPins);
    const auto      found = std::ranges::find_if(pins, [design](const FinPanelPin& pin) {
        return pin.source == "built" && pin.design == design;
    });
    QTROCKET_ASSERT(found != pins.end());
    return found->lines;
}

// ------------------------------------------------------------------------------- the designs

/// The probe's mount(): a motor mount of the outer radius @p radius and the length @p length at
/// the bottom of @p body, moved by @p bottomOffset.
void addMount(BodyTube& body, double radius, double length, double bottomOffset)
{
    auto tube = std::make_unique<InnerTube>();
    tube->setOuterRadius(radius);
    tube->setThickness(0.0005);
    tube->setLength(length);
    tube->setAxialMethod(AxialMethod::BOTTOM);
    tube->setAxialOffset(bottomOffset);
    body.addChild(std::move(tube));
}

/// The probe's ring(): a centering ring of the length @p length @p top from the top of its
/// parent, with automatic radii.
[[nodiscard]] std::unique_ptr<CenteringRing> makeRing(double top, double length)
{
    auto ring = std::make_unique<CenteringRing>();
    ring->setLength(length);
    ring->setAxialMethod(AxialMethod::TOP);
    ring->setAxialOffset(top);
    return ring;
}

/// Adds makeRing(@p top, @p length) to @p body.
void addRing(BodyTube& body, double top, double length)
{
    body.addChild(makeRing(top, length));
}

/// The probe's trapezoid(): a trapezoidal fin set of this outline at the bottom of its parent.
[[nodiscard]] std::unique_ptr<TrapezoidFinSet> makeTrapezoid(double root, double tip, double height,
                                                             double sweep)
{
    auto fins = std::make_unique<TrapezoidFinSet>();
    fins->setRootChord(root);
    fins->setTipChord(tip);
    fins->setHeight(height);
    fins->setSweep(sweep);
    return fins;
}

/// The probe's design of most "autotab" edits: a motor mount of @p mountLength, the rings at
/// @p rings (5 mm long each) and a trapezoidal fin set (root 8 cm, tip 4 cm, height 5 cm, sweep
/// 3 cm).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> mountAndRings(double                  mountLength,
                                                                std::span<const double> rings)
{
    return onBodyTube([&](BodyTube& body) {
        addMount(body, 0.0125, mountLength, 0);
        for (const double top : rings)
        {
            addRing(body, top, 0.005);
        }
        body.addChild(makeTrapezoid(0.08, 0.04, 0.05, 0.03));
    });
}

/// The edit "Calculate automatically".
constexpr std::array kAutoCalc{PanelEdit{.property = "AutoCalc", .value = "button"}};

/// The probe's edits of a fin set of a design of its own, against the pin of @p design.
[[nodiscard]] std::string builtDifferences(OpenRocketDocument& document, std::string_view design,
                                           std::size_t index, std::span<const PanelEdit> edits)
{
    const ConfigDialogServices services;
    PanelReplay                replay(document, services, finLines());
    replay.edits(index, edits);
    return pinnedLineDifferences(replay.lines(), builtLines(design));
}

// ------------------------------------------------------------------------------- the tests

class FinPanelGolden : public Test::GuiTest, public ::testing::WithParamInterface<FinPanelPin>
{ };

TEST(FinPanelPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(kFinPanelPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(kFinPanelPins.size(), 39U);
    EXPECT_EQ(designPins().size(), 25U);
    EXPECT_EQ(kFinDialogPins.size(), 10U);
}

TEST_P(FinPanelGolden, ShowsAsOpenRocket)
{
    const FinPanelPin&                        pin = GetParam();
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        pinnedDesignDocument(pin.source, pin.design);
    ASSERT_NE(document, nullptr) << "no design " << pin.design;
    PanelReplay replay(*document, services, finLines());
    replay.panels();
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), pin.lines), "");
}

INSTANTIATE_TEST_SUITE_P(Designs, FinPanelGolden, ::testing::ValuesIn(designPins()),
                         [](const ::testing::TestParamInfo<FinPanelPin>& param) {
                             return std::string(param.param.testName);
                         });

TEST_F(FinPanelBuiltGolden, EditsOfATrapezoidalFinSet)
{
    // The fin count clamped to 1 ... 8, the cant to 15 degrees, the outline, the sweep angle (the
    // sweep from the height), the cross section, the fin rotation clamped to 180 degrees by the
    // field, the tab (its height clamped to the largest one when the panel was made, its offset
    // in each method), the fillet radius at least 0, the root chord moving the tab.
    static constexpr std::array kEdits{
        PanelEdit{.property = "FinCount", .value = "int:4"},
        PanelEdit{.property = "FinCount", .value = "int:1"},
        PanelEdit{.property = "FinCount", .value = "int:9"},
        PanelEdit{.property = "CantAngle", .value = "5"},
        PanelEdit{.property = "CantAngle", .value = "20"},
        PanelEdit{.property = "RootChord", .value = "8"},
        PanelEdit{.property = "TipChord", .value = "3"},
        PanelEdit{.property = "Height", .value = "4"},
        PanelEdit{.property = "Sweep", .value = "2"},
        PanelEdit{.property = "SweepAngle", .value = "30"},
        PanelEdit{.property = "CrossSection", .value = "select:AIRFOIL"},
        PanelEdit{.property = "Thickness", .value = "0.4"},
        PanelEdit{.property = "BaseRotation", .value = "200"},
        PanelEdit{.property = "TabLength", .value = "3"},
        PanelEdit{.property = "TabHeight", .value = "1"},
        PanelEdit{.property = "TabHeight", .value = "5"},
        PanelEdit{.property = "TabOffset", .value = "1"},
        PanelEdit{.property = "TabOffsetMethod", .value = "select:TOP"},
        PanelEdit{.property = "TabOffset", .value = "-2"},
        PanelEdit{.property = "TabOffsetMethod", .value = "select:AFTER"},
        PanelEdit{.property = "FilletRadius", .value = "0.5"},
        PanelEdit{.property = "FilletRadius", .value = "-1"},
        PanelEdit{.property = "RootChord", .value = "2"},
        PanelEdit{.property = "Height", .value = "0"},
    };
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<TrapezoidFinSet>()); });
    EXPECT_EQ(builtDifferences(*document, "trapezoid", 3, kEdits), "");
}

TEST_F(FinPanelBuiltGolden, EditsOfAnEllipticalFinSet)
{
    // The fin rotation unbounded (the setter reduces -90 degrees to itself), the length moving the
    // tab, the tab offset in the method BOTTOM.
    static constexpr std::array kEdits{
        PanelEdit{.property = "FinCount", .value = "int:2"},
        PanelEdit{.property = "CantAngle", .value = "-3"},
        PanelEdit{.property = "Length", .value = "7"},
        PanelEdit{.property = "Height", .value = "3"},
        PanelEdit{.property = "CrossSection", .value = "select:ROUNDED"},
        PanelEdit{.property = "Thickness", .value = "0.2"},
        PanelEdit{.property = "BaseRotation", .value = "-90"},
        PanelEdit{.property = "TabLength", .value = "4"},
        PanelEdit{.property = "TabHeight", .value = "0.5"},
        PanelEdit{.property = "TabOffsetMethod", .value = "select:BOTTOM"},
        PanelEdit{.property = "TabOffset", .value = "0.5"},
        PanelEdit{.property = "FilletRadius", .value = "0.3"},
        PanelEdit{.property = "Length", .value = "3"},
    };
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<EllipticalFinSet>()); });
    EXPECT_EQ(builtDifferences(*document, "elliptical", 3, kEdits), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyWithoutRings)
{
    // No centering ring and no inner tube: the tab runs along the whole root (method TOP, offset
    // 0) and reaches down to the axis.
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(makeTrapezoid(0.08, 0.04, 0.05, 0.03)); });
    EXPECT_EQ(builtDifferences(*document, "autotab plain", 3, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyBetweenTwoRings)
{
    // The tab from the front of the root to the ring within it, down to the motor mount; then a
    // tab height typed, the method MIDDLE, and again: the method stays MIDDLE.
    static constexpr std::array kEdits{
        PanelEdit{.property = "AutoCalc", .value = "button"},
        PanelEdit{.property = "TabHeight", .value = "0.3"},
        PanelEdit{.property = "TabOffsetMethod", .value = "select:MIDDLE"},
        PanelEdit{.property = "AutoCalc", .value = "button"},
    };
    static constexpr std::array               kRings{0.21, 0.27};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.1, kRings);
    EXPECT_EQ(builtDifferences(*document, "autotab mount", 6, kEdits), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyWithRingsAhead)
{
    static constexpr std::array               kRings{0.16, 0.18};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.15, kRings);
    EXPECT_EQ(builtDifferences(*document, "autotab rings ahead", 6, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyWithOneRingWithin)
{
    static constexpr std::array               kRings{0.17, 0.25};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.15, kRings);
    EXPECT_EQ(builtDifferences(*document, "autotab one ring within", 6, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyWithARingBehind)
{
    static constexpr std::array               kRings{0.24, 0.295};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.15, kRings);
    EXPECT_EQ(builtDifferences(*document, "autotab ring behind", 6, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyMergesAdjacentRings)
{
    static constexpr std::array               kRings{0.22, 0.225, 0.28};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.15, kRings);
    EXPECT_EQ(builtDifferences(*document, "autotab adjacent rings", 7, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyIgnoresATubeOutsideTheFin)
{
    // The motor mount lies outside the fin's span: the tab reaches down to the axis.
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        addMount(body, 0.0125, 0.05, 0.15);
        addRing(body, 0.12, 0.005);
        body.addChild(makeTrapezoid(0.08, 0.04, 0.05, 0.03));
    });
    EXPECT_EQ(builtDifferences(*document, "autotab tube ahead", 5, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyIgnoresRingsWithinTheTube)
{
    // Rings no larger than the motor mount do not count: the tab runs along the whole root.
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        addMount(body, 0.0125, 0.1, 0);
        auto small = makeRing(0.23, 0.005);
        small->setOuterRadiusAutomatic(false);
        small->setOuterRadius(0.01);
        body.addChild(std::move(small));
        auto touching = makeRing(0.27, 0.005);
        touching->setOuterRadiusAutomatic(false);
        touching->setOuterRadius(0.0125);
        body.addChild(std::move(touching));
        body.addChild(makeTrapezoid(0.08, 0.04, 0.05, 0.03));
    });
    EXPECT_EQ(builtDifferences(*document, "autotab small rings", 6, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyKeepsTheTabMethod)
{
    // A fin set in the middle of its parent, its tab placed relative to the bottom of the fin.
    static constexpr std::array kEdits{
        PanelEdit{.property = "TabOffsetMethod", .value = "select:BOTTOM"},
        PanelEdit{.property = "AutoCalc", .value = "button"},
    };
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        addMount(body, 0.0125, 0.2, 0);
        addRing(body, 0.1, 0.004);
        addRing(body, 0.13, 0.004);
        addRing(body, 0.2, 0.004);
        auto fins = makeTrapezoid(0.08, 0.04, 0.05, 0.03);
        fins->setAxialMethod(AxialMethod::MIDDLE);
        fins->setAxialOffset(0.01);
        body.addChild(std::move(fins));
    });
    EXPECT_EQ(builtDifferences(*document, "autotab middle fins", 7, kEdits), "");
}

TEST_F(FinPanelBuiltGolden, CalculateAutomaticallyForAnEllipticalFinSet)
{
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        addMount(body, 0.012, 0.1, 0);
        addRing(body, 0.235, 0.005);
        addRing(body, 0.29, 0.005);
        auto fins = std::make_unique<EllipticalFinSet>();
        fins->setLength(0.07);
        body.addChild(std::move(fins));
    });
    EXPECT_EQ(builtDifferences(*document, "autotab elliptical", 6, kAutoCalc), "");
}

TEST_F(FinPanelBuiltGolden, TwoTrapezoidalFinSetsEachTakeTheSweepOfTheirOwnHeight)
{
    // The user's decision U3: the sweep angle (45 degrees) gives each selected fin set the sweep
    // of its own height, the second one 3 cm. OpenRocket forwards the main one's setSweep(tan(45
    // degrees) times its 5 cm), which gives the second one 5 cm (the pin's line). Every other edit,
    // "Calculate automatically" among them (the main fin set's tab for both), is the same in both.
    static constexpr std::array kEdits{
        PanelEdit{.property = "SweepAngle", .value = "45"},
        PanelEdit{.property = "Height", .value = "4"},
        PanelEdit{.property = "TabLength", .value = "2"},
        PanelEdit{.property = "RootChord", .value = "7"},
        PanelEdit{.property = "CrossSection", .value = "select:AIRFOIL"},
        PanelEdit{.property = "FilletRadius", .value = "0.5"},
        PanelEdit{.property = "TabOffsetMethod", .value = "select:MIDDLE"},
        PanelEdit{.property = "AutoCalc", .value = "button"},
        PanelEdit{.property = "FinCount", .value = "int:2"},
        PanelEdit{.property = "CantAngle", .value = "3"},
        PanelEdit{.property = "TabHeight", .value = "2"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        addMount(body, 0.0125, 0.1, 0);
        addRing(body, 0.21, 0.005);
        addRing(body, 0.27, 0.005);
        body.addChild(makeTrapezoid(0.08, 0.04, 0.05, 0.03));
        auto second = makeTrapezoid(0.06, 0.02, 0.03, 0.01);
        second->setFinCount(4);
        body.addChild(std::move(second));
    });
    PanelReplay                               replay(*document, services, finLines());
    replay.multi(6, 7, kEdits);
    const std::vector<std::string_view> expected =
        withDifference(builtLines("trapezoid pair"), "== multi edit | SweepAngle 45",
                       "state other SweepUm=50000", "state other SweepUm=30000");
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expected), "");
}

TEST_F(FinPanelBuiltGolden, TwoEllipticalFinSetsTakeEachEdit)
{
    static constexpr std::array kEdits{
        PanelEdit{.property = "Length", .value = "6"},
        PanelEdit{.property = "Height", .value = "4"},
        PanelEdit{.property = "TabHeight", .value = "1"},
        PanelEdit{.property = "Thickness", .value = "0.5"},
        PanelEdit{.property = "TabOffset", .value = "0.2"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        body.addChild(std::make_unique<EllipticalFinSet>());
        auto second = std::make_unique<EllipticalFinSet>();
        second->setLength(0.04);
        second->setHeight(0.02);
        body.addChild(std::move(second));
    });
    PanelReplay                               replay(*document, services, finLines());
    replay.multi(3, 4, kEdits);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtLines("elliptical pair")), "");
}

// ------------------------------------------------------------------------------- the dialogs

/// The probe's finsDocument(): trapezoidal fin sets on a body tube, the main one at index 3 and
/// @p others more after it ("Trapezoidal fin set 2", ...), each of @p finCount fins.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> finsDocument(int others, int finCount)
{
    return onBodyTube([&](BodyTube& body) {
        for (int i = 0; i <= others; ++i)
        {
            auto fins = makeTrapezoid(0.05 + (0.01 * i), 0.03, 0.04, 0.02);
            fins->setFinCount(finCount);
            if (i > 0)
            {
                fins->setName(std::format("Trapezoidal fin set {}", i + 1));
            }
            body.addChild(std::move(fins));
        }
    });
}

/// The components of @p document in tree order.
[[nodiscard]] std::vector<RocketComponent*> preorder(OpenRocketDocument& document)
{
    std::vector<RocketComponent*> components;
    for (RocketComponent& component : document.getRocket().subtree(true))
    {
        components.push_back(&component);
    }
    return components;
}

/// The probe's part E: the configuration dialog of a window with a fin set panel, and what it
/// prints after each step.
class DialogReplay
{
public:
    DialogReplay(QWidget& window, const ConfigDialogServices& services)
      : m_window(&window), m_services(&services)
    {
    }

    [[nodiscard]] std::vector<std::string> take() { return std::exchange(m_lines, {}); }

    /// The probe's showFor(): the dialog of the component @p index of @p document with the
    /// @p others after it selected too (Java: its config listeners).
    void show(OpenRocketDocument& document, int others, std::ptrdiff_t index = 3)
    {
        const std::vector<RocketComponent*> components = preorder(document);
        const std::vector<RocketComponent*> selected(components.begin() + index + 1,
                                                     components.begin() + index + 1 + others);
        ComponentConfigDialog::showDialog(*m_window, m_services->services(), document,
                                          *components.at(static_cast<std::size_t>(index)),
                                          selected);
    }

    /// The panel of the open dialog.
    [[nodiscard]] static FinSetConfig& panel()
    {
        const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
        QTROCKET_ASSERT(dialog != nullptr);
        return dynamic_cast<FinSetConfig&>(dialog->configurator());
    }

    void step(std::string_view name) { m_lines.push_back(std::format("-- {}", name)); }

    void line(std::string text) { m_lines.push_back(std::move(text)); }

    /// The probe's dialogState(): the open dialog, the tree and the undo history.
    void state(OpenRocketDocument& document)
    {
        m_lines.push_back(dialogLine());
        const std::vector<RocketComponent*> components = preorder(document);
        for (std::size_t i = 0; i < components.size(); ++i)
        {
            const RocketComponent& component = *components[i];
            std::string text = std::format("tree {} {} {}", i, probeClassName(component),
                                           quoted(QtStrings::fromUtf8(component.getName())));
            if (const auto* fins = dynamic_cast<const FinSet*>(&component))
            {
                text += std::format(" fins={} rotation={}", fins->getFinCount(),
                                    javaRound(fins->getBaseRotation() * 180 / std::numbers::pi));
            }
            m_lines.push_back(std::move(text));
        }
        m_lines.push_back(std::format("undo={} redo={}", described(document.getUndoDescription()),
                                      described(document.getRedoDescription())));
    }

    /// The probe's closeAndUndo().
    void closeAndUndo(OpenRocketDocument& document)
    {
        ComponentConfigDialog::disposeDialog();
        step("after closing");
        state(document);
        undo(document);
        step("after undo");
        state(document);
    }

    /// An undo when there is one (the probe's guarded undo).
    void undo(OpenRocketDocument& document)
    {
        if (document.isUndoAvailable())
        {
            document.undo();
        }
        else
        {
            m_lines.emplace_back("undo not available");
        }
    }

private:
    /// "dialog none", or the open dialog's main component, title, tab and other components.
    [[nodiscard]] static std::string dialogLine()
    {
        const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
        if (dialog == nullptr || !dialog->isVisible())
        {
            return "dialog none";
        }
        const ComponentSelection& selection = *dialog->configurator().selection();
        std::string               others;
        for (const RocketComponent* other : selection.getOthers())
        {
            others += (others.empty() ? "" : " ") + quoted(QtStrings::fromUtf8(other->getName()));
        }
        const RocketComponent& main = selection.getMain();
        return std::format("dialog {} {} title={} tab={} others=[{}]", probeClassName(main),
                           quoted(QtStrings::fromUtf8(main.getName())),
                           quoted(dialog->windowTitle()), quoted(dialog->selectedTabName()),
                           others);
    }

    /// The probe's q() of an undo or redo description.
    [[nodiscard]] static std::string described(const std::optional<std::string>& description)
    {
        return description.has_value() ? '"' + *description + '"' : std::string("null");
    }

    QWidget*                    m_window;
    const ConfigDialogServices* m_services;
    std::vector<std::string>    m_lines;
};

/// The probe's convert(): the dialog of the main fin set and @p others, its tab "Fin tabs"
/// selected when @p finTabsTab, then "Convert to freeform".
[[nodiscard]] std::vector<std::string> convertFlow(DialogReplay& replay, int others,
                                                   bool finTabsTab)
{
    const std::unique_ptr<OpenRocketDocument> document = finsDocument(others, 3);
    replay.show(*document, others);
    if (finTabsTab)
    {
        DialogReplay::panel().setSelectedTab(QStringLiteral("Fin tabs"));
    }
    replay.step("shown");
    replay.state(*document);
    QPushButton* convert = DialogReplay::panel().convertButton();
    QTROCKET_ASSERT(convert != nullptr);
    convert->click();
    replay.step("after convert");
    replay.state(*document);
    replay.closeAndUndo(*document);
    return replay.take();
}

/// The probe's split(): the dialog of the main fin set and @p others, each of @p finCount fins,
/// then "Split fins" when it is enabled.
[[nodiscard]] std::vector<std::string> splitFlow(DialogReplay& replay, int others, int finCount)
{
    const std::unique_ptr<OpenRocketDocument> document = finsDocument(others, finCount);
    replay.show(*document, others);
    replay.step("shown");
    replay.state(*document);
    QPushButton* split = DialogReplay::panel().splitButton();
    QTROCKET_ASSERT(split != nullptr);
    replay.line(std::format("split enabled={}", split->isEnabled() ? "true" : "false"));
    split->click();
    replay.step("after split");
    replay.state(*document);
    replay.closeAndUndo(*document);
    return replay.take();
}

/// The fin set's values in the probe's autoCalcDialog(), sorted.
void addSortedFinState(DialogReplay& replay, OpenRocketDocument& document)
{
    std::vector<std::string> lines;
    addFinSetState(*preorder(document).at(6), "", false, lines);
    std::ranges::sort(lines);
    for (std::string& line : lines)
    {
        replay.line(std::move(line));
    }
}

/// The probe's autoCalcDialog(): an edit, "Calculate automatically", closing and two undos.
[[nodiscard]] std::vector<std::string> autoCalcFlow(DialogReplay& replay)
{
    static constexpr std::array               kRings{0.21, 0.27};
    const std::unique_ptr<OpenRocketDocument> document = mountAndRings(0.1, kRings);
    EXPECT_TRUE(settleAutomaticDimensions(document->getRocket()).has_value());
    replay.show(*document, 0, 6);
    replay.step("shown");
    replay.state(*document);
    DoubleBinding& thickness = *DialogReplay::panel().thicknessWidgets().spinBox->binding();
    EXPECT_TRUE(thickness.commitText("0.5"));
    replay.step("after an edit");
    replay.state(*document);
    DialogReplay::panel().autoCalcButton().click();
    replay.step("after calculate");
    replay.state(*document);
    addSortedFinState(replay, *document);
    ComponentConfigDialog::disposeDialog();
    replay.step("after closing");
    replay.state(*document);
    replay.undo(*document);
    replay.step("after undo");
    replay.state(*document);
    addSortedFinState(replay, *document);
    replay.undo(*document);
    replay.step("after a second undo");
    replay.state(*document);
    addSortedFinState(replay, *document);
    return replay.take();
}

/// The pinned lines of the dialog run @p name, as strings.
[[nodiscard]] std::vector<std::string> dialogPin(std::string_view name)
{
    const std::span pins(kFinDialogPins);
    const auto      found =
        std::ranges::find_if(pins, [name](const FinDialogPin& pin) { return pin.name == name; });
    QTROCKET_ASSERT(found != pins.end());
    return {found->lines.begin(), found->lines.end()};
}

/// Replaces the line @p javaLine of the step @p step ("-- <step>" up to the next step) of
/// @p lines by @p ourLines (none: removed): where QtRocket differs from the pin by design.
/// The line must be there.
void replaceInStep(std::vector<std::string>& lines, std::string_view step,
                   std::string_view javaLine, std::initializer_list<std::string_view> ourLines)
{
    const std::string header = std::format("-- {}", step);
    bool              inStep = false;
    for (auto line = lines.begin(); line != lines.end(); ++line)
    {
        if (line->starts_with("-- "))
        {
            inStep = *line == header;
        }
        else if (inStep && *line == javaLine)
        {
            line = lines.erase(line);
            lines.insert(line, ourLines.begin(), ourLines.end());
            return;
        }
    }
    ADD_FAILURE() << "the pin has no " << javaLine << " in " << header;
}

/// The differences between QtRocket's lines @p actual and @p expected.
[[nodiscard]] std::string flowDifferences(const std::vector<std::string>& actual,
                                          const std::vector<std::string>& expected)
{
    const std::vector<std::string_view> views(expected.begin(), expected.end());
    return pinnedLineDifferences(actual, views);
}

/// The dialog line of the converted main fin set with every other one selected.
constexpr std::string_view kAllConvertedThree =
    R"(dialog FreeformFinSet "Freeform Fin Set" title="(Multi-component) Freeform Fin Set )"
    R"(configuration" tab="Fin tabs" others=["Trapezoidal fin set 2" "Trapezoidal fin set 3"])";
constexpr std::string_view kAllConvertedFour =
    R"(dialog FreeformFinSet "Freeform Fin Set" title="(Multi-component) Freeform Fin Set )"
    R"(configuration" tab="Fin tabs" others=["Trapezoidal fin set 2" "Trapezoidal fin set 3" )"
    R"("Trapezoidal fin set 4"])";

/// The pins of "convert three", "convert four" and "split three" with QtRocket's differences:
/// every selected fin set is converted or split (OpenRocket's loop over its config listeners
/// stops after the first, or throws a ConcurrentModificationException with three: see
/// FinSetConfig::convertToFreeform()).
[[nodiscard]] std::vector<std::string> convertThreeExpected()
{
    std::vector<std::string> lines = dialogPin("convert three");
    replaceInStep(lines, "after convert",
                  R"(dialog FreeformFinSet "Freeform Fin Set" title="(Multi-component) Freeform )"
                  R"(Fin Set configuration" tab="Fin tabs" others=["Trapezoidal fin set 2"])",
                  {kAllConvertedThree});
    for (const std::string_view step : {"after convert", "after closing"})
    {
        replaceInStep(lines, step,
                      R"(tree 5 TrapezoidFinSet "Trapezoidal fin set 3" fins=3 rotation=0)",
                      {R"(tree 5 FreeformFinSet "Trapezoidal fin set 3" fins=3 rotation=0)"});
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> convertFourExpected()
{
    std::vector<std::string> lines = dialogPin("convert four");
    replaceInStep(lines, "shown", "uncaught java.util.ConcurrentModificationException", {});
    replaceInStep(lines, "after convert", "dialog none", {kAllConvertedFour});
    for (const std::string_view step : {"after convert", "after closing"})
    {
        replaceInStep(lines, step,
                      R"(tree 5 TrapezoidFinSet "Trapezoidal fin set 3" fins=3 rotation=0)",
                      {R"(tree 5 FreeformFinSet "Trapezoidal fin set 3" fins=3 rotation=0)"});
        replaceInStep(lines, step,
                      R"(tree 6 TrapezoidFinSet "Trapezoidal fin set 4" fins=3 rotation=0)",
                      {R"(tree 6 FreeformFinSet "Trapezoidal fin set 4" fins=3 rotation=0)"});
    }
    return lines;
}

[[nodiscard]] std::vector<std::string> splitThreeExpected()
{
    std::vector<std::string> lines = dialogPin("split three");
    for (const std::string_view step : {"after split", "after closing"})
    {
        replaceInStep(
            lines, step, R"(tree 9 TrapezoidFinSet "Trapezoidal fin set 3" fins=3 rotation=0)",
            {R"(tree 9 TrapezoidFinSet "Trapezoidal fin set 3 #1" fins=1 rotation=0)",
             R"(tree 10 TrapezoidFinSet "Trapezoidal fin set 3 #2" fins=1 rotation=120)",
             R"(tree 11 TrapezoidFinSet "Trapezoidal fin set 3 #3" fins=1 rotation=-120)"});
    }
    return lines;
}

/// The differences of every dialog run of @p actual from @p expected (the pins in the order of
/// kFinDialogPins), each run named.
[[nodiscard]] std::string allFlowDifferences(const std::vector<std::vector<std::string>>& actual,
                                             const std::vector<std::vector<std::string>>& expected)
{
    if (actual.size() != expected.size() || expected.size() != kFinDialogPins.size())
    {
        return "not every dialog run of the probe\n";
    }
    std::string differences;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const std::string run = flowDifferences(actual[i], expected[i]);
        if (!run.empty())
        {
            differences += std::format("{}:\n{}", kFinDialogPins.at(i).name, run);
        }
    }
    return differences;
}

TEST_F(FinDialogFlowsGolden, ConvertSplitAndCalculateAsOpenRocket)
{
    // The probe's part E in its order: the dialog remembers the tab of the one before (Java's
    // previousSelectedTab), which the fixture resets before the first.
    const ConfigDialogServices services;
    QWidget                    window;
    DialogReplay               replay(window, services);

    std::vector<std::vector<std::string>> expected;
    std::vector<std::vector<std::string>> actual;
    actual.push_back(convertFlow(replay, 0, false));
    expected.push_back(dialogPin("convert one"));
    actual.push_back(convertFlow(replay, 0, true));
    expected.push_back(dialogPin("convert one from fin tabs"));
    actual.push_back(convertFlow(replay, 1, false));
    expected.push_back(dialogPin("convert two"));
    actual.push_back(convertFlow(replay, 2, false));
    expected.push_back(convertThreeExpected());
    actual.push_back(convertFlow(replay, 3, false));
    expected.push_back(convertFourExpected());
    actual.push_back(splitFlow(replay, 0, 3));
    expected.push_back(dialogPin("split one"));
    actual.push_back(splitFlow(replay, 1, 3));
    expected.push_back(dialogPin("split two"));
    actual.push_back(splitFlow(replay, 2, 3));
    expected.push_back(splitThreeExpected());
    actual.push_back(splitFlow(replay, 0, 1));
    expected.push_back(dialogPin("split single fin"));
    actual.push_back(autoCalcFlow(replay));
    expected.push_back(dialogPin("autocalc"));
    EXPECT_EQ(allFlowDifferences(actual, expected), "");
}

TEST_F(FinDialogFlowsGolden, OpenRocketConvertsAndSplitsOnlyTwoSelectedFinSets)
{
    // The pins of the documented difference: OpenRocket leaves the third selected fin set as it
    // was and throws with a fourth (QtRocket converts and splits them all, see above).
    const std::vector<std::string> three = dialogPin("convert three");
    EXPECT_NE(std::ranges::find(
                  three, R"(tree 5 TrapezoidFinSet "Trapezoidal fin set 3" fins=3 rotation=0)"),
              three.end());
    const std::vector<std::string> four = dialogPin("convert four");
    EXPECT_NE(std::ranges::find(four, "uncaught java.util.ConcurrentModificationException"),
              four.end());
    const std::vector<std::string> split = dialogPin("split three");
    EXPECT_NE(std::ranges::find(
                  split, R"(tree 9 TrapezoidFinSet "Trapezoidal fin set 3" fins=3 rotation=0)"),
              split.end());
}

}  // namespace

}  // namespace QtRocket::Gui
