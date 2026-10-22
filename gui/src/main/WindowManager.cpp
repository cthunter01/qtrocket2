#include "QtRocket/gui/main/WindowManager.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <utility>
#include <vector>

#include <QAction>
#include <QLayout>
#include <QMenuBar>
#include <QObject>
#include <QString>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// A closed window and its document, which the event loop deletes (deleteLater()): the window
/// first, then the document. Deleting the window at once could pull it from under the code that
/// closed it (its own menu item, its close button's event).
class ClosedWindow final : public QObject
{
public:
    ClosedWindow(std::unique_ptr<OpenRocketDocument> document, std::unique_ptr<MainWindow> window,
                 QObject* parent)
      : QObject(parent), m_document(std::move(document)), m_window(std::move(window))
    {
    }

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<MainWindow>         m_window;
};

/// Calls an action when it goes out of scope, also when an exception passes.
template <std::invocable Action>
class OnScopeExit
{
public:
    explicit OnScopeExit(Action action) : m_action(std::move(action)) { }
    ~OnScopeExit() { m_action(); }
    OnScopeExit(const OnScopeExit&)            = delete;
    OnScopeExit& operator=(const OnScopeExit&) = delete;
    OnScopeExit(OnScopeExit&&)                 = delete;
    OnScopeExit& operator=(OnScopeExit&&)      = delete;

private:
    Action m_action;
};

}  // namespace

WindowManager::LastWindowBehaviour WindowManager::platformLastWindowBehaviour() noexcept
{
#ifdef __APPLE__
    // macOS: an application stays open without a window (BasicFrame.closeAction()).
    return LastWindowBehaviour::KEEP_RUNNING;
#else
    // Linux and Windows.
    return LastWindowBehaviour::EXIT;
#endif
}

WindowManager::WindowManager(const GuiServices& services, LastWindowBehaviour behaviour,
                             QObject* parent)
  : WindowManager(services, OpenRocketClipboard::instance(), behaviour, parent)
{
}

WindowManager::WindowManager(const GuiServices& services, OpenRocketClipboard& clipboard,
                             LastWindowBehaviour behaviour, QObject* parent)
  : QObject(parent), m_services(services), m_clipboard(&clipboard), m_behaviour(behaviour)
{
}

WindowManager::~WindowManager()
{
    // The last opened first; each window before its document (Entry).
    while (!m_entries.empty())
    {
        m_entries.pop_back();
    }
}

MainWindow& WindowManager::newDesign(MainWindow* origin)
{
    MainWindow& window = open(OpenRocketDocumentFactory::createNewRocket(), true);
    closeIfReplaceable(origin, window);
    return window;
}

MainWindow& WindowManager::addDocument(std::unique_ptr<OpenRocketDocument> document,
                                       MainWindow*                         origin)
{
    QTROCKET_ASSERT(document != nullptr);
    MainWindow& window = open(std::move(document), false);
    closeIfReplaceable(origin, window);
    return window;
}

bool WindowManager::close(MainWindow& window)
{
    Entry* const entry = find(window);
    if (entry == nullptr || entry->closing)
    {
        return false;
    }
    entry->closing = true;
    // While the question runs (an event loop), the window must not be closed twice; afterwards
    // it is open again, unless it was closed.
    const OnScopeExit reopen([this, &window] noexcept {
        if (Entry* const current = find(window); current != nullptr)
        {
            current->closing = false;
        }
    });
    if (!confirmClose(window))
    {
        return false;
    }
    destroyLater(window);
    // BasicFrame.closeAction(): ComponentConfigDialog.disposeDialog() after the frame's
    // dispose() (the window lives until the event loop deletes it).
    window.requestConfigDialogClose();
    if (m_entries.empty())
    {
        lastWindowClosed();
    }
    return true;
}

bool WindowManager::quit()
{
    if (m_quitting)
    {
        return false;
    }
    m_quitting = true;
    const OnScopeExit done([this] noexcept { m_quitting = false; });
    while (!m_entries.empty())
    {
        if (!close(*m_entries.back()->window))
        {
            return false;
        }
    }
    Q_EMIT exitRequested();
    return true;
}

void WindowManager::setSaveHook(SaveHook hook)
{
    m_saveHook = std::move(hook);
}

std::vector<MainWindow*> WindowManager::windows() const
{
    std::vector<MainWindow*> result;
    result.reserve(m_entries.size());
    for (const std::unique_ptr<Entry>& entry : m_entries)
    {
        result.push_back(entry->window.get());
    }
    return result;
}

MainWindow* WindowManager::findWindow(const Rocket& rocket) const noexcept
{
    for (const std::unique_ptr<Entry>& entry : m_entries)
    {
        if (&entry->document->getRocket() == &rocket)
        {
            return entry->window.get();
        }
    }
    return nullptr;
}

bool WindowManager::isReplaceable(const MainWindow& window) const noexcept
{
    const Entry* const entry = find(window);
    return entry != nullptr && entry->replaceable;
}

MainWindow& WindowManager::open(std::unique_ptr<OpenRocketDocument> document, bool replaceable)
{
    auto entry         = std::make_unique<Entry>();
    entry->document    = std::move(document);
    entry->window      = std::make_unique<MainWindow>(*entry->document, m_services);
    entry->replaceable = replaceable;
    MainWindow& window = *entry->window;
    // BasicFrame's DesignPanel, which selects the first stage; the tree has the focus when the
    // window is shown.
    auto* designPanel = makeChild<DesignPanel>(&window, window, *m_clipboard);
    window.setDesignPanel(designPanel);
    designPanel->takeTheSpotlight();
    // BasicFrame.positionRelativeToExistingFrames(), after the remembered geometry.
    if (!m_entries.empty())
    {
        WindowGeometry::cascade(window, *m_entries.back()->window);
    }
    connectMenus(window.menus(), &window);
    connect(&window, &MainWindow::closeRequested, this,
            guarded([this, &window] { static_cast<void>(close(window)); },
                    QStringLiteral("Close design")));
    m_entries.push_back(std::move(entry));
    Q_EMIT menusCreated(&window.menus(), &window);
    // The layout's sizes before the platform window exists: a window whose minimum size is set
    // as it is shown passes it on to the platform window, which the offscreen platform cannot
    // take (it warns "This plugin does not support propagateSizeHints()" on every start).
    window.layout()->activate();
    window.show();
    Q_EMIT windowOpened(&window);
    return window;
}

void WindowManager::connectMenus(MainMenus& menus, MainWindow* window)
{
    connect(
        menus.newAction(), &QAction::triggered, this,
        guarded([this, window] { static_cast<void>(newDesign(window)); }, QStringLiteral("New")));
    if (window != nullptr)
    {
        connect(menus.closeAction(), &QAction::triggered, this,
                guarded([this, window] { static_cast<void>(close(*window)); },
                        QStringLiteral("Close design")));
    }
    connect(menus.quitAction(), &QAction::triggered, this,
            guarded([this] { static_cast<void>(quit()); }, QStringLiteral("Quit")));
}

void WindowManager::closeIfReplaceable(MainWindow* origin, MainWindow& replacement)
{
    if (origin == nullptr || origin == &replacement)
    {
        return;
    }
    const Entry* const entry = find(*origin);
    if (entry == nullptr || !entry->replaceable || !origin->document().isSaved())
    {
        return;
    }
    // The new window takes the place of the one it replaces.
    replacement.move(origin->pos());
    static_cast<void>(close(*origin));
}

bool WindowManager::confirmClose(MainWindow& window)
{
    const OpenRocketDocument& document = window.document();
    if (document.isSaved())
    {
        return true;
    }
    // BasicFrame first closes the component configuration dialog.
    window.requestConfigDialogClose();
    switch (m_services.prompts().askSaveChanges(
        &window, QtStrings::fromUtf8(document.getRocket().getName())))
    {
        case UserPrompts::SaveChangesAnswer::SAVE:
            if (m_saveHook)
            {
                return m_saveHook(window);
            }
            m_services.prompts().showMessage(
                &window,
                {.severity = UserPrompts::Severity::WARNING,
                 //: BasicFrame.dlg.title
                 .title = tr("Design not saved"),
                 // QtRocket's own text: saving comes with the file operations later in Milestone 2.
                 .lines = {tr("Saving designs is not available in this version of QtRocket.")}});
            return false;
        case UserPrompts::SaveChangesAnswer::DISCARD:
            return true;
        case UserPrompts::SaveChangesAnswer::CANCEL:
            return false;
    }
    QTROCKET_UNREACHABLE();
}

WindowManager::Entry* WindowManager::find(const MainWindow& window) const noexcept
{
    for (const std::unique_ptr<Entry>& entry : m_entries)
    {
        if (entry->window.get() == &window)
        {
            return entry.get();
        }
    }
    return nullptr;
}

void WindowManager::destroyLater(const MainWindow& window)
{
    const auto found = std::ranges::find_if(
        m_entries,
        [&window](const std::unique_ptr<Entry>& entry) { return entry->window.get() == &window; });
    QTROCKET_ASSERT(found != m_entries.end());
    const std::unique_ptr<Entry> entry = std::move(*found);
    m_entries.erase(found);
    entry->window->hide();
    makeChild<ClosedWindow>(this, std::move(entry->document), std::move(entry->window))
        ->deleteLater();
}

void WindowManager::lastWindowClosed()
{
    if (m_quitting)
    {
        // quit() exits once every window is closed.
        return;
    }
    if (m_behaviour == LastWindowBehaviour::EXIT)
    {
        Q_EMIT exitRequested();
        return;
    }
    // DummyFrameMenuOSX.createDummyDialog(): made once, kept.
    if (m_noWindowMenuBar == nullptr)
    {
        m_noWindowMenuBar = std::make_unique<QMenuBar>();
        m_noWindowMenuBar->setObjectName(QStringLiteral("noWindowMenuBar"));
        m_noWindowMenus = makeChild<MainMenus>(m_noWindowMenuBar.get());
        connectMenus(*m_noWindowMenus, nullptr);
        Q_EMIT menusCreated(m_noWindowMenus, nullptr);
    }
}

}  // namespace QtRocket::Gui
