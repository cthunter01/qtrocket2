#include "QtRocket/file/DirectoryIterator.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <set>
#include <source_location>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket
{

namespace
{

/// One walk of listFiles(): its filter, its bounds and its start, which must outlive it, the
/// directories it has entered and what it found.
class Walk
{
public:
    Walk(const DirectoryIterator::FileFilter& accept, const DirectoryIterator::Limits& limits,
         const std::filesystem::path& start)
      : m_accept(&accept), m_limits(&limits), m_start(&start)
    {
    }

    /// The walk below @p directory, at @p depth below the start: its entries in name order,
    /// depth first, skipping names that start with '.' and a directory already visited. False
    /// once the walk has ended at its bound of files.
    [[nodiscard]] bool collect(const std::filesystem::path& directory, std::size_t depth)
    {
        std::error_code             error;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(directory, error);
        if (!m_visited.insert(error ? directory : canonical).second)
        {
            return true;
        }

        std::vector<std::filesystem::path>  entries;
        std::filesystem::directory_iterator iterator(directory, error);
        for (; !error && iterator != std::filesystem::directory_iterator();
             iterator.increment(error))
        {
            if (!pathToUtf8(iterator->path().filename()).starts_with('.'))
            {
                entries.push_back(iterator->path());
            }
        }
        if (error)
        {
            addProblem(directory, ErrorCode::IO,
                       "not a directory or IOException occurred when listing files from " +
                           pathToUtf8(directory));
            return true;
        }
        std::ranges::sort(entries, {}, [](const std::filesystem::path& path) {
            return path.filename().u8string();
        });

        for (const std::filesystem::path& entry : entries)
        {
            if (std::filesystem::is_directory(entry, error))
            {
                if (depth >= m_limits->depth)
                {
                    addProblem(entry, ErrorCode::UNSUPPORTED_FORMAT,
                               std::format("{} is more than {} directories deep; it was not "
                                           "searched",
                                           pathToUtf8(entry), m_limits->depth));
                }
                else if (!collect(entry, depth + 1))
                {
                    return false;
                }
            }
            else if ((*m_accept)(entry))
            {
                if (m_listing.files.size() >= m_limits->files)
                {
                    addProblem(*m_start, ErrorCode::UNSUPPORTED_FORMAT,
                               std::format("{} holds more than {} files; the first {} were taken",
                                           pathToUtf8(*m_start), m_limits->files, m_limits->files));
                    return false;
                }
                m_listing.files.push_back(entry);
            }
        }
        return true;
    }

    /// What the walk found.
    [[nodiscard]] DirectoryIterator::Listing takeListing() { return std::move(m_listing); }

private:
    void addProblem(const std::filesystem::path& directory, ErrorCode code, std::string message,
                    std::source_location where = std::source_location::current())
    {
        m_listing.problems.push_back(DirectoryIterator::Problem{
            .directory = directory,
            .error     = Error{.code = code, .message = std::move(message), .where = where}});
    }

    const DirectoryIterator::FileFilter* m_accept;
    const DirectoryIterator::Limits*     m_limits;
    const std::filesystem::path*         m_start;
    std::set<std::filesystem::path>      m_visited;
    DirectoryIterator::Listing           m_listing;
};

}  // namespace

DirectoryIterator::Listing DirectoryIterator::listFiles(const std::filesystem::path& directory,
                                                        const FileFilter&            accept,
                                                        const Limits&                limits)
{
    Walk walk(accept, limits, directory);
    static_cast<void>(walk.collect(directory, 0));
    return walk.takeListing();
}

DirectoryIterator::Listing DirectoryIterator::listFiles(const std::filesystem::path& directory,
                                                        const FileFilter&            accept)
{
    return listFiles(directory, accept, Limits{});
}

}  // namespace QtRocket
