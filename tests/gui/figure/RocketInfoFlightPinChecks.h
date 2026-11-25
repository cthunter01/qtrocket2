#pragma once

// The comparisons of the design view's flight block with OpenRocket's (RocketInfoFlightPins.h:
// the rocket-info family's flight-blocks.json), shared by rocket_info_flight_golden_tests.cpp (a
// design's RocketInfo and BackgroundSimulation without a window) and the exit suite of the flight
// block (exit/open_designs_flight_block_exit_tests.cpp, plan 11.6 E5: a design opened in a window
// with the estimate on): the overlay's state (calculating, the configuration's name, the flight
// data, the status line), what RocketInfo draws of the block (each text with its font, colour,
// row above the bottom one and column, which do not depend on the font), and which simulations
// the estimate runs. Each reports what differs as a test failure (GoogleTest's EXPECTs).

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <QRect>

#include "QtRocket/gui/figure/RocketInfo.h"
#include "RocketInfoFlightPins.h"

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class BackgroundSimulation;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The visible rectangle the probe painted the overlay into: the block's rows and columns are
/// compared in RocketInfo::layout() of it.
inline constexpr QRect kFlightPinVisible{0, 0, 1000, 700};

/// The maxima of @p summary in the pins' order (altitude, velocity, Mach number, acceleration).
[[nodiscard]] std::array<double, 4> flightPinMaximaOf(
    const RocketInfo::FlightSummary& summary) noexcept;

/// The stored maxima of the simulation @p pin's overlay shows (FlightPinSimulation::maxima of its
/// shownSimulation), or none.
[[nodiscard]] std::optional<std::array<double, 4>> shownFlightPinMaxima(
    const RocketInfoFlightPin& pin);

/// Compares @p summary with @p maxima (altitude, velocity, Mach number, acceleration), NaN
/// included: exactly (stored values round-trip exactly).
void expectFlightPinMaxima(const RocketInfo::FlightSummary& summary,
                           const std::array<double, 4>&     maxima);

/// Compares the state of @p info with @p overlay, OpenRocket's overlay of the configuration named
/// @p configurationName: calculating, the configuration's name, the flight data (none; "N/A":
/// FlightData.NaN_DATA; or the shown simulation's, whose maxima must be @p shownMaxima: a test
/// failure without them) and the status line (the shown simulation's text when it is ABORTED,
/// else none).
void expectFlightOverlayAsJava(const RocketInfo& info, std::string_view configurationName,
                               const FlightPinOverlay&                     overlay,
                               const std::optional<std::array<double, 4>>& shownMaxima);

/// The items of the flight block of @p info's layout in kFlightPinVisible, with the application's
/// font: those a layout without flight data and without calculating does not have (they come
/// last). @p info is left as it was.
[[nodiscard]] std::vector<RocketInfo::Item> flightBlockItemsOf(RocketInfo& info);

/// Compares what @p info draws of the flight block in kFlightPinVisible (flightBlockItemsOf(), the
/// overlay's fonts of @p preferences) with @p texts: as many texts, each with its text (a value of
/// a fresh flight, which no test pins: the default unit's text of the number @p info's flight
/// data holds, "N/A" for NaN, which the callers compare with the simulation's), its font, its
/// colour (also the light theme's of its key), its row above the bottom one and its column (the
/// left margin, or the values' 10 pixels right of the widest label).
void expectFlightBlockAsJava(RocketInfo& info, const Preferences& preferences,
                             std::span<const FlightPinText> texts);

/// The simulations @p background runs or will run, in order (the running one, then its chain), by
/// their index in @p document (the number of its simulations for one that is not in it).
[[nodiscard]] std::vector<std::size_t> scheduledFlightPinSimulations(
    const BackgroundSimulation& background, const OpenRocketDocument& document);

}  // namespace QtRocket::Gui::Test
