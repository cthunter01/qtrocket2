#pragma once

// A preset written as the probe tools/openrocket-probes/orc-loader writes the presets OpenRocket's
// OpenRocketComponentLoader makes (OrcEdgeProbe.run()), so that its transcript (expected.txt) can
// be pinned line for line.

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace QtRocket
{
class ComponentPreset;
}

namespace QtRocket::Test
{

/// @p text with its backslashes, tabs, line feeds and carriage returns escaped as in Java source,
/// as the probe writes a string, a name and the message of an exception (its escape()).
[[nodiscard]] std::string orcProbeEscape(std::string_view text);

/// The line the probe prints for @p preset, without its two leading spaces: every property in the
/// order of the preset's map as "<key>=<value>; ", then "digest=<digest>". A value is written as
/// Java's String.valueOf() writes it (a double by Double.toString() of JDK 17, a manufacturer by
/// its display name, a shape by its English name, a preset type by its name), but for a material,
/// "[<type> '<name>' <density> shear <shear modulus> <group's database string> user|builtin]", and
/// an image, "bytes<count>:<hexadecimal bytes>"; strings and names have their backslashes, tabs,
/// line feeds and carriage returns escaped as in Java source.
[[nodiscard]] std::string orcProbeLine(const ComponentPreset& preset);

/// orcProbeLine() of every preset of @p presets, in order.
[[nodiscard]] std::vector<std::string> orcProbeLines(
    std::span<const std::shared_ptr<const ComponentPreset>> presets);

}  // namespace QtRocket::Test
