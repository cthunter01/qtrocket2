#include "QtRocket/gui/tree/ComponentTreeDelegate.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QIcon>
#include <QItemSelectionModel>
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
#include <QToolTip>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/ThemeColors.h"

namespace QtRocket::Gui
{

namespace
{

/// The largest icon the tree draws: its icons are drawn at their natural size (the PNG's), as
/// OpenRocket draws them, never scaled up to the view's icon size.
constexpr QSize kLargestIcon(256, 256);

/// The markers' icons.
constexpr std::array kMarkers{
    Icons::Id::MASS_OVERRIDE,    Icons::Id::MASS_OVERRIDE_SUBCOMPONENT,
    Icons::Id::CG_OVERRIDE,      Icons::Id::CG_OVERRIDE_SUBCOMPONENT,
    Icons::Id::CD_OVERRIDE,      Icons::Id::CD_OVERRIDE_SUBCOMPONENT,
    Icons::Id::COMPONENT_HIDDEN, Icons::Id::COMPONENT_DISABLED,
};

/// Whether @p index is a row of a component tree.
bool isComponentTree(const QModelIndex& index)
{
    return qobject_cast<const ComponentTreeModel*>(index.model()) != nullptr;
}

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

/// The parts of the row of @p option, which initStyleOption() has filled, at @p index; the name
/// elided to the room the row has when @p elide.
ComponentTreeDelegate::Cell layOut(const QStyleOptionViewItem& option, const QModelIndex& index,
                                   bool elide)
{
    ComponentTreeDelegate::Cell cell;
    const bool                  selected = option.state.testFlag(QStyle::State_Selected);
    const bool                  inactive = index.data(ComponentTreeModel::kInactiveRole).toBool();
    const bool                  hidden   = index.data(ComponentTreeModel::kHiddenRole).toBool();

    cell.font = option.font;
    if (inactive && !selected)
    {
        cell.font.setItalic(true);
    }
    const QPalette::ColorGroup group = colorGroupOf(option);
    cell.textColor =
        option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
    if (!selected && (hidden || inactive))
    {
        cell.textColor =
            ThemeColors::color(ThemeColors::Key::VISIBILITY_HIDDEN_FOREGROUND, option.palette);
    }

    const QRect& rect   = option.rect;
    const int    margin = marginOf(option);
    const QSize  iconSize =
        option.icon.isNull() ? QSize(0, 0) : option.icon.actualSize(kLargestIcon);
    int x     = rect.left() + margin;
    cell.icon = QRect(QPoint(x, rect.top() + ((rect.height() - iconSize.height()) / 2)), iconSize);
    x += iconSize.width() + ComponentTreeDelegate::kPartGap;

    std::vector<std::pair<Icons::Id, int>> markers;
    int                                    markersWidth = 0;
    for (const Icons::Id marker : ComponentTreeModel::markersOf(index))
    {
        const int side = Icons::iconHeight(marker, cell.font);
        markersWidth += (markers.empty() ? 0 : ComponentTreeDelegate::kMarkerSpacing) + side;
        markers.emplace_back(marker, side);
    }

    const QFontMetrics metrics(cell.font);
    int                textWidth = metrics.horizontalAdvance(option.text);
    cell.displayText             = option.text;
    if (elide)
    {
        const int room =
            std::max(0, rect.right() + 1 - margin - x -
                            (markers.empty() ? 0 : ComponentTreeDelegate::kPartGap + markersWidth));
        if (textWidth > room)
        {
            cell.displayText = metrics.elidedText(option.text, option.textElideMode, room);
            textWidth        = room;
        }
    }
    cell.text = QRect(x, rect.top(), textWidth, rect.height());

    x = cell.text.right() + 1 + ComponentTreeDelegate::kPartGap;
    for (const auto& [marker, side] : markers)
    {
        cell.markers.emplace_back(marker,
                                  QRect(x, rect.top() + ((rect.height() - side) / 2), side, side));
        x += side + ComponentTreeDelegate::kMarkerSpacing;
    }
    return cell;
}

}  // namespace

ComponentTreeDelegate::ComponentTreeDelegate(QObject* parent) : QStyledItemDelegate(parent)
{
    for (const Icons::Id marker : kMarkers)
    {
        m_markerIcons.emplace(marker, Icons::icon(marker));
    }
}

ComponentTreeDelegate::~ComponentTreeDelegate() = default;

void ComponentTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                  const QModelIndex& index) const
{
    if (painter == nullptr)
    {
        return;
    }
    if (!isComponentTree(index))
    {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const Cell parts = layOut(opt, index, true);

    // The panel, the selection and the focus frame, as the style draws a row without contents.
    QStyleOptionViewItem background = opt;
    background.text.clear();
    background.icon = QIcon();
    background.features.setFlag(QStyleOptionViewItem::HasDisplay, false);
    background.features.setFlag(QStyleOptionViewItem::HasDecoration, false);
    styleOf(opt).drawControl(QStyle::CE_ItemViewItem, &background, painter, opt.widget);

    painter->save();
    const QIcon::Mode mode =
        opt.state.testFlag(QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
    if (!opt.icon.isNull())
    {
        opt.icon.paint(painter, parts.icon, Qt::AlignCenter, mode, QIcon::Off);
    }
    painter->setFont(parts.font);
    painter->setPen(parts.textColor);
    painter->drawText(parts.text, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                      parts.displayText);
    for (const auto& [marker, rect] : parts.markers)
    {
        markerIcon(marker).paint(painter, rect, Qt::AlignCenter, mode, QIcon::Off);
    }
    painter->restore();
}

QSize ComponentTreeDelegate::sizeHint(const QStyleOptionViewItem& option,
                                      const QModelIndex&          index) const
{
    if (!isComponentTree(index))
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

    opt.rect          = QRect(QPoint(0, 0), QSize(0, textRow.height()));
    const Cell parts  = layOut(opt, index, false);
    int        right  = parts.text.right();
    int        height = std::max(textRow.height(), parts.icon.height());
    if (!parts.markers.empty())
    {
        right = parts.markers.back().second.right();
    }
    for (const auto& [marker, rect] : parts.markers)
    {
        height = std::max(height, rect.height());
    }
    return {right + 1 + marginOf(opt), height};
}

bool ComponentTreeDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
                                      const QStyleOptionViewItem& option, const QModelIndex& index)
{
    if (event == nullptr || view == nullptr || event->type() != QEvent::ToolTip ||
        !isComponentTree(index))
    {
        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }
    const QString text = toolTip(*view, index);
    QToolTip::showText(event->globalPos(), text, view->viewport(), option.rect);
    event->setAccepted(!text.isEmpty());
    return true;
}

ComponentTreeDelegate::Cell ComponentTreeDelegate::cell(const QStyleOptionViewItem& option,
                                                        const QModelIndex&          index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    return layOut(opt, index, true);
}

QString ComponentTreeDelegate::toolTip(const QAbstractItemView& view, const QModelIndex& index)
{
    const auto* model = qobject_cast<const ComponentTreeModel*>(index.model());
    if (model == nullptr)
    {
        return index.data(Qt::ToolTipRole).toString();
    }
    const QItemSelectionModel* selection = view.selectionModel();
    return model->toolTip(index,
                          selection != nullptr ? selection->selectedIndexes() : QModelIndexList());
}

const QIcon& ComponentTreeDelegate::markerIcon(Icons::Id marker) const
{
    const auto it = m_markerIcons.find(marker);
    return it != m_markerIcons.end() ? it->second : m_noIcon;
}

}  // namespace QtRocket::Gui
