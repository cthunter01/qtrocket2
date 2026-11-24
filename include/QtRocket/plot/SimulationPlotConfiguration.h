#pragma once

#include <bitset>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/plot/PlotConfiguration.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/FlightEvent.h"

namespace QtRocket
{

// Instantiated once, in SimulationPlotConfiguration.cpp.
extern template class PlotConfiguration<FlightDataType, FlightDataBranch>;

/// What one simulation plot shows (OpenRocket's swing gui/plot/SimulationPlotConfiguration): a
/// PlotConfiguration over the flight data of a simulation's branches, with the flight events the
/// plot marks. defaultConfigurations() are the plot dialog's 9 presets, customConfiguration() the
/// "Custom" entry beside them.
///
/// Events: a set of FlightEvent types (Java: an EnumSet), empty for a new configuration, listed in
/// the declaration order of FlightEvent::Type.
///
/// Copying: a value type; a copy (and cloneConfiguration()) copies the events too (Java's clone()
/// does, its cloneConfiguration() and so fillAutoAxes() share the set with the source, which
/// nothing in OpenRocket changes afterwards).
///
/// Deviations from OpenRocket:
/// - DEFAULT_CONFIGURATIONS, a static array of shared, mutable configurations (OpenRocket's plot
///   panel resets the units of the first one in place), is defaultConfigurations(), new
///   configurations in the default units of the moment on every call. Each preset carries the
///   translation key of its name (getNameKey(): "PlotConfiguration.Verticalmotion", ...) beside
///   the English name OpenRocket's messages.properties gives it.
/// - customConfiguration() is OpenRocket's SimulationPlotPanel.CUSTOM_CONFIGURATION, which the
///   swing panel keeps; it is here with the presets it stands beside.
class SimulationPlotConfiguration : public PlotConfiguration<FlightDataType, FlightDataBranch>
{
public:
    using Base = PlotConfiguration<FlightDataType, FlightDataBranch>;

    /// The translation key and the English text of the "Custom" configuration's name
    /// (simplotpanel.CUSTOM).
    static constexpr std::string_view kCustomNameKey = "simplotpanel.CUSTOM";
    static constexpr std::string_view kCustomName    = "Custom";

    /// A configuration without a name over time (Java: SimulationPlotConfiguration(), whose name
    /// is null).
    SimulationPlotConfiguration();

    /// A configuration named @p name over time.
    explicit SimulationPlotConfiguration(std::string name);

    /// A configuration named @p name over @p domainType.
    SimulationPlotConfiguration(std::string name, const FlightDataType& domainType);

    /// A configuration named @p name, the English text of the translation key @p nameKey, over
    /// @p domainType.
    SimulationPlotConfiguration(std::string name, std::string nameKey,
                                const FlightDataType& domainType);

    /// The 9 presets of the plot dialog, in its order (Java: DEFAULT_CONFIGURATIONS), each over
    /// its rows in their unit groups' default units (y axis -1: automatic):
    /// 0 "Vertical motion vs. time": altitude on the left, vertical velocity and acceleration;
    /// 1 "Total motion vs. time": altitude on the left, total velocity and acceleration;
    /// 2 "Flight side profile" over the position east of launch: altitude;
    /// 3 "Ground track" over the position east of launch: the position north of launch on the
    ///   left, altitude on the right;
    /// 4 "Stability vs. time": stability on the left, the CP and CG locations on the right;
    /// 5 "Drag coefficients vs. Mach number": the drag, friction, base and pressure drag
    ///   coefficients on the left;
    /// 6 "Roll characteristics": the roll rate on the left, the roll moment, forcing and damping
    ///   coefficients on the right;
    /// 7 "Angle of attack and orientation vs. time": the angle of attack on the left, the two
    ///   orientations;
    /// 8 "Simulation time step and computation time": the time step and the computation time.
    /// The events: ignition, burnout, apogee, recovery device deployment, stage separation, ground
    /// hit, tumble, exception, warning and abort, without stage separation and tumble for the
    /// ground track, with launch rod clearance for the roll characteristics, and only exception,
    /// warning and abort for the drag coefficients.
    [[nodiscard]] static std::vector<SimulationPlotConfiguration> defaultConfigurations();

    /// The "Custom" configuration (kCustomName, kCustomNameKey) over time, without rows or events,
    /// which stands for any configuration that is not a preset in the plot dialog's list (Java:
    /// SimulationPlotPanel.CUSTOM_CONFIGURATION).
    [[nodiscard]] static SimulationPlotConfiguration customConfiguration();

    //// Events

    /// The marked event types, in FlightEvent::Type's declaration order (Java: getActiveEvents(),
    /// a copy of the EnumSet).
    [[nodiscard]] std::vector<FlightEvent::Type> getActiveEvents() const;

    /// Marks (@p active) or stops marking events of @p type.
    void setEvent(FlightEvent::Type type, bool active);

    /// Whether events of @p type are marked.
    [[nodiscard]] bool isEventActive(FlightEvent::Type type) const;

    /// A copy of this configuration (with its events) as a SimulationPlotConfiguration.
    [[nodiscard]] std::unique_ptr<Base> cloneConfiguration() const override;

private:
    std::bitset<FlightEvent::kAllTypes.size()> m_events;
};

}  // namespace QtRocket
