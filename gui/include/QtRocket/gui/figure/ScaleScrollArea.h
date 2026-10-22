#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QScrollArea>
#include <QSize>
#include <Qt>

class QLabel;
class QMouseEvent;
class QResizeEvent;
class QShowEvent;
class QWidget;

namespace QtRocket
{
class Unit;
}

namespace QtRocket::Gui
{

class Ruler;
class ScaleFigure;

/// The scroll area of a ScaleFigure with rulers (OpenRocket's ScaleScrollPane): the design
/// view's and the fin point editor's. The figure is its widget, resized to the larger of the
/// viewport and the figure's canvas; a horizontal and a vertical Ruler, 20 pixels thick, sit
/// above and to the left of the viewport (viewport margins 20, 20, 0, 0), with a corner widget
/// in the top left corner (Java: a UnitSelector for the rulers' unit; here a label of the unit's
/// symbol until setRulerCornerWidget() puts the selector there). Both scroll bars are always
/// shown and step 50 pixels.
///
/// Fitting (isFitting()): while it is on, the figure is scaled to fit the area less 40 pixels in
/// each direction (fitViewSize(), Java's "magic" viewport margin) whenever the area is resized,
/// the figure changes (ScaleFigure::changed()) or updateScaling() is called, which the design
/// view does on every rocket change; then the area scrolls to the figure's autoZoomPoint().
/// setScaling() (an explicit zoom) turns fitting off and keeps the point at the centre of the
/// view at the same fraction of the figure once it is resized.
///
/// Panning: a drag with one of panButtons() (the middle and right buttons unless set) moves the
/// view with the mouse. The scroll area sees the mouse events its figure leaves unaccepted (Qt
/// passes an ignored event on to the parent, the viewport): a figure that uses a button itself
/// accepts its press. The wheel scrolls, as Qt's scroll areas do.
///
/// Every change is applied at once: the figure is laid out (resized) before setScaling(),
/// setFitting() and updateScaling() return, where Java revalidates later.
///
/// A scroll repaints the whole visible figure, as the design view's JViewport.SIMPLE_SCROLL_MODE
/// does in Java (RocketPanel), so that what a figure paints at its visible rectangle (the
/// information overlay) is not carried along with the pixels Qt moves.
///
/// Deviations from OpenRocket:
/// - The area has the platform style's frame instead of a light grey line border.
/// - The change a refit of the figure causes does not make it fit again: Java's figure listener
///   refits on every change, its own refit's too, which recurses until the stack overflows for a
///   design longer than about 1e161 m (see ScaleFigure::scaleTo()).
class ScaleScrollArea : public QScrollArea
{
    Q_OBJECT

public:
    /// ScaleScrollPane.viewportMarginPx: the fitted view is the area less this in each direction.
    static constexpr int kViewportMargin = 40;
    /// The scroll bars' unit increment.
    static constexpr int kScrollStep = 50;
    /// The unit label's pixel size (Java: the unit selector's font size 8).
    static constexpr int kCornerFontPixelSize = 8;

    /// A scroll area showing @p figure, which it takes as its widget (and owns from now on);
    /// fitting is off.
    ScaleScrollArea(ScaleFigure* figure, QWidget* parent);

    [[nodiscard]] ScaleFigure* figure() const noexcept { return m_figure; }
    [[nodiscard]] Ruler*       horizontalRuler() const noexcept { return m_horizontalRuler; }
    [[nodiscard]] Ruler*       verticalRuler() const noexcept { return m_verticalRuler; }

    /// Whether the figure is fitted to the area (isFitting()).
    [[nodiscard]] bool isFitting() const noexcept { return m_fit; }
    /// Turns fitting on or off; on, the figure is fitted now (setFitting()).
    void setFitting(bool shouldFit);

    /// The figure's user scale (getUserScale()).
    [[nodiscard]] double userScale() const;
    /// The user scale that would fit the figure into fitViewSize() (getFitScale()).
    [[nodiscard]] double fitScale();
    /// The user scale relative to fitScale(), 1 when the fit scale is not a positive finite
    /// number (getRelativeScale(): the design view's zoom).
    [[nodiscard]] double relativeScale();
    /// Zooms to the user scale @p newScale and turns fitting off, unless it is within 1 % of the
    /// current one (setScaling()). The view's centre stays at the same fraction of the figure.
    void setScaling(double newScale);
    /// setScaling() @p relativeScale times fitScale(), unless that is not a positive finite
    /// number (setRelativeScaling()).
    void setRelativeScaling(double relativeScale);

    /// The area's size less kViewportMargin in each direction: what fitting fits the figure into.
    [[nodiscard]] QSize fitViewSize() const;
    /// The part of the figure in the viewport, in the figure's pixels
    /// (getViewport().getViewRect()).
    [[nodiscard]] QRect viewRect() const;

    /// The reaction to a resize of the area or the figure (componentResized()): when fitting,
    /// fits the figure and scrolls to its autoZoomPoint(); else, after a zoom, puts the view's
    /// centre back at its fraction of the figure. The design view calls it on every rocket change
    /// so that a smaller rocket is fitted again.
    void updateScaling();

    /// The rulers' unit (getCurrentUnit()); the default length unit at first.
    [[nodiscard]] const Unit& rulerUnit() const noexcept { return *m_rulerUnit; }
    /// Sets the rulers' unit, which must outlive the area (a unit group's unit does), and emits
    /// rulerUnitChanged() when it changed.
    void setRulerUnit(const Unit& unit);
    /// Sets the rulers' unit to the length group's default unit (updateRulerUnit(): after the
    /// preferences changed).
    void updateRulerUnit();

    /// The widget in the corner between the rulers.
    [[nodiscard]] QWidget* rulerCornerWidget() const noexcept { return m_corner; }
    /// Puts @p widget in the corner between the rulers (20 x 20 pixels), taking it over; the
    /// previous one, the unit label at first, is deleted.
    void setRulerCornerWidget(QWidget* widget);

    /// The mouse buttons whose drag pans the view (the middle and right buttons at first).
    [[nodiscard]] Qt::MouseButtons panButtons() const noexcept { return m_panButtons; }
    void setPanButtons(Qt::MouseButtons buttons) noexcept { m_panButtons = buttons; }
    /// Whether a pan drag is in progress.
    [[nodiscard]] bool isPanning() const noexcept { return m_panButton != Qt::NoButton; }

Q_SIGNALS:
    /// setRulerUnit() changed the unit.
    void rulerUnitChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    /// Java's revalidate(): brings the figure up to date, lays it out and repaints the rulers.
    void revalidate();
    /// The figure changed: repaints the rulers and fits again when fitting, unless the change is
    /// that of the refit itself.
    void figureChanged();
    /// Scrolls so that the figure's pixel @p topLeft is at the viewport's top left corner, as
    /// far as the scroll bars go (scrollRectToVisible() of a viewport-sized rectangle).
    void scrollFigureTo(QPoint topLeft);
    /// Places the rulers and the corner widget around the viewport.
    void layoutRulers();
    void updateUnitLabel();

    ScaleFigure*     m_figure;
    Ruler*           m_horizontalRuler = nullptr;
    Ruler*           m_verticalRuler   = nullptr;
    QWidget*         m_corner          = nullptr;
    QLabel*          m_unitLabel       = nullptr;
    const Unit*      m_rulerUnit;
    bool             m_fit            = false;
    bool             m_figureRescaled = false;
    bool             m_refitting      = false;
    QPointF          m_viewCentre{0.5, 0.5};
    int              m_resizeDepth = 0;
    Qt::MouseButtons m_panButtons{Qt::MiddleButton | Qt::RightButton};
    Qt::MouseButton  m_panButton = Qt::NoButton;
    QPoint           m_panPosition;
};

}  // namespace QtRocket::Gui
