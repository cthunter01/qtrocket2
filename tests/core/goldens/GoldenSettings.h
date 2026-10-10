#pragma once

// Comparing a rocket with what OpenRocket's re-save of its design (<input>/resave/rocket.ork of
// the goldens, read with loadGoldenResave() of GoldenGeometry.h) says of the settings that no
// golden document records: when the recovery devices deploy, when the stages separate, when the
// motors ignite, and which component preset a component was made from. geometry.json has the
// ignition and the ejection delay of the motors that are active in a configuration and nothing
// of the deployments and the separations; of a preset it has only the values the preset gave
// the component, and a design file stores those itself but for two (kLostValues of
// example_design_golden_tests.cpp), so that the document cannot tell whether a preset was
// loaded; and a simulation times only what its own flight meets. Test-only.
//
// What is compared is what OpenRocket's savers write (RecoveryDeviceSaver, AxialStageSaver and
// RocketComponentSaver: addParams() for the preset and motorMountParams()), element by element:
// - a component with a component preset: <preset type manufacturer partno digest>, the name of
//   the preset's type, the simple name of its manufacturer, its part number and its digest. A
//   component has one exactly when the file has the element (SavedPresets says what is expected
//   of a design that was loaded without the presets);
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
// rocket, and the rocket has no preset, deployment, separation or motor beyond the elements
// (their numbers are compared). That no element sits where the comparison does not look is the
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
// rocket was given, which reads back as the same double. Texts, the four of a preset included,
// are compared as they are.

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
    /// <preset>, with its type, manufacturer, partno and digest. As a number of elements
    /// compared: those held against the preset of their component, or, for a design loaded
    /// without the presets (SavedPresets::NOT_FOUND), against its having none.
    int presets{0};

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
        presets += other.presets;
        return *this;
    }
};

/// What a rocket is to hold of the component presets its re-save names.
enum class SavedPresets
{
    /// A design loaded with a preset database that holds them, or a rocket built by a maker: a
    /// component has a preset exactly when the file has a <preset> for it, and it is that one.
    LOADED,
    /// A design loaded without them (an empty preset database, which is how the product loads
    /// a design until it reads preset files): no component has a preset, whatever the file
    /// names. OpenRocket's loader warns for each and goes on; the warnings are the business of
    /// the caller (the "loadWarnings" of geometry.json).
    NOT_FOUND,
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

/// Compares the preset and the deployment, separation and ignition settings of every component
/// of @p rocket with the elements of its component in @p resave (see the top of this file).
/// @p geometry is the geometry.json of the same input: it gives the id by which a component and
/// a flight configuration are found in the file. @p presets: what the rocket is to hold of the
/// presets the file names.
[[nodiscard]] SettingsComparison compareSavedSettings(const Rocket&             rocket,
                                                      const nlohmann::json&     geometry,
                                                      const pugi::xml_document& resave,
                                                      SavedPresets presets = SavedPresets::LOADED);

}  // namespace QtRocket::Test
