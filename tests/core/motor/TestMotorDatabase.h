#pragma once

// The motor databases of the tests: the bundled database for the tests that resolve the motors
// of design files and for the goldens, and two small ones for unit tests (the GUI's among them,
// whose motor tables and choosers would be slow over 1458 sets). The definitions are in
// TestMotorDatabase.cpp. Test-only.

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"

namespace QtRocket::Test
{

/// Whether @p digest has the form of every digest MotorDigest gives: 32 lowercase hexadecimal
/// characters (risk 13 of the plan: the saver names an embedded motor's zip entry
/// thrustcurves/<digest>.rse, and the zip writer refuses a backslash or a NUL in a name).
[[nodiscard]] bool hasMotorDigestForm(std::string_view digest) noexcept;

/// The thrust curves of the motor database OpenRocket ships (data/motors/initial_motors.db),
/// 1591 of them, in the order SqliteMotorDatabaseReader::readDatabase() returns them. Read once
/// per test process and shared (the motors are immutable); empty with a test failure when the
/// database cannot be read.
[[nodiscard]] const std::vector<std::shared_ptr<const ThrustCurveMotor>>& bundledMotors();

/// The motor database OpenRocket ships (data/motors/initial_motors.db: 1591 thrust curves),
/// filled exactly as the golden harness fills the one its reference data was made with:
/// SqliteMotorDatabaseReader::readDatabase(), then addMotor() for every motor in the order
/// returned (bundledMotors()), which gives 1458 motor sets holding 1588 motors. The order
/// matters: of several motors that fit a design's motor, DatabaseMotorFinder takes the first.
///
/// It is read once per test process (about 0.2 s in a debug build, 0.6 s under the sanitizers)
/// and shared: never load it per test, and never change it.
[[nodiscard]] const ThrustCurveMotorSetDatabase& bundledMotorDatabase();

/// The one motor of tests/data/motors/Estes_A8.rse (a copy of OpenRocket's
/// swing/src/test/resources/Estes_A8.rse and core/src/test/resources/Estes_A8.rse): the motor of
/// IntegrationTest.readMotor() and OpenRocketSaverTest.readMotor(), read through
/// GeneralMotorLoader as they read it. A new object at every call; null with a test failure when
/// the file cannot be read.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> estesA8Motor();

/// The motor database of swing's IntegrationTest (its MotorDbProvider): one motor set holding
/// estesA8Motor(); a database without sets, with a test failure, when the file cannot be read.
[[nodiscard]] ThrustCurveMotorSetDatabase estesA8Database();

/// A motor set of smallMotorDatabase(): the one set of the bundled database whose manufacturer
/// matches() @c manufacturer and whose designation is @c designation.
struct SmallMotorSet
{
    std::string_view manufacturer;
    std::string_view designation;
    /// How many curves the set holds (ThrustCurveMotorSet::getMotorCount()).
    std::size_t curves;
};

/// The sets of smallMotorDatabase(), in their order there (the bundled database's), and what
/// each of them is there for:
/// - Klima C6 and Estes C6: one motor of two manufacturers (the manufacturer filter, a search);
///   Estes C6 has two curves and the delays 0, 3, 5, 7 and plugged;
/// - AeroTech E15W: three curves, of which the first two are very similar (similarity 0.988,
///   the third 0.72 and 0.71: "Hide very similar thrust curves" hides one of the first two);
/// - AeroTech E16W: a reload (case "RMS-29/40-120");
/// - AeroTech G12ST: plugged only (the delay "P"), two curves;
/// - AeroTech J145H_2JET_STD: a hybrid, 54 mm (common name J145);
/// - AeroTech M1850W: a 75 mm reload of class M, three curves;
/// - Cesaroni 117G69-14A: a reload of a third manufacturer, sparky, case "Pro38-1G", three
///   curves;
/// - Estes 1/2A3: 13 mm, of the class 1/2A;
/// - Estes A8: the motor of OpenRocket's example "A simple model rocket" and of the A8 designs
///   (two curves);
/// - Quest B6W: a designation that differs from its common name (B6).
/// Five manufacturers, the three types (single use, reload, hybrid), 13 to 75 mm; 21 curves in
/// all.
inline constexpr std::array<SmallMotorSet, 11> kSmallMotorSets{{
    {.manufacturer = "Klima", .designation = "C6", .curves = 2},
    {.manufacturer = "AeroTech", .designation = "E15W", .curves = 3},
    {.manufacturer = "AeroTech", .designation = "E16W", .curves = 1},
    {.manufacturer = "AeroTech", .designation = "G12ST", .curves = 2},
    {.manufacturer = "AeroTech", .designation = "J145H_2JET_STD", .curves = 1},
    {.manufacturer = "AeroTech", .designation = "M1850W", .curves = 3},
    {.manufacturer = "Cesaroni", .designation = "117G69-14A", .curves = 3},
    {.manufacturer = "Estes", .designation = "1/2A3", .curves = 1},
    {.manufacturer = "Estes", .designation = "A8", .curves = 2},
    {.manufacturer = "Estes", .designation = "C6", .curves = 2},
    {.manufacturer = "Quest", .designation = "B6W", .curves = 1},
}};

/// A small motor database of the bundled database's kinds (kSmallMotorSets): the motors of
/// those sets of bundledMotorDatabase(), the very objects, added in the order of
/// bundledMotors(), so that each set is the bundled one (the same curves in the same order, the
/// same delays, type and texts) and the sets keep the bundled order. Reading it reads the
/// bundled database once per process (bundledMotorDatabase()); the database itself is a new one
/// at every call. A set the bundled database does not hold is a test failure.
[[nodiscard]] ThrustCurveMotorSetDatabase smallMotorDatabase();

}  // namespace QtRocket::Test
