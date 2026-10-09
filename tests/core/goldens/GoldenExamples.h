#pragma once

// OpenRocket's sixteen example designs (data/examples), loaded for the tests as the golden
// harness loaded them (tools/openrocket-goldens, GoldenDumper.examplePass()): the one fixture of
// the tests that load an example, whether they look at the design as loaded or compare it with
// its goldens (tests/data/goldens/example-*). Test-only.
//
// WHAT A TEST MAY RELY ON.
//
// The examples (goldenExamples()) are the inputs of kind "example" of the goldens' manifest, in
// its order, which is the order of the file names. Each can be loaded from two sources: the
// file OpenRocket ships (ExampleSource::ORIGINAL, a zip archive with the decal images and, in
// two of them, thrust curves) and OpenRocket's re-save of the loaded design
// (ExampleSource::RESAVE, resave/rocket.ork of its goldens: plain XML of file version 1.11,
// written before anything was settled. Its simulations keep their options and their summary
// values and have no stored branch, and its decal images are named and cannot be read: the
// re-save is the document alone).
//
// The environment of a load (goldenExampleEnvironment()) is that of the harness, a
// DesignFileEnvironment:
// - the motors come from the bundled motor database behind a DatabaseMotorFinder, so every
//   <motor> element gets the motor OpenRocket gave it, the seven included that are taken by
//   their designation because no curve of the database has the file's digest;
// - the component presets are the six the examples refer to (DesignFileEnvironment::Presets::
//   EXAMPLES), with which a load gives what OpenRocket's whole preset database gives, or none
//   (Presets::NONE), which is what the product has before it reads preset files: then three
//   examples give six "No matching ComponentPreset ..." warnings and lose the six links;
// - the preference store holds what OpenRocket's test preferences answer
//   (storeJavaTestPreferences()), shows a motor by its common name (setMotorNameColumn(false):
//   the names of the goldens, "[G40-7]" and not "[G40W-7]") and names no default material;
// - the application's materials are OpenRocket's own, the extension providers the bundled
//   ones (RollControl, AirStart, the script and the Java code that never run).
// There is one environment for each of the two preset choices in a test process, made when it
// is first asked for and alive until the process ends: it outlives every document a test
// loads, so a test keeps a loaded document as long as it likes. The motor database is read
// once per process (about half a second in a debug build, some seconds under the sanitizers,
// and ctest runs every test in a process of its own: put what needs no motor of its own into
// few tests).
//
// A load (loadGoldenExample()) gives a new document every time, in the state the loader leaves
// it in, which is OpenRocket's state after GeneralRocketLoader.load(): AS LOADED. Automatic
// dimensions are stale in it where OpenRocket's are, and what a getter answers can depend on
// what was asked before (see AutomaticDimensions.h). It is the state OpenRocket's own tests of
// the examples look at (ExampleFilesTest) and the one a save writes; with the six presets the
// load of either source gives no warning (tests/core/file/openrocket/example_files_tests.cpp
// compares this state with OpenRocket's for every example).
//
// The goldens describe another state, the SETTLED one: the harness settles the automatic
// dimensions before it dumps geometry, mass, aerodynamics and simulations.
// settleGoldenExample() is that step, to be taken right after the load and before anything
// else is asked of the design; loadSettledGoldenExample() does both. Never settle a document
// that is to be saved or compared with a re-save.
//
// What is shared between the loads of a process is the environment, and of it only the
// preference store can change: reading a <simulationsteppermethod> element writes the method
// into the store (as OpenRocket writes it into its preferences). Every such element of the
// examples and of their re-saves names RK4, which is also what a store without the key
// answers, so no load changes what another one reads. A test must not write to the store.
//
// The names a design shows (a flight configuration's, an AirStart extension's) are formatted
// with the default units: a test that looks at them holds a DefaultUnitsGuard.

#include <filesystem>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Test
{

/// Where a load of an example takes the design from.
enum class ExampleSource
{
    /// The file of data/examples: what OpenRocket ships.
    ORIGINAL,
    /// resave/rocket.ork of the example's goldens: OpenRocket's save of the loaded design.
    RESAVE,
};

/// One of OpenRocket's example designs and where its files are.
struct GoldenExample
{
    /// The name of its golden input, which is the name of its directory in tests/data/goldens:
    /// "example-a-simple-model-rocket".
    std::string name;
    /// The name of its file in data/examples: "A simple model rocket.ork".
    std::string file;
    /// The file of data/examples.
    std::filesystem::path original;
    /// OpenRocket's re-save of the design among its goldens.
    std::filesystem::path resave;

    /// The file of @p source.
    [[nodiscard]] const std::filesystem::path& path(ExampleSource source) const noexcept
    {
        return source == ExampleSource::ORIGINAL ? original : resave;
    }
};

/// What GoogleTest prints for the example of a test that failed: the name of its file.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
inline void PrintTo(const GoldenExample& example, std::ostream* out)
{
    *out << example.file;
}

/// The sixteen examples: the inputs of kind "example" of the goldens' manifest, in its order
/// (that of the file names). Read once per test process; empty when the manifest cannot be
/// read, which GoldenExamples.AreTheSixteenExamplesOfTheManifest reports.
[[nodiscard]] const std::vector<GoldenExample>& goldenExamples();

/// The example whose file in data/examples, or whose golden input, is named @p name; null when
/// there is none.
[[nodiscard]] const GoldenExample* findGoldenExample(std::string_view name);

/// The name of the test of an example, for INSTANTIATE_TEST_SUITE_P: its golden input's without
/// the "example-" in front and with '_' for '-' ("a_simple_model_rocket").
[[nodiscard]] std::string goldenExampleTestName(
    const ::testing::TestParamInfo<GoldenExample>& info);

/// The environment the examples are loaded in (see the top of this file), with the six
/// presets of the examples or with none. One per choice and test process; it lives until the
/// process ends. It is handed out as a constant: every load of the process shares its
/// preference store, so a test reads the store and does not write to it (a test that needs
/// other preferences makes a DesignFileEnvironment of its own).
[[nodiscard]] const DesignFileEnvironment& goldenExampleEnvironment(
    DesignFileEnvironment::Presets presets = DesignFileEnvironment::Presets::EXAMPLES);

/// Loads @p example from @p source in the environment of @p presets: a new document in the
/// state as loaded, with the warnings of the load, or the Error of the loader. @p options are
/// those of the loader (GeneralRocketLoader::Options): for a test that wants to hear the events
/// of the load.
[[nodiscard]] Result<LoadedDocument> loadGoldenExample(
    const GoldenExample& example, ExampleSource source = ExampleSource::ORIGINAL,
    DesignFileEnvironment::Presets presets = DesignFileEnvironment::Presets::EXAMPLES,
    GeneralRocketLoader::Options   options = {});

/// Brings the design of @p loaded into the settled state the goldens describe
/// (settleAutomaticDimensions() of AutomaticDimensions.h): the number of passes that changed
/// something, or the Error of a design that does not settle.
[[nodiscard]] Result<int> settleGoldenExample(const LoadedDocument& loaded);

/// loadGoldenExample() and then settleGoldenExample(): the document in the settled state.
[[nodiscard]] Result<LoadedDocument> loadSettledGoldenExample(
    const GoldenExample& example, ExampleSource source = ExampleSource::ORIGINAL,
    DesignFileEnvironment::Presets presets = DesignFileEnvironment::Presets::EXAMPLES);

}  // namespace QtRocket::Test
