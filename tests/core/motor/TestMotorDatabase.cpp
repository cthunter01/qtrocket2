#include "motor/TestMotorDatabase.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSet.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"

namespace QtRocket::Test
{

namespace
{

using MotorList = std::vector<std::shared_ptr<const ThrustCurveMotor>>;

/// The one set of the bundled database that @p spec names, or null with a test failure when
/// there is none or more than one.
[[nodiscard]] const ThrustCurveMotorSet* bundledSet(const SmallMotorSet& spec)
{
    const ThrustCurveMotorSet* found = nullptr;
    std::size_t                count = 0;
    for (const ThrustCurveMotorSet& set : bundledMotorDatabase().getMotorSets())
    {
        if (set.getDesignation() == spec.designation &&
            set.getManufacturer()->matches(spec.manufacturer))
        {
            found = &set;
            ++count;
        }
    }
    if (count != 1)
    {
        ADD_FAILURE() << "the bundled database holds " << count << " sets of " << spec.manufacturer
                      << " " << spec.designation << ", not one";
        return nullptr;
    }
    return found;
}

}  // namespace

bool hasMotorDigestForm(std::string_view digest) noexcept
{
    return digest.size() == 32 && std::ranges::all_of(digest, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}

const std::vector<std::shared_ptr<const ThrustCurveMotor>>& bundledMotors()
{
    static const MotorList kMotors = [] {
        const Result<SqliteMotorDatabaseReader::Contents> contents =
            SqliteMotorDatabaseReader::readDatabase(dataDir() / "motors" / "initial_motors.db");
        if (!contents)
        {
            ADD_FAILURE() << "cannot read the bundled motor database: " << contents.error().message;
            return MotorList{};
        }
        return contents->motors;
    }();
    return kMotors;
}

const ThrustCurveMotorSetDatabase& bundledMotorDatabase()
{
    static const ThrustCurveMotorSetDatabase kDatabase = [] {
        ThrustCurveMotorSetDatabase database;
        for (const std::shared_ptr<const ThrustCurveMotor>& motor : bundledMotors())
        {
            database.addMotor(motor);
        }
        return database;
    }();
    return kDatabase;
}

std::shared_ptr<const ThrustCurveMotor> estesA8Motor()
{
    const Result<std::vector<std::byte>> bytes =
        readFile(testDataDir() / "motors" / "Estes_A8.rse");
    if (!bytes)
    {
        ADD_FAILURE() << "Problem in unit test, cannot find Estes_A8.rse";
        return nullptr;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> builders =
        GeneralMotorLoader().load(*bytes, "Estes_A8.rse");
    if (!builders || builders->empty())
    {
        ADD_FAILURE() << "Could not load motor";
        return nullptr;
    }
    Result<ThrustCurveMotor> motor = builders->front().build();
    if (!motor)
    {
        ADD_FAILURE() << motor.error().toString();
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

ThrustCurveMotorSetDatabase estesA8Database()
{
    ThrustCurveMotorSetDatabase database;
    if (std::shared_ptr<const ThrustCurveMotor> motor = estesA8Motor())
    {
        database.addMotor(std::move(motor));
    }
    return database;
}

ThrustCurveMotorSetDatabase smallMotorDatabase()
{
    std::set<const ThrustCurveMotor*> chosen;
    for (const SmallMotorSet& spec : kSmallMotorSets)
    {
        if (const ThrustCurveMotorSet* const set = bundledSet(spec))
        {
            for (const std::shared_ptr<const ThrustCurveMotor>& motor : set->getMotors())
            {
                chosen.insert(motor.get());
            }
        }
    }
    ThrustCurveMotorSetDatabase database;
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : bundledMotors())
    {
        if (chosen.contains(motor.get()))
        {
            database.addMotor(motor);
        }
    }
    return database;
}

}  // namespace QtRocket::Test
