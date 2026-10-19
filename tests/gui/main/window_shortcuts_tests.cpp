// The keys of a design window's menu items are window shortcuts (MainMenus): they act wherever
// the focus is in the window, unless the focused widget takes the key for itself, as a text field
// takes the keys of its own editing (undo, copy, delete). A tree view takes none of them: the
// component tree needs nothing of what BasicFrame's frame does to its input map.

#include <memory>
#include <string>

#include <QAction>
#include <QApplication>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/Rocket.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

/// A shown, active window of a new design whose design panel holds a tree view and a text
/// field; the rocket was renamed "Mars" as an undo step "Rename", and the component items Copy,
/// Duplicate and Delete are enabled as RocketActions would enable them.
class WindowShortcutsTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_document   = OpenRocketDocumentFactory::createNewRocket();
        m_window     = std::make_unique<MainWindow>(*m_document, m_services.services());
        auto* panel  = makeChild<QWidget>(m_window.get());
        auto* layout = makeChild<QVBoxLayout>(panel);
        m_tree       = makeChild<QTreeView>(panel);
        m_field      = makeChild<QLineEdit>(panel);
        layout->addWidget(m_tree);
        layout->addWidget(m_field);
        m_window->setDesignPanel(panel);
        for (QAction* action :
             {menus().copyAction(), menus().duplicateAction(), menus().deleteAction()})
        {
            action->setEnabled(true);
        }
        m_document->addUndoPosition("Rename");
        m_document->getRocket().setName("Mars");

        m_window->show();
        m_window->activateWindow();
        ASSERT_TRUE(QTest::qWaitForWindowActive(m_window.get()));
    }

    void TearDown() override
    {
        m_window.reset();
        m_document.reset();
        GuiTest::TearDown();
    }

    [[nodiscard]] const MainMenus& menus() const { return m_window->menus(); }
    [[nodiscard]] std::string      rocketName() const { return m_document->getRocket().getName(); }

    /// Gives the focus to @p widget; whether it has it.
    [[nodiscard]] static bool focus(QWidget& widget)
    {
        widget.setFocus();
        return QApplication::focusWidget() == &widget;
    }

    /// Presses Ctrl (Cmd on macOS) and @p key in @p widget.
    static void pressMenuKey(QWidget& widget, Qt::Key key)
    {
        QTest::keyClick(&widget, key, Qt::ControlModifier);
    }

    QTreeView* m_tree{nullptr};
    QLineEdit* m_field{nullptr};

private:
    Gui::Test::TestGuiServices          m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<MainWindow>         m_window;
};

TEST_F(WindowShortcutsTest, ActInATreeView)
{
    ASSERT_TRUE(focus(*m_tree));
    const QSignalSpy newDesign(menus().newAction(), &QAction::triggered);
    const QSignalSpy close(menus().closeAction(), &QAction::triggered);
    const QSignalSpy quit(menus().quitAction(), &QAction::triggered);
    const QSignalSpy copy(menus().copyAction(), &QAction::triggered);
    const QSignalSpy duplicate(menus().duplicateAction(), &QAction::triggered);
    const QSignalSpy remove(menus().deleteAction(), &QAction::triggered);

    pressMenuKey(*m_tree, Qt::Key_Z);
    EXPECT_EQ(rocketName(), "Rocket");
    pressMenuKey(*m_tree, Qt::Key_N);
    pressMenuKey(*m_tree, Qt::Key_W);
    pressMenuKey(*m_tree, Qt::Key_Q);
    pressMenuKey(*m_tree, Qt::Key_C);
    pressMenuKey(*m_tree, Qt::Key_D);
    QTest::keyClick(m_tree, Qt::Key_Delete);
    EXPECT_EQ(newDesign.count(), 1);
    EXPECT_EQ(close.count(), 1);
    EXPECT_EQ(quit.count(), 1);
    EXPECT_EQ(copy.count(), 1);
    EXPECT_EQ(duplicate.count(), 1);
    EXPECT_EQ(remove.count(), 1);
}

TEST_F(WindowShortcutsTest, ActInATextFieldButForItsOwnEditingKeys)
{
    ASSERT_TRUE(focus(*m_field));
    const QSignalSpy newDesign(menus().newAction(), &QAction::triggered);
    const QSignalSpy close(menus().closeAction(), &QAction::triggered);
    const QSignalSpy quit(menus().quitAction(), &QAction::triggered);
    const QSignalSpy copy(menus().copyAction(), &QAction::triggered);
    const QSignalSpy duplicate(menus().duplicateAction(), &QAction::triggered);
    const QSignalSpy remove(menus().deleteAction(), &QAction::triggered);

    // The field's own undo, copy and delete.
    pressMenuKey(*m_field, Qt::Key_Z);
    EXPECT_EQ(rocketName(), "Mars");
    pressMenuKey(*m_field, Qt::Key_C);
    QTest::keyClick(m_field, Qt::Key_Delete);
    EXPECT_EQ(copy.count(), 0);
    EXPECT_EQ(remove.count(), 0);
    // The window's.
    pressMenuKey(*m_field, Qt::Key_N);
    pressMenuKey(*m_field, Qt::Key_W);
    pressMenuKey(*m_field, Qt::Key_Q);
    pressMenuKey(*m_field, Qt::Key_D);
    EXPECT_EQ(newDesign.count(), 1);
    EXPECT_EQ(close.count(), 1);
    EXPECT_EQ(quit.count(), 1);
    EXPECT_EQ(duplicate.count(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
