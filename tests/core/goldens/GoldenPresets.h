#pragma once

// The preset golden, tests/data/goldens/presets/presets.json (tools/openrocket-goldens,
// PresetDumper; the README's "presets/presets.json"): every component preset OpenRocket's
// OpenRocketComponentLoader reads from the bundled .orc files, file by file in the order of the
// bundled directory, with every property in the order the loader put it, and the order in which
// OpenRocket's ComponentPresetDatabase holds them once every file was added. Test-only.

#include <string>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/util/Error.h"

namespace QtRocket
{
class ComponentPreset;
}

namespace QtRocket::Test
{

/// The relative tolerance of the densities of hasTranscendentalDensity().
inline constexpr double kTranscendentalDensityTolerance = 1e-10;

/// presets.json, parsed with the members of its objects in the order of the file (a preset's
/// properties in OpenRocket's order). Read once per test process; the Error of a file that cannot
/// be read or parsed. Its layout:
///   {"schema": "presets", "schemaVersion", "generator", "fileCount", "presetCount",
///    "files": [{"file": the path below data/components ("database/BMS.ORC"), "sha256",
///               "presetCount", "presets": [{"type", "manufacturer" (the simple name),
///               "manufacturerDisplayName", "partNo", "digest", "properties": {name: value}}]}],
///    "databaseCount", "databaseOrder": ["<file>#<index in the file>", ...]}
/// A property's value is a number (a double as Double.toString writes it, NaN and the infinities
/// as the strings "NaN", "Infinity" and "-Infinity"; an integer as such), a boolean, a string,
/// the simple name of a manufacturer, the constant's name of a preset type, a shape or a finish,
/// a material {"type", "name", "density", "shearModulus", "group" (its database string),
/// "userDefined", "documentMaterial"} or an image in base64.
[[nodiscard]] const Result<nlohmann::ordered_json>& goldenPresets();

/// Whether @p preset is one of the six nose cones of the bundled presets whose material density
/// OpenRocket's ComponentPresetFactory derives from the volume of a HAACK or a POWER shape, which
/// goes through acos, sin and pow, whose last bits differ between the libms of the platforms: a
/// NOSE_CONE preset with such a Shape and a Mass. Their density is compared within
/// kTranscendentalDensityTolerance and their digest, which the density goes into, not at all.
/// Every other value of the bundled presets comes from + - * /, sqrt and the files' texts.
[[nodiscard]] bool hasTranscendentalDensity(const ComponentPreset& preset);

/// What differs between @p preset and its golden entry @p golden (an element of the "presets" of
/// a file): one line per difference, each ending in '\n'; empty when nothing does. Exact: the
/// type, the manufacturer's simple and display names, the part number, the digest, and the
/// properties, the same keys in the same order with the same values (doubles bit for bit,
/// materials field by field, an image byte for byte). For a preset of hasTranscendentalDensity()
/// the material's density is compared within the tolerance and the digest not at all.
[[nodiscard]] std::string goldenPresetMismatches(const ComponentPreset&        preset,
                                                 const nlohmann::ordered_json& golden);

}  // namespace QtRocket::Test
