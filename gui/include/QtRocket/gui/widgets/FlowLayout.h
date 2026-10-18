#pragma once

#include <memory>
#include <vector>

#include <QLayout>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QStyle>
#include <Qt>

class QLayoutItem;
class QWidget;

namespace QtRocket::Gui
{

/// A layout that places its items in rows, left to right, and starts a new row when the next
/// item would pass the right edge (Qt's flow layout example): the add buttons, which OpenRocket
/// wraps to the width of its panel (ComponentAddButtons.flowButtons()). Each item gets its size
/// hint; a row is as tall as its tallest item. The height follows the width
/// (hasHeightForWidth()).
///
/// The spacing between items is the one given, or the style's layout spacing of the parent
/// widget (or the parent layout's spacing) when it is -1.
class FlowLayout : public QLayout
{
    Q_OBJECT

public:
    explicit FlowLayout(QWidget* parent);
    /// With the contents margin @p margin on every side (-1: the style's) and the spacings
    /// between items in a row and between rows (-1: the style's).
    FlowLayout(int margin, int horizontalSpacing, int verticalSpacing, QWidget* parent);
    ~FlowLayout() override;

    FlowLayout(const FlowLayout&)            = delete;
    FlowLayout& operator=(const FlowLayout&) = delete;
    FlowLayout(FlowLayout&&)                 = delete;
    FlowLayout& operator=(FlowLayout&&)      = delete;

    /// The space between two items of a row, and between two rows.
    [[nodiscard]] int horizontalSpacing() const;
    [[nodiscard]] int verticalSpacing() const;

    /// Takes @p item over (QLayout::addItem()).
    void                       addItem(QLayoutItem* item) override;
    [[nodiscard]] int          count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    /// Hands the item over to the caller; null for an index out of range.
    [[nodiscard]] QLayoutItem* takeAt(int index) override;

    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool             hasHeightForWidth() const override;
    [[nodiscard]] int              heightForWidth(int width) const override;
    void                           setGeometry(const QRect& rect) override;
    [[nodiscard]] QSize            sizeHint() const override;
    [[nodiscard]] QSize            minimumSize() const override;

private:
    /// Places the items in @p rect (only computes when @p testOnly) and returns the height used.
    [[nodiscard]] int doLayout(const QRect& rect, bool testOnly) const;
    /// The style's spacing @p metric for the parent, or the parent layout's spacing.
    [[nodiscard]] int smartSpacing(QStyle::PixelMetric metric) const;

    std::vector<std::unique_ptr<QLayoutItem>> m_items;
    int                                       m_horizontalSpacing;
    int                                       m_verticalSpacing;
};

}  // namespace QtRocket::Gui
