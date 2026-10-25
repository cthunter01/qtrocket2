#include "QtRocket/file/DirectoryIterator.h"

#include <algorithm>
#include <filesystem>
#include <set>
#include <source_location>
#include <system_error>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket
{

namespace
{

/// The walk of listFiles() below @p directory: its entries in name order, depth first, skipping
/// names that start with '.' and a directory already in @p visited.
void collectFiles(const std::filesystem::path&         directory,
                  const DirectoryIterator::FileFilter& accept,
                  std::set<std::filesystem::path>& visited, DirectoryIterator::Listing& listing)
{
    std::error_code             error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(directory, error);
    if (!visited.insert(error ? directory : canonical).second)
    {
        return;
    }

    std::vector<std::filesystem::path>  entries;
    std::filesystem::directory_iterator iterator(directory, error);
    for (; !error && iterator != std::filesystem::directory_iterator(); iterator.increment(error))
    {
        if (!pathToUtf8(iterator->path().filename()).starts_with('.'))
        {
            entries.push_back(iterator->path());
        }
    }
    if (error)
    {
        listing.problems.push_back(DirectoryIterator::Problem{
            .directory = directory,
            .error = Error{.code    = ErrorCode::IO,
                           .message = "not a directory or IOException occurred when listing files "
                                      "from " +
                                      pathToUtf8(directory),
                           .where   = std::source_location::current()}});
        return;
    }
    std::ranges::sort(entries, {},
                      [](const std::filesystem::path& path) { return path.filename().u8string(); });

    for (const std::filesystem::path& entry : entries)
    {
        if (std::filesystem::is_directory(entry, error))
        {
            collectFiles(entry, accept, visited, listing);
        }
        else if (accept(entry))
        {
            listing.files.push_back(entry);
        }
    }
}

}  // namespace

DirectoryIterator::Listing DirectoryIterator::listFiles(const std::filesystem::path& directory,
                                                        const FileFilter&            accept)
{
    Listing                         listing;
    std::set<std::filesystem::path> visited;
    collectFiles(directory, accept, visited, listing);
    return listing;
}

}  // namespace QtRocket
