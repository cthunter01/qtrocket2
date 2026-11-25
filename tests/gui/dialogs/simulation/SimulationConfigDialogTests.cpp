// SimulationConfigDialog: OpenRocket's simulation settings dialog (SimulationConfigDialog.java,
// which has no JUnit test of its own): its top rows, tabs and buttons as OpenRocket shows them
// (the M4 sim-config scout's walk of "A simple model rocket" Simulation 1, java/dialog-walk.txt,
// hand-pinned here), the buttons' texts per tab, the live edits, OK (with and without runs, the
// plot, the export: OpenRocket bug 49), Cancel and Close (the discard question, the undo,
// OpenRocket bugs 29 and 30), the multi-simulation edit, the background estimate's hooks and the
// remembered geometry. The questions are scripted (ScriptedUserPrompts), the runs go through the
// manual executor, and the plot dialog is recorded (RecordingSimulationDialogs). Fresh runs: "A
// simple model rocket" #1 only, no value of the flight pinned.

#include "QtRocket/gui/dialogs/simulation/SimulationConfigDialog.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtTest/qtestkeyboard.h>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/dialogs/plot/SimulationPlotPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationConditionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationExportPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationOptionsPanel.h"
#include "QtRocket/gui/dialogs/simulation/SimulationWarningsPanel.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationDialogs.h"
#include "QtRocket/gui/util/SimulationStatusColors.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/CsvExportPanel.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/example/AirStart.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/GeodeticComputationStrategy.h"
#include "QtRocket/util/Signal.h"
#include "RecordingSimulationDialogs.h"
#include "ScriptedUserPrompts.h"
#include "SimulationRunTestSupport.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "dialogs/simulation/SimulationDialogWalk.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationConfigDialogTest = Gui::Test::GuiTest;
using Gui::Test::ExampleDesign;
using PromptKind  = Gui::Test::ScriptedUserPrompts::Kind;
using RequestKind = Gui::Test::RecordingSimulationDialogs::Kind;
using Tab         = SimulationDialogs::Tab;
using Status      = Simulation::Status;
using Simulations = std::vector<std::shared_ptr<Simulation>>;

/// The request of a dialog over @p simulations on @p tab, without the window's hooks.
[[nodiscard]] SimulationDialogs::EditRequest editRequest(Simulations simulations,
                                                         Tab         tab = Tab::LAUNCH_CONDITIONS,
                                                         bool        newSimulation = false)
{
    return {.simulations          = std::move(simulations),
            .tab                  = tab,
            .newSimulation        = newSimulation,
            .cancelBackgroundRuns = {},
            .pauseBackgroundRuns  = {}};
}

/// The settings dialogs of a test: the test's services (scripted questions, the manual executor,
/// the recorded plot dialog), an example design and the design window (not shown) whose children
/// the dialogs are.
class Session
{
public:
    explicit Session(const char* example = ExampleDesign::kSimpleModelRocket)
      : m_design(example) { }

    [[nodiscard]] Gui::Test::TestGuiServices&     services() noexcept { return m_services; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }
    [[nodiscard]] Preferences&        preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] OpenRocketDocument& document() const noexcept { return m_design.document(); }
    [[nodiscard]] std::shared_ptr<Simulation> simulation(std::size_t index) const
    {
        return m_design.simulation(index);
    }
    [[nodiscard]] QWidget& window() noexcept { return m_window; }

    /// A new dialog of @p request over the design, a child of the window.
    SimulationConfigDialog& open(const SimulationDialogs::EditRequest& request)
    {
        m_dialog = makeChild<SimulationConfigDialog>(&m_window, m_services.services(), document(),
                                                     request);
        return *m_dialog;
    }

    /// The design's simulation @p index made OUTDATED by a launch rod 10 cm longer.
    void outdate(std::size_t index) const
    {
        SimulationOptions& options = simulation(index)->getOptions();
        options.setLaunchRodLength(options.getLaunchRodLength() + 0.1);
    }

private:
    Gui::Test::TestGuiServices m_services;
    ExampleDesign              m_design;
    QWidget                    m_window;
    SimulationConfigDialog*    m_dialog{nullptr};
};

/// The texts of the walk of OpenRocket's dialog (the scout's java/dialog-walk.txt) of @p dialog's
/// top rows and buttons, and the first line of its tabs.
[[nodiscard]] std::vector<std::string> shellWalk(const SimulationConfigDialog& dialog)
{
    std::vector<std::string> lines{"TITLE " + QtStrings::toUtf8(dialog.windowTitle())};
    for (const std::string& line : Gui::Test::dialogWalk(dialog.topPanel()))
    {
        lines.push_back(line);
    }
    lines.push_back(Gui::Test::dialogWalk(dialog.tabs()).front());
    for (const std::string& line : Gui::Test::dialogWalk(dialog.bottomPanel()))
    {
        lines.push_back(line);
    }
    return lines;
}

/// "<ok> / <cancel>": the buttons' texts.
[[nodiscard]] QString buttonTexts(const SimulationConfigDialog& dialog)
{
    return dialog.okButton().text() + QStringLiteral(" / ") + dialog.cancelButton().text();
}

/// The buttons' texts on each of the five tabs, in order.
[[nodiscard]] QStringList buttonTextsPerTab(const SimulationConfigDialog& dialog)
{
    QStringList texts;
    for (int index = 0; index < dialog.tabs().count(); ++index)
    {
        dialog.tabs().setCurrentIndex(index);
        texts.append(buttonTexts(dialog));
    }
    return texts;
}

/// The tooltips and enabled states of the tabs, "<title> enabled|disabled[ tip=<tooltip>]".
[[nodiscard]] QStringList tabStates(const QTabWidget& tabs)
{
    QStringList states;
    for (int index = 0; index < tabs.count(); ++index)
    {
        QString state =
            tabs.tabText(index) +
            (tabs.isTabEnabled(index) ? QStringLiteral(" enabled") : QStringLiteral(" disabled"));
        if (!tabs.tabToolTip(index).isEmpty())
        {
            state += QStringLiteral(" tip=") + tabs.tabToolTip(index);
        }
        states.append(state);
    }
    return states;
}

/// The undo step the document's undo would undo, or "(none)".
[[nodiscard]] std::string undoDescription(const OpenRocketDocument& document)
{
    return document.getUndoDescription().value_or("(none)");
}

/// The rocket's first stage, the component the tests rename before the dialog opens (the
/// scout's probe of bug 29 renames a stage).
[[nodiscard]] RocketComponent& firstStage(OpenRocketDocument& document)
{
    return document.getRocket().getChild(0);
}

/// Renames the first stage as one undo step ("Rename stage"), the user's change before the
/// dialog opens.
void renameFirstStage(OpenRocketDocument& document, const std::string& name)
{
    document.addUndoPosition("Rename stage");
    firstStage(document).setName(name);
}

/// The index of @p strategy in the options tab's geodetic combo box (the strategies in their
/// declaration order).
[[nodiscard]] int geodeticIndex(GeodeticComputationStrategy strategy)
{
    // An index loop: an iterator of std::array is a class on MSVC.
    for (std::size_t index = 0; index < kAllGeodeticComputationStrategies.size(); ++index)
    {
        if (kAllGeodeticComputationStrategies.at(index) == strategy)
        {
            return static_cast<int>(index);
        }
    }
    ADD_FAILURE() << "no item of the strategy";
    return 0;
}

/// Types @p text into the launch rod length's field of @p dialog (in its unit, cm), committed.
void typeRodLength(const SimulationConfigDialog& dialog, std::string_view text)
{
    EXPECT_TRUE(Gui::Test::enterText(*dialog.conditionsPanel().rodLength().spinBox, text));
}

/// The names of @p simulations.
[[nodiscard]] QStringList namesOf(std::span<const std::shared_ptr<Simulation>> simulations)
{
    QStringList names;
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        names.append(QtStrings::fromUtf8(simulation->getName()));
    }
    return names;
}

/// The kinds of the questions and messages @p prompts has seen, by name.
[[nodiscard]] std::vector<std::string> promptKinds(const Gui::Test::ScriptedUserPrompts& prompts)
{
    std::vector<std::string> kinds;
    for (const Gui::Test::ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        kinds.emplace_back(Gui::Test::ScriptedUserPrompts::kindName(prompt.kind));
    }
    return kinds;
}

/// The text of the exported file @p file; empty, with a test failure, when it cannot be read.
[[nodiscard]] std::string exportedText(const std::filesystem::path& file)
{
    const Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    return bytesToString(*bytes);
}

/// What the core's CsvExport writes of @p branch of @p simulation for the rows ticked in
/// @p panel, in their units, with the panel's options; empty, with a test failure, when it fails.
[[nodiscard]] std::string coreExportOf(Simulation& simulation, const FlightDataBranch& branch,
                                       const SimulationExportPanel& panel)
{
    std::vector<const FlightDataType*> fields;
    std::vector<const Unit*>           units;
    const std::vector<bool>            selected = panel.exportPanel().selected();
    for (std::size_t row = 0; row < selected.size(); ++row)
    {
        if (selected.at(row))
        {
            fields.push_back(panel.types().at(row));
            units.push_back(&panel.exportPanel().unit(row));
        }
    }
    std::ostringstream out;
    const Result<void> written =
        CsvExport::exportCsv(out, simulation, branch, fields, units,
                             CsvOptionPreferences::toCsvExportOptions(panel.csvOptions().values()));
    if (!written.has_value())
    {
        ADD_FAILURE() << written.error().toString();
        return {};
    }
    return out.str();
}

// ------------------------------------------------------------------------------ the shell

TEST_F(SimulationConfigDialogTest, ShowsOpenRocketsTopRowsTabsAndButtons)
{
    Session                       session;
    const SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    // The scout's walk of OpenRocket's dialog, lines 2-9 and 363-364, the status in the colour of
    // the test's palette (OpenRocket's walk: "#000000").
    const QString statusColor = ColorConversion::formatHtmlColor(
        SimulationStatusColors::color(Status::LOADED), QStringLiteral("<i>Loaded from file</i>"));
    // Lines broken in two would read as a missing comma in the list.
    const std::string combo =
        std::string("COMBO selected=[A8-3] items=[No motors]|[A8-3]|[B4-4]|[C6-3]|[C6-5]|[C6-7] ") +
        "tip=Select the flight configuration to use.";
    const std::string tabs =
        std::string("TABS [Launch conditions] [Simulation options] [Warnings] [Plot data] ") +
        "[Export data] selected=0";
    const std::vector<std::string> expected{
        "TITLE Edit simulation",
        "LABEL Name: ",
        "FIELD Simulation 1",
        "LABEL Flight configuration: tip=Select the flight configuration to use.",
        combo,
        "LABEL Status:",
        "LABEL <html>" + QtStrings::toUtf8(statusColor) + "</html>",
        tabs,
        "BUTTON Cancel tip=Discard changes and close the dialog",
        "BUTTON OK tip=Keep changes and close the dialog"};
    Gui::Test::expectSameLines(shellWalk(dialog), expected);

    EXPECT_EQ(dialog.objectName(), QStringLiteral("SimulationConfigDialog"));
    EXPECT_EQ(dialog.windowModality(), Qt::WindowModal);
    EXPECT_EQ(dialog.parentWidget(), &session.window());
    EXPECT_FALSE(dialog.isMultiEdit());
    EXPECT_EQ(dialog.multiSimEditLabel(), nullptr);
    EXPECT_FALSE(dialog.isModified());
    // The plot and export tabs of a simulation with data.
    ASSERT_NE(dialog.plotPanel(), nullptr);
    EXPECT_EQ(dialog.tabs().widget(SimulationConfigDialog::kPlotTab), dialog.plotPanel());
    ASSERT_NE(dialog.exportPanel(), nullptr);
    EXPECT_EQ(dialog.tabs().widget(SimulationConfigDialog::kExportTab), dialog.exportPanel());
    EXPECT_EQ(dialog.exportPanel()->simulation(), session.simulation(0));
    EXPECT_EQ(tabStates(dialog.tabs()),
              (QStringList{QStringLiteral("Launch conditions enabled"),
                           QStringLiteral("Simulation options enabled"),
                           QStringLiteral("Warnings enabled"), QStringLiteral("Plot data enabled"),
                           QStringLiteral("Export data enabled")}));
    // Tabs 0 and 1 scroll; the Warnings tab shows the simulation's warnings.
    EXPECT_EQ(dialog.launchConditionsScrollArea().widget(), &dialog.conditionsPanel());
    EXPECT_EQ(dialog.simulationOptionsScrollArea().widget(), &dialog.optionsPanel());
    EXPECT_EQ(dialog.tabs().widget(SimulationConfigDialog::kWarningsTab), &dialog.warningsPanel());
    EXPECT_EQ(dialog.warningsPanel().unavailableLabel(), nullptr);
    EXPECT_EQ(dialog.launchConditionsScrollArea().verticalScrollBar()->singleStep(), 16);

    // The undo position of the dialog: "Edit <name>", the next change's.
    EXPECT_EQ(session.document().getUndoDetail().nextDescription,
              std::optional<std::string>("Edit Simulation 1"));
}

TEST_F(SimulationConfigDialogTest, WithoutDataThePlotAndExportTabsAreDisabledWithTheirTooltips)
{
    const Gui::Test::TestGuiServices          services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const auto simulation = std::make_shared<Simulation>(document.get(), document->getRocket());
    const SimulationConfigDialog dialog(services.services(), *document, editRequest({simulation}));
    EXPECT_EQ(dialog.plotPanel(), nullptr);
    EXPECT_EQ(tabStates(dialog.tabs()),
              (QStringList{
                  QStringLiteral("Launch conditions enabled"),
                  QStringLiteral("Simulation options enabled"), QStringLiteral("Warnings enabled"),
                  QStringLiteral("Plot data disabled tip=Simulation has no data to plot"),
                  QStringLiteral("Export data disabled tip=Simulation has no data to export")}));
    EXPECT_EQ(dialog.tabs().widget(SimulationConfigDialog::kPlotTab)->objectName(),
              QStringLiteral("plotPlaceholder"));
    EXPECT_EQ(dialog.exportPanel(), nullptr);
    EXPECT_EQ(dialog.tabs().widget(SimulationConfigDialog::kExportTab)->objectName(),
              QStringLiteral("exportPlaceholder"));
}

TEST_F(SimulationConfigDialogTest, AMultiSimulationEditShowsItsLabelAndDisablesPlotAndExport)
{
    Session                       session;
    const SimulationConfigDialog& dialog =
        session.open(editRequest({session.simulation(0), session.simulation(1)}));
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("Multi-simulation edit"));
    EXPECT_TRUE(dialog.isMultiEdit());
    const StyledLabel* const label = dialog.multiSimEditLabel();
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->text(), QStringLiteral("Multi-simulation edit"));
    EXPECT_EQ(label->toolTip(), QStringLiteral("<html>You are editing the following simulations: "
                                               "<br>Simulation 1, Simulation 2"));
    EXPECT_TRUE(label->font().bold());
    // The name and the configuration are the first simulation's.
    EXPECT_EQ(dialog.nameField().text(), QStringLiteral("Simulation 1"));
    EXPECT_EQ(dialog.configurationComboBox().selectedConfigurationId(),
              session.simulation(0)->getFlightConfigurationId());
    // The data of the first is there, but a multi-simulation edit neither plots nor exports.
    EXPECT_NE(dialog.plotPanel(), nullptr);
    EXPECT_NE(dialog.exportPanel(), nullptr);
    EXPECT_EQ(tabStates(dialog.tabs()).mid(3),
              (QStringList{
                  QStringLiteral(
                      "Plot data disabled tip=Plotting not supported for multi-simulation editing"),
                  QStringLiteral("Export data disabled tip=Exporting not supported for "
                                 "multi-simulation editing")}));
    // The Warnings tab without the simulation's warnings.
    EXPECT_NE(dialog.warningsPanel().unavailableLabel(), nullptr);
    EXPECT_EQ(session.document().getUndoDetail().nextDescription,
              std::optional<std::string>("Edit simulations"));
    // The bottom row: the label, then the buttons.
    const std::vector<std::string> bottom = Gui::Test::dialogWalk(dialog.bottomPanel());
    ASSERT_EQ(bottom.size(), 3U);
    EXPECT_EQ(bottom.front(),
              "LABEL Multi-simulation edit tip=<html>You are editing the following simulations: "
              "<br>Simulation 1, Simulation 2");
}

TEST_F(SimulationConfigDialogTest, TheButtonsFollowTheSelectedTab)
{
    Session                       session;
    const SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    EXPECT_EQ(buttonTextsPerTab(dialog),
              (QStringList{QStringLiteral("OK / Cancel"), QStringLiteral("OK / Cancel"),
                           QStringLiteral("OK / Cancel"), QStringLiteral("Plot / Close"),
                           QStringLiteral("Export / Close")}));
    dialog.tabs().setCurrentIndex(SimulationConfigDialog::kLaunchConditionsTab);
    EXPECT_EQ(buttonTexts(dialog), QStringLiteral("OK / Cancel"));
    // The tooltips stay.
    dialog.tabs().setCurrentIndex(SimulationConfigDialog::kPlotTab);
    EXPECT_EQ(dialog.okButton().toolTip(), QStringLiteral("Keep changes and close the dialog"));
    EXPECT_EQ(dialog.cancelButton().toolTip(),
              QStringLiteral("Discard changes and close the dialog"));
}

/// A new dialog of @p session asked to open on @p tab: on the tab @p index, with the buttons
/// @p buttons.
void expectOpensOn(Session& session, Tab tab, int index, const QString& buttons)
{
    const SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}, tab));
    EXPECT_EQ(dialog.tabs().currentIndex(), index);
    EXPECT_EQ(SimulationConfigDialog::tabIndex(tab), index);
    EXPECT_EQ(buttonTexts(dialog), buttons);
}

TEST_F(SimulationConfigDialogTest, OpensOnTheTabItIsAskedFor)
{
    // SimulationPanel opens it on the first tab (New, Edit), the Warnings tab (a double click on
    // the Warnings column) or the Plot data tab (Plot / Export).
    Session session;
    expectOpensOn(session, Tab::LAUNCH_CONDITIONS, 0, QStringLiteral("OK / Cancel"));
    expectOpensOn(session, Tab::WARNINGS, 2, QStringLiteral("OK / Cancel"));
    expectOpensOn(session, Tab::PLOT_DATA, 3, QStringLiteral("Plot / Close"));
    expectOpensOn(session, Tab::EXPORT_DATA, 4, QStringLiteral("Export / Close"));
}

TEST_F(SimulationConfigDialogTest, NoButtonIsTheDefaultOneAndReturnClicksNone)
{
    Session                 session;
    SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    dialog.show();
    QCoreApplication::processEvents();
    for (const QPushButton* const button : dialog.findChildren<QPushButton*>())
    {
        EXPECT_FALSE(button->isDefault()) << QtStrings::toUtf8(button->objectName());
        EXPECT_FALSE(button->autoDefault()) << QtStrings::toUtf8(button->objectName());
    }
    dialog.nameField().setFocus();
    QTest::keyClick(&dialog.nameField(), Qt::Key_Return);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_FALSE(dialog.isClosed());
    dialog.cancel();
    EXPECT_FALSE(dialog.isVisible());
}

// ----------------------------------------------------------------------------- the edits

TEST_F(SimulationConfigDialogTest, TheNameAndTheConfigurationEditTheFirstSimulationAtOnce)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    const SimulationConfigDialog&     dialog     = session.open(editRequest({simulation}));

    // The first change puts "* " before the title, once.
    QTest::keyClicks(&dialog.nameField(), QStringLiteral("x"));
    EXPECT_EQ(simulation->getName(), "Simulation 1x");
    EXPECT_TRUE(dialog.isModified());
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("* Edit simulation"));
    // An empty field leaves the name.
    dialog.nameField().clear();
    EXPECT_EQ(simulation->getName(), "Simulation 1x");
    dialog.nameField().setText(QStringLiteral("Renamed"));
    EXPECT_EQ(simulation->getName(), "Renamed");

    // The configuration: the combo box's own selection, the simulation's.
    ConfigurationComboBox& configurations = dialog.configurationComboBox();
    EXPECT_FALSE(configurations.updatesRocketConfig());
    const FlightConfigurationId selectedBefore =
        session.document().getRocket().getSelectedConfiguration().getId();
    Gui::Test::chooseItem(configurations, 2);
    EXPECT_EQ(configurations.currentText(), QStringLiteral("[B4-4]"));
    EXPECT_EQ(simulation->getFlightConfigurationId(), configurations.configurationIdAt(2));
    EXPECT_EQ(session.document().getRocket().getSelectedConfiguration().getId(), selectedBefore);
    EXPECT_EQ(dialog.windowTitle(), QStringLiteral("* Edit simulation"));
    // The edits are the dialog's undo step.
    EXPECT_EQ(undoDescription(session.document()), "Edit Simulation 1");
}

TEST_F(SimulationConfigDialogTest, TheOptionsTabsEditTheFirstSimulationsOptions)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    const SimulationConfigDialog&     dialog     = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_EQ(simulation->getStatus(), Status::OUTDATED);
    EXPECT_TRUE(dialog.isModified());
    Gui::Test::chooseItem(dialog.optionsPanel().geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    EXPECT_EQ(simulation->getOptions().getGeodeticComputation(),
              GeodeticComputationStrategy::WGS84);
}

TEST_F(SimulationConfigDialogTest, TheStatusLineShowsTheStatusInItsColourAsTheDialogOpens)
{
    Session session;
    session.outdate(0);
    const SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    const QString                 expected =
        QStringLiteral("<html>") +
        ColorConversion::formatHtmlColor(SimulationStatusColors::color(Status::OUTDATED),
                                         QStringLiteral("<i>Out of date</i>")) +
        QStringLiteral("</html>");
    EXPECT_EQ(dialog.statusText().text(), expected);
    EXPECT_EQ(dialog.statusText().textFormat(), Qt::RichText);
}

// ---------------------------------------------------------------------------------------- OK

TEST_F(SimulationConfigDialogTest, OkOfASimulationUpToDateClosesWithoutARun)
{
    Session                 session;
    SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    dialog.show();
    dialog.okButton().click();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(session.prompts().prompts().empty());
    // Closed, it does nothing more.
    dialog.ok();
    dialog.cancel();
    EXPECT_TRUE(session.prompts().prompts().empty());
}

TEST_F(SimulationConfigDialogTest, OkRunsTheOutdatedSimulationsThenCloses)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    const SimulationConfigDialog&     dialog     = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    ASSERT_EQ(simulation->getStatus(), Status::OUTDATED);
    dialog.okButton().click();

    // The run dialog's request: the design window, the document, the outdated simulation and
    // the services' executor; the scripted run dialog really ran it.
    ASSERT_EQ(session.prompts().runSimulationsQuestions().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::RunSimulationsQuestion& question =
        session.prompts().runSimulationsQuestions().front();
    EXPECT_EQ(question.simulationNames, QStringList{QStringLiteral("Simulation 1")});
    EXPECT_EQ(question.window, &session.window());
    EXPECT_EQ(question.document, &session.document());
    EXPECT_EQ(question.executor, &session.services().executor());
    EXPECT_EQ(simulation->getStatus(), Status::UPTODATE);
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(SimulationConfigDialogTest, OkCommitsAPendingEditFirst)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog&           dialog     = session.open(editRequest({simulation}));
    // Typed, not committed (a button that takes no focus leaves the edit pending).
    UnitSpinBox& rodLength = *dialog.conditionsPanel().rodLength().spinBox;
    rodLength.selectAll();
    QTest::keyClicks(&rodLength, QStringLiteral("150"));
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.0);
    session.prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    dialog.ok();
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_EQ(session.prompts().runSimulationsQuestions().size(), 1U);
}

TEST_F(SimulationConfigDialogTest, PlotRunsTheOutdatedSimulationThenPlotsAndStaysOpen)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog& dialog = session.open(editRequest({simulation}, Tab::PLOT_DATA));
    session.outdate(0);
    dialog.okButton().click();
    EXPECT_EQ(session.prompts().runSimulationsQuestions().size(), 1U);
    EXPECT_EQ(simulation->getStatus(), Status::UPTODATE);
    // The plot dialog over the settings dialog, which stays open.
    const std::vector<Gui::Test::RecordingSimulationDialogs::Request>& requests =
        session.services().simulationDialogs().requests();
    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests.front().kind, RequestKind::PLOT);
    EXPECT_EQ(requests.front().parent, &dialog);
    EXPECT_EQ(requests.front().simulationNames, QStringList{QStringLiteral("Simulation 1")});
    EXPECT_FALSE(dialog.isClosed());
    // Plot again: no run now.
    dialog.ok();
    EXPECT_EQ(session.prompts().runSimulationsQuestions().size(), 1U);
    EXPECT_EQ(session.services().simulationDialogs().requests().size(), 2U);
    // Close keeps the edits, without a question.
    dialog.cancelButton().click();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(promptKinds(session.prompts()), std::vector<std::string>{"RUN_SIMULATIONS"});
}

TEST_F(SimulationConfigDialogTest, PlotAfterARunThatLeftNoDataPlotsNothingAndStaysOpen)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog& dialog = session.open(editRequest({simulation}, Tab::PLOT_DATA));
    session.outdate(0);
    // A run that leaves the simulation without data (a refused input does: SimulationRun).
    session.prompts().answerRunSimulationsWith(
        [simulation](QWidget* /*parent*/, const UserPrompts::RunSimulationsRequest& /*request*/) {
            simulation->loadFrom(*simulation->copy());
            return UserPrompts::RunSimulationsAnswer{.cancelled = false, .allSuccessful = false};
        });
    dialog.ok();
    ASSERT_FALSE(simulation->hasSimulationData());
    EXPECT_TRUE(session.services().simulationDialogs().requests().empty());
    EXPECT_FALSE(dialog.isClosed());
}

TEST_F(SimulationConfigDialogTest, PlotWithoutAPlotTabCloses)
{
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const auto simulation = std::make_shared<Simulation>(document.get(), document->getRocket());
    SimulationConfigDialog dialog(services.services(), *document,
                                  editRequest({simulation}, Tab::PLOT_DATA));
    // The new rocket has no motors: its simulation cannot run (CANT_RUN), which is not up to date,
    // so OK first hands it to the run dialog, as Java's getOutdatedSimulations() does, and then
    // closes (Java: plotTab == null).
    ASSERT_EQ(simulation->getStatus(), Status::CANT_RUN);
    services.prompts().answerRunSimulations({.cancelled = false, .allSuccessful = false});
    dialog.ok();
    EXPECT_EQ(promptKinds(services.prompts()), std::vector<std::string>{"RUN_SIMULATIONS"});
    ASSERT_EQ(services.prompts().runSimulationsQuestions().size(), 1U);
    EXPECT_EQ(services.prompts().runSimulationsQuestions().front().simulationNames,
              QStringList{QtStrings::fromUtf8(simulation->getName())});
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_TRUE(services.simulationDialogs().requests().empty());
}

TEST_F(SimulationConfigDialogTest,
       ExportRunsTheOutdatedSimulationThenWritesItsNewFlightAndStaysOpen)
{
    // OpenRocket bug 49: Java's export tab writes the flight the dialog opened with, under the
    // comments of the new one. Here the file question comes after the run, and the file holds
    // the simulation's flight data as the run left it.
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    const SimulationConfigDialog&     dialog =
        session.open(editRequest({simulation}, Tab::EXPORT_DATA));
    const SimulationExportPanel* const panel = dialog.exportPanel();
    ASSERT_NE(panel, nullptr);
    panel->exportPanel().setSelected(0, true);
    panel->exportPanel().setSelected(1, true);
    const std::shared_ptr<FlightData> stored = simulation->getSimulatedData();
    session.outdate(0);
    const QtRocket::Test::TempDir directory;
    session.prompts().answerSaveFile(
        UserPrompts::SaveFileAnswer{.file               = directory.resolve("flight"),
                                    .directory          = directory.path(),
                                    .saveSimulationData = std::nullopt,
                                    .selectedNameFilter = FileNames::csvFilter(),
                                    .csvOptions         = std::nullopt});
    EXPECT_EQ(dialog.okButton().text(), QStringLiteral("Export"));
    dialog.okButton().click();

    EXPECT_EQ(promptKinds(session.prompts()),
              (std::vector<std::string>{"RUN_SIMULATIONS", "SAVE_FILE"}));
    EXPECT_EQ(simulation->getStatus(), Status::UPTODATE);
    const std::shared_ptr<FlightData>& flown = simulation->getSimulatedData();
    ASSERT_NE(flown, nullptr);
    EXPECT_NE(flown, stored);
    EXPECT_EQ(exportedText(directory.resolve("flight.csv")),
              coreExportOf(*simulation, flown->getBranch(0), *panel));
    EXPECT_TRUE(
        exportedText(directory.resolve("flight.csv")).starts_with("# Simulation 1 (Up to date)\n"));
    // The dialog stays open; Close keeps the edits, without a question.
    EXPECT_FALSE(dialog.isClosed());
    dialog.cancelButton().click();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(session.prompts().prompts().size(), 2U);
}

TEST_F(SimulationConfigDialogTest, ExportAfterARunThatLeftNoDataAsksNothingAndStaysOpen)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog& dialog = session.open(editRequest({simulation}, Tab::EXPORT_DATA));
    session.outdate(0);
    session.prompts().answerRunSimulationsWith(
        [simulation](QWidget* /*parent*/, const UserPrompts::RunSimulationsRequest& /*request*/) {
            simulation->loadFrom(*simulation->copy());
            return UserPrompts::RunSimulationsAnswer{.cancelled = false, .allSuccessful = false};
        });
    dialog.ok();
    ASSERT_FALSE(simulation->hasSimulationData());
    EXPECT_EQ(promptKinds(session.prompts()), std::vector<std::string>{"RUN_SIMULATIONS"});
    EXPECT_FALSE(dialog.isClosed());
}

TEST_F(SimulationConfigDialogTest, ExportWithoutAnExportTabCloses)
{
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const auto simulation = std::make_shared<Simulation>(document.get(), document->getRocket());
    SimulationConfigDialog dialog(services.services(), *document,
                                  editRequest({simulation}, Tab::EXPORT_DATA));
    // The run of the simulation that cannot run (CANT_RUN, not up to date) comes first, then the
    // dialog closes (Java: exportTab == null).
    ASSERT_EQ(simulation->getStatus(), Status::CANT_RUN);
    services.prompts().answerRunSimulations({.cancelled = false, .allSuccessful = false});
    dialog.ok();
    EXPECT_EQ(promptKinds(services.prompts()), std::vector<std::string>{"RUN_SIMULATIONS"});
    EXPECT_EQ(services.prompts().runSimulationsQuestions().size(), 1U);
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_TRUE(services.prompts().saveFileRequests().empty());
}

TEST_F(SimulationConfigDialogTest, OkOfAMultiSimulationEditCopiesTheConditionsAndExtensions)
{
    Session                           session;
    const std::shared_ptr<Simulation> first  = session.simulation(0);
    const std::shared_ptr<Simulation> second = session.simulation(1);
    first->getSimulationExtensions().push_back(std::make_shared<AirStart>());
    const SimulationConfigDialog& dialog = session.open(editRequest({first, second}));
    typeRodLength(dialog, "150");
    Gui::Test::chooseItem(dialog.optionsPanel().geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    // The other simulation is not touched before OK.
    EXPECT_DOUBLE_EQ(second->getOptions().getLaunchRodLength(), 1.0);
    // The run dialog ends without running: the runs are not this test's.
    session.prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    dialog.okButton().click();

    EXPECT_DOUBLE_EQ(second->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_EQ(second->getOptions().getGeodeticComputation(), GeodeticComputationStrategy::WGS84);
    ASSERT_EQ(second->getSimulationExtensions().size(), 1U);
    // Clones, not the same objects.
    EXPECT_NE(second->getSimulationExtensions().front(), first->getSimulationExtensions().front());
    EXPECT_EQ(second->getSimulationExtensions().front()->getId(),
              first->getSimulationExtensions().front()->getId());
    // The name and the configuration stay the second's own.
    EXPECT_EQ(second->getName(), "Simulation 2");
    // Both are outdated now, and both run.
    ASSERT_EQ(session.prompts().runSimulationsQuestions().size(), 1U);
    EXPECT_EQ(session.prompts().runSimulationsQuestions().front().simulationNames,
              (QStringList{QStringLiteral("Simulation 1"), QStringLiteral("Simulation 2")}));
    EXPECT_TRUE(dialog.isClosed());
}

// ----------------------------------------------------------------------------------- Cancel

TEST_F(SimulationConfigDialogTest, CancelOfAnUnchangedSimulationClosesWithoutAQuestion)
{
    Session session;
    renameFirstStage(session.document(), "Renamed");
    const SimulationConfigDialog& dialog = session.open(editRequest({session.simulation(0)}));
    dialog.cancelButton().click();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(dialog.result(), QDialog::Rejected);
    EXPECT_TRUE(session.prompts().prompts().empty());
    // Nothing was undone.
    EXPECT_EQ(firstStage(session.document()).getName(), "Renamed");
}

TEST_F(SimulationConfigDialogTest, CancelAsksThenDiscardsTheChanges)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    renameFirstStage(session.document(), "Renamed");
    session.document().setSaved(true);
    const SimulationConfigDialog& dialog = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    dialog.nameField().setText(QStringLiteral("Edited"));
    EXPECT_FALSE(session.document().isSaved());
    std::vector<DocumentChangeEvent>                           events;
    const Signal<const DocumentChangeEvent&>::ScopedConnection listening(
        session.document().documentChanged().connect(
            [&events](const DocumentChangeEvent& event) { events.push_back(event); }));

    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancelButton().click();
    ASSERT_EQ(session.prompts().prompts().size(), 1U);
    EXPECT_EQ(session.prompts().prompts().front().kind, PromptKind::DISCARD_CHANGES);
    EXPECT_EQ(session.prompts().prompts().front().texts,
              QStringList{QStringLiteral("SIMULATION_CHANGES")});
    EXPECT_TRUE(dialog.isClosed());
    // The simulation as it was, the change before the dialog kept, the document saved again,
    // and the event of the discard last.
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.0);
    EXPECT_EQ(simulation->getName(), "Simulation 1");
    EXPECT_EQ(firstStage(session.document()).getName(), "Renamed");
    EXPECT_TRUE(session.document().isSaved());
    ASSERT_FALSE(events.empty());
    EXPECT_EQ(events.back(), (DocumentChangeEvent{.kind   = DocumentChangeEvent::Kind::DOCUMENT,
                                                  .source = std::monostate{}}));
    EXPECT_TRUE(session.preferences().isShowDiscardSimulationConfirmation());
}

TEST_F(SimulationConfigDialogTest, CancelAnsweredNoKeepsTheDialogAndItsEdits)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog&           dialog     = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    session.prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_FALSE(dialog.isClosed());
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.5);
    // The fields still edit the simulation.
    typeRodLength(dialog, "120");
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.2);
}

TEST_F(SimulationConfigDialogTest, DontAskMeAgainTurnsTheQuestionOffWhateverTheAnswer)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog&           dialog     = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    session.prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = true});
    dialog.cancel();
    EXPECT_FALSE(session.preferences().isShowDiscardSimulationConfirmation());
    EXPECT_FALSE(dialog.isClosed());
    // Without the question, Cancel discards at once.
    dialog.cancel();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(session.prompts().prompts().size(), 1U);
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.0);
}

TEST_F(SimulationConfigDialogTest, EscapeAndTheCloseButtonActAsCancel)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    SimulationConfigDialog&           dialog     = session.open(editRequest({simulation}));
    dialog.show();
    QCoreApplication::processEvents();
    typeRodLength(dialog, "150");
    // The window's close button: asked, No, the dialog stays.
    session.prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = false});
    EXPECT_FALSE(dialog.close());
    EXPECT_TRUE(dialog.isVisible());
    // Escape: asked, Yes, discarded.
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    QTest::keyClick(&dialog, Qt::Key_Escape);
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.0);
    EXPECT_EQ(session.prompts().prompts().size(), 2U);
}

TEST_F(SimulationConfigDialogTest, CancelOfANewSimulationAsksToUndoAddingIt)
{
    Session                           session;
    const std::shared_ptr<Simulation> added =
        session.simulation(0)->duplicateSimulation(session.document().getRocket());
    added->setName("Simulation 6");
    // SimulationPanel.newSimulation(): the undo position, then the addition.
    session.document().addUndoPosition("Add Simulation 6");
    session.document().addSimulation(added);
    SimulationConfigDialog& dialog =
        session.open(editRequest({added}, Tab::LAUNCH_CONDITIONS, true));
    EXPECT_TRUE(dialog.isNewSimulation());
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_EQ(session.prompts().prompts().front().texts,
              QStringList{QStringLiteral("SIMULATION_ADDITION")});
    EXPECT_FALSE(session.document().getSimulationIndex(*added).has_value());
    EXPECT_EQ(session.document().getSimulationCount(), 5U);
}

TEST_F(SimulationConfigDialogTest, CancelOfANewSimulationAfterAnEditUndoesTheAdditionToo)
{
    // OpenRocket bug 30: Java's Cancel only undoes the edit, and the new simulation stays (the
    // sim-config scout's probe: "simulations left 1").
    Session session;
    renameFirstStage(session.document(), "Renamed");
    const std::shared_ptr<Simulation> added =
        session.simulation(0)->duplicateSimulation(session.document().getRocket());
    added->setName("Simulation 6");
    session.document().addUndoPosition("Add Simulation 6");
    session.document().addSimulation(added);
    SimulationConfigDialog& dialog =
        session.open(editRequest({added}, Tab::LAUNCH_CONDITIONS, true));
    typeRodLength(dialog, "150");
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_FALSE(session.document().getSimulationIndex(*added).has_value());
    EXPECT_EQ(session.document().getSimulationCount(), 5U);
    // No further: the change before the addition stays.
    EXPECT_EQ(firstStage(session.document()).getName(), "Renamed");
}

TEST_F(SimulationConfigDialogTest, CancelAfterAGeodeticEditKeepsTheChangeMadeBefore)
{
    // OpenRocket bug 29 through the dialog (the sim-config scout's probe: Java's Cancel after an
    // edit of the geodetic method only reverts the stage renamed before the dialog opened).
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    renameFirstStage(session.document(), "Renamed");
    const GeodeticComputationStrategy before = simulation->getOptions().getGeodeticComputation();
    SimulationConfigDialog&           dialog = session.open(editRequest({simulation}));
    Gui::Test::chooseItem(dialog.optionsPanel().geodeticComboBox(),
                          geodeticIndex(GeodeticComputationStrategy::WGS84));
    ASSERT_NE(simulation->getOptions().getGeodeticComputation(), before);
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_EQ(simulation->getOptions().getGeodeticComputation(), before);
    EXPECT_EQ(firstStage(session.document()).getName(), "Renamed");
}

TEST_F(SimulationConfigDialogTest, CancelAfterAnEditUndoneByHandKeepsTheChangeMadeBefore)
{
    // OpenRocket bug 29, the dialog's half: the edits leave the document as it was when the
    // dialog opened, so Java's undo() goes back one step further and reverts the rename.
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    renameFirstStage(session.document(), "Renamed");
    SimulationConfigDialog& dialog = session.open(editRequest({simulation}));
    typeRodLength(dialog, "150");
    typeRodLength(dialog, "100");
    ASSERT_TRUE(dialog.isModified());
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_TRUE(dialog.isClosed());
    EXPECT_EQ(firstStage(session.document()).getName(), "Renamed");
    EXPECT_DOUBLE_EQ(simulation->getOptions().getLaunchRodLength(), 1.0);
}

// ------------------------------------------------------------------- the window's estimate

/// What the hooks of a request were called with.
struct HookCalls
{
    std::vector<bool>        pauses;
    std::vector<QStringList> cancelled;
    /// The launch rod length of the first simulation when the estimate was resumed.
    std::optional<double> rodLengthAtResume;
};

/// A request over @p simulation whose hooks record into @p calls.
[[nodiscard]] SimulationDialogs::EditRequest hookedRequest(
    const std::shared_ptr<Simulation>& simulation, HookCalls& calls)
{
    SimulationDialogs::EditRequest request = editRequest({simulation});
    request.cancelBackgroundRuns =
        [&calls](std::span<const std::shared_ptr<Simulation>> simulations) {
            calls.cancelled.push_back(namesOf(simulations));
        };
    request.pauseBackgroundRuns = [&calls, simulation](bool paused) {
        calls.pauses.push_back(paused);
        if (!paused)
        {
            calls.rodLengthAtResume = simulation->getOptions().getLaunchRodLength();
        }
    };
    return request;
}

TEST_F(SimulationConfigDialogTest, PausesTheBackgroundEstimateWhileOpenAndCancelsItsRuns)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    HookCalls                         calls;
    SimulationConfigDialog&           dialog = session.open(hookedRequest(simulation, calls));
    EXPECT_EQ(calls.pauses, std::vector<bool>{true});
    typeRodLength(dialog, "150");
    session.prompts().answerRunSimulations({.cancelled = false, .allSuccessful = true});
    dialog.ok();
    // The run's request carries the hook; the scripted run dialog does not call it.
    EXPECT_EQ(calls.pauses, (std::vector<bool>{true, false}));
    // The opener's hook goes to the run dialog's request.
    HookCalls                            runCalls;
    const SimulationDialogs::EditRequest request = hookedRequest(simulation, runCalls);
    SimulationConfigDialog&              second  = session.open(request);
    session.prompts().answerRunSimulationsWith(
        [](QWidget* /*parent*/, const UserPrompts::RunSimulationsRequest& run) {
            if (run.cancelBackgroundRuns)
            {
                run.cancelBackgroundRuns(run.simulations);
            }
            return UserPrompts::RunSimulationsAnswer{.cancelled = false, .allSuccessful = true};
        });
    typeRodLength(second, "120");
    second.ok();
    EXPECT_EQ(runCalls.cancelled,
              std::vector<QStringList>{QStringList{QStringLiteral("Simulation 1")}});
    EXPECT_EQ(runCalls.pauses, (std::vector<bool>{true, false}));
}

TEST_F(SimulationConfigDialogTest, ResumesTheBackgroundEstimateAfterCancelsUndo)
{
    Session                           session;
    const std::shared_ptr<Simulation> simulation = session.simulation(0);
    HookCalls                         calls;
    SimulationConfigDialog&           dialog = session.open(hookedRequest(simulation, calls));
    typeRodLength(dialog, "150");
    session.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog.cancel();
    EXPECT_EQ(calls.pauses, (std::vector<bool>{true, false}));
    ASSERT_TRUE(calls.rodLengthAtResume.has_value());
    EXPECT_DOUBLE_EQ(calls.rodLengthAtResume.value_or(0), 1.0);
}

TEST_F(SimulationConfigDialogTest, TheOpenDialogGoesWithItsWindow)
{
    // The production SimulationDialogs::editSimulations() (edit()): modal over the window, which
    // goes while the dialog is open; edit() returns, and the estimate is not resumed (the window's
    // design view went with it).
    const Gui::Test::TestGuiServices services;
    const ExampleDesign              design(ExampleDesign::kSimpleModelRocket);
    auto                             window = std::make_unique<QWidget>();
    window->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(window.get()));
    HookCalls                        calls;
    const Gui::Test::NextModalDialog answer([&window](QWidget& dialog) {
        EXPECT_NE(qobject_cast<SimulationConfigDialog*>(&dialog), nullptr);
        EXPECT_EQ(dialog.parentWidget(), window.get());
        window.reset();
    });
    SimulationConfigDialog::edit(window.get(), services.services(), design.document(),
                                 hookedRequest(design.simulation(0), calls));
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(calls.pauses, std::vector<bool>{true});
}

/// OK's run dialog, during which the window goes (and the settings dialog, its child, with it):
/// the run counts as cancelled.
struct CloseTheWindowDuringTheRun
{
    std::unique_ptr<QWidget>* window;

    UserPrompts::RunSimulationsAnswer operator()(
        QWidget* /*parent*/, const UserPrompts::RunSimulationsRequest& /*request*/) const
    {
        window->reset();
        return {.cancelled = true, .allSuccessful = false};
    }
};

/// The test's prompts, which close the window (and the settings dialog, its child, with it) while
/// the discard question is asked, after the script's answer.
class CloseTheWindowDuringTheDiscardQuestion final : public Gui::Test::ForwardingUserPrompts
{
public:
    CloseTheWindowDuringTheDiscardQuestion(UserPrompts& delegate, std::unique_ptr<QWidget>& window)
      : ForwardingUserPrompts(delegate), m_window(&window)
    {
    }

    [[nodiscard]] DiscardAnswer askDiscardChanges(QWidget*        parent,
                                                  DiscardQuestion question) override
    {
        const DiscardAnswer answer = delegate().askDiscardChanges(parent, question);
        m_window->reset();
        return answer;
    }

private:
    std::unique_ptr<QWidget>* m_window;
};

/// Edits the launch rod length of the settings dialog @p dialog (150 cm), then clicks @p button
/// (OK or Cancel) of it, which may destroy the dialog.
struct EditThenClick
{
    enum class Button
    {
        OK,
        CANCEL,
    };

    Button button;

    void operator()(QWidget& dialog) const
    {
        auto* const settings = qobject_cast<SimulationConfigDialog*>(&dialog);
        ASSERT_NE(settings, nullptr);
        typeRodLength(*settings, "150");
        ASSERT_TRUE(settings->isModified());
        (button == Button::OK ? settings->okButton() : settings->cancelButton()).click();
    }
};

TEST_F(SimulationConfigDialogTest, TheDialogGoingWithItsWindowDuringOksRunEndsItsOk)
{
    // The window goes while OK's run dialog is open: OK reads nothing of the destroyed dialog
    // afterwards (no plot, no close of its own), edit() returns, and the estimate is not resumed
    // (the window's design view went with it). The edit stays: nothing undoes it.
    Gui::Test::TestGuiServices services;
    const ExampleDesign        design(ExampleDesign::kSimpleModelRocket);
    auto                       window = std::make_unique<QWidget>();
    window->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(window.get()));
    HookCalls calls;
    services.prompts().answerRunSimulationsWith(CloseTheWindowDuringTheRun{&window});
    const Gui::Test::NextModalDialog answer(EditThenClick{EditThenClick::Button::OK});
    SimulationConfigDialog::edit(window.get(), services.services(), design.document(),
                                 hookedRequest(design.simulation(0), calls));
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(promptKinds(services.prompts()), std::vector<std::string>{"RUN_SIMULATIONS"});
    EXPECT_EQ(calls.pauses, std::vector<bool>{true});
    EXPECT_DOUBLE_EQ(design.simulation(0)->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_TRUE(services.simulationDialogs().requests().empty());
}

TEST_F(SimulationConfigDialogTest, TheDialogGoingWithItsWindowDuringTheDiscardQuestionEndsCancel)
{
    // The window goes while Cancel's discard question is open: Cancel reads nothing of the
    // destroyed dialog afterwards (no discard, no undo), edit() returns, and the estimate is not
    // resumed. "Don't ask me again" is kept whatever happened (Java turns it off with the answer).
    Gui::Test::TestGuiServices services;
    const ExampleDesign        design(ExampleDesign::kSimpleModelRocket);
    auto                       window = std::make_unique<QWidget>();
    window->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(window.get()));
    CloseTheWindowDuringTheDiscardQuestion prompts(services.prompts(), window);
    const GuiServices                      closing = services.services().withPrompts(prompts);
    HookCalls                              calls;
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = true});
    const Gui::Test::NextModalDialog answer(EditThenClick{EditThenClick::Button::CANCEL});
    SimulationConfigDialog::edit(window.get(), closing, design.document(),
                                 hookedRequest(design.simulation(0), calls));
    EXPECT_TRUE(answer.acted());
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(promptKinds(services.prompts()), std::vector<std::string>{"DISCARD_CHANGES"});
    EXPECT_EQ(calls.pauses, std::vector<bool>{true});
    EXPECT_DOUBLE_EQ(design.simulation(0)->getOptions().getLaunchRodLength(), 1.5);
    EXPECT_FALSE(services.preferences().isShowDiscardSimulationConfirmation());
}

/// What the window's estimate sees of the dialog when the dialog pauses it: the number of tabs
/// of the settings dialog under @p window, or -1 while the dialog has none yet.
struct NoteTheDialogAtThePause
{
    const QWidget*    window;
    std::vector<int>* tabCounts;

    void operator()(bool paused) const
    {
        if (!paused)
        {
            return;
        }
        const auto* const tabs = window->findChild<QTabWidget*>(QStringLiteral("tabbedPane"));
        tabCounts->push_back(tabs == nullptr ? -1 : tabs->count());
    }
};

TEST_F(SimulationConfigDialogTest, PausesTheEstimateOnlyOnceItIsComplete)
{
    // The estimate is paused last, once the dialog is complete (its panels made, its tabs
    // there): a BugError of a panel while the dialog is made then leaves no pause behind that
    // nothing would end (the hook is called with false only after a close).
    Session                        session;
    std::vector<int>               tabCounts;
    SimulationDialogs::EditRequest request = editRequest({session.simulation(0)});
    request.pauseBackgroundRuns =
        NoteTheDialogAtThePause{.window = &session.window(), .tabCounts = &tabCounts};
    const SimulationConfigDialog& dialog = session.open(request);
    EXPECT_EQ(tabCounts, std::vector<int>{5});
    EXPECT_FALSE(dialog.isClosed());
}

// ---------------------------------------------------------------------------- the geometry

TEST_F(SimulationConfigDialogTest, TakesItsContentsSizeCappedToTheScreen)
{
    Session                       session;
    const SimulationConfigDialog& dialog    = session.open(editRequest({session.simulation(0)}));
    const QRect                   available = dialog.screen()->availableGeometry();
    const QSize                   cap(available.width() - SimulationConfigDialog::kScreenMargin,
                                      available.height() - SimulationConfigDialog::kScreenMargin);
    EXPECT_EQ(dialog.size(), dialog.sizeHint().boundedTo(cap).expandedTo(dialog.minimumSize()));
}

TEST_F(SimulationConfigDialogTest, RemembersItsSizeCappedToTheScreen)
{
    Session                 session;
    SimulationConfigDialog& first = session.open(editRequest({session.simulation(0)}));
    first.show();
    QCoreApplication::processEvents();
    const QSize remembered(640, 470);
    first.resize(remembered);
    QCoreApplication::processEvents();
    first.cancel();
    // GUIUtil.rememberWindowSize() and rememberWindowPosition(): one key of the node "windows".
    const Preferences* const windows =
        session.preferences().findNode(GuiPreferenceKeys::kNodeWindows);
    ASSERT_NE(windows, nullptr);
    EXPECT_TRUE(
        windows->get(WindowGeometry::key(QStringLiteral("SimulationConfigDialog"))).has_value());
    const SimulationConfigDialog& second = session.open(editRequest({session.simulation(1)}));
    EXPECT_EQ(second.size(), remembered);

    // Larger than the screen's area less the margin: capped.
    SimulationConfigDialog& third = session.open(editRequest({session.simulation(0)}));
    third.show();
    QCoreApplication::processEvents();
    const QRect available = third.screen()->availableGeometry();
    third.resize(available.width() - 10, available.height() - 10);
    QCoreApplication::processEvents();
    third.cancel();
    const SimulationConfigDialog& fourth = session.open(editRequest({session.simulation(0)}));
    EXPECT_EQ(fourth.size(), QSize(available.width() - SimulationConfigDialog::kScreenMargin,
                                   available.height() - SimulationConfigDialog::kScreenMargin));
}

}  // namespace

}  // namespace QtRocket::Gui
