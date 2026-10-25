#include "ScriptedUserPrompts.h"

#include <filesystem>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui::Test
{

namespace
{

[[nodiscard]] QString insertPositionQuestionName(UserPrompts::InsertPositionQuestion question)
{
    switch (question)
    {
        case UserPrompts::InsertPositionQuestion::BODY_COMPONENT:
            return QStringLiteral("BODY_COMPONENT");
        case UserPrompts::InsertPositionQuestion::STAGE:
            return QStringLiteral("STAGE");
    }
    QTROCKET_UNREACHABLE();
}

[[nodiscard]] QString discardQuestionName(UserPrompts::DiscardQuestion question)
{
    switch (question)
    {
        case UserPrompts::DiscardQuestion::COMPONENT_CHANGES:
            return QStringLiteral("COMPONENT_CHANGES");
        case UserPrompts::DiscardQuestion::COMPONENT_ADDITION:
            return QStringLiteral("COMPONENT_ADDITION");
        case UserPrompts::DiscardQuestion::PREFERENCE_CHANGES:
            return QStringLiteral("PREFERENCE_CHANGES");
    }
    QTROCKET_UNREACHABLE();
}

[[nodiscard]] QString severityName(UserPrompts::Severity severity)
{
    switch (severity)
    {
        case UserPrompts::Severity::INFORMATION:
            return QStringLiteral("INFORMATION");
        case UserPrompts::Severity::WARNING:
            return QStringLiteral("WARNING");
        case UserPrompts::Severity::ERROR:
            return QStringLiteral("ERROR");
    }
    QTROCKET_UNREACHABLE();
}

[[nodiscard]] QString databaseKindName(UserPrompts::DatabaseKind database)
{
    switch (database)
    {
        case UserPrompts::DatabaseKind::THRUST_CURVES:
            return QStringLiteral("THRUST_CURVES");
        case UserPrompts::DatabaseKind::COMPONENT_PRESETS:
            return QStringLiteral("COMPONENT_PRESETS");
    }
    QTROCKET_UNREACHABLE();
}

[[nodiscard]] QString presetOutcomeName(UserPrompts::PresetOutcome outcome)
{
    switch (outcome)
    {
        case UserPrompts::PresetOutcome::CHOSEN:
            return QStringLiteral("CHOSEN");
        case UserPrompts::PresetOutcome::NONE:
            return QStringLiteral("NONE");
        case UserPrompts::PresetOutcome::CLOSED:
            return QStringLiteral("CLOSED");
    }
    QTROCKET_UNREACHABLE();
}

/// @p value as the texts of a prompt have a number (QString::number()).
[[nodiscard]] QString numberText(double value)
{
    return QString::number(value);
}

/// "true" or "false".
[[nodiscard]] QString booleanText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

/// @p diameter, or "none" without one.
[[nodiscard]] QString diameterText(const std::optional<double>& diameter)
{
    return diameter.has_value() ? numberText(*diameter) : QStringLiteral("none");
}

[[nodiscard]] QString failureKindName(Guard::Failure::Kind kind)
{
    switch (kind)
    {
        case Guard::Failure::Kind::BUG:
            return QStringLiteral("BUG");
        case Guard::Failure::Kind::EXCEPTION:
            return QStringLiteral("EXCEPTION");
        case Guard::Failure::Kind::UNKNOWN:
            return QStringLiteral("UNKNOWN");
        case Guard::Failure::Kind::ERROR_CONDITION:
            return QStringLiteral("ERROR_CONDITION");
    }
    QTROCKET_UNREACHABLE();
}

}  // namespace

ScriptedUserPrompts::Prompt ScriptedUserPrompts::makePrompt(Kind kind, QString title,
                                                            QStringList texts)
{
    return {.kind = kind, .title = std::move(title), .texts = std::move(texts)};
}

ScriptedUserPrompts::~ScriptedUserPrompts()
{
    for (const Scripted& unused : m_script)
    {
        ADD_FAILURE() << "the script's answer to a " << kindName(unused.kind)
                      << " question was never asked for";
    }
}

std::string_view ScriptedUserPrompts::kindName(Kind kind)
{
    switch (kind)
    {
        case Kind::INSERT_POSITION:
            return "INSERT_POSITION";
        case Kind::DISCARD_CHANGES:
            return "DISCARD_CHANGES";
        case Kind::SAVE_CHANGES:
            return "SAVE_CHANGES";
        case Kind::OVERWRITE:
            return "OVERWRITE";
        case Kind::STORAGE_OPTIONS:
            return "STORAGE_OPTIONS";
        case Kind::YES_NO:
            return "YES_NO";
        case Kind::DECAL_FILE:
            return "DECAL_FILE";
        case Kind::OPEN_FILES:
            return "OPEN_FILES";
        case Kind::SAVE_FILE:
            return "SAVE_FILE";
        case Kind::DESIGN_INFORMATION:
            return "DESIGN_INFORMATION";
        case Kind::MESSAGE:
            return "MESSAGE";
        case Kind::WARNINGS:
            return "WARNINGS";
        case Kind::ERRORS_AND_WARNINGS:
            return "ERRORS_AND_WARNINGS";
        case Kind::INTERNAL_ERROR:
            return "INTERNAL_ERROR";
        case Kind::IGNITION:
            return "IGNITION";
        case Kind::DEPLOYMENT:
            return "DEPLOYMENT";
        case Kind::SEPARATION:
            return "SEPARATION";
        case Kind::RENAME_CONFIGURATION:
            return "RENAME_CONFIGURATION";
        case Kind::MOTOR:
            return "MOTOR";
        case Kind::PRESET:
            return "PRESET";
        case Kind::DATABASE_PROBLEMS:
            return "DATABASE_PROBLEMS";
    }
    QTROCKET_UNREACHABLE();
}

QString ScriptedUserPrompts::scopeName(ConfigurationScope scope)
{
    switch (scope)
    {
        case ConfigurationScope::ALL_CONFIGURATIONS:
            return QStringLiteral("ALL_CONFIGURATIONS");
        case ConfigurationScope::THIS_CONFIGURATION:
            return QStringLiteral("THIS_CONFIGURATION");
    }
    QTROCKET_UNREACHABLE();
}

void ScriptedUserPrompts::answerInsertPosition(InsertPositionAnswer answer)
{
    script(Kind::INSERT_POSITION, Answer(std::in_place_type<InsertPositionAnswer>, answer));
}

void ScriptedUserPrompts::answerDiscardChanges(DiscardAnswer answer)
{
    script(Kind::DISCARD_CHANGES, Answer(std::in_place_type<DiscardAnswer>, answer));
}

void ScriptedUserPrompts::answerSaveChanges(SaveChangesAnswer answer)
{
    script(Kind::SAVE_CHANGES, Answer(std::in_place_type<SaveChangesAnswer>, answer));
}

void ScriptedUserPrompts::answerOverwrite(bool overwrite)
{
    script(Kind::OVERWRITE, Answer(std::in_place_type<bool>, overwrite));
}

void ScriptedUserPrompts::answerStorageOptions(std::optional<bool> saveSimulationData)
{
    script(Kind::STORAGE_OPTIONS,
           Answer(std::in_place_type<std::optional<bool>>, saveSimulationData));
}

void ScriptedUserPrompts::answerYesNo(bool yes)
{
    script(Kind::YES_NO, Answer(std::in_place_type<bool>, yes));
}

void ScriptedUserPrompts::answerDecalFile(std::optional<OpenFileAnswer> answer)
{
    script(Kind::DECAL_FILE,
           Answer(std::in_place_type<std::optional<OpenFileAnswer>>, std::move(answer)));
}

void ScriptedUserPrompts::answerOpenFiles(std::optional<OpenFileAnswer> answer)
{
    script(Kind::OPEN_FILES,
           Answer(std::in_place_type<std::optional<OpenFileAnswer>>, std::move(answer)));
}

void ScriptedUserPrompts::answerSaveFile(std::optional<SaveFileAnswer> answer)
{
    script(Kind::SAVE_FILE,
           Answer(std::in_place_type<std::optional<SaveFileAnswer>>, std::move(answer)));
}

void ScriptedUserPrompts::answerDesignInformation(DesignInformationEdit edit)
{
    script(Kind::DESIGN_INFORMATION,
           Answer(std::in_place_type<DesignInformationEdit>, std::move(edit)));
}

void ScriptedUserPrompts::answerInternalError(InternalErrorAnswer answer)
{
    script(Kind::INTERNAL_ERROR, Answer(std::in_place_type<InternalErrorAnswer>, answer));
}

void ScriptedUserPrompts::answerIgnition(std::optional<IgnitionChoice> answer)
{
    script(Kind::IGNITION, Answer(std::in_place_type<std::optional<IgnitionChoice>>, answer));
}

void ScriptedUserPrompts::answerDeployment(std::optional<DeploymentChoice> answer)
{
    script(Kind::DEPLOYMENT, Answer(std::in_place_type<std::optional<DeploymentChoice>>, answer));
}

void ScriptedUserPrompts::answerSeparation(std::optional<SeparationChoice> answer)
{
    script(Kind::SEPARATION, Answer(std::in_place_type<std::optional<SeparationChoice>>, answer));
}

void ScriptedUserPrompts::answerRenameConfiguration(RenameConfigurationAnswer answer,
                                                    QStringList               saveAsDefault)
{
    script(Kind::RENAME_CONFIGURATION,
           Answer(std::in_place_type<RenameScript>,
                  RenameScript{.answer        = std::move(answer),
                               .saveAsDefault = std::move(saveAsDefault)}));
}

void ScriptedUserPrompts::answerMotor(std::optional<MotorChoice> answer)
{
    if (answer.has_value() && answer->motor == nullptr)
    {
        ADD_FAILURE() << "a scripted motor choice without a motor";
    }
    script(Kind::MOTOR, Answer(std::in_place_type<std::optional<MotorChoice>>, std::move(answer)));
}

void ScriptedUserPrompts::answerPreset(PresetAnswer answer)
{
    if ((answer.outcome == PresetOutcome::CHOSEN) != (answer.preset != nullptr))
    {
        ADD_FAILURE() << "a scripted " << QtStrings::toUtf8(presetOutcomeName(answer.outcome))
                      << " parts library answer " << (answer.preset != nullptr ? "with" : "without")
                      << " a preset";
    }
    script(Kind::PRESET, Answer(std::in_place_type<PresetAnswer>, std::move(answer)));
}

UserPrompts::InsertPositionAnswer ScriptedUserPrompts::askInsertPosition(
    QWidget* /*parent*/, InsertPositionQuestion question)
{
    return next(makePrompt(Kind::INSERT_POSITION, {}, {insertPositionQuestionName(question)}),
                InsertPositionAnswer{});
}

UserPrompts::DiscardAnswer ScriptedUserPrompts::askDiscardChanges(QWidget* /*parent*/,
                                                                  DiscardQuestion question)
{
    return next(makePrompt(Kind::DISCARD_CHANGES, {}, {discardQuestionName(question)}),
                DiscardAnswer{});
}

UserPrompts::SaveChangesAnswer ScriptedUserPrompts::askSaveChanges(QWidget* /*parent*/,
                                                                   const QString& designName)
{
    return next(makePrompt(Kind::SAVE_CHANGES, {}, {designName}), SaveChangesAnswer::CANCEL);
}

bool ScriptedUserPrompts::askOverwrite(QWidget* /*parent*/, const QString& fileName)
{
    return next(makePrompt(Kind::OVERWRITE, {}, {fileName}), false);
}

std::optional<bool> ScriptedUserPrompts::askStorageOptions(QWidget* /*parent*/,
                                                           bool saveSimulationData)
{
    return next(makePrompt(Kind::STORAGE_OPTIONS, {},
                           {saveSimulationData ? QStringLiteral("all simulated data")
                                               : QStringLiteral("only summary data")}),
                std::optional<bool>());
}

bool ScriptedUserPrompts::askYesNo(QWidget* /*parent*/, const Question& question)
{
    return next(
        makePrompt(Kind::YES_NO, question.title, {severityName(question.severity), question.text}),
        false);
}

std::optional<UserPrompts::OpenFileAnswer> ScriptedUserPrompts::askDecalFile(
    QWidget* /*parent*/, const QString& message, const std::filesystem::path& directory)
{
    return next(makePrompt(Kind::DECAL_FILE, {}, {message, QtStrings::fromPath(directory)}),
                std::optional<OpenFileAnswer>());
}

std::optional<UserPrompts::OpenFileAnswer> ScriptedUserPrompts::askOpenFiles(
    QWidget* /*parent*/, const OpenFileRequest& request)
{
    m_openFileRequests.push_back(request);
    return next(makePrompt(Kind::OPEN_FILES, request.title, request.nameFilters),
                std::optional<OpenFileAnswer>());
}

std::optional<UserPrompts::SaveFileAnswer> ScriptedUserPrompts::askSaveFile(
    QWidget* /*parent*/, const SaveFileRequest& request)
{
    m_saveFileRequests.push_back(request);
    return next(makePrompt(Kind::SAVE_FILE, request.title, {request.fileName}),
                std::optional<SaveFileAnswer>());
}

void ScriptedUserPrompts::editDesignInformation(QWidget* /*parent*/,
                                                const GuiServices& /*services*/,
                                                OpenRocketDocument& document)
{
    const DesignInformationEdit edit =
        next(makePrompt(Kind::DESIGN_INFORMATION, {}, {}), DesignInformationEdit());
    if (edit)
    {
        edit(document);
    }
}

std::optional<UserPrompts::IgnitionChoice> ScriptedUserPrompts::askIgnition(
    QWidget* /*parent*/, const IgnitionRequest& request)
{
    m_ignitionRequests.push_back(request);
    return next(makePrompt(Kind::IGNITION, {},
                           {request.configurationName, scopeName(request.initial.scope),
                            QtStrings::fromUtf8(QtRocket::name(request.initial.event)),
                            numberText(request.initial.delay)}),
                std::optional<IgnitionChoice>());
}

std::optional<UserPrompts::DeploymentChoice> ScriptedUserPrompts::askDeployment(
    QWidget* /*parent*/, const DeploymentRequest& request)
{
    m_deploymentRequests.push_back(request);
    const DeploymentConfiguration& deployment = request.initial.deployment;
    return next(
        makePrompt(Kind::DEPLOYMENT, {},
                   {request.configurationName, scopeName(request.initial.scope),
                    QtStrings::fromUtf8(deployEventName(deployment.getDeployEvent())),
                    numberText(deployment.getDeployAltitude()),
                    numberText(deployment.getDeployDelay()), booleanText(request.bottomStage)}),
        std::optional<DeploymentChoice>());
}

std::optional<UserPrompts::SeparationChoice> ScriptedUserPrompts::askSeparation(
    QWidget* /*parent*/, const SeparationRequest& request)
{
    m_separationRequests.push_back(request);
    const StageSeparationConfiguration& separation = request.initial.separation;
    return next(
        makePrompt(Kind::SEPARATION, {},
                   {request.configurationName, scopeName(request.initial.scope),
                    QtStrings::fromUtf8(separationEventName(separation.getSeparationEvent())),
                    numberText(separation.getSeparationAltitude()),
                    numberText(separation.getSeparationDelay())}),
        std::optional<SeparationChoice>());
}

UserPrompts::RenameConfigurationAnswer ScriptedUserPrompts::askRenameConfiguration(
    QWidget* parent, const RenameConfigurationRequest& request)
{
    const RenameScript script = next(makePrompt(Kind::RENAME_CONFIGURATION, {}, {request.name}),
                                     RenameScript{.answer = {}, .saveAsDefault = {}});
    for (const QString& name : script.saveAsDefault)
    {
        if (!request.saveAsDefault)
        {
            ADD_FAILURE() << "the script clicks Save as default, but the rename has no callback";
            break;
        }
        request.saveAsDefault(parent, name);
    }
    return script.answer;
}

std::optional<UserPrompts::MotorChoice> ScriptedUserPrompts::chooseMotor(
    QWidget* /*parent*/, const GuiServices& /*services*/, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    const RocketComponent& component = asComponent(mount);
    const MotorQuestion    question{.mountName       = QtStrings::fromUtf8(component.getName()),
                                    .mountId         = component.getId(),
                                    .configurationId = configurationId};
    m_motorQuestions.push_back(question);
    return next(makePrompt(Kind::MOTOR, {},
                           {question.mountName, QtStrings::fromUtf8(configurationId.toString())}),
                std::optional<MotorChoice>());
}

UserPrompts::PresetAnswer ScriptedUserPrompts::choosePreset(QWidget* /*parent*/,
                                                            const GuiServices& /*services*/,
                                                            const PresetRequest& request)
{
    m_presetRequests.push_back(request);
    const std::optional<ComponentPresetType> type = presetTypeOf(request.componentKind);
    return next(makePrompt(Kind::PRESET, {},
                           {QtStrings::fromUtf8(displayName(request.componentKind)),
                            type.has_value() ? QtStrings::fromUtf8(componentPresetTypeName(*type))
                                             : QStringLiteral("none"),
                            diameterText(request.foreDiameter), diameterText(request.aftDiameter),
                            booleanText(request.flippedNoseCone)}),
                PresetAnswer{});
}

void ScriptedUserPrompts::showMessage(QWidget* /*parent*/, const Message& message)
{
    QStringList texts{severityName(message.severity)};
    texts.append(message.lines);
    m_prompts.push_back(makePrompt(Kind::MESSAGE, message.title, texts));
}

void ScriptedUserPrompts::showWarnings(QWidget* /*parent*/, const WarningsMessage& message)
{
    QStringList texts = message.lines;
    texts.append(message.warnings);
    m_prompts.push_back(makePrompt(Kind::WARNINGS, message.title, texts));
}

void ScriptedUserPrompts::showErrorsAndWarnings(QWidget* /*parent*/,
                                                const ErrorsAndWarningsMessage& message)
{
    QStringList texts = message.lines;
    texts.append(message.errors);
    texts.append(message.warnings);
    m_prompts.push_back(makePrompt(Kind::ERRORS_AND_WARNINGS, message.title, texts));
}

void ScriptedUserPrompts::showDatabaseProblems(QWidget* /*parent*/,
                                               std::span<const DatabaseProblem> problems)
{
    QStringList texts;
    for (const DatabaseProblem& problem : problems)
    {
        m_databaseProblems.push_back(problem);
        texts.append(databaseKindName(problem.database));
        texts.append(QtStrings::fromPath(problem.file));
        texts.append(problem.message);
    }
    m_prompts.push_back(makePrompt(Kind::DATABASE_PROBLEMS, {}, texts));
}

UserPrompts::InternalErrorAnswer ScriptedUserPrompts::showInternalError(
    QWidget* /*parent*/, const Guard::Failure& failure)
{
    return next(makePrompt(Kind::INTERNAL_ERROR, {},
                           {failureKindName(failure.kind), failure.message, failure.context}),
                InternalErrorAnswer::CONTINUE);
}

void ScriptedUserPrompts::script(Kind kind, Answer answer)
{
    m_script.push_back(Scripted{.kind = kind, .answer = std::move(answer)});
}

template <class T>
T ScriptedUserPrompts::next(Prompt prompt, T cancel)
{
    const Kind kind = prompt.kind;
    m_prompts.push_back(std::move(prompt));
    if (m_script.empty())
    {
        ADD_FAILURE() << "unexpected " << kindName(kind)
                      << " question: the script has no answer left";
        return cancel;
    }
    if (m_script.front().kind != kind)
    {
        ADD_FAILURE() << "a " << kindName(kind)
                      << " question, but the script's next answer is to a "
                      << kindName(m_script.front().kind) << " question";
        return cancel;
    }
    T answer = std::get<T>(std::move(m_script.front().answer));
    m_script.pop_front();
    return answer;
}

}  // namespace QtRocket::Gui::Test
