#pragma once

// Comparing a rocket with the golden data of its design (tests/data/goldens/<input>/geometry.json,
// mass.json and aero.json, written by tools/openrocket-goldens; the formats are in that tool's
// README.md): its components and flight configurations, its mass calculations and its
// aerodynamics. Test-only.
//
// Every comparison takes the rocket and the parsed golden document, so that a rocket built by a
// maker of tests/core/rocket/TestRockets.h and one loaded from a file are compared alike. The
// golden tests of the test rockets use them and say, in their header comments, what is compared
// and why: test_rockets_golden_tests.cpp (geometry.json), tests/core/mass/MassCalculatorTests.cpp
// (mass.json) and aero_golden_tests.cpp (aero.json).
//
// Two things about the rocket are the caller's to say:
// - randomConfigurationId: whether the maker of the rocket draws the id of its flight
//   configuration at random (TestRocketMaker::randomConfigurationId); such an id is not compared.
//   False for a rocket that has the ids of the golden data (one loaded from the file the data
//   was dumped from).
// - preferences: the store the names of the flight configurations and of the motors are made
//   with (FlightConfiguration::getName(), MotorConfiguration::toMotorName()). The golden names of
//   the test rockets are those of an empty store (InMemoryPreferences).
//
// The comparisons of geometry.json and aero.json collect their mismatches in a report
// (GoldenMismatches; tolerances of plan section 6.4: geometry, mass and aerodynamic coefficients
// relative 1e-9, positions, CGs and CPs absolute 1e-9 m); those of mass.json report through
// GoogleTest's EXPECT macros (relative 1e-9; locations absolute 1e-12 m).

#include <ostream>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"
#include "goldens/GoldenMismatches.h"

namespace QtRocket::Test
{

// ============================================================================= geometry.json

/// What a comparison of a rocket's components, or of its configurations, found.
struct GeometryComparison
{
    int         compared{0};  ///< the components (configurations) compared
    std::string report;       ///< the mismatches, empty when everything matched
};

/// Compares every component of @p rocket with its golden entry in @p geometry.
[[nodiscard]] GeometryComparison compareComponents(const Rocket&         rocket,
                                                   const nlohmann::json& geometry);

/// The number of components of @p rocket, itself included.
[[nodiscard]] int componentCount(const Rocket& rocket);

/// Compares every flight configuration of @p rocket with its golden entry in @p geometry, each
/// one selected while it is compared (as the golden harness dumps it). The id of a configuration
/// is compared unless it is one the maker of the rocket draws at random (@p randomConfigurationId).
/// @p preferences are those the names of the configurations and of the motors are made with (the
/// default name of a configuration, a motor by its designation or by its common name).
[[nodiscard]] GeometryComparison compareConfigurations(Rocket&               rocket,
                                                       const nlohmann::json& geometry,
                                                       bool                  randomConfigurationId,
                                                       const Preferences&    preferences);

/// The index of the selected configuration of @p rocket among its configurations, the default
/// first (the "index" of the golden configurations).
[[nodiscard]] int selectedIndex(const Rocket& rocket);

/// The index of the configuration OpenRocket's maker leaves selected, from @p geometry; -2 when
/// its id is none of the golden configurations'.
[[nodiscard]] int goldenSelectedIndex(const nlohmann::json& geometry);

/// The number of entries of the list @p key of @p geometry.
[[nodiscard]] int goldenCount(const nlohmann::json& geometry, const char* key);

// ================================================================================= mass.json

/// Expects every component of @p rocket at the absolute locations its OpenRocket counterpart
/// has in @p geometry (the geometry.json document of the input: the locations the mass
/// calculations rest on).
void expectGoldenLocations(Rocket& rocket, const nlohmann::json& geometry);

/// Expects the mass calculations of every flight configuration of @p rocket to give the golden
/// values of @p mass: the header (the default flag, the name and the id), the four rigid bodies
/// and the CM analysis rows. Each configuration is selected while it is calculated, as the golden
/// harness does. The golden configurations are the rocket's, in order (the default first): each
/// `index` is its place in the list, so none is compared twice. @p randomConfigurationId: whether
/// the maker of the rocket draws the id of its configuration at random (it is then not compared);
/// @p preferences: those the names of the configurations are made with. Returns the number of
/// configurations compared.
[[nodiscard]] int expectGoldenMass(Rocket& rocket, const nlohmann::json& mass,
                                   bool randomConfigurationId, const Preferences& preferences);

// ================================================================================= aero.json

/// How much of an aero.json document there is to compare, or was compared.
struct AeroCounts
{
    int configurations{0};  ///< the configurations, those with "sameResultsAs" included
    int points{0};          ///< the points of every configuration
    int components{0};      ///< the entries of the force analyses of every point
    int worstCPs{0};        ///< the worst CPs of every configuration
    int warnings{0};        ///< the geometry warnings and the warnings of every point

    [[nodiscard]] bool operator==(const AeroCounts&) const = default;

    AeroCounts& operator+=(const AeroCounts& other)
    {
        configurations += other.configurations;
        points += other.points;
        components += other.components;
        worstCPs += other.worstCPs;
        warnings += other.warnings;
        return *this;
    }
};

/// "47 configurations, 1175 points, ...", for the messages of the tests.
[[nodiscard]] std::string toText(const AeroCounts& counts);

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const AeroCounts& counts);

/// What the comparison of a rocket with an aero.json document compared and found.
struct AeroComparison
{
    AeroCounts  compared;
    std::string report;  ///< the mismatches, empty when everything matched
    /// The worst CPs whose theta is not the golden one but an equally bad direction, one line
    /// each (see compareWorstTheta()); they are not mismatches.
    std::string otherThetas;
};

/// What the document @p aero holds, every configuration counted with the results it has or
/// refers to. A configuration without results counts as nothing, so that the comparison, which
/// reports it, cannot match the count by skipping it.
[[nodiscard]] AeroCounts goldenCounts(const nlohmann::json& aero);

/// One configuration under comparison: its rocket, the configuration, selected, and the
/// calculator that is its own.
struct AeroSubject
{
    const Rocket*              rocket;
    const FlightConfiguration* config;
    BarrowmanCalculator*       calculator;
};

/// The theta put into the conditions before getWorstCP(): none of the 360 directions it tries.
/// getWorstCP() works on a copy of the conditions and overwrites their theta with the one it
/// found (OpenRocket's too: the probe VerifyTheta.java gives the same worst CP and theta whatever
/// theta the conditions come with), so a theta that it did not set is seen. The dumper's
/// conditions come with 0, which is a direction of the 360 and the answer for most rockets.
inline constexpr double kThetaBeforeWorstCp = 1.0;

/// Whether @p theta is one of the 360 directions getWorstCP() tries: 2 pi i / 360 for an i of 0
/// to 359, the library's own expression, so exactly that double. False for NaN.
[[nodiscard]] bool isWorstCpDirection(double theta);

/// Compares the theta getWorstCP() left in the conditions, @p theta, with that of the golden
/// worst CP @p expected ("mach", "cp" and "theta"); @p worst is the worst CP found, which the
/// caller compares with the golden one. Returns whether the two thetas are the same direction; a
/// problem is one line in @p m.
///
/// getWorstCP() takes the first of 360 directions whose CP is strictly ahead of every earlier
/// one. Where the CP depends on the direction, that is a property of the rocket and the thetas
/// are equal. Where it does not (three or four equal fins: the same CP in every direction, up to
/// the rounding of the sum over the fins), the direction found is the one where the rounding
/// happens to give the smallest x. No port reproduces that: OpenRocket sums the components in
/// the order of their random ids, so it finds another direction itself with other ids, and the
/// last bit of a sine differs between math libraries. A theta that is not the golden one is
/// therefore checked for what can be checked, on both sides:
/// - the theta found is a direction getWorstCP() tries, and the CP at it is the worst CP found,
///   so, with the comparison of the two worst CPs, the golden worst CP (foundThetaProblem()): a
///   stale theta, one off the grid, NaN, or a direction with another CP is reported;
/// - the golden theta is an equally bad direction for QtRocket: the CP at it is within the CP
///   tolerance of the worst CP found.
/// What stays unchecked is which of several equally bad directions is found;
/// AeroGoldenWorstTheta holds that strict comparison, disabled.
[[nodiscard]] bool compareWorstTheta(GoldenMismatches& m, const AeroSubject& subject,
                                     const nlohmann::json& expected, double theta,
                                     const Coordinate& worst);

/// Compares the results of the configuration @p config of @p rocket, which is selected, with the
/// golden @p results, making the dumper's calls in the dumper's order on a calculator of its
/// own. @p context names the configuration in the report.
[[nodiscard]] AeroComparison compareResults(const nlohmann::json& results, const Rocket& rocket,
                                            const FlightConfiguration& config,
                                            const std::string&         context);

/// Compares @p rocket with the aero.json document @p aero: the file's values, then every
/// configuration, each one selected while it is calculated (as the dumper selects it) and the
/// original selection restored afterwards. @p randomConfigurationId: whether the maker of the
/// rocket draws the id of its configuration at random; @p preferences: those the names of the
/// configurations are made with.
[[nodiscard]] AeroComparison compareAero(Rocket& rocket, const nlohmann::json& aero,
                                         bool               randomConfigurationId,
                                         const Preferences& preferences);

}  // namespace QtRocket::Test
