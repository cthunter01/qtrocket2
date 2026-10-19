#include "QtRocket/gui/widgets/FlowLayout.h"

#include <memory>

#include <QLayoutItem>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStyle>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/MakeChild.h"

namespace QtRocket::Gui
{

namespace
{

using FlowLayoutTest = Test::GuiTest;

/// A panel with a flow layout (margin 0, spacing 10 in a row, 5 between rows) of three 50x20
/// widgets.
class ThreeButtons
{
public:
    ThreeButtons() : m_layout(makeChild<FlowLayout>(&m_panel, 0, 10, 5))
    {
        for (int index = 0; index < 3; ++index)
        {
            auto* button = makeChild<QWidget>(&m_panel);
            button->setFixedSize(50, 20);
            m_layout->addWidget(button);
        }
    }

    [[nodiscard]] FlowLayout& layout() const { return *m_layout; }
    [[nodiscard]] QRect       geometry(int index) const
    {
        return m_layout->itemAt(index)->widget()->geometry();
    }

private:
    QWidget     m_panel;
    FlowLayout* m_layout;
};

TEST_F(FlowLayoutTest, PutsTheItemsInOneRowWhenTheyFitExactly)
{
    const ThreeButtons buttons;
    // 50 + 10 + 50 + 10 + 50.
    buttons.layout().setGeometry(QRect(0, 0, 170, 100));
    EXPECT_EQ(buttons.geometry(0), QRect(0, 0, 50, 20));
    EXPECT_EQ(buttons.geometry(1), QRect(60, 0, 50, 20));
    EXPECT_EQ(buttons.geometry(2), QRect(120, 0, 50, 20));
    EXPECT_EQ(buttons.layout().heightForWidth(170), 20);
}

TEST_F(FlowLayoutTest, WrapsTheItemThatPassesTheRightEdge)
{
    const ThreeButtons buttons;
    buttons.layout().setGeometry(QRect(0, 0, 169, 100));
    EXPECT_EQ(buttons.geometry(1), QRect(60, 0, 50, 20));
    EXPECT_EQ(buttons.geometry(2), QRect(0, 25, 50, 20));
    EXPECT_EQ(buttons.layout().heightForWidth(169), 45);
    EXPECT_EQ(buttons.layout().heightForWidth(50), 70);
}

TEST_F(FlowLayoutTest, NeverLeavesARowEmpty)
{
    const ThreeButtons buttons;
    // Narrower than one item: each item has a row of its own.
    buttons.layout().setGeometry(QRect(10, 10, 30, 100));
    EXPECT_EQ(buttons.geometry(0), QRect(10, 10, 50, 20));
    EXPECT_EQ(buttons.geometry(1), QRect(10, 35, 50, 20));
    EXPECT_EQ(buttons.geometry(2), QRect(10, 60, 50, 20));
}

TEST_F(FlowLayoutTest, AnswersAsALayout)
{
    const ThreeButtons buttons;
    FlowLayout&        layout = buttons.layout();
    EXPECT_EQ(layout.count(), 3);
    EXPECT_TRUE(layout.hasHeightForWidth());
    EXPECT_EQ(layout.expandingDirections(), Qt::Orientations());
    EXPECT_EQ(layout.minimumSize(), QSize(50, 20));
    EXPECT_EQ(layout.sizeHint(), QSize(50, 20));
    EXPECT_EQ(layout.horizontalSpacing(), 10);
    EXPECT_EQ(layout.verticalSpacing(), 5);
    EXPECT_EQ(layout.itemAt(3), nullptr);
    EXPECT_EQ(layout.itemAt(-1), nullptr);
    EXPECT_EQ(layout.takeAt(3), nullptr);
}

TEST_F(FlowLayoutTest, HandsATakenItemOver)
{
    const ThreeButtons buttons;
    QLayoutItem*       item = buttons.layout().takeAt(0);
    ASSERT_NE(item, nullptr);
    EXPECT_EQ(buttons.layout().count(), 2);
    // The caller owns the item now (the widget stays the panel's).
    const std::unique_ptr<QLayoutItem> owned(item);
    buttons.layout().setGeometry(QRect(0, 0, 200, 100));
    EXPECT_EQ(buttons.geometry(0).topLeft(), QPoint(0, 0));
}

/// A panel with a flow layout of the style's margins and spacings.
class StyledPanel
{
public:
    StyledPanel() : m_layout(makeChild<FlowLayout>(&m_panel)) { }

    [[nodiscard]] const QWidget&    panel() const { return m_panel; }
    [[nodiscard]] const FlowLayout& layout() const { return *m_layout; }

private:
    QWidget     m_panel;
    FlowLayout* m_layout;
};

TEST_F(FlowLayoutTest, TakesTheStylesSpacingWhenNoneIsGiven)
{
    const StyledPanel styled;
    const QWidget&    panel = styled.panel();
    EXPECT_EQ(styled.layout().horizontalSpacing(),
              panel.style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing, nullptr, &panel));
    EXPECT_EQ(styled.layout().verticalSpacing(),
              panel.style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, &panel));
}

}  // namespace

}  // namespace QtRocket::Gui
