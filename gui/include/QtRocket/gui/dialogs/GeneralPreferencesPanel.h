#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <vector>

#include <QObject>
#include <QWidget>

class QCheckBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;

namespace QtRocket
{
class OpenRocketDocument;
class Preferences;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DescriptionArea;
class UserPrompts;

/// The General page of the preferences dialog (OpenRocket's GeneralPreferencesPanel), each item
/// written to the preferences at once:
/// - "User-defined thrust curves:" and "User-defined component presets:", each a text field with
///   the list of the user's files and directories joined by ';'
///   (Preferences::getUserThrustCurveFilesAsString(), getUserComponentPresetFilesAsString(): the
///   default directory of the environment while the preference is unset), whose every change
///   stores the list it holds (split at ';', each part trimmed: Preferences::parseUserFilesText()
///   and setUserThrustCurveFiles(), setUserComponentPresetFiles()); "Add", which asks for a file
///   or a directory (UserPrompts::askOpenFiles() with OpenFileSelection::FILES_AND_DIRECTORIES,
///   titled "Add" with an "Add" button, starting in the preferences' default directory
///   (Preferences::getDefaultDirectory(), the last one a file dialog was in; the user's home
///   directory without one), Java's filters: "All thrust curve files (*.eng; *.rse; *.zip; *.db;
///   directories)" chosen, "RASP motor files (*.eng)", "RockSim engine files (*.rse)", "ZIP
///   archives (*.zip)"; "All component preset files (*.orc; directories)" chosen and "OpenRocket
///   component files (*.orc)"), appends its absolute path to the field's trimmed text after a ';'
///   and stores the dialog's directory as the preferences' default directory
///   (Preferences::setDefaultDirectory()); and "Reset", which shows the default directory of the
///   list in the field and then removes the preference (Preferences::resetUserThrustCurveFiles(),
///   ...), so that the default directory is used again. Below the thrust curves, the description
///   "Add directories, RASP motor files (*.eng), ... Changes will take effect the next time you
///   start QtRocket." (a DescriptionArea of 3 rows, 1.5 points smaller, on the window's
///   background). The lists are read at start (app/ApplicationContext), so a change takes effect
///   at the next start;
/// - a separator, then "Open last design file on startup" (AutoOpenLastDesign);
/// - "Show confirmation dialog when discarding preferences" (IgnoreDiscardPreferencesWarning),
///   which survives the dialog's Cancel;
/// - "Automatically open the Parts Library dialog" (AutoOpenPartsLibrary, on by default:
///   GuiPreferenceKeys), with Java's tooltip;
/// - "Reset all preferences" at the bottom right: asks "Reset preferences?" (UserPrompts::
///   askYesNo()); on Yes the flight configuration names of every open document of the
///   environment are pinned (OpenRocketDocument::pinFlightConfigurationNames()), every
///   preference is removed and the default units reset and stored again
///   (PreferencesStore::clearPreferences()), the message "Reload QtRocket" says that QtRocket may
///   need a restart, and preferencesCleared() tells the dialog, which then shows the preferences
///   anew (Java: PreferencesDialog.showPreferences()).
///
/// Left out: the interface language with its note and the separator below them (QtRocket is
/// English only), the update checks with their buttons (QtRocket checks for no updates: no
/// network), "Show warning when saving in RASAero format" (no RASAero format), "Show warning when
/// saving in RockSim format" (Milestone 8), and Import preferences and Export preferences
/// (OpenRocket's java.util.prefs XML files).
///
/// Deviations: the texts that name OpenRocket name QtRocket ("Reload QtRocket", the
/// description's "the next time you start QtRocket"); the message after the reset is shown at
/// once (Java: invokeLater()). The default directories are QtRocket's
/// (GuiServices::StartupData: the user data directory, QStandardPaths::GenericDataLocation +
/// "QtRocket", + "ThrustCurves" / "Components"; OpenRocket's ~/.openrocket and the like), made at
/// start by the application, never here: Java's getters make the directory when Reset or the
/// field reads it. Without a default directory (none at start, as in a test) a field shows
/// nothing while its preference is unset and Reset empties it. Add asks first, in a menu,
/// whether a file or a directory is added and then shows the platform's dialog for it (the
/// user's decision U-M3-6c: Qt's dialogs pick one of the two, Java's JFileChooser either), whose
/// filters are Java's texts followed by the patterns Qt reads where Java's own list holds more
/// than patterns ("(*.eng *.rse *.zip *.db)", "(*.orc)"). "Reset all preferences" pins the
/// configuration names first because QtRocket's configurations read the default name when they
/// are asked, where Java's store the one they were made with (O-M3-7): the reset keeps the names
/// they show, as in Java.
class GeneralPreferencesPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The documents whose flight configuration names "Reset all preferences" pins first: every
    /// open design (the WindowManager's).
    using OpenDocuments = std::function<std::vector<OpenRocketDocument*>()>;

    /// What the page needs of the application besides the preferences and the prompts.
    struct Environment
    {
        /// The directories the lists of the user's thrust-curve and component preset files
        /// default to (GuiServices::StartupData), which a field shows while its preference is
        /// unset and Reset puts in it; nullopt for none.
        std::optional<std::filesystem::path> defaultUserThrustCurveDirectory;
        std::optional<std::filesystem::path> defaultUserComponentDirectory;
        /// The open documents (an empty function: none).
        OpenDocuments openDocuments;
    };

    /// The user's file lists, each a block of the page: "User-defined thrust curves:"
    /// (UserThrustCurves) and "User-defined component presets:" (UserComponentPresets).
    enum class UserFiles
    {
        THRUST_CURVES,
        COMPONENT_PRESETS,
    };

    /// The page over @p preferences, asking through @p prompts, both of which must outlive it,
    /// with no default directories and no open documents.
    GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts, QWidget* parent);
    /// The page over @p preferences, asking through @p prompts, in @p environment.
    GeneralPreferencesPanel(Preferences& preferences, UserPrompts& prompts, Environment environment,
                            QWidget* parent);
    ~GeneralPreferencesPanel() override;
    GeneralPreferencesPanel(const GeneralPreferencesPanel&)            = delete;
    GeneralPreferencesPanel& operator=(const GeneralPreferencesPanel&) = delete;
    GeneralPreferencesPanel(GeneralPreferencesPanel&&)                 = delete;
    GeneralPreferencesPanel& operator=(GeneralPreferencesPanel&&)      = delete;

    /// The text field of the list @p files (its objectName the preference key).
    [[nodiscard]] QLineEdit* userFilesField(UserFiles files) const noexcept;
    /// Its "Add" and "Reset".
    [[nodiscard]] QPushButton* addUserFilesButton(UserFiles files) const noexcept;
    [[nodiscard]] QPushButton* resetUserFilesButton(UserFiles files) const noexcept;
    /// The description below the thrust curves' block.
    [[nodiscard]] DescriptionArea* userThrustCurvesDescription() const noexcept
    {
        return m_thrustCurvesDescription;
    }
    [[nodiscard]] QCheckBox*   openLastDesign() const noexcept { return m_openLast; }
    [[nodiscard]] QCheckBox*   showDiscardConfirmation() const noexcept { return m_discard; }
    [[nodiscard]] QCheckBox*   autoOpenPartsLibrary() const noexcept { return m_partsLibrary; }
    [[nodiscard]] QPushButton* resetAllPreferencesButton() const noexcept { return m_reset; }

    /// What "Reset all preferences" does (see the class comment): true when the preferences were
    /// reset. The page may be destroyed during its question or its message (with the window of
    /// its dialog): then nothing more is done, and preferencesCleared() is not emitted.
    bool resetAllPreferences();

Q_SIGNALS:
    /// Every preference was reset ("Reset all preferences", answered Yes).
    void preferencesCleared();

private:
    /// The widgets of one list's block.
    struct UserFilesBlock
    {
        QLineEdit*   field{nullptr};
        QPushButton* add{nullptr};
        QPushButton* reset{nullptr};
    };

    /// The label and the row of the field, Add and Reset of the list @p files, added to
    /// @p layout.
    void addUserFilesBlock(QVBoxLayout& layout, UserFiles files);
    /// Add of the list @p files (see the class comment).
    void addUserFile(UserFiles files);
    /// Reset of the list @p files.
    void resetUserFiles(UserFiles files);
    /// The block of @p files.
    [[nodiscard]] UserFilesBlock&       block(UserFiles files) noexcept;
    [[nodiscard]] const UserFilesBlock& block(UserFiles files) const noexcept;
    /// The default directory of @p files, or nullopt for none.
    [[nodiscard]] const std::optional<std::filesystem::path>& defaultDirectory(
        UserFiles files) const noexcept;

    Preferences*     m_preferences;
    UserPrompts*     m_prompts;
    Environment      m_environment;
    UserFilesBlock   m_thrustCurveFiles;
    UserFilesBlock   m_componentPresetFiles;
    DescriptionArea* m_thrustCurvesDescription{nullptr};
    QCheckBox*       m_openLast{nullptr};
    QCheckBox*       m_discard{nullptr};
    QCheckBox*       m_partsLibrary{nullptr};
    QPushButton*     m_reset{nullptr};
};

}  // namespace QtRocket::Gui
