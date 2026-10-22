#include "QtRocket/gui/main/MainWindow.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include <QCloseEvent>
#include <QMainWindow>
#include <QMenuBar>
#include <QObject>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/SplitPane.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

MainWindow::MainWindow(OpenRocketDocument& document, const GuiServices& services)
  : m_document(&document),
    m_services(services),
    m_undoRedo(makeChild<UndoRedoActions>(this, document)),
    m_menus(makeChild<MainMenus>(menuBar(), *m_undoRedo)),
    m_splitter(makeChild<SplitPane>(this, Qt::Vertical, kResizeWeight, kDividerLocation)),
    m_tabs(makeChild<QTabWidget>(m_splitter)),
    m_designPanel(makeChild<QWidget>(m_tabs)),
    m_rocketPanel(makeChild<QWidget>(m_splitter))
{
    m_splitter->setObjectName(QStringLiteral("splitPane"));
    m_tabs->setObjectName(QStringLiteral("tabbedPane"));
    m_designPanel->setObjectName(QStringLiteral("designPanel"));
    m_rocketPanel->setObjectName(QStringLiteral("rocketPanel"));

    //: BasicFrame.tab.Rocketdesign
    m_tabs->addTab(m_designPanel, tr("Rocket design"));
    m_splitter->addWidget(m_tabs);
    m_splitter->addWidget(m_rocketPanel);
    // Both sides stretch; how a change of the height is shared is JSplitPane's resize weight
    // (SplitPane).
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    setCentralWidget(m_splitter);

    setWindowIcon(Icons::windowIcon());
    m_documentChanged = m_document->documentChanged().connect(
        guarded([this] { refreshTitle(); }, QStringLiteral("Window title")));
    refreshTitle();

    // BasicFrame's order: 90 % of the screen, then the remembered size and place.
    WindowGeometry::setInitialSize(*this);
    m_geometry =
        makeChild<WindowGeometry>(this, m_services.preferences(), QStringLiteral("MainWindow"));
}

QSplitter* MainWindow::splitter() const noexcept
{
    return m_splitter;
}

DesignPanel* MainWindow::componentDesignPanel() const
{
    return qobject_cast<DesignPanel*>(m_designPanel);
}

RocketPanel* MainWindow::componentRocketPanel() const
{
    return qobject_cast<RocketPanel*>(m_rocketPanel);
}

void MainWindow::setDesignPanel(QWidget* panel)
{
    QTROCKET_ASSERT(panel != nullptr);
    if (panel == m_designPanel)
    {
        return;
    }
    const int     index = m_tabs->indexOf(m_designPanel);
    const QString text  = m_tabs->tabText(index);
    QWidget*      old   = m_designPanel;
    m_tabs->removeTab(index);
    m_tabs->insertTab(index, panel, text);
    m_tabs->setCurrentIndex(index);
    m_designPanel = panel;
    old->deleteLater();
}

void MainWindow::setRocketPanel(QWidget* panel)
{
    QTROCKET_ASSERT(panel != nullptr && panel != m_tabs);
    if (panel == m_rocketPanel)
    {
        return;
    }
    QWidget* const old = m_splitter->replaceWidget(m_splitter->indexOf(m_rocketPanel), panel);
    QTROCKET_ASSERT(old == m_rocketPanel);
    m_rocketPanel = panel;
    // replaceWidget() leaves the old panel without a parent.
    old->deleteLater();
}

void MainWindow::refreshTitle()
{
    setWindowTitle(titleFor(*m_document));
    setWindowModified(!m_document->isSaved());
    const std::optional<std::filesystem::path>& file = m_document->getFile();
    setWindowFilePath(file.has_value() ? QtStrings::fromPath(*file) : QString());
}

QString MainWindow::titleFor(const OpenRocketDocument& document)
{
    const QString placeholder = QStringLiteral("[*]");
    QString       name        = QtStrings::fromUtf8(document.getRocket().getName());
    // Qt takes every "[*]" of a title for the place of the modified mark (and has no escape that
    // works in every place): a zero-width space keeps the name's as it looks.
    name.replace(placeholder, QStringLiteral(u"[*\u200B]"));
    QString title = placeholder + name;
    if (const std::optional<std::filesystem::path>& file = document.getFile(); file.has_value())
    {
        title += QStringLiteral(" (") + QtStrings::fromPath(file->filename()) + QStringLiteral(")");
    }
    return title;
}

void MainWindow::requestConfigDialog(RocketComponent&                  component,
                                     std::span<RocketComponent* const> others, bool isNewComponent,
                                     bool rememberPreviousTab)
{
    const RocketComponent& rocket = m_document->getRocket();
    QTROCKET_ASSERT(&component.getRoot() == &rocket);
    QTROCKET_ASSERT(std::ranges::all_of(others, [&rocket](const RocketComponent* other) {
        return other != nullptr && &other->getRoot() == &rocket;
    }));
    const std::vector<RocketComponent*> otherComponents(others.begin(), others.end());
    Q_EMIT configDialogRequested(&component, otherComponents, isNewComponent, rememberPreviousTab);
}

void MainWindow::requestConfigDialogClose()
{
    Q_EMIT closeConfigDialogRequested();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // BasicFrame: DO_NOTHING_ON_CLOSE, and windowClosing() calls closeAction().
    event->ignore();
    Q_EMIT closeRequested();
}

}  // namespace QtRocket::Gui
