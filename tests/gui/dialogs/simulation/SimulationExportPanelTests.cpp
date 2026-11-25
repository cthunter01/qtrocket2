// SimulationExportPanel: OpenRocket's "Export data" tab of the simulation settings dialog
// (SimulationExportPanel.java, which has no JUnit test of its own): the tab as OpenRocket shows it
// (the M4 sim-config scout's walk of "A simple model rocket" Simulation 1, java/dialog-walk.txt,
// "TAB Export data", hand-pinned here), its rows, ticks and options from the preferences, the
// stage box, and the export: the file and overwrite questions, what it stores, the file it writes
// (the core's CsvExport of the same choices, byte for byte), the flight data as it is at the export
// (OpenRocket bug 49) and the errors (bug 53). The questions are scripted (ScriptedUserPrompts);
// no flight is run here (the settings dialog's tests run one before an export). The comparison
// with OpenRocket's own files is simulation_export_golden_tests.cpp.

#include "QtRocket/gui/dialogs/simulation/SimulationExportPanel.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ForwardingUserPrompts.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/CsvExport.h"
#include "QtRocket/gui/settings/CsvOptionPreferences.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/CsvOptionValues.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/FileNames.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/CsvExportPanel.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
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

using SimulationExportPanelTest = Gui::Test::GuiTest;
using Gui::Test::ExampleDesign;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;
using Status     = Simulation::Status;
using TypeId     = FlightDataTypeId;

/// The tab of a test: the test's services (scripted questions), an example design, and the tab
/// over one of its simulations, a child of a widget that is never shown.
class TabUnderTest
{
public:
    explicit TabUnderTest(const char* example    = ExampleDesign::kSimpleModelRocket,
                          std::size_t simulation = 0)
      : m_design(example)
    {
        m_panel = makeChild<SimulationExportPanel>(&m_holder, m_services.services(),
                                                   m_design.simulation(simulation));
    }

    [[nodiscard]] Gui::Test::TestGuiServices&     services() noexcept { return m_services; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept
    {
        return m_services.prompts();
    }
    [[nodiscard]] Preferences&           preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] SimulationExportPanel& panel() const noexcept { return *m_panel; }
    [[nodiscard]] Simulation& simulation() const noexcept { return *m_panel->simulation(); }

private:
    Gui::Test::TestGuiServices m_services;
    ExampleDesign              m_design;
    QWidget                    m_holder;
    SimulationExportPanel*     m_panel{nullptr};
};

/// The answer of the file question: @p name in @p directory, the CSV filter chosen.
[[nodiscard]] UserPrompts::SaveFileAnswer saveAnswer(const QtRocket::Test::TempDir& directory,
                                                     std::string_view               name)
{
    return {.file               = directory.resolve(std::string(name)),
            .directory          = directory.path(),
            .saveSimulationData = std::nullopt,
            .selectedNameFilter = FileNames::csvFilter(),
            .csvOptions         = std::nullopt};
}

/// The text of the file @p file; empty, with a test failure, when it cannot be read.
[[nodiscard]] std::string fileText(const std::filesystem::path& file)
{
    const Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    return bytesToString(*bytes);
}

/// What the core's CsvExport writes of @p branch of @p simulation for @p fields in @p units with
/// @p options; empty, with a test failure, when it fails.
[[nodiscard]] std::string coreExport(Simulation& simulation, const FlightDataBranch& branch,
                                     const std::vector<const FlightDataType*>& fields,
                                     const std::vector<const Unit*>&           units,
                                     const CsvExport::Options&                 options)
{
    std::ostringstream out;
    const Result<void> written =
        CsvExport::exportCsv(out, simulation, branch, fields, units, options);
    if (!written.has_value())
    {
        ADD_FAILURE() << written.error().toString();
        return {};
    }
    return out.str();
}

/// The types of the rows of @p panel that are ticked, in order.
[[nodiscard]] std::vector<const FlightDataType*> tickedTypes(const SimulationExportPanel& panel)
{
    std::vector<const FlightDataType*> types;
    const std::vector<bool>            selected = panel.exportPanel().selected();
    for (std::size_t row = 0; row < panel.types().size(); ++row)
    {
        if (selected.at(row))
        {
            types.push_back(panel.types().at(row));
        }
    }
    return types;
}

/// The units of the rows of @p panel that are ticked, in order.
[[nodiscard]] std::vector<const Unit*> tickedUnits(const SimulationExportPanel& panel)
{
    std::vector<const Unit*>       units;
    const std::vector<bool>        selected = panel.exportPanel().selected();
    const std::vector<const Unit*> all      = panel.exportPanel().units();
    for (std::size_t row = 0; row < all.size(); ++row)
    {
        if (selected.at(row))
        {
            units.push_back(all.at(row));
        }
    }
    return units;
}

/// The row of @p panel whose type is the built-in @p id; a test failure, and 0, when none is.
[[nodiscard]] std::size_t rowOf(const SimulationExportPanel& panel, TypeId id)
{
    const FlightDataType& type = FlightDataType::builtin(id);
    for (std::size_t row = 0; row < panel.types().size(); ++row)
    {
        if (panel.types().at(row)->equals(type))
        {
            return row;
        }
    }
    ADD_FAILURE() << "no row of " << type.getName();
    return 0;
}

/// Ticks the row of the built-in @p id in @p panel.
void tick(const SimulationExportPanel& panel, TypeId id)
{
    panel.exportPanel().setSelected(rowOf(panel, id), true);
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

/// A branch @p name of the built-in types @p ids, with @p rows rows whose values are the row's
/// index times @p scale.
[[nodiscard]] std::shared_ptr<FlightDataBranch> handMadeBranch(const std::string&         name,
                                                               const std::vector<TypeId>& ids,
                                                               int rows, double scale)
{
    std::vector<const FlightDataType*> types;
    types.reserve(ids.size());
    for (const TypeId id : ids)
    {
        types.push_back(&FlightDataType::builtin(id));
    }
    auto branch = std::make_shared<FlightDataBranch>(name, types);
    for (int row = 0; row < rows; ++row)
    {
        branch->addPoint();
        for (const FlightDataType* const type : types)
        {
            branch->setValue(*type, row * scale);
        }
    }
    return branch;
}

/// A simulation of @p document's rocket, named "Hand made", loaded from a file with @p data.
[[nodiscard]] std::shared_ptr<Simulation> handMadeSimulation(OpenRocketDocument&         document,
                                                             std::shared_ptr<FlightData> data)
{
    return std::make_shared<Simulation>(
        nullptr, document.getRocket(), Status::LOADED, "Hand made", SimulationOptions(),
        std::vector<std::shared_ptr<SimulationExtension>>{}, std::move(data));
}

/// Flight data of a sustainer (time, altitude and vertical velocity, 3 rows) and a booster (time
/// and vertical velocity only, 2 rows), each value scaled by @p scale.
[[nodiscard]] std::shared_ptr<FlightData> twoStageData(double scale)
{
    return std::make_shared<FlightData>(std::initializer_list<std::shared_ptr<FlightDataBranch>>{
        handMadeBranch("Sustainer",
                       {TypeId::TYPE_TIME, TypeId::TYPE_ALTITUDE, TypeId::TYPE_VELOCITY_Z}, 3,
                       scale),
        handMadeBranch("Booster", {TypeId::TYPE_TIME, TypeId::TYPE_VELOCITY_Z}, 2, scale)});
}

/// Makes the tab of @p simulation with @p services, and lets it go.
void makeTab(const GuiServices& services, std::shared_ptr<Simulation> simulation)
{
    const SimulationExportPanel tab(services, std::move(simulation));
    static_cast<void>(tab);
}

/// A variable of the walk of OpenRocket's export tab: its name and its unit.
using WalkRow = std::pair<std::string_view, std::string_view>;

/// The rows of OpenRocket's export tab of "A simple model rocket" Simulation 1, in order (the
/// scout's walk, lines 172-345; "\u200B", the zero-width space, is the unit of a dimensionless
/// variable).
constexpr std::array<WalkRow, 58> kWalkRows{{
    {"Time", "s"},
    {"Altitude", "m"},
    {"Altitude above sea level", "m"},
    {"Vertical velocity", "m/s"},
    {"Total velocity", "m/s"},
    {"Vertical acceleration", "m/s\u00B2"},
    {"Total acceleration", "m/s\u00B2"},
    {"Position East of launch", "m"},
    {"Position North of launch", "m"},
    {"Lateral distance", "m"},
    {"Lateral direction", "\u00B0"},
    {"Lateral velocity", "m/s"},
    {"Lateral acceleration", "m/s\u00B2"},
    {"Latitude", "\u00B0 N"},
    {"Longitude", "\u00B0 E"},
    {"Angle of attack", "\u00B0"},
    {"Roll rate (Z)", "\u00B0/s"},
    {"Pitch rate (Y)", "\u00B0/s"},
    {"Yaw rate (X)", "\u00B0/s"},
    {"Vertical orientation (zenith)", "\u00B0"},
    {"Lateral orientation (azimuth)", "\u00B0"},
    {"Mass", "g"},
    {"Motor mass", "g"},
    {"Longitudinal moment of inertia", "kg\u00B7m\u00B2"},
    {"Rotational moment of inertia", "kg\u00B7m\u00B2"},
    {"Gravitational acceleration", "m/s\u00B2"},
    {"CP location", "cm"},
    {"CG location", "cm"},
    {"Stability margin calibers", "\u200B"},
    {"Thrust", "N"},
    {"Thrust-to-weight ratio", "\u200B"},
    {"Drag force", "N"},
    {"Drag coefficient (CD)", "\u200B"},
    {"Friction drag coefficient (CD_friction)", "\u200B"},
    {"Pressure drag coefficient (CD_pressure)", "\u200B"},
    {"Base drag coefficient (CD_base)", "\u200B"},
    {"Axial drag coefficient (CA)", "\u200B"},
    {"Normal force coefficient (CN)", "\u200B"},
    {"Pitch moment coefficient (Cm)", "\u200B"},
    {"Yaw moment coefficient", "\u200B"},
    {"Side force coefficient", "\u200B"},
    {"Roll moment coefficient", "\u200B"},
    {"Roll forcing coefficient", "\u200B"},
    {"Roll damping coefficient", "\u200B"},
    {"Pitch damping coefficient", "\u200B"},
    {"Wind velocity", "m/s"},
    {"Wind direction", "\u00B0"},
    {"Air temperature", "\u00B0C"},
    {"Air pressure", "mbar"},
    {"Air density", "g/cm\u00B3"},
    {"Speed of sound", "m/s"},
    {"Mach number", "\u200B"},
    {"Reynolds number", "\u200B"},
    {"Reference length", "cm"},
    {"Reference area", "cm\u00B2"},
    {"Simulation time step", "s"},
    {"Computation time", "s"},
    {"Coriolis acceleration", "m/s\u00B2"},
}};

/// The walk of OpenRocket's export tab (the scout's java/dialog-walk.txt, lines 169-363, without
/// the tab's indent), as QtRocket's walk writes the same: the headers are bold labels (Java's
/// "<html><b>Export</b></html>"), and the scroll bar's two arrow buttons (Java's two "BUTTON "
/// lines before "Select all") are no widgets of a QScrollArea.
[[nodiscard]] std::vector<std::string> openRocketsWalk()
{
    const std::string separatorTip =
        std::string("tip=<html>The string used to separate the fields in the exported file. ") +
        "<br>Use ',' for a Comma Separated Values (CSV) file.";
    const std::string decimalsTip =
        "tip=The number of decimal places to use for the variables in the exported file.";
    const std::string exponentialTip =
        std::string("tip=Whether to use exponential notation for the variables (e.g. 1.5e-4) or ") +
        "not (e.g. 0.00015).";
    const std::string simulationComments =
        std::string("  CHECK Include simulation description selected=true tip=Include a comment ") +
        "at the beginning of the file describing the simulation.";
    const std::string fieldComments =
        std::string(
            "  CHECK Include field descriptions selected=true tip=Include a comment line ") +
        "with the descriptions of the exported variables.";
    const std::string eventComments =
        std::string("  CHECK Include flight events selected=true tip=Include a comment line for ") +
        "every flight event.";
    std::vector<std::string> lines{"LABEL Export", "LABEL Variable", "LABEL Unit"};
    for (const WalkRow& row : kWalkRows)
    {
        lines.emplace_back("CHECK  selected=false");
        lines.push_back("LABEL " + std::string(row.first));
        lines.push_back("LABEL " + std::string(row.second));
    }
    const std::vector<std::string> rest{
        "BUTTON Select all",
        "BUTTON Select none",
        "LABEL Exporting 0 variables out of 58.",
        "BORDER Format settings",
        "  LABEL Field separator string: " + separatorTip,
        "  COMBO selected=, items=,|;|SPACE|TAB editable " + separatorTip,
        "  LABEL Decimal places: " + decimalsTip,
        "  SPINNER 3 " + decimalsTip,
        "  CHECK Use exponential notation selected=true " + exponentialTip,
        "BORDER Comments",
        simulationComments,
        fieldComments,
        eventComments,
        "  LABEL Comment character: tip=The character(s) that mark a comment line.",
        "  COMBO selected=# items=#|%|; editable tip=The character(s) that mark a comment line."};
    lines.insert(lines.end(), rest.begin(), rest.end());
    return lines;
}

/// The question in which ClosingPrompts closes the window.
enum class ClosingQuestion
{
    FILE,
    OVERWRITE,
};

/// The test's prompts, which close the window (destroy the tab's holder) when @p question is
/// asked, before the answer of the script.
class ClosingPrompts final : public Gui::Test::ForwardingUserPrompts
{
public:
    ClosingPrompts(UserPrompts& delegate, ClosingQuestion question,
                   std::unique_ptr<QWidget>& window)
      : ForwardingUserPrompts(delegate), m_question(question), m_window(&window)
    {
    }

    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget*               parent,
                                                            const SaveFileRequest& request) override
    {
        if (m_question == ClosingQuestion::FILE)
        {
            m_window->reset();
        }
        return delegate().askSaveFile(parent, request);
    }

    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override
    {
        if (m_question == ClosingQuestion::OVERWRITE)
        {
            m_window->reset();
        }
        return delegate().askOverwrite(parent, fileName);
    }

private:
    ClosingQuestion           m_question;
    std::unique_ptr<QWidget>* m_window;
};

/// A tab whose window goes during @p question: the export ends there, and nothing is stored or
/// written.
void expectNothingAfterThePanelWent(ClosingQuestion question)
{
    SCOPED_TRACE(question == ClosingQuestion::FILE ? "the file question"
                                                   : "the overwrite question");
    Gui::Test::TestGuiServices    services;
    const ExampleDesign           design(ExampleDesign::kSimpleModelRocket);
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   existing = directory.write("flight.csv", "old");
    auto                          window   = std::make_unique<QWidget>();
    ClosingPrompts                prompts(services.prompts(), question, window);
    const GuiServices             closing = services.services().withPrompts(prompts);
    auto* const                   panel =
        makeChild<SimulationExportPanel>(window.get(), closing, design.simulation(0));
    panel->exportPanel().selectAll();
    services.prompts().answerSaveFile(saveAnswer(directory, "flight"));
    if (question == ClosingQuestion::OVERWRITE)
    {
        services.prompts().answerOverwrite(true);
    }
    EXPECT_FALSE(panel->doExport());
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(fileText(existing), "old");
    EXPECT_EQ(services.preferences().findNode(GuiPreferenceKeys::kExportsNode), nullptr);
}

// ------------------------------------------------------------------------------- the tab

TEST_F(SimulationExportPanelTest, IsOpenRocketsExportTab)
{
    // Nothing stored: every row unticked, the options' defaults.
    const TabUnderTest tab;
    Gui::Test::expectSameLines(Gui::Test::dialogWalk(tab.panel()), openRocketsWalk());
    EXPECT_EQ(tab.panel().objectName(), QStringLiteral("exportTab"));
    EXPECT_EQ(tab.panel().exportPanel().objectName(), QStringLiteral("csvExportPanel"));
    EXPECT_EQ(&tab.panel().exportPanel().csvOptions(), &tab.panel().csvOptions());
    EXPECT_EQ(tab.panel().csvOptions().baseClassName(), "SimulationExportPanel");
    // One stage: no stage box.
    EXPECT_EQ(tab.panel().stagePanel(), nullptr);
    EXPECT_EQ(tab.panel().stageSelection(), nullptr);
    EXPECT_EQ(tab.panel().stageIndex(), 0U);
}

TEST_F(SimulationExportPanelTest, TheRowsAreTheFirstBranchsTypesTickedAsStored)
{
    TabUnderTest tab;
    // SwingPreferences.setFlightDataTypeExportSelected(): the node "exports".
    CsvOptionPreferences::setFlightDataTypeExportSelected(tab.preferences(), "Altitude", true);
    CsvOptionPreferences::setFlightDataTypeExportSelected(tab.preferences(), "Mass", true);
    CsvOptionPreferences::setFlightDataTypeExportSelected(tab.preferences(), "Time", false);
    const SimulationExportPanel panel(tab.services().services(), tab.panel().simulation());
    EXPECT_EQ(panel.types(), tab.simulation().getSimulatedData()->getBranch(0).getTypes());
    const CsvExportPanel& rows = panel.exportPanel();
    ASSERT_EQ(rows.rowCount(), 58U);
    EXPECT_EQ(rows.selectedCount(), 2U);
    EXPECT_TRUE(rows.isSelected(rowOf(panel, TypeId::TYPE_ALTITUDE)));
    EXPECT_TRUE(rows.isSelected(rowOf(panel, TypeId::TYPE_MASS)));
    EXPECT_FALSE(rows.isSelected(rowOf(panel, TypeId::TYPE_TIME)));
    // Named as the plots name them, each in its group's default unit.
    EXPECT_EQ(rows.name(1), flightDataTypeText(*panel.types().at(1)));
    EXPECT_EQ(&rows.unit(1), &panel.types().at(1)->getUnitGroup().getDefaultUnit());
    EXPECT_EQ(rows.selectedCountLabel()->text(),
              QStringLiteral("Exporting 2 variables out of 58."));
}

TEST_F(SimulationExportPanelTest, TheOptionsAreThePreferencesOfTheSimulationExportPanel)
{
    TabUnderTest tab;
    tab.preferences().putString(PreferenceKeys::kExportFieldSeparator, std::string_view(";"));
    tab.preferences().putInt(PreferenceKeys::kExportDecimalPlaces, 6);
    tab.preferences().putBoolean(PreferenceKeys::kExportExponentialNotation, false);
    tab.preferences().putString(PreferenceKeys::kExportCommentCharacter, std::string_view("%"));
    tab.preferences().putBoolean(GuiPreferenceKeys::csvOptionKey("SimulationExportPanel", 1),
                                 false);
    const SimulationExportPanel panel(tab.services().services(), tab.panel().simulation());
    EXPECT_EQ(panel.csvOptions().values(),
              (CsvOptionValues{.fieldSeparator      = QStringLiteral(";"),
                               .decimalPlaces       = 6,
                               .exponentialNotation = false,
                               .commentCharacter    = QStringLiteral("%"),
                               .commentOptions      = {true, false, true}}));

    // The three comment options, in Java's order (OPTION_SIMULATION_COMMENTS, ...).
    const std::vector<CsvOptionPanel::CommentOption> options =
        SimulationExportPanel::commentOptions();
    ASSERT_EQ(options.size(), CsvOptionPreferences::kSimulationExportOptionCount);
    EXPECT_EQ(options[CsvOptionPreferences::kSimulationCommentsOption].text,
              QStringLiteral("Include simulation description"));
    EXPECT_EQ(options[CsvOptionPreferences::kFieldDescriptionsOption].text,
              QStringLiteral("Include field descriptions"));
    EXPECT_EQ(options[CsvOptionPreferences::kFlightEventsOption].toolTip,
              QStringLiteral("Include a comment line for every flight event."));
}

TEST_F(SimulationExportPanelTest, TheStageBoxListsTheBranchesOfAFlightOfSeveralStages)
{
    const TabUnderTest           tab(ExampleDesign::kThreeStageLowPowerRocket);
    const SimulationExportPanel& panel  = tab.panel();
    const QGroupBox* const       stages = panel.stagePanel();
    ASSERT_NE(stages, nullptr);
    QComboBox* const box = panel.stageSelection();
    ASSERT_NE(box, nullptr);
    // Util.generateSeriesLabels(): the scout's probe, "Booster stage(1)" without a space.
    EXPECT_EQ(stages->title(), QStringLiteral("Stage to export"));
    EXPECT_EQ(stages->objectName(), QStringLiteral("stagePanel"));
    EXPECT_EQ(box->objectName(), QStringLiteral("stageSelection"));
    EXPECT_FALSE(box->isEditable());
    EXPECT_EQ(box->count(), 3);
    EXPECT_EQ(box->itemText(0), QStringLiteral("Sustainer"));
    EXPECT_EQ(box->itemText(1), QStringLiteral("Booster stage(1)"));
    EXPECT_EQ(box->itemText(2), QStringLiteral("Booster stage(2)"));
    EXPECT_EQ(box->currentIndex(), 0);
    EXPECT_EQ(panel.stageIndex(), 0U);
    // Below the options, in the export panel (Java's extra components).
    EXPECT_TRUE(panel.exportPanel().isAncestorOf(stages));
    Gui::Test::chooseItem(*box, 2);
    EXPECT_EQ(panel.stageIndex(), 2U);
    // The rows stay the first stage's.
    EXPECT_EQ(panel.types(), panel.simulation()->getSimulatedData()->getBranch(0).getTypes());
}

TEST_F(SimulationExportPanelTest, AnEmptyOrMissingFlightIsABug)
{
    const Gui::Test::TestGuiServices          services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    EXPECT_THROW(makeTab(services.services(), nullptr), BugError);
    EXPECT_THROW(makeTab(services.services(),
                         std::make_shared<Simulation>(document.get(), document->getRocket())),
                 BugError);
    EXPECT_THROW(
        makeTab(services.services(), handMadeSimulation(*document, std::make_shared<FlightData>())),
        BugError);
}

// ---------------------------------------------------------------------------- the questions

TEST_F(SimulationExportPanelTest, AsksForACsvFileInTheDefaultDirectory)
{
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tab.preferences().setDefaultDirectory(directory.path());
    // SaveFileChooser: no title of its own, the CSV filter chosen beside All Files, no name.
    const UserPrompts::SaveFileRequest request =
        SimulationExportPanel::exportRequest(tab.preferences());
    EXPECT_TRUE(request.title.isEmpty());
    EXPECT_EQ(request.nameFilters, (QStringList{QStringLiteral("Comma Separated Files (*.csv)"),
                                                QStringLiteral("All Files (*)")}));
    EXPECT_EQ(request.directory, directory.path());
    EXPECT_TRUE(request.fileName.isEmpty());
    EXPECT_TRUE(request.refuseIllegalNames);
    EXPECT_FALSE(request.saveSimulationData.has_value());
    EXPECT_FALSE(request.csvOptions.has_value());
    const Gui::Test::TestGuiServices other;
    EXPECT_TRUE(
        SimulationExportPanel::exportRequest(other.services().preferences()).directory.empty());

    // doExport() asks it.
    tab.prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(tab.panel().doExport());
    ASSERT_EQ(tab.prompts().saveFileRequests().size(), 1U);
    EXPECT_EQ(tab.prompts().saveFileRequests().front().nameFilters, request.nameFilters);
    EXPECT_EQ(tab.prompts().saveFileRequests().front().directory, directory.path());
    EXPECT_TRUE(tab.prompts().saveFileRequests().front().refuseIllegalNames);
}

TEST_F(SimulationExportPanelTest, ACancelledFileQuestionWritesAndStoresNothing)
{
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tick(tab.panel(), TypeId::TYPE_TIME);
    tab.panel().csvOptions().setValues(
        {.fieldSeparator = QStringLiteral(";"), .commentOptions = {true, true, true}});
    tab.prompts().answerSaveFile(std::nullopt);
    EXPECT_FALSE(tab.panel().doExport());
    EXPECT_TRUE(std::filesystem::is_empty(directory.path()));
    EXPECT_EQ(tab.preferences().findNode(GuiPreferenceKeys::kExportsNode), nullptr);
    EXPECT_FALSE(tab.preferences().get(PreferenceKeys::kExportFieldSeparator).has_value());
    EXPECT_FALSE(tab.preferences().getDefaultDirectory().has_value());
    EXPECT_EQ(promptKinds(tab.prompts()), std::vector<std::string>{"SAVE_FILE"});
}

TEST_F(SimulationExportPanelTest, AddsTheCsvExtensionUnlessTheNameEndsInIt)
{
    // FileHelper.forceExtension(file, "csv").
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tab.prompts().answerSaveFile(saveAnswer(directory, "flight"));
    EXPECT_TRUE(tab.panel().doExport());
    tab.prompts().answerSaveFile(saveAnswer(directory, "data.CSV"));
    EXPECT_TRUE(tab.panel().doExport());
    tab.prompts().answerSaveFile(saveAnswer(directory, "a.txt"));
    EXPECT_TRUE(tab.panel().doExport());
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("flight.csv")));
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("data.CSV")));
    EXPECT_TRUE(std::filesystem::is_regular_file(directory.resolve("a.txt.csv")));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("flight")));
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("a.txt")));
    // No file existed: no overwrite question.
    EXPECT_EQ(promptKinds(tab.prompts()),
              (std::vector<std::string>{"SAVE_FILE", "SAVE_FILE", "SAVE_FILE"}));
}

TEST_F(SimulationExportPanelTest, AnExistingFileIsReplacedOnlyAfterYes)
{
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    const std::filesystem::path   existing = directory.write("old.csv", "old");
    tick(tab.panel(), TypeId::TYPE_TIME);

    // FileHelper.confirmWrite(): No writes and stores nothing.
    tab.prompts().answerSaveFile(saveAnswer(directory, "old"));
    tab.prompts().answerOverwrite(false);
    EXPECT_FALSE(tab.panel().doExport());
    EXPECT_EQ(fileText(existing), "old");
    EXPECT_EQ(tab.preferences().findNode(GuiPreferenceKeys::kExportsNode), nullptr);
    EXPECT_FALSE(tab.preferences().getDefaultDirectory().has_value());

    tab.prompts().answerSaveFile(saveAnswer(directory, "old.csv"));
    tab.prompts().answerOverwrite(true);
    EXPECT_TRUE(tab.panel().doExport());
    EXPECT_NE(fileText(existing), "old");
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& asked = tab.prompts().prompts();
    ASSERT_EQ(asked.size(), 4U);
    EXPECT_EQ(asked[1].kind, PromptKind::OVERWRITE);
    EXPECT_EQ(asked[1].texts, QStringList{QStringLiteral("old.csv")});
    EXPECT_EQ(asked[3].kind, PromptKind::OVERWRITE);
}

// ------------------------------------------------------------------------------ the export

TEST_F(SimulationExportPanelTest, StoresTheOptionsEveryRowsTickAndTheDirectory)
{
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    SimulationExportPanel&        panel = tab.panel();
    tick(panel, TypeId::TYPE_ALTITUDE);
    tick(panel, TypeId::TYPE_MASS);
    const CsvOptionValues values{.fieldSeparator      = QStringLiteral("|"),
                                 .decimalPlaces       = 2,
                                 .exponentialNotation = false,
                                 .commentCharacter    = QStringLiteral("!!"),
                                 .commentOptions      = {true, false, true}};
    panel.csvOptions().setValues(values);
    tab.prompts().answerSaveFile(saveAnswer(directory, "flight"));
    ASSERT_TRUE(panel.doExport());

    // csvOptions.storePreferences(), setDefaultDirectory() and every row's tick, ticked or not.
    EXPECT_EQ(CsvOptionPreferences::load(tab.preferences(), "SimulationExportPanel", 3), values);
    EXPECT_EQ(tab.preferences().getString(PreferenceKeys::kExportFieldSeparator, ""), "|");
    EXPECT_EQ(tab.preferences().getDefaultDirectory(), std::optional(directory.path()));
    const Preferences* const exports = tab.preferences().findNode(GuiPreferenceKeys::kExportsNode);
    ASSERT_NE(exports, nullptr);
    EXPECT_EQ(exports->keys().size(), 58U);
    EXPECT_EQ(exports->get("Altitude"), std::optional<std::string>("true"));
    EXPECT_EQ(exports->get("Mass"), std::optional<std::string>("true"));
    EXPECT_EQ(exports->get("Time"), std::optional<std::string>("false"));

    // The next tab opens with them.
    const SimulationExportPanel next(tab.services().services(), panel.simulation());
    EXPECT_EQ(next.exportPanel().selected(), panel.exportPanel().selected());
    EXPECT_EQ(next.csvOptions().values(), values);
}

TEST_F(SimulationExportPanelTest, WritesTheTickedVariablesInTheirUnitsWithTheOptions)
{
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    SimulationExportPanel&        panel = tab.panel();
    tick(panel, TypeId::TYPE_TIME);
    tick(panel, TypeId::TYPE_ALTITUDE);
    tick(panel, TypeId::TYPE_VELOCITY_Z);
    tick(panel, TypeId::TYPE_MASS);
    tick(panel, TypeId::TYPE_STABILITY);
    // The altitude in its group's last unit, the mass in its first.
    const UnitGroup& distance = FlightDataType::builtin(TypeId::TYPE_ALTITUDE).getUnitGroup();
    panel.exportPanel().setSelectedUnit(rowOf(panel, TypeId::TYPE_ALTITUDE),
                                        distance.getUnit(distance.getUnitCount() - 1));
    const UnitGroup& mass = FlightDataType::builtin(TypeId::TYPE_MASS).getUnitGroup();
    panel.exportPanel().setSelectedUnit(rowOf(panel, TypeId::TYPE_MASS), mass.getUnit(0));
    panel.csvOptions().setValues({.fieldSeparator      = QStringLiteral("\t"),
                                  .decimalPlaces       = 6,
                                  .exponentialNotation = false,
                                  .commentCharacter    = QStringLiteral("%"),
                                  .commentOptions      = {true, true, false}});
    tab.prompts().answerSaveFile(saveAnswer(directory, "flight"));
    ASSERT_TRUE(panel.doExport());

    const std::vector<const FlightDataType*> fields = tickedTypes(panel);
    ASSERT_EQ(fields.size(), 5U);
    // The rows' order, not the order of the ticks.
    EXPECT_EQ(fields.front()->getName(), "Time");
    const std::string expected =
        coreExport(tab.simulation(), tab.simulation().getSimulatedData()->getBranch(0), fields,
                   tickedUnits(panel),
                   {.fieldSeparator      = "\t",
                    .decimalPlaces       = 6,
                    .exponentialNotation = false,
                    .commentStarter      = "%",
                    .simulationComments  = true,
                    .fieldComments       = true,
                    .eventComments       = false});
    EXPECT_EQ(fileText(directory.resolve("flight.csv")), expected);
    // Stored data round-trips exactly: the comments of the 233 points of the stored flight.
    EXPECT_TRUE(
        expected.starts_with("% Simulation 1 (Data loaded from a file)\n"
                             "% 233 data points written for 5 variables.\n"))
        << expected.substr(0, 200);
    // Only the file question was asked, and nothing was shown.
    EXPECT_EQ(promptKinds(tab.prompts()), std::vector<std::string>{"SAVE_FILE"});
}

TEST_F(SimulationExportPanelTest, WritesTheStageChosen)
{
    TabUnderTest                  tab(ExampleDesign::kThreeStageLowPowerRocket);
    const QtRocket::Test::TempDir directory;
    SimulationExportPanel&        panel = tab.panel();
    panel.exportPanel().selectAll();
    Gui::Test::chooseItem(*panel.stageSelection(), 2);
    tab.prompts().answerSaveFile(saveAnswer(directory, "booster"));
    ASSERT_TRUE(panel.doExport());
    const FlightData& data = *tab.simulation().getSimulatedData();
    EXPECT_EQ(
        fileText(directory.resolve("booster.csv")),
        coreExport(tab.simulation(), data.getBranch(2), panel.types(), tickedUnits(panel), {}));
}

TEST_F(SimulationExportPanelTest, NoVariableTickedStillWritesAFile)
{
    // Java exports zero variables: the comments, an empty field line and an empty line per row.
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tab.prompts().answerSaveFile(saveAnswer(directory, "nothing"));
    ASSERT_TRUE(tab.panel().doExport());
    EXPECT_EQ(fileText(directory.resolve("nothing.csv")),
              coreExport(tab.simulation(), tab.simulation().getSimulatedData()->getBranch(0), {},
                         {}, {}));
}

TEST_F(SimulationExportPanelTest, WritesTheFlightDataAsItIsAtTheExport)
{
    // OpenRocket bug 49: Java's panel keeps the branch of the data it was made with, and writes
    // it after a run replaced the data. Here the data is fetched at the export, and the panel
    // keeps nothing of the old data alive.
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const std::shared_ptr<Simulation> simulation = handMadeSimulation(*document, twoStageData(1.0));
    QWidget                           holder;
    auto* const panel = makeChild<SimulationExportPanel>(&holder, services.services(), simulation);
    const QtRocket::Test::TempDir directory;
    panel->exportPanel().selectAll();
    const std::weak_ptr<FlightData> old = simulation->getSimulatedData();

    // A run's new data (Simulation::loadFrom() shares it, as a run's adoption does).
    simulation->loadFrom(*handMadeSimulation(*document, twoStageData(10.0)));
    EXPECT_TRUE(old.expired());
    services.prompts().answerSaveFile(saveAnswer(directory, "new"));
    ASSERT_TRUE(panel->doExport());
    const std::string written = fileText(directory.resolve("new.csv"));
    EXPECT_EQ(written, coreExport(*simulation, simulation->getSimulatedData()->getBranch(0),
                                  panel->types(), tickedUnits(*panel), {}));
    // The rows of the new data: 10 m and 20 m, not 1 m and 2 m.
    EXPECT_NE(written.find("\n0,0,0\n10,10,10\n20,20,20\n"), std::string::npos) << written;
}

TEST_F(SimulationExportPanelTest, AStageTheNewDataLacksWritesTheFirstStage)
{
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const std::shared_ptr<Simulation> simulation = handMadeSimulation(*document, twoStageData(1.0));
    QWidget                           holder;
    auto* const panel = makeChild<SimulationExportPanel>(&holder, services.services(), simulation);
    const QtRocket::Test::TempDir directory;
    tick(*panel, TypeId::TYPE_TIME);
    Gui::Test::chooseItem(*panel->stageSelection(), 1);

    // New data of one stage only.
    simulation->loadFrom(*handMadeSimulation(
        *document, std::make_shared<FlightData>(std::initializer_list{handMadeBranch(
                       "Sustainer", {TypeId::TYPE_TIME, TypeId::TYPE_ALTITUDE}, 4, 2.0)})));
    services.prompts().answerSaveFile(saveAnswer(directory, "first"));
    ASSERT_TRUE(panel->doExport());
    EXPECT_EQ(fileText(directory.resolve("first.csv")),
              coreExport(*simulation, simulation->getSimulatedData()->getBranch(0),
                         tickedTypes(*panel), tickedUnits(*panel), {}));
}

TEST_F(SimulationExportPanelTest, WithoutFlightDataAnyMoreItAsksNothing)
{
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const std::shared_ptr<Simulation> simulation = handMadeSimulation(*document, twoStageData(1.0));
    QWidget                           holder;
    auto* const panel = makeChild<SimulationExportPanel>(&holder, services.services(), simulation);
    // A run that left no data (a refused input does: SimulationRun).
    simulation->loadFrom(*simulation->copy());
    ASSERT_FALSE(simulation->hasSimulationData());
    EXPECT_FALSE(panel->doExport());
    EXPECT_TRUE(services.prompts().prompts().empty());
}

// ------------------------------------------------------------------------------ the errors

TEST_F(SimulationExportPanelTest, AFileThatCannotBeOpenedShowsErrorWritingFile)
{
    // OpenRocket bug 53: Java's bug report dialog.
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tick(tab.panel(), TypeId::TYPE_TIME);
    UserPrompts::SaveFileAnswer answer = saveAnswer(directory, "flight");
    answer.file                        = directory.resolve("missing") / "flight";
    tab.prompts().answerSaveFile(answer);
    EXPECT_FALSE(tab.panel().doExport());

    const std::filesystem::path file = directory.resolve("missing") / "flight.csv";
    EXPECT_FALSE(std::filesystem::exists(file));
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& shown = tab.prompts().prompts();
    ASSERT_EQ(shown.size(), 2U);
    EXPECT_EQ(shown[1].kind, PromptKind::MESSAGE);
    EXPECT_EQ(shown[1].title, QStringLiteral("Error writing file"));
    ASSERT_EQ(shown[1].texts.size(), 3);
    EXPECT_EQ(shown[1].texts[0], QStringLiteral("ERROR"));
    EXPECT_EQ(shown[1].texts[1], QStringLiteral("An error occurred while writing to the file:"));
    EXPECT_TRUE(shown[1].texts[2].contains(QStringLiteral("flight.csv")))
        << shown[1].texts[2].toStdString();
    // The preferences were stored before the file was written, as Java stores them.
    EXPECT_EQ(tab.preferences().getDefaultDirectory(), std::optional(directory.path()));
}

TEST_F(SimulationExportPanelTest, AFailedWriteShowsErrorWritingFile)
{
    // OpenRocket bug 53: Java's PrintWriter swallows a failed write, and its export says
    // nothing. A file that is a link to /dev/full opens, and every write to it fails.
#ifdef __linux__
    std::error_code error;
    if (!std::filesystem::exists("/dev/full", error))
    {
        GTEST_SKIP() << "no /dev/full";
    }
    TabUnderTest                  tab;
    const QtRocket::Test::TempDir directory;
    tab.panel().exportPanel().selectAll();
    std::filesystem::create_symlink("/dev/full", directory.resolve("full.csv"));
    tab.prompts().answerSaveFile(saveAnswer(directory, "full.csv"));
    tab.prompts().answerOverwrite(true);
    EXPECT_FALSE(tab.panel().doExport());
    EXPECT_EQ(promptKinds(tab.prompts()),
              (std::vector<std::string>{"SAVE_FILE", "OVERWRITE", "MESSAGE"}));
    EXPECT_EQ(tab.prompts().prompts().back().title, QStringLiteral("Error writing file"));
#else
    GTEST_SKIP() << "/dev/full is Linux's";
#endif
}

TEST_F(SimulationExportPanelTest, AVariableTheStageLacksIsReportedBeforeTheFileIsOpened)
{
    // Java's export fails with a NullPointerException in its worker.
    Gui::Test::TestGuiServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const std::shared_ptr<Simulation> simulation = handMadeSimulation(*document, twoStageData(1.0));
    QWidget                           holder;
    auto* const panel = makeChild<SimulationExportPanel>(&holder, services.services(), simulation);
    const QtRocket::Test::TempDir directory;
    tick(*panel, TypeId::TYPE_ALTITUDE);
    Gui::Test::chooseItem(*panel->stageSelection(), 1);
    services.prompts().answerSaveFile(saveAnswer(directory, "booster"));
    EXPECT_FALSE(panel->doExport());
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("booster.csv")));
    const Gui::Test::ScriptedUserPrompts::Prompt& shown = services.prompts().prompts().back();
    EXPECT_EQ(shown.kind, PromptKind::MESSAGE);
    EXPECT_EQ(shown.title, QStringLiteral("Error writing file"));
    EXPECT_EQ(shown.texts.back(),
              QStringLiteral("The flight data branch 'Booster' has no data of type 'Altitude'."));
}

TEST_F(SimulationExportPanelTest, ThePanelGoingDuringAQuestionEndsTheExport)
{
    // A question runs an event loop, in which the settings dialog may go with its window.
    expectNothingAfterThePanelWent(ClosingQuestion::FILE);
    expectNothingAfterThePanelWent(ClosingQuestion::OVERWRITE);
}

}  // namespace

}  // namespace QtRocket::Gui
