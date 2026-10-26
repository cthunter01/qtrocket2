#include "QtRocket/file/preset/ComponentPresetDatabaseLoader.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <source_location>
#include <span>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/file/DirectoryIterator.h"
#include "QtRocket/file/preset/OpenRocketComponentLoader.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The warning a set gets in the place of the first one beyond the bound of @p warnings.
[[nodiscard]] Warning::Other tooManyWarnings(std::size_t warnings)
{
    return Warning::fromString(std::format(
        "The component preset files gave more than {} warnings; the further ones were dropped.",
        warnings));
}

}  // namespace

ComponentPresetDatabaseLoader::ComponentPresetDatabaseLoader(const MaterialStorage& materials)
  : ComponentPresetDatabaseLoader(materials, Limits{})
{
}

ComponentPresetDatabaseLoader::ComponentPresetDatabaseLoader(const MaterialStorage& materials,
                                                             const Limits&          limits)
  : m_loader(materials, limits.file), m_limits(limits)
{
    m_warnings.setCapacity(limits.warnings, tooManyWarnings(limits.warnings));
}

void ComponentPresetDatabaseLoader::loadDatabase(const std::filesystem::path& bundledDirectory,
                                                 std::span<const std::filesystem::path> userFiles)
{
    loadPresetComponents(bundledDirectory);
    loadUserComponents(userFiles);
}

void ComponentPresetDatabaseLoader::loadPresetComponents(
    const std::filesystem::path& bundledDirectory)
{
    const std::vector<std::filesystem::path> files = findFiles(bundledDirectory, Origin::BUNDLED);
    if (files.empty())
    {
        // DirectoryIterator.findDirectory() finds nothing and logs that it could not open the
        // directory.
        addProblem(
            bundledDirectory,
            Error{.code    = ErrorCode::NOT_FOUND,
                  .message = "No component preset file found in " + pathToUtf8(bundledDirectory),
                  .where   = std::source_location::current()},
            Origin::BUNDLED, false);
        return;
    }
    for (const std::filesystem::path& file : files)
    {
        loadFile(file, Origin::BUNDLED, Found::WALKED);
    }
}

void ComponentPresetDatabaseLoader::loadUserComponents(
    std::span<const std::filesystem::path> userFiles)
{
    for (const std::filesystem::path& file : userFiles)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error) && isPresetFile(file))
        {
            loadFile(file, Origin::USER, Found::LISTED);
        }
        else if (std::filesystem::is_directory(file, error))
        {
            for (const std::filesystem::path& found : findFiles(file, Origin::USER))
            {
                loadFile(found, Origin::USER, Found::WALKED);
            }
        }
        else if (std::filesystem::is_regular_file(file, error))
        {
            addProblem(file,
                       Error{.code    = ErrorCode::UNSUPPORTED_FORMAT,
                             .message = "User-defined component preset file " + pathToUtf8(file) +
                                        " does not have the extension ." + std::string(kExtension),
                             .where   = std::source_location::current()},
                       Origin::USER, false);
        }
        else
        {
            addProblem(file,
                       Error{.code    = ErrorCode::NOT_FOUND,
                             .message = "User-defined component preset file " + pathToUtf8(file) +
                                        " is neither file nor directory",
                             .where   = std::source_location::current()},
                       Origin::USER, false);
        }
    }
}

ComponentPresetDatabase ComponentPresetDatabaseLoader::takeDatabase()
{
    return std::exchange(m_database, ComponentPresetDatabase());
}

bool ComponentPresetDatabaseLoader::isPresetFile(const std::filesystem::path& file)
{
    return Strings::toLower(pathToUtf8(file.filename())).ends_with("." + std::string(kExtension));
}

void ComponentPresetDatabaseLoader::loadFile(const std::filesystem::path& file, Origin origin,
                                             Found found)
{
    if (m_presetBoundReached || (found == Found::WALKED && !canOpenWalkedFile(file, origin)))
    {
        return;
    }
    WarningSet                                                fileWarnings;
    Result<std::vector<OpenRocketComponentLoader::PresetPtr>> presets =
        m_loader.loadFile(file, fileWarnings);
    if (!presets)
    {
        addProblem(file, std::move(presets.error()), origin, true);
        return;
    }
    if (presets->size() > m_limits.presets - m_presetCount)
    {
        m_presetBoundReached = true;
        addProblem(file,
                   Error{.code    = ErrorCode::UNSUPPORTED_FORMAT,
                         .message = std::format("The component preset files hold more than {} "
                                                "presets; {} and the files after it were not read",
                                                m_limits.presets, pathToUtf8(file)),
                         .where   = std::source_location::current()},
                   origin, false);
        return;
    }
    m_database.addAll(*presets);
    m_warnings.addAll(fileWarnings);
    m_fileCount++;
    m_presetCount += presets->size();
}

bool ComponentPresetDatabaseLoader::canOpenWalkedFile(const std::filesystem::path& file,
                                                      Origin                       origin)
{
    // A FIFO would block the open, and a device would be read without an end (OpenRocket opens
    // them all): only a regular file is opened. A broken link is no file at all, which the open
    // finds.
    std::error_code                    error;
    const std::filesystem::file_status status = std::filesystem::status(file, error);
    if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status))
    {
        addProblem(file,
                   Error{.code    = ErrorCode::UNSUPPORTED_FORMAT,
                         .message = pathToUtf8(file) + " is not a regular file; it was not read",
                         .where   = std::source_location::current()},
                   origin, false);
        return false;
    }
    // DirectoryIterator.next() opens the file and logs a failure ("Error opening file/directory
    // <file>"), never handing it to the loader.
    if (!std::ifstream(file, std::ios::binary))
    {
        addProblem(file,
                   Error{.code    = ErrorCode::IO,
                         .message = "Error opening file/directory " + pathToUtf8(file),
                         .where   = std::source_location::current()},
                   origin, false);
        return false;
    }
    return true;
}

std::vector<std::filesystem::path> ComponentPresetDatabaseLoader::findFiles(
    const std::filesystem::path& directory, Origin origin)
{
    DirectoryIterator::Listing listing =
        DirectoryIterator::listFiles(directory, isPresetFile, m_limits.directory);
    for (DirectoryIterator::Problem& problem : listing.problems)
    {
        addProblem(problem.directory, std::move(problem.error), origin, false);
    }
    return std::move(listing.files);
}

void ComponentPresetDatabaseLoader::addProblem(const std::filesystem::path& file, Error error,
                                               Origin origin, bool refusedFile)
{
    m_problems.push_back(Problem{
        .file = file, .error = std::move(error), .origin = origin, .refusedFile = refusedFile});
}

}  // namespace QtRocket
