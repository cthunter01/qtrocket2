#include "QtRocket/file/openrocket/LoaderWarnings.h"

#include <format>
#include <string>
#include <string_view>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

std::string LoaderWarnings::tooManyComponents(std::string_view componentName)
{
    return std::format(
        "{} would give the rocket more than {} components; ignoring this component "
        "and its subcomponents.",
        componentName, DocumentConfig::kMaxComponents);
}

std::string LoaderWarnings::tooManyAssemblies(std::string_view componentName)
{
    return std::format(
        "{} would give the rocket more than {} stages, booster sets and pod sets; ignoring this "
        "component and its subcomponents.",
        componentName, DocumentConfig::kMaxAssemblies);
}

std::string LoaderWarnings::nestedTooDeeply(std::string_view componentName)
{
    return std::format("{} is nested too deeply; ignoring this component and its subcomponents.",
                       componentName);
}

std::string LoaderWarnings::tooManyInstances(std::string_view componentName)
{
    return std::format(
        "{} would give the rocket too many component instances; ignoring this "
        "component and its subcomponents.",
        componentName);
}

std::string LoaderWarnings::countAboveBound(int bound)
{
    return std::format("A count above {} was refused; the component keeps the count it had.",
                       bound);
}

std::string LoaderWarnings::numberOutsideRange(std::string_view range)
{
    return std::format(
        "A number outside its range, {}, was refused; the component keeps what it had.", range);
}

std::string LoaderWarnings::tooManyDecalImages()
{
    return std::format(
        "The document has {} decal images already; ignoring the decal of a further image.",
        DocumentConfig::kMaxDecalImages);
}

std::string LoaderWarnings::decalFileOutsideDesign(std::string_view file)
{
    return std::format(
        "The decal image file '{}' was not read: a design may name such a file only by a "
        "relative path inside its own directory; ignoring the decal.",
        file);
}

std::string LoaderWarnings::decalFileOfDeviceName(std::string_view file)
{
    return std::format(
        "The decal image file '{}' was not read: its path holds a name that Windows reserves "
        "for a device; ignoring the decal.",
        file);
}

std::string LoaderWarnings::presetMaterialOfAnotherType(std::string_view componentName)
{
    return std::format(
        "The ComponentPreset for component {} has a material of a type the "
        "component cannot take; the preset is not loaded.",
        componentName);
}

std::string LoaderWarnings::presetIntoFrozenRocket(std::string_view componentName)
{
    return std::format(
        "The ComponentPreset for component {} cannot be loaded while the rocket is "
        "frozen; the preset is not loaded.",
        componentName);
}

std::string LoaderWarnings::numberNotFinite(std::string_view element)
{
    return std::format("The number of element '{}' is not finite; ignoring it.", element);
}

std::string LoaderWarnings::numberOutOfRange(std::string_view element, std::string_view range)
{
    return std::format("The number of element '{}' is outside its range, {}; ignoring it.", element,
                       range);
}

std::string LoaderWarnings::motorConfigurationBeyondBound()
{
    return std::format(
        "The rocket has {} flight configurations already; ignoring a further motor "
        "configuration.",
        DocumentConfig::kMaxFlightConfigurations);
}

std::string LoaderWarnings::motorBeyondConfigurationBound()
{
    return std::format(
        "The rocket has {} flight configurations already; ignoring the motor of a "
        "further flight configuration.",
        DocumentConfig::kMaxFlightConfigurations);
}

std::string LoaderWarnings::simulationBeyondConfigurationBound()
{
    return std::format(
        "The rocket has {} flight configurations already; ignoring the simulation "
        "of a further flight configuration.",
        DocumentConfig::kMaxFlightConfigurations);
}

std::string LoaderWarnings::tooManySimulations()
{
    return std::format("The document has {} simulations already; ignoring a further simulation.",
                       DocumentConfig::kMaxSimulations);
}

std::string LoaderWarnings::tooManyWindLevels()
{
    return std::format("The wind model has {} levels already; ignoring a further wind level.",
                       DocumentConfig::kMaxWindLevels);
}

std::string LoaderWarnings::unknownAltitudeReference(std::string_view text)
{
    return std::format("Unknown wind altitude reference '{}', using MSL.", text);
}

std::string LoaderWarnings::optionNotFinite(std::string_view element)
{
    return std::format(
        "The number of element '{}' would give a simulation option that is not "
        "finite; the option keeps what it had.",
        element);
}

std::string LoaderWarnings::optionOutOfRange(std::string_view element)
{
    return std::format(
        "The number of element '{}' would give a simulation option outside its "
        "range; the option keeps what it had.",
        element);
}

std::string LoaderWarnings::launchAltitudeLowered(double altitude, double lowered)
{
    return std::format(
        "The launch altitude of {} m is above the highest launch site the atmospheric model "
        "takes; it was lowered to {} m.",
        Strings::javaDoubleToString(altitude), Strings::javaDoubleToString(lowered));
}

std::string LoaderWarnings::lookupFileOutsideDesign(std::string_view element, std::string_view file)
{
    return std::format(
        "The lookup table file '{}' of element '{}' was not read: a design may name "
        "such a file only by a relative path inside its own directory.",
        file, element);
}

std::string LoaderWarnings::lookupFileOfDeviceName(std::string_view element, std::string_view file)
{
    return std::format(
        "The lookup table file '{}' of element '{}' was not read: its path holds a "
        "name that Windows reserves for a device.",
        file, element);
}

std::string LoaderWarnings::tooManyWarnings()
{
    return std::format("The file gave more than {} warnings; the further ones were dropped.",
                       DocumentConfig::kMaxWarnings);
}

std::string LoaderWarnings::tooManyStoredWarnings()
{
    return std::format(
        "The stored flight data holds more than {} warnings; the further ones were "
        "dropped.",
        DocumentConfig::kMaxWarnings);
}

}  // namespace QtRocket
