#pragma once

// Every typed accessor of the core's Preferences, run on a store and written down, so that two
// stores can be compared call for call: the same calls on SettingsPreferences and on
// InMemoryPreferences must give the same transcript.

#include <string>
#include <vector>

namespace QtRocket
{
class Preferences;
}

namespace QtRocket::Gui::Test
{

/// What every getter of Preferences answers on @p preferences, one line "name = value" each:
/// the ORPreferences forms on a few keys, the per-class lookups along three component classes
/// and every named getter of the class, in its order, with the default units after
/// loadDefaultUnits() too (which changes the process-wide default units: guard them). Reads
/// only: getLaunchRodDirection(), which may store, is in writeEveryPreference().
[[nodiscard]] std::vector<std::string> readEveryPreference(const Preferences& preferences);

/// Calls every setter of Preferences on @p preferences with a value unlike its default (and a
/// few values each setter treats specially: out of range, NaN, a negative wind), and
/// getLaunchRodDirection() and storeDefaultUnits() (of the default units as they are); one line
/// per call with the number of changed() emissions so far.
[[nodiscard]] std::vector<std::string> writeEveryPreference(Preferences& preferences);

}  // namespace QtRocket::Gui::Test
