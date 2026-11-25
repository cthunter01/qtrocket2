// The Warnings tab of the simulation settings dialog (OpenRocket's SimulationWarningsPanel): what
// the probe tools/openrocket-probes/simulation-warnings shows of OpenRocket's over the simulations
// of the examples (SimulationWarningsPins.h), each line pinned (the walk, the labels' styles, the
// scroll areas) with the edits of QtRocket's deliberate differences named where they are made;
// the default buttons; and the ports of RecoveryWarningsPanelTest's tests of the tab (the
// behaviour of testSettingsMovedFromOptionsToWarningsPanel, the relational geometry of
// testConfigurationResizesAndUpdatesScrollRanges and testDescriptionsWrapToAvailableWidth).

#include "QtRocket/gui/dialogs/simulation/SimulationWarningsPanel.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QFocusEvent>
#include <QFont>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QListWidgetItem>
#include <QObject>
#include <QPalette>
#include <QPoint>
#include <QPushButton>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/dialogs/MessageList.h"
#include "QtRocket/gui/dialogs/simulation/RecoveryWarningsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/CollapsiblePanel.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "TestGuiServices.h"
#include "TestPaths.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "dialogs/simulation/SimulationWarningsPins.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationWarningsPanelTest = Gui::Test::GuiTest;
namespace Pins                    = Gui::Test::SimulationWarningsPins;
using Gui::Test::expectSameLines;
using Gui::Test::replacedIn;

// ------------------------------------------------------------------------------ the examples

/// The examples, loaded as the probe loads them (the bundled motors, no presets).
class Examples
{
public:
    /// The example @p name (data/examples/<name>.ork); an empty design with a test failure when it
    /// cannot be loaded.
    [[nodiscard]] std::unique_ptr<OpenRocketDocument> load(std::string_view name) const
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir() / "examples" / (std::string(name) + ".ork"));
        if (!loaded.has_value())
        {
            ADD_FAILURE() << loaded.error().message;
            return OpenRocketDocumentFactory::createEmptyRocket();
        }
        return std::move(loaded->document);
    }

private:
    QtRocket::Test::DesignFileEnvironment m_environment{
        QtRocket::Test::DesignFileEnvironment::Presets::NONE,
        QtRocket::Test::bundledMotorDatabase()};
};

/// The Warnings tab of a simulation, its bindings kept by a scope of its own, released before the
/// panel goes.
class WarningsTab
{
public:
    WarningsTab(std::shared_ptr<Simulation> simulation, bool showSimulationWarnings,
                Preferences& preferences)
      : m_panel(std::move(simulation), showSimulationWarnings, preferences, m_scope, &m_root)
    {
    }
    ~WarningsTab() { m_scope.release(); }
    WarningsTab(const WarningsTab&)            = delete;
    WarningsTab& operator=(const WarningsTab&) = delete;
    WarningsTab(WarningsTab&&)                 = delete;
    WarningsTab& operator=(WarningsTab&&)      = delete;

    [[nodiscard]] SimulationWarningsPanel& panel() noexcept { return m_panel; }

private:
    QWidget                 m_root;
    BindingScope            m_scope{m_root, nullptr};
    SimulationWarningsPanel m_panel;
};

// ----------------------------------------------------------------------------- the transcript

/// "#rrggbb" of @p color, as the probe prints a colour.
[[nodiscard]] std::string hex(const QColor& color)
{
    return std::format("#{:02x}{:02x}{:02x}", color.red(), color.green(), color.blue());
}

/// The probe's STYLE line of @p label: its text, colour, style and size above the default label
/// font's.
[[nodiscard]] std::string styleLine(const QLabel& label)
{
    const QFont& font  = label.font();
    const char*  style = "PLAIN";
    if (font.bold() && font.italic())
    {
        style = "BOLD_ITALIC";
    }
    else if (font.bold())
    {
        style = "BOLD";
    }
    else if (font.italic())
    {
        style = "ITALIC";
    }
    const double size = font.pointSizeF() - QApplication::font("QLabel").pointSizeF();
    return std::format("STYLE {} color={} style={} size=+{:.1f}", QtStrings::toUtf8(label.text()),
                       hex(label.palette().color(QPalette::WindowText)), style, size);
}

/// The probe's TABS line of the tab @p index of @p tabs.
[[nodiscard]] std::string tabsLine(const QTabWidget& tabs, int index)
{
    const auto* scrollArea = qobject_cast<const QScrollArea*>(tabs.widget(index));
    if (scrollArea == nullptr)
    {
        ADD_FAILURE() << "the tab " << index << " is no scroll area";
        return {};
    }
    return std::format("TABS {} scrollPane={} unitIncrement={} preferredHeight={}",
                       QtStrings::toUtf8(tabs.tabText(index)),
                       scrollArea->frameShape() == QFrame::NoFrame ? "no border" : "border",
                       scrollArea->verticalScrollBar()->singleStep(),
                       scrollArea->sizeHint().height());
}

/// What the probe prints of a panel: its walk, the styles of the Messages tab's labels and the
/// scroll areas of the tabs.
[[nodiscard]] std::vector<std::string> transcript(const SimulationWarningsPanel& panel)
{
    std::vector<std::string> lines;
    for (const std::string& line : Gui::Test::dialogWalk(panel))
    {
        lines.push_back("WALK " + line);
    }
    for (const QLabel* const label : panel.messagesPanel().findChildren<QLabel*>())
    {
        lines.push_back(styleLine(*label));
    }
    for (int index = 0; index < panel.tabs().count(); ++index)
    {
        lines.push_back(tabsLine(panel.tabs(), index));
    }
    return lines;
}

/// The pinned transcript @p pins with QtRocket's deliberate differences in the Configuration tab:
/// the sections' headers draw the style's disclosure arrow, not an icon (CollapsiblePanel, K3's
/// deviation), and the description is a word-wrapped label (the walk's TEXTAREA) of Java's HTML.
[[nodiscard]] std::vector<std::string> expectedTranscript(std::span<const std::string_view> pins)
{
    std::vector<std::string> lines(pins.begin(), pins.end());
    lines = replacedIn(std::move(lines), "BUTTON Single Deployment Warnings icon",
                       "BUTTON Single Deployment Warnings", 1);
    lines = replacedIn(std::move(lines), "BUTTON Dual Deployment Warnings icon",
                       "BUTTON Dual Deployment Warnings", 1);
    return replacedIn(std::move(lines),
                      "LABEL <html><body style=", "TEXTAREA <html><body style=", 1);
}

/// Java's Double.toString() of the three thresholds of @p options and of @p preferences, as the
/// probe prints them.
[[nodiscard]] std::string thresholds(const SimulationOptions& options)
{
    return "options " + Strings::javaDoubleToString(options.getRecoverySpeedWarning()) + " " +
           Strings::javaDoubleToString(options.getRecoveryDrogueMainLowSpeedWarning()) + " " +
           Strings::javaDoubleToString(options.getRecoveryDrogueMainHighSpeedWarning());
}
[[nodiscard]] std::string preferenceThresholds(const Preferences& preferences)
{
    return "preferences " + Strings::javaDoubleToString(preferences.getRecoverySpeedWarning()) +
           " " + Strings::javaDoubleToString(preferences.getRecoveryDrogueMainLowSpeedWarning()) +
           " " + Strings::javaDoubleToString(preferences.getRecoveryDrogueMainHighSpeedWarning());
}

/// The probe's "[<text>] [<text>] [<text>]" of the three spin boxes.
[[nodiscard]] std::string spinnerTexts(const RecoveryWarningsPanel& recovery)
{
    return "[" + QtStrings::toUtf8(recovery.highSpeedThreshold().spinBox->text()) + "] [" +
           QtStrings::toUtf8(recovery.dualLowSpeedThreshold().spinBox->text()) + "] [" +
           QtStrings::toUtf8(recovery.dualHighSpeedThreshold().spinBox->text()) + "]";
}

/// Java's spinner.setValue("<text>").
void enter(const RecoveryWarningsPanel::Threshold& threshold, std::string_view text)
{
    static_cast<void>(Gui::Test::enterText(*threshold.spinBox, text));
}

// ------------------------------------------------------------------------------- the messages

TEST_F(SimulationWarningsPanelTest, ShowsWhatOpenRocketsTabShowsWithoutWarnings)
{
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 document = examples.load("A simple model rocket");
    WarningsTab                tab(document->getSimulation(0), true, services.preferences());
    expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kSimpleRocketFirst));
    ASSERT_NE(tab.panel().noWarningsLabel(), nullptr);
    EXPECT_EQ(tab.panel().unavailableLabel(), nullptr);
    EXPECT_EQ(tab.panel().warningList(MessagePriority::NORMAL), nullptr);
}

TEST_F(SimulationWarningsPanelTest, AMultiSimulationEditShowsNoWarningsAndOpensOnConfiguration)
{
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 document = examples.load("A simple model rocket");
    WarningsTab                tab(document->getSimulation(0), false, services.preferences());
    expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kSimpleRocketFirstMultiEdit));
    EXPECT_EQ(tab.panel().noWarningsLabel(), nullptr);
    ASSERT_NE(tab.panel().unavailableLabel(), nullptr);
}

TEST_F(SimulationWarningsPanelTest, ShowsTheWarningsOfTheExamplesAsOpenRocketDoes)
{
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 simple   = examples.load("A simple model rocket");
    const auto                 twoStage = examples.load("Two stage high power rocket");
    const auto                 payload  = examples.load("Deployable payload");
    {
        WarningsTab tab(simple->getSimulation(2), true, services.preferences());
        expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kSimpleRocketThird));
        EXPECT_EQ(tab.panel().noWarningsLabel(), nullptr);
    }
    {
        WarningsTab tab(twoStage->getSimulation(0), true, services.preferences());
        expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kTwoStageFirst));
    }
    {
        WarningsTab tab(payload->getSimulation(2), true, services.preferences());
        expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kDeployablePayloadThird));
    }
}

TEST_F(SimulationWarningsPanelTest, GroupsTheWarningsOfEveryPriority)
{
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 document   = examples.load("A simple model rocket");
    const auto                 simulation = document->getSimulation(0);
    // The probe's: the loaded set (immutable) replaced by one with four more warnings.
    WarningSet warnings;
    ASSERT_NE(simulation->getSimulatedWarnings(), nullptr);
    warnings.addAll(*simulation->getSimulatedWarnings());
    warnings.add(Warning::kNoRecoveryDevice);
    warnings.add(Warning::kSupersonic);
    warnings.add(Warning::kTumbleUnderThrust);
    warnings.add(Warning::kDiameterDiscontinuity);
    *simulation->getSimulatedWarnings() = warnings;
    WarningsTab tab(simulation, true, services.preferences());
    expectSameLines(transcript(tab.panel()), expectedTranscript(Pins::kWithAddedWarnings));

    // The titles' and descriptions' colours are OpenRocket's light theme's.
    const SimulationWarningsPanel& panel = tab.panel();
    ASSERT_NE(panel.groupTitle(MessagePriority::HIGH), nullptr);
    EXPECT_EQ(panel.groupTitle(MessagePriority::HIGH)->palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::DARK_ERROR, ThemeColors::Theme::LIGHT));
    EXPECT_EQ(panel.groupTitle(MessagePriority::NORMAL)->palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::WARNING, ThemeColors::Theme::LIGHT));
    EXPECT_EQ(panel.groupTitle(MessagePriority::LOW)->palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::INFO, ThemeColors::Theme::LIGHT));
    EXPECT_EQ(panel.groupDescription(MessagePriority::LOW)->palette().color(QPalette::WindowText),
              ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::LIGHT));
    const MessageList* const critical = panel.warningList(MessagePriority::HIGH);
    ASSERT_NE(critical, nullptr);
    EXPECT_EQ(critical->messages(),
              (QStringList{QStringLiteral("No recovery device defined in the simulation."),
                           QStringLiteral("Stage began to tumble under thrust.")}));
    EXPECT_FALSE(critical->item(0)->icon().isNull());
}

TEST_F(SimulationWarningsPanelTest, AWarningListsFocusLossClearsItsSelection)
{
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 document = examples.load("Deployable payload");
    WarningsTab                tab(document->getSimulation(2), true, services.preferences());
    MessageList* const         list = tab.panel().warningList(MessagePriority::NORMAL);
    ASSERT_NE(list, nullptr);
    list->setCurrentRow(1);
    ASSERT_EQ(list->selectedItems().size(), 1);
    QFocusEvent focusOut(QEvent::FocusOut, Qt::TabFocusReason);
    QCoreApplication::sendEvent(list, &focusOut);
    EXPECT_TRUE(list->selectedItems().isEmpty());
}

// --------------------------------------------------------------------------- the default buttons

TEST_F(SimulationWarningsPanelTest, TheDefaultButtonsAreOpenRockets)
{
    const Examples               examples;
    Gui::Test::TestGuiServices   services;
    InMemoryPreferences&         preferences = services.preferences();
    const auto                   document    = examples.load("A simple model rocket");
    const auto                   simulation  = document->getSimulation(0);
    const SimulationOptions&     options     = simulation->getOptions();
    WarningsTab                  tab(simulation, true, preferences);
    const RecoveryWarningsPanel& recovery = tab.panel().recoveryWarnings();
    recovery.dualDeploymentSection().setExpanded(true);

    std::vector<std::string> lines;
    lines.push_back("STEP made | spinners " +
                    std::to_string(recovery.findChildren<UnitSpinBox*>().size()) + " | " +
                    thresholds(options) + " | " + preferenceThresholds(preferences));
    enter(recovery.highSpeedThreshold(), "25");
    enter(recovery.dualLowSpeedThreshold(), "10");
    enter(recovery.dualHighSpeedThreshold(), "40");
    lines.push_back("STEP typed 25 10 40 | " + thresholds(options) + " | " +
                    preferenceThresholds(preferences));
    tab.panel().resetButton().click();
    lines.push_back("STEP reset, fresh preferences | " + thresholds(options) + " | " +
                    preferenceThresholds(preferences) + " | spinners " + spinnerTexts(recovery));
    preferences.setRecoverySpeedWarning(33);
    preferences.setRecoveryDrogueMainLowSpeedWarning(11);
    preferences.setRecoveryDrogueMainHighSpeedWarning(44);
    tab.panel().resetButton().click();
    lines.push_back("STEP reset, stored preferences | " + thresholds(options) + " | " +
                    preferenceThresholds(preferences) + " | spinners " + spinnerTexts(recovery));
    enter(recovery.highSpeedThreshold(), "26");
    enter(recovery.dualLowSpeedThreshold(), "12.5");
    enter(recovery.dualHighSpeedThreshold(), "41");
    tab.panel().saveButton().click();
    lines.push_back("STEP save 26 12.5 41 | " + thresholds(options) + " | " +
                    preferenceThresholds(preferences));
    expectSameLines(lines, std::vector<std::string>(Pins::kDefaultButtons.begin(),
                                                    Pins::kDefaultButtons.end()));
}

TEST_F(SimulationWarningsPanelTest, TheDefaultButtonsCommitAPendingEditFirst)
{
    // A click on a button that takes no focus (macOS) leaves the spin box's edit pending.
    const Examples             examples;
    Gui::Test::TestGuiServices services;
    const auto                 document   = examples.load("A simple model rocket");
    const auto                 simulation = document->getSimulation(0);
    WarningsTab                tab(simulation, true, services.preferences());
    const UnitSpinBox& spinBox = *tab.panel().recoveryWarnings().highSpeedThreshold().spinBox;
    // The text typed: shown, modified, not committed.
    QLineEdit& editor = *Gui::Test::editorOf(spinBox);
    editor.setText(QStringLiteral("27"));
    editor.setModified(true);
    tab.panel().saveButton().click();
    EXPECT_EQ(services.preferences().getRecoverySpeedWarning(), 27.0);
    editor.setText(QStringLiteral("28"));
    editor.setModified(true);
    tab.panel().resetButton().click();
    // Committed (28), then reset to the preferences' 27.
    EXPECT_EQ(simulation->getOptions().getRecoverySpeedWarning(), 27.0);
}

// ------------------------------------------------------------------------------------- the ports

TEST_F(SimulationWarningsPanelTest, SettingsMovedFromOptionsToWarningsPanel)
{
    // Port of RecoveryWarningsPanelTest.testSettingsMovedFromOptionsToWarningsPanel.
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const auto   simulation = std::make_shared<Simulation>(document.get(), document->getRocket());
    QWidget      root;
    BindingScope scope(root, nullptr);
    const SimulationOptionsPanel  optionsPanel(simulation, services.services(), scope, &root);
    const SimulationWarningsPanel warningsPanel(simulation, false, services.preferences(), scope,
                                                &root);

    EXPECT_TRUE(optionsPanel.findChildren<RecoveryWarningsPanel*>().isEmpty());
    EXPECT_EQ(warningsPanel.findChildren<RecoveryWarningsPanel*>().size(), 1);
    const QList<QTabWidget*> warningTabs = warningsPanel.findChildren<QTabWidget*>();
    ASSERT_EQ(warningTabs.size(), 1);
    EXPECT_EQ(warningTabs.at(0)->count(), 2);
    // "Messages should have its own scroll pane below the tab header"
    EXPECT_NE(qobject_cast<QScrollArea*>(warningTabs.at(0)->widget(0)), nullptr);
    // "Configuration should have its own scroll pane below the tab header"
    EXPECT_NE(qobject_cast<QScrollArea*>(warningTabs.at(0)->widget(1)), nullptr);
    // "Multi-simulation editing should open configuration because messages are unavailable"
    EXPECT_EQ(warningTabs.at(0)->currentIndex(), 1);
    // "Multi-simulation editing should hide run-specific results, not warning settings"
    EXPECT_TRUE(warningsPanel.findChildren<QAbstractItemView*>().isEmpty());
    const SimulationWarningsPanel singleSimulationPanel(simulation, true, services.preferences(),
                                                        scope, &root);
    const QList<QTabWidget*>      singleSimulationTabs =
        singleSimulationPanel.findChildren<QTabWidget*>();
    ASSERT_EQ(singleSimulationTabs.size(), 1);
    // "Actual warning messages should be shown first for a single simulation"
    EXPECT_EQ(singleSimulationTabs.at(0)->currentIndex(), 0);
    scope.release();
}

/// Lets the posted events (layout requests, the scroll areas' updates) through.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

/// The height @p content needs at its width (its layout's height for width, or its minimum).
[[nodiscard]] int neededHeight(const QWidget& content)
{
    const int height = content.heightForWidth(content.width());
    return height >= 0 ? height : content.minimumSizeHint().height();
}

/// Whether @p child lies inside @p ancestor's rectangle.
[[nodiscard]] bool insideOf(const QWidget& child, const QWidget& ancestor)
{
    const QRect bounds(child.mapTo(&ancestor, QPoint(0, 0)), child.size());
    return ancestor.rect().contains(bounds);
}

/// The new rocket's simulation of RecoveryWarningsPanelTest's geometry tests, in a multi-
/// simulation edit (OpenRocket opens it on Configuration), shown in a window of its own.
class GeometryTab
{
public:
    GeometryTab()
      : m_document(OpenRocketDocumentFactory::createNewRocket()),
        m_panel(std::make_shared<Simulation>(m_document.get(), m_document->getRocket()), false,
                m_services.preferences(), m_scope, nullptr)
    {
        m_panel.layout()->activate();
        m_panel.show();
        settle();
    }
    ~GeometryTab() { m_scope.release(); }
    GeometryTab(const GeometryTab&)            = delete;
    GeometryTab& operator=(const GeometryTab&) = delete;
    GeometryTab(GeometryTab&&)                 = delete;
    GeometryTab& operator=(GeometryTab&&)      = delete;

    [[nodiscard]] SimulationWarningsPanel& panel() noexcept { return m_panel; }
    /// The panel resized to @p width x @p height, its layouts done.
    void resize(int width, int height)
    {
        m_panel.resize(width, height);
        settle();
    }

private:
    Gui::Test::TestGuiServices          m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    QWidget                             m_root;
    BindingScope                        m_scope{m_root, nullptr};
    SimulationWarningsPanel             m_panel;
};

/// The widths and heights of testConfigurationResizesAndUpdatesScrollRanges.
struct Extents
{
    int preferredContentWidth  = 0;
    int preferredContentHeight = 0;
    int horizontalChrome       = 0;
    int narrowViewportWidth    = 0;
};

/// The first half of testConfigurationResizesAndUpdatesScrollRanges: the chrome measured, then a
/// viewport narrower than the controls, which scrolls without clipping.
[[nodiscard]] Extents expectNarrowConfigurationScrolls(GeometryTab& tab)
{
    const SimulationWarningsPanel& warningsPanel = tab.panel();
    const QTabWidget&              nestedTabs    = warningsPanel.tabs();
    const QScrollArea& configurationScrollPane   = warningsPanel.configurationScrollArea();
    const QWidget&     configurationContent      = warningsPanel.configurationPanel();
    Extents            extents;
    extents.preferredContentWidth = configurationContent.minimumSizeHint().width();

    // Measure the tab and scroll-pane chrome, then make the viewport narrower than the controls
    // can fit. Only then should horizontal scrolling be needed.
    tab.resize(extents.preferredContentWidth + 200, 400);
    extents.horizontalChrome = warningsPanel.width() - configurationScrollPane.viewport()->width();
    extents.narrowViewportWidth = std::max(100, extents.preferredContentWidth - 100);
    tab.resize(extents.narrowViewportWidth + extents.horizontalChrome, 400);
    // "The nested tabs should shrink instead of clipping their scroll pane"
    EXPECT_TRUE(insideOf(nestedTabs, warningsPanel));
    // "The configuration scroll pane should remain within the nested tab bounds"
    EXPECT_TRUE(insideOf(configurationScrollPane, nestedTabs));
    // "Configuration should scroll before the dual-threshold row is clipped"
    EXPECT_TRUE(configurationScrollPane.horizontalScrollBar()->isVisible());
    // "Narrow resizing must preserve enough width for the threshold controls"
    EXPECT_GE(configurationContent.width(), extents.preferredContentWidth);
    const RecoveryWarningsPanel& recovery = warningsPanel.recoveryWarnings();
    const QWidget& dualThresholdPanel = *recovery.dualLowSpeedThreshold().spinBox->parentWidget();
    const QWidget& secondSpinner      = *recovery.dualHighSpeedThreshold().spinBox;
    // "The second threshold spinner should remain inside the scrollable content"
    EXPECT_LE(secondSpinner.x() + secondSpinner.width(), dualThresholdPanel.width());
    extents.preferredContentHeight = neededHeight(configurationContent);
    return extents;
}

/// One size of the loop of testConfigurationResizesAndUpdatesScrollRanges: the scroll bars shown
/// exactly when an axis is short of room, nothing clipped, the trailing content reachable.
void expectScrollRangesAt(GeometryTab& tab, int width, int height, bool narrow, bool shortOfHeight)
{
    tab.resize(width, height);
    const SimulationWarningsPanel& warningsPanel = tab.panel();
    const QScrollArea& configurationScrollPane   = warningsPanel.configurationScrollArea();
    const QWidget&     configurationContent      = warningsPanel.configurationPanel();
    EXPECT_EQ(narrow, configurationScrollPane.horizontalScrollBar()->isVisible())
        << width << " x " << height;
    EXPECT_EQ(shortOfHeight, configurationScrollPane.verticalScrollBar()->isVisible())
        << width << " x " << height;
    // "Vertical resizing should keep the nested tabs inside the panel"
    EXPECT_TRUE(insideOf(warningsPanel.tabs(), warningsPanel));
    // "Vertical resizing should keep the scroll pane inside the tabs"
    EXPECT_TRUE(insideOf(configurationScrollPane, warningsPanel.tabs()));

    // At the end of each scrollbar, the trailing content must be reachable.
    QScrollBar& horizontal = *configurationScrollPane.horizontalScrollBar();
    QScrollBar& vertical   = *configurationScrollPane.verticalScrollBar();
    horizontal.setValue(horizontal.maximum());
    vertical.setValue(vertical.maximum());
    EXPECT_EQ(configurationContent.width(),
              horizontal.value() + configurationScrollPane.viewport()->width());
    EXPECT_EQ(configurationContent.height(),
              vertical.value() + configurationScrollPane.viewport()->height());
}

/// The last part of testConfigurationResizesAndUpdatesScrollRanges: a height between the
/// collapsed and the expanded content's, then the sections toggled: the vertical scroll range
/// follows them.
void expectTogglingUpdatesTheScrollRange(GeometryTab& tab, int wideWidth,
                                         int preferredContentHeight)
{
    const SimulationWarningsPanel&         warningsPanel = tab.panel();
    const QScrollArea&                     scrollPane    = warningsPanel.configurationScrollArea();
    const QWidget&                         content       = warningsPanel.configurationPanel();
    const RecoveryWarningsPanel&           recovery      = warningsPanel.recoveryWarnings();
    const std::array<CollapsiblePanel*, 2> sections{&recovery.singleDeploymentSection(),
                                                    &recovery.dualDeploymentSection()};
    // Toggle after layout to catch stale preferred-size caches.
    for (CollapsiblePanel* const section : sections)
    {
        section->setExpanded(false);
    }
    settle();
    const int collapsedHeight = neededHeight(content);
    const int verticalChrome  = warningsPanel.height() - scrollPane.viewport()->height();
    tab.resize(wideWidth, verticalChrome + ((collapsedHeight + preferredContentHeight) / 2));
    for (const bool expanded : {false, true, false, true})
    {
        for (CollapsiblePanel* const section : sections)
        {
            section->setExpanded(expanded);
        }
        settle();
        // "Expanding and collapsing must update the vertical scroll range"
        EXPECT_EQ(expanded, scrollPane.verticalScrollBar()->isVisible());
        EXPECT_LE(neededHeight(content), preferredContentHeight);
    }
}

TEST_F(SimulationWarningsPanelTest, ConfigurationResizesAndUpdatesScrollRanges)
{
    // Port of RecoveryWarningsPanelTest.testConfigurationResizesAndUpdatesScrollRanges, as
    // relational geometry of the panel shown offscreen.
    GeometryTab tab;
    tab.panel().recoveryWarnings().dualDeploymentSection().setExpanded(true);
    ASSERT_EQ(tab.panel().configurationScrollArea().widget(), &tab.panel().configurationPanel());
    settle();
    const Extents extents      = expectNarrowConfigurationScrolls(tab);
    const int     wideWidth    = extents.preferredContentWidth + 200;
    const int     tallHeight   = extents.preferredContentHeight + 200;
    const int     narrowWidth  = extents.narrowViewportWidth + extents.horizontalChrome;
    constexpr int kShortHeight = 180;
    // Exercise each axis independently and together, then return to ample space.
    expectScrollRangesAt(tab, wideWidth, tallHeight, false, false);
    expectScrollRangesAt(tab, wideWidth, kShortHeight, false, true);
    expectScrollRangesAt(tab, narrowWidth, kShortHeight, true, true);
    expectScrollRangesAt(tab, narrowWidth, tallHeight, true, false);
    expectScrollRangesAt(tab, wideWidth, tallHeight, false, false);
    expectTogglingUpdatesTheScrollRange(tab, wideWidth, extents.preferredContentHeight);
}

/// The descriptions of testDescriptionsWrapToAvailableWidth, given its real prose (instead of the
/// test translator's unbreakable resource keys).
[[nodiscard]] std::array<QLabel*, 3> proseDescriptions(const RecoveryWarningsPanel& recovery)
{
    const std::array<QLabel*, 3> descriptions{&recovery.singleDescription(),
                                              &recovery.dualDescription(), &recovery.dualHowTo()};
    QString                      prose;
    for (int repeat = 0; repeat < 4; ++repeat)
    {
        prose += QStringLiteral(
            "Warn when a recovery device deploys at an unsafe speed. Warnings "
            "apply independently by stage. ");
    }
    for (QLabel* const description : descriptions)
    {
        description->setText(
            QStringLiteral("<html><i>") + prose +
            QStringLiteral("</i><br><b>Check the recovery configuration.</b></html>"));
    }
    return descriptions;
}

/// Every line of @p description shown, and every section up to @p content as tall as its child.
void expectWrappedLinesVisible(const QLabel& description, const QWidget& content)
{
    // "All wrapped lines must remain visible"
    EXPECT_GE(description.height(), description.heightForWidth(description.width()));
    // "Sections must grow to contain the reflowed text"
    for (const QWidget* child = &description; child != &content; child = child->parentWidget())
    {
        const QWidget* const parent = child->parentWidget();
        ASSERT_NE(parent, nullptr);
        EXPECT_LE(child->y() + child->height(), parent->height());
    }
}

/// One width of testDescriptionsWrapToAvailableWidth: the content as wide as the viewport, no
/// horizontal scrolling, every line shown; the first description's height.
[[nodiscard]] int expectDescriptionsWrapAt(GeometryTab& tab, int width,
                                           const std::array<QLabel*, 3>& descriptions)
{
    tab.resize(width, 800);
    const auto* const scrollPane = qobject_cast<QScrollArea*>(tab.panel().tabs().currentWidget());
    if (scrollPane == nullptr)
    {
        ADD_FAILURE() << "the current tab is no scroll area";
        return 0;
    }
    const QWidget& content = *scrollPane->widget();
    // "Descriptions should wrap while controls fit in the viewport"
    EXPECT_FALSE(scrollPane->horizontalScrollBar()->isVisible());
    EXPECT_EQ(scrollPane->viewport()->width(), content.width());
    for (const QLabel* const description : descriptions)
    {
        expectWrappedLinesVisible(*description, content);
    }
    return descriptions.at(0)->height();
}

TEST_F(SimulationWarningsPanelTest, DescriptionsWrapToAvailableWidth)
{
    // Port of RecoveryWarningsPanelTest.testDescriptionsWrapToAvailableWidth, as relational
    // geometry of the panel shown offscreen.
    GeometryTab tab;
    tab.panel().recoveryWarnings().dualDeploymentSection().setExpanded(true);
    const std::array<QLabel*, 3> descriptions = proseDescriptions(tab.panel().recoveryWarnings());
    settle();
    const int minimumWidth = tab.panel().configurationPanel().minimumSizeHint().width();
    const int narrowHeight = expectDescriptionsWrapAt(tab, minimumWidth + 100, descriptions);
    // "Widening the panel must reduce the wrapped text height"
    EXPECT_LT(expectDescriptionsWrapAt(tab, minimumWidth + 700, descriptions), narrowHeight);
    // "Narrowing again must restore all wrapped lines"
    EXPECT_EQ(expectDescriptionsWrapAt(tab, minimumWidth + 100, descriptions), narrowHeight);
}

}  // namespace

}  // namespace QtRocket::Gui
