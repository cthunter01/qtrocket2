#pragma once

#include <memory>

#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"

/// The motors of OpenRocket's OpenRocketSaverTest (core/src/test/.../file/openrocket), whose
/// set-up the tests of the saver that reload a document of TestRockets
/// (document/TestDocuments.h) reproduce. The definitions are in OpenRocketSaverTestMotors.cpp.
/// Test-only.
namespace QtRocket::Test
{

/// OpenRocketSaverTest's motor database (MotorDbProvider): exactly the Estes A8 of its
/// readMotor() (estesA8Motor() of motor/TestMotorDatabase.h) and an F12X of the manufacturer "A"
/// with the digest "digestA" (the getTestMotor() of document/TestDocuments.h without its case
/// info), two motor sets. A test failure when either is missing.
[[nodiscard]] std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors();

}  // namespace QtRocket::Test
