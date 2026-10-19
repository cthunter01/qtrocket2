#pragma once

#include <array>
#include <memory>
#include <vector>

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

class SettingsPreferences;
class UserPrompts;

/// The application's services, which outlive every document and window (OpenRocket's global
/// objects: Application.getPreferences(), Databases, the Guice injector of GuiModule with its
/// motor and preset providers). It owns them, makes them ready in the order of
/// SwingStartup.runInEDT(), and hands them down as the GuiServices (O10), the one place the GUI
/// gets them from:
/// 1. the preference store (the user's QSettings file, SettingsPreferences): the default units
///    are loaded from it (loadDefaultUnits()) before any window opens;
/// 2. the application's materials: OpenRocket's built-in ones, then the user's from the store
///    (addUserMaterials()), and from then on every user material added or removed is stored
///    (storeUserMaterialChanges(), Databases' listener);
/// 3. the default component materials (loadDefaultComponentMaterials(), at every start as
///    SwingPreferences does: a new nose cone is Polystyrene and new fins are Balsa, where
///    without it they would be Cardboard);
/// 4. the motor database: every "*.db" of the data directory's motors/ (MotorDatabaseLoader,
///    synchronously: 0.03 s in a Release build, so no splash and no loading thread, O15);
///    what could not be read is kept for the user (motorDatabaseProblems());
/// 5. an empty component preset database (the .orc files come in Milestone 3), the bundled
///    simulation extensions, and the loading context of the designs over all of these, as the
///    command-line tool's (CliEnvironment);
/// 6. the questions and messages (the UserPrompts it is given: the dialogs of the application,
///    a scripted double in a test).
///
/// Deviations: OpenRocket loads the motors on a thread of their own, started before the units
/// and materials, and blocks the first use until they are loaded; the order does not matter, as
/// none of these reads another. The user's thrust-curve files (the motor library preference) are
/// not read before Milestone 3. A bundled motor database that cannot be read is not a bug report
/// (OpenRocket's BugException) but a problem shown to the user, and the designs open without
/// their motors.
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
    [[nodiscard]] const DocumentLoadingContext&      loadingContext() const noexcept
    {
        return m_loadingContext;
    }
    [[nodiscard]] UserPrompts& prompts() const noexcept { return *m_prompts; }

    /// What went wrong while the motor database was loaded: the files and directories that
    /// could not be read (MotorDatabaseLoader::getProblems(), "no motor database found" among
    /// them), then the error that stopped the load, if one did; empty when every motor loaded.
    [[nodiscard]] const std::vector<MotorDatabaseLoader::Problem>& motorDatabaseProblems()
        const noexcept
    {
        return m_motorDatabaseProblems;
    }

private:
    /// Step 2 and 3 of the class comment.
    void loadMaterials();
    /// Step 4 of the class comment.
    void loadMotorDatabase();

    /// Declared first, so that what refers to it goes first.
    std::unique_ptr<SettingsPreferences> m_preferences;
    DataPaths                            m_dataPaths;
    std::unique_ptr<UserPrompts>         m_prompts;
    MaterialStorage                      m_materials;
    /// Stores the user materials' changes; disconnected before the materials go.
    std::array<Signal<const Material&>::ScopedConnection, 2> m_materialChanges;
    ThrustCurveMotorSetDatabase                              m_motors;
    std::vector<MotorDatabaseLoader::Problem>                m_motorDatabaseProblems;
    DatabaseMotorFinder                                      m_motorFinder;
    ComponentPresetDatabase                                  m_presets;
    SimulationExtensionRegistry                              m_extensions;
    DocumentLoadingContext                                   m_loadingContext;
    GuiServices                                              m_services;
};

}  // namespace QtRocket::Gui
