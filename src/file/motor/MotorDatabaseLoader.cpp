#include "QtRocket/file/motor/MotorDatabaseLoader.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <set>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The file name of @p path lower-cased as Locale.ENGLISH does for the ASCII extensions compared
/// here.
[[nodiscard]] std::string lowerCaseName(const std::filesystem::path& path)
{
    return Strings::toLower(pathToUtf8(path.filename()));
}

/// SimpleFileFilter.accept() for a file: the name ends with "." and one of @p extensions.
[[nodiscard]] bool hasExtension(const std::filesystem::path&      file,
                                std::span<const std::string_view> extensions)
{
    const std::string name = lowerCaseName(file);
    return std::ranges::any_of(extensions, [&name](std::string_view extension) {
        return name.ends_with("." + std::string(extension));
    });
}

/// isSqliteFile(): the name ends with ".db".
[[nodiscard]] bool isSqliteFile(const std::filesystem::path& file)
{
    return lowerCaseName(file).ends_with(".db");
}

/// The files DirectoryIterator finds under a directory, and the entries it could not read.
struct DirectoryListing
{
    std::vector<std::filesystem::path>        files;
    std::vector<MotorDatabaseLoader::Problem> problems;
};

/// DirectoryIterator(directory, filter, recursive = true) with DirSelectionFileFilter: every file
/// accepted by @p accept under @p directory, depth first, skipping names that start with '.'.
/// Entries are taken in name order and a directory already visited (through a link) is skipped.
void collectFiles(const std::filesystem::path& directory,
                  bool (*accept)(const std::filesystem::path&),
                  std::set<std::filesystem::path>& visited, DirectoryListing& listing)
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
        listing.problems.push_back(MotorDatabaseLoader::Problem{
            .file  = directory,
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

[[nodiscard]] DirectoryListing collectFiles(const std::filesystem::path& directory,
                                            bool (*accept)(const std::filesystem::path&))
{
    DirectoryListing                listing;
    std::set<std::filesystem::path> visited;
    collectFiles(directory, accept, visited, listing);
    return listing;
}

[[nodiscard]] bool isUserMotorFile(const std::filesystem::path& file)
{
    return hasExtension(file, MotorDatabaseLoader::kUserMotorExtensions);
}

}  // namespace

MotorDatabaseLoader::MotorDatabaseLoader() : m_loader(std::make_unique<GeneralMotorLoader>()) { }

MotorDatabaseLoader::~MotorDatabaseLoader() = default;

Result<void> MotorDatabaseLoader::loadDatabase(const std::filesystem::path& bundledDirectory,
                                               std::span<const std::filesystem::path> userFiles,
                                               const std::filesystem::path& motorLibraryDatabase)
{
    if (Result<void> internal = loadInternalMotorDatabase(bundledDirectory, motorLibraryDatabase);
        !internal)
    {
        return internal;
    }
    loadUserDefinedMotors(userFiles);
    return {};
}

Result<void> MotorDatabaseLoader::loadInternalMotorDatabase(
    const std::filesystem::path& bundledDirectory,
    const std::filesystem::path& motorLibraryDatabase)
{
    // First, try the motor library directory (where MotorDatabaseInitializer copies the database)
    std::error_code error;
    if (!motorLibraryDatabase.empty() &&
        std::filesystem::is_regular_file(motorLibraryDatabase, error))
    {
        Result<SqliteMotorDatabaseReader::Contents> contents =
            SqliteMotorDatabaseReader::readDatabase(motorLibraryDatabase);
        if (contents)
        {
            addMotors(contents->motors);
            m_skippedCurves.insert(m_skippedCurves.end(), contents->skipped.begin(),
                                   contents->skipped.end());
            return {};
        }
        // Fall through to try bundled resources
        addProblem(motorLibraryDatabase, std::move(contents.error()));
    }

    // Fall back to the bundled databases (OpenRocket logs the directories it cannot read)
    DirectoryListing listing = collectFiles(bundledDirectory, isSqliteFile);
    for (Problem& problem : listing.problems)
    {
        m_problems.push_back(std::move(problem));
    }
    if (listing.files.empty())
    {
        addProblem(bundledDirectory, Error{.code    = ErrorCode::NOT_FOUND,
                                           .message = "No SQLite motor database found in " +
                                                      pathToUtf8(bundledDirectory),
                                           .where   = std::source_location::current()});
        return {};
    }
    for (const std::filesystem::path& file : listing.files)
    {
        if (Result<void> loaded = loadSqlite(file); !loaded)
        {
            return loaded;
        }
    }
    return {};
}

void MotorDatabaseLoader::loadUserDefinedMotors(std::span<const std::filesystem::path> userFiles)
{
    for (const std::filesystem::path& file : userFiles)
    {
        std::error_code error;
        if (std::filesystem::is_regular_file(file, error))
        {
            if (!isUserMotorFile(file))
            {
                addProblem(file, Error{.code    = ErrorCode::UNSUPPORTED_FORMAT,
                                       .message = "User-defined motor file " + pathToUtf8(file) +
                                                  " does not have a supported extension",
                                       .where   = std::source_location::current()});
                continue;
            }
            loadFile(file);
        }
        else if (std::filesystem::is_directory(file, error))
        {
            loadDirectory(file);
        }
        else
        {
            addProblem(file, Error{.code    = ErrorCode::NOT_FOUND,
                                   .message = "User-defined motor file " + pathToUtf8(file) +
                                              " is neither file nor directory",
                                   .where   = std::source_location::current()});
        }
    }
}

ThrustCurveMotorSetDatabase MotorDatabaseLoader::takeDatabase()
{
    return std::exchange(m_database, ThrustCurveMotorSetDatabase());
}

Result<void> MotorDatabaseLoader::loadSqlite(const std::filesystem::path& file)
{
    Result<SqliteMotorDatabaseReader::Contents> contents =
        SqliteMotorDatabaseReader::readDatabase(file);
    if (!contents)
    {
        return std::unexpected(std::move(contents.error()));
    }
    addMotors(contents->motors);
    m_skippedCurves.insert(m_skippedCurves.end(), contents->skipped.begin(),
                           contents->skipped.end());
    return {};
}

void MotorDatabaseLoader::loadFile(const std::filesystem::path& file)
{
    if (isSqliteFile(file))
    {
        if (Result<void> loaded = loadSqlite(file); !loaded)
        {
            addProblem(file, std::move(loaded.error()));
        }
        return;
    }

    Result<std::vector<std::byte>> bytes = readFile(file);
    if (!bytes)
    {
        addProblem(file, std::move(bytes.error()));
        return;
    }
    Result<std::vector<ThrustCurveMotor::Builder>> motors =
        m_loader->load(*bytes, pathToUtf8(file.filename()));
    if (!motors)
    {
        addProblem(file, std::move(motors.error()));
        return;
    }
    if (Result<void> added = addMotorsFromBuilders(*motors); !added)
    {
        addProblem(file, std::move(added.error()));
    }
}

void MotorDatabaseLoader::loadDirectory(const std::filesystem::path& directory)
{
    DirectoryListing listing = collectFiles(directory, isUserMotorFile);
    for (Problem& problem : listing.problems)
    {
        m_problems.push_back(std::move(problem));
    }
    for (const std::filesystem::path& file : listing.files)
    {
        loadFile(file);
    }
}

void MotorDatabaseLoader::addMotors(
    const std::vector<std::shared_ptr<const ThrustCurveMotor>>& motors)
{
    for (const std::shared_ptr<const ThrustCurveMotor>& motor : motors)
    {
        m_motorCount++;
        m_database.addMotor(motor);
    }
}

Result<void> MotorDatabaseLoader::addMotorsFromBuilders(
    const std::vector<ThrustCurveMotor::Builder>& motorBuilders)
{
    for (const ThrustCurveMotor::Builder& builder : motorBuilders)
    {
        m_motorCount++;
        Result<ThrustCurveMotor> motor = builder.build();
        if (!motor)
        {
            return std::unexpected(std::move(motor.error()));
        }
        m_database.addMotor(std::make_shared<const ThrustCurveMotor>(std::move(*motor)));
    }
    return {};
}

void MotorDatabaseLoader::addProblem(const std::filesystem::path& file, Error error)
{
    m_problems.push_back(Problem{.file = file, .error = std::move(error)});
}

}  // namespace QtRocket
