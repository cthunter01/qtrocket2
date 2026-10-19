#pragma once

#include <QMainWindow>
#include <QString>

#include "QtRocket/document/events/DocumentChangeEvent.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/util/Signal.h"

class QCloseEvent;
class QShowEvent;
class QSplitter;
class QTabWidget;
class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{

class MainMenus;
class UndoRedoActions;
class WindowGeometry;

/// The window of one design (OpenRocket's BasicFrame): the menus, and a vertical splitter with
/// the tabs above (in Milestone 2 only "Rocket design", the design panel of the component tree
/// and the add buttons) and the rocket's figure below (the rocket panel). The document belongs to
/// the WindowManager, which makes the window and destroys it before the document.
///
/// - The design panel and the rocket panel are placeholders (empty widgets named designPanel and
///   rocketPanel) until the design panel (DesignPanel, run 2) and the rocket panel (RocketPanel,
///   run 3) take their places through setDesignPanel() and setRocketPanel().
/// - The title is BasicFrame.setTitle()'s: the rocket's name, then " (<file name>)" when the
///   document has a file, marked modified (the "*" of BasicFrame, "[*]" in Qt's title) when the
///   document is not saved. It is renewed whenever the document changes (every rocket event comes
///   to the document too); setSaved() and setFile() change nothing that the document tells, so
///   whoever calls them calls refreshTitle(). The file is also the window's file path (the
///   proxy icon of macOS).
/// - The size is BasicFrame's: 90 % of the screen, then the geometry the last main window had
///   (WindowGeometry "MainWindow"), and the splitter's divider at 0.4 of its height when the
///   window is first shown (JSplitPane.setDividerLocation(0.4)); further space goes half to each
///   side (resizeWeight 0.5). Where a further window opens is the WindowManager's
///   (WindowGeometry::cascade()).
/// - Closing the window (its close button, the platform) closes nothing by itself: it asks the
///   WindowManager through closeRequested(), as BasicFrame's DO_NOTHING_ON_CLOSE frame calls
///   closeAction().
///
/// Deviations: an "[*]" in the rocket's name is shown with a zero-width space after the "*"
/// ("[*\u200B]"), since Qt takes every "[*]" of a title for the place of the modified mark; the
/// splitter's sides cannot be collapsed (JSplitPane keeps their minimum sizes too).
class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    /// JSplitPane.setDividerLocation(0.4) of the splitter.
    static constexpr double kDividerLocation = 0.4;

    /// The window of @p document, which must outlive it, with the application's @p services
    /// (whose objects outlive it too).
    MainWindow(OpenRocketDocument& document, const GuiServices& services);

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] const GuiServices&  services() const noexcept { return m_services; }
    [[nodiscard]] MainMenus&          menus() const noexcept { return *m_menus; }
    [[nodiscard]] UndoRedoActions&    undoRedoActions() const noexcept { return *m_undoRedo; }

    /// The tabs ("tabbedPane"): only "Rocket design" in Milestone 2.
    [[nodiscard]] QTabWidget* tabs() const noexcept { return m_tabs; }
    /// The vertical splitter ("splitPane"): the tabs and the rocket panel.
    [[nodiscard]] QSplitter* splitter() const noexcept { return m_splitter; }
    /// The widget of the "Rocket design" tab.
    [[nodiscard]] QWidget* designPanel() const noexcept { return m_designPanel; }
    /// The widget below the tabs.
    [[nodiscard]] QWidget* rocketPanel() const noexcept { return m_rocketPanel; }

    /// Puts @p panel (not null) into the "Rocket design" tab, which takes it over, in place of
    /// the design panel there, which is deleted later.
    void setDesignPanel(QWidget* panel);
    /// Puts @p panel (not null) below the tabs, which takes it over, in place of the rocket
    /// panel there, which is deleted later.
    void setRocketPanel(QWidget* panel);

    /// Renews the title, the modified mark and the file path from the document.
    void refreshTitle();

    /// The title of the window of @p document, with Qt's modified mark: "[*]<rocket name>" and
    /// " (<file name>)" when the document has a file (BasicFrame.setTitle()).
    [[nodiscard]] static QString titleFor(const OpenRocketDocument& document);

Q_SIGNALS:
    /// The window was asked to close (its close button, the platform); the WindowManager closes
    /// it, asking first to save the design.
    void closeRequested();

protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    OpenRocketDocument*                                  m_document;
    GuiServices                                          m_services;
    UndoRedoActions*                                     m_undoRedo;
    MainMenus*                                           m_menus;
    QSplitter*                                           m_splitter;
    QTabWidget*                                          m_tabs;
    QWidget*                                             m_designPanel;
    QWidget*                                             m_rocketPanel;
    WindowGeometry*                                      m_geometry{nullptr};
    bool                                                 m_dividerPlaced{false};
    Signal<const DocumentChangeEvent&>::ScopedConnection m_documentChanged;
};

}  // namespace QtRocket::Gui
