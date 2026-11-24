#pragma once

#include <optional>

#include "QtRocket/simulation/PlotAppearance.h"

namespace QtRocket
{

class FlightDataType;
class Preferences;

/// What OpenRocket's simulation plots keep in the preferences, as free functions on a Preferences
/// (as motor/MotorPreferences.h does for the motor chooser): the user's default appearance of
/// each flight data type (SwingPreferences.getDefaultPlotAppearance() and
/// setDefaultPlotAppearance(), SwingPreferences.java:1098-1167: the plot panel's "Set as
/// default"), and the flags of the plot dialog and panel (ApplicationPreferences'
/// PLOT_SHOW_POINTS, PLOT_SHOW_EVENTS and MARKER_STYLE_ICON with the defaults their readers give:
/// PlotDialog.java, SimulationPlotDialog.java:58-59, SimulationPlotPanel.java:238), under Java's
/// keys (PreferenceKeys.h).
///
/// A default appearance is stored in the node "PlotDefaultAppearances"
/// (kPlotDefaultAppearancesNode) under the type's name N: "N.color" as "R,G,B"
/// (Preferences::stringifyColor(), without the alpha) and "N.lineStyle" as the LineStyle
/// constant's name ("DASHDOT", lineStyleName()).
///
/// Every getter reads, and creates no node and stores nothing.
///
/// Placement: OpenRocket keeps these in its swing package; they are typed on FlightDataType and
/// PlotAppearance (simulation/), which preferences/ may not include, so they are here with the
/// plot model.
///
/// Deviation from OpenRocket: the key is the type's English name (FlightDataType::getName());
/// Java's getName() is the name in the user's language, so OpenRocket keeps a default per
/// language.
namespace PlotPreferences
{

// ---- the default appearances

/// SwingPreferences.getDefaultPlotAppearance(): the default appearance stored for @p type, or
/// nullopt when nothing is stored for it (neither key exists) or what is stored gives an empty
/// appearance. The colour is Preferences::parseColor() of the stored text (nullopt when it does
/// not parse, a channel clamped to 0-255, the alpha opaque); the line style is the LineStyle whose
/// constant name is the stored text exactly (Java: LineStyle.valueOf(), nullopt for anything
/// else: "dashed", "DOTTED "). nullopt for a type without a name.
[[nodiscard]] std::optional<PlotAppearance> getDefaultPlotAppearance(const Preferences& preferences,
                                                                     const FlightDataType& type);

/// SwingPreferences.setDefaultPlotAppearance(): stores @p appearance as the default of @p type:
/// its colour as "R,G,B" (the alpha is dropped) and its line style by name, each key removed when
/// the appearance has none. nullopt or an empty appearance removes both keys. Nothing happens for
/// a type without a name.
void setDefaultPlotAppearance(Preferences& preferences, const FlightDataType& type,
                              const std::optional<PlotAppearance>& appearance);

// ---- the flags

/// "Show data points" of the plot dialog (kPlotShowPoints, "ShowPlotPoints"), default false.
[[nodiscard]] bool isShowPoints(const Preferences& preferences);
void               setShowPoints(Preferences& preferences, bool show);
/// "Show events" of the plot dialog (kPlotShowEvents, "ShowPlotEvents"), default true.
[[nodiscard]] bool isShowEvents(const Preferences& preferences);
void               setShowEvents(Preferences& preferences, bool show);
/// The plot panel's marker style "Icon" rather than "Vertical line" (kMarkerStyleIcon,
/// "MarkerStyleIcon"), default false: whether a plot over time marks its events with icons on
/// the curves (SimulationPlotModel::getIconMarkers()) rather than with lines
/// (SimulationPlotModel::getEventGroups()); a plot over another type always uses icons.
[[nodiscard]] bool isMarkerStyleIcon(const Preferences& preferences);
void               setMarkerStyleIcon(Preferences& preferences, bool icon);

}  // namespace PlotPreferences

}  // namespace QtRocket
