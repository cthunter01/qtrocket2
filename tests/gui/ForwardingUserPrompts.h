#pragma once

#include <filesystem>
#include <optional>

#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui::Test
{

/// A UserPrompts that passes every question and message on to another one (a test's
/// ScriptedUserPrompts), the base of a test double that does something of its own in one
/// question: it derives from this one and overrides that question (a question that runs an event
/// loop, as a modal dialog does, in which the test closes a window).
class ForwardingUserPrompts : public UserPrompts
{
public:
    explicit ForwardingUserPrompts(UserPrompts& delegate) noexcept : m_delegate(&delegate) { }

    /// The prompts every question goes to.
    [[nodiscard]] UserPrompts& delegate() const noexcept { return *m_delegate; }

    [[nodiscard]] InsertPositionAnswer askInsertPosition(QWidget*               parent,
                                                         InsertPositionQuestion question) override
    {
        return m_delegate->askInsertPosition(parent, question);
    }
    [[nodiscard]] DiscardAnswer askDiscardChanges(QWidget*        parent,
                                                  DiscardQuestion question) override
    {
        return m_delegate->askDiscardChanges(parent, question);
    }
    [[nodiscard]] SaveChangesAnswer askSaveChanges(QWidget*       parent,
                                                   const QString& designName) override
    {
        return m_delegate->askSaveChanges(parent, designName);
    }
    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override
    {
        return m_delegate->askOverwrite(parent, fileName);
    }
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* parent,
                                                        bool     saveSimulationData) override
    {
        return m_delegate->askStorageOptions(parent, saveSimulationData);
    }
    [[nodiscard]] bool askYesNo(QWidget* parent, const Question& question) override
    {
        return m_delegate->askYesNo(parent, question);
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) override
    {
        return m_delegate->askDecalFile(parent, message, directory);
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override
    {
        return m_delegate->askOpenFiles(parent, request);
    }
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget*               parent,
                                                            const SaveFileRequest& request) override
    {
        return m_delegate->askSaveFile(parent, request);
    }
    void editDesignInformation(QWidget* parent, OpenRocketDocument& document) override
    {
        m_delegate->editDesignInformation(parent, document);
    }
    void showMessage(QWidget* parent, const Message& message) override
    {
        m_delegate->showMessage(parent, message);
    }
    void showWarnings(QWidget* parent, const WarningsMessage& message) override
    {
        m_delegate->showWarnings(parent, message);
    }
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override
    {
        return m_delegate->showInternalError(parent, failure);
    }

private:
    UserPrompts* m_delegate;
};

/// @p services with the questions of @p prompts.
[[nodiscard]] inline GuiServices withPrompts(const GuiServices& services, UserPrompts& prompts)
{
    return {services.preferences(), services.materials(), services.motors(),
            services.loadingContext(), prompts};
}

}  // namespace QtRocket::Gui::Test
