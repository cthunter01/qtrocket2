#include "QtRocket/gui/flightconfig/MotorCellDelegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QIcon>
#include <QMetaType>
#include <QModelIndex>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

namespace
{

/// The style that draws the view of @p option.
const QStyle& styleOf(const QStyleOptionViewItem& option)
{
    return option.widget != nullptr ? *option.widget->style() : *QApplication::style();
}

/// The palette's colour group for the state of @p option.
QPalette::ColorGroup colorGroupOf(const QStyleOptionViewItem& option)
{
    if (!option.state.testFlag(QStyle::State_Enabled))
    {
        return QPalette::Disabled;
    }
    return option.state.testFlag(QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
}

/// The style's margin left and right of an item's contents (QCommonStyle's text margin).
int marginOf(const QStyleOptionViewItem& option)
{
    return styleOf(option).pixelMetric(QStyle::PM_FocusFrameHMargin, &option, option.widget) + 1;
}

/// The labels of the motor cell @p index in @p option, which initStyleOption() has filled.
MotorCellDelegate::Cell layOut(const QStyleOptionViewItem& option, const QModelIndex& index)
{
    MotorCellDelegate::Cell cell;
    const bool              selected = option.state.testFlag(QStyle::State_Selected);
    const bool shaded = index.data(FlightConfigurableTableModel::kDefaultRole).toBool();
    const QPalette::ColorGroup group = colorGroupOf(option);
    const QColor               text =
        option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);

    cell.motorText    = index.data(Qt::DisplayRole).toString();
    cell.motorFont    = option.font;
    cell.motorColor   = text;
    cell.ignitionText = index.data(FlightConfigurableTableModel::kIgnitionTextRole).toString();
    cell.ignitionFont = option.font;
    cell.ignitionFont.setItalic(shaded);
    cell.ignitionColor =
        shaded && !selected ? ThemeColors::color(ThemeColors::Key::TEXT_DIM, option.palette) : text;

    const QRect& rect       = option.rect;
    const int    motorWidth = QFontMetrics(cell.motorFont).horizontalAdvance(cell.motorText);
    const int ignitionWidth = QFontMetrics(cell.ignitionFont).horizontalAdvance(cell.ignitionText);
    cell.motor    = QRect(rect.left() + marginOf(option), rect.top(), motorWidth, rect.height());
    cell.ignition = QRect(cell.motor.left() + motorWidth + MotorCellDelegate::kLabelGap, rect.top(),
                          ignitionWidth, rect.height());
    return cell;
}

}  // namespace

MotorCellDelegate::MotorCellDelegate(QObject* parent) : QStyledItemDelegate(parent) { }

MotorCellDelegate::~MotorCellDelegate() = default;

void MotorCellDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    if (painter == nullptr)
    {
        return;
    }
    if (!isMotorCell(index))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const Cell labels = layOut(opt, index);

    // The panel, the selection and the focus frame, as the style draws a cell without contents.
    QStyleOptionViewItem background = opt;
    background.text.clear();
    background.icon = QIcon();
    background.features.setFlag(QStyleOptionViewItem::HasDisplay, false);
    background.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
    styleOf(opt).drawControl(QStyle::CE_ItemViewItem, &background, painter, opt.widget);

    painter->save();
    painter->setClipRect(opt.rect);
    constexpr int kFlags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
    painter->setFont(labels.motorFont);
    painter->setPen(labels.motorColor);
    painter->drawText(labels.motor, kFlags, labels.motorText);
    painter->setFont(labels.ignitionFont);
    painter->setPen(labels.ignitionColor);
    painter->drawText(labels.ignition, kFlags, labels.ignitionText);
    painter->restore();
}

QSize MotorCellDelegate::sizeHint(const QStyleOptionViewItem& option,
                                  const QModelIndex&          index) const
{
    if (!isMotorCell(index))
    {
        return QStyledItemDelegate::sizeHint(option, index);
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The style's height of a row of text.
    QStyleOptionViewItem textOnly = opt;
    textOnly.icon                 = QIcon();
    textOnly.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
    const QSize textRow =
        styleOf(opt).sizeFromContents(QStyle::CT_ItemViewItem, &textOnly, QSize(), opt.widget);

    opt.rect          = QRect(0, 0, 0, textRow.height());
    const Cell labels = layOut(opt, index);
    return {labels.ignition.left() + labels.ignition.width() + marginOf(opt), textRow.height()};
}

bool MotorCellDelegate::isMotorCell(const QModelIndex& index)
{
    return index.data(FlightConfigurableTableModel::kIgnitionTextRole).typeId() ==
           QMetaType::QString;
}

MotorCellDelegate::Cell MotorCellDelegate::cell(const QStyleOptionViewItem& option,
                                                const QModelIndex&          index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    return layOut(opt, index);
}

}  // namespace QtRocket::Gui
