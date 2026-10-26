#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

/// The arithmetic of the sliders of the motor chooser's filter panel (OpenRocket's swing
/// MotorFilterPanel, gui/dialogs/motor/thrustcurve/MotorFilterPanel.java), without the widgets: the
/// stops of the "Nominal Diameter" slider and their labels, and where "Limit motor diameter to
/// mount diameter" and "Limit motor length to mount length" put the sliders' upper thumbs. The
/// panel sets MotorRowFilter's limits from the thumbs. (The "Total Impulse" slider runs over
/// kAllImpulseClasses, labelled with their toString().)
namespace QtRocket::MotorFilterSliders
{

/// A stop of the diameter slider (MotorFilterPanel.MotorDiameter): the nominal diameter its label
/// shows and the actual diameter the filter compares with, in m.
struct MotorDiameter
{
    double nominal;
    double actual;
};

/// The stops of the diameter slider, in order (motorDiameters): 0, 6, 13, 18, 24, 29, 32, 38, 54,
/// 75 and 98 mm nominal (38, 75 and 98 mm being 38.1, 76.3 and 98.3 mm actual), and 1 m, the "+"
/// that means no upper limit.
inline constexpr std::array<MotorDiameter, 12> kMotorDiameters{{
    {.nominal = 0, .actual = 0},
    {.nominal = 0.006, .actual = 0.006},
    {.nominal = 0.013, .actual = 0.013},
    {.nominal = 0.018, .actual = 0.018},
    {.nominal = 0.024, .actual = 0.024},
    {.nominal = 0.029, .actual = 0.029},
    {.nominal = 0.032, .actual = 0.032},
    {.nominal = 0.038, .actual = 0.0381},
    {.nominal = 0.054, .actual = 0.054},
    {.nominal = 0.075, .actual = 0.0763},
    {.nominal = 0.098, .actual = 0.0983},
    {.nominal = 1.000, .actual = 1.000},
}};

/// The labels of the diameter slider's stops (scaleDiameterLabels()), in the motor dimension unit
/// group's default unit: each nominal diameter rounded to a tenth of the unit's rounding
/// (Unit::round() of ten times the value, divided by ten, "for imperial units, the labels can
/// otherwise overlap"), written by Unit::toString() without the unit, with the leading "0" left
/// out of a value between 0 and 1; the last is "+". In mm: "0", "6", "13", ..., "98", "+"; in
/// inches: "0", ".24", ".51", ".71", ".94", "1.1", "1.3", "1.5", "2.1", "3", "3.9", "+".
[[nodiscard]] std::vector<std::string> diameterLabels();

/// The minimum diameter the slider's lower thumb at @p stop sets (setMinimumDiameter()): the
/// stop's actual diameter.
/// @throws BugError when @p stop is not a stop.
[[nodiscard]] double minimumDiameter(std::size_t stop);

/// The maximum diameter the slider's upper thumb at @p stop sets (setMaximumDiameter()): the
/// stop's actual diameter, and none (no upper limit) for the last stop, "+".
/// @throws BugError when @p stop is not a stop.
[[nodiscard]] std::optional<double> maximumDiameter(std::size_t stop);

/// Where "Limit motor diameter to mount diameter" puts the upper thumb for a mount of the motor
/// mount diameter @p mountDiameter (setLimitDiameter()): just below the first stop whose actual
/// diameter less 0.5 mm exceeds the mount's, so at the largest nominal diameter that fits (a 28 mm
/// mount gives the 24 mm stop, 4), or at the stop before the last when none exceeds it. -1 for a
/// negative mount diameter (which OpenRocket passes on to its slider as it is).
[[nodiscard]] int limitDiameterStop(double mountDiameter);

/// The value of the length slider's upper end (it runs from 0 to 1000 mm, in whole mm; at 1000 the
/// maximum length is +infinity, no limit).
inline constexpr int kLengthSliderMaximum = 1000;

/// Where "Limit motor length to mount length" puts the length slider's upper thumb for a mount of
/// the length @p mountLength (setLimitLength(): the mount's length in whole mm, Math.round, at most
/// kLengthSliderMaximum); the panel then sets the maximum length to the mount's length itself.
[[nodiscard]] int limitLengthStop(double mountLength);

}  // namespace QtRocket::MotorFilterSliders
