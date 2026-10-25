#include "QtRocket/file/motor/MotorDatabaseLoader.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/file/DirectoryIterator.h"
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

/// The files DirectoryIterator::listFiles() finds under @p directory, whose problems go to
/// @p problems.
[[nodiscard]] std::vector<std::filesystem::path> collectFiles(
    const std::filesystem::path& directory, bool (*accept)(const std::filesystem::path&),
    std::vector<MotorDatabaseLoader::Problem>& problems)
{
    DirectoryIterator::Listing listing = DirectoryIterator::listFiles(directory, accept);
    for (DirectoryIterator::Problem& problem : listing.problems)
    {
        problems.push_back(MotorDatabaseLoader::Problem{.file  = std::move(problem.directory),
                                                        .error = std::move(problem.error)});
    }
    return std::move(listing.files);
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
    const std::vector<std::filesystem::path> files =
        collectFiles(bundledDirectory, isSqliteFile, m_problems);
    if (files.empty())
    {
        addProblem(bundledDirectory, Error{.code    = ErrorCode::NOT_FOUND,
                                           .message = "No SQLite motor database found in " +
                                                      pathToUtf8(bundledDirectory),
                                           .where   = std::source_location::current()});
        return {};
    }
    for (const std::filesystem::path& file : files)
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
    for (const std::filesystem::path& file : collectFiles(directory, isUserMotorFile, m_problems))
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
