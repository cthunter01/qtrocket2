#include "TestGuiServices.h"

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"

namespace QtRocket::Gui::Test
{

TestGuiServices::TestGuiServices()
  : m_motorFinder(m_motors),
    m_services(m_preferences, m_materials, m_motors, m_presets, m_context, m_prompts)
{
    m_context.setMotorFinder(&m_motorFinder);
    m_context.setApplicationMaterials(&m_materials);
    m_context.setPreferences(&m_preferences);
    m_context.setComponentPresetDatabase(&m_presets);
    m_services.setStartupData(m_startupData);
    // A new component's dialog does not open the parts library (GuiTestSupport.h).
    m_preferences.putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, false);
}

}  // namespace QtRocket::Gui::Test
