#pragma once

// The windows of the exit suites that open OpenRocket's designs (OpenDesignsExitGolden,
// open_designs_exit_tests.cpp, and the flight block's OpenDesignsFlightBlockExitGolden,
// open_designs_flight_block_exit_tests.cpp): the application's windows without its start-up, a
// design opened in them through File > Open of a New design's window, as a user opens it. Each
// suite gives them the services it needs (the environment of the designs, the questions, the
// executor of the simulations).

#include <filesystem>
#include <memory>

#include "QtRocket/gui/main/OpenRocketClipboard.h"

namespace QtRocket::Gui
{
class ConfigDialogRequests;
class DocumentFileOperations;
class GuiServices;
class MainWindow;
class WindowManager;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

class ScriptedUserPrompts;

/// Delivers the posted events until the windows, their layouts and their figures are done.
void settleWindows();

/// The windows of the application without its start-up (the start is tested in tests/gui/app/):
/// a WindowManager over @p services (the last window's close exits), with a clipboard of its own,
/// ConfigDialogRequests and DocumentFileOperations, as ApplicationSession connects them.
class OpenDesignWindows
{
public:
    /// The windows of @p services, which must outlive them.
    explicit OpenDesignWindows(const GuiServices& services);
    /// The file operations and the dialog requests go first, then the windows (with their
    /// documents and dialogs); the deletions they posted are delivered.
    ~OpenDesignWindows();
    OpenDesignWindows(const OpenDesignWindows&)            = delete;
    OpenDesignWindows& operator=(const OpenDesignWindows&) = delete;
    OpenDesignWindows(OpenDesignWindows&&)                 = delete;
    OpenDesignWindows& operator=(OpenDesignWindows&&)      = delete;

    /// Opens @p file through File > Open of a New design's window, the file chooser answered by
    /// @p prompts (the services' scripted questions: answerOpenFiles()), and returns the window it
    /// opened in, which took the New design's place (settled); null, with a test failure, when
    /// none did or its design has another file.
    [[nodiscard]] MainWindow* openThroughFileOpen(const std::filesystem::path& file,
                                                  ScriptedUserPrompts&         prompts);

    [[nodiscard]] WindowManager& windows() const noexcept { return *m_windows; }

private:
    OpenRocketClipboard                     m_clipboard;
    std::unique_ptr<WindowManager>          m_windows;
    std::unique_ptr<ConfigDialogRequests>   m_requests;
    std::unique_ptr<DocumentFileOperations> m_fileOperations;
};

}  // namespace QtRocket::Gui::Test
