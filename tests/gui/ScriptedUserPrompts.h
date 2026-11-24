#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include <QColor>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Uuid.h"

class QObject;
class QWidget;

namespace QtRocket
{
class MotorMount;
class OpenRocketDocument;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class GuiServices;
class SimulationExecutor;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The UserPrompts of the tests: every question is answered from a script the test writes
/// beforehand, in the order the questions come, and every question and message is recorded.
/// A question the script has no answer for, or an answer of another kind than the question,
/// fails the test and gives the cancelling answer (Cancel, No, no file), so that no test can hang
/// on a dialog or go on with an answer it did not mean; an answer left unused when the double
/// is destroyed fails the test too. Messages need no answer.
///
/// The questions of Milestone 3 are answered in the same way: the value dialogs (ignition,
/// deployment, separation: a choice, or nullopt for Cancel), the rename (with "Save as default"
/// clicks before the answer), the motor chooser (a motor, or nullopt for Cancel) and the parts
/// library (a preset, none, or closed without a choice); the database load problems are a
/// message. Their requests are kept as values (the motor chooser's mount by its name and id).
///
/// The questions of Milestone 4 too: the deletion of simulations (an answer), the CSV import
/// settings and the colour (a value, or nullopt for Cancel), and the run dialog. The run dialog is
/// no question that needs a script: unless the script's next answer is one of the run dialog's,
/// the simulations really run, as the run dialog runs them (O-M4-8): the run dialog's batch
/// (SimulationBatch) of the request, through the request's executor, with its failures shown
/// through this double (recorded as messages), and the answer is how the batch ended. The tests'
/// executor (ManualSimulationExecutor) runs the jobs in the next turn of the event loop in its
/// IMMEDIATE mode (the double waits for them in the event loop), at once in its STEPPED mode
/// (runAll()), and never in its NEVER mode, which fails the test (the batch is cancelled); a
/// request without a document, an executor or a simulation fails the test and answers a cancelled
/// batch. A script answers the run dialog with how it ended, without running anything
/// (answerRunSimulations(): a run the test cancels, or one that must not run), or with an action
/// of the test that is given the request (answerRunSimulationsWith()). Their requests are kept as
/// values (the simulations of a run by their names).
class ScriptedUserPrompts final : public UserPrompts
{
public:
    /// What was asked or shown.
    enum class Kind
    {
        INSERT_POSITION,
        DISCARD_CHANGES,
        SAVE_CHANGES,
        OVERWRITE,
        STORAGE_OPTIONS,
        YES_NO,
        DECAL_FILE,
        OPEN_FILES,
        SAVE_FILE,
        DESIGN_INFORMATION,
        MESSAGE,
        WARNINGS,
        ERRORS_AND_WARNINGS,
        INTERNAL_ERROR,
        IGNITION,
        DEPLOYMENT,
        SEPARATION,
        RENAME_CONFIGURATION,
        MOTOR,
        PRESET,
        DATABASE_PROBLEMS,
        DELETE_SIMULATIONS,
        RUN_SIMULATIONS,
        WIND_CSV_IMPORT_SETTINGS,
        COLOR,
    };

    /// One question or message, as the code under test asked it.
    struct Prompt
    {
        Kind    kind;
        QString title;
        /// The question's texts: the design's or the file's name, the message lines and
        /// warnings, the request's name filters, ...; the name of the question's enum constant
        /// for the fixed questions.
        QStringList texts;
    };

    /// What editDesignInformation() does with the document (the user's edits); nothing by
    /// default.
    using DesignInformationEdit = std::function<void(OpenRocketDocument&)>;

    /// A question of chooseMotor(), as it was asked: the mount (its name and its component id,
    /// so that nothing keeps a pointer to it) and the flight configuration.
    struct MotorQuestion
    {
        QString               mountName;
        Uuid                  mountId;
        FlightConfigurationId configurationId;
    };

    /// A question of runSimulations(), as it was asked: the simulations by their names (so that
    /// nothing keeps them), the window, the document and the executor, only to compare.
    struct RunSimulationsQuestion
    {
        QStringList               simulationNames;
        const QObject*            window   = nullptr;
        const OpenRocketDocument* document = nullptr;
        const SimulationExecutor* executor = nullptr;
    };

    /// What runSimulations() does with the request when the script says so: the test's own run
    /// of the simulations (through the request's executor), which answers how it ended.
    using RunSimulationsAction =
        std::function<RunSimulationsAnswer(QWidget* parent, const RunSimulationsRequest& request)>;

    /// A question of askColor(), as it was asked.
    struct ColorQuestion
    {
        QColor  initial;
        QString title;
    };

    ScriptedUserPrompts() = default;
    ~ScriptedUserPrompts() override;
    ScriptedUserPrompts(const ScriptedUserPrompts&)            = delete;
    ScriptedUserPrompts& operator=(const ScriptedUserPrompts&) = delete;
    ScriptedUserPrompts(ScriptedUserPrompts&&)                 = delete;
    ScriptedUserPrompts& operator=(ScriptedUserPrompts&&)      = delete;

    // The script: the answer of the next question of that kind, in order.
    void answerInsertPosition(InsertPositionAnswer answer);
    void answerDiscardChanges(DiscardAnswer answer);
    void answerSaveChanges(SaveChangesAnswer answer);
    void answerOverwrite(bool overwrite);
    void answerStorageOptions(std::optional<bool> saveSimulationData);
    void answerYesNo(bool yes);
    void answerDecalFile(std::optional<OpenFileAnswer> answer);
    void answerOpenFiles(std::optional<OpenFileAnswer> answer);
    void answerSaveFile(std::optional<SaveFileAnswer> answer);
    void answerDesignInformation(DesignInformationEdit edit = {});
    void answerInternalError(InternalErrorAnswer answer);
    /// nullopt answers Cancel.
    void answerIgnition(std::optional<IgnitionChoice> answer);
    void answerDeployment(std::optional<DeploymentChoice> answer);
    void answerSeparation(std::optional<SeparationChoice> answer);
    /// Clicks "Save as default" with each of @p saveAsDefault (the text field's text then) in
    /// order, through the request's callback (given the question's parent as the dialog), and
    /// then answers @p answer.
    void answerRenameConfiguration(RenameConfigurationAnswer answer,
                                   QStringList               saveAsDefault = {});
    /// nullopt answers Cancel; a choice needs a motor (a test failure otherwise).
    void answerMotor(std::optional<MotorChoice> answer);
    /// A CHOSEN answer needs a preset and the others none (a test failure otherwise).
    void answerPreset(PresetAnswer answer);
    void answerDeleteSimulations(DeleteSimulationsAnswer answer);
    /// The run dialog ends as @p answer says, and nothing runs.
    void answerRunSimulations(RunSimulationsAnswer answer);
    /// The run dialog is @p action (not empty: a test failure otherwise), with the question's
    /// parent and request; it answers what @p action returns.
    void answerRunSimulationsWith(RunSimulationsAction action);
    /// nullopt answers Cancel; settings need a unit in each column (a test failure otherwise).
    void answerWindCsvImportSettings(std::optional<WindCsvImportSettings> answer);
    /// nullopt answers Cancel.
    void answerColor(std::optional<QColor> answer);

    /// Every question and message so far, in order.
    [[nodiscard]] const std::vector<Prompt>& prompts() const noexcept { return m_prompts; }
    /// The requests of askOpenFiles() and askSaveFile() so far, in order.
    [[nodiscard]] const std::vector<OpenFileRequest>& openFileRequests() const noexcept
    {
        return m_openFileRequests;
    }
    [[nodiscard]] const std::vector<SaveFileRequest>& saveFileRequests() const noexcept
    {
        return m_saveFileRequests;
    }
    /// The requests of the Milestone 3 questions so far, each kind in order.
    [[nodiscard]] const std::vector<IgnitionRequest>& ignitionRequests() const noexcept
    {
        return m_ignitionRequests;
    }
    [[nodiscard]] const std::vector<DeploymentRequest>& deploymentRequests() const noexcept
    {
        return m_deploymentRequests;
    }
    [[nodiscard]] const std::vector<SeparationRequest>& separationRequests() const noexcept
    {
        return m_separationRequests;
    }
    [[nodiscard]] const std::vector<MotorQuestion>& motorQuestions() const noexcept
    {
        return m_motorQuestions;
    }
    [[nodiscard]] const std::vector<PresetRequest>& presetRequests() const noexcept
    {
        return m_presetRequests;
    }
    /// The problems of every showDatabaseProblems() so far, in order.
    [[nodiscard]] const std::vector<DatabaseProblem>& databaseProblems() const noexcept
    {
        return m_databaseProblems;
    }
    /// The requests of the Milestone 4 questions so far, each kind in order.
    [[nodiscard]] const std::vector<RunSimulationsQuestion>& runSimulationsQuestions()
        const noexcept
    {
        return m_runSimulationsQuestions;
    }
    [[nodiscard]] const std::vector<WindCsvImportSettings>& windCsvImportRequests() const noexcept
    {
        return m_windCsvImportRequests;
    }
    [[nodiscard]] const std::vector<ColorQuestion>& colorQuestions() const noexcept
    {
        return m_colorQuestions;
    }
    /// The answers of the script not asked for yet.
    [[nodiscard]] std::size_t unusedAnswers() const noexcept { return m_script.size(); }

    /// The name of @p kind ("SAVE_CHANGES").
    [[nodiscard]] static std::string_view kindName(Kind kind);

    [[nodiscard]] InsertPositionAnswer askInsertPosition(QWidget*               parent,
                                                         InsertPositionQuestion question) override;
    [[nodiscard]] DiscardAnswer        askDiscardChanges(QWidget*        parent,
                                                         DiscardQuestion question) override;
    [[nodiscard]] SaveChangesAnswer    askSaveChanges(QWidget*       parent,
                                                      const QString& designName) override;
    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override;
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* parent,
                                                        bool     saveSimulationData) override;
    [[nodiscard]] bool                askYesNo(QWidget* parent, const Question& question) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) override;
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override;
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) override;
    void editDesignInformation(QWidget* parent, const GuiServices& services,
                               OpenRocketDocument& document) override;
    /// Recorded with the configuration's name, the scope, the event and the delay as the texts.
    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(
        QWidget* parent, const IgnitionRequest& request) override;
    /// Recorded with the configuration's name, the scope, the event, the altitude, the delay
    /// and whether the device is in the bottom stage ("true", "false") as the texts.
    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override;
    /// Recorded with the configuration's name, the scope, the event, the altitude and the delay
    /// as the texts.
    [[nodiscard]] std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) override;
    /// Recorded with the name as the text; the cancelling answer is CANCEL.
    [[nodiscard]] RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) override;
    /// Recorded with the mount's name and the configuration's id as the texts.
    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override;
    /// Recorded with the component's English name, the preset type, the two diameters ("none"
    /// without one) and whether the nose cone is flipped as the texts; the cancelling answer is
    /// CLOSED.
    [[nodiscard]] PresetAnswer choosePreset(QWidget* parent, const GuiServices& services,
                                            const PresetRequest& request) override;
    /// Recorded with the count as the text; the cancelling answer neither confirms nor ticks.
    [[nodiscard]] DeleteSimulationsAnswer askDeleteSimulations(QWidget*    parent,
                                                               std::size_t count) override;
    /// Recorded with the simulations' names as the texts; without a scripted answer, the
    /// simulations run (see the class comment).
    [[nodiscard]] RunSimulationsAnswer runSimulations(
        QWidget* parent, const RunSimulationsRequest& request) override;
    /// Recorded with the header row ("true", "false"), the separator and the four columns' names
    /// as the texts; the cancelling answer is nullopt.
    [[nodiscard]] std::optional<WindCsvImportSettings> askWindCsvImportSettings(
        QWidget* parent, const WindCsvImportSettings& initial) override;
    /// Recorded with the title as the title and the initial colour (QColor::name(HexArgb)) as the
    /// text; the cancelling answer is nullopt.
    [[nodiscard]] std::optional<QColor> askColor(QWidget* parent, const QColor& initial,
                                                 const QString& title) override;
    void showMessage(QWidget* parent, const Message& message) override;
    void showWarnings(QWidget* parent, const WarningsMessage& message) override;
    /// Recorded with the lines, then the errors, then the warnings as the texts.
    void showErrorsAndWarnings(QWidget* parent, const ErrorsAndWarningsMessage& message) override;
    /// Recorded with the failure's kind (BUG, EXCEPTION, UNKNOWN, ERROR_CONDITION), its message
    /// and its context as the texts; the cancelling answer is CONTINUE.
    /// Recorded with the database ("THRUST_CURVES", "COMPONENT_PRESETS"), the file and the
    /// message of each problem as the texts.
    void showDatabaseProblems(QWidget* parent, std::span<const DatabaseProblem> problems) override;
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override;

    /// The name of @p scope ("ALL_CONFIGURATIONS").
    [[nodiscard]] static QString scopeName(ConfigurationScope scope);

private:
    /// A scripted rename: the "Save as default" clicks, then the answer.
    struct RenameScript
    {
        RenameConfigurationAnswer answer;
        QStringList               saveAsDefault;
    };

    /// A scripted run dialog: the test's action, or without one the answer.
    struct RunScript
    {
        RunSimulationsAnswer answer;
        RunSimulationsAction action;
    };

    using Answer =
        std::variant<InsertPositionAnswer, DiscardAnswer, SaveChangesAnswer, bool,
                     std::optional<bool>, std::optional<OpenFileAnswer>,
                     std::optional<SaveFileAnswer>, DesignInformationEdit, InternalErrorAnswer,
                     std::optional<IgnitionChoice>, std::optional<DeploymentChoice>,
                     std::optional<SeparationChoice>, RenameScript, std::optional<MotorChoice>,
                     PresetAnswer, DeleteSimulationsAnswer, RunScript,
                     std::optional<WindCsvImportSettings>, std::optional<QColor>>;

    struct Scripted
    {
        Kind   kind;
        Answer answer;
    };

    [[nodiscard]] static Prompt makePrompt(Kind kind, QString title, QStringList texts);
    void                        script(Kind kind, Answer answer);
    /// Records the question and returns the script's next answer, or @p cancel when the script
    /// has none of this kind (a test failure).
    template <class T>
    [[nodiscard]] T next(Prompt prompt, T cancel);

    std::deque<Scripted>                m_script;
    std::vector<Prompt>                 m_prompts;
    std::vector<OpenFileRequest>        m_openFileRequests;
    std::vector<SaveFileRequest>        m_saveFileRequests;
    std::vector<IgnitionRequest>        m_ignitionRequests;
    std::vector<DeploymentRequest>      m_deploymentRequests;
    std::vector<SeparationRequest>      m_separationRequests;
    std::vector<MotorQuestion>          m_motorQuestions;
    std::vector<PresetRequest>          m_presetRequests;
    std::vector<DatabaseProblem>        m_databaseProblems;
    std::vector<RunSimulationsQuestion> m_runSimulationsQuestions;
    std::vector<WindCsvImportSettings>  m_windCsvImportRequests;
    std::vector<ColorQuestion>          m_colorQuestions;
};

}  // namespace QtRocket::Gui::Test
