#include "QtRocket/gui/app/ApplicationContext.h"

#include <memory>
#include <utility>

#include "QtRocket/file/motor/MotorDatabaseLoader.h"
#include "QtRocket/gui/app/DataPaths.h"
#include "QtRocket/gui/settings/SettingsPreferences.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
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

}  // namespace

ApplicationContext::ApplicationContext(std::unique_ptr<SettingsPreferences> preferences,
                                       DataPaths dataPaths, std::unique_ptr<UserPrompts> prompts)
  : m_preferences(nonNull(std::move(preferences))),
    m_dataPaths(std::move(dataPaths)),
    m_prompts(nonNull(std::move(prompts))),
    m_motorFinder(m_motors),
    m_extensions(SimulationExtensionRegistry::bundled()),
    m_services(*m_preferences, m_materials, m_motors, m_loadingContext, *m_prompts)
{
    // SwingStartup.runInEDT(): the default units before any window.
    m_preferences->loadDefaultUnits();
    loadMaterials();
    loadMotorDatabase();
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

void ApplicationContext::loadMotorDatabase()
{
    MotorDatabaseLoader loader;
    const Result<void>  loaded = loader.loadDatabase(m_dataPaths.motorDirectory(), {});
    m_motors                   = loader.takeDatabase();
    m_motorDatabaseProblems    = loader.getProblems();
    if (!loaded)
    {
        m_motorDatabaseProblems.push_back(
            {.file = m_dataPaths.motorDirectory(), .error = loaded.error()});
    }
}

}  // namespace QtRocket::Gui
