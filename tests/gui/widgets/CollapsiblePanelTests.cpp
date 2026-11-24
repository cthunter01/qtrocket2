// A section with a full-width header that shows or hides its content (OpenRocket's
// CollapsiblePanel: the recovery warnings' two sections).

#include "QtRocket/gui/widgets/CollapsiblePanel.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include <QApplication>
#include <QColor>
#include <QFont>
#include <QPalette>
#include <QPoint>
#include <QRegion>
#include <QSizePolicy>
#include <QSpinBox>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/BugError.h"
#include "figure/RecordingPaintDevice.h"

namespace QtRocket::Gui
{

namespace
{

using CollapsiblePanelTest = Test::GuiTest;
using Test::RecordedPaint;
using Test::RecordingPaintDevice;

/// A content of a threshold spin box (RecoveryWarningsPanel's sections), a child of @p parent
/// until a panel takes it over.
[[nodiscard]] QWidget* thresholdContent(QWidget& parent)
{
    auto* content = makeChild<QWidget>(&parent);
    auto* layout  = makeChild<QVBoxLayout>(content);
    layout->addWidget(makeChild<QSpinBox>(content));
    return content;
}

/// The records of @p kind among @p records.
[[nodiscard]] std::size_t countOf(const std::vector<RecordedPaint>& records,
                                  RecordedPaint::Kind               kind)
{
    return static_cast<std::size_t>(std::ranges::count(records, kind, &RecordedPaint::kind));
}

/// The records of @p records drawn with a pen of @p color.
[[nodiscard]] std::size_t countOf(const std::vector<RecordedPaint>& records, const QColor& color)
{
    return static_cast<std::size_t>(std::ranges::count(
        records, color, [](const RecordedPaint& record) { return record.pen.color(); }));
}

/// What @p panel paints itself (without its children and background).
[[nodiscard]] std::vector<RecordedPaint> paintedFrame(CollapsiblePanel& panel)
{
    RecordingPaintDevice device(panel.size());
    panel.render(&device, QPoint(), QRegion(), {});
    return device.records();
}

}  // namespace

TEST_F(CollapsiblePanelTest, StartsExpandedOrCollapsedAsGiven)
{
    QWidget          holder;
    QWidget*         content = thresholdContent(holder);
    CollapsiblePanel single(QStringLiteral("Single Deployment Warnings"), content, true, nullptr);
    CollapsiblePanel dual(QStringLiteral("Dual Deployment Warnings"), thresholdContent(holder),
                          false, nullptr);
    EXPECT_TRUE(single.isExpanded());
    EXPECT_EQ(single.content(), content);
    EXPECT_TRUE(single.isAncestorOf(content));
    EXPECT_TRUE(content->isVisibleTo(&single));
    EXPECT_TRUE(single.headerButton()->isChecked());
    EXPECT_EQ(single.headerButton()->arrowType(), Qt::DownArrow);

    EXPECT_FALSE(dual.isExpanded());
    EXPECT_FALSE(dual.content()->isVisibleTo(&dual));
    EXPECT_FALSE(dual.headerButton()->isChecked());
    EXPECT_EQ(dual.headerButton()->arrowType(), Qt::RightArrow);
    // Collapsed, the content's spin box is not shown (Java: not in the component hierarchy).
    EXPECT_FALSE(dual.findChild<QSpinBox*>()->isVisibleTo(&dual));
}

TEST_F(CollapsiblePanelTest, TheHeaderIsAFullWidthToolBarToggle)
{
    QWidget                holder;
    const CollapsiblePanel panel(QStringLiteral("Dual Deployment Warnings"),
                                 thresholdContent(holder), false, nullptr);
    const QToolButton*     header = panel.headerButton();
    EXPECT_EQ(header->objectName(), QStringLiteral("headerButton"));
    EXPECT_EQ(header->text(), QStringLiteral("Dual Deployment Warnings"));
    EXPECT_EQ(header->accessibleName(), QStringLiteral("Dual Deployment Warnings"));
    EXPECT_TRUE(header->isCheckable());
    EXPECT_TRUE(header->autoRaise());
    EXPECT_EQ(header->toolButtonStyle(), Qt::ToolButtonTextBesideIcon);
    EXPECT_EQ(header->sizePolicy().horizontalPolicy(), QSizePolicy::Expanding);

    // The label font, bold and a point smaller (at least 9 points).
    const QFont label = QApplication::font("QLabel");
    ASSERT_GT(label.pointSizeF(), 0.0);
    EXPECT_TRUE(header->font().bold());
    EXPECT_DOUBLE_EQ(header->font().pointSizeF(), std::max(9.0, label.pointSizeF() - 1.0));
}

TEST_F(CollapsiblePanelTest, TheHeaderShowsAndHidesTheContent)
{
    QWidget          holder;
    CollapsiblePanel panel(QStringLiteral("Dual Deployment Warnings"), thresholdContent(holder),
                           false, nullptr);
    panel.headerButton()->click();
    EXPECT_TRUE(panel.isExpanded());
    EXPECT_TRUE(panel.content()->isVisibleTo(&panel));
    EXPECT_EQ(panel.headerButton()->arrowType(), Qt::DownArrow);
    panel.headerButton()->click();
    EXPECT_FALSE(panel.isExpanded());
    EXPECT_FALSE(panel.content()->isVisibleTo(&panel));
    EXPECT_FALSE(panel.headerButton()->isChecked());

    panel.setExpanded(true);
    EXPECT_TRUE(panel.headerButton()->isChecked());
    EXPECT_TRUE(panel.content()->isVisibleTo(&panel));
}

TEST_F(CollapsiblePanelTest, CollapsingGivesTheContentsSpaceBack)
{
    QWidget          holder;
    CollapsiblePanel panel(QStringLiteral("Single Deployment Warnings"), thresholdContent(holder),
                           true, nullptr);
    const int        expanded = panel.sizeHint().height();
    panel.setExpanded(false);
    const int collapsed = panel.sizeHint().height();
    EXPECT_LT(collapsed, expanded);
    // The header inside the 1 pixel frame.
    EXPECT_EQ(collapsed, panel.headerButton()->sizeHint().height() + 2);
    panel.setExpanded(true);
    EXPECT_EQ(panel.sizeHint().height(), expanded);
}

TEST_F(CollapsiblePanelTest, ItDrawsAFrameAndALineAboveTheContent)
{
    QWidget          holder;
    CollapsiblePanel panel(QStringLiteral("Single Deployment Warnings"), thresholdContent(holder),
                           true, nullptr);
    panel.resize(panel.sizeHint());
    const std::vector<RecordedPaint> expanded = paintedFrame(panel);
    EXPECT_EQ(countOf(expanded, RecordedPaint::Kind::RECT), 1U);
    EXPECT_EQ(countOf(expanded, RecordedPaint::Kind::LINE), 1U);
    EXPECT_EQ(countOf(expanded, panel.palette().color(QPalette::Mid)), expanded.size());

    panel.setExpanded(false);
    const std::vector<RecordedPaint> collapsed = paintedFrame(panel);
    EXPECT_EQ(countOf(collapsed, RecordedPaint::Kind::RECT), 1U);
    EXPECT_EQ(countOf(collapsed, RecordedPaint::Kind::LINE), 0U);
}

TEST_F(CollapsiblePanelTest, ItNeedsAContent)
{
    EXPECT_THROW(
        static_cast<void>(CollapsiblePanel(QStringLiteral("Empty"), nullptr, true, nullptr)),
        BugError);
}

}  // namespace QtRocket::Gui
