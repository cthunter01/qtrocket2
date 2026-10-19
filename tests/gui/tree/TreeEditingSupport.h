#pragma once

// What the tests of the component tree's editing share (the drops of ComponentDropTests.cpp, the
// actions of main/ComponentEditingTests.cpp): the texts of the Java probes' transcripts
// (tree/ProbeDesign.h), a rocket filled up to a bound, and a design carried into another one with
// its decal images, saved and loaded again (the M2 scout's decal probe).

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace QtRocket
{
class OpenRocketDocument;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// The name of the test of a scenario @p test of a table of scenarios with a member name, for
/// INSTANTIATE_TEST_SUITE_P. (A function: a lambda's parameter there would shadow the macro's own,
/// which GCC's -Wshadow reports.)
template <class Scenario>
[[nodiscard]] std::string scenarioName(const ::testing::TestParamInfo<Scenario>& test)
{
    return std::string(test.param.name);
}

/// Java's text of @p value ("true", "false").
[[nodiscard]] std::string javaBool(bool value);

/// The probes' "undo='<description>'" of @p document ("null" for none).
[[nodiscard]] std::string undoText(const OpenRocketDocument& document);

/// The probes' "redo='<description>'" of @p document ("null" for none).
[[nodiscard]] std::string redoText(const OpenRocketDocument& document);

/// The lines of @p text, an expected transcript.
[[nodiscard]] std::vector<std::string> linesOf(std::string_view text);

/// Where the transcript @p actual differs from @p expected, a text for each line that differs
/// ("line <n>: <actual> | expected: <expected>", "(none)" for a missing line): empty when they
/// are the same (a failure then names the lines instead of printing both transcripts whole).
[[nodiscard]] std::vector<std::string> lineDifferences(const std::vector<std::string>& actual,
                                                       const std::vector<std::string>& expected);

/// Adds mass components to @p parent, a component of @p rocket, until the rocket has @p count
/// components (DocumentConfig::componentCount()), within one freeze.
void fillUpTo(Rocket& rocket, RocketComponent& parent, std::size_t count);

/// The names of @p document's decal list, in its order.
[[nodiscard]] std::vector<std::string> decalListOf(const OpenRocketDocument& document);

/// The first body component of @p document's rocket, in tree order, whose appearance has a decal
/// (the decal probe's choice: in OpenRocket's example "Parallel booster staging" the body tube
/// with the booster set, the fins and the recovery); null when there is none.
[[nodiscard]] const RocketComponent* firstBodyComponentWithDecal(
    const OpenRocketDocument& document);

/// The entries of the .ork archive @p bytes as the decal probe prints them: "rocket.ork", and
/// "<name> <size> bytes" for each other entry.
[[nodiscard]] std::vector<std::string> archiveEntries(const std::vector<std::byte>& bytes);

/// The entries of OpenRocket's archive of the decal probe (the M2 scout's DecalProbe and
/// DecalTransferProbe of Milestone 2 run 1, "== probe", "saved:"): the six images of the
/// example's body tube.
[[nodiscard]] std::vector<std::string> decalProbeArchiveEntries();

/// @p document saved as an .ork archive, in the environment of the golden examples; a test
/// failure when the save fails, warns or misses an image.
[[nodiscard]] std::vector<std::byte> savedWithoutWarnings(OpenRocketDocument& document);

}  // namespace QtRocket::Gui::Test
