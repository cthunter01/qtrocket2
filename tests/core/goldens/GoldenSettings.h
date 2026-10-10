#pragma once

// Comparing a rocket with what OpenRocket's re-save of its design (<input>/resave/rocket.ork of
// the goldens, read with loadGoldenResave() of GoldenGeometry.h) says of the settings that no
// golden document records: when the recovery devices deploy, when the stages separate and when
// the motors ignite. geometry.json has the ignition and the ejection delay of the motors that are
// active in a configuration and nothing of the other two, and a simulation times only what its
// own flight meets. Test-only.
//
// What is compared is what OpenRocket's savers write (RecoveryDeviceSaver, AxialStageSaver and
// RocketComponentSaver.motorMountParams()), element by element:
// - a recovery device: <deployevent>, <deployaltitude> and <deploydelay> of the default
//   deployment, and one <deploymentconfiguration configid> with the same three for every flight
//   configuration that has a deployment of its own;
// - a stage whose number is above 0 (the saver writes nothing for the first stage):
//   <separationevent>, <separationaltitude> and <separationdelay> of the default separation, and
//   one <separationconfiguration configid> for every flight configuration with its own;
// - a motor mount: <motormount> with <ignitionevent>, <ignitiondelay> and <overhang>; for every
//   flight configuration of the rocket that gives the mount a motor a <motor configid> (its
//   designation, digest and ejection delay, "none" for a plugged motor) and an
//   <ignitionconfiguration configid> with the event and the delay of that motor's ignition
//   (OpenRocket writes one for every motor: the motor of a configuration is never the mount's
//   default instance).
// A component that is not a motor mount has no <motormount>, and the first stage has no
// separation element.
//
// Both directions are held: every element of a component in the file is compared with the
// rocket, and the rocket has no deployment, separation or motor beyond the elements (their
// numbers are compared). That no element sits where the comparison does not look is the
// caller's to assert: savedSettingsCounts() counts the elements of the whole file, and the
// counts of what was compared have to be those.
//
// The rocket may be loaded from a file or rebuilt by a maker of TestRockets.h: the element of a
// component is found by the id of its golden entry (the id the re-save has; a rebuilt rocket has
// other ids), and a flight configuration by the id of the golden configuration at its place.
// For a loaded design those are its own ids, which compareComponentIds() and
// compareConfigurations() of GoldenDesign.h hold.
//
// Numbers are compared exactly: the file holds what Double.toString() printed of the value the
// rocket was given, which reads back as the same double.

#include <ostream>
#include <string>

#include <nlohmann/json_fwd.hpp>
#include <pugixml.hpp>

namespace QtRocket
{
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Test
{

/// How many of the settings elements a re-save holds, or how many were compared.
struct SettingsCounts
{
    /// <deployevent>: the default of every recovery device and one per <deploymentconfiguration>
    /// (each with its <deployaltitude> and <deploydelay>).
    int deployEvents{0};
    int deploymentConfigurations{0};  ///< <deploymentconfiguration>
    /// <separationevent>: the default of every stage above the first and one per
    /// <separationconfiguration> (each with its <separationaltitude> and <separationdelay>).
    int separationEvents{0};
    int separationConfigurations{0};  ///< <separationconfiguration>
    int motorMounts{0};  ///< <motormount>, with its <ignitionevent>, <ignitiondelay>, <overhang>
    int motors{0};       ///< <motor>, with its <designation>, <digest> and <delay>
    int ignitionConfigurations{0};  ///< <ignitionconfiguration>

    [[nodiscard]] bool operator==(const SettingsCounts&) const = default;

    SettingsCounts& operator+=(const SettingsCounts& other)
    {
        deployEvents += other.deployEvents;
        deploymentConfigurations += other.deploymentConfigurations;
        separationEvents += other.separationEvents;
        separationConfigurations += other.separationConfigurations;
        motorMounts += other.motorMounts;
        motors += other.motors;
        ignitionConfigurations += other.ignitionConfigurations;
        return *this;
    }
};

/// "31 deployevent, 6 deploymentconfiguration, ...", for the messages of the tests.
[[nodiscard]] std::string toText(const SettingsCounts& counts);

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const SettingsCounts& counts);

/// The settings elements of the design @p resave, wherever they are in the file.
[[nodiscard]] SettingsCounts savedSettingsCounts(const pugi::xml_document& resave);

/// What the comparison of a rocket with the settings of its re-save compared and found.
struct SettingsComparison
{
    SettingsCounts compared;
    std::string    report;  ///< the mismatches, empty when everything matched
};

/// Compares the deployment, separation and ignition settings of every component of @p rocket with
/// the elements of its component in @p resave (see the top of this file). @p geometry is the
/// geometry.json of the same input: it gives the id by which a component and a flight
/// configuration are found in the file.
[[nodiscard]] SettingsComparison compareSavedSettings(const Rocket&             rocket,
                                                      const nlohmann::json&     geometry,
                                                      const pugi::xml_document& resave);

}  // namespace QtRocket::Test
