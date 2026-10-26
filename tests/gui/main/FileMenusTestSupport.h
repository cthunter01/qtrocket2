#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "GuiTestSupport.h"
#include "HookedUserPrompts.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"
#include "TestTempDir.h"

class QAction;
class QMenu;

namespace QtRocket::Gui
{
class MainWindow;
}

// The fixture of the tests of the menus that open design files (RecentFilesMenu,
// ExampleDesignsMenu): windows and their file operations over test services, and a directory of
// the test's own.

namespace QtRocket::Gui::Test
{

/// A window manager and its file operations (made on first use) over test services, whose
/// questions ScriptedUserPrompts answers (through HookedUserPrompts, so that a test can act
/// while a message is shown: beforeNextMessage()), and a directory of the test. TearDown()
/// destroys the operations, then the windows (and what waits for deleteLater()), before
/// GuiTest's checks.
class FileMenusTest : public GuiTest
{
protected:
    void TearDown() override;

    /// The manager, made the first time with @p behaviour, with its file operations.
    [[nodiscard]] WindowManager& manager(
        WindowManager::LastWindowBehaviour behaviour = WindowManager::LastWindowBehaviour::EXIT);
    [[nodiscard]] DocumentFileOperations& operations();
    /// Destroys the file operations (the manager stays).
    void destroyOperations();

    [[nodiscard]] ScriptedUserPrompts& prompts() noexcept { return m_prompts.scripted(); }
    [[nodiscard]] InMemoryPreferences& preferences() noexcept { return m_services.preferences(); }
    [[nodiscard]] const QtRocket::Test::TempDir& directory() const noexcept { return m_directory; }
    /// The questions and messages so far.
    [[nodiscard]] const std::vector<ScriptedUserPrompts::Prompt>& asked() const noexcept
    {
        return m_prompts.scripted().prompts();
    }
    /// Calls @p hook before the next message the questions show, once (as the event loop of a
    /// message box may run another window's events).
    void beforeNextMessage(std::function<void()> hook);
    /// Closes @p window of the manager, and deletes it at once, before the next message the
    /// questions show (another window's event while the message box runs: on macOS a design file
    /// the Finder opens, the Dock's Quit).
    void closeBeforeNextMessage(MainWindow& window);

    /// Writes a design whose rocket is named @p rocketName to @p name in the directory.
    [[nodiscard]] std::filesystem::path writeDesign(const std::filesystem::path& name,
                                                    std::string_view             rocketName);

    /// Deletes what was given to deleteLater().
    static void deleteLaterNow();

private:
    QtRocket::Test::TempDir m_directory;
    TestGuiServices         m_services;
    HookedUserPrompts       m_prompts;
    /// The test services with the hooked questions.
    GuiServices                             m_hookedServices{m_services.preferences(),
                                                             m_services.services().materials(),
                                                             m_services.services().motors(),
                                                             m_services.services().presets(),
                                                             m_services.services().loadingContext(),
                                                             m_prompts};
    std::unique_ptr<WindowManager>          m_manager;
    std::unique_ptr<DocumentFileOperations> m_operations;
};

/// The texts of the items of @p menu in order (QAction::text(), with the mnemonic marks), "----"
/// for a separator.
[[nodiscard]] QStringList menuTexts(const QMenu& menu);

/// The item of @p menu whose text is @p text (QAction::text()); a test failure and null when
/// there is none.
[[nodiscard]] QAction* menuItem(const QMenu& menu, const QString& text);

}  // namespace QtRocket::Gui::Test
