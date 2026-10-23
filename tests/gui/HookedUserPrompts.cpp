#include "HookedUserPrompts.h"

#include <filesystem>
#include <functional>
#include <optional>
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
