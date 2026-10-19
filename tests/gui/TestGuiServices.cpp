#include "TestGuiServices.h"

namespace QtRocket::Gui::Test
{

TestGuiServices::TestGuiServices()
  : m_motorFinder(m_motors), m_services(m_preferences, m_materials, m_motors, m_context, m_prompts)
{
    m_context.setMotorFinder(&m_motorFinder);
    m_context.setApplicationMaterials(&m_materials);
    m_context.setPreferences(&m_preferences);
}

}  // namespace QtRocket::Gui::Test
