#include "QtRocket/gui/simulation/SimulationWarningsDelegate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include <QApplication>
#include <QChar>
#include <QFontMetrics>
#include <QIcon>
#include <QModelIndex>
#include <QObject>
#include <QPainter>
#include <QPalette>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableModel.h"
#include "QtRocket/gui/util/Icons.h"

namespace QtRocket::Gui
{

namespace
{

/// The three warning levels, critical first, with their icons (WarningsBox's order).
constexpr std::array kLevelIcons{Icons::Id::WARNING_HIGH, Icons::Id::WARNING_NORMAL,
                                 Icons::Id::WARNING_LOW};

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

/// A count of the model's role @p role at @p index, or nullopt when it has none.
std::optional<std::size_t> countOf(const QModelIndex& index, int role)
{
    const QVariant value = index.data(role);
    if (!value.isValid())
    {
        return std::nullopt;
    }
    return static_cast<std::size_t>(std::max(0, value.toInt()));
}

}  // namespace

SimulationWarningsDelegate::SimulationWarningsDelegate(QObject* parent)
  : QStyledItemDelegate(parent)
{
    for (const Icons::Id id : kLevelIcons)
    {
        m_icons.emplace(id, Icons::icon(id));
    }
}

SimulationWarningsDelegate::~SimulationWarningsDelegate() = default;

const QIcon& SimulationWarningsDelegate::icon(Icons::Id id) const
{
    return m_icons.at(id);
}

std::optional<SimulationColumns::WarningCounts> SimulationWarningsDelegate::counts(
    const QModelIndex& index)
{
    const std::optional<std::size_t> critical =
        countOf(index, SimulationTableModel::kCriticalWarningsRole);
    const std::optional<std::size_t> normal =
        countOf(index, SimulationTableModel::kNormalWarningsRole);
    const std::optional<std::size_t> informational =
        countOf(index, SimulationTableModel::kInformationalWarningsRole);
    if (!critical.has_value() || !normal.has_value() || !informational.has_value())
    {
        return std::nullopt;
    }
    return SimulationColumns::WarningCounts{
        .critical = *critical, .normal = *normal, .informational = *informational};
}

std::vector<SimulationWarningsDelegate::Part> SimulationWarningsDelegate::layOut(
    const SimulationColumns::WarningCounts& counts, const QRect& rect, const QFont& font)
{
    const QFontMetrics metrics(font);
    std::vector<Part>  parts;
    int                x = rect.left() + kLeftPadding;
    // A label of WarningsBox: centred vertically in the cell, as wide as its text or icon.
    const auto add = [&](QString text, std::optional<Icons::Id> icon) {
        QSize size;
        if (icon.has_value())
        {
            const int side = Icons::iconHeight(*icon, font);
            size           = QSize(side, side);
        }
        else
        {
            size = QSize(metrics.horizontalAdvance(text), metrics.height());
        }
        const QRect where(QPoint(x, rect.top() + ((rect.height() - size.height()) / 2)), size);
        parts.push_back(Part{.text = std::move(text), .icon = icon, .rect = where});
        x += size.width();
    };
    const std::array levels{counts.critical, counts.normal, counts.informational};
    bool             before = false;
    for (std::size_t level = 0; level < levels.size(); ++level)
    {
        if (levels.at(level) == 0)
        {
            continue;
        }
        if (before)
        {
            add(QStringLiteral(", "), std::nullopt);
        }
        add(QString::number(levels.at(level)) + QLatin1Char(' '), std::nullopt);
        add(QString(), kLevelIcons.at(level));
        before = true;
    }
    return parts;
}

SimulationWarningsDelegate::Cell SimulationWarningsDelegate::cell(
    const QStyleOptionViewItem& option, const QModelIndex& index)
{
    Cell       cell;
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    cell.font           = option.font;
    cell.textColor = option.palette.color(colorGroupOf(option),
                                          selected ? QPalette::HighlightedText : QPalette::Text);
    cell.width     = kLeftPadding;
    if (const std::optional<SimulationColumns::WarningCounts> found = counts(index);
        found.has_value())
    {
        cell.parts = layOut(*found, option.rect, cell.font);
    }
    for (const Part& part : cell.parts)
    {
        cell.width += part.rect.width();
    }
    return cell;
}

void SimulationWarningsDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                       const QModelIndex& index) const
{
    if (painter == nullptr)
    {
        return;
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The background, the selection and the focus frame, as the style draws a cell without
    // contents.
    QStyleOptionViewItem background = opt;
    background.text.clear();
    background.icon = QIcon();
    background.features.setFlag(QStyleOptionViewItem::HasDisplay, false);
    background.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
    styleOf(opt).drawControl(QStyle::CE_ItemViewItem, &background, painter, opt.widget);

    const Cell parts = cell(opt, index);
    if (parts.parts.empty())
    {
        return;
    }
    painter->save();
    painter->setClipRect(opt.rect);
    painter->setFont(parts.font);
    painter->setPen(parts.textColor);
    const QIcon::Mode mode =
        opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
    for (const Part& part : parts.parts)
    {
        if (part.icon.has_value())
        {
            icon(*part.icon).paint(painter, part.rect, Qt::AlignCenter, mode, QIcon::Off);
        }
        else
        {
            painter->drawText(part.rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              part.text);
        }
    }
    painter->restore();
}

QSize SimulationWarningsDelegate::sizeHint(const QStyleOptionViewItem& option,
                                           const QModelIndex&          index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    // The style's height of a row of text.
    QStyleOptionViewItem textOnly = opt;
    textOnly.icon                 = QIcon();
    textOnly.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
    textOnly.text = QStringLiteral("0");
    textOnly.features.setFlag(QStyleOptionViewItem::HasDisplay, true);
    const QSize textRow =
        styleOf(opt).sizeFromContents(QStyle::CT_ItemViewItem, &textOnly, QSize(), opt.widget);

    opt.rect          = QRect(QPoint(0, 0), QSize(0, textRow.height()));
    const Cell parts  = cell(opt, index);
    int        height = textRow.height();
    for (const Part& part : parts.parts)
    {
        height = std::max(height, part.rect.height());
    }
    return {parts.width, height};
}

}  // namespace QtRocket::Gui
