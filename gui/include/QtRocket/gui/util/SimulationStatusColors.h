#pragma once

#include <QColor>

#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/simulation/Simulation.h"

class QPalette;

namespace QtRocket::Gui
{

/// The colour OpenRocket writes a simulation's status in (UITheme.getStatusColor()): the
/// simulations table's tooltips, the simulation dialog's status line and the design view's
/// flight data. Here rather than in ThemeColors, so that ThemeColors.h, which most of the GUI
/// includes, need not include the core's Simulation.h.
class SimulationStatusColors
{
public:
    SimulationStatusColors() = delete;

    /// The key of @p status's colour: ERROR (OR.colors.error) for ABORTED and CANT_RUN, WARNING
    /// (OR.colors.warning) for OUTDATED and TEXT (OR.colors.text) for the others.
    [[nodiscard]] static ThemeColors::Key key(Simulation::Status status) noexcept;

    /// ThemeColors::color() of key(@p status) for @p palette.
    [[nodiscard]] static QColor color(Simulation::Status status, const QPalette& palette);

    /// color() for the application's palette, read now (call it when the text is made).
    [[nodiscard]] static QColor color(Simulation::Status status);
};

}  // namespace QtRocket::Gui
