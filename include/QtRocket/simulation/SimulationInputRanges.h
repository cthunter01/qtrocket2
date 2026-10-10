#pragma once

#include <array>
#include <numbers>
#include <optional>
#include <string_view>

#include "QtRocket/aero/lookup/MachAoALookup.h"

namespace QtRocket
{

/// The ranges of the numbers a simulation run reads: the table that
/// Simulation::validateInputs() holds every input to, after it has found the input finite, and
/// that the .ork reader holds a value to before it applies it.
///
/// QtRocket's own; OpenRocket has no such table. There a finite number is taken as it is,
/// however absurd, and what becomes of it is whatever the computation makes of it: a time step
/// of 1e300 s is a first step that never ends and that no stop request reaches, a wind of
/// 1e300 m/s overflows on the way and ends in a BugException, a time step of zero or less is
/// silently replaced by a millisecond. A number like that comes from outside the program (a
/// design file, a preference store, a dialog), so it is refused with an ordinary error that
/// names it, like a NaN or an infinity (Simulation::validateInputs()), and the .ork reader does
/// not apply it (a warning; the option or the component keeps what it had).
///
/// The ranges are generous on purpose: each reaches about ten times beyond what a real launch
/// has, or is the range OpenRocket itself keeps a value in (where a setter of its own clamps,
/// or its reader refuses). They are there to keep absurd numbers out, not to judge a design:
/// a value inside its range need not make a flight that succeeds. Every simulation of the test
/// suite and of the committed goldens lies inside them.
///
/// The table is kTable: one row for every input, with the range and the reason for it. The
/// numbers that an extension reads have ranges of their own, which the extension states
/// (SimulationExtension::getInputNumbers(); RollControl and AirStart do).
///
/// A value is compared as it is stored, in the unit it is stored in: lengths in m, times in s,
/// speeds in m/s, angles in rad, but the latitude and the longitude in degrees and the angle
/// of attack of a lookup table in degrees, as the options and the tables hold them.
class SimulationInputRanges final
{
public:
    SimulationInputRanges() = delete;

    /// A range of a number: from min to max, both inside it, or with minExcluded from above min.
    struct Range
    {
        double min;
        double max;
        /// Whether min itself is outside the range ("above 0").
        bool minExcluded;
        /// The range in words, as a message has it: "from 0 to 1000 m".
        std::string_view text;

        /// Whether @p value lies in the range. Never for a NaN.
        [[nodiscard]] constexpr bool contains(double value) const noexcept
        {
            return (minExcluded ? value > min : value >= min) && value <= max;
        }
    };

    /// A row of the table: what a message of Simulation::validateInputs() calls the input, its
    /// range, and why the range is what it is.
    struct Row
    {
        std::string_view input;
        Range            range;
        std::string_view reason;
    };

    // ------------------------------------------------------------- the simulation options

    static constexpr Range kLaunchRodLength{
        .min = 0.0, .max = 1000.0, .minExcluded = false, .text = "from 0 to 1000 m"};

    /// SimulationOptions::kMaxLaunchRodAngle either way.
    static constexpr Range kLaunchRodAngle{.min         = -std::numbers::pi / 3,
                                           .max         = std::numbers::pi / 3,
                                           .minExcluded = false,
                                           .text        = "from -pi/3 to pi/3 rad"};

    static constexpr Range kLaunchLatitude{
        .min = -90.0, .max = 90.0, .minExcluded = false, .text = "from -90 to 90 degrees"};

    static constexpr Range kLaunchLongitude{
        .min = -180.0, .max = 180.0, .minExcluded = false, .text = "from -180 to 180 degrees"};

    static constexpr Range kLaunchAltitude{
        .min = -1000.0, .max = 100000.0, .minExcluded = false, .text = "from -1000 to 100000 m"};

    /// The speed of the average wind and of every level of the multi-level wind.
    static constexpr Range kWindSpeed{
        .min = 0.0, .max = 1000.0, .minExcluded = false, .text = "from 0 to 1000 m/s"};

    /// The standard deviation of the wind speed, of the average wind and of every level.
    static constexpr Range kWindSpeedDeviation{
        .min = 0.0, .max = 1000.0, .minExcluded = false, .text = "from 0 to 1000 m/s"};

    /// A compass direction: of the average wind, of a wind level, of the launch rod.
    static constexpr Range kDirection{.min         = -2 * std::numbers::pi,
                                      .max         = 2 * std::numbers::pi,
                                      .minExcluded = false,
                                      .text        = "from -2 pi to 2 pi rad"};

    static constexpr Range kWindLevelAltitude{
        .min = -1000.0, .max = 1000000.0, .minExcluded = false, .text = "from -1000 to 1000000 m"};

    static constexpr Range kLaunchTemperature{
        .min = 1.0, .max = 1000.0, .minExcluded = false, .text = "from 1 to 1000 K"};

    static constexpr Range kLaunchPressure{
        .min = 0.001, .max = 1000000.0, .minExcluded = false, .text = "from 0.001 to 1000000 Pa"};

    static constexpr Range kLaunchRelativeHumidity{
        .min = 0.0, .max = 1.0, .minExcluded = false, .text = "from 0 to 1"};

    static constexpr Range kConstantGravity{
        .min = 0.0, .max = 100.0, .minExcluded = false, .text = "from 0 to 100 m/s^2"};

    static constexpr Range kLookupMach{
        .min = 0.0, .max = 100.0, .minExcluded = false, .text = "from 0 to 100"};

    static constexpr Range kLookupAngleOfAttack{
        .min = -180.0, .max = 180.0, .minExcluded = false, .text = "from -180 to 180 degrees"};

    /// A value of a lookup table that is a coefficient: "cd", "cn", "cm".
    static constexpr Range kLookupCoefficient{.min         = -1000000.0,
                                              .max         = 1000000.0,
                                              .minExcluded = false,
                                              .text        = "from -1000000 to 1000000"};

    /// The value "cp" of a stability lookup table: where the centre of pressure is, along the
    /// rocket.
    static constexpr Range kLookupCentreOfPressure{
        .min = -1000.0, .max = 1000.0, .minExcluded = false, .text = "from -1000 to 1000 m"};

    static constexpr Range kTimeStep{
        .min = 0.0, .max = 10.0, .minExcluded = true, .text = "above 0 and at most 10 s"};

    static constexpr Range kMaxSimulationTime{
        .min = 0.0, .max = 1000000.0, .minExcluded = false, .text = "from 0 to 1000000 s"};

    /// The limits SimulationOptions::setMaximumStepAngle() clamps to: 1 and 20 degrees.
    static constexpr Range kMaximumStepAngle{.min         = 1 * std::numbers::pi / 180,
                                             .max         = 20 * std::numbers::pi / 180,
                                             .minExcluded = false,
                                             .text        = "from pi/180 to pi/9 rad"};

    /// Each of the four recovery speed warning thresholds.
    static constexpr Range kRecoverySpeedThreshold{
        .min = 0.0, .max = 10000.0, .minExcluded = false, .text = "from 0 to 10000 m/s"};

    // ------------------------------------------------------------------------- the design

    /// A delay: of the ignition and of the ejection charge of a motor, of the deployment of a
    /// recovery device, of the separation of a stage. (Motor::kPluggedDelay, +infinity, is an
    /// ejection delay as well: a motor without an ejection charge.)
    static constexpr Range kDelay{
        .min = 0.0, .max = 1000000.0, .minExcluded = false, .text = "from 0 to 1000000 s"};

    /// An altitude above the ground at which a recovery device deploys or a stage separates.
    static constexpr Range kEventAltitude{
        .min = 0.0, .max = 1000000.0, .minExcluded = false, .text = "from 0 to 1000000 m"};

    /// The range of @p number, a number of a lookup table: of a Mach number, of an angle of
    /// attack, or of a value by its column (of the centre of pressure for "cp", of a
    /// coefficient for every other column).
    [[nodiscard]] static const Range& ofLookupNumber(const MachAoALookup::Number& number);

    /// The first number of @p table that a run may not read, one that is not finite or lies
    /// outside its range, or nullopt when the whole table may be read. The numbers are gone
    /// through in the order of MachAoALookup::findNumber().
    [[nodiscard]] static std::optional<MachAoALookup::Number> findRefusedLookupNumber(
        const MachAoALookup& table);

    // -------------------------------------------------------------------------- the table

    /// Every input Simulation::validateInputs() reads, in the order it reads them, with its
    /// range and the reason for it.
    static constexpr std::array<Row, 29> kTable{{
        {.input  = "the launch rod length",
         .range  = kLaunchRodLength,
         .reason = "Rails and towers are a few metres long, the longest some tens of metres. "
                   "OpenRocket's dialog starts at 0, a launch without a rod."},
        {.input  = "the launch rod angle",
         .range  = kLaunchRodAngle,
         .reason = "OpenRocket's own limit, 60 degrees from the vertical, to which its setter "
                   "clamps."},
        {.input  = "the launch latitude",
         .range  = kLaunchLatitude,
         .reason = "The latitudes there are; OpenRocket's setter clamps to them."},
        {.input  = "the launch longitude",
         .range  = kLaunchLongitude,
         .reason = "The longitudes there are; OpenRocket's setter clamps to them."},
        {.input  = "the launch altitude",
         .range  = kLaunchAltitude,
         .reason = "The lowest land lies 430 m below sea level and the highest launch sites "
                   "below 5 km. OpenRocket's setter stores nothing above the 11 km at which its "
                   "atmospheric model takes a site; 100 km is where space begins."},
        {.input  = "the average wind speed",
         .range  = kWindSpeed,
         .reason = "The fastest wind measured near the ground had 113 m/s, a jet stream has "
                   "about as much. OpenRocket's dialog calls 20 m/s extreme."},
        {.input  = "the standard deviation of the wind speed",
         .range  = kWindSpeedDeviation,
         .reason = "No more than the wind speed in OpenRocket's dialog, so the range of the "
                   "wind speed."},
        {.input  = "the wind direction",
         .range  = kDirection,
         .reason = "A full turn either way. OpenRocket's setters reduce a direction to 0 ... 2 "
                   "pi; a huge number they reduce to nonsense, which is what the range keeps "
                   "out."},
        {.input  = "the altitude of a wind level",
         .range  = kWindLevelAltitude,
         .reason = "From below the lowest launch site to ten times the 100 km at which space "
                   "begins: a wind profile may reach above any flight."},
        {.input  = "the wind speed of a wind level",
         .range  = kWindSpeed,
         .reason = "As the average wind speed."},
        {.input  = "the wind direction of a wind level",
         .range  = kDirection,
         .reason = "As the wind direction."},
        {.input  = "the standard deviation of the wind speed of a wind level",
         .range  = kWindSpeedDeviation,
         .reason = "As the standard deviation of the average wind."},
        {.input  = "the launch rod direction",
         .range  = kDirection,
         .reason = "As the wind direction, which it is when launching into the wind."},
        {.input  = "the launch temperature",
         .range  = kLaunchTemperature,
         .reason = "Air at the ground has had between 184 K and 330 K. Near 0 K the density "
                   "of the air, the pressure over the temperature, grows beyond all bounds."},
        {.input  = "the launch pressure",
         .range  = kLaunchPressure,
         .reason = "About 100000 Pa at sea level and 0.03 Pa at 100 km. 0.001 Pa is the least "
                   "OpenRocket's reader and dialog take."},
        {.input  = "the launch relative humidity",
         .range  = kLaunchRelativeHumidity,
         .reason = "A fraction: OpenRocket's reader and its atmospheric model refuse anything "
                   "else."},
        {.input  = "the constant gravity",
         .range  = kConstantGravity,
         .reason = "9.8 m/s^2 on the Earth, 25 m/s^2 on Jupiter. OpenRocket's dialog starts at "
                   "0."},
        {.input  = "a Mach number of a lookup table",
         .range  = kLookupMach,
         .reason = "The fastest flights of such rockets stay below Mach 10, and no flight has "
                   "a negative Mach number."},
        {.input  = "an angle of attack of a lookup table",
         .range  = kLookupAngleOfAttack,
         .reason = "Every direction the air can come from."},
        {.input  = "a coefficient of a lookup table ('cd', 'cn', 'cm')",
         .range  = kLookupCoefficient,
         .reason = "The force coefficients of a rocket are below 100, and a moment "
                   "coefficient is a force coefficient times a lever arm of at most some "
                   "hundred reference lengths."},
        {.input  = "the centre of pressure of a lookup table ('cp')",
         .range  = kLookupCentreOfPressure,
         .reason = "A place along the rocket: the tallest rockets built measure about 120 m."},
        {.input  = "the time step",
         .range  = kTimeStep,
         .reason = "OpenRocket's dialog offers 0.01 s to 1 s. A step of zero or less the "
                   "steppers replace by 1 ms without saying so, and with a step of hours the "
                   "first step does not end."},
        {.input  = "the maximum simulation time",
         .range  = kMaxSimulationTime,
         .reason = "OpenRocket's default is 1200 s, and a descent from 100 km under a "
                   "parachute takes some hours; 1000000 s are eleven and a half days. With 0 a "
                   "flight ends as it begins."},
        {.input  = "the maximum step angle",
         .range  = kMaximumStepAngle,
         .reason = "OpenRocket's own limits, 1 and 20 degrees, to which its setter clamps."},
        {.input  = "a recovery speed warning threshold (each of the four)",
         .range  = kRecoverySpeedThreshold,
         .reason = "A speed at which a recovery device deploys: some tens of m/s, a few "
                   "hundred when a deployment has failed. OpenRocket's dialog starts at 0."},
        {.input  = "the ignition delay and the ejection delay of a motor",
         .range  = kDelay,
         .reason = "Seconds in a design, and no more than the longest simulation lasts. "
                   "OpenRocket's dialogs start at 0. A plugged motor has no ejection charge: "
                   "its delay, +infinity, is valid as well."},
        {.input  = "the deployment delay of a recovery device and the separation delay of a "
                   "stage",
         .range  = kDelay,
         .reason = "As the delays of a motor."},
        {.input  = "the deployment altitude of a recovery device",
         .range  = kEventAltitude,
         .reason = "Above the ground: some hundred metres in a design, and no more than the "
                   "highest wind level. OpenRocket's dialogs start at 0."},
        {.input  = "the separation altitude of a stage",
         .range  = kEventAltitude,
         .reason = "As the deployment altitude."},
    }};
};

}  // namespace QtRocket
