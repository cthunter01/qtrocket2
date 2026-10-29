#include "QtRocket/gui/app/ApplicationContext.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/app/MotorDatabaseMetadataIO.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/preferences/PreferenceKeys.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/simulation/extension/SimulationExtensionRegistry.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"

namespace QtRocket::Gui
{

namespace
{

/// @p object, which must not be null (before the services take a reference to it).
template <class T>
[[nodiscard]] std::unique_ptr<T> nonNull(std::unique_ptr<T> object)
{
    QTROCKET_ASSERT(object != nullptr);
    return object;
}

/// Makes @p directory, the default directory of the user's files of the preference @p key, when
/// the preference is unset (the mkdirs() of OpenRocket's getters); a directory that cannot be made
/// is left.
void makeDefaultDirectory(const Preferences& preferences, std::string_view key,
                          const std::optional<std::filesystem::path>& directory)
{
    if (!directory.has_value() || preferences.get(key).has_value())
    {
        return;
    }
    std::error_code error;
    static_cast<void>(std::filesystem::create_directories(*directory, error));
}

/// The user's files of the preference @p key, whose default directory is @p defaultDirectory:
/// none when the preference is unset and there is no default directory, else what @p readFiles
/// reads (Preferences::getUserThrustCurveFiles() or getUserComponentPresetFiles()).
template <class ReadFiles>
[[nodiscard]] std::vector<std::filesystem::path> userFiles(
    const Preferences& preferences, std::string_view key,
    const std::optional<std::filesystem::path>& defaultDirectory, const ReadFiles& readFiles)
{
    if (!defaultDirectory.has_value() && !preferences.get(key).has_value())
    {
        return {};
    }
    return readFiles(defaultDirectory.value_or(std::filesystem::path()));
}

/// The problem of the database @p database with the file @p file and the message of @p error.
[[nodiscard]] UserPrompts::DatabaseProblem databaseProblem(UserPrompts::DatabaseKind    database,
                                                           const std::filesystem::path& file,
                                                           const Error&                 error)
{
    return {.database = database, .file = file, .message = QtStrings::fromUtf8(error.message)};
}

}  // namespace

ApplicationContext::ApplicationContext(std::unique_ptr<SettingsPreferences> preferences,
                                       DataPaths dataPaths, std::unique_ptr<UserPrompts> prompts)
  : m_preferences(nonNull(std::move(preferences))),
    m_dataPaths(std::move(dataPaths)),
    m_prompts(nonNull(std::move(prompts))),
    m_motorFinder(m_motors),
    m_extensions(SimulationExtensionRegistry::bundled()),
    m_services(*m_preferences, m_materials, m_motors, m_presets, m_loadingContext, *m_prompts)
{
    // SwingStartup.runInEDT(): the default units before any window.
    m_preferences->loadDefaultUnits();
    loadMaterials();
    // GuiModule.startLoader(): the databases.
    makeUserDirectories();
    loadMotorDatabase();
    loadComponentPresets();
    m_startupData = {.defaultUserThrustCurveDirectory = m_dataPaths.userThrustCurveDirectory(),
                     .defaultUserComponentDirectory   = m_dataPaths.userComponentDirectory(),
                     .motorDatabaseVersion =
                         MotorDatabaseMetadataIO::readDatabaseVersion(m_dataPaths.motorMetadata())};
    m_services.setStartupData(m_startupData);
    m_loadingContext.setMotorFinder(&m_motorFinder);
    m_loadingContext.setApplicationMaterials(&m_materials);
    m_loadingContext.setPreferences(m_preferences.get());
    m_loadingContext.setComponentPresetDatabase(&m_presets);
    m_loadingContext.setSimulationExtensionRegistry(&m_extensions);
}

ApplicationContext::~ApplicationContext() = default;

void ApplicationContext::loadMaterials()
{
    // Databases' static initialiser: the built-in materials, the user's, then the listener that
    // stores the user's changes. SwingPreferences.loadDefaultComponentMaterials() reaches the
    // databases first, so they are ready before it.
    addBuiltinMaterials(m_materials);
    addUserMaterials(m_materials, *m_preferences);
    m_materialChanges = storeUserMaterialChanges(m_materials, *m_preferences);
    loadDefaultComponentMaterials(*m_preferences, m_materials);
}

void ApplicationContext::makeUserDirectories() const
{
    // ApplicationPreferences.getUserThrustCurveFiles() and getUserComponentPresetFiles() make
    // their default directory when the list is unset.
    makeDefaultDirectory(*m_preferences, PreferenceKeys::kUserThrustCurvesKey,
                         m_dataPaths.userThrustCurveDirectory());
    makeDefaultDirectory(*m_preferences, PreferenceKeys::kUserComponentPresetsKey,
                         m_dataPaths.userComponentDirectory());
}

void ApplicationContext::loadMotorDatabase()
{
    // MotorDatabaseLoader.loadDatabase(): the internal database, then the user's motors.
    MotorDatabaseLoader loader;
    const Result<void>  loaded = loader.loadInternalMotorDatabase(m_dataPaths.motorDirectory());
    m_motorDatabaseProblems    = loader.getProblems();
    if (!loaded)
    {
        m_motorDatabaseProblems.push_back(
            {.file = m_dataPaths.motorDirectory(), .error = loaded.error()});
    }
    else
    {
        const std::vector<std::filesystem::path> files = userFiles(
            *m_preferences, PreferenceKeys::kUserThrustCurvesKey,
            m_dataPaths.userThrustCurveDirectory(), [this](const std::filesystem::path& directory) {
                return m_preferences->getUserThrustCurveFiles(directory);
            });
        const std::size_t bundledProblems = loader.getProblems().size();
        loader.loadUserDefinedMotors(files);
        const std::span<const MotorDatabaseLoader::Problem> problems =
            std::span(loader.getProblems()).subspan(bundledProblems);
        m_userThrustCurveProblems.assign(problems.begin(), problems.end());
        for (const MotorDatabaseLoader::Problem& problem : problems)
        {
            if (problem.refusedFile)
            {
                m_databaseProblems.push_back(databaseProblem(
                    UserPrompts::DatabaseKind::THRUST_CURVES, problem.file, problem.error));
            }
        }
    }
    m_motors = loader.takeDatabase();
}

void ApplicationContext::loadComponentPresets()
{
    // ComponentPresetDatabaseLoader.loadDatabase(): the bundled presets, then the user's.
    ComponentPresetDatabaseLoader loader(m_materials);
    if (const std::optional<std::filesystem::path>& bundled = m_dataPaths.componentDirectory();
        bundled.has_value())
    {
        loader.loadPresetComponents(*bundled);
    }
    loader.loadUserComponents(userFiles(
        *m_preferences, PreferenceKeys::kUserComponentPresetsKey,
        m_dataPaths.userComponentDirectory(), [this](const std::filesystem::path& directory) {
            return m_preferences->getUserComponentPresetFiles(directory);
        }));
    m_presets                 = loader.takeDatabase();
    m_componentPresetProblems = loader.getProblems();
    for (const ComponentPresetDatabaseLoader::Problem& problem : m_componentPresetProblems)
    {
        // showLoadingErrorDialog() of loadUserFile() and loadUserDirectory().
        if (problem.origin == ComponentPresetDatabaseLoader::Origin::USER && problem.refusedFile)
        {
            m_databaseProblems.push_back(databaseProblem(
                UserPrompts::DatabaseKind::COMPONENT_PRESETS, problem.file, problem.error));
        }
    }
}

}  // namespace QtRocket::Gui
