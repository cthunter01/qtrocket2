#include "QtRocket/gui/app/DataPaths.h"

#include <filesystem>
#include <utility>

#include <QCoreApplication>

#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

DataPaths::DataPaths(std::filesystem::path dataDirectory)
  : m_dataDirectory(std::move(dataDirectory))
{
}

DataPaths DataPaths::forApplication()
{
    return forApplicationDirectory(QtStrings::toPath(QCoreApplication::applicationDirPath()));
}

DataPaths DataPaths::forApplicationDirectory(const std::filesystem::path& applicationDirectory)
{
    return DataPaths((applicationDirectory / ".." / "share" / "QtRocket").lexically_normal());
}

std::filesystem::path DataPaths::motorDirectory() const
{
    return m_dataDirectory / "motors";
}

std::filesystem::path DataPaths::motorDatabase() const
{
    return motorDirectory() / "initial_motors.db";
}

std::filesystem::path DataPaths::examplesDirectory() const
{
    return m_dataDirectory / "examples";
}

}  // namespace QtRocket::Gui
