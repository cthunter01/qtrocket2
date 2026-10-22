// OpenRocket's stage buttons and flight configuration combo box of the design view's ribbon
// (RibbonProbe, OpenRocket 5f164fd0e: StageSelector and ConfigurationComboBox made as RocketPanel
// makes them, the buttons clicked with doClick(), the items picked with setSelectedItem(), as the
// popup picks them), against QtRocket's StageSelector and ConfigurationComboBox over the same
// designs: the buttons' texts, states, fonts, icons and tooltips, the stages active after each
// click, the rocket's change events, the document's undo history, and the combo box's items and
// selections. The pins are RibbonPins.h, generated from the probe's output.

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QFont>
#include <QIcon>
#include <QString>
#include <QTest>
#include <QToolButton>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/StageSelector.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "RibbonPins.h"
#include "RibbonTestSupport.h"
#include "aero/BarrowmanTestRockets.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using Test::ChangeEventTypes;
using Test::ConfigurationPickPin;
using Test::RibbonScenarioPin;
using Test::StageButtonPin;
using Test::StageButtonTip;
using Test::StageClickPin;
using Test::UndoPin;

/// The design of a scenario as the probe made it: an example loaded without presets and settled
/// (also for the scenario "local"), a TestRockets maker, or one of the probe's own designs
/// (special:...).
class RibbonDesign
{
public:
    explicit RibbonDesign(const RibbonScenarioPin& pin)
    {
        if (pin.source == "example" || pin.source == "local")
        {
            loadExample(pin.design);
        }
        else if (pin.source == "testrocket")
        {
            for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
            {
                if (maker.method == pin.design)
                {
                    m_rocket = maker.make();
                }
            }
        }
        else
        {
            makeSpecial(pin.design);
        }
        if (!isLoaded())
        {
            ADD_FAILURE() << "no design " << pin.name;
        }
    }

    [[nodiscard]] bool isLoaded() const noexcept
    {
        return m_rocket != nullptr || m_document != nullptr;
    }

    [[nodiscard]] Rocket& rocket() const
    {
        return m_rocket != nullptr ? *m_rocket : m_document->getRocket();
    }

    /// The document of an example or a new design; null for a bare rocket.
    [[nodiscard]] const OpenRocketDocument* document() const noexcept { return m_document.get(); }

private:
    void loadExample(std::string_view file)
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
        if (example == nullptr)
        {
            return;
        }
        Result<LoadedDocument> loaded = QtRocket::Test::loadSettledGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL,
            QtRocket::Test::DesignFileEnvironment::Presets::NONE);
        ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
        m_document = std::move(loaded->document);
    }

    /// The probe's own designs (RibbonProbe.main()).
    void makeSpecial(std::string_view name)
    {
        if (name == "new-design" || name == "new-design-with-nose")
        {
            m_document = OpenRocketDocumentFactory::createNewRocket();
            if (name == "new-design-with-nose")
            {
                m_document->getRocket().getChild(0).addChild(std::make_unique<NoseCone>());
            }
        }
        else if (name == "simple2stage-empty-booster")
        {
            QtRocket::Test::TestSimple2Stage       simple;
            const std::unique_ptr<RocketComponent> body = simple.boosterStage->removeChild(0);
            m_rocket                                    = std::move(simple.rocket);
        }
        else if (name == "boosters-on-boosters")
        {
            QtRocket::Test::TestBoostersOnBoostersRocket nested;
            m_rocket = std::move(nested.rocket);
        }
        else if (name == "beta-sequence")
        {
            QtRocket::Test::TestBeta beta;
            m_rocket = std::move(beta.rocket);
        }
    }

    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<Rocket>             m_rocket;
};

/// The tooltip of @p tip, as messages.properties has it.
[[nodiscard]] QString tipText(StageButtonTip tip)
{
    switch (tip)
    {
        case StageButtonTip::TOGGLE:
            return QStringLiteral("Toggle this button to activate/deactivate the stage.");
        case StageButtonTip::LAST_ACTIVE:
            return QStringLiteral("Cannot deactivate the only active stage.");
        case StageButtonTip::NO_CHILDREN:
            return QStringLiteral(
                "<html>This stage does not have child components and is therefore marked as "
                "inactive. <br>Add components to the stage to activate it.</html>");
    }
    return {};
}

/// The stage button @p button as Java's @p pin.
void expectButtonAsJava(const QToolButton& button, const StageButtonPin& pin)
{
    EXPECT_EQ(button.text(), QtStrings::fromUtf8(pin.text));
    EXPECT_EQ(button.isChecked(), pin.selected) << pin.text;
    EXPECT_EQ(button.isEnabled(), pin.enabled) << pin.text;
    EXPECT_EQ(button.font().italic(), pin.italic) << pin.text;
    EXPECT_EQ(!button.icon().isNull(), pin.inactiveIcon) << pin.text;
    EXPECT_EQ(button.toolTip(), tipText(pin.tip)) << pin.text;
}

/// Which stages of the rocket's selected configuration are active, as the probe prints them.
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

/// The buttons of @p selector and the active stages as Java's.
void expectSelectorAsJava(const StageSelector& selector, std::span<const StageButtonPin> buttons,
                          std::string_view stages)
{
    ASSERT_EQ(selector.buttonCount(), buttons.size());
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        SCOPED_TRACE(i);
        expectButtonAsJava(*selector.button(i), buttons[i]);
    }
    EXPECT_EQ(activeStages(selector.rocket()), stages);
}

/// The undo history of @p document as Java's.
void expectUndoAsJava(const OpenRocketDocument& document, const UndoPin& pin)
{
    const OpenRocketDocument::UndoDetail detail = document.getUndoDetail();
    EXPECT_EQ(detail.descriptions.size(), static_cast<std::size_t>(pin.states));
    EXPECT_EQ(detail.position, static_cast<std::size_t>(pin.position));
    EXPECT_EQ(document.isUndoAvailable(), pin.available);
    EXPECT_EQ(document.getUndoDescription(), std::nullopt);
}

/// A design made for clicks, with its stage selector and the record of its events.
struct ClickSession
{
    explicit ClickSession(const RibbonScenarioPin& pin) : design(pin) { }

    RibbonDesign     design;
    StageSelector    selector{design.rocket(), nullptr};
    ChangeEventTypes events{design.rocket()};
};

/// The events recorded as Java's: @p count events of the type @p type, from the rocket.
void expectEventsAsJava(const ChangeEventTypes& events, int count, int type)
{
    ASSERT_EQ(events.types().size(), static_cast<std::size_t>(count));
    for (const int recorded : events.types())
    {
        EXPECT_EQ(recorded, type);
    }
    EXPECT_TRUE(events.allFromRocket());
}

/// Clicks the button of @p click in @p session as Java's StageSelector was clicked.
void clickAsJava(ClickSession& session, const StageClickPin& click)
{
    if (click.document)
    {
        SCOPED_TRACE("undo before");
        ASSERT_NE(session.design.document(), nullptr);
        expectUndoAsJava(*session.design.document(), click.undoBefore);
    }
    session.events.clear();
    ASSERT_LT(static_cast<std::size_t>(click.button), session.selector.buttonCount());
    session.selector.button(static_cast<std::size_t>(click.button))->click();
    expectEventsAsJava(session.events, click.eventCount, click.eventType);
    if (click.document)
    {
        SCOPED_TRACE("undo after");
        expectUndoAsJava(*session.design.document(), click.undoAfter);
    }
    expectSelectorAsJava(session.selector, click.buttons, click.activeStages);
}

/// The index of the item of @p combo whose configuration is @p id; -1 when there is none.
[[nodiscard]] int itemOf(const ConfigurationComboBox& combo, const FlightConfigurationId& id)
{
    for (int i = 0; i < combo.count(); ++i)
    {
        if (combo.configurationIdAt(i) == id)
        {
            return i;
        }
    }
    return -1;
}

/// The items of @p combo as Java's.
void expectItemsAsJava(const ConfigurationComboBox& combo, const RibbonScenarioPin& pin)
{
    ASSERT_EQ(static_cast<std::size_t>(combo.count()), pin.items.size());
    for (std::size_t i = 0; i < pin.items.size(); ++i)
    {
        EXPECT_EQ(combo.itemText(static_cast<int>(i)), QtStrings::fromUtf8(pin.items[i])) << i;
    }
    EXPECT_EQ(combo.currentIndex(), pin.selectedItem);
}

/// Picks the item of @p pick in @p combo as the probe picked it.
void pickAsJava(ConfigurationComboBox const& combo, ChangeEventTypes& events,
                const ConfigurationPickPin& pick)
{
    SCOPED_TRACE(pick.item);
    events.clear();
    Gui::Test::pickComboItem(combo, pick.item);
    expectEventsAsJava(events, pick.eventCount, pick.eventType);
    EXPECT_EQ(combo.currentIndex(), pick.selected);
    EXPECT_EQ(itemOf(combo, combo.selectedConfigurationId()), pick.selected);
    EXPECT_EQ(itemOf(combo, combo.rocket().getSelectedConfiguration().getId()),
              pick.rocketSelected);
    EXPECT_EQ(combo.currentText(), QtStrings::fromUtf8(pick.selectedText));
}

/// The scenarios with stage buttons: every one but the combo box with its own selection.
[[nodiscard]] std::vector<RibbonScenarioPin> stageScenarios()
{
    std::vector<RibbonScenarioPin> scenarios;
    for (const RibbonScenarioPin& pin : Gui::Test::kRibbonScenarios)
    {
        if (pin.source != "local")
        {
            scenarios.push_back(pin);
        }
    }
    return scenarios;
}

[[nodiscard]] std::string scenarioName(const ::testing::TestParamInfo<RibbonScenarioPin>& param)
{
    return std::string(param.param.testName);
}

class StageSelectorGolden : public Test::GuiTest,
                            public ::testing::WithParamInterface<RibbonScenarioPin>
{ };

class ConfigurationComboBoxGolden : public Test::GuiTest,
                                    public ::testing::WithParamInterface<RibbonScenarioPin>
{ };

TEST(RibbonPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kRibbonPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Gui::Test::kRibbonScenarios.size(), 35U);
}

TEST_P(StageSelectorGolden, ShowsTheButtonsAsOpenRocket)
{
    const RibbonScenarioPin& pin = GetParam();
    const RibbonDesign       design(pin);
    ASSERT_TRUE(design.isLoaded());
    const StageSelector selector(design.rocket(), nullptr);
    expectSelectorAsJava(selector, pin.buttons, pin.activeStages);
}

TEST_P(StageSelectorGolden, TogglesAsOpenRocket)
{
    const RibbonScenarioPin&      pin = GetParam();
    std::unique_ptr<ClickSession> session;
    for (std::size_t i = 0; i < pin.clicks.size(); ++i)
    {
        SCOPED_TRACE(i);
        const StageClickPin& click = pin.clicks[i];
        if (click.fresh || session == nullptr)
        {
            session.reset();
            session = std::make_unique<ClickSession>(pin);
            ASSERT_TRUE(session->design.isLoaded());
        }
        clickAsJava(*session, click);
    }
}

TEST_P(ConfigurationComboBoxGolden, ListsAndSelectsAsOpenRocket)
{
    const RibbonScenarioPin& pin = GetParam();
    const RibbonDesign       design(pin);
    ASSERT_TRUE(design.isLoaded());
    const InMemoryPreferences preferences;
    ChangeEventTypes          events(design.rocket());
    ConfigurationComboBox     combo(design.rocket(), preferences, pin.source != "local", nullptr);
    combo.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&combo));
    expectItemsAsJava(combo, pin);
    for (const ConfigurationPickPin& pick : pin.picks)
    {
        pickAsJava(combo, events, pick);
    }
}

INSTANTIATE_TEST_SUITE_P(Scenarios, StageSelectorGolden, ::testing::ValuesIn(stageScenarios()),
                         scenarioName);
INSTANTIATE_TEST_SUITE_P(Scenarios, ConfigurationComboBoxGolden,
                         ::testing::ValuesIn(Gui::Test::kRibbonScenarios), scenarioName);

}  // namespace

}  // namespace QtRocket::Gui
