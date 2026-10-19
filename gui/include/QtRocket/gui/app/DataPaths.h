#pragma once

#include <filesystem>

namespace QtRocket::Gui
{

/// Where the application finds the data it ships with (QtRocket's own; OpenRocket reads its data
/// from its jar). The layout is the installed one, as the command-line tool's
/// (src/cli/DataDirectory.cpp): the data directory <prefix>/share/QtRocket beside
/// <prefix>/bin/QtRocket, so the data directory of the running program is
/// <QCoreApplication::applicationDirPath()>/../share/QtRocket (O11). The build tree has the same
/// layout (the QtRocket_data copy of data/), and so do the archives; no path of the source tree is
/// built into the program. The GUI does not link the command-line tool's library, so it has its
/// own copy of the rule. (A macOS application bundle, Milestone 9, will keep its data elsewhere.)
///
/// The data directory holds motors/initial_motors.db (OpenRocket's motor database, the bundled
/// thrust curves) and examples/*.ork (OpenRocket's 16 example designs).
class DataPaths
{
public:
    /// The data directory @p dataDirectory.
    explicit DataPaths(std::filesystem::path dataDirectory);

    /// The data of the running program: forApplicationDirectory() of
    /// QCoreApplication::applicationDirPath(), which needs the QCoreApplication.
    [[nodiscard]] static DataPaths forApplication();

    /// The data of a program in the directory @p applicationDirectory:
    /// <applicationDirectory>/../share/QtRocket, normalised.
    [[nodiscard]] static DataPaths forApplicationDirectory(
        const std::filesystem::path& applicationDirectory);

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

    /// <data>/examples: OpenRocket's example designs (the Examples menu).
    [[nodiscard]] std::filesystem::path examplesDirectory() const;

private:
    std::filesystem::path m_dataDirectory;
};

}  // namespace QtRocket::Gui
