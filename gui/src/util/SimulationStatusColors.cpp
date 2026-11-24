#include "QtRocket/gui/util/SimulationStatusColors.h"

#include <QColor>
#include <QPalette>

#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/simulation/Simulation.h"

namespace QtRocket::Gui
{

ThemeColors::Key SimulationStatusColors::key(Simulation::Status status) noexcept
{
    switch (status)
    {
        case Simulation::Status::ABORTED:
        case Simulation::Status::CANT_RUN:
            return ThemeColors::Key::ERROR;
        case Simulation::Status::OUTDATED:
            return ThemeColors::Key::WARNING;
        default:
            return ThemeColors::Key::TEXT;
    }
}

QColor SimulationStatusColors::color(Simulation::Status status, const QPalette& palette)
{
    return ThemeColors::color(key(status), palette);
}

QColor SimulationStatusColors::color(Simulation::Status status)
{
    return ThemeColors::color(key(status));
}

}  // namespace QtRocket::Gui
