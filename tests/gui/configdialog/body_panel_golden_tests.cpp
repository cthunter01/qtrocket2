// OpenRocket's panels of the body components (BodyTubeConfig, NoseConeConfig, TransitionConfig,
// OpenRocket 5f164fd0e) made by BodyPanelProbe (tools/openrocket-panel-pins) under Xvfb, against
// QtRocket's in the same state. For each design (the 16 examples loaded without presets, the 13
// test rockets, the probe's own designs) the probe replays, and so does this test:
// - "panel": the panel of every body tube, nose cone and transition, in tree order, each after
//   the design was settled (settleAutomaticDimensions(): the automatic radii are stored lazily,
//   and the "Automatic" check boxes look at them), and then let go: the General tab's labels,
//   spin box texts, unit selectors, slider positions, check boxes (checked, enabled, tooltip) and
//   the shape combo box, the titles and tooltips of the first two tabs, and the automatic radii of
//   the component after the panel was made (an "Automatic" that cannot be used switches them off);
// - "flip", "unflip": "Flip to tail cone" of every nose cone's panel clicked, and clicked again:
//   "Automatic" decided again for the new orientation, and the nose cone's state;
// - "multi": the panel of a component whose "Automatic" check box is disabled, with the first
//   other component of its class whose automatic radius is on as the other selected component
//   (Java: its config listener): what the switch-off does to both.
// The lines of each section are sorted. The pins are BodyPanelPins.h, generated from the probe's
// output.
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
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
#include <QWidget>

#include <gtest/gtest.h>

#include "BodyPanelPins.h"
#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "PanelProbeLines.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/rocket/AutomaticDimensions.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using BodyPanelBuiltGolden = Test::GuiTest;
using Test::BodyPanelPin;
using Test::checkLine;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::javaBool;
using Test::kBodyPanelPins;
using Test::kBodyPanelPinsCommit;
using Test::pinnedLineDifferences;
using Test::quoted;

/// Whether @p kind is one of the kinds the probe opens a panel for.
[[nodiscard]] bool isBodyKind(ComponentKind kind)
{
    return kind == ComponentKind::BODY_TUBE || kind == ComponentKind::NOSE_CONE ||
           kind == ComponentKind::TRANSITION;
}

/// The class name the probe prints for @p kind.
[[nodiscard]] std::string_view probeClassName(ComponentKind kind)
{
    switch (kind)
    {
        case ComponentKind::BODY_TUBE:
            return "BodyTube";
        case ComponentKind::NOSE_CONE:
            return "NoseCone";
        default:
            return "Transition";
    }
}

/// The probe's state(): the automatic radii (and a nose cone's orientation) of @p component.
void addStateLines(const RocketComponent& component, std::string_view prefix,
                   std::vector<std::string>& lines)
{
    if (const auto* nose = dynamic_cast<const NoseCone*>(&component))
    {
        lines.push_back(std::format("state {}BaseRadiusAutomatic={}", prefix,
                                    javaBool(nose->isBaseRadiusAutomatic())));
        lines.push_back(std::format("state {}Flipped={}", prefix, javaBool(nose->isFlipped())));
        lines.push_back(std::format("state {}ForeRadiusAutomatic={}", prefix,
                                    javaBool(nose->isForeRadiusAutomatic())));
        lines.push_back(std::format("state {}AftRadiusAutomatic={}", prefix,
                                    javaBool(nose->isAftRadiusAutomatic())));
    }
    else if (const auto* tube = dynamic_cast<const BodyTube*>(&component))
    {
        lines.push_back(std::format("state {}OuterRadiusAutomatic={}", prefix,
                                    javaBool(tube->isOuterRadiusAutomatic())));
    }
    else if (const auto* transition = dynamic_cast<const Transition*>(&component))
    {
        lines.push_back(std::format("state {}ForeRadiusAutomatic={}", prefix,
                                    javaBool(transition->isForeRadiusAutomatic())));
        lines.push_back(std::format("state {}AftRadiusAutomatic={}", prefix,
                                    javaBool(transition->isAftRadiusAutomatic())));
    }
}

/// The probe's general(): the first two tabs and the widgets of the General tab.
void addGeneralLines(const RocketComponentConfig& panel, std::vector<std::string>& lines)
{
    const QTabWidget& tabs = panel.tabs();
    for (int tab = 0; tab < 2; ++tab)
    {
        lines.push_back(std::format("tab{} {} tip={}", tab, quoted(tabs.tabText(tab)),
                                    quoted(tabs.tabToolTip(tab))));
    }
    Test::addTabWidgetLines(*tabs.widget(0), {.labels = true, .labelTips = false}, lines);
}

/// The probe's replay of one design (design()): the sections it prints, each a header and its
/// sorted lines.
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

    /// Part A: every panel.
    void panels()
    {
        for (std::size_t i = 0; i < m_components.size(); ++i)
        {
            RocketComponent& component = *m_components[i];
            if (!isBodyKind(component.kind()))
            {
                continue;
            }
            settle();
            m_lines.push_back(
                std::format("== panel | {} | {}", i, probeClassName(component.kind())));
            std::vector<std::string> section;
            {
                const FactoryPanel<> made(*m_document, *m_services, component);
                addGeneralLines(*made, section);
            }
            addStateLines(component, "", section);
            addSection(std::move(section));
        }
    }

    /// Part B: "Flip to tail cone" clicked on and off.
    void flips()
    {
        for (std::size_t i = 0; i < m_components.size(); ++i)
        {
            RocketComponent& component = *m_components[i];
            if (component.kind() != ComponentKind::NOSE_CONE)
            {
                continue;
            }
            settle();
            const FactoryPanel<> made(*m_document, *m_services, component);
            auto*                flip = made->findChild<BoolCheckBox*>(QStringLiteral("Flipped"));
            const auto*          automatic =
                made->findChild<BoolCheckBox*>(QStringLiteral("BaseRadiusAutomatic"));
            ASSERT_NE(flip, nullptr);
            ASSERT_NE(automatic, nullptr);
            for (const std::string_view step : {"flip", "unflip"})
            {
                flip->click();
                m_lines.push_back(std::format("== {} | {} | NoseCone", step, i));
                std::vector<std::string> section{checkLine(*automatic), checkLine(*flip)};
                addStateLines(component, "", section);
                addSection(std::move(section));
            }
        }
    }

    /// Part C: the panels that disable an "Automatic" check box, with another component.
    void multis()
    {
        for (std::size_t i = 0; i < m_components.size(); ++i)
        {
            RocketComponent& component = *m_components[i];
            if (!isBodyKind(component.kind()))
            {
                continue;
            }
            settle();
            const std::size_t other = otherWithAutomatic(i);
            if (other == m_components.size() || !disablesAutomatic(component))
            {
                continue;
            }
            settle();
            multi(i, other);
        }
    }

    /// The probe's multi(): the panel of the component @p main with @p other selected too.
    void multi(std::size_t main, std::size_t other)
    {
        RocketComponent& mainComponent  = *m_components.at(main);
        RocketComponent& otherComponent = *m_components.at(other);
        m_lines.push_back(std::format("== multi | {} | {} | {}", main, other,
                                      probeClassName(mainComponent.kind())));
        std::vector<std::string> section;
        addStateLines(mainComponent, "main ", section);
        addStateLines(otherComponent, "other ", section);
        {
            const std::array<RocketComponent*, 1> others{&otherComponent};
            const FactoryPanel<> made(*m_document, *m_services, mainComponent, others);
            addStateLines(mainComponent, "main after ", section);
            addStateLines(otherComponent, "other after ", section);
        }
        addSection(std::move(section));
    }

    void settle() const
    {
        const Result<int> settled = settleAutomaticDimensions(m_document->getRocket());
        EXPECT_TRUE(settled.has_value()) << settled.error().message;
    }

private:
    void addSection(std::vector<std::string> section)
    {
        std::ranges::sort(section);
        for (std::string& line : section)
        {
            m_lines.push_back(std::move(line));
        }
    }

    /// The first component other than the component @p index of the same kind whose automatic
    /// radius is on (the probe's anyAutomatic()); m_components.size() when there is none.
    [[nodiscard]] std::size_t otherWithAutomatic(std::size_t index) const
    {
        const ComponentKind kind = m_components[index]->kind();
        for (std::size_t j = 0; j < m_components.size(); ++j)
        {
            if (j != index && m_components[j]->kind() == kind && anyAutomatic(*m_components[j]))
            {
                return j;
            }
        }
        return m_components.size();
    }

    [[nodiscard]] static bool anyAutomatic(const RocketComponent& component)
    {
        if (const auto* nose = dynamic_cast<const NoseCone*>(&component))
        {
            return nose->isBaseRadiusAutomatic();
        }
        if (const auto* tube = dynamic_cast<const BodyTube*>(&component))
        {
            return tube->isOuterRadiusAutomatic();
        }
        if (const auto* transition = dynamic_cast<const Transition*>(&component))
        {
            return transition->isForeRadiusAutomatic() || transition->isAftRadiusAutomatic();
        }
        return false;
    }

    /// Whether the panel of @p component disables one of its "Automatic" check boxes (made and
    /// let go, as the probe does).
    [[nodiscard]] bool disablesAutomatic(RocketComponent& component) const
    {
        const FactoryPanel<> made(*m_document, *m_services, component);
        bool                 disables = false;
        for (const char* name : {"OuterRadiusAutomatic", "BaseRadiusAutomatic",
                                 "ForeRadiusAutomatic", "AftRadiusAutomatic"})
        {
            const auto* box = made->findChild<BoolCheckBox*>(QString::fromLatin1(name));
            disables        = disables || (box != nullptr && !box->isEnabled());
        }
        return disables;
    }

    OpenRocketDocument*           m_document;
    const ConfigDialogServices*   m_services;
    std::vector<RocketComponent*> m_components;
    std::vector<std::string>      m_lines;
};

/// The document of a pinned example or test rocket, settled as the probe settles it.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> pinnedDocument(const BodyPanelPin& pin)
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
[[nodiscard]] std::vector<BodyPanelPin> designPins()
{
    std::vector<BodyPanelPin> pins;
    std::ranges::copy_if(kBodyPanelPins, std::back_inserter(pins),
                         [](const BodyPanelPin& pin) { return pin.source != "built"; });
    return pins;
}

/// The pin of the probe's own design @p design.
[[nodiscard]] const BodyPanelPin& builtPin(std::string_view design)
{
    const std::span pins(kBodyPanelPins);
    const auto      found = std::ranges::find_if(pins, [design](const BodyPanelPin& pin) {
        return pin.source == "built" && pin.design == design;
    });
    QTROCKET_ASSERT(found != pins.end());
    return *found;
}

class BodyPanelGolden : public Test::GuiTest, public ::testing::WithParamInterface<BodyPanelPin>
{ };

TEST(BodyPanelPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(kBodyPanelPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(kBodyPanelPins.size(), 32U);
}

TEST_P(BodyPanelGolden, ShowsAndSwitchesAsOpenRocket)
{
    const BodyPanelPin&                       pin = GetParam();
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = pinnedDocument(pin);
    ASSERT_NE(document, nullptr) << "no design " << pin.design;
    DesignReplay replay(*document, services);
    replay.settle();
    replay.panels();
    replay.flips();
    replay.multis();
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), pin.lines), "");
}

INSTANTIATE_TEST_SUITE_P(Designs, BodyPanelGolden, ::testing::ValuesIn(designPins()),
                         [](const ::testing::TestParamInfo<BodyPanelPin>& param) {
                             return std::string(param.param.testName);
                         });

/// The probe's nose cones: Rocket > Stage > [nose cone, body tube, nose cone], the body tube's
/// automatic radius off, the first nose cone's automatic base on (and flipped when @p flipped).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> builtNoseCones(bool flipped)
{
    auto        rocket = std::make_unique<Rocket>();
    AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
    NoseCone&   first  = stage.addChild(std::make_unique<NoseCone>());
    BodyTube&   tube   = stage.addChild(std::make_unique<BodyTube>());
    stage.addChild(std::make_unique<NoseCone>());
    tube.setOuterRadiusAutomatic(false);
    if (flipped)
    {
        first.setFlipped(true);
    }
    first.setBaseRadiusAutomatic(true);
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

TEST_F(BodyPanelBuiltGolden, TheSwitchOffReachesTheOtherNoseCone)
{
    // The second nose cone has no next component: its panel switches the automatic base off on
    // every selected nose cone (Java: setBaseRadiusAutomatic(false) of the main one, forwarded).
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = builtNoseCones(false);
    DesignReplay                              replay(*document, services);
    replay.settle();
    replay.multi(4, 2);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtPin("nose cones").lines), "");
}

TEST_F(BodyPanelBuiltGolden, TheSwitchOffReachesAFlippedNoseConesBase)
{
    // The user's decision U3: each selected nose cone switches its own base off. Java's main nose
    // cone (not flipped) forwards setAftRadiusAutomatic(false), which leaves the base of a flipped
    // one (its fore end) automatic: the pin says true where QtRocket gives false.
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document = builtNoseCones(true);
    DesignReplay                              replay(*document, services);
    replay.settle();
    replay.multi(4, 2);
    std::vector<std::string> expected;
    for (const std::string_view line : builtPin("nose cones flipped").lines)
    {
        if (line == "state other after BaseRadiusAutomatic=true")
        {
            expected.emplace_back("state other after BaseRadiusAutomatic=false");
        }
        else if (line == "state other after ForeRadiusAutomatic=true")
        {
            expected.emplace_back("state other after ForeRadiusAutomatic=false");
        }
        else
        {
            expected.emplace_back(line);
        }
    }
    const std::vector<std::string_view> expectedViews(expected.begin(), expected.end());
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), expectedViews), "");
    EXPECT_NE(std::ranges::find(builtPin("nose cones flipped").lines,
                                "state other after BaseRadiusAutomatic=true"),
              builtPin("nose cones flipped").lines.end());
}

TEST_F(BodyPanelBuiltGolden, TheSwitchOffReachesTheOtherTransition)
{
    // The first transition has no previous component: its fore "Automatic" switches the fore
    // radius of both off; its aft one can then use the second transition.
    const ConfigDialogServices services;
    auto                       rocket = std::make_unique<Rocket>();
    AxialStage&                stage  = rocket->addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<Transition>());
    Transition& second = stage.addChild(std::make_unique<Transition>());
    second.setForeRadiusAutomatic(true);
    rocket->enableEvents();
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    DesignReplay replay(*document, services);
    replay.settle();
    replay.multi(2, 3);
    EXPECT_EQ(pinnedLineDifferences(replay.lines(), builtPin("transitions").lines), "");
}

}  // namespace

}  // namespace QtRocket::Gui
