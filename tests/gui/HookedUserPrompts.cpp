#include "HookedUserPrompts.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <utility>

#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket::Gui::Test
{

void HookedUserPrompts::beforeNextMessage(std::function<void()> hook)
{
    m_hook = std::move(hook);
}

UserPrompts::InsertPositionAnswer HookedUserPrompts::askInsertPosition(
    QWidget* parent, InsertPositionQuestion question)
{
    return m_scripted.askInsertPosition(parent, question);
}

UserPrompts::DiscardAnswer HookedUserPrompts::askDiscardChanges(QWidget*        parent,
                                                                DiscardQuestion question)
{
    return m_scripted.askDiscardChanges(parent, question);
}

UserPrompts::SaveChangesAnswer HookedUserPrompts::askSaveChanges(QWidget*       parent,
                                                                 const QString& designName)
{
    return m_scripted.askSaveChanges(parent, designName);
}

bool HookedUserPrompts::askOverwrite(QWidget* parent, const QString& fileName)
{
    return m_scripted.askOverwrite(parent, fileName);
}

std::optional<bool> HookedUserPrompts::askStorageOptions(QWidget* parent, bool saveSimulationData)
{
    return m_scripted.askStorageOptions(parent, saveSimulationData);
}

bool HookedUserPrompts::askYesNo(QWidget* parent, const Question& question)
{
    return m_scripted.askYesNo(parent, question);
}

std::optional<UserPrompts::OpenFileAnswer> HookedUserPrompts::askDecalFile(
    QWidget* parent, const QString& message, const std::filesystem::path& directory)
{
    return m_scripted.askDecalFile(parent, message, directory);
}

std::optional<UserPrompts::OpenFileAnswer> HookedUserPrompts::askOpenFiles(
    QWidget* parent, const OpenFileRequest& request)
{
    return m_scripted.askOpenFiles(parent, request);
}

std::optional<UserPrompts::SaveFileAnswer> HookedUserPrompts::askSaveFile(
    QWidget* parent, const SaveFileRequest& request)
{
    return m_scripted.askSaveFile(parent, request);
}

void HookedUserPrompts::editDesignInformation(QWidget* parent, const GuiServices& services,
                                              OpenRocketDocument& document)
{
    m_scripted.editDesignInformation(parent, services, document);
}

std::optional<UserPrompts::IgnitionChoice> HookedUserPrompts::askIgnition(
    QWidget* parent, const IgnitionRequest& request)
{
    return m_scripted.askIgnition(parent, request);
}

std::optional<UserPrompts::DeploymentChoice> HookedUserPrompts::askDeployment(
    QWidget* parent, const DeploymentRequest& request)
{
    return m_scripted.askDeployment(parent, request);
}

std::optional<UserPrompts::SeparationChoice> HookedUserPrompts::askSeparation(
    QWidget* parent, const SeparationRequest& request)
{
    return m_scripted.askSeparation(parent, request);
}

UserPrompts::RenameConfigurationAnswer HookedUserPrompts::askRenameConfiguration(
    QWidget* parent, const RenameConfigurationRequest& request)
{
    return m_scripted.askRenameConfiguration(parent, request);
}

std::optional<UserPrompts::MotorChoice> HookedUserPrompts::chooseMotor(
    QWidget* parent, const GuiServices& services, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    return m_scripted.chooseMotor(parent, services, mount, configurationId);
}

UserPrompts::PresetAnswer HookedUserPrompts::choosePreset(QWidget*             parent,
                                                          const GuiServices&   services,
                                                          const PresetRequest& request)
{
    return m_scripted.choosePreset(parent, services, request);
}

void HookedUserPrompts::showMessage(QWidget* parent, const Message& message)
{
    runHook();
    m_scripted.showMessage(parent, message);
}

void HookedUserPrompts::showWarnings(QWidget* parent, const WarningsMessage& message)
{
    runHook();
    m_scripted.showWarnings(parent, message);
}

void HookedUserPrompts::showErrorsAndWarnings(QWidget*                        parent,
                                              const ErrorsAndWarningsMessage& message)
{
    runHook();
    m_scripted.showErrorsAndWarnings(parent, message);
}

void HookedUserPrompts::showDatabaseProblems(QWidget*                         parent,
                                             std::span<const DatabaseProblem> problems)
{
    runHook();
    m_scripted.showDatabaseProblems(parent, problems);
}

UserPrompts::InternalErrorAnswer HookedUserPrompts::showInternalError(QWidget*              parent,
                                                                      const Guard::Failure& failure)
{
    return m_scripted.showInternalError(parent, failure);
}

void HookedUserPrompts::runHook()
{
    if (m_hook)
    {
        const std::function<void()> hook = std::exchange(m_hook, {});
        hook();
    }
}

}  // namespace QtRocket::Gui::Test
