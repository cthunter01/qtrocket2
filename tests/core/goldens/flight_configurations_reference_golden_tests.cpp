// The references of the GUI's scenarios of the Motors & Configuration tab (schema version 2:
// tests/data/goldens/gui/flight-configurations/ and multi-stage/, kGuiTabScenarios), written by
// tools/openrocket-gui-reference: OpenRocket's real FlightConfigurationPanel, its sub-panels and
// their dialogs run each scenario.json on a virtual display after OpenRocket's start-up with the
// bundled motors and component presets, and save the design; see GoldenGuiReference.h. The GUI's
// exit test FlightConfigurationsExitGolden drives the same scenarios through QtRocket's GUI and
// compares with the references; this file checks, without a GUI, that the references stay usable:
//
// - each scenario is well formed, opens a design file of the repository, and state.json is the
//   record of exactly this scenario and this input (the SHA-256 of their bytes, a record per step
//   of the same kind), made with the OpenRocket and the preset database of the other goldens,
//   after a start-up that loaded QtRocket's bundled databases; every record's tree digest is the
//   digest of its tree; every step that sets a field, and every operation of the tab but a
//   cancelled question, changed the tree; every question of a step is the one its answer
//   answers, and a chooser chose the motor of its answer, a motor of the bundled database;
// - QtRocket loads the input into the tree the open step recorded (describeRocket() without ids)
//   with OpenRocket's motor mounts;
// - the component paths of the records and of the scenario name components of the reload;
// - QtRocket loads the saved design (rocket.ork, the document of the archive whose entries
//   state.json lists with their SHA-256: the thrust curves of the design's motors besides it)
//   without warnings into the state OpenRocket's own load gives (designFileState(), line for line,
//   the component ids of the file included), with OpenRocket's masses, centres of mass and length;
// - the reload is the saved rocket (the last record's tree) but for what OpenRocket's own save
//   and reload changes (guiReloadChanges() of GoldenGuiReference.h);
// - the expected differences of kGuiExpectedDifferences name recorded questions with OpenRocket's
//   value, and OpenRocket's bug 1 holds in every record (its deployment and separation questions
//   all start on "this configuration", its Recovery and Stages tables shade no cell), so that
//   guiExpectedRecord() makes every record QtRocket's without a problem; the overrides the
//   Recovery and Stages cells record are those of the record's tree;
// - the fixes of OpenRocket's bugs 5 and 6 that the tool makes in OpenRocket's run show in the
//   records kGuiToolFixes lists, and in no other.
// The parser of version 2 itself is checked on small scenarios (GuiScenarioReader), and the
// comparison of another GUI's records with these, which the exit test makes (guiRecordDifferences()
// and guiExpectedRecord() of GoldenGuiReference.h), on the records (GuiRecordComparison).

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
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
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Uuid.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DesignFileState.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GuiReferenceEnvironment.h"
#include "motor/TestMotorDatabase.h"
#include "rocket/preset/TestPresetDatabase.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::ErrorCode;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Uuid;
using QtRocket::Test::DefaultUnitsGuard;
using QtRocket::Test::GuiAnswer;
using QtRocket::Test::GuiComponentPath;
using QtRocket::Test::guiComponentPathOf;
using QtRocket::Test::guiComponentPathText;
using QtRocket::Test::GuiExpectedDifference;
using QtRocket::Test::GuiMountRow;
using QtRocket::Test::GuiQuestion;
using QtRocket::Test::GuiQuestionKind;
using QtRocket::Test::GuiReference;
using QtRocket::Test::GuiReferenceEnvironment;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::guiStepName;
using QtRocket::Test::GuiStepRecord;
using QtRocket::Test::resolveGuiComponentPath;

/// The contents of @p file; a test failure and "" when it cannot be read.
[[nodiscard]] std::string textOf(const std::filesystem::path& file)
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
[[nodiscard]] std::string sha256Of(const std::filesystem::path& file)
{
    return QtRocket::sha256Hex(QtRocket::stringToBytes(textOf(file)));
}

[[nodiscard]] Result<GuiScenario> loadScenario(std::string_view scenario)
{
    return QtRocket::Test::parseGuiScenario(
        textOf(QtRocket::Test::guiScenarioDir(scenario) / "scenario.json"));
}

[[nodiscard]] Result<GuiReference> loadReference(std::string_view scenario)
{
    return QtRocket::Test::parseGuiReference(
        textOf(QtRocket::Test::guiScenarioDir(scenario) / "state.json"));
}

[[nodiscard]] std::filesystem::path referenceFile(std::string_view scenario)
{
    return QtRocket::Test::guiScenarioDir(scenario) / "rocket.ork";
}

/// The design file the scenario's open step names; a test failure and "" when it names none.
[[nodiscard]] std::filesystem::path inputFile(const GuiScenario& scenario)
{
    if (scenario.steps.empty() || scenario.steps.front().kind != GuiStepKind::OPEN)
    {
        ADD_FAILURE() << "the scenario opens no file";
        return {};
    }
    const Result<std::filesystem::path> file =
        QtRocket::Test::guiRepositoryFile(scenario.steps.front().file);
    if (!file)
    {
        ADD_FAILURE() << file.error().toString();
        return {};
    }
    return *file;
}

/// A design loaded by QtRocket in the environment of the references of version 2.
struct GuiLoad
{
    GuiReferenceEnvironment environment{GuiReferenceEnvironment::Databases::BUNDLED};
    DefaultUnitsGuard       units;
    Result<LoadedDocument>  loaded;
    explicit GuiLoad(const std::filesystem::path& file)
      : loaded(GeneralRocketLoader(environment.get().context()).load(file))
    {
    }
    ~GuiLoad() = default;

    GuiLoad(const GuiLoad&)            = delete;
    GuiLoad& operator=(const GuiLoad&) = delete;
    GuiLoad(GuiLoad&&)                 = delete;
    GuiLoad& operator=(GuiLoad&&)      = delete;

    [[nodiscard]] const Rocket& rocket() const { return loaded->document->getRocket(); }
    /// describeRocket() with no known id, as the records print the tree.
    [[nodiscard]] std::vector<std::string> tree()
    {
        return QtRocket::Test::describeRocket(rocket(), environment.get().preferences(),
                                              std::set<Uuid>{});
    }
};

/// The kinds of the scenario's steps by name.
[[nodiscard]] std::vector<std::string_view> scenarioStepNames(const GuiScenario& scenario)
{
    std::vector<std::string_view> names;
    names.reserve(scenario.steps.size());
    for (const GuiScenarioStep& step : scenario.steps)
    {
        names.push_back(guiStepName(step.kind));
    }
    return names;
}

/// The kinds of the records by name.
[[nodiscard]] std::vector<std::string_view> recordStepNames(const GuiReference& reference)
{
    std::vector<std::string_view> names;
    names.reserve(reference.steps.size());
    for (const GuiStepRecord& record : reference.steps)
    {
        names.push_back(guiStepName(record.kind));
    }
    return names;
}

/// The records whose index is not their place in the list.
[[nodiscard]] std::vector<std::size_t> misplacedRecords(const GuiReference& reference)
{
    std::vector<std::size_t> misplaced;
    for (std::size_t at = 0; at < reference.steps.size(); at++)
    {
        if (reference.steps[at].index != at)
        {
            misplaced.push_back(at);
        }
    }
    return misplaced;
}

/// The records whose tree digest is not the digest of their tree, by index.
[[nodiscard]] std::vector<std::size_t> wrongDigests(const GuiReference& reference)
{
    std::vector<std::size_t> wrong;
    for (const GuiStepRecord& record : reference.steps)
    {
        if (record.tree && QtRocket::Test::guiTreeSha256(*record.tree) != record.treeSha256)
        {
            wrong.push_back(record.index);
        }
    }
    return wrong;
}

/// Whether the record of a step of kind @p kind has its tree: after the steps that end an action
/// (README.md of the tool: open, ok, button, undo, redo, chooseFile) and after every operation
/// of the tab.
[[nodiscard]] bool recordsTheTree(GuiStepKind kind) noexcept
{
    return kind == GuiStepKind::OPEN || kind == GuiStepKind::OK || kind == GuiStepKind::BUTTON ||
           kind == GuiStepKind::UNDO || kind == GuiStepKind::REDO ||
           kind == GuiStepKind::CHOOSE_FILE || QtRocket::Test::isGuiTabOperation(kind);
}

/// The records that have a tree where none belongs, or none where one does, by index.
[[nodiscard]] std::vector<std::size_t> misplacedTrees(const GuiReference& reference)
{
    std::vector<std::size_t> misplaced;
    for (const GuiStepRecord& record : reference.steps)
    {
        if (record.tree.has_value() != recordsTheTree(record.kind))
        {
            misplaced.push_back(record.index);
        }
    }
    return misplaced;
}

/// Whether @p step answers its question with Cancel.
[[nodiscard]] bool isCancelled(const GuiScenarioStep& step) noexcept
{
    return step.answer.has_value() && step.answer->button == "cancel";
}

/// The steps that should change the tree and did not, or should not and did, by index: a field
/// step (type, choose, check, editPoint, insertPoint) changes it; so does an operation of the tab
/// but for a question cancelled (New excepted, whose new configuration is a change), which leaves
/// it as it was.
[[nodiscard]] std::vector<std::size_t> wrongTreeChanges(const GuiScenario&  scenario,
                                                        const GuiReference& reference)
{
    std::vector<std::size_t> wrong;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 1; at < steps; at++)
    {
        const GuiScenarioStep& step = scenario.steps[at];
        const bool field     = step.kind == GuiStepKind::TYPE || step.kind == GuiStepKind::CHOOSE ||
                               step.kind == GuiStepKind::CHECK ||
                               step.kind == GuiStepKind::EDIT_POINT ||
                               step.kind == GuiStepKind::INSERT_POINT;
        const bool operation = QtRocket::Test::isGuiTabOperation(step.kind);
        if (!field && !operation)
        {
            continue;
        }
        const bool changed   = reference.steps[at].treeSha256 != reference.steps[at - 1].treeSha256;
        const bool cancelled = isCancelled(step) && step.kind != GuiStepKind::NEW_CONFIGURATION;
        if (changed == cancelled)
        {
            wrong.push_back(at);
        }
    }
    return wrong;
}

/// The sub-tab of the tab before step @p at (the record before it), "" when there is none.
[[nodiscard]] std::string configTabBefore(const GuiReference& reference, std::size_t at)
{
    if (at == 0 || at > reference.steps.size())
    {
        return {};
    }
    const std::optional<QtRocket::Test::GuiFlightConfigurationState>& state =
        reference.steps[at - 1].flightConfiguration;
    return state ? state->configTab : std::string();
}

/// The question the answer of step @p at answers first: its kind's, for New the current
/// sub-tab's (MotorConfigurationPanel, RecoveryConfigurationPanel, SeparationConfigurationPanel).
[[nodiscard]] std::optional<GuiQuestionKind> expectedQuestion(const GuiReference&    reference,
                                                              const GuiScenarioStep& step)
{
    switch (step.kind)
    {
        case GuiStepKind::RENAME_CONFIGURATION:
            return GuiQuestionKind::RENAME;
        case GuiStepKind::SELECT_MOTOR:
            return GuiQuestionKind::MOTOR;
        case GuiStepKind::SELECT_IGNITION:
            return GuiQuestionKind::IGNITION;
        case GuiStepKind::SELECT_DEPLOYMENT:
            return GuiQuestionKind::DEPLOYMENT;
        case GuiStepKind::SELECT_SEPARATION:
            return GuiQuestionKind::SEPARATION;
        case GuiStepKind::NEW_CONFIGURATION:
        {
            const std::string tab = configTabBefore(reference, step.index);
            if (tab == "recovery")
            {
                return GuiQuestionKind::DEPLOYMENT;
            }
            if (tab == "stages")
            {
                return GuiQuestionKind::SEPARATION;
            }
            return GuiQuestionKind::MOTOR;
        }
        default:
            return std::nullopt;
    }
}

/// What is wrong with the questions of the record of @p step: a step with an answer asked its
/// question first (the rename's "Save as default" its message after it), one without asked none.
[[nodiscard]] std::vector<std::string> questionProblems(const GuiReference&    reference,
                                                        const GuiScenarioStep& step)
{
    std::vector<std::string>             problems;
    const std::vector<GuiQuestion>&      questions = reference.steps[step.index].questions;
    const std::optional<GuiQuestionKind> expected  = expectedQuestion(reference, step);
    if (!step.answer.has_value())
    {
        if (!questions.empty())
        {
            problems.push_back(std::format("step {} has no answer but asked", step.index));
        }
        return problems;
    }
    if (questions.empty() || !expected || questions.front().kind != *expected)
    {
        problems.push_back(std::format("step {} did not ask its question first", step.index));
        return problems;
    }
    const std::size_t expectedCount = step.answer->saveAsDefault ? 2 : 1;
    if (questions.size() != expectedCount ||
        (step.answer->saveAsDefault && questions.back().kind != GuiQuestionKind::MESSAGE))
    {
        problems.push_back(std::format("step {} asked {} questions", step.index, questions.size()));
    }
    return problems;
}

/// What is wrong with a chooser question answered by @p answer: what the answer chose (the search,
/// the motor set, the curve, the delay text) is what the chooser showed after it.
[[nodiscard]] std::vector<std::string> chooserProblems(const GuiQuestion& question,
                                                       const GuiAnswer& answer, std::size_t index)
{
    std::vector<std::string> problems;
    const auto               expect = [&problems, index](bool ok, std::string_view what) {
        if (!ok)
        {
            problems.push_back(std::format("step {}: the chooser shows another {}", index, what));
        }
    };
    const QtRocket::Test::GuiChooserState& shown = question.answered;
    expect(!answer.search || shown.search == *answer.search, "search");
    expect(
        !answer.motor || (shown.motor && shown.motor->manufacturer == answer.motor->manufacturer &&
                          shown.motor->designation == answer.motor->designation),
        "motor");
    expect(!answer.curve || (shown.motor && shown.motor->digest == *answer.curve), "curve");
    expect(!answer.chooserDelay || shown.delayText == answer.chooserDelay, "delay");
    expect(answer.button != "ok" || shown.motor.has_value(), "motor (none)");
    return problems;
}

/// The questions of the records that do not belong to their steps, and the choosers that do not
/// show what their answers chose.
[[nodiscard]] std::vector<std::string> wrongQuestions(const GuiScenario&  scenario,
                                                      const GuiReference& reference)
{
    std::vector<std::string> wrong;
    const std::size_t        steps = std::min(scenario.steps.size(), reference.steps.size());
    for (std::size_t at = 0; at < steps; at++)
    {
        const GuiScenarioStep&         step     = scenario.steps[at];
        const std::vector<std::string> problems = questionProblems(reference, step);
        wrong.insert(wrong.end(), problems.begin(), problems.end());
        const std::vector<GuiQuestion>& questions = reference.steps[at].questions;
        if (problems.empty() && step.answer && !questions.empty() &&
            questions.front().kind == GuiQuestionKind::MOTOR)
        {
            const std::vector<std::string> chooser =
                chooserProblems(questions.front(), *step.answer, at);
            wrong.insert(wrong.end(), chooser.begin(), chooser.end());
        }
    }
    return wrong;
}

/// Whether the bundled motor database has the thrust curve @p motor (its digest) in the set of
/// its manufacturer (by its display name) and designation.
[[nodiscard]] bool isBundledMotor(const QtRocket::Test::GuiMotor& motor)
{
    for (const QtRocket::ThrustCurveMotorSet& set :
         QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        for (const auto& curve : set.getMotors())
        {
            if (curve->getDigest() == motor.digest &&
                curve->getManufacturer().getDisplayName() == motor.manufacturer &&
                curve->getDesignation() == motor.designation)
            {
                return true;
            }
        }
    }
    return false;
}

/// The motors the choosers of the records chose that the bundled database does not have, as
/// "<manufacturer> <designation> <digest>".
[[nodiscard]] std::vector<std::string> chosenMotorsNotBundled(const GuiReference& reference)
{
    std::vector<std::string> missing;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiQuestion& question : record.questions)
        {
            if (question.kind != GuiQuestionKind::MOTOR)
            {
                continue;
            }
            const std::array<const std::optional<QtRocket::Test::GuiMotor>*, 2> motors{
                &question.shown.motor, &question.answered.motor};
            for (const std::optional<QtRocket::Test::GuiMotor>* const motor : motors)
            {
                if (motor->has_value() && !isBundledMotor(**motor))
                {
                    missing.push_back(std::format("{} {} {}", (*motor)->manufacturer,
                                                  (*motor)->designation, (*motor)->digest));
                }
            }
        }
    }
    return missing;
}

/// The number of motors of the bundled database (in all its sets).
[[nodiscard]] std::size_t bundledMotorCount()
{
    std::size_t count = 0;
    for (const QtRocket::ThrustCurveMotorSet& set :
         QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        count += set.getMotorCount();
    }
    return count;
}

/// The digests of the motors of @p rocket's flight configurations, in every component that acts
/// as a motor mount (the thrust curves GeneralRocketSaver puts into the archive).
[[nodiscard]] std::set<std::string> motorDigests(const Rocket& rocket)
{
    std::set<std::string> digests;
    for (const RocketComponent& component : rocket.subtree(true))
    {
        const auto* const mount = dynamic_cast<const QtRocket::MotorMount*>(&component);
        if (mount == nullptr || !mount->isMotorMount())
        {
            continue;
        }
        for (const QtRocket::FlightConfigurationId& id : rocket.getIds())
        {
            const QtRocket::MotorConfiguration& configuration = mount->getMotorConfig(id);
            if (configuration.hasMotor())
            {
                digests.insert(configuration.getMotor()->getDigest());
            }
        }
    }
    return digests;
}

/// The thrust curve entries of the archive, "thrustcurves/<digest>.rse", by digest; an entry of
/// another form as itself.
[[nodiscard]] std::set<std::string> thrustCurveEntries(const GuiReference& reference)
{
    std::set<std::string> digests;
    for (const std::string& entry : reference.archiveEntries)
    {
        constexpr std::string_view kPrefix = "thrustcurves/";
        constexpr std::string_view kSuffix = ".rse";
        if (entry == "rocket.ork")
        {
            continue;
        }
        if (entry.starts_with(kPrefix) && entry.ends_with(kSuffix))
        {
            digests.insert(
                entry.substr(kPrefix.size(), entry.size() - kPrefix.size() - kSuffix.size()));
        }
        else
        {
            digests.insert(entry);
        }
    }
    return digests;
}

/// The names of @p digests that do not have the form of a motor digest.
[[nodiscard]] std::vector<std::string> malformedDigests(const std::set<std::string>& digests)
{
    std::vector<std::string> malformed;
    for (const std::string& digest : digests)
    {
        if (!QtRocket::Test::hasMotorDigestForm(digest))
        {
            malformed.push_back(digest);
        }
    }
    return malformed;
}

/// The motor mount table of @p rocket as the records have it: every MotorMount component in tree
/// order (MotorMountTableModel's), its name and whether it acts as a motor mount.
[[nodiscard]] std::vector<std::string> mountRows(const Rocket& rocket)
{
    std::vector<std::string> rows;
    for (const RocketComponent& component : rocket.subtree(false))
    {
        const auto* const mount = dynamic_cast<const QtRocket::MotorMount*>(&component);
        if (mount != nullptr)
        {
            rows.push_back(std::format("{} '{}' {}",
                                       guiComponentPathText(guiComponentPathOf(component)),
                                       component.getName(), mount->isMotorMount()));
        }
    }
    return rows;
}

/// The motor mount table of @p record in the lines of mountRows().
[[nodiscard]] std::vector<std::string> recordedMountRows(const GuiStepRecord& record)
{
    std::vector<std::string> rows;
    if (record.flightConfiguration)
    {
        for (const GuiMountRow& row : record.flightConfiguration->motors.mounts)
        {
            rows.push_back(std::format("{} '{}' {}", guiComponentPathText(row.component), row.text,
                                       row.motorMount));
        }
    }
    return rows;
}

/// Every component path of @p scenario and @p reference: the selections, the components of the
/// open dialogs and the motor mount tables of the records, the rows selected, the cells' columns
/// and the mounts of the scenario.
[[nodiscard]] std::vector<GuiComponentPath> everyPath(const GuiScenario&  scenario,
                                                      const GuiReference& reference)
{
    std::vector<GuiComponentPath> paths;
    for (const GuiStepRecord& record : reference.steps)
    {
        paths.insert(paths.end(), record.selection.begin(), record.selection.end());
        if (record.windowComponent)
        {
            paths.push_back(*record.windowComponent);
        }
        if (record.flightConfiguration)
        {
            for (const GuiMountRow& row : record.flightConfiguration->motors.mounts)
            {
                paths.push_back(row.component);
            }
        }
    }
    for (const GuiScenarioStep& step : scenario.steps)
    {
        paths.insert(paths.end(), step.paths.begin(), step.paths.end());
        for (const QtRocket::Test::GuiCell& cell : step.cells)
        {
            if (cell.column)
            {
                paths.push_back(*cell.column);
            }
        }
        if (step.kind == GuiStepKind::MOUNT_CHECK)
        {
            paths.push_back(step.mount);
        }
    }
    return paths;
}

/// The paths of @p paths that name no component of @p rocket, or one whose path is another (as
/// text).
[[nodiscard]] std::vector<std::string> unresolvedPaths(const Rocket&                     rocket,
                                                       std::span<const GuiComponentPath> paths)
{
    std::vector<std::string> wrong;
    for (const GuiComponentPath& path : paths)
    {
        const Result<const RocketComponent*> component = resolveGuiComponentPath(rocket, path);
        if (!component)
        {
            wrong.push_back(guiComponentPathText(path) + " (none)");
        }
        else if (guiComponentPathOf(**component) != path)
        {
            wrong.push_back(std::format("{} -> {}", guiComponentPathText(path),
                                        guiComponentPathText(guiComponentPathOf(**component))));
        }
    }
    return wrong;
}

/// The lines of @p state that are warnings ("W <text>").
[[nodiscard]] std::vector<std::string> warningLinesOf(const std::vector<std::string>& state)
{
    std::vector<std::string> warnings;
    for (const std::string& line : state)
    {
        if (line.starts_with("W "))
        {
            warnings.push_back(line);
        }
    }
    return warnings;
}

/// What is wrong with the expected difference @p difference in @p reference: its record and
/// question exist, the question's member has OpenRocket's value, and QtRocket's is another.
[[nodiscard]] std::string differenceProblem(const GuiReference&          reference,
                                            const GuiExpectedDifference& difference)
{
    const std::string what = std::format("{} step {} question {} {}", difference.scenario,
                                         difference.step, difference.question, difference.member);
    if (difference.step >= reference.steps.size() ||
        difference.question >= reference.steps[difference.step].questions.size())
    {
        return what + ": no such question";
    }
    const GuiQuestion& question = reference.steps[difference.step].questions[difference.question];
    const std::optional<std::string> value =
        QtRocket::Test::guiQuestionMember(question, difference.member);
    if (!value || *value != difference.openRocket || difference.qtRocket == difference.openRocket)
    {
        return std::format("{}: OpenRocket's value is {}", what, value.value_or("(no member)"));
    }
    if (difference.bug == 1 && question.kind != GuiQuestionKind::DEPLOYMENT &&
        question.kind != GuiQuestionKind::SEPARATION)
    {
        return what + ": bug 1 is the deployment's and the separation's";
    }
    return {};
}

/// The deployment and separation questions of @p reference that do not start on "this
/// configuration", as "step <n>".
[[nodiscard]] std::vector<std::string> valueQuestionsNotOnThis(const GuiReference& reference)
{
    std::vector<std::string> wrong;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const GuiQuestion& question : record.questions)
        {
            if ((question.kind == GuiQuestionKind::DEPLOYMENT ||
                 question.kind == GuiQuestionKind::SEPARATION) &&
                question.scope != "this")
            {
                wrong.push_back(std::format("step {}", record.index));
            }
        }
    }
    return wrong;
}

/// The problems guiExpectedRecord() reports for the records of @p reference, the scenario
/// @p scenario's (an expected difference that names no recorded value, a cell OpenRocket shaded).
[[nodiscard]] std::string expectedRecordProblems(const GuiReference& reference,
                                                 std::string_view    scenario)
{
    std::string problems;
    for (const GuiStepRecord& record : reference.steps)
    {
        static_cast<void>(QtRocket::Test::guiExpectedRecord(scenario, record, problems));
    }
    return problems;
}

/// The number of times @p text holds @p part.
[[nodiscard]] std::size_t occurrences(std::string_view text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string_view::npos;
         at             = text.find(part, at + part.size()))
    {
        ++count;
    }
    return count;
}

/// What is wrong with the overrides of the table @p table (named @p name: "recovery", whose
/// tree lines hold " deploy=" and an entry " deploy[<id>]=" per override, or "stages", " sep=")
/// in the record @p record: in each column, the cells with an override are as many as the
/// overrides of the column's component in the record's tree (its line found by the column's
/// header, the component's name).
void addOverrideProblems(const GuiStepRecord& record, std::string_view name,
                         const QtRocket::Test::GuiConfigurationTable& table,
                         std::vector<std::string>&                    problems)
{
    const std::string_view          value    = name == "recovery" ? " deploy=" : " sep=";
    const std::string_view          override = name == "recovery" ? " deploy[" : " sep[";
    const std::vector<std::string>& tree     = record.tree.value_or(std::vector<std::string>{});
    for (std::size_t column = 1; column < table.header.size(); ++column)
    {
        const std::string        component = " '" + table.header[column] + "' ";
        std::vector<std::string> lines;
        std::ranges::copy_if(tree, std::back_inserter(lines), [&](const std::string& line) {
            return line.contains(component) && line.contains(value);
        });
        std::size_t cells = 0;
        for (const QtRocket::Test::GuiTableRow& row : table.rows)
        {
            cells += column <= row.cells.size() && row.cells[column - 1].override ? 1U : 0U;
        }
        if (lines.size() != 1 || occurrences(lines.front(), override) != cells)
        {
            problems.push_back(
                std::format("step {} {} column {}: {} overrides in the cells, {} tree "
                            "lines",
                            record.index, name, column, cells, lines.size()));
        }
    }
}

/// What is wrong with the overrides the Recovery and Stages cells of @p reference record, in the
/// records that have their tree (addOverrideProblems()).
[[nodiscard]] std::vector<std::string> overrideProblems(const GuiReference& reference)
{
    std::vector<std::string> problems;
    for (const GuiStepRecord& record : reference.steps)
    {
        if (record.tree.has_value() && record.flightConfiguration.has_value())
        {
            addOverrideProblems(record, "recovery", record.flightConfiguration->recovery, problems);
            addOverrideProblems(record, "stages", record.flightConfiguration->stages, problems);
        }
    }
    return problems;
}

/// The start of the tree line @p line that names its component: its indentation, class and name
/// ("      InnerTube 'Inner Tube' "), as GuiToolFix::line has it.
[[nodiscard]] std::string_view componentOfLine(std::string_view line)
{
    const std::size_t axial = line.find(" axial=");
    return axial == std::string_view::npos ? line : line.substr(0, axial + 1);
}

/// Whether the fix @p fix holds in @p record.
[[nodiscard]] bool toolFixHolds(const QtRocket::Test::GuiToolFix& fix, const GuiStepRecord& record)
{
    if (fix.member == "undo" || fix.member == "redo")
    {
        return (fix.member == "undo" ? record.undo : record.redo).text == fix.value;
    }
    const std::vector<std::string>& tree = record.tree.value_or(std::vector<std::string>{});
    return fix.member == "tree" && std::ranges::any_of(tree, [&fix](const std::string& line) {
               return componentOfLine(line) == fix.line && line.contains(fix.value);
           });
}

/// What is wrong with the fixes kGuiToolFixes lists for @p scenario in @p reference: each must
/// hold in its record (toolFixHolds()), name its bug's value (an undo position of bug 5, a
/// default ignition other than OpenRocket's new one for bug 6).
[[nodiscard]] std::vector<std::string> toolFixProblems(const GuiReference& reference,
                                                       std::string_view    scenario)
{
    std::vector<std::string> problems;
    for (const QtRocket::Test::GuiToolFix& fix : QtRocket::Test::kGuiToolFixes)
    {
        if (fix.scenario != scenario)
        {
            continue;
        }
        const std::string what = std::format("step {} bug {} {} {}{}", fix.step, fix.bug,
                                             fix.member, fix.line, fix.value);
        const bool        names =
            fix.bug == 5 ? std::ranges::any_of(QtRocket::Test::kGuiToolUndoPositions,
                                               [&fix](std::string_view position) {
                                                   return fix.value.contains(position);
                                               })
                         : fix.bug == 6 && fix.value != QtRocket::Test::kGuiFreshDefaultIgnition;
        if (!names || fix.step >= reference.steps.size() ||
            !toolFixHolds(fix, reference.steps[fix.step]))
        {
            problems.push_back(what);
        }
    }
    return problems;
}

/// Whether kGuiToolFixes lists the fix of @p bug in the member @p member of step @p step of
/// @p scenario, with @p line and @p value.
[[nodiscard]] bool toolFixListed(std::string_view scenario, std::size_t step, int bug,
                                 std::string_view member, std::string_view line,
                                 std::string_view value)
{
    return std::ranges::any_of(
        QtRocket::Test::kGuiToolFixes, [&](const QtRocket::Test::GuiToolFix& fix) {
            return fix.scenario == scenario && fix.step == step && fix.bug == bug &&
                   fix.member == member && fix.line == line && fix.value == value;
        });
}

/// The fixes of bugs 5 and 6 that the records of @p reference show and kGuiToolFixes does not
/// list: an Undo or Redo text naming one of QtRocket's undo positions, and after an undo or a
/// redo a tree line whose default ignition is not OpenRocket's new one.
[[nodiscard]] std::vector<std::string> unlistedToolFixes(const GuiReference& reference,
                                                         std::string_view    scenario)
{
    std::vector<std::string> unlisted;
    for (const GuiStepRecord& record : reference.steps)
    {
        for (const auto& [member, action] : {std::pair{std::string_view("undo"), &record.undo},
                                             std::pair{std::string_view("redo"), &record.redo}})
        {
            const bool fix = std::ranges::any_of(
                QtRocket::Test::kGuiToolUndoPositions,
                [action](std::string_view position) { return action->text.contains(position); });
            if (fix && !toolFixListed(scenario, record.index, 5, member, "", action->text))
            {
                unlisted.push_back(
                    std::format("step {} {} '{}'", record.index, member, action->text));
            }
        }
        if (record.kind != GuiStepKind::UNDO && record.kind != GuiStepKind::REDO)
        {
            continue;
        }
        for (const std::string& line : record.tree.value_or(std::vector<std::string>{}))
        {
            const std::size_t at  = line.find(" ign=");
            const std::size_t end = at == std::string::npos ? at : line.find(' ', at + 1);
            const std::string ignition =
                at == std::string::npos ? std::string() : line.substr(at, end - at);
            if (!ignition.empty() && ignition != QtRocket::Test::kGuiFreshDefaultIgnition &&
                !toolFixListed(scenario, record.index, 6, "tree", componentOfLine(line), ignition))
            {
                unlisted.push_back(std::format("step {} tree {}{}", record.index,
                                               componentOfLine(line), ignition));
            }
        }
    }
    return unlisted;
}

/// The preset database commit of the goldens' manifest ("openrocket.presetDatabaseCommit").
[[nodiscard]] std::string manifestPresetCommit()
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

class FlightConfigurationsReferenceGolden : public ::testing::TestWithParam<std::string_view>
{
protected:
    [[nodiscard]] static std::string_view scenario() { return GetParam(); }
};

/// The name of a test of @p param: the scenario's, with '_' for '-' (a test name is an identifier).
[[nodiscard]] std::string scenarioTestName(const ::testing::TestParamInfo<std::string_view>& param)
{
    std::string name(param.param);
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationsReferenceGolden,
                         ::testing::ValuesIn(QtRocket::Test::kGuiTabScenarios), scenarioTestName);

// ================================================================================== the files

TEST_P(FlightConfigurationsReferenceGolden, TheScenarioIsWellFormed)
{
    const Result<GuiScenario> loaded = loadScenario(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(loaded->schemaVersion, 2);
    EXPECT_FALSE(loaded->title.empty());
    ASSERT_FALSE(loaded->steps.empty());
    EXPECT_EQ(loaded->steps.front().kind, GuiStepKind::OPEN);
    EXPECT_TRUE(std::filesystem::is_regular_file(inputFile(*loaded)));
    // It ends with the save: the file chooser of Save as, after the design information dialog.
    EXPECT_EQ(loaded->steps.back().kind, GuiStepKind::CHOOSE_FILE);
}

TEST_P(FlightConfigurationsReferenceGolden, TheReferenceIsTheRecordOfThisScenario)
{
    const Result<GuiScenario>  loaded    = loadScenario(scenario());
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(reference->schemaVersion, 2);
    EXPECT_EQ(reference->scenarioFile, "scenario.json");
    EXPECT_EQ(reference->scenarioSha256,
              sha256Of(QtRocket::Test::guiScenarioDir(scenario()) / "scenario.json"))
        << "scenario.json changed after the reference was made: run "
           "tools/openrocket-gui-reference/generate.sh";
    EXPECT_EQ(reference->scenarioTitle, loaded->title);
    EXPECT_EQ(reference->scenarioSteps, loaded->steps.size());
    EXPECT_EQ(recordStepNames(*reference), scenarioStepNames(*loaded));
    EXPECT_EQ(misplacedRecords(*reference), std::vector<std::size_t>{});
    // The input as it was when the reference was made.
    ASSERT_FALSE(loaded->steps.empty());
    EXPECT_EQ(reference->inputFile, loaded->steps.front().file);
    EXPECT_EQ(reference->inputSha256, sha256Of(inputFile(*loaded)))
        << "the scenario's design file changed after the reference was made";
}

TEST_P(FlightConfigurationsReferenceGolden, TheReferenceComesFromTheOpenRocketOfTheGoldens)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    const auto manifest = QtRocket::Test::loadGoldenManifest();
    ASSERT_TRUE(manifest.has_value()) << manifest.error().toString();
    EXPECT_EQ(reference->openRocketCommit, manifest->openrocketCommit);
    EXPECT_EQ(reference->openRocketVersion, manifest->openrocketVersion);
    EXPECT_EQ(reference->presetDatabaseCommit, manifestPresetCommit());
    EXPECT_FALSE(reference->dirty);
    EXPECT_TRUE(reference->javaVersion.starts_with("17."));
    // OpenRocket's start-up loaded the databases QtRocket bundles: data/motors/initial_motors.db
    // into motor sets as QtRocket's database groups it, and the presets of data/components.
    EXPECT_EQ(static_cast<std::size_t>(reference->startup.motorSets),
              QtRocket::Test::bundledMotorDatabase().getMotorSets().size());
    EXPECT_EQ(static_cast<std::size_t>(reference->startup.motors), bundledMotorCount());
    EXPECT_EQ(static_cast<std::size_t>(reference->startup.presets),
              QtRocket::Test::bundledPresetDatabase().size());
}

TEST_P(FlightConfigurationsReferenceGolden, TheArchiveHoldsTheDesignAndTheThrustCurvesOfItsMotors)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(reference->referenceFile, "rocket.ork");
    EXPECT_EQ(reference->savedAs, "rocket.ork");
    ASSERT_FALSE(reference->archiveEntries.empty());
    // The document first (OpenRocket loads an archive by its first entry), then the thrust curves
    // of the design's motors: no decal, no preview.
    EXPECT_EQ(reference->archiveEntries.front(), "rocket.ork");
    const std::set<std::string> entries(reference->archiveEntries.begin(),
                                        reference->archiveEntries.end());
    std::set<std::string>       digested;
    for (const auto& [entry, sha256] : reference->archiveSha256)
    {
        digested.insert(entry);
    }
    EXPECT_EQ(digested, entries);
    EXPECT_EQ(entries.size(), reference->archiveEntries.size());
    // rocket.ork is that entry, byte for byte.
    const auto document = reference->archiveSha256.find("rocket.ork");
    ASSERT_NE(document, reference->archiveSha256.end());
    EXPECT_EQ(document->second, sha256Of(referenceFile(scenario())));

    GuiLoad load(referenceFile(scenario()));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    const std::set<std::string> digests = motorDigests(load.rocket());
    EXPECT_FALSE(digests.empty());
    EXPECT_EQ(thrustCurveEntries(*reference), digests);
    EXPECT_EQ(malformedDigests(digests), std::vector<std::string>{});
}

TEST_P(FlightConfigurationsReferenceGolden, EveryTreeDigestIsTheDigestOfItsTree)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    EXPECT_TRUE(reference->steps.back().saved);
    EXPECT_EQ(misplacedTrees(*reference), std::vector<std::size_t>{});
    EXPECT_EQ(wrongDigests(*reference), std::vector<std::size_t>{});
}

TEST_P(FlightConfigurationsReferenceGolden, EveryFieldStepAndEveryOperationChangesTheTree)
{
    // A step that changes nothing would also pass a GUI that does nothing; the tool refuses such
    // a scenario, and this guards the committed record. A cancelled question changes nothing,
    // which is what it checks.
    const Result<GuiScenario>  loaded    = loadScenario(scenario());
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(wrongTreeChanges(*loaded, *reference), std::vector<std::size_t>{});
}

TEST_P(FlightConfigurationsReferenceGolden, EveryQuestionBelongsToItsStep)
{
    const Result<GuiScenario>  loaded    = loadScenario(scenario());
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(wrongQuestions(*loaded, *reference), std::vector<std::string>{});
    EXPECT_EQ(chosenMotorsNotBundled(*reference), std::vector<std::string>{});
}

TEST_P(FlightConfigurationsReferenceGolden, EveryRecordShowsTheTab)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    std::vector<std::size_t> without;
    for (const GuiStepRecord& record : reference->steps)
    {
        if (!record.flightConfiguration || record.flightConfiguration->configTabs.size() != 3)
        {
            without.push_back(record.index);
        }
    }
    EXPECT_EQ(without, std::vector<std::size_t>{});
}

// ============================================================================ the input design

TEST_P(FlightConfigurationsReferenceGolden, TheOpenStepGivesQtRocketsLoadOfTheInput)
{
    // The exit test opens the input through QtRocket's GUI: its tree is the one OpenRocket's open
    // step recorded, with OpenRocket's motor mounts.
    const Result<GuiScenario>  loaded    = loadScenario(scenario());
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    const GuiStepRecord& open = reference->steps.front();
    ASSERT_TRUE(open.tree.has_value());
    GuiLoad load(inputFile(*loaded));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    EXPECT_EQ(QtRocket::Test::warningLines(load.loaded->warnings), "");
    EXPECT_EQ(load.tree(), open.tree.value_or(std::vector<std::string>{}));
    EXPECT_EQ(mountRows(load.rocket()), recordedMountRows(open));
    EXPECT_TRUE(open.saved);
}

// ========================================================================== the component paths

TEST_P(FlightConfigurationsReferenceGolden, TheRecordedPathsNameTheirComponentsInTheReload)
{
    // The scenarios of version 2 add, move and remove no component (no add, copy or paste step):
    // every path of a record or of a step names a component of the saved design.
    const Result<GuiScenario>  loaded    = loadScenario(scenario());
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    GuiLoad load(referenceFile(scenario()));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    const std::vector<GuiComponentPath> paths = everyPath(*loaded, *reference);
    EXPECT_GT(paths.size(), loaded->steps.size());
    EXPECT_EQ(unresolvedPaths(load.rocket(), paths), std::vector<std::string>{});
    // The last record's motor mount table is the saved design's.
    ASSERT_FALSE(reference->steps.empty());
    EXPECT_EQ(mountRows(load.rocket()), recordedMountRows(reference->steps.back()));
}

// ================================================================================== the design

TEST_P(FlightConfigurationsReferenceGolden, TheSavedDesignLoadsWithoutWarnings)
{
    GuiLoad load(referenceFile(scenario()));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    EXPECT_EQ(QtRocket::Test::warningLines(load.loaded->warnings), "");
}

TEST_P(FlightConfigurationsReferenceGolden, TheSavedDesignLoadsIntoOpenRocketsState)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    GuiReferenceEnvironment        environment(GuiReferenceEnvironment::Databases::BUNDLED);
    const std::vector<std::string> state =
        QtRocket::Test::designFileState(environment.get(), referenceFile(scenario()),
                                        QtRocket::Test::StateDetail::WITH_DOCUMENT_PREFERENCES);
    const std::vector<std::string_view> expected(reference->loadedLines.begin(),
                                                 reference->loadedLines.end());
    EXPECT_EQ(QtRocket::Test::firstDifference(expected, state), "");
    // OpenRocket's load gave no warning either.
    EXPECT_EQ(warningLinesOf(reference->loadedLines), std::vector<std::string>{});
}

TEST_P(FlightConfigurationsReferenceGolden, TheSavedDesignHasOpenRocketsMassesAndLength)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    GuiLoad load(referenceFile(scenario()));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    const std::vector<std::string_view> expected(reference->loadedNumbers.begin(),
                                                 reference->loadedNumbers.end());
    EXPECT_EQ(QtRocket::Test::wrongNumbers(expected, QtRocket::Test::massAndLength(load.rocket())),
              "");
}

TEST_P(FlightConfigurationsReferenceGolden, TheReloadIsTheSavedRocketButForWhatTheReloadChanges)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    ASSERT_FALSE(reference->steps.empty());
    ASSERT_TRUE(reference->steps.back().tree.has_value());
    GuiLoad load(referenceFile(scenario()));
    ASSERT_TRUE(load.loaded.has_value()) << load.loaded.error().toString();
    std::string                    problems;
    const std::vector<std::string> reloaded = QtRocket::Test::guiReloadedTree(
        reference->steps.back().tree.value_or(std::vector<std::string>{}),
        QtRocket::Test::guiReloadChanges(scenario()), problems);
    EXPECT_EQ(problems, "");
    EXPECT_EQ(load.tree(), reloaded);
}

// ======================================================================= the expected differences

TEST_P(FlightConfigurationsReferenceGolden, TheExpectedDifferencesNameRecordedQuestions)
{
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    std::vector<std::string> problems;
    std::size_t              differences = 0;
    for (const GuiExpectedDifference& difference : QtRocket::Test::kGuiExpectedDifferences)
    {
        if (difference.scenario == scenario())
        {
            ++differences;
            if (std::string problem = differenceProblem(*reference, difference); !problem.empty())
            {
                problems.push_back(std::move(problem));
            }
        }
    }
    EXPECT_EQ(problems, std::vector<std::string>{});
    EXPECT_GT(differences, 0U);
    // Bug 1 as the records show it: OpenRocket's deployment and separation dialogs always start
    // on "this configuration" (kGuiExpectedDifferences lists where QtRocket's start on "all"),
    // and its Recovery and Stages tables shade no cell (guiExpectedRecord() shades those without
    // an override of their own, as QtRocket's tables do).
    EXPECT_EQ(valueQuestionsNotOnThis(*reference), std::vector<std::string>{});
    EXPECT_EQ(expectedRecordProblems(*reference, scenario()), "");
}

TEST_P(FlightConfigurationsReferenceGolden, TheValueCellsOverridesAreThoseOfTheTree)
{
    // The override a Recovery or Stages cell records (FlightConfigurableParameterSet.containsId(),
    // which QtRocket's shading shows) is the rocket's: per column as many as the tree's line of the
    // component has.
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(overrideProblems(*reference), std::vector<std::string>{});
}

TEST_P(FlightConfigurationsReferenceGolden, TheToolsFixesShowWhereTheListNamesThem)
{
    // The records are OpenRocket's with QtRocket's fixes of bugs 5 and 6 (the tool makes them):
    // kGuiToolFixes names every record where they show.
    const Result<GuiReference> reference = loadReference(scenario());
    ASSERT_TRUE(reference.has_value()) << reference.error().toString();
    EXPECT_EQ(toolFixProblems(*reference, scenario()), std::vector<std::string>{});
    EXPECT_EQ(unlistedToolFixes(*reference, scenario()), std::vector<std::string>{});
    EXPECT_GT(std::ranges::count(QtRocket::Test::kGuiToolFixes, scenario(),
                                 &QtRocket::Test::GuiToolFix::scenario),
              0);
}

// ================================================================================= the reader

/// A scenario of version 2 whose steps are @p steps (JSON text of the array's elements) after
/// the open step.
[[nodiscard]] std::string scenarioWith(std::string_view steps)
{
    return std::format(R"({{"schema": "gui-scenario", "schemaVersion": 2, "title": "t", "steps": [)"
                       R"({{"step": "open", "file": "tests/data/x.ork"}}{}{}]}})",
                       steps.empty() ? "" : ", ", steps);
}

/// The error of parsing @p text, "" when it parses.
[[nodiscard]] std::string parseError(const std::string& text)
{
    const Result<GuiScenario> parsed = QtRocket::Test::parseGuiScenario(text);
    return parsed ? std::string() : parsed.error().message;
}

TEST(GuiScenarioReader, ReadsTheStepsOfTheTabWithTheirAnswers)
{
    const Result<GuiScenario> parsed = QtRocket::Test::parseGuiScenario(scenarioWith(
        R"({"step": "tab", "tab": "flightConfiguration"},
           {"step": "configTab", "tab": "recovery"},
           {"step": "selectCells", "cells": [{"row": 1, "column": ["S", {"name": "P", "occurrence": 2}]},
                                             {"row": 0, "with": "ctrl"}]},
           {"step": "mountCheck", "mount": ["S", "B"], "checked": true},
           {"step": "newConfiguration"},
           {"step": "newConfiguration", "answer": {"search": "C6", "motor": {"manufacturer": "Estes", "designation": "C6"},
                                                   "delay": "Plugged", "button": "ok"}},
           {"step": "selectDeployment", "answer": {"scope": "all", "event": "ALTITUDE", "altitude": {"text": "200", "unit": "m"},
                                                   "delay": {"text": "1", "unit": "s"}, "button": "ok"}},
           {"step": "renameConfiguration", "answer": {"name": "{motors}", "saveAsDefault": true, "button": "reset"}},
           {"step": "selectIgnition", "answer": {"button": "cancel"}},
           {"step": "resetSeparation", "note": "a note"})"));
    ASSERT_TRUE(parsed.has_value()) << parsed.error().toString();
    EXPECT_EQ(parsed->schemaVersion, 2);
    ASSERT_EQ(parsed->steps.size(), 11U);
    EXPECT_EQ(parsed->steps[0].file, "tests/data/x.ork");
    EXPECT_EQ(parsed->steps[1].tab, "flightConfiguration");
    EXPECT_EQ(parsed->steps[2].tab, "recovery");
    const std::vector<QtRocket::Test::GuiCell>& cells = parsed->steps[3].cells;
    ASSERT_EQ(cells.size(), 2U);
    EXPECT_EQ(cells[0].row, 1);
    EXPECT_EQ(cells[0].column.value_or(GuiComponentPath{}),
              (GuiComponentPath{{.name = "S", .occurrence = 1}, {.name = "P", .occurrence = 2}}));
    EXPECT_EQ(cells[0].with, "");
    EXPECT_FALSE(cells[1].column.has_value());
    EXPECT_EQ(cells[1].with, "ctrl");
    EXPECT_EQ(parsed->steps[4].mount, (GuiComponentPath{{.name = "S"}, {.name = "B"}}));
    EXPECT_TRUE(parsed->steps[4].checked);
    EXPECT_FALSE(parsed->steps[5].answer.has_value());
    const GuiAnswer chooser = parsed->steps[6].answer.value_or(GuiAnswer{});
    EXPECT_EQ(chooser.search, "C6");
    EXPECT_EQ(chooser.motor.value_or(QtRocket::Test::GuiMotor{}).designation, "C6");
    EXPECT_EQ(chooser.chooserDelay, "Plugged");
    EXPECT_FALSE(chooser.delay.has_value());
    const GuiAnswer deployment = parsed->steps[7].answer.value_or(GuiAnswer{});
    EXPECT_EQ(deployment.scope, "all");
    EXPECT_EQ(deployment.altitude, (QtRocket::Test::GuiTypedValue{.text = "200", .unit = "m"}));
    EXPECT_EQ(deployment.delay, (QtRocket::Test::GuiTypedValue{.text = "1", .unit = "s"}));
    const GuiAnswer rename = parsed->steps[8].answer.value_or(GuiAnswer{});
    EXPECT_EQ(rename.name, "{motors}");
    EXPECT_TRUE(rename.saveAsDefault);
    EXPECT_EQ(rename.button, "reset");
    EXPECT_EQ(parsed->steps[9].answer.value_or(GuiAnswer{}).button, "cancel");
    EXPECT_EQ(parsed->steps[10].kind, GuiStepKind::RESET_SEPARATION);
    EXPECT_EQ(parsed->steps[10].note, "a note");
}

TEST(GuiScenarioReader, RefusesWhatTheToolRefuses)
{
    // The steps of version 1 that version 2 has not, and the other way round.
    EXPECT_NE(parseError(scenarioWith(R"({"step": "copy"})")), "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "add", "component": "NoseCone"})")), "");
    EXPECT_NE(parseError(R"({"schema": "gui-from-scratch-scenario", "schemaVersion": 1,
        "title": "t", "steps": [{"step": "new"}, {"step": "tab", "tab": "design"}]})"),
              "");
    // Open first, and only there; a schema of the other version's number.
    EXPECT_NE(parseError(scenarioWith(R"({"step": "open", "file": "a.ork"})")), "");
    EXPECT_NE(parseError(R"({"schema": "gui-scenario", "schemaVersion": 1, "title": "t",
        "steps": [{"step": "open", "file": "a.ork"}]})"),
              "");
    // A tab that is not one, a cell without a row or with another key, no cell.
    EXPECT_NE(parseError(scenarioWith(R"({"step": "configTab", "tab": "design"})")), "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectCells", "cells": [{"with": "ctrl"}]})")),
              "");
    EXPECT_NE(parseError(
                  scenarioWith(R"({"step": "selectCells", "cells": [{"row": 0, "with": "alt"}]})")),
              "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectCells", "cells": []})")), "");
    // Answers: a question's answer is required (but New's), has a button, and only the members
    // of its question.
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectMotor"})")), "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectMotor", "answer": {"search": "x"}})")),
              "");
    EXPECT_NE(parseError(scenarioWith(
                  R"({"step": "selectMotor", "answer": {"scope": "all", "button": "ok"}})")),
              "");
    EXPECT_NE(
        parseError(scenarioWith(
            R"({"step": "selectIgnition", "answer": {"event": "LAUNCH", "button": "cancel"}})")),
        "");
    EXPECT_NE(
        parseError(scenarioWith(
            R"({"step": "selectIgnition", "answer": {"scope": "this", "event": "LAUNCH", "button": "ok"}})")),
        "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectIgnition", "answer": {"scope": "some",
        "event": "LAUNCH", "delay": {"text": "1", "unit": "s"}, "button": "ok"}})")),
              "");
    // The altitude exactly for the altitude events.
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectDeployment", "answer": {"scope": "this",
        "event": "APOGEE", "altitude": {"text": "1", "unit": "m"}, "delay": {"text": "1", "unit": "s"},
        "button": "ok"}})")),
              "");
    EXPECT_NE(parseError(scenarioWith(R"({"step": "selectSeparation", "answer": {"scope": "this",
        "event": "ALTITUDE_ASCENDING", "delay": {"text": "1", "unit": "s"}, "button": "ok"}})")),
              "");
    EXPECT_NE(parseError(scenarioWith(
                  R"({"step": "renameConfiguration", "answer": {"button": "apply"}})")),
              "");
    EXPECT_NE(
        parseError(scenarioWith(
            R"({"step": "renameConfiguration", "answer": {"saveAsDefault": false, "button": "ok"}})")),
        "");
    EXPECT_NE(
        parseError(scenarioWith(R"({"step": "selectMotor", "answer": {"nozzleExitDiameter": "5",
        "button": "ok"}})")),
        "");
    // And what it takes.
    EXPECT_EQ(parseError(scenarioWith(R"({"step": "selectSeparation", "answer": {"scope": "this",
        "event": "ALTITUDE_DESCENDING", "altitude": {"text": "300", "unit": "ft"},
        "delay": {"text": "0", "unit": "s"}, "button": "ok"}})")),
              "");
}

TEST(GuiScenarioReader, RepositoryPathsAreComposedNameByName)
{
    const Result<std::filesystem::path> file =
        QtRocket::Test::guiRepositoryFile("tests/data/goldens/gui/from-scratch/rocket.ork");
    ASSERT_TRUE(file.has_value()) << file.error().toString();
    EXPECT_EQ(*file, QtRocket::Test::guiFromScratchDir() / "rocket.ork");
    EXPECT_TRUE(std::filesystem::is_regular_file(*file));
    for (const std::string_view bad : {"", "/etc/x", "a/../b", "a//b", "./a", "a\\b", "C:/a"})
    {
        const Result<std::filesystem::path> refused = QtRocket::Test::guiRepositoryFile(bad);
        EXPECT_FALSE(refused.has_value()) << bad;
        EXPECT_EQ(refused ? ErrorCode::UNKNOWN : refused.error().code, ErrorCode::PARSE) << bad;
    }
}

// =================================================================== comparing a GUI's records

/// The reference of @p scenario; a test failure and none when it cannot be read.
[[nodiscard]] GuiReference referenceOf(std::string_view scenario)
{
    Result<GuiReference> reference = loadReference(scenario);
    if (!reference)
    {
        ADD_FAILURE() << reference.error().toString();
        return {};
    }
    return std::move(*reference);
}

/// The record of step @p step of @p reference; a test failure and an empty record when it has
/// none.
[[nodiscard]] GuiStepRecord recordAt(const GuiReference& reference, std::size_t step)
{
    if (step >= reference.steps.size())
    {
        ADD_FAILURE() << "the reference has no step " << step;
        return {};
    }
    return reference.steps[step];
}

/// The steps of @p reference whose record differs from itself (guiRecordDifferences()).
[[nodiscard]] std::vector<std::size_t> recordsDifferingFromThemselves(const GuiReference& reference)
{
    std::vector<std::size_t> differing;
    for (const GuiStepRecord& record : reference.steps)
    {
        if (!QtRocket::Test::guiRecordDifferences(record, record).empty())
        {
            differing.push_back(record.index);
        }
    }
    return differing;
}

TEST(GuiRecordComparison, ARecordAgreesWithItself)
{
    for (const std::string_view scenario : QtRocket::Test::kGuiTabScenarios)
    {
        const GuiReference reference = referenceOf(scenario);
        EXPECT_FALSE(reference.steps.empty()) << scenario;
        EXPECT_EQ(recordsDifferingFromThemselves(reference), std::vector<std::size_t>{})
            << scenario;
    }
}

/// The tab's state of @p record; a test failure and an empty state when it has none.
[[nodiscard]] QtRocket::Test::GuiFlightConfigurationState tabStateOf(const GuiStepRecord& record)
{
    if (!record.flightConfiguration.has_value())
    {
        ADD_FAILURE() << "the record of step " << record.index << " has no tab";
        return {};
    }
    return *record.flightConfiguration;
}

/// @p record with its tab's state changed by @p change.
[[nodiscard]] GuiStepRecord withTabChanged(
    GuiStepRecord record, void (*change)(QtRocket::Test::GuiFlightConfigurationState&))
{
    QtRocket::Test::GuiFlightConfigurationState state = tabStateOf(record);
    change(state);
    record.flightConfiguration = std::move(state);
    return record;
}

// Changes of a tab's state.
void unshadeTheFirstMotorCell(QtRocket::Test::GuiFlightConfigurationState& state)
{
    if (!state.motors.rows.empty() && !state.motors.rows.front().motorCells.empty())
    {
        state.motors.rows.front().motorCells.front().ignitionDefault = false;
    }
}
void selectTheFirstRecoveryColumn(QtRocket::Test::GuiFlightConfigurationState& state)
{
    state.recovery.selectedColumns = {1};
}
void enableSelectSeparation(QtRocket::Test::GuiFlightConfigurationState& state)
{
    state.stages.buttons["selectSeparationButton"] = true;
}
void dropTheSimulations(QtRocket::Test::GuiFlightConfigurationState& state)
{
    state.simulations.clear();
}
void dropTheMotorRows(QtRocket::Test::GuiFlightConfigurationState& state)
{
    state.motors.rows.clear();
}
void addThirtyMotorRows(QtRocket::Test::GuiFlightConfigurationState& state)
{
    constexpr int kRows = 30;
    for (int row = 0; row < kRows; ++row)
    {
        state.motors.rows.push_back(
            {.configuration = std::format("row {}", row), .cells = {}, .motorCells = {}});
    }
}

TEST(GuiRecordComparison, EveryMemberOfVersionTwoIsCompared)
{
    // Step 3 of flight-configurations: New with the motor chooser.
    const GuiStepRecord record = recordAt(referenceOf("flight-configurations"), 3);
    ASSERT_EQ(record.questions.size(), 1U);
    const QtRocket::Test::GuiFlightConfigurationState state = tabStateOf(record);
    ASSERT_FALSE(state.motors.rows.empty());
    ASSERT_FALSE(state.motors.rows.front().motorCells.empty());
    ASSERT_TRUE(state.motors.rows.front().motorCells.front().ignitionDefault);

    GuiStepRecord tab      = record;
    tab.tab                = "design";
    GuiStepRecord question = record;
    question.questions.front().answered.rows += 1;
    for (const GuiStepRecord& changed :
         {tab, question, withTabChanged(record, unshadeTheFirstMotorCell),
          withTabChanged(record, selectTheFirstRecoveryColumn),
          withTabChanged(record, enableSelectSeparation),
          withTabChanged(record, dropTheSimulations)})
    {
        const std::vector<std::string> differences =
            QtRocket::Test::guiRecordDifferences(record, changed);
        EXPECT_EQ(differences.size(), 1U);
        EXPECT_TRUE(!differences.empty() && differences.front().starts_with("OpenRocket: "));
    }
    // A question that is not asked, and a row that is not shown, are differences too.
    GuiStepRecord asked = record;
    asked.questions.clear();
    EXPECT_FALSE(QtRocket::Test::guiRecordDifferences(record, asked).empty());
    EXPECT_FALSE(
        QtRocket::Test::guiRecordDifferences(record, withTabChanged(record, dropTheMotorRows))
            .empty());
}

TEST(GuiRecordComparison, DifferencesBeyondTheLimitAreCounted)
{
    // A Motors table of 30 more rows: every line from its rows on differs.
    const GuiStepRecord            record = recordAt(referenceOf("flight-configurations"), 3);
    const std::vector<std::string> differences =
        QtRocket::Test::guiRecordDifferences(record, withTabChanged(record, addThirtyMotorRows));
    ASSERT_FALSE(differences.empty());
    EXPECT_TRUE(differences.back().starts_with("... and ")) << differences.back();
    EXPECT_EQ(
        std::ranges::count_if(
            differences, [](const std::string& line) { return line.starts_with("OpenRocket: "); }),
        static_cast<std::ptrdiff_t>(QtRocket::Test::kGuiRecordDifferenceLines));
}

/// @p record with every cell of its Recovery and Stages tables unshaded, as OpenRocket's records
/// have them.
[[nodiscard]] GuiStepRecord withValueCellsUnshaded(GuiStepRecord record)
{
    if (record.flightConfiguration.has_value())
    {
        for (QtRocket::Test::GuiConfigurationTable* const table :
             {&record.flightConfiguration->recovery, &record.flightConfiguration->stages})
        {
            for (QtRocket::Test::GuiTableRow& row : table->rows)
            {
                for (QtRocket::Test::GuiValueCell& cell : row.cells)
                {
                    cell.shaded = false;
                }
            }
        }
    }
    return record;
}

/// The shading of the Recovery table's cells of @p record, row by row ("s" shaded, "-" not).
[[nodiscard]] std::vector<std::string> recoveryShading(const GuiStepRecord& record)
{
    std::vector<std::string> rows;
    for (const QtRocket::Test::GuiTableRow& row : tabStateOf(record).recovery.rows)
    {
        std::string text;
        for (const QtRocket::Test::GuiValueCell& cell : row.cells)
        {
            text += cell.shaded ? 's' : '-';
        }
        rows.push_back(text);
    }
    return rows;
}

TEST(GuiRecordComparison, TheValueCellsAreShadedAsQtRocketShadesThem)
{
    // Step 21 of flight-configurations: the undo of Reset deployment, the parachute's override of
    // the first configuration back; OpenRocket's table shades no cell (bug 1).
    const GuiStepRecord record = recordAt(referenceOf("flight-configurations"), 21);
    EXPECT_EQ(recoveryShading(record), (std::vector<std::string>{"--", "--"}));
    std::string         problems;
    const GuiStepRecord shadedAsQtRocket =
        QtRocket::Test::guiExpectedRecord("flight-configurations", record, problems);
    EXPECT_EQ(problems, "");
    // QtRocket's shades every cell without an override of its own.
    EXPECT_EQ(recoveryShading(shadedAsQtRocket), (std::vector<std::string>{"-s", "ss"}));
    const std::vector<std::string> differences =
        QtRocket::Test::guiRecordDifferences(record, shadedAsQtRocket);
    ASSERT_EQ(differences.size(), 2U);
    EXPECT_TRUE(differences.front().starts_with("OpenRocket: recovery row 0: "))
        << differences.front();
    EXPECT_TRUE(differences.back().contains("Altitude 150 m (shaded)")) << differences.back();
    // A cell that OpenRocket shaded is a problem, and keeps its shading.
    GuiStepRecord                               shaded = record;
    QtRocket::Test::GuiFlightConfigurationState state  = tabStateOf(shaded);
    ASSERT_FALSE(state.recovery.rows.empty());
    ASSERT_FALSE(state.recovery.rows.front().cells.empty());
    state.recovery.rows.front().cells.front().shaded = true;
    shaded.flightConfiguration                       = std::move(state);
    const GuiStepRecord kept =
        QtRocket::Test::guiExpectedRecord("flight-configurations", shaded, problems);
    EXPECT_NE(problems, "");
    EXPECT_EQ(recoveryShading(kept), (std::vector<std::string>{"ss", "ss"}));
}

TEST(GuiRecordComparison, TheExpectedDifferencesGiveQtRocketsValues)
{
    // Step 16 of flight-configurations: the parachute's deployment, which OpenRocket's dialog
    // starts on "this configuration" (bug 1).
    const GuiReference  reference = referenceOf("flight-configurations");
    const GuiStepRecord record    = recordAt(reference, 16);
    ASSERT_EQ(record.questions.size(), 1U);
    EXPECT_EQ(record.questions.front().scope, "this");
    std::string         problems;
    const GuiStepRecord expected =
        QtRocket::Test::guiExpectedRecord("flight-configurations", record, problems);
    EXPECT_EQ(problems, "");
    ASSERT_EQ(expected.questions.size(), 1U);
    EXPECT_EQ(expected.questions.front().scope, "all");
    // Nothing else changes but the shading of the Recovery and Stages cells (the rule of bug 1,
    // TheValueCellsAreShadedAsQtRocketShadesThem).
    GuiStepRecord back           = expected;
    back.questions.front().scope = "this";
    EXPECT_EQ(QtRocket::Test::guiRecordDifferences(record, withValueCellsUnshaded(back)),
              std::vector<std::string>{});
    // The questions of a step without an expected difference, or of another scenario, stay as
    // they are.
    const GuiStepRecord other = recordAt(reference, 18);
    EXPECT_EQ(QtRocket::Test::guiRecordDifferences(
                  other, withValueCellsUnshaded(QtRocket::Test::guiExpectedRecord(
                             "flight-configurations", other, problems))),
              std::vector<std::string>{});
    EXPECT_EQ(QtRocket::Test::guiRecordDifferences(
                  record, withValueCellsUnshaded(
                              QtRocket::Test::guiExpectedRecord("multi-stage", record, problems))),
              std::vector<std::string>{});
    EXPECT_EQ(problems, "");
    // A record that does not show OpenRocket's value any more is a problem, and stays.
    std::string         stale;
    const GuiStepRecord again =
        QtRocket::Test::guiExpectedRecord("flight-configurations", expected, stale);
    EXPECT_NE(stale, "");
    EXPECT_EQ(again.questions.front().scope, "all");
}

}  // namespace
