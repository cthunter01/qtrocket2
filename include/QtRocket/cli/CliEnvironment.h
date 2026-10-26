#pragma once

#include <filesystem>
#include <vector>

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/MotorDatabase.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// What the command-line tool loads a design with: the application's environment, which
/// OpenRocket takes from its global objects (QtRocket's own; the tests have the same for
/// OpenRocket's test set-up in tests/core/file/DesignFileEnvironment.h):
/// - the preference store in its factory state (an empty InMemoryPreferences: every getter gives
///   OpenRocket's default), which the simulations of the loaded documents keep;
/// - OpenRocket's materials (addBuiltinMaterials());
/// - the component presets of a database the caller hands in, for the tool the bundled presets of
///   its data directory (readComponentPresets() of defaultComponentPresetDirectory(): the 5228
///   presets of OpenRocket's database), which OpenRocket's core loads before a design is opened
///   (CoreModule's ComponentPresetDatabaseLoader). A <preset> of a design finds its preset there
///   (the three examples with presets load without a warning); one the database lacks gives
///   OpenRocket's warning, and the component keeps the reference
///   (RocketComponent::getUnresolvedPreset());
/// - the bundled simulation extensions (SimulationExtensionRegistry::bundled());
/// - the motors of a motor database, looked up by a DatabaseMotorFinder.
///
/// Deviation from OpenRocket: no preset file of the user is read. OpenRocket's loader reads the
/// bundled presets and then the user's files and directories of its preferences
/// (getUserComponentPresetFiles(): by default the directory Components of the user's OpenRocket
/// directory, which that getter creates when it is missing); the tool has the factory
/// preferences, reads the bundled presets alone and creates nothing, so that a design gives the
/// same output on every machine (the user's decision U-M3-4 of 2026-10-24: the tool loads the
/// bundled presets).
///
/// Every document the environment loaded keeps its preference store: declare the documents after
/// the environment, so that they go first. Neither copied nor moved (the context points at the
/// members).
class CliEnvironment
{
public:
    /// What readComponentPresets() read: the database and what went wrong.
    struct ComponentPresets
    {
        /// The presets of the files that were read.
        ComponentPresetDatabase database;
        /// ComponentPresetDatabaseLoader::getProblems(): the files and directories that could
        /// not be read, in the order they were met (OpenRocket logs them).
        std::vector<ComponentPresetDatabaseLoader::Problem> problems;
        /// ComponentPresetDatabaseLoader::getWarnings(): the components the files' loader
        /// skipped.
        WarningSet warnings;
    };

    /// The motor database file of the data directory @p dataDirectory:
    /// <dataDirectory>/motors/initial_motors.db (data/motors in the source tree, share/QtRocket/
    /// motors installed).
    [[nodiscard]] static std::filesystem::path defaultMotorDatabase(
        const std::filesystem::path& dataDirectory);

    /// Reads the SQLite motor database @p file into a database, as OpenRocket's loader of its
    /// bundled database fills it: SqliteMotorDatabaseReader::readDatabase(), then addMotor() for
    /// each motor in the order read (the curves the reader leaves out are left out silently,
    /// as OpenRocket only logs them). The Error of the reader when the file cannot be read.
    [[nodiscard]] static Result<ThrustCurveMotorSetDatabase> readMotorDatabase(
        const std::filesystem::path& file);

    /// The directory of the bundled component presets of the data directory @p dataDirectory:
    /// <dataDirectory>/components (ComponentPresetDatabaseLoader::kBundledDirectory;
    /// data/components in the source tree, share/QtRocket/components installed).
    [[nodiscard]] static std::filesystem::path defaultComponentPresetDirectory(
        const std::filesystem::path& dataDirectory);

    /// Reads the component presets of the directory @p directory into a database, as OpenRocket's
    /// ComponentPresetDatabaseLoader fills its database with the bundled presets
    /// (ComponentPresetDatabaseLoader::loadPresetComponents(), the materials looked up in
    /// OpenRocket's own, addBuiltinMaterials()): every .orc file under it, file after file. What
    /// cannot be read is left out and recorded, never thrown: a file the loader refuses, a
    /// directory that cannot be listed, a directory without preset files (two problems for a
    /// missing directory: that it cannot be listed, then that it has no preset file).
    [[nodiscard]] static ComponentPresets readComponentPresets(
        const std::filesystem::path& directory);

    /// An environment whose motors come from @p motors and whose component presets come from
    /// @p presets, which must both outlive it and every load.
    CliEnvironment(const MotorDatabase& motors, const ComponentPresetDatabase& presets);
    CliEnvironment(const MotorDatabase&& motors, const ComponentPresetDatabase& presets)  = delete;
    CliEnvironment(const MotorDatabase& motors, const ComponentPresetDatabase&& presets)  = delete;
    CliEnvironment(const MotorDatabase&& motors, const ComponentPresetDatabase&& presets) = delete;
    ~CliEnvironment()                                                                     = default;

    CliEnvironment(const CliEnvironment&)            = delete;
    CliEnvironment& operator=(const CliEnvironment&) = delete;
    CliEnvironment(CliEnvironment&&)                 = delete;
    CliEnvironment& operator=(CliEnvironment&&)      = delete;

    /// Loads the design file @p file (GeneralRocketLoader::load(file)).
    [[nodiscard]] Result<LoadedDocument> load(const std::filesystem::path& file) const;

    /// The preference store of the loads.
    [[nodiscard]] InMemoryPreferences&       preferences() noexcept { return m_preferences; }
    [[nodiscard]] const InMemoryPreferences& preferences() const noexcept { return m_preferences; }

    /// The context the loads are made with.
    [[nodiscard]] const DocumentLoadingContext& context() const noexcept { return m_context; }

private:
    InMemoryPreferences         m_preferences;
    MaterialStorage             m_materials;
    DatabaseMotorFinder         m_motorFinder;
    SimulationExtensionRegistry m_extensions;
    DocumentLoadingContext      m_context;
};

}  // namespace QtRocket
