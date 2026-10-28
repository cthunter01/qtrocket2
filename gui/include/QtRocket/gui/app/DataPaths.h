#pragma once

#include <filesystem>
#include <optional>

namespace QtRocket::Gui
{

/// Where the application finds the data it ships with (QtRocket's own; OpenRocket reads its data
/// from its jar), and where the user's own files go. The layout is the installed one, as the
/// command-line tool's (src/cli/DataDirectory.cpp): the data directory <prefix>/share/QtRocket
/// beside <prefix>/bin/QtRocket, so the data directory of the running program is
/// <QCoreApplication::applicationDirPath()>/../share/QtRocket (O11). The build tree has the same
/// layout (the QtRocket_data copy of data/), and so do the archives; no path of the source tree is
/// built into the program. The GUI does not link the command-line tool's library, so it has its
/// own copy of the rule. (A macOS application bundle, Milestone 9, will keep its data elsewhere.)
///
/// The data directory holds motors/initial_motors.db (OpenRocket's motor database, the bundled
/// thrust curves) with motors/metadata.json (its version), components/ (OpenRocket's component
/// preset files, the bundled presets) and examples/*.ork (OpenRocket's 16 example designs).
///
/// The user's data directory (OpenRocket's SystemInfo.getUserApplicationDirectory()) holds the
/// directories the preferences' lists of the user's thrust-curve and component preset files
/// default to: ThrustCurves and Components (Preferences::kUserThrustCurvesDirectoryName,
/// kUserComponentPresetsDirectoryName), which the application makes at start when their list is
/// unset (ApplicationContext). It is the platform's (standardUserDataDirectory()) for the running
/// program, and none for the other data: then the lists default to no file, and nothing is made.
///
/// The test seam: in QStandardPaths' test mode (a test process: QtRocket_gui_tests turns it on
/// for every test, GuiTestMain.cpp), forApplication() has no user's data directory and no bundled
/// presets, so that the tests that start the application as main() does read none of the user's
/// files, make no directory in the user's home (Qt's directory of that mode, ~/.qttest, lies there
/// all the same) and do not read all 31 bundled preset files (some 1.6 s in a Debug build, 7 s
/// under AddressSanitizer). A test of either names its own: withUserDataDirectory(),
/// withComponentDirectory(bundledComponentDirectory()); each can be another directory or none.
///
/// Deviations: OpenRocket's user directory is ~/.openrocket on Unix, ~/Library/Application
/// Support/OpenRocket on macOS and %APPDATA%/OpenRocket on Windows; QtRocket's is Qt's
/// GenericDataLocation and "QtRocket" (the user's decision U-M3-3): ~/.local/share/QtRocket,
/// ~/Library/Application Support/QtRocket and %LOCALAPPDATA%/QtRocket.
class DataPaths
{
public:
    /// The data directory @p dataDirectory, its bundled presets in <data>/components, without a
    /// user's data directory.
    explicit DataPaths(std::filesystem::path dataDirectory);

    /// The data of the running program: forApplicationDirectory() of
    /// QCoreApplication::applicationDirPath(), which needs the QCoreApplication, with the user's
    /// data directory standardUserDataDirectory(); in QStandardPaths' test mode, without a user's
    /// data directory and without the bundled presets (the test seam: see the class comment).
    [[nodiscard]] static DataPaths forApplication();

    /// The data of a program in the directory @p applicationDirectory:
    /// <applicationDirectory>/../share/QtRocket, normalised, without a user's data directory.
    [[nodiscard]] static DataPaths forApplicationDirectory(
        const std::filesystem::path& applicationDirectory);

    /// The platform's directory of the user's QtRocket data:
    /// QStandardPaths::writableLocation(GenericDataLocation) and "QtRocket", with the platform's
    /// separators; nullopt when Qt knows none. Nothing is made or read.
    [[nodiscard]] static std::optional<std::filesystem::path> standardUserDataDirectory();

    /// The data directory.
    [[nodiscard]] const std::filesystem::path& dataDirectory() const noexcept
    {
        return m_dataDirectory;
    }

    /// <data>/motors: the directory of the bundled motor databases (OpenRocket's
    /// datafiles/thrustcurves/), which the motor database loader reads every "*.db" file of.
    [[nodiscard]] std::filesystem::path motorDirectory() const;

    /// <data>/motors/initial_motors.db: OpenRocket's motor database.
    [[nodiscard]] std::filesystem::path motorDatabase() const;

    /// <data>/motors/metadata.json: the bundled motor database's metadata (OpenRocket's
    /// datafiles/thrustcurves/metadata.json, byte for byte), whose database_version the motor
    /// chooser shows (MotorDatabaseMetadataIO).
    [[nodiscard]] std::filesystem::path motorMetadata() const;

    /// <data>/examples: OpenRocket's example designs (the Examples menu).
    [[nodiscard]] std::filesystem::path examplesDirectory() const;

    /// <data>/components (ComponentPresetDatabaseLoader::kBundledDirectory): the bundled
    /// component presets (OpenRocket's datafiles/components).
    [[nodiscard]] std::filesystem::path bundledComponentDirectory() const;

    /// The directory of the component presets the start loads before the user's:
    /// bundledComponentDirectory() unless replaced; nullopt for none.
    [[nodiscard]] const std::optional<std::filesystem::path>& componentDirectory() const noexcept
    {
        return m_componentDirectory;
    }

    /// The user's data directory; nullopt for none.
    [[nodiscard]] const std::optional<std::filesystem::path>& userDataDirectory() const noexcept
    {
        return m_userDataDirectory;
    }

    /// <user data>/ThrustCurves: the directory the user's thrust-curve files default to
    /// (OpenRocket's getDefaultUserThrustCurveFile()); nullopt without a user's data directory.
    [[nodiscard]] std::optional<std::filesystem::path> userThrustCurveDirectory() const;

    /// <user data>/Components: the directory the user's component preset files default to
    /// (OpenRocket's getDefaultUserComponentFile()); nullopt without a user's data directory.
    [[nodiscard]] std::optional<std::filesystem::path> userComponentDirectory() const;

    /// These paths with the bundled presets in @p directory (nullopt: none).
    [[nodiscard]] DataPaths withComponentDirectory(
        std::optional<std::filesystem::path> directory) const;

    /// These paths with the user's data directory @p directory (nullopt: none).
    [[nodiscard]] DataPaths withUserDataDirectory(
        std::optional<std::filesystem::path> directory) const;

private:
    std::filesystem::path                m_dataDirectory;
    std::optional<std::filesystem::path> m_componentDirectory;
    std::optional<std::filesystem::path> m_userDataDirectory;
};

}  // namespace QtRocket::Gui
