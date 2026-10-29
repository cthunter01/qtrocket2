#include "QtRocket/gui/app/DataPaths.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

namespace
{

/// The last element of the user's data directory (the user's decision U-M3-3).
constexpr std::string_view kUserDataDirectoryName = "QtRocket";

/// @p directory / @p name, or nullopt without a directory.
[[nodiscard]] std::optional<std::filesystem::path> subdirectory(
    const std::optional<std::filesystem::path>& directory, std::string_view name)
{
    if (!directory.has_value())
    {
        return std::nullopt;
    }
    return *directory / std::filesystem::path(name);
}

}  // namespace

DataPaths::DataPaths(std::filesystem::path dataDirectory)
  : m_dataDirectory(std::move(dataDirectory)), m_componentDirectory(bundledComponentDirectory())
{
}

DataPaths DataPaths::forApplication()
{
    const DataPaths paths =
        forApplicationDirectory(QtStrings::toPath(QCoreApplication::applicationDirPath()));
    if (QStandardPaths::isTestModeEnabled())
    {
        // The test seam (the class comment).
        return paths.withComponentDirectory(std::nullopt);
    }
    return paths.withUserDataDirectory(standardUserDataDirectory());
}

DataPaths DataPaths::forApplicationDirectory(const std::filesystem::path& applicationDirectory)
{
    return DataPaths((applicationDirectory / ".." / "share" / "QtRocket").lexically_normal());
}

std::optional<std::filesystem::path> DataPaths::standardUserDataDirectory()
{
    const QString location = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (location.isEmpty())
    {
        return std::nullopt;
    }
    // With the platform's separators, as Java's File has them: Qt names files with '/' on
    // Windows too.
    return QtStrings::toPath(QDir::toNativeSeparators(location)) /
           std::filesystem::path(kUserDataDirectoryName);
}

std::filesystem::path DataPaths::motorDirectory() const
{
    return m_dataDirectory / "motors";
}

std::filesystem::path DataPaths::motorDatabase() const
{
    return motorDirectory() / "initial_motors.db";
}

std::filesystem::path DataPaths::motorMetadata() const
{
    return motorDirectory() / "metadata.json";
}

std::filesystem::path DataPaths::examplesDirectory() const
{
    return m_dataDirectory / "examples";
}

std::filesystem::path DataPaths::bundledComponentDirectory() const
{
    return m_dataDirectory /
           std::filesystem::path(ComponentPresetDatabaseLoader::kBundledDirectory);
}

std::optional<std::filesystem::path> DataPaths::userThrustCurveDirectory() const
{
    return subdirectory(m_userDataDirectory, Preferences::kUserThrustCurvesDirectoryName);
}

std::optional<std::filesystem::path> DataPaths::userComponentDirectory() const
{
    return subdirectory(m_userDataDirectory, Preferences::kUserComponentPresetsDirectoryName);
}

DataPaths DataPaths::withComponentDirectory(std::optional<std::filesystem::path> directory) const
{
    DataPaths paths            = *this;
    paths.m_componentDirectory = std::move(directory);
    return paths;
}

DataPaths DataPaths::withUserDataDirectory(std::optional<std::filesystem::path> directory) const
{
    DataPaths paths           = *this;
    paths.m_userDataDirectory = std::move(directory);
    return paths;
}

}  // namespace QtRocket::Gui
