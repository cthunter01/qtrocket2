// Tests of goldens/GoldenExamples.h, the fixture that loads OpenRocket's example designs for
// the tests: the list of the examples, the environment of a load, the two sources of every
// example, and the settled state the goldens describe.
//
// What OpenRocket makes of the examples as loaded is the business of
// tests/core/file/openrocket/example_files_tests.cpp. The numbers of the settled state here
// are OpenRocket's too: the Java probe ExampleProbe of the probes of tier 9c, part "examples",
// loads each example, runs the golden harness's AutomaticDimensions.settle() on it and prints
// the masses, the centres of mass and the length (the section "settled" of ExampleOrkFiles.h);
// it finds the same numbers for OpenRocket's re-save of the example.

#include "goldens/GoldenExamples.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/DesignFileState.h"
#include "file/openrocket/ExampleOrkFiles.h"
#include "goldens/GoldenData.h"
#include "simulation/SimulationRunSupport.h"

namespace
{

using QtRocket::DocumentLoadingContext;
using QtRocket::InMemoryPreferences;
using QtRocket::LoadedDocument;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::ExampleOrkFile;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::findGoldenExample;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::kExampleOrkFiles;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::loadSettledGoldenExample;
using QtRocket::Test::settleGoldenExample;

using Presets = DesignFileEnvironment::Presets;
using Lines   = std::vector<std::string>;

/// The names of the .ork files of data/examples, sorted.
[[nodiscard]] Lines designsOfTheExamplesDirectory()
{
    Lines names;
    for (const auto& entry :
         std::filesystem::directory_iterator(QtRocket::Test::dataDir() / "examples"))
    {
        if (entry.path().extension() == ".ork")
        {
            names.push_back(entry.path().filename().string());
        }
    }
    std::ranges::sort(names);
    return names;
}

/// The names of the directories of tests/data/goldens that start with "example-", sorted.
[[nodiscard]] Lines exampleDirectoriesOfTheGoldens()
{
    Lines names;
    for (const auto& entry : std::filesystem::directory_iterator(QtRocket::Test::goldensDir()))
    {
        const std::string name = entry.path().filename().string();
        if (entry.is_directory() && name.starts_with("example-"))
        {
            names.push_back(name);
        }
    }
    std::ranges::sort(names);
    return names;
}

/// What the fixture lists: the files in the order of the list, and the golden inputs, sorted.
struct Listed
{
    Lines files;
    Lines names;
    /// The names the tests of the examples get (goldenExampleTestName() is made from the name
    /// of the golden input as testName() makes it here), each once.
    std::set<std::string> testNames;
    /// The examples one of whose two files does not exist.
    Lines missing;
};

/// The name of the test of @p example, as goldenExampleTestName() gives it.
[[nodiscard]] std::string testName(const GoldenExample& example)
{
    return QtRocket::Test::goldenExampleTestName(
        ::testing::TestParamInfo<GoldenExample>(example, 0));
}

[[nodiscard]] Listed listed()
{
    Listed result;
    for (const GoldenExample& example : goldenExamples())
    {
        result.files.push_back(example.file);
        result.names.push_back(example.name);
        result.testNames.insert(testName(example));
        if (!std::filesystem::is_regular_file(example.path(ExampleSource::ORIGINAL)) ||
            !std::filesystem::is_regular_file(example.path(ExampleSource::RESAVE)))
        {
            result.missing.push_back(example.file);
        }
    }
    std::ranges::sort(result.names);
    return result;
}

// The examples of the fixture are the sixteen designs of data/examples, in the order of their
// file names, each with the directory of its goldens and its two files; a test of an example
// is named after its golden input.
TEST(GoldenExamples, AreTheSixteenExamplesOfTheManifest)
{
    const Listed list = listed();
    EXPECT_EQ(list.files.size(), 16U);
    EXPECT_TRUE(std::ranges::is_sorted(list.files));
    EXPECT_EQ(list.files, designsOfTheExamplesDirectory());
    EXPECT_EQ(list.names, exampleDirectoriesOfTheGoldens());
    EXPECT_EQ(list.missing, Lines{});
    EXPECT_EQ(list.testNames.size(), 16U);
    EXPECT_TRUE(list.testNames.contains("a_simple_model_rocket"));
    EXPECT_TRUE(list.testNames.contains("pods_powered_with_recovery_deployment"));

    // An example is found by the name of its file and by that of its golden input.
    const GoldenExample* const byFile = findGoldenExample("A simple model rocket.ork");
    ASSERT_NE(byFile, nullptr);
    EXPECT_EQ(byFile->name, "example-a-simple-model-rocket");
    EXPECT_EQ(byFile->original,
              QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork");
    EXPECT_EQ(byFile->resave, QtRocket::Test::goldensDir() / "example-a-simple-model-rocket" /
                                  "resave" / "rocket.ork");
    EXPECT_EQ(findGoldenExample("example-a-simple-model-rocket"), byFile);
    EXPECT_EQ(findGoldenExample("simplerocket.ork"), nullptr);
}

/// The row of ExampleOrkFiles.h for the example whose file is named @p file, or null.
[[nodiscard]] const ExampleOrkFile* tableOf(std::string_view file)
{
    // Through a span: an iterator of an array is a pointer with some standard libraries.
    const std::span<const ExampleOrkFile> rows(kExampleOrkFiles);
    const auto found = std::ranges::find(rows, file, &ExampleOrkFile::file);
    return found == rows.end() ? nullptr : &*found;
}

/// What is wrong with a load of @p example from @p source and with its settled state: the
/// Error of the load, its warnings, a document without a stage or without the simulations of
/// the goldens, a settling that fails or takes another number of passes than OpenRocket's, and
/// the masses, centres of mass and the length of the settled design that are not OpenRocket's.
/// Empty when nothing is.
[[nodiscard]] std::string problemsOf(const GoldenExample& example, ExampleSource source)
{
    const ExampleOrkFile* const  row    = tableOf(example.file);
    const Result<LoadedDocument> loaded = loadGoldenExample(example, source);
    if (row == nullptr || !loaded)
    {
        return row == nullptr ? "not in ExampleOrkFiles.h\n" : loaded.error().toString() + "\n";
    }
    std::string   problems;
    const Rocket& rocket = loaded->document->getRocket();
    if (!loaded->warnings.empty())
    {
        problems += std::format("{} warnings, the first: {}\n", loaded->warnings.size(),
                                loaded->warnings.begin()->toString());
    }
    if (rocket.getStageCount() == 0 || loaded->document->getSimulationCount() == 0)
    {
        problems += std::format("{} stages, {} simulations\n", rocket.getStageCount(),
                                loaded->document->getSimulationCount());
    }
    const Result<int> passes = settleGoldenExample(*loaded);
    if (!passes || *passes != row->settlingPasses)
    {
        problems += passes ? std::format("settled in {} passes, OpenRocket in {}\n", *passes,
                                         row->settlingPasses)
                           : passes.error().toString() + "\n";
    }
    return problems +
           QtRocket::Test::wrongNumbers(row->settled, QtRocket::Test::massAndLength(rocket));
}

/// problemsOf() of every example loaded from @p source, each with the name of its file in
/// front; "" when no load has a problem.
[[nodiscard]] std::string problemsOfTheExamples(ExampleSource source)
{
    std::string report;
    for (const GoldenExample& example : goldenExamples())
    {
        const std::string problems = problemsOf(example, source);
        if (!problems.empty())
        {
            report += std::format("{}:\n{}", example.file, problems);
        }
    }
    return report;
}

/// The preference store of an environment of the fixture once a load has written the stepper
/// method into it: what OpenRocket's test preferences answer (storeJavaTestPreferences()), a
/// motor shown by its common name, and the stepper method RK4. It names no default material.
struct StoreAfterALoad
{
    InMemoryPreferences store;

    StoreAfterALoad()
    {
        QtRocket::Test::storeJavaTestPreferences(store);
        store.setMotorNameColumn(false);
        store.setSimulationStepperMethodName("RK4");
    }
};

/// Whether @p store holds what the fixture gave it and nothing else but, when a load has
/// written it, the stepper method, and that as RK4, which is also what a store without the key
/// answers: no load of an example changes what a later one reads. (The store of a process is
/// shared by its tests, so whether the key is there depends on what ran before.)
[[nodiscard]] bool isAsTheFixtureMadeIt(const InMemoryPreferences& store)
{
    InMemoryPreferences withTheMethod(store);
    withTheMethod.setSimulationStepperMethodName("RK4");
    return withTheMethod == StoreAfterALoad().store &&
           store.getSimulationStepperMethodName() == "RK4";
}

// Every example loads from both of its sources without an Error and without a warning, and
// its automatic dimensions settle (settleGoldenExample()) into the state the goldens describe:
// in as many passes as the golden harness takes, and with OpenRocket's masses, centres of mass
// and length, for the original and for the re-save. One test for all thirty-two loads: the
// bundled motor database is read once per process.
//
// The loads share the environment and change nothing of it but one key of the preference
// store: the stepper method, which every simulation of a re-save and of the two examples of
// file version 1.11 names, and names as RK4.
TEST(GoldenExamples, LoadFromBothSourcesAndSettleIntoTheStateOfTheGoldens)
{
    const DesignFileEnvironment& environment = goldenExampleEnvironment();
    EXPECT_TRUE(isAsTheFixtureMadeIt(environment.preferences()));

    EXPECT_EQ(problemsOfTheExamples(ExampleSource::ORIGINAL), "");
    EXPECT_EQ(problemsOfTheExamples(ExampleSource::RESAVE), "");

    // Every simulation of a re-save names its stepper method.
    EXPECT_TRUE(environment.preferences() == StoreAfterALoad().store);
}

/// What the context of @p environment holds: "motor finder, preferences, materials, <n>
/// presets, <n> extension providers", with "no" in front of what is missing.
[[nodiscard]] std::string contextOf(const DesignFileEnvironment& environment)
{
    const DocumentLoadingContext& context = environment.context();
    return std::format("{}motor finder, {}, {}materials, {} presets, {} extension providers",
                       context.getMotorFinder() == nullptr ? "no " : "",
                       context.getPreferences() == &environment.preferences() ? "its preferences"
                                                                              : "other preferences",
                       context.getApplicationMaterials() == nullptr ? "no " : "",
                       context.getComponentPresetDatabase() == nullptr
                           ? std::string("no")
                           : std::to_string(context.getComponentPresetDatabase()->size()),
                       context.getSimulationExtensionRegistry() == nullptr
                           ? std::string("no")
                           : std::to_string(context.getSimulationExtensionRegistry()->size()));
}

// The environment of the loads is one object per choice of presets for the whole process, with
// what the golden harness loads the examples with: a motor finder, the preference store of
// OpenRocket's tests with the common names of the motors, OpenRocket's materials, the six
// presets of the examples or none, and the four bundled extension providers. A document loaded
// with the settling done at once (loadSettledGoldenExample()) is in the settled state.
TEST(GoldenExamples, ShareOneEnvironmentPerChoiceOfPresets)
{
    const DesignFileEnvironment& withPresets = goldenExampleEnvironment(Presets::EXAMPLES);
    const DesignFileEnvironment& withoutAny  = goldenExampleEnvironment(Presets::NONE);
    EXPECT_EQ(&withPresets, &goldenExampleEnvironment());
    EXPECT_EQ(&withoutAny, &goldenExampleEnvironment(Presets::NONE));
    EXPECT_NE(&withPresets, &withoutAny);
    EXPECT_EQ(contextOf(withPresets),
              "motor finder, its preferences, materials, 6 presets, 4 extension providers");
    EXPECT_EQ(contextOf(withoutAny),
              "motor finder, its preferences, materials, 0 presets, 4 extension providers");
    EXPECT_FALSE(withPresets.preferences().getMotorNameColumn());
    EXPECT_TRUE(isAsTheFixtureMadeIt(withoutAny.preferences()));

    const GoldenExample* const example = findGoldenExample("Dual parachute deployment.ork");
    ASSERT_NE(example, nullptr);
    const ExampleOrkFile* const row = tableOf(example->file);
    ASSERT_NE(row, nullptr);
    const Result<LoadedDocument> settled =
        loadSettledGoldenExample(*example, ExampleSource::RESAVE, Presets::NONE);
    ASSERT_TRUE(settled.has_value()) << settled.error().toString();
    EXPECT_EQ(QtRocket::Test::wrongNumbers(
                  row->settled, QtRocket::Test::massAndLength(settled->document->getRocket())),
              "");
    // The re-save holds the six <preset> elements of three examples; this one has none, so it
    // loads without presets as with them.
    EXPECT_TRUE(settled->warnings.empty());
}

}  // namespace
