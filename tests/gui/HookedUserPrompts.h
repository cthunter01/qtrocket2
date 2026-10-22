#pragma once

#include <filesystem>
#include <functional>
#include <optional>

#include <QString>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "ScriptedUserPrompts.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{
class GuiServices;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The questions of a ScriptedUserPrompts of its own (scripted(): the script and the record),
/// which also runs a hook of the test before the next message it shows (showMessage(),
/// showWarnings() or showErrorsAndWarnings()), once: the event loop of a message box may run
/// other code, such as the platform's events that close a window or start a reopen.
class HookedUserPrompts final : public UserPrompts
{
public:
    HookedUserPrompts()                                    = default;
    ~HookedUserPrompts() override                          = default;
    HookedUserPrompts(const HookedUserPrompts&)            = delete;
    HookedUserPrompts& operator=(const HookedUserPrompts&) = delete;
    HookedUserPrompts(HookedUserPrompts&&)                 = delete;
    HookedUserPrompts& operator=(HookedUserPrompts&&)      = delete;

    [[nodiscard]] ScriptedUserPrompts&       scripted() noexcept { return m_scripted; }
    [[nodiscard]] const ScriptedUserPrompts& scripted() const noexcept { return m_scripted; }
    /// Calls @p hook before the next message, once.
    void beforeNextMessage(std::function<void()> hook);

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
    void showMessage(QWidget* parent, const Message& message) override;
    void showWarnings(QWidget* parent, const WarningsMessage& message) override;
    void showErrorsAndWarnings(QWidget* parent, const ErrorsAndWarningsMessage& message) override;
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override;

private:
    /// Runs the hook, if there is one, and forgets it.
    void runHook();

    ScriptedUserPrompts   m_scripted;
    std::function<void()> m_hook;
};

}  // namespace QtRocket::Gui::Test
