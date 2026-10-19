#include "QtRocket/gui/widgets/SingleRowScrollArea.h"

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QTest>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <Qt>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

namespace
{

using SingleRowScrollAreaTest = Test::GuiTest;

/// Delivers the posted events until the layouts are done: a scroll area shows or hides a scroll
/// bar one pass of the event loop after the bar's range changed (a queued connection), and the
/// layout around it makes room one pass after that.
void layOut()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

/// SingleRowScrollPaneTest.createFooter(): a label that takes the width left over, two buttons
/// and a check box, in a row without margins (MigLayout "fillx, gapy 0, ins 0"), made in
/// @p holder until a scroll area takes it.
[[nodiscard]] QWidget* makeFooter(QWidget& holder)
{
    auto* footer = makeChild<QWidget>(&holder);
    auto* layout = makeChild<QHBoxLayout>(footer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(
        makeChild<QLabel>(
            footer, QStringLiteral("Click to select; shift-click to add; double-click to edit")),
        1);
    layout->addWidget(makeChild<QPushButton>(footer, QStringLiteral("View settings")));
    layout->addWidget(makeChild<QPushButton>(footer, QStringLiteral("Capture")));
    layout->addWidget(makeChild<QCheckBox>(footer, QStringLiteral("Show warnings")));
    return footer;
}

/// The footer in a SingleRowScrollArea, a window of its own.
struct AreaWithFooter
{
    /// Resizes the area to @p size and shows it (offscreen), its layout done.
    void showAt(QSize size)
    {
        area.resize(size);
        area.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&area));
        layOut();
    }

    [[nodiscard]] QScrollBar& horizontal() const { return *area.horizontalScrollBar(); }
    [[nodiscard]] int         scrollBarHeight() const { return horizontal().sizeHint().height(); }

    QWidget             holder;  // the footer's parent until the area takes it over
    QWidget*            footer{makeFooter(holder)};
    QSize               footerSize{footer->sizeHint()};
    SingleRowScrollArea area{footer, nullptr};
};

/// The footer's area in a window with a column layout, as the design view holds its ribbon.
struct AreaInColumn
{
    AreaInColumn()
    {
        auto* layout = makeChild<QVBoxLayout>(&window);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(area);
        layout->addStretch(1);
    }

    /// Resizes the window to @p width and shows it, its layouts done.
    void showAt(int width)
    {
        window.resize(width, 200);
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        layOut();
    }

    QWidget              window;
    QWidget*             footer{makeFooter(window)};
    QSize                footerSize{footer->sizeHint()};
    SingleRowScrollArea* area{makeChild<SingleRowScrollArea>(&window, footer)};
};

/// A wheel event of @p angleDelta at the middle of @p widget, sent to it.
[[nodiscard]] bool sendWheel(QWidget& widget, QPoint angleDelta)
{
    const QPointF centre = QPointF(widget.rect().center());
    QWheelEvent   wheel(centre, widget.mapToGlobal(centre), QPoint(), angleDelta, Qt::NoButton,
                        Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &wheel);
    return wheel.isAccepted();
}

// Port of SingleRowScrollPaneTest.narrowViewportShowsHorizontalScrollbarWithoutClippingControls.
TEST_F(SingleRowScrollAreaTest, NarrowViewportShowsHorizontalScrollbarWithoutClippingControls)
{
    AreaWithFooter shown;
    shown.showAt(QSize(250, shown.footerSize.height() + shown.scrollBarHeight()));

    EXPECT_TRUE(shown.horizontal().isVisible());
    EXPECT_FALSE(shown.area.verticalScrollBar()->isVisible());
    EXPECT_GE(shown.area.viewport()->height(), shown.footerSize.height())
        << "The scrollbar must receive its own height instead of clipping the footer controls";
}

// Port of SingleRowScrollPaneTest.wideViewportDoesNotShowHorizontalScrollbar.
TEST_F(SingleRowScrollAreaTest, WideViewportDoesNotShowHorizontalScrollbar)
{
    AreaWithFooter shown;
    shown.showAt(QSize(shown.footerSize.width() + 20, shown.footerSize.height()));

    EXPECT_FALSE(shown.horizontal().isVisible());
}

TEST_F(SingleRowScrollAreaTest, IsABorderlessRowWithoutAVerticalScrollBar)
{
    const AreaWithFooter       shown;
    const SingleRowScrollArea& area = shown.area;
    EXPECT_EQ(area.widget(), shown.footer);
    EXPECT_EQ(area.frameShape(), QFrame::NoFrame);
    EXPECT_EQ(area.frameWidth(), 0);
    EXPECT_EQ(area.horizontalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
    EXPECT_EQ(area.verticalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_FALSE(area.widgetResizable());
    EXPECT_EQ(area.sizePolicy().horizontalPolicy(), QSizePolicy::Preferred);
    EXPECT_EQ(area.sizePolicy().verticalPolicy(), QSizePolicy::Fixed);
    // The height is the preferred one; the width can shrink far below the row's.
    EXPECT_EQ(area.minimumSizeHint().height(), area.sizeHint().height());
    EXPECT_LT(area.minimumSizeHint().width(), shown.footerSize.width());
}

TEST_F(SingleRowScrollAreaTest, GivesTheRowItsPreferredSizeOrTheViewports)
{
    AreaWithFooter shown;
    shown.showAt(QSize(250, shown.footerSize.height() + shown.scrollBarHeight()));
    // Narrower than the footer: the footer keeps its preferred width and height.
    EXPECT_EQ(shown.footer->size(), shown.footerSize);

    // Wider and higher: the footer fills the viewport.
    shown.area.resize(shown.footerSize.width() + 100, shown.footerSize.height() + 10);
    layOut();
    ASSERT_FALSE(shown.area.isHorizontalScrollBarShown());
    EXPECT_EQ(shown.area.viewport()->size(), shown.area.size());
    EXPECT_EQ(shown.footer->size(), shown.area.viewport()->size());
}

TEST_F(SingleRowScrollAreaTest, GrowsByTheScrollBarWhileItShows)
{
    AreaInColumn column;
    QSignalSpy visibility(column.area, &SingleRowScrollArea::horizontalScrollBarVisibilityChanged);
    column.showAt(column.footerSize.width() + 50);
    ASSERT_FALSE(column.area->isHorizontalScrollBarShown());
    EXPECT_EQ(column.area->height(), column.footerSize.height());
    // (The window's first layout may have passed through a narrow area.)
    visibility.clear();

    // Narrower than the footer: the scroll bar shows below the whole footer.
    column.window.resize(250, 200);
    layOut();
    ASSERT_TRUE(column.area->isHorizontalScrollBarShown());
    const int scrollBar = column.area->horizontalScrollBar()->sizeHint().height();
    EXPECT_EQ(column.area->horizontalScrollBarSpace(), scrollBar);
    EXPECT_EQ(column.area->sizeHint().height(), column.footerSize.height() + scrollBar);
    EXPECT_EQ(column.area->height(), column.footerSize.height() + scrollBar);
    EXPECT_EQ(column.area->viewport()->height(), column.footerSize.height());
    ASSERT_EQ(visibility.count(), 1);
    EXPECT_TRUE(visibility.at(0).at(0).toBool());

    // Wide again: back to the footer's height.
    column.window.resize(column.footerSize.width() + 50, 200);
    layOut();
    EXPECT_FALSE(column.area->isHorizontalScrollBarShown());
    EXPECT_EQ(column.area->horizontalScrollBarSpace(), 0);
    EXPECT_EQ(column.area->height(), column.footerSize.height());
    ASSERT_EQ(visibility.count(), 2);
    EXPECT_FALSE(visibility.at(1).at(0).toBool());
}

TEST_F(SingleRowScrollAreaTest, FollowsTheRowsContents)
{
    AreaInColumn column;
    column.showAt(column.footerSize.width() + 50);
    ASSERT_FALSE(column.area->isHorizontalScrollBarShown());

    // The row grows by a wide control: it is laid out again, now wider than the area.
    auto* wide =
        makeChild<QPushButton>(column.footer, QStringLiteral("A control that does not fit"));
    wide->setMinimumWidth(200);
    column.footer->layout()->addWidget(wide);
    layOut();
    EXPECT_GT(column.footer->width(), column.area->viewport()->width());
    EXPECT_EQ(column.footer->width(), column.footer->sizeHint().width());
    EXPECT_TRUE(column.area->isHorizontalScrollBarShown());
}

TEST_F(SingleRowScrollAreaTest, TheWheelScrollsTheRowSideways)
{
    AreaWithFooter shown;
    shown.showAt(QSize(250, shown.footerSize.height() + shown.scrollBarHeight()));
    ASSERT_TRUE(shown.area.isHorizontalScrollBarShown());
    ASSERT_EQ(shown.horizontal().value(), 0);

    // A vertical wheel turned down scrolls to the right, up back to the left.
    EXPECT_TRUE(sendWheel(*shown.area.viewport(), QPoint(0, -120)));
    EXPECT_GT(shown.horizontal().value(), 0);
    EXPECT_TRUE(sendWheel(*shown.area.viewport(), QPoint(0, 120)));
    EXPECT_EQ(shown.horizontal().value(), 0);
    EXPECT_EQ(shown.area.verticalScrollBar()->value(), 0);

    // Without the scroll bar the wheel is left to the parent.
    shown.area.resize(shown.footerSize.width() + 20, shown.footerSize.height());
    layOut();
    ASSERT_FALSE(shown.area.isHorizontalScrollBarShown());
    EXPECT_FALSE(sendWheel(*shown.area.viewport(), QPoint(0, -120)));
    EXPECT_EQ(shown.horizontal().value(), 0);
}

}  // namespace

}  // namespace QtRocket::Gui
