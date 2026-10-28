#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

class SettingsPreferences;

/// The application's services, which outlive every document and window (OpenRocket's global
/// objects: Application.getPreferences(), Databases, the Guice injector of GuiModule with its
/// motor and preset providers). It owns them, makes them ready in the order of
/// SwingStartup.runInEDT() and GuiModule.startLoader(), and hands them down as the GuiServices
/// (O10), the one place the GUI gets them from:
/// 1. the preference store (the user's QSettings file, SettingsPreferences): the default units
///    are loaded from it (loadDefaultUnits()) before any window opens;
/// 2. the application's materials: OpenRocket's built-in ones, then the user's from the store
///    (addUserMaterials()), and from then on every user material added or removed is stored
///    (storeUserMaterialChanges(), Databases' listener);
/// 3. the default component materials (loadDefaultComponentMaterials(), at every start as
///    SwingPreferences does: a new nose cone is Polystyrene and new fins are Balsa, where
///    without it they would be Cardboard);
/// 4. the user's directories: the directory the user's thrust-curve files default to
///    (DataPaths::userThrustCurveDirectory(), <user data>/ThrustCurves) is made, with its
///    parents, when the preference of those files (PreferenceKeys::kUserThrustCurvesKey) is
///    unset, and so is the one of the component preset files (<user data>/Components,
///    kUserComponentPresetsKey); a directory that cannot be made is left, as OpenRocket's
///    mkdirs() leaves it (its list then names a path that is neither file nor directory);
/// 5. the motor database (MotorDatabaseLoader): the bundled one, every "*.db" of the data
///    directory's motors/, then the user's thrust-curve files and directories
///    (Preferences::getUserThrustCurveFiles() of the default directory of step 4; none without
///    one when the preference is unset), unless a bundled file failed to read, as OpenRocket's
///    loadDatabase() stops at its BugException (finding none is no failure: OpenRocket logs it
///    and goes on); what could not be read of the bundled one is kept
///    for the user (motorDatabaseProblems()), and so is what could not be read of the user's
///    (userThrustCurveProblems());
/// 6. the component presets (ComponentPresetDatabaseLoader, over the materials of step 2): the
///    bundled ones, every .orc file under DataPaths::componentDirectory() (none without one),
///    then the user's files and directories (getUserComponentPresetFiles(), as in step 5); every
///    problem is kept (componentPresetProblems());
/// 7. the bundled motor database's version (MotorDatabaseMetadataIO::readDatabaseVersion() of
///    DataPaths::motorMetadata()) and the default directories of step 4, for the motor chooser
///    and the preferences page (GuiServices::startupData());
/// 8. the bundled simulation extensions, and the loading context of the designs over the motors,
///    materials, preferences, presets and extensions, as the command-line tool's
///    (CliEnvironment);
/// 9. the questions and messages (the UserPrompts it is given: the dialogs of the application,
///    a scripted double in a test).
/// The user's files that could not be loaded and that OpenRocket shows a warning for are kept for
/// the user (databaseProblems(): the start shows them after its first window).
///
/// Deviations: OpenRocket loads the motors and the presets on threads of their own
/// (AsynchronousDatabaseLoader, started before the units and materials, with a "Loading motors"
/// dialog when a motor is wanted before they are loaded) and blocks the first use until they
/// are loaded; here everything loads before the first window, without that dialog (some 0.1 s in
/// a Release build, most of it the 31 bundled preset files), and the order does not matter, as
/// none of these reads another but the presets, which read the materials (OpenRocket's preset
/// loader reads them through Databases too). A bundled motor database that cannot be read is not
/// a bug report (OpenRocket's BugException) but a problem shown to the user, and the designs open
/// without their motors. OpenRocket's getters of the user file lists make the default directory
/// whenever they read an unset list; here only the start makes it (step 4), never a getter.
///
/// Neither copied nor moved: the services and the loading context point at the members.
class ApplicationContext
{
public:
    /// Makes the services ready (see the class comment) from the store @p preferences, the data
    /// in @p dataPaths and the questions of @p prompts (neither null). Changes the process-wide
    /// default units, so for the GUI thread only.
    ApplicationContext(std::unique_ptr<SettingsPreferences> preferences, DataPaths dataPaths,
                       std::unique_ptr<UserPrompts> prompts);
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&)            = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;
    ApplicationContext(ApplicationContext&&)                 = delete;
    ApplicationContext& operator=(ApplicationContext&&)      = delete;

    /// The services, for every window and dialog.
    [[nodiscard]] const GuiServices& services() const noexcept { return m_services; }

    /// The preference store (the services' preferences(), as the store it is: for the
    /// preferences dialog's snapshot and the like).
    [[nodiscard]] SettingsPreferences& preferences() const noexcept { return *m_preferences; }
    [[nodiscard]] const DataPaths&     dataPaths() const noexcept { return m_dataPaths; }
    [[nodiscard]] MaterialStorage&     materials() noexcept { return m_materials; }
    [[nodiscard]] const ThrustCurveMotorSetDatabase& motors() const noexcept { return m_motors; }
    [[nodiscard]] const ComponentPresetDatabase&     presets() const noexcept { return m_presets; }
    [[nodiscard]] const DocumentLoadingContext&      loadingContext() const noexcept
    {
        return m_loadingContext;
    }
    [[nodiscard]] UserPrompts& prompts() const noexcept { return *m_prompts; }

    /// What went wrong while the bundled motor database was loaded: the files and directories
    /// that could not be read (MotorDatabaseLoader::getProblems() of
    /// loadInternalMotorDatabase(), "no motor database found" among them), then the error that
    /// stopped the load, if one did; empty when every bundled motor loaded.
    [[nodiscard]] const std::vector<MotorDatabaseLoader::Problem>& motorDatabaseProblems()
        const noexcept
    {
        return m_motorDatabaseProblems;
    }

    /// What went wrong while the user's thrust-curve files were loaded
    /// (MotorDatabaseLoader::getProblems() of loadUserDefinedMotors()), in order.
    [[nodiscard]] const std::vector<MotorDatabaseLoader::Problem>& userThrustCurveProblems()
        const noexcept
    {
        return m_userThrustCurveProblems;
    }

    /// What went wrong while the component presets were loaded, the bundled and the user's
    /// (ComponentPresetDatabaseLoader::getProblems()), in order.
    [[nodiscard]] const std::vector<ComponentPresetDatabaseLoader::Problem>&
    componentPresetProblems() const noexcept
    {
        return m_componentPresetProblems;
    }

    /// The user's files that could not be loaded and for which OpenRocket shows its warning
    /// ("Error upon thrust curve import", "Error upon component preset import"), each with its
    /// loader's message: the thrust-curve files their loader refused
    /// (MotorDatabaseLoader::Problem::refusedFile), then the component preset files of the
    /// user's list or of a walk of the user's directories that could not be read or were refused
    /// (ComponentPresetDatabaseLoader::Problem::refusedFile of the USER origin). The other
    /// problems OpenRocket only logs. Empty when every user file loaded.
    [[nodiscard]] const std::vector<UserPrompts::DatabaseProblem>& databaseProblems() const noexcept
    {
        return m_databaseProblems;
    }

    /// The bundled motor database's version (step 7), nullopt when its metadata gives none.
    [[nodiscard]] std::optional<std::int64_t> motorDatabaseVersion() const noexcept
    {
        return m_startupData.motorDatabaseVersion;
    }

private:
    /// Step 2 and 3 of the class comment.
    void loadMaterials();
    /// Step 4 of the class comment.
    void makeUserDirectories() const;
    /// Step 5 of the class comment.
    void loadMotorDatabase();
    /// Step 6 of the class comment.
    void loadComponentPresets();

    /// Declared first, so that what refers to it goes first.
    std::unique_ptr<SettingsPreferences> m_preferences;
    DataPaths                            m_dataPaths;
    std::unique_ptr<UserPrompts>         m_prompts;
    MaterialStorage                      m_materials;
    /// Stores the user materials' changes; disconnected before the materials go.
    std::array<Signal<const Material&>::ScopedConnection, 2> m_materialChanges;
    ThrustCurveMotorSetDatabase                              m_motors;
    std::vector<MotorDatabaseLoader::Problem>                m_motorDatabaseProblems;
    std::vector<MotorDatabaseLoader::Problem>                m_userThrustCurveProblems;
    DatabaseMotorFinder                                      m_motorFinder;
    ComponentPresetDatabase                                  m_presets;
    std::vector<ComponentPresetDatabaseLoader::Problem>      m_componentPresetProblems;
    std::vector<UserPrompts::DatabaseProblem>                m_databaseProblems;
    SimulationExtensionRegistry                              m_extensions;
    DocumentLoadingContext                                   m_loadingContext;
    /// Declared before the services, which refer to it.
    GuiServices::StartupData m_startupData;
    GuiServices              m_services;
};

}  // namespace QtRocket::Gui
