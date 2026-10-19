#pragma once

#include <memory>

#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket
{

class FlightConfiguration;
class Rocket;

/// The stability units of a design (OpenRocket's UnitGroup.stabilityUnits(FlightConfiguration),
/// stabilityUnits(Rocket) and their secondary forms, with CaliberUnit and PercentageOfLengthUnit
/// bound to a configuration or a rocket): UNITS_STABILITY or UNITS_SECONDARY_STABILITY whose
/// caliber unit divides by the design's caliber and whose percentage unit by its aerodynamic
/// length. The design window's info overlay ("2.83 cal / 16.6 %": swing RocketInfo, Milestone 2
/// run 2) and the command-line tool's info command convert the stability margin with them.
///
/// The references, as Java computes them:
/// - calculateCaliber(): the largest fore or aft diameter (2 x getForeRadius(), 2 x
///   getAftRadius()) of the SymmetricComponents among the configuration's active components
///   (FlightConfiguration::getActiveComponents()), or among every component of the rocket for
///   the Rocket form, inactive stages included; CaliberUnit::kDefaultCaliber (0.01 m) when that
///   is below 0.1 mm (no body at all, or a hair-thin one);
/// - getPercentageReferenceLength(): the configuration's getLengthAerodynamic(), for the Rocket
///   form that of the rocket's selected configuration (Java: PercentageOfLengthUnit
///   .getReferenceLength(), not FlightConfiguration::getReferenceLength(), which is the reference
///   diameter of the aerodynamics). It is 0 for a design without components, and a percentage
///   then converts to an infinity or NaN, as in Java.
///
/// The bound units read their reference when they convert, as Java's do (CaliberUnit
/// .checkCaliber(), PercentageOfLengthUnit.checkLength()): each unit keeps the last reference and
/// computes it again when the configuration's modification id (FlightConfiguration::getModId())
/// or the rocket's (Rocket::getModId()) differs from the one it last saw; the Rocket form looks
/// at the rocket's only. So an existing group follows every change that draws a new id: an edit
/// of a component (also while the rocket is frozen, since the id is drawn before the event is
/// queued), a stage toggle (toggleStage(), setOnlyStage()), a new selected configuration for the
/// Rocket form. Like Java's, it keeps an outdated reference after a change that draws none:
/// FlightConfiguration::setAllStages() (which also leaves getLengthAerodynamic()'s own cache
/// alone), any change while the rocket's events are disabled, and for the Rocket form a stage
/// toggle of the selected configuration (no rocket event).
///
/// Lifetime and threads: a group refers to its configuration or rocket, which must outlive it.
/// Rocket::loadFrom() (undo and redo) replaces every configuration object of the rocket, and a
/// removed configuration is destroyed, so a holder of a configuration-bound group makes it anew
/// for the current configuration on every change event, as OpenRocket's RocketPanel does
/// (RocketInfo.setCurrentConfig() in updateExtras()). The units of the group are the group's own
/// (the plain ones are UNITS_STABILITY's): see UnitGroup::stabilityUnits(double) for the rule
/// that the group must outlive every unit and Value taken from it. A conversion refreshes the
/// unit's cache and reads the configuration's mutable caches, so a group belongs to the thread
/// of its rocket. The default unit is the process-wide group's, and setting it on a bound group
/// sets it there too (UnitGroup::StabilityUnitGroup).
class StabilityUnits
{
public:
    StabilityUnits() = delete;

    /// CaliberUnit.calculateCaliber(FlightConfiguration): the largest diameter of the symmetric
    /// components among the active components of @p configuration (see the class comment).
    [[nodiscard]] static double calculateCaliber(const FlightConfiguration& configuration);

    /// CaliberUnit.calculateCaliber(Rocket): the largest diameter of the symmetric components of
    /// @p rocket, every stage included.
    [[nodiscard]] static double calculateCaliber(const Rocket& rocket);

    /// PercentageOfLengthUnit.getReferenceLength(FlightConfiguration):
    /// @p configuration.getLengthAerodynamic().
    [[nodiscard]] static double getPercentageReferenceLength(
        const FlightConfiguration& configuration);

    /// PercentageOfLengthUnit.getReferenceLength(Rocket): the aerodynamic length of the selected
    /// configuration of @p rocket.
    [[nodiscard]] static double getPercentageReferenceLength(const Rocket& rocket);

    /// UnitGroup.stabilityUnits(FlightConfiguration): UNITS_STABILITY bound to
    /// @p configuration (see the class comment for when its references are read and how long
    /// it may live).
    [[nodiscard]] static std::unique_ptr<UnitGroup::StabilityUnitGroup> stabilityUnits(
        const FlightConfiguration& configuration);

    /// UnitGroup.secondaryStabilityUnits(FlightConfiguration): UNITS_SECONDARY_STABILITY bound
    /// to @p configuration.
    [[nodiscard]] static std::unique_ptr<UnitGroup::StabilityUnitGroup> secondaryStabilityUnits(
        const FlightConfiguration& configuration);

    /// UnitGroup.stabilityUnits(Rocket): UNITS_STABILITY bound to @p rocket (the calibers of
    /// every component, the percentage of the selected configuration's length).
    [[nodiscard]] static std::unique_ptr<UnitGroup::StabilityUnitGroup> stabilityUnits(
        const Rocket& rocket);

    /// UnitGroup.secondaryStabilityUnits(Rocket): UNITS_SECONDARY_STABILITY bound to @p rocket.
    [[nodiscard]] static std::unique_ptr<UnitGroup::StabilityUnitGroup> secondaryStabilityUnits(
        const Rocket& rocket);
};

}  // namespace QtRocket
