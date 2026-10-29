#pragma once

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "ScriptedUserPrompts.h"

namespace QtRocket::Gui::Test
{

/// The application's services (GuiServices) for a test, made of test objects it owns: a
/// preference store in memory, no materials, no motors, no component presets, a loading context
/// over these (no simulation extensions) and the scripted questions (ScriptedUserPrompts), which
/// the test answers through prompts(); no motor database version and no default directories of
/// the user's files (startupData()). A test may fill the motor and preset databases (motors(),
/// presets()) and the start's data before anything reads them. Declare it before the windows and
/// documents that use it.
///
/// The store holds one preference from the start: "Automatically open the Parts Library dialog"
/// (GuiPreferenceKeys::kAutoOpenPartsLibrary) is off, so that the configuration dialog of a new
/// component does not ask for a preset (see GuiTestSupport.h); a test of that question turns it
/// on (preferences().putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, true)).
class TestGuiServices
{
public:
    TestGuiServices();
    ~TestGuiServices()                                 = default;
    TestGuiServices(const TestGuiServices&)            = delete;
    TestGuiServices& operator=(const TestGuiServices&) = delete;
    TestGuiServices(TestGuiServices&&)                 = delete;
    TestGuiServices& operator=(TestGuiServices&&)      = delete;

    /// The services, which refer to the objects below.
    [[nodiscard]] const GuiServices& services() const noexcept { return m_services; }
    /// What the services' start found (GuiServices::startupData()): nothing, until the test sets
    /// some.
    [[nodiscard]] GuiServices::StartupData&    startupData() noexcept { return m_startupData; }
    [[nodiscard]] InMemoryPreferences&         preferences() noexcept { return m_preferences; }
    [[nodiscard]] ThrustCurveMotorSetDatabase& motors() noexcept { return m_motors; }
    [[nodiscard]] ComponentPresetDatabase&     presets() noexcept { return m_presets; }
    [[nodiscard]] ScriptedUserPrompts&         prompts() noexcept { return m_prompts; }
    [[nodiscard]] const ScriptedUserPrompts&   prompts() const noexcept { return m_prompts; }

private:
    InMemoryPreferences         m_preferences;
    MaterialStorage             m_materials;
    ThrustCurveMotorSetDatabase m_motors;
    ComponentPresetDatabase     m_presets;
    DatabaseMotorFinder         m_motorFinder;
    DocumentLoadingContext      m_context;
    ScriptedUserPrompts         m_prompts;
    GuiServices::StartupData    m_startupData;
    GuiServices                 m_services;
};

}  // namespace QtRocket::Gui::Test
