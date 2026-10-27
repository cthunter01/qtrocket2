// The operations of OpenRocket's "Motors & Configuration" tab (FlightConfigurationOperationsProbe,
// OpenRocket 5f164fd0e: its real FlightConfigurationPanel and sub-panels under xvfb, with
// SwingPreferences, FlatLaf Light and the bundled motors) replayed through QtRocket's
// FlightConfigurationEditing on the same designs: after every step the configurations and their
// names, the simulations, every motor mount's, recovery device's and stage's values per
// configuration, the three tables (FlightConfigurableTableModel), the selected configuration and
// the undo history, the change events of the step and what each dialog was asked. The pins are
// FlightConfigurationOperationPins.h, generated from the probe's output.
//
// The replay does what OpenRocket's panels do around the operations: a selection of cells selects
// the first row's configuration in the rocket (FlightConfigurablePanel's table listener), a sub-tab
// switch leaves the rocket's selection as OpenRocket's left it (synchronizePanelSelection()), and
// an operation acts on the cells the probe printed (getSelectedConfigurationIds() and
// getSelectedComponents()); the dialogs are answered as the probe answered them
// (ScriptedUserPrompts).
//
// QtRocket's fixes of OpenRocket's bugs (plan 10.3) show in a few steps, each the last step of its
// scenario; the expected state of such a step is OpenRocket's with the fix applied (kPatches, each
// with its bug). Bug 5 (the undo positions of New, Duplicate and the motor mount check box) is in
// the pins already: the probe adds those positions as QtRocket does. The start radio button of the
// value dialogs (bugs 1 and 2) is checked against QtRocket's rule, and OpenRocket's against its
// own.

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "FlightConfigurationOperationPins.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using Kind  = FlightConfigurableTableModel::Kind;
using Scope = UserPrompts::ConfigurationScope;
using Test::OperationScenarioPin;
using Test::OperationStepPin;
using DeployEvent     = DeploymentConfiguration::DeployEvent;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

constexpr int kMotorsTab   = 0;
constexpr int kRecoveryTab = 1;

// ------------------------------------------------------------------------------------ the texts

/// The fields of a line of the transcript.
[[nodiscard]] std::vector<std::string_view> fieldsOf(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t                   start = 0;
    while (true)
    {
        const std::size_t tab = line.find('\t', start);
        fields.push_back(line.substr(start, tab == std::string_view::npos ? tab : tab - start));
        if (tab == std::string_view::npos)
        {
            return fields;
        }
        start = tab + 1;
    }
}

/// The comma-separated integers of a CELLS field.
[[nodiscard]] std::vector<int> integersOf(std::string_view text)
{
    std::vector<int> values;
    for (const std::string_view part : fieldsOf(text))
    {
        // fieldsOf() splits on tabs; the CELLS lists are comma-separated.
        std::size_t start = 0;
        while (start < part.size())
        {
            std::size_t comma = part.find(',', start);
            if (comma == std::string_view::npos)
            {
                comma = part.size();
            }
            values.push_back(Strings::parseInt(part.substr(start, comma - start)).value_or(-1));
            start = comma + 1;
        }
    }
    return values;
}

[[nodiscard]] int integerOf(std::string_view text)
{
    const std::optional<int> value = Strings::parseInt(text);
    EXPECT_TRUE(value.has_value()) << text;
    return value.value_or(-1);
}

[[nodiscard]] double doubleOf(std::string_view text)
{
    const std::optional<double> value = Strings::javaParseDouble(text);
    EXPECT_TRUE(value.has_value()) << text;
    return value.value_or(0.0);
}

/// Java's Double.toString (JDK 17), as the probe prints a double.
[[nodiscard]] std::string javaText(double value)
{
    return FloatingDecimal::toJavaFormatString(value);
}

[[nodiscard]] std::string_view javaText(bool value)
{
    return value ? "true" : "false";
}

/// A description as the probe prints it: the text, or <null>.
[[nodiscard]] std::string javaText(const std::optional<std::string>& value)
{
    return value.value_or("<null>");
}

[[nodiscard]] std::string utf8(const QVariant& value)
{
    return QtStrings::toUtf8(value.toString());
}

/// @p fields separated by tabs.
[[nodiscard]] std::string joined(std::initializer_list<std::string> fields)
{
    std::string text;
    for (const std::string& field : fields)
    {
        if (!text.empty())
        {
            text += '\t';
        }
        text += field;
    }
    return text;
}

[[nodiscard]] std::string_view scopeText(Scope scope)
{
    return scope == Scope::THIS_CONFIGURATION ? "THIS" : "ALL";
}

template <typename E, std::size_t N, typename Name>
[[nodiscard]] E eventOf(std::string_view text, const std::array<E, N>& all, Name name)
{
    for (const E event : all)
    {
        if (name(event) == text)
        {
            return event;
        }
    }
    ADD_FAILURE() << "no event " << text;
    return all.front();
}

[[nodiscard]] Scope scopeOf(std::string_view text)
{
    EXPECT_TRUE(text == "ALL" || text == "THIS") << text;
    return text == "THIS" ? Scope::THIS_CONFIGURATION : Scope::ALL_CONFIGURATIONS;
}

/// The motor the probe picked: the one of the bundled database with its manufacturer (simple
/// name), designation and digest.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> bundledMotor(std::string_view manufacturer,
                                                                   std::string_view designation,
                                                                   std::string_view digest)
{
    for (const ThrustCurveMotorSet& set : QtRocket::Test::bundledMotorDatabase().getMotorSets())
    {
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : set.getMotors())
        {
            if (motor->getManufacturer().getSimpleName() == manufacturer &&
                motor->getDesignation() == designation && motor->getDigest() == digest)
            {
                return motor;
            }
        }
    }
    ADD_FAILURE() << "no bundled motor " << manufacturer << " " << designation << " " << digest;
    return nullptr;
}

// ------------------------------------------------------------------------- the deviations

/// How a step's expected state differs from OpenRocket's: QtRocket's fix of an OpenRocket bug.
struct Patch
{
    enum class What
    {
        /// The state before the step (Cancel changes nothing), with the step's own undo line.
        PREVIOUS_STATE,
        /// The line @c line becomes @c with.
        REPLACE,
        /// The line @c with comes after the line @c line.
        INSERT_AFTER,
        /// The undo line becomes @c with, and the step fires no event.
        UNDO_WITHOUT_EVENT,
    };

    std::string_view scenario;
    int              step;
    What             what;
    std::string_view line;
    std::string_view with;
};

// NOLINTBEGIN(modernize-raw-string-literal): the lines as the transcript has them, tabs escaped
inline constexpr std::array<Patch, 8> kPatches{{
    // Bug 3: OpenRocket's Cancel sets the start values back through the setters, which marks the
    // ignition overridden ("Default (Automatic)" becomes "Automatic"); QtRocket's changes nothing.
    {.scenario = "new-design",
     .step     = 10,
     .what     = Patch::What::PREVIOUS_STATE,
     .line     = {},
     .with     = {}},
    // Bug 3: OpenRocket's separation dialog edits the override in place, and Cancel keeps the edits
    // (and fires their event, making a step of them); QtRocket's changes nothing.
    {.scenario = "separation-cancel",
     .step     = 4,
     .what     = Patch::What::PREVIOUS_STATE,
     .line     = {},
     .with     = {}},
    {.scenario = "separation-cancel",
     .step     = 4,
     .what     = Patch::What::UNDO_WITHOUT_EVENT,
     .line     = {},
     .with     = "UNDO\t0\tfalse\t<null>\tSelect separation\tfalse\t<null>\t<null>"},
    // Bug 4: an ignition for this configuration only on a cell without a motor configuration of its
    // own: OpenRocket's dialog edits the mount's default configuration; QtRocket gives the cell a
    // configuration of its own (made from the default) with the ignition. (The cell's text is the
    // same either way.)
    {.scenario = "override-without-configuration",
     .step     = 4,
     .what     = Patch::What::REPLACE,
     .line     = "MOUNT\tInner Tube\ttrue\tBURNOUT\t0.5\ttrue",
     .with     = "MOUNT\tInner Tube\ttrue\tAUTOMATIC\t0.0\ttrue"},
    {.scenario = "override-without-configuration",
     .step     = 4,
     .what     = Patch::What::INSERT_AFTER,
     .line     = "ME\t4\tC6-7\t2967cd7a160b396ef96f09695429d8e9\t7.0\t0.0\tAUTOMATIC\t0.0\ttrue",
     .with     = "ME\t5\tNone\t-\t0.0\t0.0\tBURNOUT\t0.5\ttrue"},
    // Bug 4, the other cells: OpenRocket's loop gives a cell without a motor configuration one (a
    // clone of the mount's default), but writes the values into the default it read before;
    // QtRocket writes them into the cell's new configuration.
    {.scenario = "override-cells-without-configuration",
     .step     = 4,
     .what     = Patch::What::REPLACE,
     .line     = "MOUNT\tInner Tube\ttrue\tEJECTION_CHARGE\t1.0\ttrue",
     .with     = "MOUNT\tInner Tube\ttrue\tAUTOMATIC\t0.0\ttrue"},
    {.scenario = "override-cells-without-configuration",
     .step     = 4,
     .what     = Patch::What::REPLACE,
     .line     = "ME\t5\tNone\t-\t0.0\t0.0\tAUTOMATIC\t0.0\ttrue",
     .with     = "ME\t5\tNone\t-\t0.0\t0.0\tEJECTION_CHARGE\t1.0\ttrue"},
    {.scenario = "override-cells-without-configuration",
     .step     = 4,
     .what     = Patch::What::REPLACE,
     .line     = "MT\t5\t1\tNone\tAutomatic\tfalse",
     .with     = "MT\t5\t1\tNone\tEjection charge + 1 s\tfalse"},
}};
// NOLINTEND(modernize-raw-string-literal)

/// The state QtRocket must show after @p step of @p scenario: OpenRocket's, with kPatches.
[[nodiscard]] std::vector<std::string> expectedState(const OperationScenarioPin& scenario,
                                                     std::size_t                 step)
{
    const OperationStepPin&  pin = scenario.steps[step];
    std::vector<std::string> lines(pin.state.begin(), pin.state.end());
    for (const Patch& patch : kPatches)
    {
        if (patch.scenario != scenario.name || static_cast<std::size_t>(patch.step) != step + 1)
        {
            continue;
        }
        switch (patch.what)
        {
            case Patch::What::PREVIOUS_STATE:
            {
                const std::string undo     = lines.back();
                const auto&       previous = scenario.steps[step - 1].state;
                lines.assign(previous.begin(), previous.end());
                lines.back() = undo;
                break;
            }
            case Patch::What::UNDO_WITHOUT_EVENT:
                lines.back() = std::string(patch.with);
                break;
            case Patch::What::REPLACE:
            case Patch::What::INSERT_AFTER:
            {
                const auto found = std::ranges::find(lines, patch.line);
                EXPECT_NE(found, lines.end())
                    << "a patch's line is not in the pins: " << patch.line;
                if (found == lines.end())
                {
                    break;
                }
                if (patch.what == Patch::What::REPLACE)
                {
                    *found = std::string(patch.with);
                }
                else
                {
                    lines.insert(std::next(found), std::string(patch.with));
                }
                break;
            }
        }
    }
    return lines;
}

/// The events QtRocket must fire in @p step of @p scenario: OpenRocket's (one for each of
/// FlightConfigurationPanel.configurationChanged()'s), or none where kPatches says so.
[[nodiscard]] std::vector<std::string> expectedEvents(const OperationScenarioPin& scenario,
                                                      std::size_t                 step)
{
    for (const Patch& patch : kPatches)
    {
        if (patch.scenario == scenario.name && static_cast<std::size_t>(patch.step) == step + 1 &&
            patch.what == Patch::What::UNDO_WITHOUT_EVENT)
        {
            return {};
        }
    }
    const auto& events = scenario.steps[step].events;
    return {events.begin(), events.end()};
}

/// The deployment answered in an A line (A, DEPLOYMENT, ok, scope, event, altitude, delay).
[[nodiscard]] UserPrompts::DeploymentChoice deploymentAnswer(std::span<const std::string_view> f)
{
    DeploymentConfiguration deployment;
    deployment.setDeployEvent(eventOf(f[4], DeploymentConfiguration::kAllDeployEvents,
                                      [](DeployEvent e) { return deployEventName(e); }));
    deployment.setDeployAltitude(doubleOf(f[5]));
    deployment.setDeployDelay(doubleOf(f[6]));
    return {.scope = scopeOf(f[3]), .deployment = deployment};
}

/// The separation answered in an A line, as deploymentAnswer().
[[nodiscard]] UserPrompts::SeparationChoice separationAnswer(std::span<const std::string_view> f)
{
    StageSeparationConfiguration separation;
    separation.setSeparationEvent(
        eventOf(f[4], StageSeparationConfiguration::kAllSeparationEvents,
                [](SeparationEvent e) { return separationEventName(e); }));
    separation.setSeparationAltitude(doubleOf(f[5]));
    separation.setSeparationDelay(doubleOf(f[6]));
    return {.scope = scopeOf(f[3]), .separation = separation};
}

// ------------------------------------------------------------------------------- the replay

/// A scenario's design with the tab's operations and tables over it.
class OperationReplay
{
public:
    explicit OperationReplay(const OperationScenarioPin& scenario)
    {
        if (scenario.source == "new")
        {
            m_document = OpenRocketDocumentFactory::createNewRocket();
        }
        else if (const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(
                     scenario.source.substr(scenario.source.find(':') + 1)))
        {
            Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(
                *example, QtRocket::Test::ExampleSource::ORIGINAL,
                QtRocket::Test::DesignFileEnvironment::Presets::NONE);
            if (loaded)
            {
                m_document = std::move(loaded->document);
            }
        }
        if (m_document == nullptr)
        {
            ADD_FAILURE() << "no design " << scenario.source;
            return;
        }
        Rocket& rocket = m_document->getRocket();
        m_editing =
            std::make_unique<FlightConfigurationEditing>(*m_document, m_services.services());
        m_motors   = std::make_unique<FlightConfigurableTableModel>(Kind::MOTOR_MOUNTS, rocket,
                                                                    m_services.preferences());
        m_recovery = std::make_unique<FlightConfigurableTableModel>(Kind::RECOVERY_DEVICES, rocket,
                                                                    m_services.preferences());
        m_stages   = std::make_unique<FlightConfigurableTableModel>(Kind::STAGES, rocket,
                                                                    m_services.preferences());
        m_mounts   = std::make_unique<MotorMountTableModel>(rocket);
        m_events   = rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
            m_fired.push_back("EVENT\t" + std::to_string(event.getType()) + "\t" +
                              event.getSource()->getName());
        });
    }

    [[nodiscard]] bool isLoaded() const noexcept { return m_document != nullptr; }

    /// Runs @p step, whose state OpenRocket showed as @p expected (for the rocket's selection after
    /// a sub-tab switch); the events it fired.
    std::vector<std::string> run(const OperationStepPin&      step,
                                 std::span<const std::string> expected);

    /// The state, in the probe's lines.
    [[nodiscard]] std::vector<std::string> state() const;

    [[nodiscard]] Test::ScriptedUserPrompts& prompts() noexcept { return m_services.prompts(); }

    /// The last question of @p kind asked so far, or null.
    [[nodiscard]] const Test::ScriptedUserPrompts::Prompt* lastPrompt(
        Test::ScriptedUserPrompts::Kind kind) const
    {
        const Test::ScriptedUserPrompts::Prompt* last = nullptr;
        for (const Test::ScriptedUserPrompts::Prompt& prompt : m_services.prompts().prompts())
        {
            if (prompt.kind == kind)
            {
                last = &prompt;
            }
        }
        return last;
    }

    /// Checks the questions the step asked against the dialogs' first states the probe printed.
    void expectQuestionsAsJava(const OperationStepPin& step);

private:
    [[nodiscard]] FlightConfigurableTableModel& table(int tab) const
    {
        if (tab == kMotorsTab)
        {
            return *m_motors;
        }
        return tab == kRecoveryTab ? *m_recovery : *m_stages;
    }
    /// The configurations and components of a CELLS line.
    struct Cells
    {
        int                                tab = kMotorsTab;
        std::vector<FlightConfigurationId> ids;
        std::vector<Uuid>                  components;
    };
    [[nodiscard]] Cells cellsOf(std::string_view line) const;
    /// Scripts the answers of the step (its A lines and a rename's) into the prompts.
    void scriptAnswers(const OperationStepPin& step);
    /// Scripts the answer of the A line @p fields.
    void scriptAnswer(std::span<const std::string_view> fields);
    /// Scripts the answer of the rename @p action (its STEP line's fields).
    void scriptRename(std::span<const std::string_view> action);
    /// Checks one question (a Q line's @p fields) against what QtRocket asked.
    void expectIgnitionAsJava(std::span<const std::string_view> fields, Scope scope);
    void expectDeploymentAsJava(std::span<const std::string_view> fields, Scope scope);
    void expectSeparationAsJava(std::span<const std::string_view> fields, Scope scope);
    void expectMotorQuestionAsJava(std::span<const std::string_view> fields);
    void expectRenameOrMessageAsJava(std::span<const std::string_view> fields) const;
    /// Runs the sub-panel operation @p kind on @p cells.
    void operate(std::string_view kind, const Cells& cells);
    /// The operations that need no cells of a sub-panel's table.
    void edit(std::string_view what);
    /// What the start radio button must be for the first cell of @p cells (bugs 1 and 2).
    [[nodiscard]] Scope expectedScope(std::string_view kind, const Cells& cells) const;

    void stateOfMounts(std::vector<std::string>& lines, const RocketComponent& component) const;
    void stateOfTables(std::vector<std::string>& lines) const;

    Test::TestGuiServices                         m_services;
    std::unique_ptr<OpenRocketDocument>           m_document;
    std::unique_ptr<FlightConfigurationEditing>   m_editing;
    std::unique_ptr<FlightConfigurableTableModel> m_motors;
    std::unique_ptr<FlightConfigurableTableModel> m_recovery;
    std::unique_ptr<FlightConfigurableTableModel> m_stages;
    std::unique_ptr<MotorMountTableModel>         m_mounts;
    ComponentChangeSignal::ScopedConnection       m_events;
    std::vector<std::string>                      m_fired;
    int                                           m_tab = kMotorsTab;
    /// The scopes expected of the questions of the step being run, in order.
    std::vector<Scope> m_expectedScopes;
};

OperationReplay::Cells OperationReplay::cellsOf(std::string_view line) const
{
    const std::vector<std::string_view> fields = fieldsOf(line);
    Cells                               cells;
    if (fields.size() != 4)
    {
        ADD_FAILURE() << "unexpected CELLS line: " << line;
        return cells;
    }
    cells.tab                                 = integerOf(fields[1]);
    const FlightConfigurableTableModel& model = table(cells.tab);
    for (const int row : integersOf(fields[2]))
    {
        cells.ids.push_back(model.configurationId(row).value_or(FlightConfigurationId::errorId()));
    }
    for (const int column : integersOf(fields[3]))
    {
        cells.components.push_back(model.componentId(column).value_or(Uuid{}));
    }
    return cells;
}

void OperationReplay::scriptAnswers(const OperationStepPin& step)
{
    for (const std::string_view answer : step.answers)
    {
        scriptAnswer(fieldsOf(answer));
    }
    const std::vector<std::string_view> action = fieldsOf(step.action);
    if (action[2] == "RENAME")
    {
        scriptRename(action);
    }
}

void OperationReplay::scriptAnswer(std::span<const std::string_view> f)
{
    const bool ok = f.size() > 2 && f[2] == "ok";
    if (f[1] == "IGNITION")
    {
        prompts().answerIgnition(
            ok ? std::optional{UserPrompts::IgnitionChoice{
                     .scope = scopeOf(f[3]),
                     .event = eventOf(f[4], kAllIgnitionEvents,
                                      [](IgnitionEvent e) { return QtRocket::name(e); }),
                     .delay = doubleOf(f[5])}}
               : std::nullopt);
    }
    else if (f[1] == "DEPLOYMENT")
    {
        prompts().answerDeployment(ok ? std::optional{deploymentAnswer(f)} : std::nullopt);
    }
    else if (f[1] == "SEPARATION")
    {
        prompts().answerSeparation(ok ? std::optional{separationAnswer(f)} : std::nullopt);
    }
    else if (f[1] == "MOTOR")
    {
        prompts().answerMotor(
            ok ? std::optional{UserPrompts::MotorChoice{.motor = bundledMotor(f[3], f[4], f[5]),
                                                        .delay = doubleOf(f[6]),
                                                        .nozzleExitDiameter = doubleOf(f[7])}}
               : std::nullopt);
    }
    else
    {
        ADD_FAILURE() << "unexpected answer: " << f[1];
    }
}

void OperationReplay::scriptRename(std::span<const std::string_view> action)
{
    const std::string_view how = action[3];
    if (how == "ok")
    {
        prompts().answerRenameConfiguration(
            {.action = UserPrompts::RenameAction::RENAME, .name = QtStrings::fromUtf8(action[4])});
    }
    else if (how == "reset")
    {
        prompts().answerRenameConfiguration(
            {.action = UserPrompts::RenameAction::RESET, .name = {}});
    }
    else if (how == "savedefault")
    {
        prompts().answerRenameConfiguration(
            {.action = UserPrompts::RenameAction::CANCEL, .name = {}},
            {QtStrings::fromUtf8(action[4])});
    }
    else
    {
        prompts().answerRenameConfiguration(
            {.action = UserPrompts::RenameAction::CANCEL, .name = {}});
    }
}

Scope OperationReplay::expectedScope(std::string_view kind, const Cells& cells) const
{
    const Rocket& rocket = m_document->getRocket();
    if (cells.ids.empty() || cells.components.empty())
    {
        return Scope::ALL_CONFIGURATIONS;
    }
    const RocketComponent*       component = rocket.findComponent(cells.components.front());
    const FlightConfigurationId& id        = cells.ids.front();
    bool                         own       = false;
    if (kind == "SELECT_IGNITION")
    {
        const auto* mount = dynamic_cast<const MotorMount*>(component);
        own               = mount != nullptr && mount->getMotorConfigurationSet().containsId(id) &&
                            mount->getMotorConfig(id).hasIgnitionOverride();
    }
    else if (const auto* device = dynamic_cast<const RecoveryDevice*>(component))
    {
        own = device->getDeploymentConfigurations().containsId(id);
    }
    else if (const auto* stage = dynamic_cast<const AxialStage*>(component))
    {
        own = stage->getSeparationConfigurations().containsId(id);
    }
    return own ? Scope::THIS_CONFIGURATION : Scope::ALL_CONFIGURATIONS;
}

void OperationReplay::operate(std::string_view kind, const Cells& cells)
{
    if (kind == "SELECT_IGNITION" || kind == "SELECT_DEPLOYMENT" || kind == "SELECT_SEPARATION")
    {
        m_expectedScopes.push_back(expectedScope(kind, cells));
    }
    FlightConfigurationEditing& e = *m_editing;
    if (kind == "SELECT_MOTOR")
    {
        static_cast<void>(e.selectMotor(cells.ids, cells.components));
    }
    else if (kind == "DELETE_MOTOR")
    {
        static_cast<void>(e.deleteMotors(cells.ids, cells.components));
    }
    else if (kind == "SELECT_IGNITION")
    {
        static_cast<void>(e.selectIgnition(cells.ids, cells.components));
    }
    else if (kind == "RESET_IGNITION")
    {
        static_cast<void>(e.resetIgnition(cells.ids, cells.components));
    }
    else if (kind == "SELECT_DEPLOYMENT")
    {
        static_cast<void>(e.selectDeployment(cells.ids, cells.components));
    }
    else if (kind == "RESET_DEPLOYMENT")
    {
        static_cast<void>(e.resetDeployment(cells.ids, cells.components));
    }
    else if (kind == "SELECT_SEPARATION")
    {
        static_cast<void>(e.selectSeparation(cells.ids, cells.components));
    }
    else if (kind == "RESET_SEPARATION")
    {
        static_cast<void>(e.resetSeparation(cells.ids, cells.components));
    }
    else
    {
        ADD_FAILURE() << "unexpected operation " << kind;
    }
}

void OperationReplay::edit(std::string_view what)
{
    Rocket& rocket = m_document->getRocket();
    if (what == "ADD_NOSE_AND_BODY")
    {
        rocket.getChild(0).addChild(std::make_unique<NoseCone>());
        rocket.getChild(0).addChild(std::make_unique<BodyTube>());
    }
    else if (what == "BODY_MOUNT")
    {
        dynamic_cast<BodyTube&>(rocket.getChild(0).getChild(1)).setMotorMount(true);
    }
    else if (what == "ADD_PARACHUTE")
    {
        rocket.getChild(0).getChild(1).addChild(std::make_unique<Parachute>());
    }
    else if (what == "ADD_STAGE")
    {
        auto& stage = rocket.addChild(std::make_unique<AxialStage>());
        stage.addChild(std::make_unique<BodyTube>());
    }
    else if (what == "REMOVE_STAGE")
    {
        static_cast<void>(rocket.removeChild(1));
    }
    else
    {
        ADD_FAILURE() << "unexpected edit " << what;
    }
}

std::vector<std::string> OperationReplay::run(const OperationStepPin&      step,
                                              std::span<const std::string> expected)
{
    m_fired.clear();
    m_expectedScopes.clear();
    scriptAnswers(step);
    const std::vector<std::string_view> action = fieldsOf(step.action);
    const std::string_view              kind   = action[2];
    Rocket&                             rocket = m_document->getRocket();
    if (kind == "OPEN")
    {
    }
    else if (kind == "SELECT")
    {
        // FlightConfigurablePanel's table listener: the first row's configuration.
        m_tab = integerOf(action[3]);
        if (const auto id = table(m_tab).configurationId(integerOf(action[4])))
        {
            rocket.setSelectedConfiguration(*id);
        }
    }
    else if (kind == "TAB")
    {
        // synchronizePanelSelection(): the rocket's selection as OpenRocket's ended.
        m_tab          = integerOf(action[3]);
        const int  row = integerOf(fieldsOf(expected.front())[1]);
        const auto id  = table(m_tab).configurationId(row);
        if (id)
        {
            rocket.setSelectedConfiguration(*id);
        }
    }
    else if (kind == "UNDO")
    {
        m_document->undo();
    }
    else if (kind == "REDO")
    {
        m_document->redo();
    }
    else if (kind == "EDIT")
    {
        edit(action[3]);
    }
    else if (kind == "MOUNT")
    {
        static_cast<void>(m_editing->setMotorMount(
            m_mounts->mountId(integerOf(action[3])).value_or(Uuid{}), action[4] == "true"));
    }
    else if (kind == "NEW")
    {
        const std::string_view followUp = action[3];
        static_cast<void>(m_editing->newConfiguration([&](const FlightConfigurationId& id) {
            // The cell the table has selected then: the new row in the current column.
            const Cells cells = cellsOf(step.cells);
            EXPECT_EQ(cells.ids, std::vector<FlightConfigurationId>{id});
            operate(followUp, cells);
        }));
    }
    else if (kind == "DUPLICATE")
    {
        static_cast<void>(m_editing->duplicateConfigurations(cellsOf(step.cells).ids));
    }
    else if (kind == "DELETE_CONFIG")
    {
        static_cast<void>(m_editing->removeConfigurations(cellsOf(step.cells).ids));
    }
    else if (kind == "RENAME")
    {
        static_cast<void>(m_editing->renameConfigurations(cellsOf(step.cells).ids));
    }
    else
    {
        operate(kind, cellsOf(step.cells));
    }
    return m_fired;
}

void OperationReplay::stateOfMounts(std::vector<std::string>& lines,
                                    const RocketComponent&    component) const
{
    const std::vector<FlightConfigurationId> ids   = m_document->getRocket().getIds();
    const auto                               rowOf = [&ids](const FlightConfigurationId& id) {
        const auto found = std::ranges::find(ids, id);
        return found == ids.end() ? -1 : static_cast<int>(found - ids.begin());
    };
    if (const auto* mount = dynamic_cast<const MotorMount*>(&component))
    {
        const MotorConfiguration& def = mount->getDefaultMotorConfig();
        lines.push_back("MOUNT\t" + component.getName() + "\t" +
                        std::string(javaText(mount->isMotorMount())) + "\t" +
                        std::string(QtRocket::name(def.getIgnitionEvent())) + "\t" +
                        javaText(def.getIgnitionDelay()) + "\t" +
                        std::string(javaText(def.hasIgnitionOverride())));
        for (const FlightConfigurationId& id : mount->getMotorConfigurationSet().getIds())
        {
            const MotorConfiguration& c = mount->getMotorConfigurationSet().get(id);
            lines.push_back(
                "ME\t" + std::to_string(rowOf(id)) + "\t" +
                c.toMotorName(m_services.services().preferences()) + "\t" +
                (c.getMotor() == nullptr ? std::string("-") : c.getMotor()->getDigest()) + "\t" +
                javaText(c.getEjectionDelay()) + "\t" + javaText(c.getNozzleExitDiameter()) + "\t" +
                std::string(QtRocket::name(c.getIgnitionEvent())) + "\t" +
                javaText(c.getIgnitionDelay()) + "\t" +
                std::string(javaText(c.hasIgnitionOverride())));
        }
    }
    if (const auto* device = dynamic_cast<const RecoveryDevice*>(&component))
    {
        const auto& set   = device->getDeploymentConfigurations();
        const auto  value = [](const DeploymentConfiguration& d) {
            return std::string(deployEventName(d.getDeployEvent())) + "\t" +
                   javaText(d.getDeployAltitude()) + "\t" + javaText(d.getDeployDelay());
        };
        lines.push_back("DEV\t" + component.getName() + "\t" + value(set.getDefault()));
        for (const FlightConfigurationId& id : set.getIds())
        {
            lines.push_back("DE\t" + std::to_string(rowOf(id)) + "\t" + value(set.get(id)));
        }
    }
    if (const auto* stage = dynamic_cast<const AxialStage*>(&component);
        stage != nullptr && stage->getStageNumber() > 0)
    {
        const auto& set   = stage->getSeparationConfigurations();
        const auto  value = [](const StageSeparationConfiguration& s) {
            return std::string(separationEventName(s.getSeparationEvent())) + "\t" +
                   javaText(s.getSeparationAltitude()) + "\t" + javaText(s.getSeparationDelay());
        };
        lines.push_back("STG\t" + component.getName() + "\t" + value(set.getDefault()));
        for (const FlightConfigurationId& id : set.getIds())
        {
            lines.push_back("SE\t" + std::to_string(rowOf(id)) + "\t" + value(set.get(id)));
        }
    }
}

void OperationReplay::stateOfTables(std::vector<std::string>& lines) const
{
    for (const auto& [prefix, model] : {std::pair{std::string_view("M"), m_motors.get()},
                                        std::pair{std::string_view("R"), m_recovery.get()},
                                        std::pair{std::string_view("T"), m_stages.get()}})
    {
        for (int c = 0; c < model->columnCount(); ++c)
        {
            lines.push_back(std::string(prefix) + "H\t" + std::to_string(c) + "\t" +
                            utf8(model->headerData(c, Qt::Horizontal)));
        }
        for (int r = 0; r < model->rowCount(); ++r)
        {
            for (int c = 1; c < model->columnCount(); ++c)
            {
                const QModelIndex index = model->index(r, c);
                std::string       line  = std::string(prefix) + "T\t" + std::to_string(r) + "\t" +
                                          std::to_string(c) + "\t" + utf8(model->data(index));
                if (prefix == "M")
                {
                    line =
                        "MT\t" + std::to_string(r) + "\t" + std::to_string(c) + "\t" +
                        utf8(model->data(index)) + "\t" +
                        utf8(model->data(index, FlightConfigurableTableModel::kIgnitionTextRole)) +
                        "\t" +
                        std::string(
                            javaText(model->data(index, FlightConfigurableTableModel::kDefaultRole)
                                         .toBool()));
                }
                lines.push_back(std::move(line));
            }
        }
    }
}

std::vector<std::string> OperationReplay::state() const
{
    const Rocket&                            rocket      = m_document->getRocket();
    const Preferences&                       preferences = m_services.services().preferences();
    const std::vector<FlightConfigurationId> ids         = rocket.getIds();
    const auto                               rowOf       = [&ids](const FlightConfigurationId& id) {
        const auto found = std::ranges::find(ids, id);
        return found == ids.end() ? -1 : static_cast<int>(found - ids.begin());
    };
    std::vector<std::string> lines;
    lines.push_back("SEL\t" + std::to_string(rowOf(rocket.getSelectedConfiguration().getId())));
    for (std::size_t row = 0; row < ids.size(); ++row)
    {
        const FlightConfiguration& c = rocket.getFlightConfiguration(ids[row]);
        lines.push_back("CONF\t" + std::to_string(row) + "\t" + c.getNameRaw(preferences) + "\t" +
                        c.getName(preferences) + "\t" +
                        std::string(javaText(c.isNameOverridden(preferences))));
    }
    for (const std::shared_ptr<Simulation>& simulation : m_document->getSimulations())
    {
        lines.push_back("SIM\t" + simulation->getName() + "\t" +
                        std::to_string(rowOf(simulation->getFlightConfigurationId())));
    }
    for (const RocketComponent& component : rocket.subtree(true))
    {
        stateOfMounts(lines, component);
    }
    stateOfTables(lines);
    const OpenRocketDocument::UndoDetail undo = m_document->getUndoDetail();
    std::string line = "UNDO\t" + std::to_string(undo.position) + "\t" +
                       std::string(javaText(m_document->isUndoAvailable())) + "\t" +
                       javaText(m_document->getUndoDescription()) + "\t" +
                       javaText(undo.nextDescription) + "\t" +
                       std::string(javaText(m_document->isRedoAvailable())) + "\t" +
                       javaText(m_document->getRedoDescription());
    for (const std::optional<std::string>& description : undo.descriptions)
    {
        line += "\t" + javaText(description);
    }
    lines.push_back(std::move(line));
    return lines;
}

void OperationReplay::expectQuestionsAsJava(const OperationStepPin& step)
{
    std::size_t valueQuestion = 0;
    for (const std::string_view question : step.questions)
    {
        SCOPED_TRACE(question);
        const std::vector<std::string_view> f = fieldsOf(question);
        if (f[1] == "IGNITION" || f[1] == "DEPLOYMENT" || f[1] == "SEPARATION")
        {
            ASSERT_LT(valueQuestion, m_expectedScopes.size());
            const Scope scope = m_expectedScopes[valueQuestion++];
            if (f[1] == "IGNITION")
            {
                expectIgnitionAsJava(f, scope);
            }
            else if (f[1] == "DEPLOYMENT")
            {
                expectDeploymentAsJava(f, scope);
            }
            else
            {
                expectSeparationAsJava(f, scope);
            }
        }
        else if (f[1] == "MOTOR")
        {
            expectMotorQuestionAsJava(f);
        }
        else
        {
            expectRenameOrMessageAsJava(f);
        }
    }
}

void OperationReplay::expectIgnitionAsJava(std::span<const std::string_view> f, Scope scope)
{
    ASSERT_FALSE(prompts().ignitionRequests().empty());
    const UserPrompts::IgnitionRequest& r = prompts().ignitionRequests().back();
    // Bug 2: the start radio button is QtRocket's rule (OpenRocket's, f[3], is "all" exactly for a
    // cell without a motor).
    EXPECT_EQ(joined({QtStrings::toUtf8(r.configurationName),
                      std::string(QtRocket::name(r.initial.event)), javaText(r.initial.delay),
                      std::string(scopeText(r.initial.scope))}),
              joined({std::string(f[2]), std::string(f[4]), std::string(f[5]),
                      std::string(scopeText(scope))}));
}

void OperationReplay::expectDeploymentAsJava(std::span<const std::string_view> f, Scope scope)
{
    ASSERT_FALSE(prompts().deploymentRequests().empty());
    const UserPrompts::DeploymentRequest& r = prompts().deploymentRequests().back();
    const DeploymentConfiguration&        d = r.initial.deployment;
    // Bug 1: OpenRocket's dialog always starts on "this configuration" (f[3]); QtRocket's on the
    // cell's state.
    EXPECT_EQ(f[3], "1");
    EXPECT_EQ(
        joined({QtStrings::toUtf8(r.configurationName),
                std::string(deployEventName(d.getDeployEvent())), javaText(d.getDeployAltitude()),
                javaText(d.getDeployDelay()), std::string(scopeText(r.initial.scope)),
                std::string(javaText(r.bottomStage))}),
        joined({std::string(f[2]), std::string(f[4]), std::string(f[5]), std::string(f[6]),
                std::string(scopeText(scope)),
                std::string(javaText(!f[8].contains("LOWER_STAGE_SEPARATION")))}));
}

void OperationReplay::expectSeparationAsJava(std::span<const std::string_view> f, Scope scope)
{
    ASSERT_FALSE(prompts().separationRequests().empty());
    const UserPrompts::SeparationRequest& r = prompts().separationRequests().back();
    const StageSeparationConfiguration&   d = r.initial.separation;
    EXPECT_EQ(f[3], "1");  // bug 1, as for the deployment
    EXPECT_EQ(joined({QtStrings::toUtf8(r.configurationName),
                      std::string(separationEventName(d.getSeparationEvent())),
                      javaText(d.getSeparationAltitude()), javaText(d.getSeparationDelay()),
                      std::string(scopeText(r.initial.scope))}),
              joined({std::string(f[2]), std::string(f[4]), std::string(f[5]), std::string(f[6]),
                      std::string(scopeText(scope))}));
}

void OperationReplay::expectMotorQuestionAsJava(std::span<const std::string_view> f)
{
    ASSERT_FALSE(prompts().motorQuestions().empty());
    const Test::ScriptedUserPrompts::MotorQuestion& q = prompts().motorQuestions().back();
    EXPECT_EQ(m_motors->rowOf(q.configurationId), integerOf(f[2]));
    EXPECT_EQ(m_motors->columnOf(q.mountId), integerOf(f[3]));
}

void OperationReplay::expectRenameOrMessageAsJava(std::span<const std::string_view> f) const
{
    const bool rename = f[1] == "RENAME";
    EXPECT_TRUE(rename || f[1] == "MESSAGE") << f[1];
    const Test::ScriptedUserPrompts::Prompt* found =
        lastPrompt(rename ? Test::ScriptedUserPrompts::Kind::RENAME_CONFIGURATION
                          : Test::ScriptedUserPrompts::Kind::MESSAGE);
    ASSERT_NE(found, nullptr);
    if (rename)
    {
        EXPECT_EQ(found->texts, QStringList{QtStrings::fromUtf8(f[2])});
        return;
    }
    EXPECT_EQ(QtStrings::toUtf8(found->title), f[2]);
    EXPECT_EQ(found->texts,
              (QStringList{QStringLiteral("INFORMATION"), QtStrings::fromUtf8(f[3])}));
}

/// Compares @p actual with @p expected line by line, reporting only the lines that differ.
void expectSameLines(const std::vector<std::string>& actual,
                     const std::vector<std::string>& expected, std::string_view what)
{
    const std::size_t count    = std::max(actual.size(), expected.size());
    int               reported = 0;
    for (std::size_t i = 0; i < count && reported < 8; ++i)
    {
        const std::string_view a = i < actual.size() ? std::string_view(actual[i]) : "(none)";
        const std::string_view e = i < expected.size() ? std::string_view(expected[i]) : "(none)";
        if (a != e)
        {
            ADD_FAILURE() << what << " line " << i << ":\n  QtRocket:   " << a
                          << "\n  OpenRocket: " << e;
            ++reported;
        }
    }
}

class FlightConfigurationOperationsGolden
  : public Gui::Test::GuiTest,
    public ::testing::WithParamInterface<OperationScenarioPin>
{ };

[[nodiscard]] std::string scenarioName(const ::testing::TestParamInfo<OperationScenarioPin>& param)
{
    std::string name;
    bool        upper = true;
    for (const char c : param.param.name)
    {
        if (c == '-')
        {
            upper = true;
            continue;
        }
        name += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        upper = false;
    }
    return name;
}

TEST(FlightConfigurationOperationPinsGolden, ComeFromTheGoldensOpenRocket)
{
    EXPECT_EQ(Gui::Test::kFlightConfigurationOperationPinsCommit,
              "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(Gui::Test::kFlightConfigurationOperations.size(), 10U);
}

TEST_P(FlightConfigurationOperationsGolden, ReplaysOpenRocketsOperations)
{
    const OperationScenarioPin& scenario = GetParam();
    OperationReplay             replay(scenario);
    ASSERT_TRUE(replay.isLoaded());
    for (std::size_t step = 0; step < scenario.steps.size(); ++step)
    {
        const OperationStepPin& pin = scenario.steps[step];
        SCOPED_TRACE(pin.action);
        const std::vector<std::string> expected = expectedState(scenario, step);
        const std::vector<std::string> events   = replay.run(pin, expected);
        expectSameLines(events, expectedEvents(scenario, step), "event");
        expectSameLines(replay.state(), expected, "state");
        replay.expectQuestionsAsJava(pin);
        EXPECT_EQ(replay.prompts().unusedAnswers(), 0U);
    }
}

INSTANTIATE_TEST_SUITE_P(Scenarios, FlightConfigurationOperationsGolden,
                         ::testing::ValuesIn(Gui::Test::kFlightConfigurationOperations),
                         scenarioName);

}  // namespace

}  // namespace QtRocket::Gui
