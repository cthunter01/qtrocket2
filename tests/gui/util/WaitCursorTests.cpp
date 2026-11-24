// The wait cursor while a file is read or written on the GUI thread.

#include "QtRocket/gui/util/WaitCursor.h"

#include <optional>

#include <QCursor>
#include <QGuiApplication>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using WaitCursorTest = Test::GuiTest;

/// The shape of the application's override cursor, or nullopt without one.
[[nodiscard]] std::optional<Qt::CursorShape> overrideShape()
{
    const QCursor* const cursor = QGuiApplication::overrideCursor();
    if (cursor == nullptr)
    {
        return std::nullopt;
    }
    return cursor->shape();
}

}  // namespace

TEST_F(WaitCursorTest, ShowsTheWaitCursorUntilItEnds)
{
    ASSERT_EQ(overrideShape(), std::nullopt);
    {
        const WaitCursor wait;
        EXPECT_EQ(overrideShape(), Qt::WaitCursor);
    }
    EXPECT_EQ(overrideShape(), std::nullopt);
}

TEST_F(WaitCursorTest, PutsTheCursorBeforeItBack)
{
    // Nested inside another override cursor (a drag's, a busy outer operation's), which comes
    // back when the wait ends.
    QGuiApplication::setOverrideCursor(QCursor(Qt::CrossCursor));
    {
        const WaitCursor outer;
        {
            const WaitCursor inner;
            EXPECT_EQ(overrideShape(), Qt::WaitCursor);
        }
        EXPECT_EQ(overrideShape(), Qt::WaitCursor);
    }
    EXPECT_EQ(overrideShape(), Qt::CrossCursor);
    QGuiApplication::restoreOverrideCursor();
    EXPECT_EQ(overrideShape(), std::nullopt);
}

}  // namespace QtRocket::Gui
