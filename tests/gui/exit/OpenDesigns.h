#pragma once

// The designs of the exit suite OpenDesignsExitGolden (open_designs_exit_tests.cpp, plan section
// 9.5): OpenRocket's sixteen example designs (data/examples), the 49 re-saves of its golden
// harness (tests/data/goldens/*/resave/rocket.ork: the sixteen examples, the thirteen test
// rockets, the eighteen legacy files and the two saver-coverage documents) and the design
// OpenRocket's GUI built from scratch (tests/data/goldens/gui/from-scratch/rocket.ork), in the
// order of the plan, with the environment each is loaded in: the bundled motor database, but for
// the test rockets' re-saves, whose motors are TestRockets' seven synthetic ones
// (resave_golden_tests.cpp), and the six component presets of the example designs. The
// open-designs family of tools/openrocket-gui-pins lists the same designs (designs.py: name, kind
// and file), and its families' probes load each as the suite does: the pins of each design are
// found by its name. Shared by the suite and the tests that compare a design of the list with
// pins made by such a probe without a window (loadOpenDesign()).

#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"

namespace QtRocket::Gui::Test
{

/// Where a design of the suite comes from.
enum class OpenDesignSource
{
    EXAMPLE,         ///< a file of data/examples (an archive)
    EXAMPLE_RESAVE,  ///< OpenRocket's re-save of an example
    TEST_ROCKET,     ///< OpenRocket's re-save of a rocket of TestRockets.java
    LEGACY,          ///< OpenRocket's re-save of a design file of tests/data/ork
    COVERAGE,        ///< OpenRocket's re-save of a saver-coverage document of the harness
    GUI_REFERENCE,   ///< the design OpenRocket's GUI built from scratch
};

/// The kind designs.py gives a design of @p source: "example", "example-resave", "testrocket",
/// "legacy", "coverage" or "gui" (the pins' kind of the design).
[[nodiscard]] std::string_view openDesignKind(OpenDesignSource source) noexcept;

/// One design the suite opens.
struct OpenDesign
{
    /// The name of its tests ("example_a_simple_model_rocket", "legacy_v1_6_apocd"), designs.py's
    /// name of it.
    std::string      name;
    OpenDesignSource source{OpenDesignSource::EXAMPLE};
    /// The file File > Open is given.
    std::filesystem::path file;
    /// The golden input whose geometry.json, mass.json and aero.json describe the design; empty
    /// for a design without goldens (the legacy and coverage re-saves, the GUI reference).
    std::string goldenInput;
};

/// What GoogleTest prints for the design of a test (its name, which ctest's test names show too:
/// gtest_discover_tests() puts the printed parameter in place of the parameter's name).
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const OpenDesign& design, std::ostream* out);

/// The 66 designs, in the order of the plan: the examples, then the re-saves of the manifest
/// (examples, test rockets, legacy files, coverage documents), then the GUI reference. What went
/// wrong reading the manifest goes to @p problems.
[[nodiscard]] std::vector<OpenDesign> makeOpenDesigns(std::string& problems);

/// The designs and what went wrong making the list (makeOpenDesigns()).
struct OpenDesignList
{
    std::string             problems;
    std::vector<OpenDesign> designs;
};

/// The list, made once per test process.
[[nodiscard]] const OpenDesignList& openDesignList();

/// The designs of openDesignList().
[[nodiscard]] const std::vector<OpenDesign>& openDesigns();

/// The design of openDesigns() named @p name, or null.
[[nodiscard]] const OpenDesign* findOpenDesign(std::string_view name);

/// The name of the tests of a design, for INSTANTIATE_TEST_SUITE_P: the design's name.
[[nodiscard]] std::string openDesignTestName(const ::testing::TestParamInfo<OpenDesign>& info);

/// The motors @p design is loaded against: TestRockets' seven (QtRocket::Test::
/// testRocketMotors()) for a test rocket's re-save, else the bundled motor database (both read
/// once per test process).
[[nodiscard]] const ThrustCurveMotorSetDatabase& openDesignMotors(const OpenDesign& design);

/// A design loaded as the suite's windows load it, without a window: the environment of its
/// load (declared first, so that it outlives the document, whose simulations keep its preference
/// store) and the document as loaded.
struct LoadedOpenDesign
{
    std::unique_ptr<QtRocket::Test::DesignFileEnvironment> environment;
    std::unique_ptr<OpenRocketDocument>                    document;
};

/// Loads @p design's file in an environment of its own, as the suite's windows have it: a
/// DesignFileEnvironment with the six component presets of the example designs over
/// openDesignMotors(); the Error of the loader when the file cannot be loaded.
[[nodiscard]] Result<LoadedOpenDesign> loadOpenDesign(const OpenDesign& design);

}  // namespace QtRocket::Gui::Test
