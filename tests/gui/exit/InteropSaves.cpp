#include "InteropSaves.h"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>

#include <QCoreApplication>

#include <gtest/gtest.h>

#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The command-line argument that names the directory of the interop check's saves.
constexpr std::string_view kInteropSavesArgument = "--interop-saves=";

/// The directory of the saves in the build tree, when no argument names one.
constexpr std::string_view kBuildTreeDirectory = "interop-saves";

}  // namespace

std::filesystem::path interopSavesDirectory()
{
    for (const std::string& argument : ::testing::internal::GetArgvs())
    {
        if (argument.starts_with(kInteropSavesArgument))
        {
            return pathFromUtf8(std::string_view(argument).substr(kInteropSavesArgument.size()));
        }
    }
    return QtStrings::toPath(QCoreApplication::applicationDirPath()).parent_path() /
           pathFromUtf8(kBuildTreeDirectory);
}

Result<std::filesystem::path> writeInteropSave(const std::filesystem::path& file,
                                               std::string_view             name)
{
    // The directory, then the file in it.
    std::filesystem::path target = interopSavesDirectory();
    std::error_code       error;
    std::filesystem::create_directories(target, error);
    if (error)
    {
        return fail(ErrorCode::IO, std::format("{}: {}", pathToUtf8(target), error.message()));
    }
    target /= pathFromUtf8(name);
    std::filesystem::copy_file(file, target, std::filesystem::copy_options::overwrite_existing,
                               error);
    if (error)
    {
        return fail(ErrorCode::IO, std::format("{}: {}", pathToUtf8(target), error.message()));
    }
    return target;
}

}  // namespace QtRocket::Gui::Test
