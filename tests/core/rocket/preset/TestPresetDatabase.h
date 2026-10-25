#pragma once

// The component preset databases of the tests that need more presets than the six of the example
// designs (ExamplePresets.h): OpenRocket's bundled presets, and a small database for the tests that
// list or show presets. Test-only.

#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"

namespace QtRocket::Test
{

/// The component presets QtRocket ships (data/components, 31 .orc files), loaded as the
/// application loads them and as OpenRocket's ExampleFilesTest and the golden harness do:
/// ComponentPresetDatabaseLoader::loadPresetComponents() with OpenRocket's built-in materials
/// (addBuiltinMaterials()). 5231 presets read, 5228 kept, in the order of the golden
/// presets/presets.json (preset_golden_tests.cpp compares them). A load that gives a problem or a
/// warning is a test failure (ADD_FAILURE()).
///
/// It is read once per test process (about one and a half seconds in a debug build, several
/// seconds under the sanitizers) and shared: never load it per test, never change it, and keep
/// tests that need it few (ctest runs every test in a process of its own). A copy, which shares
/// the presets, may be changed.
[[nodiscard]] const ComponentPresetDatabase& bundledPresetDatabase();

/// A small database for the tests that list, filter, sort or show presets, where the bundled
/// one's 5228 presets (1496 body tubes) would be slow: the presets of the fixtures
/// tests/data/presets/every-kind.orc (one of each of the eleven kinds) and legacy.orc (a legacy
/// streamer and bulkhead of Quest), then of the bundled database/estes_ps2.orc (30 parts of Estes'
/// PS II kits), loaded in that order with ComponentPresetDatabaseLoader::loadUserComponents() and
/// the built-in materials, so held in Database.add()'s order (manufacturer, then part number).
/// 43 presets of 4 manufacturers (Estes, Quest, Rail-Buttons.com, Top Flight Recovery) and every
/// type: 12 body tubes, 6 nose cones, 8 centering rings, 4 tube couplers, 3 parachutes, 2
/// transitions, 2 engine blocks, 2 bulkheads, 2 streamers, 1 launch lug and 1 rail button; the
/// two of legacy.orc are the only legacy presets. (test_preset_database_tests.cpp holds these
/// numbers.)
///
/// Loaded once per test process (a few milliseconds) and handed out as a new database each time,
/// which shares the presets with the others: a test may change its database.
[[nodiscard]] ComponentPresetDatabase smallPresetDatabase();

}  // namespace QtRocket::Test
