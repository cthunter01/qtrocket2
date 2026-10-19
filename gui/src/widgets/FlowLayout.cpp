#include "QtRocket/gui/widgets/FlowLayout.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>

#include <QLayout>
#include <QLayoutItem>
#include <QMargins>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStyle>
#include <QWidget>
#include <Qt>

namespace QtRocket::Gui
{

FlowLayout::FlowLayout(QWidget* parent) : FlowLayout(-1, -1, -1, parent) { }

FlowLayout::FlowLayout(int margin, int horizontalSpacing, int verticalSpacing, QWidget* parent)
  : QLayout(parent), m_horizontalSpacing(horizontalSpacing), m_verticalSpacing(verticalSpacing)
{
    if (margin >= 0)
    {
        setContentsMargins(margin, margin, margin, margin);
    }
}

FlowLayout::~FlowLayout() = default;

int FlowLayout::horizontalSpacing() const
{
    if (m_horizontalSpacing >= 0)
    {
        return m_horizontalSpacing;
    }
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const
{
    if (m_verticalSpacing >= 0)
    {
        return m_verticalSpacing;
    }
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.emplace_back(item);
}

int FlowLayout::count() const
{
    return static_cast<int>(m_items.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_items.size())
    {
        return nullptr;
    }
    return m_items.at(static_cast<std::size_t>(index)).get();
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= m_items.size())
    {
        return nullptr;
    }
    const auto   position = std::next(m_items.begin(), index);
    QLayoutItem* item     = position->release();
    m_items.erase(position);
    return item;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    static_cast<void>(doLayout(rect, false));
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const std::unique_ptr<QLayoutItem>& item : m_items)
    {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const
{
    const QMargins margins = contentsMargins();
    const QRect    area =
        rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    // The first x past the area: an item fits in a row when it ends at or before it.
    const int end        = area.x() + area.width();
    int       x          = area.x();
    int       y          = area.y();
    int       lineHeight = 0;
    for (const std::unique_ptr<QLayoutItem>& item : m_items)
    {
        if (item->isEmpty())
        {
            continue;
        }
        const QSize hint = item->sizeHint();
        if (x + hint.width() > end && lineHeight > 0)
        {
            x          = area.x();
            y          = y + lineHeight + verticalSpacing();
            lineHeight = 0;
        }
        if (!testOnly)
        {
            item->setGeometry(QRect(QPoint(x, y), hint));
        }
        x          = x + hint.width() + horizontalSpacing();
        lineHeight = std::max(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + margins.bottom();
}

int FlowLayout::smartSpacing(QStyle::PixelMetric metric) const
{
    QObject* owner = parent();
    if (auto* widget = qobject_cast<QWidget*>(owner))
    {
        return widget->style()->pixelMetric(metric, nullptr, widget);
    }
    if (const auto* layout = qobject_cast<QLayout*>(owner))
    {
        return layout->spacing();
    }
    return -1;
}

}  // namespace QtRocket::Gui
