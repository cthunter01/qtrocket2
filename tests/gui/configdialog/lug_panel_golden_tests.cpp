// OpenRocket's panels of the tube fin set, the launch lug and the rail button (TubeFinSetConfig,
// LaunchLugConfig, RailButtonConfig, OpenRocket 5f164fd0e) made by LugPanelProbe
// (tools/openrocket-panel-pins) under Xvfb, against QtRocket's in the same state. The probe
// printed, and this test replays:
// - "panel": the panel of every tube fin set, launch lug and rail button of the 16 examples (loaded
//   without presets) and the 13 test rockets, in tree order, each after the design was settled
//   (settleAutomaticDimensions()): the General tab's labels (with their tooltips), spin box texts
//   (the counts too), unit selectors, slider positions, check boxes, the placement's combo box
//   (the material panel left out), the first tab's title and tooltip, and the component's values
//   (its counts, flags and those lengths and angles no sine enters);
// - "edit": on designs of the probe's own (a body tube 30 cm long with a fixed radius of 2.1 cm
//   holding the component), edits through the panel, as a user makes them: the text a spin box
//   commits (in its unit: "3" is 3 cm, "200" is 200 degrees), a count, a check box clicked; after
//   each the tab's widgets but the labels, and the component's values;
// - "multi" and "multi edit": a panel with a second component of its kind selected (Java: the
//   config listener of the first) and edits through it, after each the values of both.
// The lines of each section are sorted. The pins are LugPanelPins.h, generated from the probe's
// output.
#include <algorithm>
#include <array>
#include <format>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QTabWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "LugPanelPins.h"
#include "PanelProbeLines.h"
#include "PanelReplay.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using LugPanelBuiltGolden = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::javaBool;
using Test::javaDouble;
using Test::kLugPanelPins;
using Test::kLugPanelPinsCommit;
using Test::LugPanelPin;
using Test::onBodyTube;
using Test::PanelEdit;
using Test::PanelReplay;
using Test::pinnedDesignDocument;
using Test::pinnedLineDifferences;
using Test::quoted;
using Test::withDifference;

/// Whether @p kind is one of the kinds the probe opens a panel for.
[[nodiscard]] bool isLugKind(ComponentKind kind)
{
    return kind == ComponentKind::TUBE_FIN_SET || kind == ComponentKind::LAUNCH_LUG ||
           kind == ComponentKind::RAIL_BUTTON;
}

/// The class name the probe prints for @p kind.
[[nodiscard]] std::string_view probeClassName(ComponentKind kind)
{
    switch (kind)
    {
        case ComponentKind::TUBE_FIN_SET:
            return "TubeFinSet";
        case ComponentKind::LAUNCH_LUG:
            return "LaunchLug";
        default:
            return "RailButton";
    }
}

/// The probe's state() of a tube fin set: the outer radius and the thickness only while the radius
/// is not automatic (the automatic one comes from a sine).
void addTubeFinSetState(const TubeFinSet& fins, std::string_view prefix,
                        std::vector<std::string>& lines)
{
    lines.push_back(std::format("state {}FinCount={}", prefix, fins.getFinCount()));
    lines.push_back(std::format("state {}OuterRadiusAutomatic={}", prefix,
                                javaBool(fins.isOuterRadiusAutomatic())));
    lines.push_back(std::format("state {}Length={}", prefix, javaDouble(fins.getLength())));
    lines.push_back(
        std::format("state {}BaseRotation={}", prefix, javaDouble(fins.getBaseRotation())));
    if (!fins.isOuterRadiusAutomatic())
    {
        lines.push_back(
            std::format("state {}OuterRadius={}", prefix, javaDouble(fins.getOuterRadius())));
        lines.push_back(
            std::format("state {}Thickness={}", prefix, javaDouble(fins.getThickness())));
    }
}

/// The probe's state() of a launch lug.
void addLaunchLugState(const LaunchLug& lug, std::string_view prefix,
                       std::vector<std::string>& lines)
{
    lines.push_back(std::format("state {}Length={}", prefix, javaDouble(lug.getLength())));
    lines.push_back(
        std::format("state {}OuterRadius={}", prefix, javaDouble(lug.getOuterRadius())));
    lines.push_back(std::format("state {}Thickness={}", prefix, javaDouble(lug.getThickness())));
    lines.push_back(
        std::format("state {}AngleOffset={}", prefix, javaDouble(lug.getAngleOffset())));
    lines.push_back(std::format("state {}InstanceCount={}", prefix, lug.getInstanceCount()));
    lines.push_back(std::format("state {}InstanceSeparation={}", prefix,
                                javaDouble(lug.getInstanceSeparation())));
}

/// The probe's state() of a rail button.
void addRailButtonState(const RailButton& button, std::string_view prefix,
                        std::vector<std::string>& lines)
{
    lines.push_back(
        std::format("state {}OuterDiameter={}", prefix, javaDouble(button.getOuterDiameter())));
    lines.push_back(
        std::format("state {}InnerDiameter={}", prefix, javaDouble(button.getInnerDiameter())));
    lines.push_back(
        std::format("state {}BaseHeight={}", prefix, javaDouble(button.getBaseHeight())));
    lines.push_back(
        std::format("state {}FlangeHeight={}", prefix, javaDouble(button.getFlangeHeight())));
    lines.push_back(
        std::format("state {}TotalHeight={}", prefix, javaDouble(button.getTotalHeight())));
    lines.push_back(
        std::format("state {}ScrewHeight={}", prefix, javaDouble(button.getScrewHeight())));
    lines.push_back(
        std::format("state {}AngleOffset={}", prefix, javaDouble(button.getAngleOffset())));
    lines.push_back(std::format("state {}InstanceCount={}", prefix, button.getInstanceCount()));
    lines.push_back(std::format("state {}InstanceSeparation={}", prefix,
                                javaDouble(button.getInstanceSeparation())));
}

/// The probe's state(): the values of @p component.
void addStateLines(const RocketComponent& component, std::string_view prefix,
                   std::vector<std::string>& lines)
{
    if (const auto* fins = dynamic_cast<const TubeFinSet*>(&component))
    {
        addTubeFinSetState(*fins, prefix, lines);
    }
    else if (const auto* lug = dynamic_cast<const LaunchLug*>(&component))
    {
        addLaunchLugState(*lug, prefix, lines);
    }
    else if (const auto* button = dynamic_cast<const RailButton*>(&component))
    {
        addRailButtonState(*button, prefix, lines);
    }
}

/// The probe's general(): with the labels, the first tab's title and tooltip and the widgets of
/// the General tab; without them (after an edit), the widgets but the labels.
void addGeneralLines(const RocketComponentConfig& panel, bool withLabels,
                     std::vector<std::string>& lines)
{
    const QTabWidget& tabs = panel.tabs();
    if (withLabels)
    {
        lines.push_back(
            std::format("tab0 {} tip={}", quoted(tabs.tabText(0)), quoted(tabs.tabToolTip(0))));
    }
    Test::addTabWidgetLines(*tabs.widget(0), {.labels = withLabels, .labelTips = true}, lines);
}

/// What the replay prints of the tube fin sets, launch lugs and rail buttons (LugPanelProbe's).
[[nodiscard]] Test::PanelReplayLines lugLines()
{
    return {
        .isPanelKind = [](const RocketComponent& component) { return isLugKind(component.kind()); },
        .className =
            [](const RocketComponent& component) { return probeClassName(component.kind()); },
        .panel = addGeneralLines,
        .state = [](const RocketComponent& component, std::string_view prefix, bool /*fromDesign*/,
                    std::vector<std::string>& lines) { addStateLines(component, prefix, lines); },
    };
}

/// The pins of the examples and the test rockets.
[[nodiscard]] std::vector<LugPanelPin> designPins()
{
    std::vector<LugPanelPin> pins;
    std::ranges::copy_if(kLugPanelPins, std::back_inserter(pins),
                         [](const LugPanelPin& pin) { return pin.source != "built"; });
    return pins;
}

/// The lines of the pin of the probe's own design @p design.
[[nodiscard]] std::span<const std::string_view> builtLines(std::string_view design)
{
    const auto* found = std::ranges::find_if(kLugPanelPins, [design](const LugPanelPin& pin) {
        return pin.source == "built" && pin.design == design;
    });
    QTROCKET_ASSERT(found != kLugPanelPins.end());
    return found->lines;
}

class LugPanelGolden : public Test::GuiTest, public ::testing::WithParamInterface<LugPanelPin>
{ };

TEST(LugPanelPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(kLugPanelPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(kLugPanelPins.size(), 25U);
    EXPECT_EQ(designPins().size(), 19U);
}

TEST_P(LugPanelGolden, ShowsAsOpenRocket)
{
    const LugPanelPin&                        pin = GetParam();
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        pinnedDesignDocument(pin.source, pin.design);
    ASSERT_NE(document, nullptr) << "no design " << pin.design;
    PanelReplay replay(*document, services, lugLines());
    replay.panels();
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), pin.lines), "");
}

INSTANTIATE_TEST_SUITE_P(Designs, LugPanelGolden, ::testing::ValuesIn(designPins()),
                         [](const ::testing::TestParamInfo<LugPanelPin>& param) {
                             return std::string(param.param.testName);
                         });

TEST_F(LugPanelBuiltGolden, EditsOfATubeFinSet)
{
    // The fin count clamped to 1 ... 8 by the field, the automatic outer diameter switched off by a
    // typed one and on again, the thickness clamped to the outer radius, the inner diameter
    // setting the thickness, the fin rotation reduced to -180 ... 180 degrees by the setter.
    static constexpr std::array kEdits{
        PanelEdit{.property = "FinCount", .value = "int:2"},
        PanelEdit{.property = "FinCount", .value = "int:9"},
        PanelEdit{.property = "OuterRadius", .value = "3"},
        PanelEdit{.property = "OuterRadiusAutomatic", .value = "click"},
        PanelEdit{.property = "OuterRadius", .value = "4"},
        PanelEdit{.property = "Thickness", .value = "10"},
        PanelEdit{.property = "InnerRadius", .value = "1"},
        PanelEdit{.property = "BaseRotation", .value = "200"},
        PanelEdit{.property = "BaseRotation", .value = "45"},
        PanelEdit{.property = "Length", .value = "5"},
        PanelEdit{.property = "FinCount", .value = "int:0"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<TubeFinSet>()); });
    PanelReplay replay(*document, services, lugLines());
    replay.edits(3, kEdits);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtLines("tube fins")), "");
}

TEST_F(LugPanelBuiltGolden, EditsOfALaunchLug)
{
    // The inner diameter moving the outer one (the wall stays), the thickness clamped to the outer
    // radius, the rotation clamped to 180 degrees by the field's bounds, the instance count to at
    // least 1.
    static constexpr std::array kEdits{
        PanelEdit{.property = "Length", .value = "4"},
        PanelEdit{.property = "OuterRadius", .value = "1.5"},
        PanelEdit{.property = "InnerRadius", .value = "1"},
        PanelEdit{.property = "Thickness", .value = "1"},
        PanelEdit{.property = "AngleOffset", .value = "200"},
        PanelEdit{.property = "AngleOffset", .value = "-45"},
        PanelEdit{.property = "InstanceCount", .value = "int:3"},
        PanelEdit{.property = "InstanceSeparation", .value = "5"},
        PanelEdit{.property = "InstanceCount", .value = "int:0"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<LaunchLug>()); });
    PanelReplay replay(*document, services, lugLines());
    replay.edits(3, kEdits);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtLines("launch lug")), "");
}

TEST_F(LugPanelBuiltGolden, EditsOfARailButton)
{
    // The heights constraining each other and the sliders following their bounds, the inner
    // diameter at most the outer one, the screw height at least 0, and the rotation: OpenRocket's
    // bounds of -180 and 180 radians (kept) let 200 degrees through to the setter, which clamps it
    // to 180 degrees, and clamp -20000 degrees to -180 radians, which the setter clamps to -180
    // degrees.
    static constexpr std::array kEdits{
        PanelEdit{.property = "OuterDiameter", .value = "0.5"},
        PanelEdit{.property = "InnerDiameter", .value = "2"},
        PanelEdit{.property = "OuterDiameter", .value = "1.2"},
        PanelEdit{.property = "FlangeHeight", .value = "1"},
        PanelEdit{.property = "TotalHeight", .value = "0.1"},
        PanelEdit{.property = "BaseHeight", .value = "0.1"},
        PanelEdit{.property = "TotalHeight", .value = "1.5"},
        PanelEdit{.property = "ScrewHeight", .value = "-1"},
        PanelEdit{.property = "ScrewHeight", .value = "0.3"},
        PanelEdit{.property = "AngleOffset", .value = "200"},
        PanelEdit{.property = "AngleOffset", .value = "-20000"},
        PanelEdit{.property = "InstanceCount", .value = "int:2"},
        PanelEdit{.property = "InstanceSeparation", .value = "10"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<RailButton>()); });
    PanelReplay replay(*document, services, lugLines());
    replay.edits(3, kEdits);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtLines("rail button")), "");
}

TEST_F(LugPanelBuiltGolden, TwoLaunchLugsEachKeepTheirWall)
{
    // The user's decision U3: the inner diameter (0.6 cm) keeps each selected lug's own wall, so
    // the second lug's outer radius becomes 0.3 cm + 0.05 cm. OpenRocket forwards the main lug's
    // setOuterRadius(r + its 0.1 cm wall), which gives the second one 0.4 cm (the pin's line).
    // The other edits are the same in both.
    static constexpr std::array kEdits{
        PanelEdit{.property = "InnerRadius", .value = "0.6"},
        PanelEdit{.property = "Length", .value = "6"},
        PanelEdit{.property = "AngleOffset", .value = "90"},
        PanelEdit{.property = "Thickness", .value = "0.05"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        body.addChild(std::make_unique<LaunchLug>());
        auto second = std::make_unique<LaunchLug>();
        second->setOuterRadius(0.004);
        second->setThickness(0.0005);
        body.addChild(std::move(second));
    });
    PanelReplay                               replay(*document, services, lugLines());
    replay.multi(3, 4, kEdits);
    const std::vector<std::string_view> expected =
        withDifference(builtLines("launch lugs"), "== multi edit | InnerRadius 0.6",
                       "state other OuterRadius=0.004", "state other OuterRadius=0.0035");
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expected), "");
}

TEST_F(LugPanelBuiltGolden, TwoTubeFinSetsEachTakeTheirOwnOuterRadius)
{
    // The user's decision U3: the inner diameter (1 cm) gives each selected tube fin set the wall
    // of its own outer radius, the second one 0.8 cm - 0.5 cm. OpenRocket forwards the main one's
    // setThickness(its 1 cm - 0.5 cm), which gives the second one 0.5 cm (the pin's line).
    static constexpr std::array kEdits{
        PanelEdit{.property = "InnerRadius", .value = "1"},
        PanelEdit{.property = "FinCount", .value = "int:3"},
        PanelEdit{.property = "BaseRotation", .value = "30"},
        PanelEdit{.property = "OuterRadiusAutomatic", .value = "click"},
    };
    const ConfigDialogServices                services;
    TubeFinSet*                               first    = nullptr;
    TubeFinSet*                               second   = nullptr;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([&](BodyTube& body) {
        first  = &body.addChild(std::make_unique<TubeFinSet>());
        second = &body.addChild(std::make_unique<TubeFinSet>());
    });
    first->setOuterRadius(0.01);
    first->setThickness(0.001);
    second->setOuterRadius(0.008);
    second->setThickness(0.002);
    PanelReplay replay(*document, services, lugLines());
    replay.multi(3, 4, kEdits);
    const std::vector<std::string_view> expected =
        withDifference(builtLines("tube fin sets"), "== multi edit | InnerRadius 1",
                       "state other Thickness=0.005", "state other Thickness=0.003");
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expected), "");
}

TEST_F(LugPanelBuiltGolden, TwoRailButtonsEachKeepTheirInnerDiameter)
{
    // The user's decision U3: the outer diameter (0.9 cm) keeps each selected rail button's own
    // inner diameter, the second one's 0.6 cm. OpenRocket forwards the main button's
    // setInnerDiameter(its 0.8 cm), which gives the second one 0.8 cm (the pin's line).
    static constexpr std::array kEdits{
        PanelEdit{.property = "OuterDiameter", .value = "0.9"},
        PanelEdit{.property = "FlangeHeight", .value = "0.3"},
        PanelEdit{.property = "AngleOffset", .value = "-90"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        body.addChild(std::make_unique<RailButton>());
        auto second = std::make_unique<RailButton>();
        second->setInnerDiameter(0.006);
        body.addChild(std::move(second));
    });
    PanelReplay                               replay(*document, services, lugLines());
    replay.multi(3, 4, kEdits);
    const std::vector<std::string_view> expected =
        withDifference(builtLines("rail buttons"), "== multi edit | OuterDiameter 0.9",
                       "state other InnerDiameter=0.008", "state other InnerDiameter=0.006");
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expected), "");
}

}  // namespace

}  // namespace QtRocket::Gui
