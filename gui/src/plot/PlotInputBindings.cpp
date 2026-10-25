#include "QtRocket/gui/plot/PlotInputBindings.h"

#include <Qt>

#include <rocketplot/InputBindings.h>
#include <rocketplot/Legend.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/enums.h>

namespace QtRocket::Gui
{

rocketplot::InputBindings PlotInputBindings::chartPanel()
{
    using rocketplot::Gesture;
    using rocketplot::PlotAction;
    rocketplot::InputBindings bindings = rocketplot::InputBindings::defaults();
    // A ChartPanel's drag draws the zoom rectangle; rocketplot's drag pans, Shift + drag zooms to
    // a box: swapped.
    bindings.bind(Gesture::DRAG, Qt::LeftButton, Qt::NoModifier, PlotAction::BOX_ZOOM);
    bindings.bind(Gesture::DRAG, Qt::LeftButton, Qt::ShiftModifier, PlotAction::PAN);
    return bindings;
}

rocketplot::InputBindings PlotInputBindings::none()
{
    return {};
}

void PlotInputBindings::setNoInteraction(rocketplot::PlotWidget& plot)
{
    plot.setInputBindings(none());
    plot.setContextMenuPolicy(Qt::NoContextMenu);
    plot.legend()->setInteractive(false);
    plot.setCrosshairEnabled(false);
}

}  // namespace QtRocket::Gui
