#pragma once

#include <rocketplot/InputBindings.h>
#include <rocketplot/PlotWidget.h>

namespace QtRocket::Gui
{

/// The pointer gestures of QtRocket's plots, as rocketplot's InputBindings
/// (PlotWidget::setInputBindings()). OpenRocket's charts are JFreeChart ChartPanels; rocketplot's
/// gestures take the place of their mouse handling (the user's decision U-M3-6b, a documented
/// difference), arranged so that what a ChartPanel does with a gesture, the plot does too:
///
/// Each gesture, with what chartPanel() binds it to and what OpenRocket's ChartPanel does with it:
/// - Left drag: BOX_ZOOM; ChartPanel: the zoom rectangle.
/// - Shift + left drag: PAN; ChartPanel: the zoom rectangle.
/// - Ctrl + left drag: nothing; ChartPanel: nothing (a pan, which its plots refuse).
/// - Double-click (left): RESET (autoscale); ChartPanel: nothing (the popup's Auto Range).
/// - Back / forward mouse button: BACK / FORWARD; ChartPanel: nothing.
/// - Wheel: ZOOM about the pointer; ChartPanel: zoom about the pointer.
/// - Ctrl + wheel, Shift + wheel: ZOOM_X, ZOOM_Y; ChartPanel: zoom about the pointer (both axes).
/// - Trackpad scroll: PAN; ChartPanel: zoom (Java delivers it as the wheel).
/// - Ctrl + trackpad scroll: ZOOM; ChartPanel: zoom.
/// - Pinch: ZOOM; ChartPanel: nothing.
///
/// A ChartPanel restores the automatic ranges after a drag up and to the left, ignores a zoom
/// rectangle narrower than 10 pixels and zooms with the wheel only over the data area; rocketplot
/// zooms to any rectangle dragged out (a thin one along one axis), zooms one axis with the wheel
/// over it, and a double-click restores the ranges. The right button opens the context
/// menu, as it opens a ChartPanel's popup: rocketplot's menu (Back, Forward, Reset view, the
/// crosshair, Copy image) with QtRocket's Export... (PlotExport) instead of JFreeChart's Copy,
/// Save as PNG and Zoom In / Zoom Out / Auto Range submenus. Where OpenRocket's chart has none of
/// this (MotorInformationPanel's preview: no zoom, no wheel, no popup), setNoInteraction().
class PlotInputBindings
{
public:
    PlotInputBindings() = delete;

    /// The gestures of an interactive chart (a ChartPanel with zooming and the wheel on: the
    /// thrust-curve dialog, Milestone 4's plot dialog): the list above, which is
    /// rocketplot::InputBindings::defaults() with the left drag and the Shift + left drag swapped.
    [[nodiscard]] static rocketplot::InputBindings chartPanel();

    /// No gesture does anything (rocketplot::InputBindings with no binding).
    [[nodiscard]] static rocketplot::InputBindings none();

    /// Makes @p plot one the pointer does nothing to (OpenRocket's ChartPanel without zooming,
    /// mouse wheel, popup menu or tool tips, as MotorInformationPanel's preview): none(), no
    /// context menu (Qt::NoContextMenu), a legend that takes no clicks (Legend::setInteractive())
    /// and no crosshair.
    static void setNoInteraction(rocketplot::PlotWidget& plot);
};

}  // namespace QtRocket::Gui
