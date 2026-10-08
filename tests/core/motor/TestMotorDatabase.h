#pragma once

// The bundled motor database for the tests that resolve the motors of design files. Test-only.

#include <memory>

#include <gtest/gtest.h>

#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

/// The motor database OpenRocket ships (data/motors/initial_motors.db: 1591 thrust curves),
/// filled exactly as the golden harness fills the one its reference data was made with:
/// SqliteMotorDatabaseReader::readDatabase(), then addMotor() for every motor in the order
/// returned, which gives 1458 motor sets holding 1588 motors. The order matters: of several
/// motors that fit a design's motor, DatabaseMotorFinder takes the first.
///
/// It is read once per test process (about half a second in a debug build, a few seconds under
/// the sanitizers) and shared: never load it per test, and never change it.
[[nodiscard]] inline const ThrustCurveMotorSetDatabase& bundledMotorDatabase()
{
    static const ThrustCurveMotorSetDatabase kDatabase = [] {
        ThrustCurveMotorSetDatabase                       database;
        const Result<SqliteMotorDatabaseReader::Contents> contents =
            SqliteMotorDatabaseReader::readDatabase(dataDir() / "motors" / "initial_motors.db");
        if (!contents)
        {
            ADD_FAILURE() << "cannot read the bundled motor database: " << contents.error().message;
            return database;
        }
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : contents->motors)
        {
            database.addMotor(motor);
        }
        return database;
    }();
    return kDatabase;
}

}  // namespace QtRocket::Test
