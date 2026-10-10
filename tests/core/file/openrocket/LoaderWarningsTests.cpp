#include "QtRocket/file/openrocket/LoaderWarnings.h"

#include <cstddef>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/simulation/SimulationInputRanges.h"

// The texts of the loader's own warnings (decision U7 of tier 9): QtRocket's, with nothing of
// OpenRocket's to compare them with. Each is pinned here word for word, because the tests of
// the handlers compare them and a user reads them.

namespace
{

using QtRocket::DocumentConfig;
using QtRocket::LoaderWarnings;
using QtRocket::Warning;

/// Every text of LoaderWarnings, the ones that take a name or a number with an example.
[[nodiscard]] std::vector<std::string> everyText()
{
    return {
        LoaderWarnings::tooManyComponents("Body Tube"),
        LoaderWarnings::nestedTooDeeply("Body Tube"),
        LoaderWarnings::tooManyInstances("Body Tube"),
        LoaderWarnings::countAboveBound(DocumentConfig::kMaxCount),
        std::string(LoaderWarnings::kCountBeyondInstances),
        std::string(LoaderWarnings::kClusterBeyondInstances),
        std::string(LoaderWarnings::kDuplicateComponentId),
        std::string(LoaderWarnings::kOverrideNotFinite),
        LoaderWarnings::numberNotFinite("deploydelay"),
        LoaderWarnings::numberOutOfRange("deploydelay", "from 0 to 1000000 s"),
        LoaderWarnings::presetMaterialOfAnotherType("Mine"),
        LoaderWarnings::presetIntoFrozenRocket("Mine"),
        std::string(LoaderWarnings::kDecalIgnored),
        std::string(LoaderWarnings::kDecalPairIgnored),
        LoaderWarnings::motorConfigurationBeyondBound(),
        LoaderWarnings::motorBeyondConfigurationBound(),
        LoaderWarnings::simulationBeyondConfigurationBound(),
        std::string(LoaderWarnings::kMotorConfigurationBeyondInstances),
        std::string(LoaderWarnings::kMotorBeyondInstances),
        std::string(LoaderWarnings::kSimulationBeyondInstances),
        std::string(LoaderWarnings::kIgnitionConfigurationWithoutDelay),
        std::string(LoaderWarnings::kDeploymentConfigurationOfErrorId),
        std::string(LoaderWarnings::kSeparationConfigurationOfErrorId),
        std::string(LoaderWarnings::kIntegerPreferenceUnreadable),
        std::string(LoaderWarnings::kDoublePreferenceUnreadable),
        std::string(LoaderWarnings::kDocumentMaterialDensityNotFinite),
        std::string(LoaderWarnings::kDocumentMaterialShearModulusNotFinite),
        std::string(LoaderWarnings::kUnitWithoutUnitType),
        std::string(LoaderWarnings::kListNestedTooDeeply),
        LoaderWarnings::tooManySimulations(),
        LoaderWarnings::tooManyWindLevels(),
        std::string(LoaderWarnings::kWindLevelIncomplete),
        std::string(LoaderWarnings::kWindLevelNotFinite),
        std::string(LoaderWarnings::kWindLevelOutOfRange),
        LoaderWarnings::unknownAltitudeReference("bogus"),
        LoaderWarnings::optionNotFinite("speed"),
        LoaderWarnings::optionOutOfRange("speed"),
        LoaderWarnings::lookupFileOutsideDesign("draglookup", "../drag.csv"),
        LoaderWarnings::lookupFileOfDeviceName("draglookup", "nul"),
        LoaderWarnings::tooManyWarnings(),
        LoaderWarnings::tooManyStoredWarnings(),
    };
}

/// The texts of @p texts that are not a sentence with a full stop at its end, that are
/// OpenRocket's general warning or hold it, or that another text of the list equals; one per
/// line, "" when there is none.
[[nodiscard]] std::string textsThatAreNotTheirOwn(const std::vector<std::string>& texts)
{
    const std::string     general = Warning::kFileInvalidParameter.toString();
    std::set<std::string> seen;
    std::string           report;
    for (const std::string& text : texts)
    {
        if (text.empty() || text.back() != '.' || text.contains(general) ||
            !seen.insert(text).second)
        {
            report += text + "\n";
        }
    }
    return report;
}

/// Whether @p text names the number @p bound as a word of its own.
[[nodiscard]] bool holds(const std::string& text, std::size_t bound)
{
    return text.contains(std::format(" {} ", bound));
}

TEST(LoaderWarnings, EveryTextIsASentenceOfItsOwn)
{
    const std::vector<std::string> texts = everyText();
    EXPECT_EQ(texts.size(), 41U);
    EXPECT_EQ(textsThatAreNotTheirOwn(texts), "");
    // What the helper tells: OpenRocket's general text, a text without a full stop, a text
    // that is there twice.
    EXPECT_EQ(textsThatAreNotTheirOwn(
                  {"Invalid parameter encountered, ignoring.", "No full stop", "Twice.", "Twice."}),
              "Invalid parameter encountered, ignoring.\nNo full stop\nTwice.\n");
}

TEST(LoaderWarnings, TheTextsOfTheComponents)
{
    EXPECT_EQ(LoaderWarnings::tooManyComponents("Mass Component"),
              "Mass Component would give the rocket more than 10000 components; ignoring this "
              "component and its subcomponents.");
    EXPECT_EQ(LoaderWarnings::nestedTooDeeply("Inner Tube"),
              "Inner Tube is nested too deeply; ignoring this component and its subcomponents.");
    EXPECT_EQ(LoaderWarnings::tooManyInstances("Launch Lug"),
              "Launch Lug would give the rocket too many component instances; ignoring this "
              "component and its subcomponents.");
    EXPECT_EQ(LoaderWarnings::countAboveBound(10000),
              "A count above 10000 was refused; the component keeps the count it had.");
    EXPECT_EQ(LoaderWarnings::kCountBeyondInstances,
              "A count that would give the rocket too many component instances was refused; the "
              "component keeps the count it had.");
    EXPECT_EQ(LoaderWarnings::kClusterBeyondInstances,
              "A cluster configuration that would give the rocket too many component instances "
              "was refused; the component keeps the configuration it had.");
    EXPECT_EQ(LoaderWarnings::kDuplicateComponentId,
              "A component id that another component has already was refused; the component "
              "keeps a new id of its own.");
    EXPECT_EQ(LoaderWarnings::kOverrideNotFinite,
              "An override value that is not finite was refused; the component keeps what it "
              "had.");
    EXPECT_EQ(LoaderWarnings::presetMaterialOfAnotherType("Mine"),
              "The ComponentPreset for component Mine has a material of a type the component "
              "cannot take; the preset is not loaded.");
    EXPECT_EQ(LoaderWarnings::presetIntoFrozenRocket("Mine"),
              "The ComponentPreset for component Mine cannot be loaded while the rocket is "
              "frozen; the preset is not loaded.");
    EXPECT_EQ(LoaderWarnings::kDecalIgnored,
              "A decal without a name, a rotation or an edge mode was ignored.");
    EXPECT_EQ(LoaderWarnings::kDecalPairIgnored,
              "A center, offset or scale of a decal without x or y was ignored.");
}

TEST(LoaderWarnings, TheTextsOfTheFlightConfigurations)
{
    EXPECT_EQ(LoaderWarnings::motorConfigurationBeyondBound(),
              "The rocket has 1000 flight configurations already; ignoring a further motor "
              "configuration.");
    EXPECT_EQ(LoaderWarnings::motorBeyondConfigurationBound(),
              "The rocket has 1000 flight configurations already; ignoring the motor of a "
              "further flight configuration.");
    EXPECT_EQ(LoaderWarnings::simulationBeyondConfigurationBound(),
              "The rocket has 1000 flight configurations already; ignoring the simulation of a "
              "further flight configuration.");
    EXPECT_EQ(LoaderWarnings::kMotorConfigurationBeyondInstances,
              "A further flight configuration would give the rocket too many component "
              "instances; ignoring this motor configuration.");
    EXPECT_EQ(LoaderWarnings::kMotorBeyondInstances,
              "A further flight configuration would give the rocket too many component "
              "instances; ignoring this motor.");
    EXPECT_EQ(LoaderWarnings::kSimulationBeyondInstances,
              "A further flight configuration would give the rocket too many component "
              "instances; ignoring this simulation.");
    EXPECT_EQ(LoaderWarnings::numberNotFinite("separationaltitude"),
              "The number of element 'separationaltitude' is not finite; ignoring it.");
    // The range is the text of its SimulationInputRanges::Range.
    EXPECT_EQ(LoaderWarnings::numberOutOfRange(
                  "separationaltitude", QtRocket::SimulationInputRanges::kEventAltitude.text),
              "The number of element 'separationaltitude' is outside its range, from 0 to "
              "1000000 m; ignoring it.");
    EXPECT_EQ(LoaderWarnings::kIgnitionConfigurationWithoutDelay,
              "An ignition configuration has no ignition delay that can be read; the delay stays "
              "what it was.");
    EXPECT_EQ(LoaderWarnings::kDeploymentConfigurationOfErrorId,
              "A deployment configuration for the reserved error configuration id was ignored.");
    EXPECT_EQ(LoaderWarnings::kSeparationConfigurationOfErrorId,
              "A separation configuration for the reserved error configuration id was ignored.");
}

TEST(LoaderWarnings, TheTextsOfTheDocument)
{
    EXPECT_EQ(LoaderWarnings::kIntegerPreferenceUnreadable,
              "A document preference of type integer that is no integer was ignored.");
    EXPECT_EQ(LoaderWarnings::kDoublePreferenceUnreadable,
              "A document preference of type double that is no number was ignored.");
    EXPECT_EQ(LoaderWarnings::kDocumentMaterialDensityNotFinite,
              "A document material with a density that is not finite was ignored.");
    EXPECT_EQ(LoaderWarnings::kDocumentMaterialShearModulusNotFinite,
              "A document material has a shear modulus that is not finite; using 0.");
    EXPECT_EQ(LoaderWarnings::kUnitWithoutUnitType,
              "A unit of a custom expression without a unit type was ignored.");
    EXPECT_EQ(LoaderWarnings::kListNestedTooDeeply, "List entries nested too deeply, ignoring.");
}

TEST(LoaderWarnings, TheTextsOfTheSimulations)
{
    EXPECT_EQ(LoaderWarnings::tooManySimulations(),
              "The document has 10000 simulations already; ignoring a further simulation.");
    EXPECT_EQ(LoaderWarnings::tooManyWindLevels(),
              "The wind model has 10000 levels already; ignoring a further wind level.");
    EXPECT_EQ(LoaderWarnings::kWindLevelIncomplete,
              "A wind level without altitude, speed, direction or standard deviation was "
              "ignored.");
    EXPECT_EQ(LoaderWarnings::kWindLevelNotFinite,
              "A wind level with a number that is not finite was ignored.");
    EXPECT_EQ(LoaderWarnings::kWindLevelOutOfRange,
              "A wind level with a number outside its range was ignored.");
    EXPECT_EQ(LoaderWarnings::unknownAltitudeReference("AGL "),
              "Unknown wind altitude reference 'AGL ', using MSL.");
    EXPECT_EQ(LoaderWarnings::optionNotFinite("recoveryspeedwarning"),
              "The number of element 'recoveryspeedwarning' would give a simulation option that "
              "is not finite; the option keeps what it had.");
    EXPECT_EQ(LoaderWarnings::optionOutOfRange("recoveryspeedwarning"),
              "The number of element 'recoveryspeedwarning' would give a simulation option "
              "outside its range; the option keeps what it had.");
    // The name of the file is the one the design has, not the path it would be read by.
    EXPECT_EQ(LoaderWarnings::lookupFileOutsideDesign("draglookup", "/home/someone/drag.csv"),
              "The lookup table file '/home/someone/drag.csv' of element 'draglookup' was not "
              "read: a design may name such a file only by a relative path inside its own "
              "directory.");
    EXPECT_EQ(LoaderWarnings::lookupFileOfDeviceName("stabilitylookupcsv", "tables/COM1.csv"),
              "The lookup table file 'tables/COM1.csv' of element 'stabilitylookupcsv' was not "
              "read: its path holds a name that Windows reserves for a device.");
    EXPECT_EQ(LoaderWarnings::tooManyWarnings(),
              "The file gave more than 1000 warnings; the further ones were dropped.");
    EXPECT_EQ(LoaderWarnings::tooManyStoredWarnings(),
              "The stored flight data holds more than 1000 warnings; the further ones were "
              "dropped.");
}

// A text that names a bound takes the number from DocumentConfig: the literals above are the
// bounds of today.
TEST(LoaderWarnings, ATextThatNamesABoundHasTheNumberOfItsConstant)
{
    EXPECT_TRUE(holds(LoaderWarnings::tooManyComponents("x"), DocumentConfig::kMaxComponents));
    EXPECT_TRUE(holds(LoaderWarnings::motorConfigurationBeyondBound(),
                      DocumentConfig::kMaxFlightConfigurations));
    EXPECT_TRUE(holds(LoaderWarnings::motorBeyondConfigurationBound(),
                      DocumentConfig::kMaxFlightConfigurations));
    EXPECT_TRUE(holds(LoaderWarnings::simulationBeyondConfigurationBound(),
                      DocumentConfig::kMaxFlightConfigurations));
    EXPECT_TRUE(holds(LoaderWarnings::tooManySimulations(), DocumentConfig::kMaxSimulations));
    EXPECT_TRUE(holds(LoaderWarnings::tooManyWindLevels(), DocumentConfig::kMaxWindLevels));
    EXPECT_TRUE(holds(LoaderWarnings::tooManyWarnings(), DocumentConfig::kMaxWarnings));
    EXPECT_TRUE(holds(LoaderWarnings::tooManyStoredWarnings(), DocumentConfig::kMaxWarnings));
    EXPECT_EQ(LoaderWarnings::countAboveBound(7),
              "A count above 7 was refused; the component keeps the count it had.");
}

}  // namespace
