#pragma once

#include <memory>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"

/// The motors of OpenRocket's OpenRocketSaverTest (core/src/test/.../file/openrocket), whose
/// set-up the tests of the saver that reload a document of TestRockets
/// (document/TestDocuments.h) reproduce. The definitions are in OpenRocketSaverTestMotors.cpp.
/// Test-only.
namespace QtRocket::Test
{

/// OpenRocketSaverTest.readMotor(): the motor of tests/data/motors/Estes_A8.rse (identical to
/// OpenRocket's core/src/test/resources/Estes_A8.rse), or null with a test failure.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> readEstesA8Motor();

/// OpenRocketSaverTest's motor database (MotorDbProvider): exactly the Estes A8 of
/// readEstesA8Motor() and an F12X of the manufacturer "A" with the digest "digestA" (the
/// getTestMotor() of document/TestDocuments.h without its case info), two motor sets. A test
/// failure when either is missing.
[[nodiscard]] std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors();

}  // namespace QtRocket::Test
