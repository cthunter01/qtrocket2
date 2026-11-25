#include "OpenDesignWindows.h"

#include <filesystem>
#include <memory>
#include <optional>
#include <system_error>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/ConfigDialogRequests.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"
#include "ScriptedUserPrompts.h"

namespace QtRocket::Gui::Test
{

void settleWindows()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

OpenDesignWindows::OpenDesignWindows(const GuiServices& services)
  : m_windows(std::make_unique<WindowManager>(services, m_clipboard,
                                              WindowManager::LastWindowBehaviour::EXIT)),
    m_requests(std::make_unique<ConfigDialogRequests>(*m_windows)),
    m_fileOperations(std::make_unique<DocumentFileOperations>(*m_windows, services))
{
}

OpenDesignWindows::~OpenDesignWindows()
{
    m_fileOperations = nullptr;
    m_requests       = nullptr;
    m_windows        = nullptr;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

MainWindow* OpenDesignWindows::openThroughFileOpen(const std::filesystem::path& file,
                                                   ScriptedUserPrompts&         prompts)
{
    const QPointer<MainWindow> start = &m_windows->newDesign();
    settleWindows();
    const MainWindow* const newDesign = start.data();
    QTROCKET_ASSERT(newDesign != nullptr);
    prompts.answerOpenFiles(
        UserPrompts::OpenFileAnswer{.files = {file}, .directory = file.parent_path()});
    newDesign->menus().openAction()->trigger();
    settleWindows();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // The design took the place of the New design, whose window is gone.
    if (m_windows->windowCount() != 1U || !start.isNull())
    {
        ADD_FAILURE() << m_windows->windowCount()
                      << " windows after File > Open, the New design's closed: " << start.isNull();
        return nullptr;
    }
    MainWindow* const                          window = m_windows->windows().front();
    const std::optional<std::filesystem::path> opened = window->document().getFile();
    std::error_code                            error;
    if (!opened.has_value() || !std::filesystem::equivalent(*opened, file, error))
    {
        ADD_FAILURE() << "the window's design has another file";
        return nullptr;
    }
    return window;
}

}  // namespace QtRocket::Gui::Test
