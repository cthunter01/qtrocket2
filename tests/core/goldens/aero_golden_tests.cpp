// Aerodynamic golden tests: the thirteen rockets of tests/core/rocket/TestRockets.h against what
// OpenRocket's BarrowmanCalculator computes for the Java rockets, in
// tests/data/goldens/testrocket-<name>/aero.json (tools/openrocket-goldens, AeroDumper.java; the
// format is in that tool's README.md).
//
// The calls are the dumper's, in the dumper's order, because the calculator caches across
// calls. Per configuration, selected in the rocket as the dumper selects it: one new
// BarrowmanCalculator; checkGeometry() on the rocket; the structure CG (the pitch centre of point
// 23); per point getCP(), getAerodynamicForces() and getForceAnalysis() with one warning set;
// then getWorstCP() per Mach number, each with new conditions.
//
// Compared (AeroGolden, one test per rocket):
// - per file: the atmosphere of the default flight conditions, the stall angle and the number of
//   configurations (the rocket's, the default one included);
// - per configuration: its header (index, id, name, whether it is the default), the reference
//   length and area, and the results. A configuration whose "sameResultsAs" names an earlier one
//   (it differs only in its motors) is calculated all the same and compared with the results it
//   refers to (aeroResults() of GoldenData.h);
// - the geometry warnings;
// - the 25 points. The flight conditions are set from the golden "conditions" (the reference
//   length and area, the Mach number, the angle of attack, theta, the three rates, the pitch
//   centre and, at point 24, the nozzle exit areas per assembly path), read back, and the derived
//   velocity and beta compared. The inputs are not taken on trust either: the Mach number, the
//   angle of attack, theta and the rates have to be those of the dumper's table of points
//   (AeroDumper.points()), the reference length and area those new FlightConditions take from
//   the configuration (the dumper sets neither), the pitch centre the nose tip or, at point 23,
//   the structure CG, and the nozzle exit areas none or, at point 24, those of the motor mounts,
//   each derived here as the dumper derives it. Then the CP, every field of "forces", every
//   entry of "components" by its path with every field (an entry that only one side has is
//   reported) and the warnings of the point;
// - the worst CP per Mach number (the dumper's Mach numbers): the CP, and the theta found as
//   compareWorstTheta() describes: it is one of the 360 directions getWorstCP() tries, the CP at
//   it is the worst CP (found, and so the golden one), and the golden theta is an equally bad
//   direction for QtRocket.
//   That is the one value of the files that cannot be compared for equality;
//   AeroGoldenWorstTheta holds the strict comparison, disabled, and the tests of the check.
// A warning is compared by its class, priority, description, text, sources (as paths) and
// parameter, and the warnings of a set in their order (compareWarnings() of GoldenWarnings.h,
// which the simulation goldens share). A golden field that nothing compares is reported, in
// every object of the document (noteUncomparedKeys()). The file's "schema", "schemaVersion" and
// "input" are checked by goldens_schema_tests.cpp.
//
// Nothing is skipped silently: the numbers of configurations, points, force-analysis entries,
// worst CPs and warnings compared have to be those of each golden file, and AeroGoldenCoverage
// sums them over the thirteen inputs of the manifest. Nor is the comparison vacuous:
// AeroGoldenMutation changes one golden value at a time in a copy of a document and expects the
// one line that reports it; AeroGoldenWorstTheta and AeroGoldenWarnings do the same for what no
// golden file exercises (a wrong theta from getWorstCP(), a warning with a parameter).
//
// Tolerances (plan section 6.4): coefficients relative 1e-9 (exact for a golden 0, as
// GoldenMismatches compares it), CP positions absolute 1e-9 m. No value needs the plan's wider
// relative 1e-7 for table-interpolated aerodynamics: on Linux the largest difference is below
// 1e-15, relative and absolute.
//
// Not compared in this tier: the sixteen example-* inputs. Their aero.json files are in
// tests/data/goldens, but the designs are .ork files, which need the .ork loader of the file
// tier. That tier adds them here: load the design and hand it to compareAero(). (Their goldens
// describe the design after OpenRocket's automatic dimensions have settled; see "Settled
// automatic dimensions" in the README of tools/openrocket-goldens.)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numbers>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/aero/AerodynamicForces.h"
#include "QtRocket/aero/BarrowmanCalculator.h"
#include "QtRocket/aero/FlightConditions.h"
#include "QtRocket/aero/ForceMap.h"
#include "QtRocket/logging/Message.h"
#include "QtRocket/logging/MessagePriority.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/models/AtmosphericConditions.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentAssembly.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/MathUtil.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenGeometry.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenWarnings.h"
#include "rocket/TestRockets.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using nlohmann::json;
using QtRocket::AerodynamicForces;
using QtRocket::BarrowmanCalculator;
using QtRocket::ComponentAssembly;
using QtRocket::Coordinate;
using QtRocket::FlightConditions;
using QtRocket::FlightConfiguration;
using QtRocket::FlightConfigurationId;
using QtRocket::ForceMap;
using QtRocket::MotorMount;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Warning;
using QtRocket::WarningSet;
using QtRocket::Test::aeroResults;
using QtRocket::Test::compareWarning;
using QtRocket::Test::compareWarnings;
using QtRocket::Test::componentAtGoldenPath;
using QtRocket::Test::goldenCoordinate;
using QtRocket::Test::goldenPathOf;
using QtRocket::Test::goldenValue;
using QtRocket::Test::kGoldenAbsolute;
using QtRocket::Test::noteUncomparedKeys;
using QtRocket::Test::parameterOf;
using QtRocket::Test::TestRocketMaker;
using QtRocket::Test::testRocketMakers;

/// The comparison collector of the golden tests.
using Mismatches = QtRocket::Test::GoldenMismatches;

/// AeroDumper.NOZZLE_EXIT_DIAMETER_FRACTION: the nozzle exit diameter of the nozzle point, as a
/// fraction of the motor mount's diameter.
constexpr double kNozzleExitDiameterFraction = 0.5;

/// The index of the point whose pitch centre is the structure CG.
constexpr std::size_t kStructureCgPoint = 23;

/// The index of the point with thrusting nozzles.
constexpr std::size_t kNozzlePoint = 24;

/// AeroDumper.MACHS: the Mach numbers of the grid of points and of the worst CPs.
constexpr std::array<double, 7> kDumperMachs{0.05, 0.3, 0.6, 0.9, 1.1, 1.5, 2.0};

/// AeroDumper.AOAS_DEG: the angles of attack of the grid of points, in degrees.
constexpr std::array<double, 3> kDumperAoasDegrees{0.0, 2.0, 10.0};

/// The number of points of the grid: index = 3 * Mach index + angle index.
constexpr std::size_t kGridPoints = kDumperMachs.size() * kDumperAoasDegrees.size();

/// The number of points of a configuration: the grid and the four after it.
constexpr std::size_t kDumperPoints = kGridPoints + 4;

/// The keys of the objects of an aero.json document that the comparison reads (compares, sets
/// as an input, follows or, for the first three of the file, leaves to goldens_schema_tests.cpp).
/// Those of a "forces" object are kForceFields and isComparedForceKey().
constexpr std::array<std::string_view, 6> kFileKeys{
    "schema", "schemaVersion", "input", "atmosphere", "stallAngle", "configurations"};
constexpr std::array<std::string_view, 6> kAtmosphereKeys{
    "temperature", "pressure", "relativeHumidity", "machSpeed", "density", "kinematicViscosity"};
constexpr std::array<std::string_view, 10> kConfigurationKeys{"index",
                                                              "id",
                                                              "name",
                                                              "isDefault",
                                                              "referenceLength",
                                                              "referenceArea",
                                                              "sameResultsAs",
                                                              "geometryWarnings",
                                                              "points",
                                                              "worstCP"};
constexpr std::array<std::string_view, 5>  kPointKeys{"conditions", "cp", "forces", "components",
                                                      "warnings"};
constexpr std::array<std::string_view, 12> kConditionKeys{
    "mach",        "aoa",       "theta",   "rollRate", "pitchRate", "yawRate",
    "pitchCenter", "refLength", "refArea", "velocity", "beta",      "thrustingNozzleExitAreas"};
constexpr std::array<std::string_view, 2> kNozzleKeys{"assembly", "area"};
constexpr std::array<std::string_view, 3> kWorstCpKeys{"mach", "cp", "theta"};

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
[[nodiscard]] std::string toText(const AeroCounts& counts)
{
    return std::format(
        "{} configurations, {} points, {} force-analysis entries, {} worst CPs, {} warnings",
        counts.configurations, counts.points, counts.components, counts.worstCPs, counts.warnings);
}

/// toText() of @p counts, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const AeroCounts& counts)
{
    return out << toText(counts);
}

/// What the comparison of a rocket with an aero.json document compared and found.
struct AeroComparison
{
    AeroCounts  compared;
    std::string report;  ///< the mismatches, empty when everything matched
    /// The worst CPs whose theta is not the golden one but an equally bad direction, one line
    /// each (see compareWorstTheta()); they are not mismatches.
    std::string otherThetas;
};

/// What the results @p results of one configuration hold.
[[nodiscard]] AeroCounts goldenResultCounts(const json& results)
{
    AeroCounts counts;
    counts.configurations = 1;
    counts.warnings       = static_cast<int>(results.at("geometryWarnings").size());
    for (const json& point : results.at("points"))
    {
        counts.points++;
        counts.components += static_cast<int>(point.at("components").size());
        counts.warnings += static_cast<int>(point.at("warnings").size());
    }
    counts.worstCPs = static_cast<int>(results.at("worstCP").size());
    return counts;
}

/// What the document @p aero holds, every configuration counted with the results it has or
/// refers to. A configuration without results counts as nothing, so that the comparison, which
/// reports it, cannot match the count by skipping it.
[[nodiscard]] AeroCounts goldenCounts(const json& aero)
{
    AeroCounts counts;
    for (std::size_t i = 0; i < aero.at("configurations").size(); i++)
    {
        if (const json* results = aeroResults(aero, i))
        {
            counts += goldenResultCounts(*results);
        }
    }
    return counts;
}

// ================================================================================ conditions

/// The nozzle exit areas of the golden list @p expected (assembly path and area), for
/// FlightConditions::setThrustingNozzleExitAreas(); an entry whose path is no assembly of
/// @p rocket is reported and left out.
[[nodiscard]] std::vector<FlightConditions::NozzleExitArea> goldenNozzleAreas(Mismatches& m,
                                                                              const json& expected,
                                                                              const Rocket& rocket)
{
    std::vector<FlightConditions::NozzleExitArea> areas;
    for (const json& entry : expected)
    {
        const auto  path = entry.at("assembly").get<std::string>();
        const auto* assembly =
            dynamic_cast<const ComponentAssembly*>(componentAtGoldenPath(rocket, path));
        if (assembly == nullptr)
        {
            m.note(std::format("thrustingNozzleExitAreas: {} is not an assembly", path));
            continue;
        }
        areas.emplace_back(assembly, goldenValue(entry.at("area")));
    }
    return areas;
}

/// The nozzle exit areas the dumper gives the nozzle point (AeroDumper.nozzleExitAreas()): every
/// active motor mount contributes getMotorCount() nozzles of half its motor mount diameter to its
/// assembly, whether or not the configuration gives it a motor. By assembly path, in tree order
/// of the first mount of each assembly.
[[nodiscard]] std::vector<std::pair<std::string, double>> dumperNozzleAreas(
    const Rocket& rocket, const FlightConfiguration& config)
{
    std::vector<std::pair<std::string, double>> areas;
    for (const RocketComponent& component : rocket.subtree())
    {
        const auto* mount = dynamic_cast<const MotorMount*>(&component);
        if (mount == nullptr || !mount->isMotorMount() || !config.isComponentActive(component))
        {
            continue;
        }
        const double radius = kNozzleExitDiameterFraction * mount->getMotorMountDiameter() / 2;
        const double area =
            mount->getMotorCount() * std::numbers::pi * QtRocket::MathUtil::pow2(radius);
        const std::string path = goldenPathOf(component.getAssembly());
        const auto same = std::ranges::find(areas, path, &std::pair<std::string, double>::first);
        if (same == areas.end())
        {
            areas.emplace_back(path, area);
        }
        else
        {
            same->second += area;
        }
    }
    return areas;
}

/// Compares the nozzle exit areas the motor mounts of @p config give (dumperNozzleAreas()) with
/// the golden list @p expected of the nozzle point, which is in tree order of the assemblies.
void compareDumperNozzleAreas(Mismatches& m, const json& expected, const Rocket& rocket,
                              const FlightConfiguration& config)
{
    std::vector<std::pair<std::string, double>> areas = dumperNozzleAreas(rocket, config);
    m.integer("nozzle areas of the motor mounts: number",
              static_cast<std::int64_t>(expected.size()), static_cast<std::int64_t>(areas.size()));
    for (const json& entry : expected)
    {
        const auto path = entry.at("assembly").get<std::string>();
        const auto same = std::ranges::find(areas, path, &std::pair<std::string, double>::first);
        if (same == areas.end())
        {
            m.note(std::format("nozzle areas of the motor mounts: none for {}", path));
            continue;
        }
        m.relative(std::format("nozzle area of the motor mounts of {}", path),
                   goldenValue(entry.at("area")), same->second);
    }
}

/// One row of the dumper's table of points (AeroDumper.Point): the Mach number, the angle of
/// attack in degrees (the dumper converts with Math.toRadians), theta and the three rates.
struct DumperPoint
{
    double mach;
    double aoaDegrees;
    double theta;
    double rollRate;
    double pitchRate;
    double yawRate;
};

/// The conditions the dumper gives point @p index (AeroDumper.points()): the grid of Mach
/// numbers and angles of attack at theta 0 without rotation, then a wind direction off the fin
/// planes (21), rotation about the nose tip (22) and about the structure CG (23), and thrusting
/// nozzles (24). nullopt beyond them.
[[nodiscard]] std::optional<DumperPoint> dumperPoint(std::size_t index)
{
    if (index < kGridPoints)
    {
        return DumperPoint{.mach       = kDumperMachs.at(index / kDumperAoasDegrees.size()),
                           .aoaDegrees = kDumperAoasDegrees.at(index % kDumperAoasDegrees.size()),
                           .theta      = 0.0,
                           .rollRate   = 0.0,
                           .pitchRate  = 0.0,
                           .yawRate    = 0.0};
    }
    switch (index)
    {
        case kGridPoints:
            return DumperPoint{.mach       = 0.3,
                               .aoaDegrees = 5.0,
                               .theta      = std::numbers::pi / 4,
                               .rollRate   = 0.0,
                               .pitchRate  = 0.0,
                               .yawRate    = 0.0};
        case kGridPoints + 1:
        case kStructureCgPoint:
            return DumperPoint{.mach       = 0.8,
                               .aoaDegrees = 2.0,
                               .theta      = 1.0,
                               .rollRate   = 20.0,
                               .pitchRate  = 2.0,
                               .yawRate    = 1.0};
        case kNozzlePoint:
            return DumperPoint{.mach       = 0.6,
                               .aoaDegrees = 0.0,
                               .theta      = 0.0,
                               .rollRate   = 0.0,
                               .pitchRate  = 0.0,
                               .yawRate    = 0.0};
        default:
            return std::nullopt;
    }
}

/// Compares the inputs of the golden "conditions" @p expected of point @p index with the
/// dumper's table: the conditions are set from the file, so a file whose inputs are not the
/// dumper's would have the forces compared at other conditions than the README states. The
/// pitch centre is the nose tip, but for its x at point 23, and there are no thrusting nozzles
/// but at point 24 (compareResults() derives both of those).
void compareWithDumperPoint(Mismatches& m, std::size_t index, const json& expected)
{
    const std::optional<DumperPoint> point = dumperPoint(index);
    if (!point)
    {
        m.note(std::format("point {} is beyond the dumper's {} points", index, kDumperPoints));
        return;
    }
    m.exact("the dumper's mach", point->mach, goldenValue(expected.at("mach")));
    m.absolute("the dumper's aoa", point->aoaDegrees * std::numbers::pi / 180,
               goldenValue(expected.at("aoa")));
    m.absolute("the dumper's theta", point->theta, goldenValue(expected.at("theta")));
    m.exact("the dumper's rollRate", point->rollRate, goldenValue(expected.at("rollRate")));
    m.exact("the dumper's pitchRate", point->pitchRate, goldenValue(expected.at("pitchRate")));
    m.exact("the dumper's yawRate", point->yawRate, goldenValue(expected.at("yawRate")));
    const Coordinate pitchCenter = goldenCoordinate(expected.at("pitchCenter"));
    if (index != kStructureCgPoint)
    {
        m.absolute("the dumper's pitchCenter.x", 0.0, pitchCenter.x);
    }
    m.absolute("the dumper's pitchCenter.y", 0.0, pitchCenter.y);
    m.absolute("the dumper's pitchCenter.z", 0.0, pitchCenter.z);
    if (index != kNozzlePoint)
    {
        m.integer("the dumper's thrustingNozzleExitAreas: number", 0,
                  static_cast<std::int64_t>(expected.at("thrustingNozzleExitAreas").size()));
    }
}

/// The flight conditions of the golden point @p expected (its "conditions") for @p config: new
/// conditions as the dumper makes them, then every value set from the golden one, in the
/// dumper's order (the reference length and area come first: the dumper's conditions take them
/// from the configuration, and so must new conditions here, which is compared before they are
/// set).
[[nodiscard]] FlightConditions goldenConditions(Mismatches& m, const json& expected,
                                                const Rocket&              rocket,
                                                const FlightConfiguration& config)
{
    FlightConditions conditions(config);
    m.relative("refLength of new conditions", goldenValue(expected.at("refLength")),
               conditions.getRefLength());
    m.relative("refArea of new conditions", goldenValue(expected.at("refArea")),
               conditions.getRefArea());
    conditions.setRefLength(goldenValue(expected.at("refLength")));
    conditions.setRefArea(goldenValue(expected.at("refArea")));
    conditions.setMach(goldenValue(expected.at("mach")));
    conditions.setAOA(goldenValue(expected.at("aoa")));
    conditions.setTheta(goldenValue(expected.at("theta")));
    conditions.setRollRate(goldenValue(expected.at("rollRate")));
    conditions.setPitchRate(goldenValue(expected.at("pitchRate")));
    conditions.setYawRate(goldenValue(expected.at("yawRate")));
    conditions.setPitchCenter(goldenCoordinate(expected.at("pitchCenter")));
    conditions.setThrustingNozzleExitAreas(
        goldenNozzleAreas(m, expected.at("thrustingNozzleExitAreas"), rocket));
    return conditions;
}

/// Compares what @p actual answers with the golden "conditions" @p expected: the values that were
/// set come back exactly (a setter that kept a slightly different value would show), the derived
/// velocity and beta within the tolerance.
void compareConditions(Mismatches& m, const json& expected, const FlightConditions& actual,
                       const Rocket& rocket)
{
    m.exact("conditions.mach", goldenValue(expected.at("mach")), actual.getMach());
    m.exact("conditions.aoa", goldenValue(expected.at("aoa")), actual.getAOA());
    m.exact("conditions.theta", goldenValue(expected.at("theta")), actual.getTheta());
    m.exact("conditions.rollRate", goldenValue(expected.at("rollRate")), actual.getRollRate());
    m.exact("conditions.pitchRate", goldenValue(expected.at("pitchRate")), actual.getPitchRate());
    m.exact("conditions.yawRate", goldenValue(expected.at("yawRate")), actual.getYawRate());
    const Coordinate pitchCenter = goldenCoordinate(expected.at("pitchCenter"));
    m.exact("conditions.pitchCenter.x", pitchCenter.x, actual.getPitchCenter().x);
    m.exact("conditions.pitchCenter.y", pitchCenter.y, actual.getPitchCenter().y);
    m.exact("conditions.pitchCenter.z", pitchCenter.z, actual.getPitchCenter().z);
    m.exact("conditions.refLength", goldenValue(expected.at("refLength")), actual.getRefLength());
    m.exact("conditions.refArea", goldenValue(expected.at("refArea")), actual.getRefArea());
    m.relative("conditions.velocity", goldenValue(expected.at("velocity")), actual.getVelocity());
    m.relative("conditions.beta", goldenValue(expected.at("beta")), actual.getBeta());

    const json& nozzles = expected.at("thrustingNozzleExitAreas");
    m.integer("conditions.thrustingNozzleExitAreas: number",
              static_cast<std::int64_t>(nozzles.size()),
              static_cast<std::int64_t>(actual.getThrustingNozzleExitAreas().size()));
    for (const json& entry : nozzles)
    {
        const auto  path = entry.at("assembly").get<std::string>();
        const auto* assembly =
            dynamic_cast<const ComponentAssembly*>(componentAtGoldenPath(rocket, path));
        if (assembly != nullptr)
        {
            m.exact(std::format("conditions.thrustingNozzleExitAreas {}", path),
                    goldenValue(entry.at("area")), actual.getThrustingNozzleExitArea(*assembly));
        }
        noteUncomparedKeys(m, std::format("conditions.thrustingNozzleExitAreas {}", path), entry,
                           kNozzleKeys);
    }
    noteUncomparedKeys(m, "conditions", expected, kConditionKeys);
}

// ==================================================================================== forces

/// One coefficient of the golden "forces" and the getter that answers it.
struct ForceField
{
    std::string_view key;
    double (AerodynamicForces::*getter)() const;
};

/// The coefficients of a golden "forces" object (AeroDumper.forces()), read through the getters,
/// so with the component's CD override applied; "cp" and "axisymmetric" are compared apart.
constexpr std::array<ForceField, 15> kForceFields{{
    {.key = "cn", .getter = &AerodynamicForces::getCN},
    {.key = "cm", .getter = &AerodynamicForces::getCm},
    {.key = "cside", .getter = &AerodynamicForces::getCside},
    {.key = "cyaw", .getter = &AerodynamicForces::getCyaw},
    {.key = "croll", .getter = &AerodynamicForces::getCroll},
    {.key = "crollDamp", .getter = &AerodynamicForces::getCrollDamp},
    {.key = "crollForce", .getter = &AerodynamicForces::getCrollForce},
    {.key = "cd", .getter = &AerodynamicForces::getCD},
    {.key = "cdAxial", .getter = &AerodynamicForces::getCDaxial},
    {.key = "pressureCD", .getter = &AerodynamicForces::getPressureCD},
    {.key = "baseCD", .getter = &AerodynamicForces::getBaseCD},
    {.key = "frictionCD", .getter = &AerodynamicForces::getFrictionCD},
    {.key = "overrideCD", .getter = &AerodynamicForces::getOverrideCD},
    {.key = "pitchDampingMoment", .getter = &AerodynamicForces::getPitchDampingMoment},
    {.key = "yawDampingMoment", .getter = &AerodynamicForces::getYawDampingMoment},
}};

/// Whether the golden forces key @p key is one compareForces() compares.
[[nodiscard]] bool isComparedForceKey(std::string_view key)
{
    return key == "path" || key == "cp" || key == "axisymmetric" ||
           std::ranges::find(kForceFields, key, &ForceField::key) != kForceFields.end();
}

/// Compares @p actual with the golden forces @p expected: the CP (its position within the
/// absolute tolerance, its weight, CNa, within the relative one), every coefficient and the
/// axisymmetric flag. A golden field that nothing compares is reported.
void compareForces(Mismatches& m, std::string_view what, const json& expected,
                   const AerodynamicForces& actual)
{
    m.cg(std::format("{}.cp", what), goldenCoordinate(expected.at("cp")), actual.getCP());
    for (const ForceField& field : kForceFields)
    {
        m.relative(std::format("{}.{}", what, field.key), goldenValue(expected.at(field.key)),
                   (actual.*field.getter)());
    }
    m.boolean(std::format("{}.axisymmetric", what), expected.at("axisymmetric").get<bool>(),
              actual.isAxisymmetric());
    for (const auto& [key, value] : expected.items())
    {
        if (!isComparedForceKey(key))
        {
            m.note(std::format("{}.{}: not compared", what, key));
        }
    }
}

/// The golden paths of the components of @p forceMap, in its order, separated by spaces.
[[nodiscard]] std::string keyPaths(const ForceMap& forceMap)
{
    std::string paths;
    for (const auto& [component, forces] : forceMap)
    {
        paths += paths.empty() ? "" : " ";
        paths += component != nullptr ? goldenPathOf(*component) : std::string{"null"};
    }
    return paths;
}

/// The paths of the golden force-analysis entries @p expected, in their order (the tree's),
/// separated by spaces.
[[nodiscard]] std::string entryPaths(const json& expected)
{
    std::string paths;
    for (const json& entry : expected)
    {
        paths += paths.empty() ? "" : " ";
        paths += entry.at("path").get<std::string>();
    }
    return paths;
}

/// Compares the force analysis @p actual with the golden "components" @p expected: the same
/// components in the same (tree) order, so none missing and none beyond them, and the forces of
/// each. Returns the number of golden entries compared.
[[nodiscard]] int compareForceAnalysis(Mismatches& m, const json& expected, const ForceMap& actual,
                                       const Rocket& rocket)
{
    m.text("components", entryPaths(expected), keyPaths(actual));
    int compared = 0;
    for (const json& entry : expected)
    {
        const auto               path      = entry.at("path").get<std::string>();
        const RocketComponent*   component = componentAtGoldenPath(rocket, path);
        const AerodynamicForces* forces    = component != nullptr ? actual.get(component) : nullptr;
        if (forces == nullptr)
        {
            m.note(std::format("components {}: not in the force analysis", path));
            continue;
        }
        compareForces(m, std::format("components {}", path), entry, *forces);
        compared++;
    }
    return compared;
}

// ============================================================================= configuration

/// One configuration under comparison: its rocket, the configuration, selected, and the
/// calculator that is its own.
struct Subject
{
    const Rocket*              rocket;
    const FlightConfiguration* config;
    BarrowmanCalculator*       calculator;
};

/// Calculates the golden point @p expected, the one of index @p index, as the dumper does
/// (getCP(), getAerodynamicForces() and getForceAnalysis() with one warning set) and compares
/// everything it records. Adds what was compared to @p counts.
void comparePoint(Mismatches& m, std::size_t index, const json& expected, const Subject& subject,
                  AeroCounts& counts)
{
    const json& expectedConditions = expected.at("conditions");
    compareWithDumperPoint(m, index, expectedConditions);
    const FlightConditions conditions =
        goldenConditions(m, expectedConditions, *subject.rocket, *subject.config);
    compareConditions(m, expectedConditions, conditions, *subject.rocket);

    WarningSet       warnings;
    const Coordinate cp = subject.calculator->getCP(*subject.config, conditions, &warnings);
    m.cg("cp", goldenCoordinate(expected.at("cp")), cp);

    const AerodynamicForces total =
        subject.calculator->getAerodynamicForces(*subject.config, conditions, &warnings);
    compareForces(m, "forces", expected.at("forces"), total);

    const ForceMap analysis =
        subject.calculator->getForceAnalysis(*subject.config, conditions, &warnings);
    counts.components +=
        compareForceAnalysis(m, expected.at("components"), analysis, *subject.rocket);
    counts.warnings +=
        compareWarnings(m, "warnings", expected.at("warnings"), warnings, *subject.rocket);
    noteUncomparedKeys(m, "", expected, kPointKeys);
    counts.points++;
}

/// The theta put into the conditions before getWorstCP(): none of the 360 directions it tries.
/// getWorstCP() works on a copy of the conditions and overwrites their theta with the one it
/// found (OpenRocket's too: the probe VerifyTheta.java gives the same worst CP and theta whatever
/// theta the conditions come with), so a theta that it did not set is seen. The dumper's
/// conditions come with 0, which is a direction of the 360 and the answer for most rockets.
constexpr double kThetaBeforeWorstCp = 1.0;

/// Whether @p theta is one of the 360 directions getWorstCP() tries: 2 pi i / 360 for an i of 0
/// to 359, the library's own expression, so exactly that double. False for NaN.
[[nodiscard]] bool isWorstCpDirection(double theta)
{
    const double i = std::round(theta * 360 / (2 * std::numbers::pi));
    return i >= 0 && i < 360 && theta == 2 * std::numbers::pi * i / 360;
}

/// The CP of the configuration of @p subject at the Mach number @p mach, an angle of attack of 0
/// and the wind direction @p theta, from a calculator of its own (so that the compared one sees
/// the dumper's calls only).
[[nodiscard]] Coordinate cpAtTheta(const Subject& subject, double mach, double theta)
{
    FlightConditions conditions(*subject.config);
    conditions.setMach(mach);
    conditions.setAOA(0.0);
    conditions.setTheta(theta);
    BarrowmanCalculator calculator;
    WarningSet          warnings;
    return calculator.getCP(*subject.config, conditions, &warnings);
}

/// What is wrong with @p theta as the direction of the worst CP @p worst that getWorstCP() found
/// at the Mach number @p mach, "" when nothing is: without a CP in any direction (a rocket
/// without lift: the golden worst CP @p goldenWorst has no weight) getWorstCP() leaves 0;
/// otherwise the theta is one of the 360 directions, and the CP at it is the worst CP found.
///
/// The CP at the theta is compared with the worst CP found, not with the golden one: the two
/// worst CPs are compared by the caller, and a difference between them must not be reported
/// twice, here only on the platforms whose theta is not the golden one.
[[nodiscard]] std::string foundThetaProblem(const Subject& subject, double mach, double theta,
                                            const Coordinate& worst, const Coordinate& goldenWorst)
{
    if (!(goldenWorst.weight > 0))
    {
        return "no direction has a CP, which leaves a theta of 0";
    }
    if (!isWorstCpDirection(theta))
    {
        return "it is none of the 360 directions getWorstCP() tries";
    }
    const Coordinate cp = cpAtTheta(subject, mach, theta);
    if (!(std::abs(cp.x - worst.x) <= kGoldenAbsolute))
    {
        return std::format("the CP at the theta found, x = {}, is not the worst CP found, x = {}",
                           cp.x, worst.x);
    }
    return "";
}

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
[[nodiscard]] bool compareWorstTheta(Mismatches& m, const Subject& subject, const json& expected,
                                     double theta, const Coordinate& worst)
{
    const double expectedTheta = goldenValue(expected.at("theta"));
    if (std::abs(theta - expectedTheta) <= kGoldenAbsolute)
    {
        return true;
    }
    const double      mach = goldenValue(expected.at("mach"));
    const std::string problem =
        foundThetaProblem(subject, mach, theta, worst, goldenCoordinate(expected.at("cp")));
    if (!problem.empty())
    {
        m.note(std::format("theta: expected {}, got {}; {}", expectedTheta, theta, problem));
        return false;
    }
    const Coordinate cp = cpAtTheta(subject, mach, expectedTheta);
    if (!(std::abs(cp.x - worst.x) <= kGoldenAbsolute))
    {
        m.note(
            std::format("theta: expected {}, got {}; the CP at the golden theta, x = {}, is "
                        "not the worst CP, x = {}",
                        expectedTheta, theta, cp.x, worst.x));
    }
    return false;
}

/// Calculates the golden worst CP @p expected as the dumper does (new conditions with the Mach
/// number at an angle of attack of 0; their theta is kThetaBeforeWorstCp, see there) and
/// compares the CP and the theta found. Returns the theta found when it is not the golden one,
/// else nullopt.
[[nodiscard]] std::optional<double> compareWorstCP(Mismatches& m, const json& expected,
                                                   const Subject& subject)
{
    const double     mach = goldenValue(expected.at("mach"));
    FlightConditions conditions(*subject.config);
    conditions.setMach(mach);
    conditions.setAOA(0.0);
    conditions.setTheta(kThetaBeforeWorstCp);
    WarningSet       warnings;
    const Coordinate cp = subject.calculator->getWorstCP(*subject.config, conditions, &warnings);
    m.cg("cp", goldenCoordinate(expected.at("cp")), cp);
    noteUncomparedKeys(m, "", expected, kWorstCpKeys);
    const double theta = conditions.getTheta();
    if (compareWorstTheta(m, subject, expected, theta, cp))
    {
        return std::nullopt;
    }
    return theta;
}

/// Compares the worst CPs of the configuration of @p subject with the golden list @p expected
/// and adds what it found to @p result.
void compareWorstCPs(AeroComparison& result, const json& expected, const Subject& subject,
                     const std::string& context)
{
    for (const json& worst : expected)
    {
        const std::string what =
            std::format("{} worst CP at Mach {}", context, goldenValue(worst.at("mach")));
        Mismatches m(what);
        if (const std::optional<double> theta = compareWorstCP(m, worst, subject))
        {
            result.otherThetas += std::format("{}: theta {}, golden {}\n", what, *theta,
                                              goldenValue(worst.at("theta")));
        }
        result.report += m.report();
        result.compared.worstCPs++;
    }
}

/// Compares the Mach numbers of the golden worst CPs @p expected with the dumper's
/// (AeroDumper.MACHS): as many, and each one.
void compareWorstCpMachs(Mismatches& m, const json& expected)
{
    m.integer("worstCP: number", static_cast<std::int64_t>(kDumperMachs.size()),
              static_cast<std::int64_t>(expected.size()));
    for (std::size_t i = 0; i < std::min(kDumperMachs.size(), expected.size()); i++)
    {
        m.exact(std::format("worstCP[{}].mach", i), kDumperMachs.at(i),
                goldenValue(expected.at(i).at("mach")));
    }
}

/// Compares the results of the configuration @p config of @p rocket, which is selected, with the
/// golden @p results, making the dumper's calls in the dumper's order on a calculator of its
/// own. @p context names the configuration in the report.
[[nodiscard]] AeroComparison compareResults(const json& results, const Rocket& rocket,
                                            const FlightConfiguration& config,
                                            const std::string&         context)
{
    AeroComparison      result;
    BarrowmanCalculator calculator;
    const Subject       subject{.rocket = &rocket, .config = &config, .calculator = &calculator};

    Mismatches geometry(context + " geometry warnings");
    WarningSet geometryWarnings;
    calculator.checkGeometry(config, rocket, &geometryWarnings);
    result.compared.warnings += compareWarnings(
        geometry, "geometryWarnings", results.at("geometryWarnings"), geometryWarnings, rocket);
    result.report += geometry.report();

    // The dumper calculates the structure CG here, for the pitch centre of point 23, and takes
    // the nozzle exit areas of point 24 from the motor mounts.
    const json&  points       = results.at("points");
    const double structureCgX = QtRocket::MassCalculator::calculateStructure(config).getCM().x;
    Mismatches   inputs(context + " inputs of the points");
    inputs.integer("points: number", static_cast<std::int64_t>(kDumperPoints),
                   static_cast<std::int64_t>(points.size()));
    compareWorstCpMachs(inputs, results.at("worstCP"));
    if (points.size() > kNozzlePoint)
    {
        inputs.absolute(
            "structure CG x (the pitch centre of point 23)",
            goldenCoordinate(points.at(kStructureCgPoint).at("conditions").at("pitchCenter")).x,
            structureCgX);
        compareDumperNozzleAreas(
            inputs, points.at(kNozzlePoint).at("conditions").at("thrustingNozzleExitAreas"), rocket,
            config);
    }
    else
    {
        inputs.note("no structure CG point and no nozzle point");
    }
    result.report += inputs.report();

    for (std::size_t i = 0; i < points.size(); i++)
    {
        const json& conditions = points.at(i).at("conditions");
        Mismatches  m(std::format("{} point {} (Mach {}, angle of attack {})", context, i,
                                  goldenValue(conditions.at("mach")),
                                  goldenValue(conditions.at("aoa"))));
        comparePoint(m, i, points.at(i), subject, result.compared);
        result.report += m.report();
    }

    compareWorstCPs(result, results.at("worstCP"), subject, context);
    result.compared.configurations = 1;
    return result;
}

/// Compares the header and the reference values of @p config with the golden configuration
/// @p expected. The id is compared unless it is one the maker draws at random.
void compareHeader(Mismatches& m, const json& expected, const FlightConfiguration& config,
                   bool randomConfigurationId)
{
    const QtRocket::InMemoryPreferences preferences;
    m.boolean("isDefault", expected.at("isDefault").get<bool>(), config.getId().isDefaultId());
    if (config.getId().isDefaultId() || !randomConfigurationId)
    {
        m.text("id", expected.at("id").get<std::string>(), config.getId().toString());
    }
    m.text("name", expected.at("name").get<std::string>(), config.getName(preferences));
    m.relative("referenceLength", goldenValue(expected.at("referenceLength")),
               config.getReferenceLength());
    m.relative("referenceArea", goldenValue(expected.at("referenceArea")),
               config.getReferenceArea());
    noteUncomparedKeys(m, "", expected, kConfigurationKeys);
}

/// Compares what every file records once: the atmosphere of the default flight conditions, the
/// stall angle, and the number of configurations, which is that of @p rocket with its default
/// one (the dumper writes one entry per configuration of rocket.getFlightConfigurations(); a
/// rocket with more configurations than the file would otherwise be compared without a
/// mismatch, since the loop follows the file).
void compareFileValues(Mismatches& m, const json& aero, const Rocket& rocket)
{
    const QtRocket::AtmosphericConditions atmosphere =
        FlightConditions().getAtmosphericConditions();
    const json& expected = aero.at("atmosphere");
    m.relative("atmosphere.temperature", goldenValue(expected.at("temperature")),
               atmosphere.getTemperature());
    m.relative("atmosphere.pressure", goldenValue(expected.at("pressure")),
               atmosphere.getPressure());
    m.relative("atmosphere.relativeHumidity", goldenValue(expected.at("relativeHumidity")),
               atmosphere.getRelativeHumidity());
    m.relative("atmosphere.machSpeed", goldenValue(expected.at("machSpeed")),
               atmosphere.getMachSpeed());
    m.relative("atmosphere.density", goldenValue(expected.at("density")), atmosphere.getDensity());
    m.relative("atmosphere.kinematicViscosity", goldenValue(expected.at("kinematicViscosity")),
               atmosphere.getKinematicViscosity());
    m.relative("stallAngle", goldenValue(aero.at("stallAngle")),
               BarrowmanCalculator().getStallAngle());
    m.integer("configurations: number", static_cast<std::int64_t>(aero.at("configurations").size()),
              static_cast<std::int64_t>(rocket.getConfigurationCount()) + 1);
    noteUncomparedKeys(m, "atmosphere", expected, kAtmosphereKeys);
    noteUncomparedKeys(m, "", aero, kFileKeys);
}

/// Compares @p rocket with the aero.json document @p aero: the file's values, then every
/// configuration, each one selected while it is calculated (as the dumper selects it) and the
/// original selection restored afterwards. @p randomConfigurationId: whether the maker of the
/// rocket draws the id of its configuration at random.
[[nodiscard]] AeroComparison compareAero(Rocket& rocket, const json& aero,
                                         bool randomConfigurationId)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // checkGeometry() prints lengths
    AeroComparison                          result;
    Mismatches                              file("file");
    compareFileValues(file, aero, rocket);
    result.report += file.report();

    const FlightConfigurationId selected       = rocket.getSelectedConfiguration().getId();
    const json&                 configurations = aero.at("configurations");
    for (std::size_t i = 0; i < configurations.size(); i++)
    {
        const std::string context = std::format("configuration {}", i);
        Mismatches        m(context);
        const json*       results = aeroResults(aero, i);
        if (results == nullptr || std::cmp_greater(i, rocket.getConfigurationCount()))
        {
            m.note("no golden results, or no such configuration in the rocket");
            result.report += m.report();
            continue;
        }
        const FlightConfiguration& config =
            rocket.getFlightConfigurationByIndex(static_cast<int>(i), true);
        rocket.setSelectedConfiguration(config.getId());
        m.integer("index", configurations.at(i).at("index").get<int>(), static_cast<int>(i));
        compareHeader(m, configurations.at(i), config, randomConfigurationId);
        result.report += m.report();

        const AeroComparison compared = compareResults(*results, rocket, config, context);
        result.compared += compared.compared;
        result.report += compared.report;
        result.otherThetas += compared.otherThetas;
    }
    rocket.setSelectedConfiguration(selected);
    return result;
}

// ============================================================================== the goldens

/// The aero.json of the golden input @p name, found through manifest.json.
[[nodiscard]] QtRocket::Result<json> loadGoldenAero(std::string_view name)
{
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        return std::unexpected(manifest.error());
    }
    const QtRocket::Test::GoldenInput* input = manifest->find(name);
    if (input == nullptr)
    {
        return QtRocket::fail(QtRocket::ErrorCode::NOT_FOUND,
                              std::format("no golden input named {}", name));
    }
    return QtRocket::Test::loadGoldenJson(input->aero);
}

/// A test rocket compared with its golden aero.json: what the file holds, and what the
/// comparison compared and found.
struct GoldenRun
{
    std::string    problem;  ///< why there is no comparison (the file could not be read), else ""
    AeroCounts     golden;
    AeroComparison comparison;
};

/// Builds the rocket of @p maker and compares it with its aero.json.
[[nodiscard]] GoldenRun runGolden(const TestRocketMaker& maker)
{
    GoldenRun                    run;
    const QtRocket::Result<json> aero = loadGoldenAero(maker.input);
    if (!aero)
    {
        run.problem = aero.error().message;
        return run;
    }
    const std::unique_ptr<Rocket> rocket = maker.make();
    run.golden                           = goldenCounts(*aero);
    run.comparison                       = compareAero(*rocket, *aero, maker.randomConfigurationId);
    return run;
}

/// runGolden() of @p maker, run once per test process and kept (std::map keeps the entries where
/// they are): the per-rocket test, the coverage test and the worst-theta test read the same
/// comparison (a getWorstCP() is 360 CPs, and there are 329 of them). The result depends on the
/// maker alone, so the order of the tests does not matter.
[[nodiscard]] const GoldenRun& goldenRun(const TestRocketMaker& maker)
{
    static std::mutex                                    s_mutex;
    static std::map<std::string, GoldenRun, std::less<>> s_runs;
    const std::scoped_lock                               lock{s_mutex};
    auto                                                 cached = s_runs.find(maker.input);
    if (cached == s_runs.end())
    {
        cached = s_runs.emplace(std::string{maker.input}, runGolden(maker)).first;
    }
    return cached->second;
}

// ===================================================================================== tests

/// One test rocket of TestRockets.h against its golden aerodynamics.
class AeroGolden : public ::testing::TestWithParam<TestRocketMaker>
{ };

TEST_P(AeroGolden, EveryConfigurationPointAndWorstCP)
{
    const GoldenRun& run = goldenRun(GetParam());
    ASSERT_EQ(run.problem, "");
    EXPECT_EQ(run.comparison.report, "");
    // Everything the file holds was compared: no configuration, point, force-analysis entry,
    // worst CP or warning skipped.
    EXPECT_EQ(run.comparison.compared, run.golden) << "what was compared, and what the file holds";
    EXPECT_GT(run.golden.points, 0);
}

/// The test name of @p info's maker: its golden input with '-' as '_'.
[[nodiscard]] std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, AeroGolden, ::testing::ValuesIn(testRocketMakers()),
                         makerTestName);

// ================================================================================== coverage

/// What the makers of TestRockets.h cover of the golden aerodynamics of the test rockets.
struct AeroCoverage
{
    int         goldenInputs{0};  ///< the "testrocket" inputs of the manifest
    AeroCounts  golden;           ///< what their aero.json files hold
    AeroCounts  compared;         ///< what was compared
    std::string problems;         ///< an input without a maker, a file not compared in full, ...
};

/// Compares every "testrocket" input of the manifest with the rocket of its maker and sums what
/// the files hold and what was compared; an input without a maker, one whose file cannot be
/// read and one that was not compared in full are problems.
[[nodiscard]] AeroCoverage aeroCoverage()
{
    AeroCoverage                                           coverage;
    const QtRocket::Result<QtRocket::Test::GoldenManifest> manifest =
        QtRocket::Test::loadGoldenManifest();
    if (!manifest)
    {
        coverage.problems = manifest.error().message;
        return coverage;
    }
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    for (const QtRocket::Test::GoldenInput& input : manifest->inputs)
    {
        if (input.kind != "testrocket")
        {
            continue;
        }
        coverage.goldenInputs++;
        const auto maker =
            std::ranges::find(makers, std::string_view{input.name}, &TestRocketMaker::input);
        if (maker == makers.end())
        {
            coverage.problems += std::format("{}: no maker\n", input.name);
            continue;
        }
        const GoldenRun& run = goldenRun(*maker);
        coverage.golden += run.golden;
        coverage.compared += run.comparison.compared;
        if (!run.problem.empty() || run.comparison.compared != run.golden)
        {
            coverage.problems +=
                std::format("{}: {} compared {}; the file holds {}\n", input.name, run.problem,
                            toText(run.comparison.compared), toText(run.golden));
        }
    }
    return coverage;
}

/// Every test rocket of the golden data is compared, and in full: the numbers of configurations,
/// points, force-analysis entries, worst CPs and warnings compared are those of each aero.json
/// (the per-rocket tests above report the mismatches; the totals are those of the thirteen
/// files).
TEST(AeroGoldenCoverage, EveryGoldenTestRocketIsComparedInFull)
{
    const AeroCoverage coverage = aeroCoverage();
    EXPECT_EQ(coverage.problems, "");
    EXPECT_EQ(coverage.goldenInputs, 13);
    EXPECT_EQ(static_cast<std::size_t>(coverage.goldenInputs), testRocketMakers().size());
    EXPECT_EQ(coverage.compared, coverage.golden) << "what was compared, and what the files hold";
    EXPECT_EQ(coverage.golden.configurations, 47) << "the default configurations included";
    EXPECT_EQ(coverage.golden.points, 1175) << "25 per configuration";
    EXPECT_EQ(coverage.golden.components, 9400) << "the force-analysis entries of every point";
    EXPECT_EQ(coverage.golden.worstCPs, 329) << "7 per configuration";
    EXPECT_EQ(coverage.golden.warnings, 294) << "the geometry warnings and those of the points";
}

// ================================================================================= mutations

// The comparison is not vacuous: a golden value changed in a copy of the document by four times
// its tolerance is reported, in one line that names it, and one changed by a quarter of its
// tolerance is not. The document is that of the Iso-Haisu: one configuration, and a worst CP
// that depends on the direction (two control fins).

/// Four times the tolerances of the comparison, relative and absolute alike.
constexpr double kBeyondTolerance = 4e-9;
/// A quarter of them.
constexpr double kWithinTolerance = 2.5e-10;

/// The value at the JSON pointer @p pointer of @p aero.
[[nodiscard]] json& valueAt(json& aero, std::string_view pointer)
{
    return aero.at(json::json_pointer{std::string{pointer}});
}

/// Multiplies the number at @p pointer by 1 + @p relative.
void scale(json& aero, std::string_view pointer, double relative)
{
    json& value = valueAt(aero, pointer);
    value       = value.get<double>() * (1 + relative);
}

/// Adds @p offset to the number at @p pointer.
void shift(json& aero, std::string_view pointer, double offset)
{
    json& value = valueAt(aero, pointer);
    value       = value.get<double>() + offset;
}

/// A change to a golden document that the comparison has to report, in one line.
struct Mutation
{
    std::string_view name;      ///< what is changed; it names the test
    void (*apply)(json& aero);  ///< makes the change
    std::string_view context;   ///< what the heading of the report starts with
    std::string_view line;      ///< what its one line starts with
};

/// The name of @p mutation, for the messages of the tests.
std::ostream& operator<<(std::ostream& out, const Mutation& mutation)
{
    return out << mutation.name;
}

/// The changes, each to the results of the one configuration of the Iso-Haisu.
[[nodiscard]] std::vector<Mutation> mutations()
{
    return {
        {.name = "CpX",
         .apply =
             [](json& aero) { shift(aero, "/configurations/0/points/4/cp/0", kBeyondTolerance); },
         .context = "configuration 0 point 4 (",
         .line    = "  cp.x: expected "},
        {.name = "CNa",
         .apply =
             [](json& aero) { scale(aero, "/configurations/0/points/4/cp/3", kBeyondTolerance); },
         .context = "configuration 0 point 4 (",
         .line    = "  cp.weight: expected "},
        {.name = "TotalCD",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/7/forces/cd", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cd: expected "},
        {.name = "TotalCm",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/7/forces/cm", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cm: expected "},
        {.name = "TotalCPOfTheForces",
         .apply =
             [](json& aero) {
                 shift(aero, "/configurations/0/points/7/forces/cp/0", kBeyondTolerance);
             },
         .context = "configuration 0 point 7 (",
         .line    = "  forces.cp.x: expected "},
        {.name = "RollDamping",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/22/forces/crollDamp", kBeyondTolerance);
             },
         .context = "configuration 0 point 22 (",
         .line    = "  forces.crollDamp: expected "},
        {.name = "PitchDampingMoment",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/22/forces/pitchDampingMoment",
                       kBeyondTolerance);
             },
         .context = "configuration 0 point 22 (",
         .line    = "  forces.pitchDampingMoment: expected "},
        {.name = "ComponentFrictionCD",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/13/components/5/frictionCD",
                       kBeyondTolerance);
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.frictionCD: expected "},
        {.name = "ComponentCpX",
         .apply =
             [](json& aero) {
                 shift(aero, "/configurations/0/points/13/components/5/cp/0", kBeyondTolerance);
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.cp.x: expected "},
        {.name = "AGoldenZero",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/0/forces/cside") = 1e-300; },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.cside: expected "},
        {.name = "Axisymmetric",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/0/forces/axisymmetric") = false;
             },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.axisymmetric: expected "},
        {.name = "AFieldNothingCompares",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/0/forces")["cnew"] = 1.0; },
         .context = "configuration 0 point 0 (",
         .line    = "  forces.cnew: not compared"},
        {.name = "Velocity",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/5/conditions/velocity", kBeyondTolerance);
             },
         .context = "configuration 0 point 5 (",
         .line    = "  conditions.velocity: expected "},
        {.name = "Beta",
         .apply =
             [](json& aero) {
                 scale(aero, "/configurations/0/points/5/conditions/beta", kBeyondTolerance);
             },
         .context = "configuration 0 point 5 (",
         .line    = "  conditions.beta: expected "},
        {.name = "AForceAnalysisEntryLess",
         .apply =
             [](json& aero) { valueAt(aero, "/configurations/0/points/3/components").erase(2); },
         .context = "configuration 0 point 3 (",
         .line    = "  components: expected "},
        {.name = "WarningText",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/text") = "Another text";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].text: expected "},
        {.name = "WarningPriority",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/priority") = "HIGH";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].priority: expected "},
        {.name = "WarningSources",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0/sources").push_back("/0/1");
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].sources: expected "},
        {.name = "AGeometryWarningMore",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/geometryWarnings")
                     .push_back(valueAt(aero, "/configurations/0/points/15/warnings/0"));
             },
         .context = "configuration 0 geometry warnings:",
         .line    = "  geometryWarnings: number: expected 1, got 0"},
        {.name = "WorstCpX",
         .apply =
             [](json& aero) { shift(aero, "/configurations/0/worstCP/2/cp/0", kBeyondTolerance); },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  cp.x: expected "},
        {.name = "WorstTheta",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/worstCP/2/theta") = std::numbers::pi / 2;
             },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  theta: expected "},
        {.name = "ReferenceArea",
         .apply =
             [](json& aero) { scale(aero, "/configurations/0/referenceArea", kBeyondTolerance); },
         .context = "configuration 0:",
         .line    = "  referenceArea: expected "},
        {.name    = "ConfigurationName",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/name") = "Another name"; },
         .context = "configuration 0:",
         .line    = "  name: expected "},
        {.name    = "StallAngle",
         .apply   = [](json& aero) { scale(aero, "/stallAngle", kBeyondTolerance); },
         .context = "file:",
         .line    = "  stallAngle: expected "},
        {.name    = "AtmosphericDensity",
         .apply   = [](json& aero) { scale(aero, "/atmosphere/density", kBeyondTolerance); },
         .context = "file:",
         .line    = "  atmosphere.density: expected "},
        // The structure of the document: a configuration or a worst CP less than the rocket and
        // the dumper have.
        {.name    = "AConfigurationLess",
         .apply   = [](json& aero) { valueAt(aero, "/configurations").erase(0); },
         .context = "file:",
         .line    = "  configurations: number: expected 0, got 1"},
        {.name    = "AWorstCpLess",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/worstCP").erase(6); },
         .context = "configuration 0 inputs of the points:",
         .line    = "  worstCP: number: expected 7, got 6"},
        // An input that is not the dumper's (the yaw rate changes no result: the yaw damping
        // moment is capped by a total Cyaw of 0).
        {.name = "AnInputThatIsNotTheDumpers",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3/conditions/yawRate") = 5.0;
             },
         .context = "configuration 0 point 3 (",
         .line    = "  the dumper's yawRate: expected 0, got 5"},
        // A field nothing compares, in every object of the document but the forces (above).
        {.name = "AConditionNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3/conditions")["altitude"] = 100.0;
             },
         .context = "configuration 0 point 3 (",
         .line    = "  conditions.altitude: not compared"},
        {.name = "APointFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/3")["cg"] = json::array({0.5, 0.0, 0.0});
             },
         .context = "configuration 0 point 3 (",
         .line    = "  cg: not compared"},
        {.name = "AComponentFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/13/components/5")["cnew"] = 1.0;
             },
         .context = "configuration 0 point 13 (",
         .line    = "  components /0/1/5.cnew: not compared"},
        {.name = "AWarningFieldNothingCompares",
         .apply =
             [](json& aero) {
                 valueAt(aero, "/configurations/0/points/15/warnings/0")["id"] = "an id";
             },
         .context = "configuration 0 point 15 (",
         .line    = "  warnings[0].id: not compared"},
        {.name    = "AWorstCpFieldNothingCompares",
         .apply   = [](json& aero) { valueAt(aero, "/configurations/0/worstCP/2")["aoa"] = 0.0; },
         .context = "configuration 0 worst CP at Mach 0.6:",
         .line    = "  aoa: not compared"},
        {.name  = "AConfigurationFieldNothingCompares",
         .apply = [](json& aero) { valueAt(aero, "/configurations/0")["motors"] = json::array(); },
         .context = "configuration 0:",
         .line    = "  motors: not compared"},
        {.name    = "AnAtmosphereFieldNothingCompares",
         .apply   = [](json& aero) { valueAt(aero, "/atmosphere")["altitude"] = 0.0; },
         .context = "file:",
         .line    = "  atmosphere.altitude: not compared"},
        {.name    = "AFileFieldNothingCompares",
         .apply   = [](json& aero) { aero["generator"] = "another dumper"; },
         .context = "file:",
         .line    = "  generator: not compared"},
    };
}

/// Changes of a quarter of the tolerance to the numbers mutations() changes.
void changeWithinTheTolerances(json& aero)
{
    shift(aero, "/configurations/0/points/4/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/4/cp/3", kWithinTolerance);
    scale(aero, "/configurations/0/points/7/forces/cd", kWithinTolerance);
    scale(aero, "/configurations/0/points/7/forces/cm", kWithinTolerance);
    shift(aero, "/configurations/0/points/7/forces/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/22/forces/crollDamp", kWithinTolerance);
    scale(aero, "/configurations/0/points/22/forces/pitchDampingMoment", kWithinTolerance);
    scale(aero, "/configurations/0/points/13/components/5/frictionCD", kWithinTolerance);
    shift(aero, "/configurations/0/points/13/components/5/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/points/5/conditions/velocity", kWithinTolerance);
    scale(aero, "/configurations/0/points/5/conditions/beta", kWithinTolerance);
    shift(aero, "/configurations/0/worstCP/2/cp/0", kWithinTolerance);
    scale(aero, "/configurations/0/referenceArea", kWithinTolerance);
    scale(aero, "/stallAngle", kWithinTolerance);
    scale(aero, "/atmosphere/density", kWithinTolerance);
}

/// The golden input the mutations change.
constexpr std::string_view kMutatedInput = "testrocket-iso-haisu";

/// The report of the comparison of the Iso-Haisu with its aero.json after @p change (nullptr: the
/// document as it is); "no golden data" when the document or the maker is missing.
[[nodiscard]] std::string reportAfter(void (*change)(json& aero))
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker            = std::ranges::find(makers, kMutatedInput, &TestRocketMaker::input);
    QtRocket::Result<json> aero = loadGoldenAero(kMutatedInput);
    if (maker == makers.end() || !aero)
    {
        return "no golden data";
    }
    if (change != nullptr)
    {
        change(*aero);
    }
    const std::unique_ptr<Rocket> rocket = maker->make();
    return compareAero(*rocket, *aero, maker->randomConfigurationId).report;
}

/// One changed golden value.
class AeroGoldenMutation : public ::testing::TestWithParam<Mutation>
{ };

TEST_P(AeroGoldenMutation, IsReportedInOneLine)
{
    const Mutation&   mutation = GetParam();
    const std::string report   = reportAfter(mutation.apply);
    EXPECT_TRUE(report.starts_with(mutation.context)) << report;
    EXPECT_NE(report.find(std::format("\n{}", mutation.line)), std::string::npos) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

/// The test name of @p info's mutation: what it changes.
[[nodiscard]] std::string mutationTestName(const ::testing::TestParamInfo<Mutation>& info)
{
    return std::string{info.param.name};
}

INSTANTIATE_TEST_SUITE_P(Changes, AeroGoldenMutation, ::testing::ValuesIn(mutations()),
                         mutationTestName);

TEST(AeroGoldenMutations, TheDocumentAsItIsMatches)
{
    EXPECT_EQ(reportAfter(nullptr), "");
}

TEST(AeroGoldenMutations, ChangesWithinTheTolerancesAreNotReported)
{
    EXPECT_EQ(reportAfter(changeWithinTheTolerances), "");
}

// =============================================================================== worst theta

/// The worst CPs of every test rocket whose theta is not the golden one, one line each.
[[nodiscard]] std::string otherWorstThetas()
{
    std::string lines;
    for (const TestRocketMaker& maker : testRocketMakers())
    {
        const GoldenRun& run = goldenRun(maker);
        if (!run.comparison.otherThetas.empty())
        {
            lines += std::format("{}:\n{}", maker.input, run.comparison.otherThetas);
        }
    }
    return lines;
}

// BLOCKED, and disabled because it cannot pass: the theta getWorstCP() leaves in the conditions,
// compared strictly with the golden one. It differs for most of the worst CPs of the rockets
// whose CP does not depend on the direction (251 of the 329 on Linux with glibc; every rocket but
// the Iso-Haisu, the simple two-stage rocket and the cluster pods): there the 360 CPs differ only
// in the rounding of the sum over the fins, and the direction that happens to give the smallest
// x follows the order of that sum (Java: the components' random ids, so OpenRocket's own answer
// changes with them) and the last bit of sin() (which differs between Java, glibc, Apple's libm
// and the UCRT). The CPs themselves match (AeroGolden), and AeroGolden accepts another theta
// only when it is a direction getWorstCP() tries whose CP is the worst CP, and the CP at the
// golden theta is the worst CP too (compareWorstTheta()): any equally bad direction passes,
// as it must, so which one is found stays uncompared and this test is its record. To see the
// list, run with --gtest_also_run_disabled_tests --gtest_filter='AeroGoldenWorstTheta.*'.
TEST(AeroGoldenWorstTheta, DISABLED_TheDirectionFoundIsJavasInEveryRocket)
{
    EXPECT_EQ(otherWorstThetas(), "");
}

// The check of the theta found is not vacuous either. The mutation WorstTheta above changes the
// golden side; these put a theta in the place of the one getWorstCP() left, for the worst CP of
// the Iso-Haisu at Mach 0.6 (golden theta 0; the CP is at x = 1.8197 m in the directions 0 and
// pi, the plane of its two control fins, and at 2.1349 m across it, in OpenRocket and here).

/// What compareWorstTheta() answers and reports.
struct ThetaVerdict
{
    bool        sameDirection{false};
    std::string report;
};

/// compareWorstTheta() for the worst CP of the Iso-Haisu at Mach 0.6, with @p theta as the
/// theta found; @p change, when given, first changes the golden worst CP.
[[nodiscard]] ThetaVerdict isoHaisuThetaVerdict(double theta, void (*change)(json& worst) = nullptr)
{
    const std::span<const TestRocketMaker> makers = testRocketMakers();
    const auto maker            = std::ranges::find(makers, kMutatedInput, &TestRocketMaker::input);
    QtRocket::Result<json> aero = loadGoldenAero(kMutatedInput);
    if (maker == makers.end() || !aero)
    {
        return {.sameDirection = false, .report = "no golden data"};
    }
    json& expected = valueAt(*aero, "/configurations/0/worstCP/2");
    if (change != nullptr)
    {
        change(expected);
    }
    const std::unique_ptr<Rocket> rocket = maker->make();
    const FlightConfiguration&    config = rocket->getSelectedConfiguration();
    BarrowmanCalculator           calculator;
    const Subject    subject{.rocket = rocket.get(), .config = &config, .calculator = &calculator};
    FlightConditions conditions(config);
    conditions.setMach(goldenValue(expected.at("mach")));
    conditions.setAOA(0.0);
    const Coordinate worst = calculator.getWorstCP(config, conditions, nullptr);

    Mismatches m("worst theta");
    const bool same = compareWorstTheta(m, subject, expected, theta, worst);
    return {.sameDirection = same, .report = m.report()};
}

/// Direction @p i of the 360 getWorstCP() tries, as the library writes it.
[[nodiscard]] double worstCpDirection(int i)
{
    return 2 * std::numbers::pi * i / 360;
}

TEST(AeroGoldenWorstTheta, TheGoldenThetaIsTheSameDirection)
{
    const ThetaVerdict verdict = isoHaisuThetaVerdict(0.0);
    EXPECT_TRUE(verdict.sameDirection);
    EXPECT_EQ(verdict.report, "");
}

TEST(AeroGoldenWorstTheta, AnEquallyBadDirectionIsAnotherThetaButNoMismatch)
{
    // pi: the other direction in the plane of the control fins, with the same CP.
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(180));
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_EQ(verdict.report, "");
}

TEST(AeroGoldenWorstTheta, ADirectionWithAnotherCpIsReported)
{
    // pi / 2, across the control fins: a wrong answer, not an equally bad direction.
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(90));
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_TRUE(verdict.report.starts_with("worst theta:\n  theta: expected 0, got 1.57079"))
        << verdict.report;
    EXPECT_NE(verdict.report.find("; the CP at the theta found, x = 2.13491"), std::string::npos)
        << verdict.report;
    EXPECT_NE(verdict.report.find(", is not the worst CP found, x = 1.81970"), std::string::npos)
        << verdict.report;
    EXPECT_EQ(std::ranges::count(verdict.report, '\n'), 2) << verdict.report;

    // One direction further than the golden one is wrong already (by 0.1 mm).
    const ThetaVerdict next = isoHaisuThetaVerdict(worstCpDirection(1));
    EXPECT_NE(next.report.find("; the CP at the theta found, x = "), std::string::npos)
        << next.report;
}

/// Whether @p verdict reports a theta that is none of the 360 directions, in one line.
[[nodiscard]] bool reportsNoDirection(const ThetaVerdict& verdict)
{
    return !verdict.sameDirection &&
           verdict.report.contains("; it is none of the 360 directions getWorstCP() tries") &&
           std::ranges::count(verdict.report, '\n') == 2;
}

TEST(AeroGoldenWorstTheta, AThetaThatIsNoneOfTheDirectionsIsReported)
{
    // The theta the conditions had before the call (a getWorstCP() that left it), a direction
    // off the grid by one rounding, values outside 0 ... 2 pi, and NaN.
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(kThetaBeforeWorstCp)));
    EXPECT_TRUE(
        reportsNoDirection(isoHaisuThetaVerdict(std::nextafter(worstCpDirection(180), 4.0))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(worstCpDirection(360))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(worstCpDirection(-1))));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(1e9)));
    EXPECT_TRUE(reportsNoDirection(isoHaisuThetaVerdict(std::numeric_limits<double>::quiet_NaN())));
    // Every direction of the 360 is one.
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(0)));
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(1)));
    EXPECT_TRUE(isWorstCpDirection(worstCpDirection(359)));
    EXPECT_FALSE(isWorstCpDirection(worstCpDirection(360)));
}

TEST(AeroGoldenWorstTheta, AThetaForARocketWithoutLiftIsReported)
{
    // A golden worst CP without weight (no direction has a CP: getWorstCP() answers
    // Double.MAX_VALUE and leaves a theta of 0) against a theta that is a direction.
    const auto withoutLift = [](json& worst) {
        worst["cp"]    = json::array({std::numeric_limits<double>::max(), 0.0, 0.0, 0.0});
        worst["theta"] = 0.0;
    };
    const ThetaVerdict verdict = isoHaisuThetaVerdict(worstCpDirection(5), withoutLift);
    EXPECT_FALSE(verdict.sameDirection);
    EXPECT_NE(verdict.report.find("; no direction has a CP, which leaves a theta of 0"),
              std::string::npos)
        << verdict.report;
    EXPECT_EQ(std::ranges::count(verdict.report, '\n'), 2) << verdict.report;
    EXPECT_TRUE(isoHaisuThetaVerdict(0.0, withoutLift).sameDirection);
}

TEST(AeroGoldenWorstTheta, TheThetaBeforeTheCallIsNoneOfTheDirections)
{
    // ... so that a getWorstCP() that left the theta it was given is seen (compareWorstCP()).
    EXPECT_FALSE(isWorstCpDirection(kThetaBeforeWorstCp));
}

// ================================================================== warnings with a parameter

// No aerodynamic calculator raises a warning with a parameter (the angle of a LargeAOA comes
// from the simulation), so no aero.json holds one and the comparison of the parameter never runs
// against golden data. It is tested here: a LargeAOA against its own golden form and against
// changed ones.

/// @p warning as the dumper writes it (Values.warning()): its class, priority, description,
/// text, the paths of its sources in @p rocket and, for a warning with one, its parameter.
[[nodiscard]] json goldenFormOf(const Warning& warning, const Rocket& rocket)
{
    json form           = json::object();
    form["type"]        = std::string{warning.typeName()};
    form["priority"]    = std::string{QtRocket::exportLabel(warning.priority())};
    form["description"] = warning.messageDescription();
    form["text"]        = warning.toString();
    form["sources"]     = json::array();
    for (const QtRocket::MessageSource& source : warning.sources())
    {
        const RocketComponent* component = rocket.findComponent(source.id);
        form["sources"].push_back(component != nullptr ? goldenPathOf(*component)
                                                       : std::string{"?"});
    }
    if (const std::optional<double> parameter = parameterOf(warning))
    {
        form["parameter"] = *parameter;
    }
    return form;
}

/// The report of compareWarning() for @p actual against the golden form @p expected.
[[nodiscard]] std::string warningReport(const json& expected, const Warning& actual,
                                        const Rocket& rocket)
{
    Mismatches m("warning");
    compareWarning(m, "w", expected, actual, rocket);
    return m.report();
}

/// The angle of the LargeAOA of these tests, in radians.
constexpr double kLargeAoa = 0.4;

TEST(AeroGoldenWarnings, TheParameterOfAWarningIsCompared)
{
    const QtRocket::Test::DefaultUnitsGuard units;  // the text of a LargeAOA prints the angle
    const Rocket                            rocket;
    const Warning::LargeAOA                 large{kLargeAoa};
    EXPECT_EQ(parameterOf(large), kLargeAoa);

    json expected = goldenFormOf(large, rocket);
    ASSERT_TRUE(expected.contains("parameter"));
    EXPECT_EQ(expected.at("type"), "LargeAOA");
    EXPECT_EQ(warningReport(expected, large, rocket), "");

    expected["parameter"] = kLargeAoa * (1 + kWithinTolerance);
    EXPECT_EQ(warningReport(expected, large, rocket), "");

    expected["parameter"]    = kLargeAoa * (1 + kBeyondTolerance);
    const std::string report = warningReport(expected, large, rocket);
    EXPECT_TRUE(report.starts_with("warning:\n  w.parameter: expected 0.40000000")) << report;
    EXPECT_NE(report.find(", got 0.4 (difference "), std::string::npos) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

TEST(AeroGoldenWarnings, AParameterOnOneSideOnlyIsReported)
{
    const QtRocket::Test::DefaultUnitsGuard units;
    const Rocket                            rocket;
    const Warning::LargeAOA                 large{kLargeAoa};

    // The golden warning without its parameter.
    json expected = goldenFormOf(large, rocket);
    expected.erase("parameter");
    const std::string missing = warningReport(expected, large, rocket);
    EXPECT_TRUE(missing.starts_with("warning:\n  w has a parameter: expected false, got true"))
        << missing;
    EXPECT_EQ(std::ranges::count(missing, '\n'), 2) << missing;

    // A golden parameter for a warning that has none.
    const Warning& plain    = Warning::kSupersonic;
    json           withOne  = goldenFormOf(plain, rocket);
    withOne["parameter"]    = 1.0;
    const std::string extra = warningReport(withOne, plain, rocket);
    EXPECT_TRUE(extra.starts_with("warning:\n  w has a parameter: expected true, got false"))
        << extra;
    EXPECT_EQ(std::ranges::count(extra, '\n'), 2) << extra;
    EXPECT_EQ(warningReport(goldenFormOf(plain, rocket), plain, rocket), "");
}

TEST(AeroGoldenWarnings, AWarningWithSourcesIsComparedByTheirPaths)
{
    // The sources of a warning are compared as golden paths, in their order.
    const QtRocket::Test::DefaultUnitsGuard units;
    const QtRocket::Test::TestEstesAlphaIII alpha;
    WarningSet                              warnings;
    warnings.add(Warning::kDiameterDiscontinuity,
                 QtRocket::MessageSources{QtRocket::MessageSource::of(*alpha.nose),
                                          QtRocket::MessageSource::of(*alpha.body)});
    const Warning& warning  = *warnings.begin();
    json           expected = goldenFormOf(warning, *alpha.rocket);
    EXPECT_EQ(expected.at("sources"), json::array({"/0/0", "/0/1"}));
    EXPECT_EQ(warningReport(expected, warning, *alpha.rocket), "");

    expected["sources"]      = json::array({"/0/1", "/0/0"});
    const std::string report = warningReport(expected, warning, *alpha.rocket);
    EXPECT_TRUE(report.starts_with("warning:\n  w.sources: expected ")) << report;
    EXPECT_EQ(std::ranges::count(report, '\n'), 2) << report;
}

}  // namespace
