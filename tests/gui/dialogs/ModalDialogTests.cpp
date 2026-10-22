#include "QtRocket/gui/dialogs/ModalDialog.h"

#include <memory>

#include <QCoreApplication>
#include <QDialog>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"

namespace QtRocket::Gui
{

namespace
{

using ModalDialogTest = Test::GuiTest;

/// Destroys the parent of the modal dialog, and so the dialog, while it is open.
struct DestroyParent
{
    std::unique_ptr<QWidget>* parent;

    void operator()(QWidget& /*dialog*/) const { parent->reset(); }
};

/// Accepts the modal dialog.
struct Accept
{
    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<QDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        dialog->accept();
    }
};

TEST_F(ModalDialogTest, ADialogWithoutAParentIsDeletedWithTheHolder)
{
    QPointer<QDialog> made;
    {
        ModalDialog<QDialog> dialog(nullptr);
        const QDialog* const shown = dialog.get();
        ASSERT_NE(shown, nullptr);
        EXPECT_EQ(shown->parentWidget(), nullptr);
        made = dialog.get();
        const Gui::Test::NextModalDialog answer{Accept{}};
        EXPECT_EQ(dialog.exec(), QDialog::Accepted);
    }
    EXPECT_TRUE(made.isNull());
}

TEST_F(ModalDialogTest, ADialogWithAParentIsItsChildAndDeletedLater)
{
    QWidget           parent;
    QPointer<QDialog> made;
    {
        const ModalDialog<QDialog> dialog(&parent);
        const QDialog* const       child = dialog.get();
        ASSERT_NE(child, nullptr);
        EXPECT_EQ(child->parentWidget(), &parent);
        made = dialog.get();
    }
    EXPECT_FALSE(made.isNull());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(made.isNull());
}

TEST_F(ModalDialogTest, ADialogDestroyedWithItsParentWhileOpenIsRejected)
{
    auto                 parent = std::make_unique<QWidget>();
    ModalDialog<QDialog> dialog(parent.get());
    {
        const Gui::Test::NextModalDialog destroy(DestroyParent{.parent = &parent});
        EXPECT_EQ(dialog.exec(), QDialog::Rejected);
    }
    EXPECT_EQ(dialog.get(), nullptr);
    // A dialog that is gone is not shown again.
    EXPECT_EQ(dialog.exec(), QDialog::Rejected);
}

}  // namespace

}  // namespace QtRocket::Gui
