#pragma once

#include <QString>

namespace QtRocket
{
class Simulation;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The texts of a simulation's status as the GUI shows them: OpenRocket's
/// Simulation.Status.getDescription(sim) (the core's getDescription() and
/// Simulation::getStatusDescription(), which keep the English texts) through the application's
/// translators.
class SimulationStatusText
{
public:
    /// The description of @p simulation's status (its getStatus(), worked out now), with
    /// OpenRocket's markup: the translated text of Simulation.Status.Description.<status>
    /// (EnumText's translatedEnumText() with the core's descriptionKey()), and for an ABORTED
    /// simulation whose data has SIM_ABORT events, for every branch that has one, that text
    /// followed by "<i>: ", the first abort's text (its cause's translated text,
    /// SimulationAbort.<cause>, with the abort's sources as Message::toString() names them) and
    /// "</i><br>" (Java: Status.getDescription(sim), Simulation.java:103-132).
    [[nodiscard]] static QString description(Simulation& simulation);

    /// description() without its markup (Java: StringUtils.removeHTMLTags(), the core's
    /// Strings::removeHtmlTags()): "Simulation Aborted: No motors ignited" (the design view's
    /// line over the flight block, RocketInfo.java:615).
    [[nodiscard]] static QString plainDescription(Simulation& simulation);
};

}  // namespace QtRocket::Gui
