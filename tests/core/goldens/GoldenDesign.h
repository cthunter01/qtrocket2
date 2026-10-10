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
// (mass.json) and aero_golden_tests.cpp (aero.json). A design that was loaded from a file is
// compared with all three at once by compareLoadedDesign(), which also holds what only a loaded
// design has in common with its goldens: the ids of its components and the warnings of its
// load (example_design_golden_tests.cpp: OpenRocket's example designs; resave_golden_tests.cpp:
// the test rockets, loaded from OpenRocket's re-saves of them).
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
#include <string_view>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "goldens/GoldenMismatches.h"

namespace QtRocket::Test
{

// ============================================================================= geometry.json

/// What a comparison of a rocket's components, or of its configurations, found.
struct GeometryComparison
{
    int         compared{0};  ///< the components (configurations, ids) compared
    std::string report;       ///< the mismatches, empty when everything matched
    /// compareComponents(): the entries of the "details" of the components that were compared.
    int details{0};
    /// compareConfigurations(): the motors compared, the components of the instance maps (the
    /// entries of "instances") whose instances were compared, and those instances.
    int motors{0};
    int instanceEntries{0};
    int instances{0};
};

/// Compares every component of @p rocket with its golden entry in @p geometry: its class, name,
/// placement, mass properties, overrides, bounds and instances, and every entry of its
/// "details" (the getters of its Java class: an entry that no comparison reads is reported, and
/// so is a key of the entry or of its "overrides" that nothing compares). Not compared: the
/// "id", which a rebuilt rocket draws at random (compareComponentIds() holds it for a loaded
/// one).
[[nodiscard]] GeometryComparison compareComponents(const Rocket&         rocket,
                                                   const nlohmann::json& geometry);

/// Compares the components of @p rocket, in the order of its tree (a component before its
/// children), with the golden list of @p geometry, in its order: as many, each at the path of
/// its golden entry and with its id. For a rocket that has the ids of the golden data: one
/// loaded from the file the data was dumped from, or from OpenRocket's re-save of it. `compared`
/// is the number of components whose path and id were compared.
[[nodiscard]] GeometryComparison compareComponentIds(const Rocket&         rocket,
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
/// What stays unchecked is which of several equally bad directions is found: a strict comparison
/// of the two thetas cannot pass (it differed for 251 of the 329 worst CPs of the test rockets
/// on Linux with glibc), and none is kept.
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

// =========================================================================== a loaded design

/// The three design documents of a golden input.
struct DesignGoldens
{
    nlohmann::json geometry;
    nlohmann::json mass;
    nlohmann::json aero;
};

/// Reads geometry.json, mass.json and aero.json of the golden input @p input, found through
/// manifest.json: ErrorCode::NOT_FOUND when the manifest has no such input, else what
/// loadGoldenJson() fails with.
[[nodiscard]] Result<DesignGoldens> loadDesignGoldens(std::string_view input);

/// How much of the design documents of an input there is to compare, or was compared.
struct DesignCounts
{
    int        components{0};          ///< the components of geometry.json
    int        ids{0};                 ///< their ids
    int        details{0};             ///< the entries of their "details"
    int        configurations{0};      ///< the configurations of geometry.json
    int        motors{0};              ///< the motors of those configurations
    int        instanceEntries{0};     ///< the components of their instance maps
    int        instances{0};           ///< the instances of those components
    int        loadWarnings{0};        ///< the warnings of the load
    int        massConfigurations{0};  ///< the configurations of mass.json
    AeroCounts aero;                   ///< what aero.json holds

    [[nodiscard]] bool operator==(const DesignCounts&) const = default;

    DesignCounts& operator+=(const DesignCounts& other)
    {
        components += other.components;
        ids += other.ids;
        details += other.details;
        configurations += other.configurations;
        motors += other.motors;
        instanceEntries += other.instanceEntries;
        instances += other.instances;
        loadWarnings += other.loadWarnings;
        massConfigurations += other.massConfigurations;
        aero += other.aero;
        return *this;
    }
};

/// "327 components, 327 ids, ...", for the messages of the tests.
[[nodiscard]] std::string toText(const DesignCounts& counts);

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const DesignCounts& counts);

/// What the documents @p goldens hold. A comparison that compared less than this skipped
/// something.
[[nodiscard]] DesignCounts goldenCounts(const DesignGoldens& goldens);

/// What the comparison of a loaded design with its design documents compared and found.
struct DesignComparison
{
    DesignCounts compared;
    std::string  report;  ///< the mismatches, empty when everything matched
};

/// Compares a design that was loaded from a file with the geometry.json document @p geometry of
/// the golden input that was dumped from that design (from the file itself or from OpenRocket's
/// re-save of it): the name of the rocket, the selected configuration, the warnings of the load
/// (@p loadWarnings, each by its class, priority, description, text and sources, in order), the
/// ids of the components in the order of the tree (compareComponentIds()), the components
/// (compareComponents()) and the configurations with their ids (compareConfigurations()); a key
/// of the file that nothing compares is reported. Of the counts, those of geometry.json are
/// filled. The rocket, the preferences and the default units: as for compareLoadedDesign().
[[nodiscard]] DesignComparison compareLoadedGeometry(Rocket& rocket, const WarningSet& loadWarnings,
                                                     const nlohmann::json& geometry,
                                                     const Preferences&    preferences);

/// Compares a design that was loaded from a file with the documents @p goldens of the golden
/// input that was dumped from that design, in the order in which the harness dumps them:
/// - geometry.json: compareLoadedGeometry();
/// - mass.json: the locations of the components and the mass calculations of every
///   configuration (expectGoldenLocations() and expectGoldenMass(); what they report through
///   GoogleTest is taken into the report here, a failure per entry under "mass.json:", and
///   fails no test by itself);
/// - aero.json: compareAero().
/// The rocket has to be in the state the goldens describe: its automatic dimensions settled
/// (settleAutomaticDimensions() of AutomaticDimensions.h) and nothing asked of it before.
/// @p preferences are those the names of the configurations and of the motors are made with
/// (the examples: a store with setMotorNameColumn(false)); the default units are those of
/// OpenRocket while the comparison runs.
[[nodiscard]] DesignComparison compareLoadedDesign(Rocket& rocket, const WarningSet& loadWarnings,
                                                   const DesignGoldens& goldens,
                                                   const Preferences&   preferences);

}  // namespace QtRocket::Test
