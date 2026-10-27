#include "QtRocket/gui/flightconfig/MotorCellDelegate.h"

#include <memory>

#include <QColor>
#include <QFontMetrics>
#include <QImage>
#include <QModelIndex>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTableView>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "ScopedApplicationPalette.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using Cell = MotorCellDelegate::Cell;
using Kind = FlightConfigurableTableModel::Kind;
using QtRocket::Test::testFcid;

/// The Motors tab's table over OpenRocket's makeBeta (two motor mounts, the five test
/// configurations), in a table view that is never shown, with the delegate.
class MotorCellDelegateTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_model = std::make_unique<FlightConfigurableTableModel>(Kind::MOTOR_MOUNTS, *m_beta.rocket,
                                                                 m_preferences);
        m_view  = std::make_unique<QTableView>();
        m_view->setModel(m_model.get());
        m_delegate = makeChild<MotorCellDelegate>(m_view.get());
        m_view->setItemDelegate(m_delegate);
    }

    void TearDown() override
    {
        m_view  = nullptr;
        m_model = nullptr;
        Gui::Test::GuiTest::TearDown();
    }

    /// The option of a cell of the view in @p rect, selected or not.
    [[nodiscard]] QStyleOptionViewItem option(const QRect& rect, bool selected = false) const
    {
        QStyleOptionViewItem option;
        option.initFrom(m_view.get());
        option.widget = m_view.get();
        option.rect   = rect;
        option.state |= QStyle::State_Enabled;
        option.state.setFlag(QStyle::State_Selected, selected);
        return option;
    }

    [[nodiscard]] Cell cell(int row, int column, const QRect& rect, bool selected = false) const
    {
        return m_delegate->cell(option(rect, selected), m_model->index(row, column));
    }

    /// The style's margin left of a cell's contents.
    [[nodiscard]] int margin() const
    {
        const QStyleOptionViewItem o = option(QRect(0, 0, 400, 24));
        return m_view->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, &o, m_view.get()) + 1;
    }

    [[nodiscard]] QColor paletteColor(QPalette::ColorRole role) const
    {
        return option(QRect(0, 0, 400, 24)).palette.color(QPalette::Inactive, role);
    }

    QtRocket::Test::TestBeta                      m_beta;
    InMemoryPreferences                           m_preferences;
    std::unique_ptr<FlightConfigurableTableModel> m_model;
    std::unique_ptr<QTableView>                   m_view;
    MotorCellDelegate*                            m_delegate = nullptr;
};

TEST_F(MotorCellDelegateTest, PutsTheIgnitionTenPixelsAfterTheMotor)
{
    const QRect rect(10, 20, 400, 24);
    const Cell  labels = cell(1, 2, rect);

    EXPECT_EQ(labels.motorText, QStringLiteral("D21-0"));
    EXPECT_EQ(labels.ignitionText, QStringLiteral("Default (Automatic)"));
    EXPECT_EQ(labels.motor.left(), rect.left() + margin());
    EXPECT_EQ(labels.motor.width(),
              QFontMetrics(labels.motorFont).horizontalAdvance(QStringLiteral("D21-0")));
    EXPECT_EQ(labels.ignition.left(), labels.motor.right() + 1 + MotorCellDelegate::kLabelGap);
    EXPECT_EQ(MotorCellDelegate::kLabelGap, 10);
    EXPECT_EQ(
        labels.ignition.width(),
        QFontMetrics(labels.ignitionFont).horizontalAdvance(QStringLiteral("Default (Automatic)")));
    // Both as high as the cell (the text centred in it).
    EXPECT_EQ(labels.motor.top(), rect.top());
    EXPECT_EQ(labels.motor.height(), rect.height());
    EXPECT_EQ(labels.ignition.top(), rect.top());
    EXPECT_EQ(labels.ignition.height(), rect.height());
}

TEST_F(MotorCellDelegateTest, ShadesTheDefaultIgnitionOnly)
{
    const QRect rect(0, 0, 400, 24);
    const Cell  labels = cell(1, 2, rect);

    EXPECT_FALSE(labels.motorFont.italic());
    EXPECT_EQ(labels.motorColor, paletteColor(QPalette::Text));
    EXPECT_TRUE(labels.ignitionFont.italic());
    EXPECT_EQ(labels.ignitionColor,
              ThemeColors::color(ThemeColors::Key::TEXT_DIM, option(rect).palette));
}

TEST_F(MotorCellDelegateTest, TakesTheDimColourOfTheCellsPalette)
{
    QStyleOptionViewItem dark = option(QRect(0, 0, 400, 24));
    dark.palette              = Gui::Test::ScopedApplicationPalette::dark();
    const Cell labels         = m_delegate->cell(dark, m_model->index(1, 2));
    EXPECT_EQ(labels.ignitionColor,
              ThemeColors::themeColor(ThemeColors::Key::TEXT_DIM, ThemeColors::Theme::DARK));
    EXPECT_EQ(labels.motorColor, dark.palette.color(QPalette::Inactive, QPalette::Text));
}

TEST_F(MotorCellDelegateTest, WritesAnOverriddenIgnitionRegular)
{
    m_beta.inner->getMotorConfig(testFcid(0)).setIgnitionEvent(IgnitionEvent::LAUNCH);
    const Cell labels = cell(0, 1, QRect(0, 0, 400, 24));

    EXPECT_EQ(labels.ignitionText, QStringLiteral("Launch"));
    EXPECT_FALSE(labels.ignitionFont.italic());
    EXPECT_EQ(labels.ignitionColor, paletteColor(QPalette::Text));
}

TEST_F(MotorCellDelegateTest, WritesASelectedCellInTheHighlightedTextKeepingTheItalics)
{
    const Cell labels = cell(1, 2, QRect(0, 0, 400, 24), true);

    EXPECT_EQ(labels.motorColor, paletteColor(QPalette::HighlightedText));
    EXPECT_EQ(labels.ignitionColor, paletteColor(QPalette::HighlightedText));
    EXPECT_TRUE(labels.ignitionFont.italic());
}

TEST_F(MotorCellDelegateTest, KeepsTheLabelsWidthsInANarrowCell)
{
    // Java's labels never narrow (a JLabel's minimum width is its preferred width): the cell
    // cuts them off.
    const Cell wide   = cell(1, 2, QRect(0, 0, 400, 24));
    const Cell narrow = cell(1, 2, QRect(0, 0, 30, 24));
    EXPECT_EQ(narrow.motor, wide.motor);
    EXPECT_EQ(narrow.ignition, wide.ignition);
    EXPECT_EQ(narrow.ignitionText, QStringLiteral("Default (Automatic)"));
}

TEST_F(MotorCellDelegateTest, AsksForRoomForBothLabels)
{
    const QModelIndex index  = m_model->index(1, 2);
    const QSize       size   = m_delegate->sizeHint(option(QRect()), index);
    const Cell        labels = m_delegate->cell(option(QRect(QPoint(0, 0), size)), index);
    EXPECT_EQ(size.width(), labels.ignition.right() + 1 + margin());
    EXPECT_GE(size.height(), QFontMetrics(labels.ignitionFont).height());
}

TEST_F(MotorCellDelegateTest, DrawsTheOtherCellsAsAStyledItemDelegate)
{
    const QModelIndex configuration = m_model->index(1, 0);
    EXPECT_FALSE(MotorCellDelegate::isMotorCell(configuration));
    EXPECT_TRUE(MotorCellDelegate::isMotorCell(m_model->index(1, 1)));
    EXPECT_FALSE(MotorCellDelegate::isMotorCell(QModelIndex()));

    const QStyledItemDelegate  plain;
    const QStyleOptionViewItem o = option(QRect());
    EXPECT_EQ(m_delegate->sizeHint(o, configuration), plain.sizeHint(o, configuration));
}

TEST_F(MotorCellDelegateTest, PaintsEveryKindOfCell)
{
    // No pixel is compared: the painting must only run (and, under the asan preset, read nothing
    // it should not).
    QImage image(400, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);
    QPainter painter(&image);
    m_delegate->paint(&painter, option(QRect(0, 0, 400, 24)), m_model->index(1, 2));
    m_delegate->paint(&painter, option(QRect(0, 24, 400, 24), true), m_model->index(0, 1));
    m_delegate->paint(&painter, option(QRect(0, 0, 400, 24)), m_model->index(1, 0));
    m_delegate->paint(nullptr, option(QRect(0, 0, 400, 24)), m_model->index(1, 2));
    painter.end();
    SUCCEED();
}

}  // namespace

}  // namespace QtRocket::Gui
