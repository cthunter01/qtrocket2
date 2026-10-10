#pragma once

#include <string>
#include <string_view>

namespace QtRocket
{

/// The warnings of the .ork loader that are QtRocket's own: what the loader says where it
/// refuses something that OpenRocket takes, or passes over something that OpenRocket dies of.
/// OpenRocket has no counterpart for any of them, so each says in a plain sentence what was
/// refused and what was done instead, in the place of OpenRocket's general "Invalid parameter
/// encountered, ignoring." (Warning::kFileInvalidParameter), which the loader gives only where
/// OpenRocket gives it. Every text is here, and no handler or setter spells one out itself.
///
/// The texts are English sentences that end in a full stop; a text that names a bound takes
/// the number from the constant of DocumentConfig. They are compared by tests and may be shown
/// to a user, so a change of one is a change of behaviour.
///
/// Where a handler has a warning of OpenRocket's own for a value it cannot read ("Illegal
/// ignition delay specified, ignoring."), it gives that same warning for a value it refuses
/// because it is not finite (decision L3 of the loader); those warnings are OpenRocket's texts
/// and are not in this list. Where the warning for a value that cannot be read is the general
/// one (an override, a deploy or separation value), the refusal of a number that is not
/// finite has a text of its own here.
class LoaderWarnings final
{
public:
    LoaderWarnings() = delete;

    // ------------------------------------------------------------------- the components

    /// A component beyond DocumentConfig::kMaxComponents: "<component name> would give the
    /// rocket more than 10000 components; ignoring this component and its subcomponents."
    /// (@p componentName is RocketComponent::getComponentName(): "Body Tube").
    [[nodiscard]] static std::string tooManyComponents(std::string_view componentName);

    /// A component more than ComponentHandler::kMaxDepth levels below the rocket: "<component
    /// name> is nested too deeply; ignoring this component and its subcomponents."
    [[nodiscard]] static std::string nestedTooDeeply(std::string_view componentName);

    /// A component that would take the rocket beyond DocumentConfig::kMaxInstances:
    /// "<component name> would give the rocket too many component instances; ignoring this
    /// component and its subcomponents."
    [[nodiscard]] static std::string tooManyInstances(std::string_view componentName);

    /// A count above the bound of its setter (DocumentConfig::kMaxCount): "A count above 10000
    /// was refused; the component keeps the count it had."
    [[nodiscard]] static std::string countAboveBound(int bound);

    /// An instance or fin count that would take the rocket beyond DocumentConfig::kMaxInstances.
    static constexpr std::string_view kCountBeyondInstances =
        "A count that would give the rocket too many component instances was refused; the "
        "component keeps the count it had.";

    /// A cluster configuration that would take the rocket beyond DocumentConfig::kMaxInstances.
    static constexpr std::string_view kClusterBeyondInstances =
        "A cluster configuration that would give the rocket too many component instances was "
        "refused; the component keeps the configuration it had.";

    /// An override mass, centre of gravity or drag coefficient that is a NaN or an infinity.
    static constexpr std::string_view kOverrideNotFinite =
        "An override value that is not finite was refused; the component keeps what it had.";

    /// An id that another component of the rocket has already.
    static constexpr std::string_view kDuplicateComponentId =
        "A component id that another component has already was refused; the component keeps a "
        "new id of its own.";

    /// A preset whose material a recovery device cannot take: "The ComponentPreset for
    /// component <name> has a material of a type the component cannot take; the preset is not
    /// loaded." (@p componentName is RocketComponent::getName(), as in OpenRocket's warnings
    /// about a preset.)
    [[nodiscard]] static std::string presetMaterialOfAnotherType(std::string_view componentName);

    /// A preset for a component of a frozen rocket: "The ComponentPreset for component <name>
    /// cannot be loaded while the rocket is frozen; the preset is not loaded."
    [[nodiscard]] static std::string presetIntoFrozenRocket(std::string_view componentName);

    // --------------------------------------------------------------------- appearances

    /// A <decal> that lacks its name, its rotation or its edge mode.
    static constexpr std::string_view kDecalIgnored =
        "A decal without a name, a rotation or an edge mode was ignored.";

    /// A <center>, <offset> or <scale> of a decal that lacks its x or its y.
    static constexpr std::string_view kDecalPairIgnored =
        "A center, offset or scale of a decal without x or y was ignored.";

    // ------------------------------------------------------- the flight configurations

    /// A <motorconfiguration> for one flight configuration more than
    /// DocumentConfig::kMaxFlightConfigurations: "The rocket has 1000 flight configurations
    /// already; ignoring a further motor configuration."
    [[nodiscard]] static std::string motorConfigurationBeyondBound();

    /// A <motor> that would make one flight configuration more than
    /// DocumentConfig::kMaxFlightConfigurations: "The rocket has 1000 flight configurations
    /// already; ignoring the motor of a further flight configuration."
    [[nodiscard]] static std::string motorBeyondConfigurationBound();

    /// A simulation that would make one flight configuration more than
    /// DocumentConfig::kMaxFlightConfigurations: "The rocket has 1000 flight configurations
    /// already; ignoring the simulation of a further flight configuration."
    [[nodiscard]] static std::string simulationBeyondConfigurationBound();

    /// A <motorconfiguration> whose flight configuration would take the rocket beyond
    /// DocumentConfig::kMaxInstances.
    static constexpr std::string_view kMotorConfigurationBeyondInstances =
        "A further flight configuration would give the rocket too many component instances; "
        "ignoring this motor configuration.";

    /// A <motor> whose flight configuration would take the rocket beyond
    /// DocumentConfig::kMaxInstances.
    static constexpr std::string_view kMotorBeyondInstances =
        "A further flight configuration would give the rocket too many component instances; "
        "ignoring this motor.";

    /// A simulation whose flight configuration would take the rocket beyond
    /// DocumentConfig::kMaxInstances.
    static constexpr std::string_view kSimulationBeyondInstances =
        "A further flight configuration would give the rocket too many component instances; "
        "ignoring this simulation.";

    /// A delay or an altitude of a deployment or separation configuration that is a NaN or an
    /// infinity: "The number of element 'deploydelay' is not finite; ignoring it."
    [[nodiscard]] static std::string numberNotFinite(std::string_view element);

    /// An <ignitionconfiguration> without an <ignitiondelay> that can be read.
    static constexpr std::string_view kIgnitionConfigurationWithoutDelay =
        "An ignition configuration has no ignition delay that can be read; the delay stays what "
        "it was.";

    /// A <deploymentconfiguration> whose configid spells out the id that stands for an error.
    static constexpr std::string_view kDeploymentConfigurationOfErrorId =
        "A deployment configuration for the reserved error configuration id was ignored.";

    /// A <separationconfiguration> whose configid spells out the id that stands for an error.
    static constexpr std::string_view kSeparationConfigurationOfErrorId =
        "A separation configuration for the reserved error configuration id was ignored.";

    // ------------------------------------------------------------------- the document

    /// A <pref type="integer"> whose text is no integer.
    static constexpr std::string_view kIntegerPreferenceUnreadable =
        "A document preference of type integer that is no integer was ignored.";

    /// A <pref type="double"> whose text is no number.
    static constexpr std::string_view kDoublePreferenceUnreadable =
        "A document preference of type double that is no number was ignored.";

    /// A material of <docmaterials> whose density is a NaN or an infinity.
    static constexpr std::string_view kDocumentMaterialDensityNotFinite =
        "A document material with a density that is not finite was ignored.";

    /// A material of <docmaterials> whose shear modulus is a NaN or an infinity.
    static constexpr std::string_view kDocumentMaterialShearModulusNotFinite =
        "A document material has a shear modulus that is not finite; using 0.";

    /// A <unit> of a custom expression without the attribute unittype.
    static constexpr std::string_view kUnitWithoutUnitType =
        "A unit of a custom expression without a unit type was ignored.";

    /// A list of an extension's or a preference's entries nested deeper than
    /// ConfigHandler::kMaxListDepth.
    static constexpr std::string_view kListNestedTooDeeply =
        "List entries nested too deeply, ignoring.";

    // ---------------------------------------------------------------- the simulations

    /// A simulation beyond DocumentConfig::kMaxSimulations: "The document has 10000
    /// simulations already; ignoring a further simulation."
    [[nodiscard]] static std::string tooManySimulations();

    /// A level beyond DocumentConfig::kMaxWindLevels: "The wind model has 10000 levels already;
    /// ignoring a further wind level."
    [[nodiscard]] static std::string tooManyWindLevels();

    /// A <windlevel> that lacks one of its four attributes.
    static constexpr std::string_view kWindLevelIncomplete =
        "A wind level without altitude, speed, direction or standard deviation was ignored.";

    /// A <windlevel> with an attribute that is a NaN or an infinity.
    static constexpr std::string_view kWindLevelNotFinite =
        "A wind level with a number that is not finite was ignored.";

    /// An altituderef of <wind> that names no reference: "Unknown wind altitude reference
    /// '<text>', using MSL."
    [[nodiscard]] static std::string unknownAltitudeReference(std::string_view text);

    /// A number of the conditions that would leave a simulation option that is not finite,
    /// where OpenRocket has no warning: "The number of element '<element>' would give a
    /// simulation option that is not finite; the option keeps what it had."
    [[nodiscard]] static std::string optionNotFinite(std::string_view element);

    // -------------------------------------------------------------------- the warnings

    /// The last warning of a load that gave more than DocumentConfig::kMaxWarnings: "The file
    /// gave more than 1000 warnings; the further ones were dropped."
    [[nodiscard]] static std::string tooManyWarnings();

    /// The last warning of the stored flight data of a simulation that holds more than
    /// DocumentConfig::kMaxWarnings: "The stored flight data holds more than 1000 warnings; the
    /// further ones were dropped."
    [[nodiscard]] static std::string tooManyStoredWarnings();
};

}  // namespace QtRocket
