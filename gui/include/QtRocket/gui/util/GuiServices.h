#pragma once

namespace QtRocket
{
class DocumentLoadingContext;
class MaterialStorage;
class MotorDatabase;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class UserPrompts;

/// The application's services that the areas below the application need, handed down by
/// reference (OpenRocket takes them from its global objects: Application.getPreferences(),
/// Databases, the Guice injector). Only the application (app/ApplicationContext) makes one, from
/// the objects it owns, which outlive every window; a test makes its own from test objects. A
/// plain value of borrowed references: copying it copies the references.
///
/// - preferences(): the application's preference store (the QSettings-backed one).
/// - materials(): the application's materials (the built-in and the user's), which the material
///   combo boxes list and the default component materials come from.
/// - motors(): the motor database, loaded at start.
/// - loadingContext(): what designs are loaded with (the motor finder over motors(), the
///   materials, the preferences, the component presets, the simulation extensions).
/// - prompts(): every modal question and message box.
class GuiServices
{
public:
    GuiServices(Preferences& preferences, MaterialStorage& materials, const MotorDatabase& motors,
                const DocumentLoadingContext& loadingContext, UserPrompts& prompts) noexcept
      : m_preferences(&preferences),
        m_materials(&materials),
        m_motors(&motors),
        m_loadingContext(&loadingContext),
        m_prompts(&prompts)
    {
    }

    [[nodiscard]] Preferences&         preferences() const noexcept { return *m_preferences; }
    [[nodiscard]] MaterialStorage&     materials() const noexcept { return *m_materials; }
    [[nodiscard]] const MotorDatabase& motors() const noexcept { return *m_motors; }
    [[nodiscard]] const DocumentLoadingContext& loadingContext() const noexcept
    {
        return *m_loadingContext;
    }
    [[nodiscard]] UserPrompts& prompts() const noexcept { return *m_prompts; }

private:
    Preferences*                  m_preferences;
    MaterialStorage*              m_materials;
    const MotorDatabase*          m_motors;
    const DocumentLoadingContext* m_loadingContext;
    UserPrompts*                  m_prompts;
};

}  // namespace QtRocket::Gui
