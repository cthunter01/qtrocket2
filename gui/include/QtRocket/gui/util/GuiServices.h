#pragma once

namespace QtRocket
{
class ComponentPresetDatabase;
class DocumentLoadingContext;
class MaterialStorage;
class Preferences;
class ThrustCurveMotorSetDatabase;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class UserPrompts;

/// The application's services that the areas below the application need, handed down by
/// reference (OpenRocket takes them from its global objects: Application.getPreferences(),
/// Databases, the Guice injector with its motor and preset providers). Only the application
/// (app/ApplicationContext) makes one, from the objects it owns, which outlive every window; a
/// test makes its own from test objects. A plain value of borrowed references: copying it copies
/// the references.
///
/// - preferences(): the application's preference store (the QSettings-backed one).
/// - materials(): the application's materials (the built-in and the user's), which the material
///   combo boxes list and the default component materials come from.
/// - motors(): the thrust-curve motor database, loaded at start: its motor sets are what the
///   motor chooser lists (OpenRocket's ThrustCurveMotorSetDatabase, which the motor provider
///   hands out).
/// - presets(): the component presets the parts library and the preset combo boxes list
///   (OpenRocket's ComponentPresetDao); loaded at start, empty until the bundled .orc files are
///   (Milestone 3, run 3).
/// - loadingContext(): what designs are loaded with (the motor finder over motors(), the
///   materials, the preferences, the component presets, the simulation extensions).
/// - prompts(): every modal question and message box.
class GuiServices
{
public:
    GuiServices(Preferences& preferences, MaterialStorage& materials,
                const ThrustCurveMotorSetDatabase& motors, const ComponentPresetDatabase& presets,
                const DocumentLoadingContext& loadingContext, UserPrompts& prompts) noexcept
      : m_preferences(&preferences),
        m_materials(&materials),
        m_motors(&motors),
        m_presets(&presets),
        m_loadingContext(&loadingContext),
        m_prompts(&prompts)
    {
    }

    [[nodiscard]] Preferences&     preferences() const noexcept { return *m_preferences; }
    [[nodiscard]] MaterialStorage& materials() const noexcept { return *m_materials; }
    [[nodiscard]] const ThrustCurveMotorSetDatabase& motors() const noexcept { return *m_motors; }
    [[nodiscard]] const ComponentPresetDatabase&     presets() const noexcept { return *m_presets; }
    [[nodiscard]] const DocumentLoadingContext&      loadingContext() const noexcept
    {
        return *m_loadingContext;
    }
    [[nodiscard]] UserPrompts& prompts() const noexcept { return *m_prompts; }

private:
    Preferences*                       m_preferences;
    MaterialStorage*                   m_materials;
    const ThrustCurveMotorSetDatabase* m_motors;
    const ComponentPresetDatabase*     m_presets;
    const DocumentLoadingContext*      m_loadingContext;
    UserPrompts*                       m_prompts;
};

}  // namespace QtRocket::Gui
