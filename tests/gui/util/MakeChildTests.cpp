#include "QtRocket/gui/util/MakeChild.h"

#include <QLabel>
#include <QObject>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using MakeChild = Test::GuiTest;

TEST_F(MakeChild, TheParentOwnsTheChild)
{
    const QObject witness;
    bool          destroyed = false;
    {
        QObject parent;
        auto*   child = makeChild<QObject>(&parent);
        EXPECT_EQ(child->parent(), &parent);
        EXPECT_EQ(parent.children(), QObjectList{child});
        QObject::connect(child, &QObject::destroyed, &witness, [&destroyed] { destroyed = true; });
    }
    EXPECT_TRUE(destroyed);
}

TEST_F(MakeChild, PassesTheArgumentsBeforeTheParent)
{
    QWidget       window;
    const QLabel* label = makeChild<QLabel>(&window, QStringLiteral("Body tube"));
    EXPECT_EQ(label->text(), QStringLiteral("Body tube"));
    EXPECT_EQ(label->parentWidget(), &window);
}

TEST_F(MakeChild, MakesALayoutOfAWidget)
{
    QWidget            window;
    const QVBoxLayout* layout = makeChild<QVBoxLayout>(&window);
    EXPECT_EQ(window.layout(), layout);
}

TEST_F(MakeChild, ANullParentIsABug)
{
    QObject* const parent = nullptr;
    EXPECT_THROW(static_cast<void>(makeChild<QObject>(parent)), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
