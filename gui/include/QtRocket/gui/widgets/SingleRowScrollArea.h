#pragma once

#include <QObject>
#include <QScrollArea>
#include <QSize>

class QEvent;
class QWheelEvent;
class QWidget;

namespace QtRocket::Gui
{

/// A borderless scroll area for a row of controls of fixed height, such as the design view's
/// ribbon and footer (OpenRocket's SingleRowScrollPane): it scrolls the row horizontally when the
/// area is narrower than the row, and grows by the scroll bar's height while the scroll bar
/// shows, so that the row keeps its full height.
///
/// - The row gets its preferred size (sizeHint()), or the viewport's when that is larger, in
///   either direction (Swing's viewport and a view that is not Scrollable): the horizontal scroll
///   bar shows when the viewport is narrower than the row's preferred width. There is no
///   vertical scroll bar; an area lower than the row cuts its bottom off, as in Swing.
/// - sizeHint() is the row's preferred size, plus the horizontal scroll bar's height while it
///   shows and takes room (a style whose scroll bars are transient lays them over the row);
///   the height is fixed at it (QSizePolicy::Fixed), as RocketPanel's "shrink 0" rows are. Every
///   change of the scroll bar's visibility updates the geometry and emits
///   horizontalScrollBarVisibilityChanged() (Java's revalidate() and scrollbarVisibilityChanged).
/// - The row follows its own contents: when its preferred size changes, it is laid out again.
/// - The mouse wheel scrolls the row while the scroll bar shows, a vertical wheel included
///   (Swing scrolls the horizontal bar when there is no vertical one); otherwise the event goes
///   on to the parent.
///
/// Deviations from OpenRocket: the callback scrollbarVisibilityChanged is the signal
/// horizontalScrollBarVisibilityChanged(); Java's theme change (updateUI()) needs no handling,
/// since a Qt frame keeps its shape across style changes.
class SingleRowScrollArea final : public QScrollArea
{
    Q_OBJECT

public:
    /// The scroll area of @p row (SingleRowScrollPane(view, ...)), which it takes over
    /// (setWidget()). The row is not to be replaced (QScrollArea::setWidget() or takeWidget()):
    /// only this one is watched for changes of its preferred size.
    /// @throws BugError when @p row is null
    SingleRowScrollArea(QWidget* row, QWidget* parent);

    /// Whether the horizontal scroll bar is shown (it is laid out as visible, also while the area
    /// itself is hidden).
    [[nodiscard]] bool isHorizontalScrollBarShown() const;

    /// The height the horizontal scroll bar takes from the viewport: its preferred height while
    /// it is shown and not transient, else 0.
    [[nodiscard]] int horizontalScrollBarSpace() const;

    /// The row's preferred size plus horizontalScrollBarSpace() (getPreferredSize()).
    [[nodiscard]] QSize sizeHint() const override;
    /// QAbstractScrollArea's minimum width, at the preferred height.
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// The horizontal scroll bar was shown (@p shown) or hidden.
    void horizontalScrollBarVisibilityChanged(bool shown);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    /// The size the row asks for: its sizeHint(), within its minimum and maximum sizes.
    [[nodiscard]] QSize rowPreferredSize() const;

    /// Gives the row its size: its preferred size or the viewport's, the larger in each
    /// direction.
    void layoutRow();

    /// Updates the geometry and tells of it when the horizontal scroll bar's visibility changed.
    void checkScrollBarVisibility();

    /// The widget whose visibility is the horizontal scroll bar's: the bar's container in the
    /// area.
    QWidget* m_scrollBarHolder{nullptr};
    bool     m_scrollBarShown{false};
};

}  // namespace QtRocket::Gui
