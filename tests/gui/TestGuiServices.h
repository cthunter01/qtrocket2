#pragma once

#include "QtRocket/file/DatabaseMotorFinder.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "ScriptedUserPrompts.h"

namespace QtRocket::Gui::Test
{

/// The application's services (GuiServices) for a test, made of test objects it owns: a
/// preference store in memory, no materials, no motors, a loading context over these (no
/// component presets, no simulation extensions) and the scripted questions (ScriptedUserPrompts),
/// which the test answers through prompts(). Declare it before the windows and documents that
/// use it.
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
    [[nodiscard]] const GuiServices&         services() const noexcept { return m_services; }
    [[nodiscard]] InMemoryPreferences&       preferences() noexcept { return m_preferences; }
    [[nodiscard]] ScriptedUserPrompts&       prompts() noexcept { return m_prompts; }
    [[nodiscard]] const ScriptedUserPrompts& prompts() const noexcept { return m_prompts; }

private:
    InMemoryPreferences         m_preferences;
    MaterialStorage             m_materials;
    ThrustCurveMotorSetDatabase m_motors;
    DatabaseMotorFinder         m_motorFinder;
    DocumentLoadingContext      m_context;
    ScriptedUserPrompts         m_prompts;
    GuiServices                 m_services;
};

}  // namespace QtRocket::Gui::Test
