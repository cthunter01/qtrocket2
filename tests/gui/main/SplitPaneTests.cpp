// The split pane of OpenRocket's windows (JSplitPane: setDividerLocation(double),
// setResizeWeight(), BasicSplitPaneUI.distributeSpace()). MainWindowTests checks the vertical one
// of the main window; these check a horizontal one, as the design panel has, and the rules on their
// own.

#include "QtRocket/gui/main/SplitPane.h"

#include <memory>

#include <QList>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// A window holding nothing but a split pane of two empty sides.
class SplitPaneTest : public Test::GuiTest
{
protected:
    void TearDown() override
    {
        m_window = nullptr;
        GuiTest::TearDown();
    }

    /// Makes the window with a split pane of @p orientation, resize weight @p weight and divider
    /// location @p location, @p width by @p height pixels, not shown.
    SplitPane& make(Qt::Orientation orientation, double weight, double location, int width,
                    int height)
    {
        m_window     = std::make_unique<QWidget>();
        auto* layout = makeChild<QVBoxLayout>(m_window.get());
        layout->setContentsMargins(0, 0, 0, 0);
        m_pane = makeChild<SplitPane>(m_window.get(), orientation, weight, location);
        layout->addWidget(m_pane);
        m_first  = makeChild<QWidget>(m_pane);
        m_second = makeChild<QWidget>(m_pane);
        m_window->resize(width, height);
        return *m_pane;
    }

    void show() const { m_window->show(); }
    void resize(int width, int height) const { m_window->resize(width, height); }

    [[nodiscard]] QList<int> sizes() const { return m_pane->sizes(); }
    /// The length the sides share (the divider's width taken off).
    [[nodiscard]] int length() const { return m_pane->width() - m_pane->handleWidth(); }

    [[nodiscard]] QWidget& first() const { return *m_first; }
    [[nodiscard]] QWidget& second() const { return *m_second; }

private:
    std::unique_ptr<QWidget> m_window;
    SplitPane*               m_pane   = nullptr;
    QWidget*                 m_first  = nullptr;
    QWidget*                 m_second = nullptr;
};

TEST_F(SplitPaneTest, KeepsItsSettings)
{
    const SplitPane& pane = make(Qt::Horizontal, 0.5, 0.4, 1000, 400);
    EXPECT_EQ(pane.orientation(), Qt::Horizontal);
    EXPECT_DOUBLE_EQ(pane.resizeWeight(), 0.5);
    EXPECT_DOUBLE_EQ(pane.dividerLocation(), 0.4);
    EXPECT_FALSE(pane.childrenCollapsible());
    EXPECT_FALSE(pane.isDividerPlaced());
    EXPECT_EQ(pane.count(), 2);
}

TEST_F(SplitPaneTest, RefusesAWeightOrALocationOutsideZeroToOne)
{
    EXPECT_THROW(static_cast<void>(SplitPane(Qt::Horizontal, 1.5, 0.4, nullptr)), BugError);
    EXPECT_THROW(static_cast<void>(SplitPane(Qt::Horizontal, 0.5, -0.1, nullptr)), BugError);
}

TEST_F(SplitPaneTest, PlacesTheDividerWhenFirstShown)
{
    const SplitPane& pane = make(Qt::Horizontal, 0.5, 0.4, 1000, 400);
    show();
    EXPECT_TRUE(pane.isDividerPlaced());
    // JSplitPane.setDividerLocation(0.4): (int) ((1000 - divider) * 0.4).
    const QList<int> placed = sizes();
    ASSERT_EQ(placed.size(), 2);
    EXPECT_EQ(placed.at(0), static_cast<int>(length() * 0.4));
    EXPECT_EQ(placed.at(0) + placed.at(1), length());
}

TEST_F(SplitPaneTest, SharesAChangeOfItsWidthByTheResizeWeight)
{
    make(Qt::Horizontal, 0.5, 0.4, 1000, 400);
    show();
    const QList<int> before = sizes();
    resize(800, 400);
    const QList<int> after = sizes();
    ASSERT_EQ(after.size(), 2);
    EXPECT_EQ(after.at(0), before.at(0) - 100);
    EXPECT_EQ(after.at(1), before.at(1) - 100);

    // A weight of 0.25: a quarter of a change goes to the first side ((int) (0.25 * change)).
    make(Qt::Horizontal, 0.25, 0.5, 1000, 400);
    show();
    const QList<int> quarterBefore = sizes();
    resize(1200, 400);
    const QList<int> quarterAfter = sizes();
    ASSERT_EQ(quarterAfter.size(), 2);
    EXPECT_EQ(quarterAfter.at(0), quarterBefore.at(0) + 50);
    EXPECT_EQ(quarterAfter.at(1), quarterBefore.at(1) + 150);
}

TEST_F(SplitPaneTest, ASideKeepsItsMinimumAndTheOtherTakesTheRest)
{
    make(Qt::Horizontal, 0.5, 0.4, 1000, 400);
    show();
    const QList<int> before = sizes();
    ASSERT_EQ(before.size(), 2);
    first().setMinimumWidth(before.at(0) - 20);
    resize(800, 400);
    const QList<int> after = sizes();
    ASSERT_EQ(after.size(), 2);
    EXPECT_EQ(after.at(0), before.at(0) - 20);
    EXPECT_EQ(after.at(1), before.at(1) - 180);
}

TEST_F(SplitPaneTest, ASideThatIgnoresItsSizeHintHasNoMinimum)
{
    make(Qt::Horizontal, 0.5, 0.4, 1000, 400);
    // A size hint the side would otherwise keep as its minimum.
    auto* layout = makeChild<QVBoxLayout>(&second());
    layout->addWidget(makeChild<QWidget>(&second()));
    second().setMinimumSize(0, 0);
    second().setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    first().setMinimumWidth(500);
    show();
    resize(560, 400);
    const QList<int> after = sizes();
    ASSERT_EQ(after.size(), 2);
    EXPECT_EQ(after.at(0), 500);
    EXPECT_EQ(after.at(1), length() - 500);
}

TEST_F(SplitPaneTest, WorksVerticallyToo)
{
    const SplitPane& pane = make(Qt::Vertical, 0.5, 0.4, 400, 1000);
    show();
    const int        height = pane.height() - pane.handleWidth();
    const QList<int> placed = sizes();
    ASSERT_EQ(placed.size(), 2);
    EXPECT_EQ(placed.at(0), static_cast<int>(height * 0.4));
    EXPECT_EQ(placed.at(0) + placed.at(1), height);
    resize(400, 800);
    const QList<int> after = sizes();
    ASSERT_EQ(after.size(), 2);
    EXPECT_EQ(after.at(0), placed.at(0) - 100);
    EXPECT_EQ(after.at(1), placed.at(1) - 100);
}

}  // namespace

}  // namespace QtRocket::Gui
