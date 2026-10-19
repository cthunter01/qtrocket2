#include "QtRocket/gui/main/MainWindow.h"

#include <algorithm>
#include <filesystem>
#include <optional>

#include <QCloseEvent>
#include <QEvent>
#include <QList>
#include <QMainWindow>
#include <QMenuBar>
#include <QObject>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSplitter>
#include <QString>
#include <QTabWidget>
#include <QWidget>
#include <Qt>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The height below which a side of the splitter does not go (JSplitPane's
/// getMinimumSizeOfComponent()).
[[nodiscard]] int sideMinimumHeight(const QWidget* side)
{
    return side == nullptr ? 0 : std::max(side->minimumHeight(), side->minimumSizeHint().height());
}

/// The part of BasicSplitPaneUI.distributeSpace() for a side @p lacking below its minimum
/// @p lackingMinimum: it takes what it lacks from the @p other side when that one keeps its own
/// minimum @p otherMinimum; when it cannot, only a negative height goes.
void takeWhatItLacks(int& lacking, int& other, int lackingMinimum, int otherMinimum)
{
    if (other - (lackingMinimum - lacking) < otherMinimum)
    {
        if (lacking < 0)
        {
            other += lacking;
            lacking = 0;
        }
    }
    else
    {
        other -= lackingMinimum - lacking;
        lacking = lackingMinimum;
    }
}

}  // namespace

MainWindow::MainWindow(OpenRocketDocument& document, const GuiServices& services)
  : m_document(&document),
    m_services(services),
    m_undoRedo(makeChild<UndoRedoActions>(this, document)),
    m_menus(makeChild<MainMenus>(menuBar(), *m_undoRedo)),
    m_splitter(makeChild<QSplitter>(this, Qt::Vertical)),
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
    // (eventFilter()).
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->installEventFilter(this);
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // After the divider's first place: until then the sizes are the layout's own.
    if (watched == m_splitter && m_dividerPlaced && event->type() == QEvent::Resize)
    {
        if (const auto* resize = dynamic_cast<const QResizeEvent*>(event);
            resize != nullptr && resize->oldSize().isValid())
        {
            distributeSpace(resize->size().height() - resize->oldSize().height());
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::distributeSpace(int change)
{
    // The splitter has not laid its sides out for its new height yet: sizes() are the old ones.
    const QList<int> sizes = m_splitter->sizes();
    if (change == 0 || sizes.size() != 2)
    {
        return;
    }
    const int topExtra =
        static_cast<int>(kResizeWeight * static_cast<double>(change));  // Java's (int) cast
    int        top           = sizes.at(0) + topExtra;
    int        bottom        = sizes.at(1) + (change - topExtra);
    const int  topMinimum    = sideMinimumHeight(m_splitter->widget(0));
    const int  bottomMinimum = sideMinimumHeight(m_splitter->widget(1));
    const bool topValid      = top >= topMinimum;
    const bool bottomValid   = bottom >= bottomMinimum;
    if (!topValid && !bottomValid)
    {
        if (top < 0)
        {
            bottom += top;
            top = 0;
        }
        else if (bottom < 0)
        {
            top += bottom;
            bottom = 0;
        }
    }
    else if (!topValid)
    {
        takeWhatItLacks(top, bottom, topMinimum, bottomMinimum);
    }
    else if (!bottomValid)
    {
        takeWhatItLacks(bottom, top, bottomMinimum, topMinimum);
    }
    m_splitter->setSizes({std::max(top, 0), std::max(bottom, 0)});
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // BasicFrame: DO_NOTHING_ON_CLOSE, and windowClosing() calls closeAction().
    event->ignore();
    Q_EMIT closeRequested();
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (m_dividerPlaced)
    {
        return;
    }
    // JSplitPane.setDividerLocation(0.4): (int) ((height - divider size) * 0.4) above the divider.
    const int length = m_splitter->height() - m_splitter->handleWidth();
    if (length > 0)
    {
        m_dividerPlaced = true;
        const int top   = static_cast<int>(static_cast<double>(length) * kDividerLocation);
        m_splitter->setSizes({top, length - top});
    }
}

}  // namespace QtRocket::Gui
