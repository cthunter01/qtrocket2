// The reference of the GUI's scenario of the Flight simulations tab (schema version 3:
// tests/data/goldens/gui/simulations/, kGuiSimulationsScenario), written by
// tools/openrocket-gui-reference: OpenRocket's real SimulationPanel, its settings dialog with the
// wind editor, the CSV import of wind levels and the aerodynamic lookup dialog, the run dialog
// (cancelled before any simulation starts) and the confirmations run scenario.json on a virtual
// display after OpenRocket's start-up with the bundled motors and component presets, and save
// the design; see GoldenGuiReference.h. The GUI's exit test SimulationsExitGolden drives the same
// scenario through QtRocket's GUI and compares with the reference; this file checks, without a
// GUI, that the reference stays usable, as flight_configurations_reference_golden_tests.cpp does
// for version 2:
//
// - the scenario is well formed, opens the flight-configurations reference's save and names
//   files of the repository, and state.json is the record of exactly this scenario and this input
//   (the SHA-256 of their bytes, a record per step of the same kind), made with the OpenRocket and
//   the preset database of the other goldens, after a start-up that loaded QtRocket's bundled
//   databases; every record's tree digest is the digest of its tree, and no step of the
//   simulations tab changes the tree;
// - every step of the tab changes what it is for (the selection, the simulations, the table's
//   columns), a cancelled one nothing, and every question of a step is one its answer answers, in
//   the order they open, the file choosers with the answers' files;
// - QtRocket loads the input into the tree and the simulations the open step recorded, and the
//   saved design (rocket.ork, the document of the archive whose entries state.json lists with
//   their SHA-256) without warnings into the state OpenRocket's own load gives (designFileState(),
//   line for line: its simulations' options, conditions, wind levels and extensions), with
//   OpenRocket's masses, centres of mass and length and the last record's simulations;
// - the records of the tool's fixes of OpenRocket's bugs 6, 32, 33 and 34 are those
//   kGuiSimulationToolFixes lists, and the expected records QtRocket's GUI is to show make the
//   difference of bug 27 (Paste) where OpenRocket's own rule holds.
// The reader of version 3 is checked on small scenarios (GuiScenarioReaderV3), and the
// comparison of another GUI's records with these (guiRecordDifferences() and guiExpectedRecord())
// on the records (GuiRecordComparisonV3).

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <iterator>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Uuid.h"
#include "TestPaths.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/ConditionsTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GuiReferenceEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "rocket/preset/TestPresetDatabase.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::GuiConfirmAnswer;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiReferenceEnvironment;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiSettingsAnswer;
using QtRocket::Test::GuiSettingsEdit;
using QtRocket::Test::GuiSettingsEditKind;
using QtRocket::Test::GuiSimulationState;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::kGuiSimulationsScenario;

/// The contents of @p file; a test failure and "" when it cannot be read.
[[nodiscard]] std::string fileText(const std::filesystem::path& file)
{
    const Result<std::string> text = QtRocket::readTextFile(file);
    if (!text)
    {
        ADD_FAILURE() << text.error().toString();
        return {};
    }
    return *text;
}

/// The SHA-256 of the bytes of @p file.
[[nodiscard]] std::string fileSha256(const std::filesystem::path& file)
{
    return QtRocket::sha256Hex(QtRocket::stringToBytes(fileText(file)));
}

[[nodiscard]] std::filesystem::path scenarioDir()
{
    return QtRocket::Test::guiScenarioDir(kGuiSimulationsScenario);
}

[[nodiscard]] Result<GuiScenario> loadSimulationsScenario()
{
    return QtRocket::Test::parseGuiScenario(fileText(scenarioDir() / "scenario.json"));
}

[[nodiscard]] Result<GuiReference> loadSimulationsReference()
{
    return QtRocket::Test::parseGuiReference(fileText(scenarioDir() / "state.json"));
}

/// The scenario; a test failure and an empty one when it cannot be read.
[[nodiscard]] GuiScenario simulationsScenario()
{
    Result<GuiScenario> scenario = loadSimulationsScenario();
    if (!scenario)
    {
        ADD_FAILURE() << scenario.error().toString();
        return {};
    }
    return std::move(*scenario);
}

/// The reference; a test failure and an empty one when it cannot be read.
[[nodiscard]] GuiReference simulationsReference()
{
    Result<GuiReference> reference = loadSimulationsReference();
    if (!reference)
    {
        ADD_FAILURE() << reference.error().toString();
        return {};
    }
    return std::move(*reference);
}

[[nodiscard]] std::filesystem::path savedFile()
{
    return scenarioDir() / "rocket.ork";
}

/// The file @p path names in the repository; a test failure and "" when it is no such path.
[[nodiscard]] std::filesystem::path repositoryFile(std::string_view path)
{
    const Result<std::filesystem::path> file = QtRocket::Test::guiRepositoryFile(path);
    if (!file)
    {
        ADD_FAILURE() << file.error().toString();
        return {};
    }
    return *file;
}

/// The design file the scenario's open step names; a test failure and "" when it names none.
[[nodiscard]] std::filesystem::path openedFile(const GuiScenario& scenario)
{
    if (scenario.steps.empty() || scenario.steps.front().kind != GuiStepKind::OPEN)
    {
        ADD_FAILURE() << "the scenario opens no file";
        return {};
    }
    return repositoryFile(scenario.steps.front().file);
}

/// A design loaded by QtRocket in the environment of the references of versions 2 and 3.
struct SimulationsLoad
{
    GuiReferenceEnvironment environment{GuiReferenceEnvironment::Databases::BUNDLED};
    DefaultUnitsGuard       units;
    Result<LoadedDocument>  loaded;
    explicit SimulationsLoad(const std::filesystem::path& file)
      : loaded(GeneralRocketLoader(environment.get().context()).load(file))
    {
    }
    ~SimulationsLoad() = default;

    SimulationsLoad(const SimulationsLoad&)            = delete;
    SimulationsLoad& operator=(const SimulationsLoad&) = delete;
    SimulationsLoad(SimulationsLoad&&)                 = delete;
    SimulationsLoad& operator=(SimulationsLoad&&)      = delete;

    /// describeRocket() with no known id, as the records print the tree.
    [[nodiscard]] std::vector<std::string> tree()
    {
        return QtRocket::Test::describeRocket(loaded->document->getRocket(),
                                              environment.get().preferences(),
                                              std::set<QtRocket::Uuid>{});
    }
    /// The simulations as the records have them.
    [[nodiscard]] std::vector<GuiSimulationState> simulations()
    {
        return QtRocket::Test::guiSimulationStates(*loaded->document);
    }
};

/// The kinds of the scenario's steps by name.
[[nodiscard]] std::vector<std::string_view> stepNamesOf(const GuiScenario& scenario)
{
    std::vector<std::string_view> names;
    names.reserve(scenario.steps.size());
    for (const GuiScenarioStep& step : scenario.steps)
    {
        names.push_back(QtRocket::Test::guiStepName(step.kind));
    }
    return names;
}

/// The kinds of the records by name.
[[nodiscard]] std::vector<std::string_view> recordNamesOf(const GuiReference& reference)
{
    std::vector<std::string_view> names;
    names.reserve(reference.steps.size());
    for (const GuiStepRecord& record : reference.steps)
    {
        names.push_back(QtRocket::Test::guiStepName(record.kind));
    }
    return names;
}

/// The records whose index is not their place, whose tree digest is not that of their tree, or
/// that have a tree where none belongs or none where one does (README.md of the tool: after
/// open, ok, button, undo, redo and chooseFile; a step of the simulations tab holds the digest
/// only), as "<index> <what>".
[[nodiscard]] std::vector<std::string> wrongRecordTrees(const GuiReference& reference)
{
    std::vector<std::string> wrong;
    for (std::size_t at = 0; at < reference.steps.size(); ++at)
    {
        const GuiStepRecord& record = reference.steps[at];
        const bool holds = record.kind == GuiStepKind::OPEN || record.kind == GuiStepKind::OK ||
                           record.kind == GuiStepKind::BUTTON || record.kind == GuiStepKind::UNDO ||
                           record.kind == GuiStepKind::REDO ||
                           record.kind == GuiStepKind::CHOOSE_FILE;
        if (record.index != at)
        {
            wrong.push_back(std::format("{} misplaced", at));
        }
        if (record.tree.has_value() != holds)
        {
            wrong.push_back(std::format("{} tree {}", at, holds ? "missing" : "not expected"));
        }
        if (record.tree && QtRocket::Test::guiTreeSha256(*record.tree) != record.treeSha256)
        {
            wrong.push_back(std::format("{} digest", at));
        }
    }
    return wrong;
}

/// The steps of the simulations tab whose record's tree digest is not the one before: no step
/// of the tab changes the design (a Cancel's undo gives the tree back).
[[nodiscard]] std::vector<std::size_t> simulationStepsChangingTheTree(const GuiScenario&  scenario,
                                                                      const GuiReference& reference)
{
    std::vector<std::size_t> changing;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 1; at < steps; ++at)
    {
        if (QtRocket::Test::isGuiSimulationStep(scenario.steps[at].kind) &&
            reference.steps[at].treeSha256 != reference.steps[at - 1].treeSha256)
        {
            changing.push_back(at);
        }
    }
    return changing;
}

/// Whether the step @p step is cancelled: its settings dialog's or its confirmation's Cancel.
[[nodiscard]] bool isCancelledStep(const GuiScenarioStep& step)
{
    return (step.settings && step.settings->button == "cancel") ||
           (step.confirm && step.confirm->button == "cancel");
}

/// The simulations tab of @p record; an empty state when it has none (the records of version 3
/// all have it: EveryRecordShowsBothTabs).
[[nodiscard]] QtRocket::Test::GuiSimulationsTabState tabOf(const GuiStepRecord& record)
{
    return record.simulationsTab.value_or(QtRocket::Test::GuiSimulationsTabState{});
}

/// What is wrong with the record of @p step after @p before (the tool's checkSimulationStep()):
/// a click on rows changes the selection; New and Edit with OK change the simulations, with
/// Cancel they leave them; Duplicate, Cut and Paste change them, Delete does unless its
/// confirmation was cancelled; a column's visibility changes the table's columns.
[[nodiscard]] std::optional<std::string> simulationStepProblem(const GuiScenarioStep& step,
                                                               const GuiStepRecord&   before,
                                                               const GuiStepRecord&   after)
{
    const bool simulations = before.simulations != after.simulations;
    const bool tab         = before.simulationsTab != after.simulationsTab;
    const bool cancelled   = isCancelledStep(step);
    bool       right       = true;
    switch (step.kind)
    {
        case GuiStepKind::SELECT_SIMULATIONS:
            right = tabOf(before).selectedRows != tabOf(after).selectedRows;
            break;
        case GuiStepKind::NEW_SIMULATION:
        case GuiStepKind::EDIT_SIMULATION:
        case GuiStepKind::DELETE_SIMULATIONS:
            right = simulations != cancelled;
            break;
        case GuiStepKind::DUPLICATE_SIMULATIONS:
        case GuiStepKind::CUT_SIMULATIONS:
        case GuiStepKind::PASTE_SIMULATIONS:
            right = simulations;
            break;
        case GuiStepKind::COLUMN_VISIBILITY:
            right = tab && tabOf(before).columns != tabOf(after).columns;
            break;
        default:
            break;
    }
    if (right)
    {
        return std::nullopt;
    }
    return std::format("step {} ({}): simulations {}, table {}", step.index,
                       QtRocket::Test::guiStepName(step.kind),
                       simulations ? "changed" : "as before", tab ? "changed" : "as before");
}

/// The steps of the simulations tab whose records do not change what the step is for
/// (simulationStepProblem()).
[[nodiscard]] std::vector<std::string> wrongSimulationSteps(const GuiScenario&  scenario,
                                                            const GuiReference& reference)
{
    std::vector<std::string> wrong;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 1; at < steps; ++at)
    {
        const GuiScenarioStep& step = scenario.steps[at];
        if (!QtRocket::Test::isGuiSimulationStep(step.kind))
        {
            continue;
        }
        if (std::optional<std::string> problem =
                simulationStepProblem(step, reference.steps[at - 1], reference.steps[at]))
        {
            wrong.push_back(std::move(*problem));
        }
    }
    return wrong;
}

/// @p names joined by blanks.
[[nodiscard]] std::string joinedNames(std::span<const std::string_view> names)
{
    std::string text;
    for (const std::string_view name : names)
    {
        text += std::format("{}{}", text.empty() ? "" : " ", name);
    }
    return "[" + text + "]";
}

/// The kinds of @p questions by name.
[[nodiscard]] std::vector<std::string_view> questionNamesOf(std::span<const GuiQuestion> questions)
{
    std::vector<std::string_view> names;
    names.reserve(questions.size());
    for (const GuiQuestion& question : questions)
    {
        names.push_back(QtRocket::Test::guiQuestionName(question.kind));
    }
    return names;
}

/// The names of @p kinds.
[[nodiscard]] std::vector<std::string_view> questionKindNames(
    std::span<const GuiQuestionKind> kinds)
{
    std::vector<std::string_view> names;
    names.reserve(kinds.size());
    for (const GuiQuestionKind kind : kinds)
    {
        names.push_back(QtRocket::Test::guiQuestionName(kind));
    }
    return names;
}

/// The records whose questions are not those the answer of their step asks, in the order they
/// open (guiSimulationStepQuestions(); none for a step that is not the simulations tab's), as
/// "step <n>: <asked> instead of <answered>".
[[nodiscard]] std::vector<std::string> wrongQuestionOrders(const GuiScenario&  scenario,
                                                           const GuiReference& reference)
{
    std::vector<std::string> wrong;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 0; at < steps; ++at)
    {
        const std::vector<GuiQuestionKind> answered =
            QtRocket::Test::guiSimulationStepQuestions(scenario.steps[at]);
        const std::vector<std::string_view> expected = questionKindNames(answered);
        const std::vector<std::string_view> asked = questionNamesOf(reference.steps[at].questions);
        if (expected != asked)
        {
            wrong.push_back(std::format("step {}: {} instead of {}", at, joinedNames(asked),
                                        joinedNames(expected)));
        }
    }
    return wrong;
}

/// The files the file choosers of the records chose, in the order of the steps.
[[nodiscard]] std::vector<std::string> chosenFiles(const GuiReference& reference)
{
    std::vector<std::string> files;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiQuestion& question : record.questions)
        {
            if (question.kind == GuiQuestionKind::OPEN_FILE && question.file)
            {
                files.push_back(*question.file);
            }
        }
    }
    return files;
}

/// The files of @p files that are no regular files of the repository.
[[nodiscard]] std::vector<std::string> missingFiles(std::span<const std::string> files)
{
    std::vector<std::string> missing;
    for (const std::string& file : files)
    {
        if (!std::filesystem::is_regular_file(repositoryFile(file)))
        {
            missing.push_back(file);
        }
    }
    return missing;
}

/// The kinds of the steps of @p scenario and of the edits of their settings dialogs, by name
/// ("editSimulation", "edit:windLevels", "wind:importLevels", "lookup:load", "cancelled
/// editSimulation", ...): what the scenario covers.
[[nodiscard]] std::set<std::string> coveredKinds(const GuiScenario& scenario)
{
    static constexpr std::array<std::string_view, 9> kEdits{
        "field",      "name",     "configuration", "tab",   "windModel",
        "randomSeed", "defaults", "windLevels",    "lookup"};
    static constexpr std::array<std::string_view, 8> kWindEdits{
        "altitudeReference", "addLevel",    "deleteLevel", "deleteRow", "cell",
        "columnUnit",        "resetLevels", "importLevels"};
    static constexpr std::array<std::string_view, 5> kLookupActions{"separator", "load", "text",
                                                                    "refresh", "clear"};
    std::set<std::string>                            kinds;
    for (const GuiScenarioStep& step : scenario.steps)
    {
        const std::string name(QtRocket::Test::guiStepName(step.kind));
        kinds.insert(name);
        if (isCancelledStep(step))
        {
            kinds.insert("cancelled " + name);
        }
        if (!step.settings)
        {
            continue;
        }
        if (step.settings->button == "cancel" && step.settings->edits.empty())
        {
            kinds.insert("unchanged cancelled " + name);
        }
        for (const GuiSettingsEdit& edit : step.settings->edits)
        {
            kinds.insert(std::format("edit:{}", kEdits.at(static_cast<std::size_t>(edit.kind))));
            for (const QtRocket::Test::GuiWindEdit& wind : edit.windLevels.edits)
            {
                kinds.insert(
                    std::format("wind:{}", kWindEdits.at(static_cast<std::size_t>(wind.kind))));
            }
            for (const QtRocket::Test::GuiLookupAction& action :
                 edit.lookup.drag.value_or(std::vector<QtRocket::Test::GuiLookupAction>{}))
            {
                kinds.insert(std::format("lookup:{}",
                                         kLookupActions.at(static_cast<std::size_t>(action.kind))));
            }
        }
    }
    return kinds;
}

/// The fields the edits of @p scenario set.
[[nodiscard]] std::set<std::string> editedFields(const GuiScenario& scenario)
{
    std::set<std::string> fields;
    for (const GuiScenarioStep& step : scenario.steps)
    {
        for (const GuiSettingsEdit& edit : step.settings.value_or(GuiSettingsAnswer{}).edits)
        {
            if (edit.kind == GuiSettingsEditKind::FIELD)
            {
                fields.insert(edit.field);
            }
        }
    }
    return fields;
}

/// The preset database commit of the goldens' manifest ("openrocket.presetDatabaseCommit").
[[nodiscard]] std::string manifestPresetDatabaseCommit()
{
    const Result<nlohmann::json> manifest =
        QtRocket::Test::loadGoldenJson(QtRocket::Test::goldensDir() / "manifest.json");
    if (!manifest)
    {
        ADD_FAILURE() << manifest.error().toString();
        return {};
    }
    return manifest->at("openrocket").at("presetDatabaseCommit").get<std::string>();
}

/// The number of motors of the bundled database (in all its sets).
[[nodiscard]] std::size_t bundledMotors()
{
    std::size_t count = 0;
    for (const QtRocket::ThrustCurveMotorSet& set :
         QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// The lines of @p state that are warnings ("W <text>").
[[nodiscard]] std::vector<std::string> warningLinesIn(const std::vector<std::string>& state)
{
    std::vector<std::string> warnings;
    std::ranges::copy_if(state, std::back_inserter(warnings),
                         [](const std::string& line) { return line.starts_with("W "); });
    return warnings;
}

/// The lookup files the options of the records name ("path:<file>" but "path:null"), each once.
[[nodiscard]] std::set<std::string> recordedLookupFiles(const GuiReference& reference)
{
    std::set<std::string> files;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiSimulationState& simulation : record.simulations)
        {
            for (const std::string& option : simulation.options)
            {
                constexpr std::string_view kPath = "path:";
                const std::size_t          at    = option.find(kPath);
                const std::size_t          end =
                    at == std::string::npos ? at : option.find(' ', at + kPath.size());
                if (at == std::string::npos)
                {
                    continue;
                }
                std::string file = option.substr(at + kPath.size(), end - at - kPath.size());
                if (file != "null")
                {
                    files.insert(std::move(file));
                }
            }
        }
    }
    return files;
}

/// The fixes the records list, as "<step> bug <n>".
[[nodiscard]] std::vector<std::string> recordedToolFixes(const GuiReference& reference)
{
    std::vector<std::string> fixes;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const QtRocket::Test::GuiRecordedToolFix& fix : record.toolFixes)
        {
            fixes.push_back(std::format("{} bug {}{}", record.index, fix.bug,
                                        fix.fix.empty() ? " (no text)" : ""));
        }
    }
    return fixes;
}

/// The fixes kGuiSimulationToolFixes lists for the scenario, as "<step> bug <n>".
[[nodiscard]] std::vector<std::string> listedToolFixes()
{
    std::vector<std::string> fixes;
    for (const QtRocket::Test::GuiSimulationToolFix& fix : QtRocket::Test::kGuiSimulationToolFixes)
    {
        if (fix.scenario == kGuiSimulationsScenario)
        {
            fixes.push_back(std::format("{} bug {}", fix.step, fix.bug));
        }
    }
    return fixes;
}

/// The records of bug 6's fix that are not those of a step that undoes (Undo, Redo, a Cancel
/// whose discard question was answered), and the records of an undo or a redo whose tree has a
/// default ignition other than OpenRocket's new one without the fix listed.
[[nodiscard]] std::vector<std::string> misplacedIgnitionFixes(const GuiScenario&  scenario,
                                                              const GuiReference& reference)
{
    std::vector<std::string> wrong;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 0; at < steps; ++at)
    {
        const GuiScenarioStep& step   = scenario.steps[at];
        const GuiStepRecord&   record = reference.steps[at];
        const bool undoes = step.kind == GuiStepKind::UNDO || step.kind == GuiStepKind::REDO ||
                            (step.settings && step.settings->discard.has_value());
        const bool listed = std::ranges::any_of(
            record.toolFixes,
            [](const QtRocket::Test::GuiRecordedToolFix& fix) { return fix.bug == 6; });
        const bool kept = std::ranges::any_of(
            record.tree.value_or(std::vector<std::string>{}), [](const std::string& line) {
                const std::size_t ignition = line.find(" ign=");
                return ignition != std::string::npos &&
                       !line.substr(ignition).starts_with(QtRocket::Test::kGuiFreshDefaultIgnition);
            });
        if ((listed && !undoes) || (record.tree && undoes && kept != listed))
        {
            wrong.push_back(std::format("step {}", at));
        }
    }
    return wrong;
}

/// The steps whose expected record (guiExpectedRecord()) has another Paste state than
/// OpenRocket's record.
[[nodiscard]] std::vector<std::size_t> pasteDifferences(const GuiReference& reference,
                                                        std::string&        problems)
{
    std::vector<std::size_t> steps;
    for (const GuiStepRecord& record : reference.steps)
    {
        const GuiStepRecord expected =
            QtRocket::Test::guiExpectedRecord(kGuiSimulationsScenario, record, problems);
        if (!record.simulationsTab || !expected.simulationsTab)
        {
            continue;
        }
        if (record.simulationsTab->actions.at("pasteSimulation") !=
            expected.simulationsTab->actions.at("pasteSimulation"))
        {
            steps.push_back(record.index);
        }
    }
    return steps;
}

/// The records whose tabs are not both shown: the Motors & Configuration tab with its three
/// sub-tabs, the simulations tab with a row per simulation, the twelve actions and columns of
/// kGuiSimulationColumns, and after the step to the simulations tab that tab.
[[nodiscard]] std::vector<std::size_t> recordsWithoutTheTabs(const GuiReference& reference)
{
    std::vector<std::size_t> without;
    for (const GuiStepRecord& record : reference.steps)
    {
        constexpr std::size_t kActions = 12;
        const bool            configuration =
            record.flightConfiguration && record.flightConfiguration->configTabs.size() == 3;
        const bool simulations =
            record.simulationsTab &&
            record.simulationsTab->rows.size() == record.simulations.size() &&
            record.simulationsTab->actions.size() == kActions &&
            std::ranges::all_of(record.simulationsTab->columns,
                                [](const QtRocket::Test::GuiSimulationTableColumn& column) {
                                    return std::ranges::find(QtRocket::Test::kGuiSimulationColumns,
                                                             column.id) !=
                                           QtRocket::Test::kGuiSimulationColumns.end();
                                });
        const bool tab = record.index == 0 ? record.tab == "design" : record.tab == "simulations";
        if (!configuration || !simulations || !tab)
        {
            without.push_back(record.index);
        }
    }
    return without;
}

/// The thrust curve entries of the archive of @p reference.
[[nodiscard]] std::vector<std::string> thrustCurves(const GuiReference& reference)
{
    std::vector<std::string> entries;
    std::ranges::copy_if(reference.archiveEntries, std::back_inserter(entries),
                         [](const std::string& entry) { return entry != "rocket.ork"; });
    return entries;
}

/// The digests of @p entries ("thrustcurves/<digest>.rse") that do not have a motor digest's
/// form, and the entries of another form, as themselves.
[[nodiscard]] std::vector<std::string> malformedThrustCurves(std::span<const std::string> entries)
{
    std::vector<std::string> malformed;
    for (const std::string& entry : entries)
    {
        constexpr std::string_view kPrefix = "thrustcurves/";
        constexpr std::string_view kSuffix = ".rse";
        if (!entry.starts_with(kPrefix) || !entry.ends_with(kSuffix) ||
            !QtRocket::Test::hasMotorDigestForm(
                entry.substr(kPrefix.size(), entry.size() - kPrefix.size() - kSuffix.size())))
        {
            malformed.push_back(entry);
        }
    }
    return malformed;
}

// ================================================================================== the files

TEST(SimulationsReferenceGolden, TheScenarioIsWellFormed)
{
    const Result<GuiScenario> loaded = loadSimulationsScenario();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(loaded->schemaVersion, 3);
    EXPECT_FALSE(loaded->title.empty());
    ASSERT_FALSE(loaded->steps.empty());
    EXPECT_EQ(loaded->steps.front().kind, GuiStepKind::OPEN);
    EXPECT_TRUE(std::filesystem::is_regular_file(openedFile(*loaded)));
    // It ends with the save: the file chooser of Save as, after the design information dialog.
    EXPECT_EQ(loaded->steps.back().kind, GuiStepKind::CHOOSE_FILE);
    // The files its answers open are files of the repository, the scenario's own fixtures.
    const std::vector<std::string> files = QtRocket::Test::guiScenarioAnswerFiles(*loaded);
    EXPECT_EQ(files, (std::vector<std::string>{"tests/data/goldens/gui/simulations/drag.csv",
                                               "tests/data/goldens/gui/simulations/wind-levels.csv",
                                               "tests/data/goldens/gui/simulations/"
                                               "wind-levels-space.csv"}));
    EXPECT_EQ(missingFiles(files), std::vector<std::string>{});
}

TEST(SimulationsReferenceGolden, TheScenarioCoversTheTabAndItsDialogs)
{
    // Plan 11.6 E6: the tab, New with conditions by property, a multi-level wind edited in the
    // wind editor and levels imported through the CSV import dialog and its chooser, the options,
    // the warnings thresholds, the lookup dialog, OK; Edit with Cancel with and without changes;
    // Duplicate; Copy, Cut, Paste; Delete (with "Do not ask me again"); Undo, Redo; a column
    // hidden and one shown; Save as.
    const GuiScenario           scenario = simulationsScenario();
    const std::set<std::string> kinds    = coveredKinds(scenario);
    for (const std::string_view kind : {"tab",
                                        "selectSimulations",
                                        "newSimulation",
                                        "editSimulation",
                                        "duplicateSimulations",
                                        "deleteSimulations",
                                        "copySimulations",
                                        "cutSimulations",
                                        "pasteSimulations",
                                        "columnVisibility",
                                        "undo",
                                        "redo",
                                        "saveAs",
                                        "chooseFile",
                                        "cancelled editSimulation",
                                        "unchanged cancelled editSimulation",
                                        "cancelled newSimulation",
                                        "cancelled deleteSimulations",
                                        "edit:field",
                                        "edit:name",
                                        "edit:configuration",
                                        "edit:windModel",
                                        "edit:randomSeed",
                                        "edit:windLevels",
                                        "edit:lookup",
                                        "wind:altitudeReference",
                                        "wind:addLevel",
                                        "wind:deleteLevel",
                                        "wind:cell",
                                        "wind:columnUnit",
                                        "wind:resetLevels",
                                        "wind:importLevels",
                                        "lookup:load",
                                        "lookup:text",
                                        "lookup:clear"})
    {
        EXPECT_TRUE(kinds.contains(std::string(kind))) << kind;
    }
    // The conditions by property, the options and the thresholds.
    const std::set<std::string> fields = editedFields(scenario);
    for (const std::string_view field : {"Average",
                                         "TurbulenceIntensity",
                                         "Direction",
                                         "LaunchAltitude",
                                         "LaunchLatitude",
                                         "LaunchLongitude",
                                         "LaunchIntoWind",
                                         "LaunchRodLength",
                                         "LaunchRodAngle",
                                         "LaunchRodDirection",
                                         "ISAAtmosphere",
                                         "LaunchTemperature",
                                         "LaunchPressure",
                                         "LaunchRelativeHumidity",
                                         "SimulationStepperMethodChoice",
                                         "GeodeticComputation",
                                         "GravityModelType",
                                         "ConstantGravity",
                                         "TimeStep",
                                         "MaxSimulationTime",
                                         "RecoverySpeedWarning",
                                         "RecoveryDrogueMainHighSpeedWarning"})
    {
        EXPECT_TRUE(fields.contains(std::string(field))) << field;
    }
}

TEST(SimulationsReferenceGolden, TheReferenceIsTheRecordOfThisScenario)
{
    const Result<GuiScenario>  loaded    = loadSimulationsScenario();
    const Result<GuiReference> reference = loadSimulationsReference();
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(reference->schemaVersion, 3);
    EXPECT_EQ(reference->scenarioFile, "scenario.json");
    EXPECT_EQ(reference->scenarioSha256, fileSha256(scenarioDir() / "scenario.json"))
        << "scenario.json changed after the reference was made: run "
           "tools/openrocket-gui-reference/generate.sh simulations";
    EXPECT_EQ(reference->scenarioTitle, loaded->title);
    EXPECT_EQ(reference->scenarioSteps, loaded->steps.size());
    EXPECT_EQ(recordNamesOf(*reference), stepNamesOf(*loaded));
    // The input as it was when the reference was made.
    ASSERT_FALSE(loaded->steps.empty());
    EXPECT_EQ(reference->inputFile, loaded->steps.front().file);
    EXPECT_EQ(reference->inputSha256, fileSha256(openedFile(*loaded)))
        << "the scenario's design file changed after the reference was made";
}

TEST(SimulationsReferenceGolden, TheInputIsTheSaveOfTheFlightConfigurationsReference)
{
    // generate.sh runs the scenario after flight-configurations, whose save it opens: the input
    // is the document of that reference's archive, byte for byte.
    const GuiReference         reference = simulationsReference();
    const Result<GuiReference> input     = QtRocket::Test::parseGuiReference(
        fileText(QtRocket::Test::guiScenarioDir("flight-configurations") / "state.json"));
    ASSERT_TRUE(input.has_value()) << input.error().toString();
    EXPECT_EQ(reference.inputFile, "tests/data/goldens/gui/flight-configurations/rocket.ork");
    const auto document = input->archiveSha256.find("rocket.ork");
    ASSERT_NE(document, input->archiveSha256.end());
    EXPECT_EQ(reference.inputSha256, document->second);
    // The same motors, whose thrust curves both archives hold.
    EXPECT_EQ(thrustCurves(reference), thrustCurves(*input));
}

TEST(SimulationsReferenceGolden, TheReferenceComesFromTheOpenRocketOfTheGoldens)
{
    const GuiReference reference = simulationsReference();
    const auto         manifest  = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(reference.openRocketCommit, manifest->openrocketCommit);
    EXPECT_EQ(reference.openRocketVersion, manifest->openrocketVersion);
    EXPECT_EQ(reference.presetDatabaseCommit, manifestPresetDatabaseCommit());
    EXPECT_FALSE(reference.dirty);
    EXPECT_TRUE(reference.javaVersion.starts_with("17."));
    // OpenRocket's start-up loaded the databases QtRocket bundles.
    EXPECT_EQ(static_cast<std::size_t>(reference.startup.motorSets),
              QtRocket::Test::bundledMotorDatabase().getMotorSets().size());
    EXPECT_EQ(static_cast<std::size_t>(reference.startup.motors), bundledMotors());
    EXPECT_EQ(static_cast<std::size_t>(reference.startup.presets),
              QtRocket::Test::bundledPresetDatabase().size());
}

TEST(SimulationsReferenceGolden, TheArchiveHoldsTheDesignAndTheThrustCurvesOfItsMotors)
{
    const GuiReference reference = simulationsReference();
    EXPECT_EQ(reference.referenceFile, "rocket.ork");
    EXPECT_EQ(reference.savedAs, "rocket.ork");
    ASSERT_FALSE(reference.archiveEntries.empty());
    EXPECT_EQ(reference.archiveEntries.front(), "rocket.ork");
    std::set<std::string> digested;
    for (const auto& [entry, sha256] : reference.archiveSha256)
    {
        digested.insert(entry);
    }
    EXPECT_EQ(digested, std::set<std::string>(reference.archiveEntries.begin(),
                                              reference.archiveEntries.end()));
    const auto document = reference.archiveSha256.find("rocket.ork");
    ASSERT_NE(document, reference.archiveSha256.end());
    EXPECT_EQ(document->second, fileSha256(savedFile()));
    const std::vector<std::string> curves = thrustCurves(reference);
    EXPECT_EQ(curves.size(), 2U);
    EXPECT_EQ(malformedThrustCurves(curves), std::vector<std::string>{});
}

TEST(SimulationsReferenceGolden, EveryTreeDigestIsTheDigestOfItsTree)
{
    const GuiReference reference = simulationsReference();
    ASSERT_FALSE(reference.steps.empty());
    EXPECT_TRUE(reference.steps.front().saved);
    EXPECT_TRUE(reference.steps.back().saved);
    EXPECT_EQ(wrongRecordTrees(reference), std::vector<std::string>{});
}

// ================================================================================ the steps

TEST(SimulationsReferenceGolden, NoStepOfTheSimulationsTabChangesTheTree)
{
    // The tool fails on a step of the tab that changes the design but by a Cancel's undo, which
    // gives it back (QtRocket's fix of bugs 29 and 30); a tree digest that changed would be such
    // a step.
    EXPECT_EQ(simulationStepsChangingTheTree(simulationsScenario(), simulationsReference()),
              std::vector<std::size_t>{});
}

TEST(SimulationsReferenceGolden, EveryStepOfTheTabChangesWhatItIsFor)
{
    // A step that changes nothing would also pass a GUI that does nothing; the tool refuses such
    // a scenario, and this guards the committed record. A cancelled one changes nothing, which is
    // what it checks.
    EXPECT_EQ(wrongSimulationSteps(simulationsScenario(), simulationsReference()),
              std::vector<std::string>{});
}

TEST(SimulationsReferenceGolden, EveryQuestionBelongsToItsStep)
{
    const GuiScenario  scenario  = simulationsScenario();
    const GuiReference reference = simulationsReference();
    EXPECT_EQ(wrongQuestionOrders(scenario, reference), std::vector<std::string>{});
    // The file choosers chose the files of their answers.
    EXPECT_EQ(chosenFiles(reference), QtRocket::Test::guiScenarioAnswerFiles(scenario));
}

TEST(SimulationsReferenceGolden, EveryRecordShowsBothTabs)
{
    EXPECT_EQ(recordsWithoutTheTabs(simulationsReference()), std::vector<std::size_t>{});
}

TEST(SimulationsReferenceGolden, TheLookupFilesOfTheRecordsAreFilesOfTheRepository)
{
    // The record writes a lookup table's file relative to the repository
    // (guiRelativeLookupPaths()); the scenario clears it before Save as.
    const GuiReference             reference = simulationsReference();
    const std::set<std::string>    files     = recordedLookupFiles(reference);
    const std::vector<std::string> listed(files.begin(), files.end());
    EXPECT_EQ(listed, std::vector<std::string>{"tests/data/goldens/gui/simulations/drag.csv"});
    EXPECT_EQ(missingFiles(listed), std::vector<std::string>{});
    ASSERT_FALSE(reference.steps.empty());
    for (const GuiSimulationState& simulation : reference.steps.back().simulations)
    {
        EXPECT_FALSE(std::ranges::any_of(simulation.options, [](const std::string& option) {
            return option.contains("path:") && !option.contains("path:null");
        })) << simulation.name;
    }
}

// =========================================================================== the input design

TEST(SimulationsReferenceGolden, TheOpenStepGivesQtRocketsLoadOfTheInput)
{
    // The exit test opens the input through QtRocket's GUI: its tree and its simulations are
    // those OpenRocket's open step recorded.
    const GuiScenario  scenario  = simulationsScenario();
    const GuiReference reference = simulationsReference();
    ASSERT_FALSE(reference.steps.empty());
    const GuiStepRecord& open = reference.steps.front();
    ASSERT_TRUE(open.tree.has_value());
    SimulationsLoad load(openedFile(scenario));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    EXPECT_EQ(QtRocket::Test::warningLines(load.loaded->warnings), "");
    EXPECT_EQ(load.tree(), open.tree.value_or(std::vector<std::string>{}));
    EXPECT_EQ(load.simulations(), open.simulations);
    EXPECT_EQ(open.simulations.size(), 3U);
}

// ================================================================================== the design

TEST(SimulationsReferenceGolden, TheSavedDesignLoadsWithoutWarnings)
{
    SimulationsLoad load(savedFile());
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    EXPECT_EQ(QtRocket::Test::warningLines(load.loaded->warnings), "");
}

TEST(SimulationsReferenceGolden, TheSavedDesignLoadsIntoOpenRocketsState)
{
    // designFileState() holds every simulation with its options, conditions, wind levels and
    // extensions (SimulationLines), line for line.
    const GuiReference             reference = simulationsReference();
    GuiReferenceEnvironment        environment(GuiReferenceEnvironment::Databases::BUNDLED);
    const std::vector<std::string> state = QtRocket::Test::designFileState(
        environment.get(), savedFile(), QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string_view> expected(reference.loadedLines.begin(),
                                                 reference.loadedLines.end());
    EXPECT_EQ(QtRocket::Test::firstDifference(expected, state), "");
    EXPECT_EQ(warningLinesIn(reference.loadedLines), std::vector<std::string>{});
}

TEST(SimulationsReferenceGolden, TheSavedDesignHasOpenRocketsMassesAndLength)
{
    const GuiReference reference = simulationsReference();
    SimulationsLoad    load(savedFile());
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    const std::vector<std::string_view> expected(reference.loadedNumbers.begin(),
                                                 reference.loadedNumbers.end());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(
                  expected, QtRocket::Test::massAndLength(load.loaded->document->getRocket())),
              "");
}

TEST(SimulationsReferenceGolden, TheReloadIsTheSavedRocketButForWhatTheReloadChanges)
{
    const GuiReference reference = simulationsReference();
    ASSERT_FALSE(reference.steps.empty());
    ASSERT_TRUE(reference.steps.back().tree.has_value());
    SimulationsLoad load(savedFile());
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    std::string                    problems;
    const std::vector<std::string> reloaded = QtRocket::Test::guiReloadedTree(
        reference.steps.back().tree.value_or(std::vector<std::string>{}),
        QtRocket::Test::guiReloadChanges(kGuiSimulationsScenario), problems);
    EXPECT_EQ(problems, "");
    EXPECT_EQ(load.tree(), reloaded);
}

TEST(SimulationsReferenceGolden, TheReloadHasTheSimulationsOfTheLastRecord)
{
    const GuiReference reference = simulationsReference();
    ASSERT_FALSE(reference.steps.empty());
    SimulationsLoad load(savedFile());
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    EXPECT_EQ(load.simulations(), reference.steps.back().simulations);
    EXPECT_EQ(reference.steps.back().simulations.size(), 5U);
}

// ======================================================================= the tool's fixes, bug 27

TEST(SimulationsReferenceGolden, TheToolsFixesAreThoseTheListNames)
{
    // The records are OpenRocket's with QtRocket's fixes of bugs 6, 32, 33 and 34 (the tool makes
    // them in OpenRocket's run): kGuiSimulationToolFixes names every record where one shows.
    const GuiScenario  scenario  = simulationsScenario();
    const GuiReference reference = simulationsReference();
    EXPECT_EQ(recordedToolFixes(reference), listedToolFixes());
    EXPECT_EQ(misplacedIgnitionFixes(scenario, reference), std::vector<std::string>{});
}

TEST(SimulationsReferenceGolden, TheExpectedRecordsHaveQtRocketsPaste)
{
    // Bug 27: OpenRocket's Paste needs a selected row; after the cancelled New simulation no row
    // is selected, and QtRocket's Paste of the two simulations on the clipboard stays enabled.
    std::string                    problems;
    const std::vector<std::size_t> steps = pasteDifferences(simulationsReference(), problems);
    EXPECT_EQ(problems, "");
    EXPECT_EQ(steps, (std::vector<std::size_t>{29, 30, 31, 32, 33}));
}

// ================================================================================= the reader

/// A scenario of version 3 whose steps are @p steps (JSON text of the array's elements) after
/// the open step.
[[nodiscard]] std::string scenarioV3(std::string_view steps)
{
    return std::format(R"({{"schema": "gui-scenario", "schemaVersion": 3, "title": "t", "steps": [)"
                       R"({{"step": "open", "file": "tests/data/x.ork"}}{}{}]}})",
                       steps.empty() ? "" : ", ", steps);
}

/// The error of parsing @p text, "" when it parses.
[[nodiscard]] std::string scenarioError(const std::string& text)
{
    const Result<GuiScenario> parsed = QtRocket::Test::parseGuiScenario(text);
    return parsed ? std::string() : parsed.error().message;
}

/// The step @p at of @p scenario; a test failure and an empty step when it has none.
[[nodiscard]] GuiScenarioStep stepAt(const Result<GuiScenario>& scenario, std::size_t at)
{
    if (!scenario || at >= scenario->steps.size())
    {
        ADD_FAILURE() << "no step " << at;
        return {};
    }
    return scenario->steps[at];
}

/// The settings dialog's answer of @p step; a test failure and an empty answer when it has none.
[[nodiscard]] GuiSettingsAnswer settingsOf(const GuiScenarioStep& step)
{
    if (!step.settings)
    {
        ADD_FAILURE() << "step " << step.index << " answers no settings dialog";
        return {};
    }
    return *step.settings;
}

/// The edit @p at of the settings answer of @p step; a test failure and an empty edit when it has
/// none.
[[nodiscard]] GuiSettingsEdit settingsEditAt(const GuiScenarioStep& step, std::size_t at)
{
    const GuiSettingsAnswer settings = settingsOf(step);
    if (at >= settings.edits.size())
    {
        ADD_FAILURE() << "step " << step.index << " has no edit " << at;
        return {};
    }
    return settings.edits[at];
}

TEST(GuiScenarioReaderV3, ReadsTheStepsOfTheSimulationsTab)
{
    const Result<GuiScenario> parsed = QtRocket::Test::parseGuiScenario(scenarioV3(
        R"({"step": "tab", "tab": "simulations"},
           {"step": "selectSimulations", "rows": [{"row": 1}, {"row": 2, "with": "ctrl"}]},
           {"step": "selectSimulations", "rows": []},
           {"step": "deleteSimulations", "answer": {"button": "ok", "doNotAskAgain": true}},
           {"step": "cutSimulations"},
           {"step": "columnVisibility", "column": "apogee", "visible": false},
           {"step": "editSimulation", "answer": {"edits": [
             {"field": "LaunchRodLength", "text": "1.5", "unit": "m"},
             {"field": "LaunchIntoWind", "checked": false},
             {"field": "GravityModelType", "item": "CONSTANT"},
             {"name": "n"}, {"configuration": 1}, {"tab": "warnings"}, {"windModel": "MULTI_LEVEL"},
             {"randomSeed": {"fixed": true, "text": "12"}}, {"defaults": "save", "panel": "warnings"}
           ], "button": "ok", "run": {"button": "cancel"}}},
           {"step": "newSimulation", "answer": {"button": "cancel", "discard": {"button": "yes", "doNotAskAgain": true}}})"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->schemaVersion, 3);
    EXPECT_EQ(stepAt(parsed, 1).tab, "simulations");
    EXPECT_EQ(stepAt(parsed, 2).rows, (std::vector<QtRocket::Test::GuiRowClick>{
                                          {.row = 1, .with = ""}, {.row = 2, .with = "ctrl"}}));
    EXPECT_TRUE(stepAt(parsed, 3).rows.empty());
    EXPECT_EQ(stepAt(parsed, 4).confirm, (GuiConfirmAnswer{.button = "ok", .doNotAskAgain = true}));
    EXPECT_FALSE(stepAt(parsed, 5).confirm.has_value());
    EXPECT_EQ(stepAt(parsed, 6).column, "apogee");
    EXPECT_FALSE(stepAt(parsed, 6).visible);
    const GuiScenarioStep   edit     = stepAt(parsed, 7);
    const GuiSettingsAnswer settings = settingsOf(edit);
    EXPECT_EQ(settings.edits.size(), 9U);
    EXPECT_EQ(settings.button, "ok");
    EXPECT_EQ(settings.run, "cancel");
    EXPECT_FALSE(settings.discard.has_value());
    EXPECT_EQ(settingsEditAt(edit, 0).text, "1.5");
    EXPECT_EQ(settingsEditAt(edit, 0).unit, "m");
    EXPECT_EQ(settingsEditAt(edit, 1).checked, false);
    EXPECT_EQ(settingsEditAt(edit, 2).item, "CONSTANT");
    EXPECT_EQ(settingsEditAt(edit, 3).name, "n");
    EXPECT_EQ(settingsEditAt(edit, 4).configuration, 1);
    EXPECT_EQ(settingsEditAt(edit, 5).tab, "warnings");
    EXPECT_EQ(settingsEditAt(edit, 6).windModel, "MULTI_LEVEL");
    EXPECT_TRUE(settingsEditAt(edit, 7).seedFixed);
    EXPECT_EQ(settingsEditAt(edit, 7).seedText, "12");
    EXPECT_EQ(settingsEditAt(edit, 8).defaults, "save");
    EXPECT_EQ(settingsEditAt(edit, 8).tab, "warnings");
    const GuiSettingsAnswer cancelled = settingsOf(stepAt(parsed, 8));
    EXPECT_TRUE(cancelled.edits.empty());
    EXPECT_EQ(cancelled.button, "cancel");
    EXPECT_FALSE(cancelled.run.has_value());
    EXPECT_EQ(cancelled.discard, (GuiConfirmAnswer{.button = "yes", .doNotAskAgain = true}));
}

TEST(GuiScenarioReaderV3, ReadsTheWindEditorAndTheLookupDialog)
{
    const Result<GuiScenario> parsed = QtRocket::Test::parseGuiScenario(scenarioV3(
        R"({"step": "editSimulation", "answer": {"edits": [
             {"windLevels": {"edits": [
               {"altitudeReference": "AGL"}, {"addLevel": true}, {"deleteLevel": 1}, {"deleteRow": 0},
               {"cell": {"row": 1, "column": "speed", "text": "6"}},
               {"columnUnit": {"column": "altitude", "unit": "ft"}},
               {"resetLevels": {"button": "no"}},
               {"importLevels": {"settings": {"hasHeader": false,
                                              "columns": {"speed": {"index": 2}, "altitude": {"name": "h", "unit": "ft"}},
                                              "separator": " ", "button": "ok"},
                                 "file": {"file": "tests/w.CSV", "button": "open"},
                                 "overwrite": {"button": "yes"}}}
             ], "button": "close"}},
             {"lookup": {"stability": [{"separator": ";"}, {"load": {"button": "cancel"}}, {"text": "x"},
                                       {"refresh": true}, {"clear": true}],
                         "drag": [{"load": {"file": "tests/d.csv", "button": "open"}}], "button": "cancel"}}
           ], "button": "ok"}})"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    const GuiScenarioStep step = stepAt(parsed, 1);
    const GuiSettingsEdit wind = settingsEditAt(step, 0);
    ASSERT_EQ(wind.windLevels.edits.size(), 8U);
    EXPECT_EQ(wind.windLevels.button, "close");
    EXPECT_EQ(wind.windLevels.edits[0].altitudeReference, "AGL");
    EXPECT_EQ(wind.windLevels.edits[1].kind, QtRocket::Test::GuiWindEditKind::ADD_LEVEL);
    EXPECT_EQ(wind.windLevels.edits[2].row, 1);
    EXPECT_EQ(wind.windLevels.edits[3].kind, QtRocket::Test::GuiWindEditKind::DELETE_ROW);
    EXPECT_EQ(wind.windLevels.edits[4].column, "speed");
    EXPECT_EQ(wind.windLevels.edits[4].text, "6");
    EXPECT_EQ(wind.windLevels.edits[5].unit, "ft");
    EXPECT_EQ(wind.windLevels.edits[6].resetQuestion.button, "no");
    const QtRocket::Test::GuiWindImportAnswer& import = wind.windLevels.edits[7].importLevels;
    EXPECT_EQ(import.settings.hasHeader, false);
    // The rows in the order of kGuiCsvImportRows.
    ASSERT_EQ(import.settings.columns.size(), 2U);
    EXPECT_EQ(import.settings.columns[0],
              (QtRocket::Test::GuiCsvColumnAnswer{
                  .row = "altitude", .name = "h", .index = std::nullopt, .unit = "ft"}));
    EXPECT_EQ(import.settings.columns[1].index, 2);
    EXPECT_EQ(import.settings.separator, " ");
    EXPECT_EQ(import.file,
              (QtRocket::Test::GuiFileAnswer{.button = "open", .file = "tests/w.CSV"}));
    EXPECT_EQ(import.overwrite, (GuiConfirmAnswer{.button = "yes", .doNotAskAgain = false}));
    const QtRocket::Test::GuiLookupAnswer lookup = settingsEditAt(step, 1).lookup;
    EXPECT_EQ(lookup.drag.value_or(std::vector<QtRocket::Test::GuiLookupAction>{}).size(), 1U);
    EXPECT_EQ(lookup.stability.value_or(std::vector<QtRocket::Test::GuiLookupAction>{}).size(), 5U);
    EXPECT_EQ(lookup.button, "cancel");
    // The questions they ask, in the order they open, and the files they open.
    EXPECT_EQ(
        questionKindNames(QtRocket::Test::guiSimulationStepQuestions(step)),
        (std::vector<std::string_view>{"simulation", "windLevels", "confirm", "csvImportSettings",
                                       "openFile", "confirm", "lookup", "openFile", "openFile"}));
    EXPECT_EQ(QtRocket::Test::guiScenarioAnswerFiles(*parsed),
              (std::vector<std::string>{"tests/w.CSV", "tests/d.csv"}));
}

/// Answers to the settings dialog that the tool refuses: OK runs, Cancel discards (Yes only), a
/// known field with the members of its kind, a combo box's item, a fixed seed with an integer, an
/// edit of one kind.
constexpr std::array<std::string_view, 16> kRefusedSettingsAnswers{
    R"({"button": "apply"})",
    R"({"button": "cancel", "run": {"button": "cancel"}})",
    R"({"button": "ok", "discard": {"button": "yes"}})",
    R"({"button": "cancel", "discard": {"button": "no"}})",
    R"({"button": "ok", "run": {"button": "ok"}})",
    R"({"edits": [{"field": "Foo", "text": "1"}], "button": "ok"})",
    R"({"edits": [{"field": "TimeStep", "item": "RK4"}], "button": "ok"})",
    R"({"edits": [{"field": "LaunchIntoWind", "text": "1"}], "button": "ok"})",
    R"({"edits": [{"field": "SimulationStepperMethodChoice", "item": "EULER"}], "button": "ok"})",
    R"({"edits": [{"name": ""}], "button": "ok"})",
    R"({"edits": [{"tab": "plot"}], "button": "ok"})",
    R"({"edits": [{"randomSeed": {"fixed": true}}], "button": "ok"})",
    R"({"edits": [{"randomSeed": {"fixed": true, "text": "1.5"}}], "button": "ok"})",
    R"({"edits": [{"randomSeed": {"fixed": false, "text": "1"}}], "button": "ok"})",
    R"({"edits": [{"defaults": "reset"}], "button": "ok"})",
    R"({"edits": [{"name": "a", "tab": "warnings"}], "button": "ok"})",
};

/// Edits of the wind editor, its import and the lookup dialog that the tool refuses (bug 37: a
/// chosen file without the extension .csv).
constexpr std::array<std::string_view, 13> kRefusedDialogEdits{
    R"({"windLevels": {"button": "ok"}})",
    R"({"windLevels": {"edits": [{"addLevel": true, "deleteRow": 1}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"addLevel": false}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"cell": {"row": 0, "column": "height", "text": "1"}}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"cell": {"row": 0, "column": "speed"}}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"importLevels": {"file": {"button": "cancel"}}}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"importLevels": {"settings": {"separator": "", "button": "ok"}}}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"importLevels": {"settings": {"columns": {"height": {}}, "button": "ok"}}}], "button": "close"}})",
    R"({"windLevels": {"edits": [{"importLevels": {"settings": {"button": "ok"}, "file": {"file": "tests/w.txt", "button": "open"}}}], "button": "close"}})",
    R"({"lookup": {"drag": [{"reload": true}], "button": "ok"}})",
    R"({"lookup": {"drag": [{"load": {"button": "open"}}], "button": "ok"}})",
    R"({"lookup": {"drag": [{"load": {"file": "/etc/passwd", "button": "open"}}], "button": "ok"}})",
    R"({"lookup": {"drag": [], "button": "close"}})",
};

/// The answers of @p answers (to the settings dialog) that an Edit simulation step takes.
[[nodiscard]] std::vector<std::string_view> acceptedSettingsAnswers(
    std::span<const std::string_view> answers)
{
    std::vector<std::string_view> accepted;
    for (const std::string_view answer : answers)
    {
        if (scenarioError(
                scenarioV3(std::format(R"({{"step": "editSimulation", "answer": {}}})", answer)))
                .empty())
        {
            accepted.push_back(answer);
        }
    }
    return accepted;
}

/// The edits of @p edits (of the settings dialog) that an Edit simulation step takes.
[[nodiscard]] std::vector<std::string_view> acceptedSettingsEdits(
    std::span<const std::string_view> edits)
{
    std::vector<std::string_view> accepted;
    for (const std::string_view edit : edits)
    {
        if (scenarioError(
                scenarioV3(std::format(
                    R"({{"step": "editSimulation", "answer": {{"edits": [{}], "button": "ok"}}}})",
                    edit)))
                .empty())
        {
            accepted.push_back(edit);
        }
    }
    return accepted;
}

TEST(GuiScenarioReaderV3, RefusesWhatTheToolRefuses)
{
    // The steps and the tab of version 3 in version 2, a step of version 1 in version 3.
    EXPECT_NE(scenarioError(R"({"schema": "gui-scenario", "schemaVersion": 2, "title": "t",
        "steps": [{"step": "open", "file": "a.ork"}, {"step": "copySimulations"}]})"),
              "");
    EXPECT_NE(scenarioError(R"({"schema": "gui-scenario", "schemaVersion": 2, "title": "t",
        "steps": [{"step": "open", "file": "a.ork"}, {"step": "tab", "tab": "simulations"}]})"),
              "");
    EXPECT_NE(scenarioError(scenarioV3(R"({"step": "copy"})")), "");
    // The tab's steps: their members, a known column, a row with a known key.
    EXPECT_NE(scenarioError(scenarioV3(R"({"step": "editSimulation"})")), "");
    EXPECT_NE(scenarioError(
                  scenarioV3(R"({"step": "columnVisibility", "column": "x", "visible": true})")),
              "");
    EXPECT_NE(scenarioError(scenarioV3(
                  R"({"step": "selectSimulations", "rows": [{"row": 0, "with": "alt"}]})")),
              "");
    EXPECT_NE(
        scenarioError(scenarioV3(R"({"step": "pasteSimulations", "answer": {"button": "ok"}})")),
        "");
    // The confirmations: their buttons, a "Do not ask me again" that is true.
    EXPECT_NE(
        scenarioError(scenarioV3(R"({"step": "deleteSimulations", "answer": {"button": "yes"}})")),
        "");
    EXPECT_NE(
        scenarioError(scenarioV3(
            R"({"step": "deleteSimulations", "answer": {"button": "ok", "doNotAskAgain": false}})")),
        "");
    // The settings dialog, the wind editor, its import and the lookup dialog.
    EXPECT_EQ(acceptedSettingsAnswers(kRefusedSettingsAnswers), std::vector<std::string_view>{});
    EXPECT_EQ(acceptedSettingsEdits(kRefusedDialogEdits), std::vector<std::string_view>{});
    // And what it takes.
    EXPECT_EQ(
        scenarioError(scenarioV3(
            R"({"step": "editSimulation", "answer": {"edits": [{"lookup": {"drag": [], "button": "ok"}}], "button": "ok"}})")),
        "");
}

// =================================================================== comparing a GUI's records

/// The record of step @p step of @p reference; a test failure and an empty record when it has
/// none.
[[nodiscard]] GuiStepRecord simulationsRecordAt(const GuiReference& reference, std::size_t step)
{
    if (step >= reference.steps.size())
    {
        ADD_FAILURE() << "the reference has no step " << step;
        return {};
    }
    return reference.steps[step];
}

TEST(GuiRecordComparisonV3, ARecordAgreesWithItself)
{
    std::vector<std::size_t> differing;
    for (const GuiStepRecord& record : simulationsReference().steps)
    {
        if (!QtRocket::Test::guiRecordDifferences(record, record).empty())
        {
            differing.push_back(record.index);
        }
    }
    EXPECT_EQ(differing, std::vector<std::size_t>{});
}

// Changes of a record of version 3, each of one member.
[[nodiscard]] GuiStepRecord selectAnotherRow(GuiStepRecord record)
{
    QtRocket::Test::GuiSimulationsTabState tab = tabOf(record);
    tab.selectedRows.push_back(1);
    record.simulationsTab = std::move(tab);
    return record;
}
[[nodiscard]] GuiStepRecord toggleCopy(GuiStepRecord record)
{
    QtRocket::Test::GuiSimulationsTabState tab = tabOf(record);
    tab.actions["copySimulation"]              = !tab.actions["copySimulation"];
    record.simulationsTab                      = std::move(tab);
    return record;
}
[[nodiscard]] GuiStepRecord renameTheFirstRow(GuiStepRecord record)
{
    QtRocket::Test::GuiSimulationsTabState tab = tabOf(record);
    if (!tab.rows.empty() && !tab.rows.front().cells.empty())
    {
        tab.rows.front().cells.front() = "another name";
    }
    record.simulationsTab = std::move(tab);
    return record;
}
[[nodiscard]] GuiStepRecord changeAnOption(GuiStepRecord record)
{
    if (!record.simulations.empty() && !record.simulations.front().options.empty())
    {
        record.simulations.front().options.front() += ' ';
    }
    return record;
}
[[nodiscard]] GuiStepRecord changeTheSettingsTitle(GuiStepRecord record)
{
    if (!record.questions.empty())
    {
        record.questions.front().settingsAnswered.title += '*';
    }
    return record;
}
[[nodiscard]] GuiStepRecord changeAWindLine(GuiStepRecord record)
{
    if (record.questions.size() > 1 && !record.questions[1].windAnswered.lines.empty())
    {
        record.questions[1].windAnswered.lines.back() += ' ';
    }
    return record;
}
[[nodiscard]] GuiStepRecord changeTheCsvSeparator(GuiStepRecord record)
{
    if (record.questions.size() > 3)
    {
        record.questions[3].csvAnswered.separator = ",";
    }
    return record;
}
[[nodiscard]] GuiStepRecord forgetTheChosenFile(GuiStepRecord record)
{
    if (record.questions.size() > 4)
    {
        record.questions[4].file = std::nullopt;
    }
    return record;
}
[[nodiscard]] GuiStepRecord putTextOnTheClipboard(GuiStepRecord record)
{
    record.clipboardText = "Status";
    return record;
}
[[nodiscard]] GuiStepRecord forgetTheToolFixes(GuiStepRecord record)
{
    record.toolFixes.clear();
    return record;
}

/// A change of a record.
using RecordChange = GuiStepRecord (*)(GuiStepRecord);

/// The numbers of differences guiRecordDifferences() finds between @p record and its changes by
/// @p changes.
[[nodiscard]] std::vector<std::size_t> differenceCounts(const GuiStepRecord&          record,
                                                        std::span<const RecordChange> changes)
{
    std::vector<std::size_t> counts;
    counts.reserve(changes.size());
    for (const RecordChange change : changes)
    {
        counts.push_back(QtRocket::Test::guiRecordDifferences(record, change(record)).size());
    }
    return counts;
}

TEST(GuiRecordComparisonV3, EveryMemberOfVersionThreeIsCompared)
{
    // Step 26: the space-separated wind import, with the settings dialog, the wind editor, its
    // overwrite questions, the CSV import settings and its chooser, and the run dialog.
    const GuiStepRecord record = simulationsRecordAt(simulationsReference(), 26);
    EXPECT_EQ(questionNamesOf(record.questions),
              (std::vector<std::string_view>{"simulation", "windLevels", "confirm",
                                             "csvImportSettings", "openFile", "confirm", "run"}));
    EXPECT_FALSE(tabOf(record).rows.empty());
    EXPECT_FALSE(record.simulations.empty());
    constexpr std::array<RecordChange, 9> kChanges{
        selectAnotherRow,       toggleCopy,      renameTheFirstRow,     changeAnOption,
        changeTheSettingsTitle, changeAWindLine, changeTheCsvSeparator, forgetTheChosenFile,
        putTextOnTheClipboard};
    EXPECT_EQ(differenceCounts(record, kChanges), std::vector<std::size_t>(kChanges.size(), 1U));
    // The tool's fixes are not compared: they tell where the record came from.
    EXPECT_FALSE(record.toolFixes.empty());
    EXPECT_EQ(QtRocket::Test::guiRecordDifferences(record, forgetTheToolFixes(record)),
              std::vector<std::string>{});
}

/// @p record with OpenRocket's Paste enabled.
[[nodiscard]] GuiStepRecord enablePaste(GuiStepRecord record)
{
    QtRocket::Test::GuiSimulationsTabState tab = tabOf(record);
    tab.actions["pasteSimulation"]             = true;
    record.simulationsTab                      = std::move(tab);
    return record;
}

TEST(GuiRecordComparisonV3, TheExpectedRecordDropsTheMonteCarloActionAndFixesPaste)
{
    // Step 29: the cancelled New simulation; no row selected, two simulations on the clipboard.
    const GuiStepRecord record = simulationsRecordAt(simulationsReference(), 29);
    EXPECT_TRUE(tabOf(record).selectedRows.empty());
    EXPECT_EQ(tabOf(record).clipboardSimulations.size(), 2U);
    EXPECT_FALSE(tabOf(record).actions.at("pasteSimulation"));
    EXPECT_TRUE(tabOf(record).actions.contains("landingDispersion"));
    std::string                                  problems;
    const QtRocket::Test::GuiSimulationsTabState expected =
        tabOf(QtRocket::Test::guiExpectedRecord(kGuiSimulationsScenario, record, problems));
    EXPECT_EQ(problems, "");
    EXPECT_TRUE(expected.actions.at("pasteSimulation"));
    EXPECT_FALSE(expected.actions.contains("landingDispersion"));
    EXPECT_EQ(expected.actions.size(), 11U);
    // A Paste that is not OpenRocket's rule is a problem, and stays.
    std::string                                  stale;
    const QtRocket::Test::GuiSimulationsTabState kept = tabOf(
        QtRocket::Test::guiExpectedRecord(kGuiSimulationsScenario, enablePaste(record), stale));
    EXPECT_NE(stale, "");
    EXPECT_TRUE(kept.actions.at("pasteSimulation"));
}

/// The summaries and tool tips of the lookup questions of @p record, as they opened and as their
/// answers left them, that guiSortedLookupColumns() changes; @p lookups counts the questions.
[[nodiscard]] std::vector<std::string> unsortedLookupSummaries(const GuiStepRecord& record,
                                                               std::size_t&         lookups)
{
    std::vector<std::string> unsorted;
    for (const GuiQuestion& question : record.questions)
    {
        if (question.kind != GuiQuestionKind::LOOKUP)
        {
            continue;
        }
        ++lookups;
        for (const QtRocket::Test::GuiLookupState* const state :
             {&question.lookupShown, &question.lookupAnswered})
        {
            for (const std::string& text :
                 {state->drag.summary, state->drag.summaryToolTip.value_or(""),
                  state->stability.summary, state->stability.summaryToolTip.value_or("")})
            {
                if (QtRocket::Test::guiSortedLookupColumns(text) != text)
                {
                    unsorted.push_back(text);
                }
            }
        }
    }
    return unsorted;
}

TEST(GuiRecordComparisonV3, TheLookupHelpersWriteWhatTheRecordsHave)
{
    // A lookup table's file inside the repository, relative to it.
    const std::string root = QtRocket::Test::genericUtf8(QtRocket::Test::dataDir().parent_path());
    EXPECT_EQ(QtRocket::Test::guiRelativeLookupPaths(
                  {"drag=path:" + root + "/tests/d.csv table:[x] rows:null", "other"}),
              (std::vector<std::string>{"drag=path:tests/d.csv table:[x] rows:null", "other"}));
    EXPECT_EQ(QtRocket::Test::guiRelativeLookupPaths({"drag=path:null table:null rows:null"}),
              std::vector<std::string>{"drag=path:null table:null rows:null"});
    // The column list of a summary sorted, its ending kept.
    EXPECT_EQ(QtRocket::Test::guiSortedLookupColumns(
                  "<html>s.csv - Mach 0.1 - 0.9; columns: CP, CN, CM <i>(modified)</i></html>"),
              "<html>s.csv - Mach 0.1 - 0.9; columns: CM, CN, CP <i>(modified)</i></html>");
    EXPECT_EQ(QtRocket::Test::guiSortedLookupColumns("s.csv; columns: CP, CN (modified)"),
              "s.csv; columns: CN, CP (modified)");
    EXPECT_EQ(QtRocket::Test::guiSortedLookupColumns("s.csv; columns: b, a"),
              "s.csv; columns: a, b");
    EXPECT_EQ(QtRocket::Test::guiSortedLookupColumns("s.csv; columns: b,c, a"),
              "s.csv; columns: a, b,c");
    EXPECT_EQ(QtRocket::Test::guiSortedLookupColumns("<html>Using built-in Barrowman data</html>"),
              "<html>Using built-in Barrowman data</html>");
    // The recorded summaries are sorted already (step 5 loads a drag table).
    std::size_t lookups = 0;
    EXPECT_EQ(unsortedLookupSummaries(simulationsRecordAt(simulationsReference(), 5), lookups),
              std::vector<std::string>{});
    EXPECT_EQ(lookups, 1U);
}

}  // namespace
