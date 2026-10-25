#pragma once

#include <optional>

#include <QFrame>
#include <QObject>
#include <QPoint>
#include <QSize>

class QEvent;
class QLabel;
class QMouseEvent;
class QResizeEvent;
class QWidget;

namespace QtRocket::Gui
{

class ThrustCurvePlot;

/// The thrust-curve preview of OpenRocket's motor chooser (MotorInformationPanel.java:233-271,
/// 446-459: the JLayeredPane that holds the chart and its zoom icon): a ThrustCurvePlot of
/// Style::PREVIEW filling the frame (chart(): CustomLayeredPane.doLayout() gives the ChartPanel
/// the bounds (0, 0, w, h)), with the zoom icon (Icons::Id::ZOOM_IN, centred in a 50 x 50 label
/// at (w - 50, 12)) above it, a hand cursor over all of it, and a click of the left button
/// anywhere on it emitting plotRequested() (OpenRocket's ChartPanel opens the
/// ThrustCurveMotorPlotDialog then, when a motor is selected: whoever receives the signal checks
/// that). No other button, gesture, wheel or context menu does anything (OpenRocket's ChartPanel
/// has no zoom, wheel or popup; the wheel goes on to the parent, as Swing passes it on).
///
/// rocketplot v0.6.0 emits PlotWidget::clicked() only over the plot area and sets its own cursor
/// (the upstream gap N6), so the chart and the icon are transparent for the mouse
/// (Qt::WA_TransparentForMouseEvents): every press reaches this frame, whose cursor shows
/// everywhere. The frame draws no border: OpenRocket's layered pane has a 1-pixel LineBorder in
/// OR.colors.cg, but the ChartPanel's bounds cover it, so it never shows (painting the panel gives
/// the chart's background on the border's pixels).
class ThrustCurvePreview : public QFrame
{
    Q_OBJECT

public:
    /// The zoom icon's label: kZoomIconBox x kZoomIconBox, its left edge kZoomIconBox from the
    /// frame's right edge and its top kZoomIconTop below the frame's top
    /// (ZOOM_ICON_POSITION_NEGATIVE_X, ZOOM_ICON_POSITION_POSITIVE_Y and the label's 50 x 50).
    static constexpr int kZoomIconBox = 50;
    static constexpr int kZoomIconTop = 12;
    /// The preview's least and preferred size ("width 300:300:, height 180:180:").
    static constexpr int kWidth  = 300;
    static constexpr int kHeight = 180;

    /// The preview without curves (chart()->clear()'s empty axes).
    explicit ThrustCurvePreview(QWidget* parent = nullptr);

    /// The chart (objectName "chartPanel"): its setCurves() and clear() are
    /// MotorInformationPanel.updateData()'s and clearData()'s plot.
    [[nodiscard]] ThrustCurvePlot& chart() const noexcept { return *m_chart; }

    /// The label with the zoom icon (objectName "zoomIcon"), drawn at Icons::iconHeight() of the
    /// frame's font in its palette's colours (again when either, or the screen's pixel ratio,
    /// changes).
    [[nodiscard]] QLabel& zoomIcon() const noexcept { return *m_zoomIcon; }

    /// kWidth x kHeight.
    [[nodiscard]] QSize sizeHint() const override;
    /// kWidth x kHeight.
    [[nodiscard]] QSize minimumSizeHint() const override;

Q_SIGNALS:
    /// The left button was pressed and released on the preview without a drag between
    /// (QApplication::startDragDistance()): Swing's mouseClicked of BUTTON1, which a press and
    /// release in one place make.
    void plotRequested();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    /// CustomLayeredPane.doLayout(): the chart over the whole frame, the icon's label at (w - 50,
    /// 12).
    void placeChildren();
    /// Draws the zoom icon into its label at the size and in the colours of now.
    void updateZoomIcon();

    ThrustCurvePlot* m_chart    = nullptr;
    QLabel*          m_zoomIcon = nullptr;
    /// Where the left button went down, until it comes up again.
    std::optional<QPoint> m_pressPosition;
};

}  // namespace QtRocket::Gui
