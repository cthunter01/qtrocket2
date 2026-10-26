#include "QtRocket/cli/CliEnvironment.h"

#include <expected>
#include <filesystem>
#include <memory>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

std::filesystem::path CliEnvironment::defaultMotorDatabase(
    const std::filesystem::path& dataDirectory)
{
    return dataDirectory / "motors" / "initial_motors.db";
}

Result<ThrustCurveMotorSetDatabase> CliEnvironment::readMotorDatabase(
    const std::filesystem::path& file)
{
    const Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(file);
    if (!contents)
    {
        return std::unexpected(contents.error());
    }
    ThrustCurveMotorSetDatabase database;
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : contents->motors)
    {
        database.addMotor(motor);
    }
    return database;
}

std::filesystem::path CliEnvironment::defaultComponentPresetDirectory(
    const std::filesystem::path& dataDirectory)
{
    return dataDirectory / ComponentPresetDatabaseLoader::kBundledDirectory;
}

CliEnvironment::ComponentPresets CliEnvironment::readComponentPresets(
    const std::filesystem::path& directory)
{
    // The presets hold copies of the materials they found (OpenRocketComponentLoader), so the
    // storage goes with the loader.
    MaterialStorage materials;
    addBuiltinMaterials(materials);
    ComponentPresetDatabaseLoader loader(materials);
    loader.loadPresetComponents(directory);
    ComponentPresets presets{.database = loader.takeDatabase(),
                             .problems = loader.getProblems(),
                             .warnings = loader.getWarnings()};
    return presets;
}

CliEnvironment::CliEnvironment(const MotorDatabase& motors, const ComponentPresetDatabase& presets)
  : m_motorFinder(motors), m_extensions(SimulationExtensionRegistry::bundled())
{
    addBuiltinMaterials(m_materials);
    m_context.setMotorFinder(&m_motorFinder);
    m_context.setApplicationMaterials(&m_materials);
    m_context.setPreferences(&m_preferences);
    m_context.setComponentPresetDatabase(&presets);
    m_context.setSimulationExtensionRegistry(&m_extensions);
}

Result<LoadedDocument> CliEnvironment::load(const std::filesystem::path& file) const
{
    const GeneralRocketLoader loader(m_context);
    return loader.load(file);
}

}  // namespace QtRocket
