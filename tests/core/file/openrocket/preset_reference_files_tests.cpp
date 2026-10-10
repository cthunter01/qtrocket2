// What whole design files leave on their components when the application does not know the
// presets they name (the user's decision of 2026-10-08): every <preset> element of a file that
// OpenRocket would forget is kept, as the file has it (RocketComponent::getUnresolvedPreset()),
// for a save to write back; and with the presets known nothing of the kind is left.
//
// The files are OpenRocket's sixteen example designs, of which three name six presets, their
// re-saves, and the recorded files of tests/data/ork. QtRocket knows no preset at all until it
// reads preset files (Milestone 3), so the first case is the product's.

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetReference.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"
#include "rocket/preset/ExamplePresets.h"

namespace
{

using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetReference;
using QtRocket::componentPresetTypeName;
using QtRocket::GeneralRocketLoader;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::Parachute;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::ExamplePresetReference;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::kExamplePresetReferences;
using QtRocket::Test::loadGoldenExample;

using Lines   = std::vector<std::string>;
using Presets = DesignFileEnvironment::Presets;

/// A reference as one line: "<type> | <manufacturer> | <part number> | <digest>", "-" for a
/// type or a digest the file did not have.
[[nodiscard]] std::string lineOf(const ComponentPresetReference& reference)
{
    return std::format("{} | {} | {} | {}", reference.type.value_or("-"), reference.manufacturer,
                       reference.partNo, reference.digest.value_or("-"));
}

/// What the components of @p rocket hold of presets, in the order of the document: for each
/// component that remembers a preset that was not found "<the component's name>: " and
/// lineOf() of its reference, and for each that has a preset "<the component's name> has "
/// and the preset's part number.
[[nodiscard]] Lines presetsOf(const Rocket& rocket)
{
    Lines lines;
    rocket.forEach([&lines](const RocketComponent& component) {
        if (const std::optional<ComponentPresetReference>& reference =
                component.getUnresolvedPreset())
        {
            lines.push_back(std::format("{}: {}", component.getName(), lineOf(*reference)));
        }
        if (const ComponentPreset* const preset = component.getPresetComponent())
        {
            lines.push_back(std::format("{} has {}", component.getName(), preset->getPartNo()));
        }
    });
    return lines;
}

/// presetsOf() of every example, loaded from @p source with @p presets, each line with the
/// name of the example's file and " > " in front; a load that fails gives its failure.
[[nodiscard]] Lines presetsOfTheExamples(ExampleSource source, Presets presets)
{
    Lines lines;
    for (const GoldenExample& example : goldenExamples())
    {
        const Result<LoadedDocument> loaded = loadGoldenExample(example, source, presets);
        if (!loaded.has_value())
        {
            lines.push_back(std::format("{} > {}", example.file, loaded.error().message));
            continue;
        }
        for (const std::string& line : presetsOf(loaded->document->getRocket()))
        {
            lines.push_back(std::format("{} > {}", example.file, line));
        }
    }
    return lines;
}

/// The six <preset> elements of the examples (kExamplePresetReferences) as
/// presetsOfTheExamples() gives the references of the components that hold them.
[[nodiscard]] Lines theSixReferences()
{
    // The names of the six components, in the order of kExamplePresetReferences.
    constexpr std::array<std::string_view, 6> kNames{
        "Paper Body Tube -- DO NOT PRINT",
        "Parachute, plastic, preassembled, 10 in., PN 2262",
        "Parachute kit, plastic, 8 in., PN 2260",
        "Nose Cone",
        "Body Tube",
        "Nose Cone",
    };
    Lines lines;
    for (std::size_t i = 0; i < kExamplePresetReferences.size(); i++)
    {
        const ExamplePresetReference& reference = kExamplePresetReferences.at(i);
        lines.push_back(std::format("{} > {}: {} | {} | {} | {}", reference.example, kNames.at(i),
                                    componentPresetTypeName(reference.type), reference.manufacturer,
                                    reference.partNo, reference.digest));
    }
    return lines;
}

/// The line presetsOfTheExamples() gives for the component @p component of the example
/// @p example that is based on the preset with the part number @p partNo.
[[nodiscard]] std::string presetLine(std::string_view example, std::string_view component,
                                     std::string_view partNo)
{
    return std::format("{} > {} has {}", example, component, partNo);
}

/// The six presets of the examples as presetsOfTheExamples() gives the presets of the
/// components that are based on them.
[[nodiscard]] Lines theSixPresets()
{
    constexpr std::string_view kPrintable = "3D printable nose cone and fins.ork";
    constexpr std::string_view kPayload   = "Deployable payload.ork";
    constexpr std::string_view kPods      = "Pods--powered with recovery deployment.ork";
    return {
        presetLine(kPrintable, "Paper Body Tube -- DO NOT PRINT", "BT-50FE, 30359"),
        presetLine(kPayload, "Parachute, plastic, preassembled, 10 in., PN 2262", "PK-10, 2262"),
        presetLine(kPayload, "Parachute kit, plastic, 8 in., PN 2260", "PK-8, 2260"),
        presetLine(kPods, "Nose Cone", "PNC-50YR, 72604"),
        presetLine(kPods, "Body Tube", "BT-50, 30352"),
        presetLine(kPods, "Nose Cone", "PNC-20, 072606"),
    };
}

// The product until it reads preset files: the three examples that carry presets hold the six
// references their files give, each on the component whose element named it, and no other
// component of the sixteen designs holds anything.
TEST(PresetReferenceFiles, TheExamplesWithoutPresetsHoldTheSixReferencesOfTheirFiles)
{
    EXPECT_EQ(presetsOfTheExamples(ExampleSource::ORIGINAL, Presets::NONE), theSixReferences());
}

// With the six presets every reference is resolved, and none is left.
TEST(PresetReferenceFiles, TheExamplesWithTheirPresetsHoldNoReference)
{
    EXPECT_EQ(presetsOfTheExamples(ExampleSource::ORIGINAL, Presets::EXAMPLES), theSixPresets());
}

// OpenRocket's own save of the examples (with the presets found) writes the six elements
// again, and they load in the same two ways.
TEST(PresetReferenceFiles, TheResavedExamplesHoldTheSameSix)
{
    EXPECT_EQ(presetsOfTheExamples(ExampleSource::RESAVE, Presets::NONE), theSixReferences());
    EXPECT_EQ(presetsOfTheExamples(ExampleSource::RESAVE, Presets::EXAMPLES), theSixPresets());
}

/// presetsOf() of the design @p file of tests/data/ork, loaded without component presets.
[[nodiscard]] Lines presetsOfRecordedFile(std::string_view file)
{
    const DesignFileEnvironment  environment(Presets::NONE);
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> loaded =
        loader.load(QtRocket::Test::testDataDir() / "ork" / std::filesystem::path(file));
    if (!loaded.has_value())
    {
        return {loaded.error().message};
    }
    return presetsOf(loaded->document->getRocket());
}

/// The line presetsOf() gives for the component @p component that remembers the preset
/// @p partNo of the type @p type and the manufacturer @p manufacturer with the digest @p digest.
[[nodiscard]] std::string referenceLine(std::string_view component, std::string_view type,
                                        std::string_view manufacturer, std::string_view partNo,
                                        std::string_view digest)
{
    return std::format("{}: {} | {} | {} | {}", component, type, manufacturer, partNo, digest);
}

// A design of OpenRocket 13.05 with seven <preset> elements of a manufacturer whose presets
// no test has. Two centering rings name the same preset: the load has one warning for the two
// (a warning set holds a warning once), and each ring has its reference.
TEST(PresetReferenceFiles, ARecordedDesignKeepsEveryReferenceOfItsFile)
{
    constexpr std::string_view kSemroc = "SEMROC Astronautics";
    EXPECT_EQ(presetsOfRecordedFile("v1.5-preset-usage.ork"),
              (Lines{
                  referenceLine("Nose cone", "NOSE_CONE", kSemroc, "BNC-55F",
                                "b2ffefd485ea774f3c63322e1b8820b3"),
                  referenceLine("Body tube", "BODY_TUBE", kSemroc, "BT-55",
                                "3c547f48d153d6bef75cf178357e3fc7"),
                  referenceLine("Centering ring", "CENTERING_RING", kSemroc, "RA-5055",
                                "1dec8a924dc469f50cd54cf2725ae484"),
                  referenceLine("Centering ring", "CENTERING_RING", kSemroc, "RA-5055",
                                "1dec8a924dc469f50cd54cf2725ae484"),
                  referenceLine("Inner Tube", "BODY_TUBE", kSemroc, "BT-50J",
                                "4dd60fdb119e05990a4903fc36e67bf9"),
                  referenceLine("Launch lug", "LAUNCH_LUG", kSemroc, "LL-117",
                                "a0af9e35fb430dda0ca561850ce3921e"),
                  referenceLine("Parachute", "PARACHUTE", kSemroc, "PN-18",
                                "57273aa3aefaad85242e3b591e847671"),
              }));
    EXPECT_EQ(presetsOfRecordedFile("v1.7-tube-fin.ork"),
              Lines{referenceLine("Body tube", "BODY_TUBE", "FlisKits", "BT-50-18",
                                  "e64b2b02cb13361924b3d78c0a68911d")});
    // A design without a <preset> element.
    EXPECT_EQ(presetsOfRecordedFile("simplerocket.ork"), Lines{});
}

/// The parachute of @p rocket named @p name; null when it has none.
[[nodiscard]] Parachute* parachuteNamed(Rocket& rocket, std::string_view name)
{
    Parachute* found = nullptr;
    rocket.forEach([&found, name](RocketComponent& component) {
        if (component.getName() == name)
        {
            found = dynamic_cast<Parachute*>(&component);
        }
    });
    return found;
}

/// The example "Deployable payload" loaded without presets, and how many of the references of
/// its two parachutes are there after each step of a user who makes one of the parachutes
/// larger and takes that back.
[[nodiscard]] Lines referencesThroughAnEditAndItsUndo()
{
    const GoldenExample* const example =
        QtRocket::Test::findGoldenExample("Deployable payload.ork");
    if (example == nullptr)
    {
        return {"no such example"};
    }
    const Result<LoadedDocument> loaded =
        loadGoldenExample(*example, ExampleSource::ORIGINAL, Presets::NONE);
    if (!loaded.has_value())
    {
        return {loaded.error().message};
    }
    OpenRocketDocument& document = *loaded->document;
    Rocket&             rocket   = document.getRocket();
    Parachute* const    chute    = parachuteNamed(rocket, "Parachute kit, plastic, 8 in., PN 2260");
    if (chute == nullptr)
    {
        return {"no such parachute"};
    }
    Lines      lines;
    const auto note = [&lines, &rocket](std::string_view step) {
        lines.push_back(std::format("{}: {}", step, presetsOf(rocket).size()));
    };
    note("loaded");
    // A copy of the rocket, as a simulation and a state of the undo history take one.
    lines.push_back(
        std::format("a copy: {}", presetsOf(*rocket.copyRocketWithOriginalId()).size()));

    // The parachute is no longer the part the file named.
    document.addUndoPosition("Make the parachute larger");
    chute->setDiameter(chute->getDiameter() * 2);
    note("changed");
    // The undo replaces every component of the rocket.
    document.undo();
    note("undone");
    document.redo();
    note("redone");
    document.undo();
    note("undone again");
    return lines;
}

// The references of a loaded design live through what a user does with it: a copy of the
// rocket has them, a change of what a preset gives drops the one of that component, and the
// undo history has every state as it was.
TEST(PresetReferenceFiles, TheReferencesOfALoadedDesignSurviveCopyUndoAndRedo)
{
    EXPECT_EQ(referencesThroughAnEditAndItsUndo(),
              (Lines{"loaded: 2", "a copy: 2", "changed: 1", "undone: 2", "redone: 1",
                     "undone again: 2"}));
}

}  // namespace
