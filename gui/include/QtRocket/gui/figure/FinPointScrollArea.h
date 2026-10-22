#pragma once

#include <memory>

#include <QObject>
#include <QPoint>
#include <Qt>

#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/Signal.h"

class QEvent;
class QFocusEvent;
class QHideEvent;
class QMouseEvent;
class QWidget;

namespace QtRocket
{
class Preferences;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;
class FinPointFigure;

/// The scroll area of the freeform fin editor's figure, which handles its mouse (OpenRocket's
/// FreeformFinSetConfig.FinPointScrollPane, an inner class of swing
/// gui/configdialog/FreeformFinSetConfig.java). The figure leaves its mouse events to the area
/// (ScaleScrollArea's viewport coordinates); every pixel below is the figure's (the event's plus
/// the view's position).
///
/// - A press on a point (FinPointFigure::indexAt()), of any button, begins an edit of the history,
///   makes the point the one dragged (dragIndex()) and, unless the preference "Live update the
///   rocket" (GuiPreferenceKeys::kUpdateRocketWhileDraggingPoint) is on, freezes the rocket, so
///   that the drag fires one event when it ends; then refreshRequested().
/// - A press on a segment (FinPointFigure::segmentAt()) does the same after inserting a point
///   there (FinPointSelection::addPoint() at the pressed point of the fin frame), which is then
///   dragged.
/// - A press elsewhere deselects (pointSelectionCleared(), the figure's selection reset and the
///   figure brought up to date) and starts ScaleScrollArea's pan, with any button.
/// - A drag moves the dragged point to the point of the fin frame under the mouse
///   (FinPointFigure::toFinPoint(), limited to +-2.5 m), snapped with Shift to the horizontal,
///   the vertical or the 45 degree diagonal through the previous point, with Ctrl and Shift the
///   next one, whichever is nearest (snapPoint(); only for a point between the first and the
///   last), that segment highlighted; then FinPointSelection::setPoint() (whose refusals are
///   silent: the core keeps the point and fires nothing), a repaint, refreshRequested(), and a
///   scroll of the view when the mouse is within the figure's border of the view's edge (fitting
///   ends then: autoScrolled()).
/// - A release ends the drag: thaws the rocket if the press froze it, clears the drag and the
///   highlight, brings the figure up to date and commits the edit (also after a pan: then there is
///   none).
/// - A click (a release where the press was, nothing moved between them): with Ctrl and the left
///   button on a point after the first, removes it (FinPointSelection::removePoint(), a refusal
///   ignored), its own edit; with the right button on a point, pointMenuRequested().
/// - The area takes the focus on a press (Qt::ClickFocus; Java: requestFocusInWindow()), and
///   focusEntered() tells the editor, which ends the table's edit (Java's focus listener).
///
/// The corner between the rulers holds a UnitSelector for their unit (and the figure's grid), as
/// Java's ScaleScrollPane has one.
///
/// Deviations from OpenRocket:
/// - A drag ends also when the area is hidden (its dialog closes) or destroyed in the middle of
///   it, when its window is deactivated or it loses the focus (but to a menu that opens), at a
///   move without the button that began it (its release never came), and when the dragged fin
///   set has left the rocket (an undo during the drag, whose event waits in the freeze): the
///   rocket is thawed then, and the drag edits nothing more. Java waits for a release that never
///   comes and leaves the rocket frozen; the area is the GUI's only holder of a freeze across
///   events.
/// - A press of a second button during a drag does nothing; Java begins the drag again and
///   freezes the frozen rocket (an error).
/// - The focus is taken at once, so a press on a point selects it after the table's selection
///   was cleared; Java's focus event comes after the press and clears the selection the press
///   made when the area did not have the focus. The focus a closing menu gives back is no
///   focusEntered() (Swing's lightweight popups never take the focus).
/// - Ctrl is Qt::ControlModifier, the Command key on macOS (Java: the Control key).
/// - A click is a press and a release at the same pixel; a double click is two clicks (Java's
///   click count is not used).
class FinPointScrollArea final : public ScaleScrollArea
{
    Q_OBJECT

public:
    /// The area of @p figure (taken over); @p preferences (which must outlive the area) holds
    /// "Live update the rocket".
    FinPointScrollArea(FinPointFigure* figure, const Preferences& preferences, QWidget* parent);
    ~FinPointScrollArea() override;
    FinPointScrollArea(const FinPointScrollArea&)            = delete;
    FinPointScrollArea& operator=(const FinPointScrollArea&) = delete;
    FinPointScrollArea(FinPointScrollArea&&)                 = delete;
    FinPointScrollArea& operator=(FinPointScrollArea&&)      = delete;

    [[nodiscard]] FinPointFigure& finFigure() const noexcept { return *m_figure; }

    /// The point being dragged, -1 when none (FreeformFinSetConfig's dragIndex).
    [[nodiscard]] int  dragIndex() const noexcept { return m_dragIndex; }
    [[nodiscard]] bool isDragging() const noexcept { return m_dragIndex >= 0; }
    /// Whether the area keeps the rocket frozen for the drag.
    [[nodiscard]] bool isRocketFrozenForDrag() const noexcept { return m_frozenRocket != nullptr; }

    /// Ends a drag as a release does (see the class comment); nothing without one.
    void endDrag();

    /// Brings the figure up to date, lays it out and repaints the rulers (Java's
    /// ScaleScrollPane.revalidate()).
    void revalidateFigure();

    /// The point the area locks the dragged one to with @p modifiers (getLockIndex()): with
    /// Qt::ControlModifier the next point, else the previous one, -1 for the first and the last
    /// point (and without a drag).
    [[nodiscard]] int lockIndex(Qt::KeyboardModifiers modifiers) const;
    /// The segment highlighted while snapping to @p lockIndex (getHighlightIndex()).
    [[nodiscard]] int highlightIndexFor(int lockIndex) const noexcept;

    /// @p point snapped to the nearest of the horizontal, the vertical and the 45 or 135 degree
    /// line through @p lockPoint (snapPoint()): distances to the first two are |dx| and |dy|, to
    /// the diagonal |x + b y + c| / sqrt(2) (b = -1 when dx and dy have the same sign, else 1); a
    /// diagonal snap puts the point (|dx| + |dy|) / 2 from the lock point on both axes.
    [[nodiscard]] static Point2D snapPoint(Point2D point, Point2D lockPoint) noexcept;

Q_SIGNALS:
    /// The editor should refresh its fields (Java: updateFields()).
    void refreshRequested();
    /// An edit of the history began or was committed (Java: begin/commitFinPointEdit()'s update
    /// of the undo and redo buttons).
    void editStateChanged();
    /// A press beside the outline deselected the points (Java: table.clearSelection()).
    void pointSelectionCleared();
    /// A right click on the point @p index at the screen position @p globalPosition (Java: the
    /// table's row selected, the actions updated and the figure's menu shown).
    void pointMenuRequested(int index, QPoint globalPosition);
    /// A drag scrolled the view and ended fitting (Java: selector.update()).
    void autoScrolled();
    /// The area took the focus (Java: the figure pane's focus listener), but from a closing menu.
    void focusEntered();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void changeEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    /// Runs @p handle (an event's handling) guarded (Guard::run()); after a failure ends a drag.
    template <class F>
    void guardedEvent(const F& handle);
    void press(QMouseEvent& event);
    void move(QMouseEvent& event);
    void release(QMouseEvent& event);
    /// Java's mouseReleased() without the pan: thaws the rocket when the drag froze it, clears
    /// the drag and the highlight, brings the figure up to date and commits the edit.
    void finishPress();
    /// The figure's pixel under the viewport's position @p position.
    [[nodiscard]] QPoint figurePixel(QPoint position) const;
    /// Begins a drag of the point @p index (the press's common part).
    void beginDrag(int index, QPoint position);
    /// Whether the fin set the drag began on is no longer the rocket's (see the class comment).
    [[nodiscard]] bool isDragTargetGone() const;
    /// Thaws the rocket when the drag froze it.
    void thawForDrag();
    /// The click at the viewport's position @p position (Java: mouseClicked()).
    void click(Qt::MouseButton button, Qt::KeyboardModifiers modifiers, QPoint position,
               QPoint globalPosition);
    /// Scrolls the view while the mouse is near its edge (handleScrolling()).
    void handleScrolling();
    void beginFinPointEdit();
    void commitFinPointEdit();

    FinPointFigure*                m_figure;
    const Preferences*             m_preferences;
    std::shared_ptr<DoubleBinding> m_rulerUnit;
    Signal<>::ScopedConnection     m_rulerUnitChanged;

    int         m_dragIndex{-1};
    QPoint      m_dragPoint;
    const void* m_dragTarget{nullptr};
    Rocket*     m_frozenRocket{nullptr};

    Qt::MouseButton m_pressButton{Qt::NoButton};
    QPoint          m_pressPosition;
    bool            m_moved{false};
};

}  // namespace QtRocket::Gui
