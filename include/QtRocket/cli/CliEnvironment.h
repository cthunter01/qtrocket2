#pragma once

#include <filesystem>

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/LoadedDocument.h"
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
/// - NO component presets: QtRocket reads no .orc files before Milestone 3, so every <preset> of
///   a design gives OpenRocket's warning (the three examples with presets load with it) and the
///   component keeps the reference (RocketComponent::getUnresolvedPreset());
/// - the bundled simulation extensions (SimulationExtensionRegistry::bundled());
/// - the motors of a motor database, looked up by a DatabaseMotorFinder.
///
/// Every document the environment loaded keeps its preference store: declare the documents after
/// the environment, so that they go first. Neither copied nor moved (the context points at the
/// members).
class CliEnvironment
{
public:
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

    /// An environment whose motors come from @p motors, which must outlive it and every load.
    explicit CliEnvironment(const MotorDatabase& motors);
    explicit CliEnvironment(const MotorDatabase&& motors) = delete;
    ~CliEnvironment()                                     = default;

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
    ComponentPresetDatabase     m_presets;
    DatabaseMotorFinder         m_motorFinder;
    SimulationExtensionRegistry m_extensions;
    DocumentLoadingContext      m_context;
};

}  // namespace QtRocket
