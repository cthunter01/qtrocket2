#pragma once

// Comparing warnings with the golden data (tools/openrocket-goldens: Values.warning()), and
// reporting the fields of a golden object that nothing compares. Test-only; the aerodynamic and
// the simulation golden tests share it.

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <nlohmann/json_fwd.hpp>

#include "goldens/GoldenMismatches.h"

namespace QtRocket
{
class Rocket;
class Warning;
class WarningSet;
}  // namespace QtRocket

namespace QtRocket::Test
{

/// Reports every key of the golden object @p object that is none of @p compared: a field a
/// later version of the dumper adds must not go uncompared without notice. @p what names the
/// object in the report ("" for the object the report is about).
void noteUncomparedKeys(GoldenMismatches& m, std::string_view what, const nlohmann::json& object,
                        std::span<const std::string_view> compared);

/// The golden "parameter" of @p warning (Values.warning(): the result of getAOA() or
/// getSpeed()): the angle of a LargeAOA, the speed of a RecoveryHighSpeedDeployment, a
/// HighSpeedMainDeployment, a LowSpeedMainDeployment or a LowSpeedDrogueDeployment; nullopt for a
/// warning without a parameter. (No aerodynamic calculator raises a warning with a parameter:
/// OpenRocket's simulation adds them.)
[[nodiscard]] std::optional<double> parameterOf(const Warning& warning);

/// The golden paths of the sources of @p warning, separated by spaces; a source that is not in
/// @p rocket is "?".
[[nodiscard]] std::string sourcePaths(const Warning& warning, const Rocket& rocket);

/// Compares what identifies @p actual with the golden warning @p expected: its class, its
/// priority and its sources (as paths in @p rocket). That is the part of a warning that does not
/// print a computed value.
void compareWarningIdentity(GoldenMismatches& m, const std::string& field,
                            const nlohmann::json& expected, const Warning& actual,
                            const Rocket& rocket);

/// Compares @p actual with the golden warning @p expected: its class, priority, description,
/// text, sources (as paths in @p rocket) and parameter. The parameter has to be within
/// @p parameterTolerance of the golden one, relative to the larger magnitude.
void compareWarning(GoldenMismatches& m, const std::string& field, const nlohmann::json& expected,
                    const Warning& actual, const Rocket& rocket,
                    double parameterTolerance = kGoldenRelative);

/// Compares the warnings @p actual with the golden list @p expected: as many, and each one, in
/// order (compareWarning()). Returns the number of golden warnings compared.
[[nodiscard]] int compareWarnings(GoldenMismatches& m, std::string_view field,
                                  const nlohmann::json& expected, const WarningSet& actual,
                                  const Rocket& rocket,
                                  double        parameterTolerance = kGoldenRelative);

}  // namespace QtRocket::Test
