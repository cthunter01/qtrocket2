// OpenRocket's panels of the tube fin set, the launch lug and the rail button (TubeFinSetConfig,
// LaunchLugConfig, RailButtonConfig, OpenRocket 5f164fd0e) made by D1bProbe under Xvfb, against
// QtRocket's in the same state. The probe printed, and this test replays:
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
#include <cstddef>
#include <format>
#include <functional>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QObject>
#include <QString>
#include <QTabWidget>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "LugPanelPins.h"
#include "PanelProbeLines.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using LugPanelBuiltGolden = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::javaBool;
using Test::kLugPanelPins;
using Test::kLugPanelPinsCommit;
using Test::LugPanelPin;
using Test::pinnedLineDifferences;
using Test::quoted;

/// Java's Double.toString() of @p value (JDK 17, which ran the probe).
[[nodiscard]] std::string javaDouble(double value)
{
    return FloatingDecimal::toJavaFormatString(value);
}

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

/// An edit of the probe: the property (the widget's objectName) and what is done to it: the text
/// its spin box commits, "int:<n>" for a count, "click" for a check box.
struct Edit
{
    std::string_view property;
    std::string_view value;
};

/// The count of an edit "int:<n>".
[[nodiscard]] int editCount(std::string_view value)
{
    value.remove_prefix(std::string_view("int:").size());
    bool      parsed = false;
    const int count  = QtStrings::fromUtf8(value).toInt(&parsed);
    QTROCKET_ASSERT(parsed);
    return count;
}

/// Makes @p edit through the widgets of @p panel, as the probe's apply() does through the Swing
/// panel's models (the spinner model's setValue() of the text or the count, doClick()): whether
/// the widget is there and took the edit.
[[nodiscard]] bool applyEdit(const RocketComponentConfig& panel, const Edit& edit)
{
    const QString name = QtStrings::fromUtf8(edit.property);
    if (edit.value == "click")
    {
        auto* box = panel.findChild<BoolCheckBox*>(name);
        if (box == nullptr)
        {
            return false;
        }
        box->click();
        return true;
    }
    if (edit.value.starts_with("int:"))
    {
        const auto* count = panel.findChild<IntSpinBox*>(name);
        if (count == nullptr)
        {
            return false;
        }
        IntBinding& binding = *count->binding();
        return binding.setSpinnerValue(editCount(edit.value));
    }
    const auto* spin = panel.findChild<UnitSpinBox*>(name);
    if (spin == nullptr)
    {
        return false;
    }
    DoubleBinding& binding = *spin->binding();
    return binding.commitText(edit.value);
}

/// The probe's replay of one design: the sections it prints, each a header and its sorted lines.
class DesignReplay
{
public:
    DesignReplay(OpenRocketDocument& document, const ConfigDialogServices& services)
      : m_document(&document), m_services(&services)
    {
        for (RocketComponent& component : document.getRocket().subtree(true))
        {
            m_components.push_back(&component);
        }
    }

    [[nodiscard]] const std::vector<std::string>& lines() const noexcept { return m_lines; }

    /// The probe's design(): the panel of every tube fin set, launch lug and rail button.
    void panels()
    {
        settle();
        for (std::size_t i = 0; i < m_components.size(); ++i)
        {
            RocketComponent& component = *m_components[i];
            if (!isLugKind(component.kind()))
            {
                continue;
            }
            settle();
            m_lines.push_back(
                std::format("== panel | {} | {}", i, probeClassName(component.kind())));
            std::vector<std::string> section;
            {
                const FactoryPanel<> made(*m_document, *m_services, component);
                addGeneralLines(*made, true, section);
            }
            addStateLines(component, "", section);
            addSection(std::move(section));
        }
    }

    /// The probe's edits(): the panel of the component @p index, then @p edits through it.
    void edits(std::size_t index, std::span<const Edit> edits)
    {
        settle();
        RocketComponent& component = *m_components.at(index);
        m_lines.push_back(
            std::format("== panel | {} | {}", index, probeClassName(component.kind())));
        const FactoryPanel<>     made(*m_document, *m_services, component);
        std::vector<std::string> section;
        addGeneralLines(*made, true, section);
        addStateLines(component, "", section);
        addSection(std::move(section));
        for (const Edit& edit : edits)
        {
            EXPECT_TRUE(applyEdit(*made, edit)) << edit.property << ' ' << edit.value;
            m_lines.push_back(
                std::format("== edit | {} | {} {}", index, edit.property, edit.value));
            std::vector<std::string> edited;
            addGeneralLines(*made, false, edited);
            addStateLines(component, "", edited);
            addSection(std::move(edited));
        }
    }

    /// The probe's multi(): the panel of the component @p main with the component @p other
    /// selected too, and @p edits through it.
    void multi(std::size_t main, std::size_t other, std::span<const Edit> edits)
    {
        settle();
        RocketComponent& mainComponent  = *m_components.at(main);
        RocketComponent& otherComponent = *m_components.at(other);
        m_lines.push_back(std::format("== multi | {} | {} | {}", main, other,
                                      probeClassName(mainComponent.kind())));
        addBothStates(mainComponent, otherComponent);
        const std::array<RocketComponent*, 1> others{&otherComponent};
        const FactoryPanel<>                  made(*m_document, *m_services, mainComponent, others);
        for (const Edit& edit : edits)
        {
            EXPECT_TRUE(applyEdit(*made, edit)) << edit.property << ' ' << edit.value;
            m_lines.push_back(std::format("== multi edit | {} {}", edit.property, edit.value));
            addBothStates(mainComponent, otherComponent);
        }
    }

private:
    void settle() const
    {
        const Result<int> settled = settleAutomaticDimensions(m_document->getRocket());
        EXPECT_TRUE(settled.has_value()) << settled.error().message;
    }

    void addBothStates(const RocketComponent& main, const RocketComponent& other)
    {
        std::vector<std::string> section;
        addStateLines(main, "main ", section);
        addStateLines(other, "other ", section);
        addSection(std::move(section));
    }

    void addSection(std::vector<std::string> section)
    {
        std::ranges::sort(section);
        for (std::string& line : section)
        {
            m_lines.push_back(std::move(line));
        }
    }

    OpenRocketDocument*           m_document;
    const ConfigDialogServices*   m_services;
    std::vector<RocketComponent*> m_components;
    std::vector<std::string>      m_lines;
};

/// The document of a pinned example or test rocket.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> pinnedDocument(const LugPanelPin& pin)
{
    if (pin.source == "example")
    {
        const QtRocket::Test::GoldenExample* example =
            QtRocket::Test::findGoldenExample(pin.design);
        if (example == nullptr)
        {
            return nullptr;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().message;
            return nullptr;
        }
        return std::move(loaded->document);
    }
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        if (maker.method == pin.design)
        {
            return OpenRocketDocumentFactory::createDocumentFromRocket(maker.make());
        }
    }
    return nullptr;
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

/// The probe's onBodyTube(): Rocket > Stage > a body tube 30 cm long with a fixed radius of
/// 2.1 cm, to which @p addChildren adds the components; with its events on.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> onBodyTube(
    const std::function<void(BodyTube&)>& addChildren)
{
    auto        rocket = std::make_unique<Rocket>();
    AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto        body   = std::make_unique<BodyTube>();
    body->setOuterRadiusAutomatic(false);
    body->setOuterRadius(0.021);
    body->setLength(0.3);
    addChildren(stage.addChild(std::move(body)));
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

/// @p pinned with @p javaLine, a line of the sections after the header @p header, replaced by
/// @p ourLine: where QtRocket differs from the pin by design. The replacement must happen.
[[nodiscard]] std::vector<std::string_view> withDifference(std::span<const std::string_view> pinned,
                                                           std::string_view                  header,
                                                           std::string_view javaLine,
                                                           std::string_view ourLine)
{
    std::vector<std::string_view> lines;
    bool                          after    = false;
    bool                          replaced = false;
    for (const std::string_view line : pinned)
    {
        after = after || line == header;
        if (after && line == javaLine)
        {
            lines.push_back(ourLine);
            replaced = true;
        }
        else
        {
            lines.push_back(line);
        }
    }
    EXPECT_TRUE(replaced) << "the pin has no " << javaLine << " after " << header;
    return lines;
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
    const std::unique_ptr<OpenRocketDocument> document = pinnedDocument(pin);
    ASSERT_NE(document, nullptr) << "no design " << pin.design;
    DesignReplay replay(*document, services);
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
        Edit{.property = "FinCount", .value = "int:2"},
        Edit{.property = "FinCount", .value = "int:9"},
        Edit{.property = "OuterRadius", .value = "3"},
        Edit{.property = "OuterRadiusAutomatic", .value = "click"},
        Edit{.property = "OuterRadius", .value = "4"},
        Edit{.property = "Thickness", .value = "10"},
        Edit{.property = "InnerRadius", .value = "1"},
        Edit{.property = "BaseRotation", .value = "200"},
        Edit{.property = "BaseRotation", .value = "45"},
        Edit{.property = "Length", .value = "5"},
        Edit{.property = "FinCount", .value = "int:0"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<TubeFinSet>()); });
    DesignReplay replay(*document, services);
    replay.edits(3, kEdits);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtLines("tube fins")), "");
}

TEST_F(LugPanelBuiltGolden, EditsOfALaunchLug)
{
    // The inner diameter moving the outer one (the wall stays), the thickness clamped to the outer
    // radius, the rotation clamped to 180 degrees by the field's bounds, the instance count to at
    // least 1.
    static constexpr std::array kEdits{
        Edit{.property = "Length", .value = "4"},
        Edit{.property = "OuterRadius", .value = "1.5"},
        Edit{.property = "InnerRadius", .value = "1"},
        Edit{.property = "Thickness", .value = "1"},
        Edit{.property = "AngleOffset", .value = "200"},
        Edit{.property = "AngleOffset", .value = "-45"},
        Edit{.property = "InstanceCount", .value = "int:3"},
        Edit{.property = "InstanceSeparation", .value = "5"},
        Edit{.property = "InstanceCount", .value = "int:0"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<LaunchLug>()); });
    DesignReplay replay(*document, services);
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
        Edit{.property = "OuterDiameter", .value = "0.5"},
        Edit{.property = "InnerDiameter", .value = "2"},
        Edit{.property = "OuterDiameter", .value = "1.2"},
        Edit{.property = "FlangeHeight", .value = "1"},
        Edit{.property = "TotalHeight", .value = "0.1"},
        Edit{.property = "BaseHeight", .value = "0.1"},
        Edit{.property = "TotalHeight", .value = "1.5"},
        Edit{.property = "ScrewHeight", .value = "-1"},
        Edit{.property = "ScrewHeight", .value = "0.3"},
        Edit{.property = "AngleOffset", .value = "200"},
        Edit{.property = "AngleOffset", .value = "-20000"},
        Edit{.property = "InstanceCount", .value = "int:2"},
        Edit{.property = "InstanceSeparation", .value = "10"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        onBodyTube([](BodyTube& body) { body.addChild(std::make_unique<RailButton>()); });
    DesignReplay replay(*document, services);
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
        Edit{.property = "InnerRadius", .value = "0.6"},
        Edit{.property = "Length", .value = "6"},
        Edit{.property = "AngleOffset", .value = "90"},
        Edit{.property = "Thickness", .value = "0.05"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        body.addChild(std::make_unique<LaunchLug>());
        auto second = std::make_unique<LaunchLug>();
        second->setOuterRadius(0.004);
        second->setThickness(0.0005);
        body.addChild(std::move(second));
    });
    DesignReplay                              replay(*document, services);
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
        Edit{.property = "InnerRadius", .value = "1"},
        Edit{.property = "FinCount", .value = "int:3"},
        Edit{.property = "BaseRotation", .value = "30"},
        Edit{.property = "OuterRadiusAutomatic", .value = "click"},
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
    DesignReplay replay(*document, services);
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
        Edit{.property = "OuterDiameter", .value = "0.9"},
        Edit{.property = "FlangeHeight", .value = "0.3"},
        Edit{.property = "AngleOffset", .value = "-90"},
    };
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = onBodyTube([](BodyTube& body) {
        body.addChild(std::make_unique<RailButton>());
        auto second = std::make_unique<RailButton>();
        second->setInnerDiameter(0.006);
        body.addChild(std::move(second));
    });
    DesignReplay                              replay(*document, services);
    replay.multi(3, 4, kEdits);
    const std::vector<std::string_view> expected =
        withDifference(builtLines("rail buttons"), "== multi edit | OuterDiameter 0.9",
                       "state other InnerDiameter=0.008", "state other InnerDiameter=0.006");
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expected), "");
}

}  // namespace

}  // namespace QtRocket::Gui
