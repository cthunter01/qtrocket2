#pragma once

#include <array>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/motor/SqliteMotorDatabaseReader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class GeneralMotorLoader;

/// Fills a ThrustCurveMotorSetDatabase with the bundled thrust curves and the user's motor files
/// (the Qt-free core of OpenRocket's MotorDatabaseLoader). Every motor goes through
/// ThrustCurveMotorSetDatabase::addMotor(), which groups the curves into motor sets and drops a
/// curve whose digest a set already holds, in the order they are loaded: the internal database
/// first, then the user's files in the order given.
///
/// Deviations: OpenRocket loads on a background thread and shows a dialog for a user file it
/// cannot read; this loader runs in the caller's thread and records such files as problems
/// (getProblems()). It has no remote updater and no serialized (.ser) fallback. The files of a
/// directory are read in name order (Java's File.listFiles() order is unspecified), and a
/// directory reached twice through links is read once.
class MotorDatabaseLoader
{
public:
    /// A file that could not be loaded, and why. OpenRocket logs these or shows them in a dialog.
    struct Problem
    {
        std::filesystem::path file;
        Error                 error;
    };

    /// The extensions of user motor files: GeneralMotorLoader's plus "db", an SQLite database
    /// (buildUserMotorExtensions()).
    static constexpr std::array<std::string_view, 4> kUserMotorExtensions{"rse", "eng", "zip",
                                                                          "db"};

    MotorDatabaseLoader();
    MotorDatabaseLoader(const MotorDatabaseLoader&)            = delete;
    MotorDatabaseLoader(MotorDatabaseLoader&&)                 = delete;
    MotorDatabaseLoader& operator=(const MotorDatabaseLoader&) = delete;
    MotorDatabaseLoader& operator=(MotorDatabaseLoader&&)      = delete;
    ~MotorDatabaseLoader();

    /// loadDatabase(): loadInternalMotorDatabase(), then, unless it failed,
    /// loadUserDefinedMotors().
    [[nodiscard]] Result<void> loadDatabase(const std::filesystem::path&           bundledDirectory,
                                            std::span<const std::filesystem::path> userFiles,
                                            const std::filesystem::path& motorLibraryDatabase = {});

    /// Loads the internal database (loadInternalMotorDatabase()): @p motorLibraryDatabase when it
    /// is a file that reads (OpenRocket's motors.db in the motor library directory; a failure is
    /// recorded as a problem and the bundled files are used instead), else every "*.db" file
    /// under @p bundledDirectory (OpenRocket's datafiles/thrustcurves/), searched recursively and
    /// skipping names that start with '.'. A bundled file that fails to read fails the load, as
    /// OpenRocket's BugException does, keeping the motors of the files read before it; finding
    /// none is recorded as a problem (OpenRocket logs an error) and is no failure, and so is a
    /// directory under @p bundledDirectory that cannot be read.
    [[nodiscard]] Result<void> loadInternalMotorDatabase(
        const std::filesystem::path& bundledDirectory,
        const std::filesystem::path& motorLibraryDatabase = {});

    /// Loads the user's motor files (loadUserDefinedMotors(), with the files OpenRocket's
    /// preferences list): a file with one of kUserMotorExtensions (ignoring ASCII case) is read
    /// with a GeneralMotorLoader, or as a database for "db"; a directory is searched recursively
    /// for such files, skipping names that start with '.'. A file that cannot be read, has another
    /// extension, is rejected by its loader or holds a motor ThrustCurveMotor rejects is recorded
    /// as a problem; the motors of a file before a rejected one stay loaded, as in OpenRocket.
    void loadUserDefinedMotors(std::span<const std::filesystem::path> userFiles);

    /// The database loaded so far (getDatabase()).
    [[nodiscard]] const ThrustCurveMotorSetDatabase& getDatabase() const noexcept
    {
        return m_database;
    }

    /// Moves the database out, leaving an empty one.
    [[nodiscard]] ThrustCurveMotorSetDatabase takeDatabase();

    /// The number of motors handed to the database, duplicates included (motorCount).
    [[nodiscard]] int getMotorCount() const noexcept { return m_motorCount; }

    /// The files that could not be loaded, in the order they were met.
    [[nodiscard]] const std::vector<Problem>& getProblems() const noexcept { return m_problems; }

    /// The curves the databases read so far left out (SqliteMotorDatabaseReader::Contents).
    [[nodiscard]] const std::vector<SqliteMotorDatabaseReader::SkippedCurve>& getSkippedCurves()
        const noexcept
    {
        return m_skippedCurves;
    }

private:
    /// loadSqlite(): reads a database file and adds its motors.
    [[nodiscard]] Result<void> loadSqlite(const std::filesystem::path& file);

    /// loadFile(): a user file, read as a database or with m_loader.
    void loadFile(const std::filesystem::path& file);

    /// loadDirectory(): every accepted file under @p directory.
    void loadDirectory(const std::filesystem::path& directory);

    /// addMotors() / addMotorsFromBuilders().
    void addMotors(const std::vector<std::shared_ptr<const ThrustCurveMotor>>& motors);
    [[nodiscard]] Result<void> addMotorsFromBuilders(
        const std::vector<ThrustCurveMotor::Builder>& motorBuilders);

    void addProblem(const std::filesystem::path& file, Error error);

    std::unique_ptr<GeneralMotorLoader>                  m_loader;
    ThrustCurveMotorSetDatabase                          m_database;
    int                                                  m_motorCount{0};
    std::vector<Problem>                                 m_problems;
    std::vector<SqliteMotorDatabaseReader::SkippedCurve> m_skippedCurves;
};

}  // namespace QtRocket
