#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

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
///   (OpenRocket's ComponentPresetDao), loaded at start: the bundled .orc files and the user's.
/// - loadingContext(): what designs are loaded with (the motor finder over motors(), the
///   materials, the preferences, the component presets, the simulation extensions).
/// - prompts(): every modal question and message box.
/// - startupData(): what the start found that the areas below the application show (StartupData),
///   all of it none until setStartupData() is given some.
class GuiServices
{
public:
    /// What the application's start found (app/ApplicationContext fills it in).
    struct StartupData
    {
        /// The directories the preferences' lists of the user's thrust-curve and component
        /// preset files default to (Preferences::getUserThrustCurveFiles(defaultDirectory),
        /// ...; OpenRocket's getDefaultUserThrustCurveFile() and getDefaultUserComponentFile()),
        /// which the General preferences page shows; nullopt when the start has none (a test's).
        std::optional<std::filesystem::path> defaultUserThrustCurveDirectory;
        std::optional<std::filesystem::path> defaultUserComponentDirectory;
        /// The version of the bundled motor database (the database_version of its
        /// metadata.json), which the motor chooser shows ("Motor DB version:"); nullopt for none.
        std::optional<std::int64_t> motorDatabaseVersion;
    };

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

    /// What the start found: the StartupData given to setStartupData(), or one of nothing.
    [[nodiscard]] const StartupData& startupData() const noexcept
    {
        return m_startupData != nullptr ? *m_startupData : noStartupData();
    }
    /// Refers to @p data from now on, borrowed as the other objects are (it must outlive these
    /// services and their copies); the copies made before keep what they referred to.
    void setStartupData(const StartupData& data) noexcept { m_startupData = &data; }

private:
    Preferences*                       m_preferences;
    MaterialStorage*                   m_materials;
    const ThrustCurveMotorSetDatabase* m_motors;
    const ComponentPresetDatabase*     m_presets;
    const DocumentLoadingContext*      m_loadingContext;
    UserPrompts*                       m_prompts;
    const StartupData*                 m_startupData{nullptr};

    /// The StartupData of nothing.
    [[nodiscard]] static const StartupData& noStartupData() noexcept
    {
        static const StartupData kNothing;
        return kNothing;
    }
};

}  // namespace QtRocket::Gui
