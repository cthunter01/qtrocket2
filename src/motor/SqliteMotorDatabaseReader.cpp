#include "QtRocket/motor/SqliteMotorDatabaseReader.h"

#include <sqlite3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

// ---------------------------------------------------------------- SQLite C API wrappers

/// How long a connection waits for a lock another connection holds: sqlite-jdbc's default
/// busy_timeout (SQLiteConfig, Pragma.BUSY_TIMEOUT), under which OpenRocket reads its databases.
constexpr int kBusyTimeoutMs = 3000;

struct DatabaseCloser
{
    // close_v2 defers the close until every statement is finalized.
    void operator()(sqlite3* db) const noexcept { sqlite3_close_v2(db); }
};

struct StatementFinalizer
{
    void operator()(sqlite3_stmt* statement) const noexcept { sqlite3_finalize(statement); }
};

/// A failure carrying SQLite's message for the last error on @p db.
[[nodiscard]] std::unexpected<Error> sqliteError(sqlite3* db)
{
    return fail(ErrorCode::DATABASE, db == nullptr ? std::string("SQLite: out of memory")
                                                   : std::string(sqlite3_errmsg(db)));
}

/// The failure of @p result, passed on.
template <class T>
[[nodiscard]] std::unexpected<Error> propagate(Result<T>& result)
{
    return std::unexpected(std::move(result.error()));
}

/// A prepared statement (sqlite3_stmt), finalized when destroyed.
class Statement
{
public:
    Statement(sqlite3* db, sqlite3_stmt* statement) noexcept : m_db(db), m_statement(statement) { }

    /// Steps once: true for a row, false when done.
    [[nodiscard]] Result<bool> step()
    {
        const int status = sqlite3_step(m_statement.get());
        if (status == SQLITE_ROW)
        {
            return true;
        }
        if (status == SQLITE_DONE)
        {
            return false;
        }
        return sqliteError(m_db);
    }

    /// Steps to the end (for statements whose rows do not matter).
    [[nodiscard]] Result<void> run()
    {
        while (true)
        {
            Result<bool> row = step();
            if (!row)
            {
                return propagate(row);
            }
            if (!*row)
            {
                return {};
            }
        }
    }

    /// Resets the statement for a new execution (the bindings are kept until replaced).
    void reset() noexcept { sqlite3_reset(m_statement.get()); }

    [[nodiscard]] Result<void> bind(int index, int value)
    {
        return check(sqlite3_bind_int(m_statement.get(), index, value));
    }

    [[nodiscard]] Result<void> bind(int index, double value)
    {
        return check(sqlite3_bind_double(m_statement.get(), index, value));
    }

    /// Binds text, or NULL for nullopt. The statement keeps its own copy of the text alive until
    /// the parameter is bound again, so SQLite need not copy it (the null destructor is
    /// SQLITE_STATIC; SQLITE_TRANSIENT is a cast macro).
    [[nodiscard]] Result<void> bind(int index, const std::optional<std::string>& value)
    {
        if (!value.has_value())
        {
            return bindNull(index);
        }
        if (value->size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return fail(ErrorCode::DATABASE, "SQLite: string or blob too big");
        }
        std::string& text = m_texts[index];
        text              = *value;
        return check(sqlite3_bind_text(m_statement.get(), index, text.c_str(),
                                       static_cast<int>(text.size()), nullptr));
    }

    [[nodiscard]] Result<void> bindNull(int index)
    {
        return check(sqlite3_bind_null(m_statement.get(), index));
    }

    /// The storage class of a result column (SQLITE_INTEGER, SQLITE_FLOAT, SQLITE_TEXT,
    /// SQLITE_BLOB or SQLITE_NULL).
    [[nodiscard]] int columnType(int column) const noexcept
    {
        return sqlite3_column_type(m_statement.get(), column);
    }

    /// ResultSet.getString(): the column as text (SQLite converts numbers), or nullopt for NULL.
    /// sqlite-jdbc decodes SQLite's bytes as UTF-8 with each malformed sequence replaced by
    /// U+FFFD, so text stored with invalid UTF-8 reads the same here (Strings::toValidUtf8).
    [[nodiscard]] std::optional<std::string> columnText(int column) const
    {
        if (columnType(column) == SQLITE_NULL)
        {
            return std::nullopt;
        }
        const unsigned char* text  = sqlite3_column_text(m_statement.get(), column);
        const int            bytes = sqlite3_column_bytes(m_statement.get(), column);
        if (text == nullptr)
        {
            return std::string();
        }
        // The C boundary: SQLite hands out its UTF-8 text as unsigned char.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        const auto* characters = reinterpret_cast<const char*>(text);
        return Strings::toValidUtf8(std::string_view(characters, static_cast<std::size_t>(bytes)));
    }

    /// ResultSet.getInt(): the column as a 32-bit integer (0 for NULL).
    [[nodiscard]] int columnInt(int column) const noexcept
    {
        return sqlite3_column_int(m_statement.get(), column);
    }

    [[nodiscard]] std::int64_t columnInt64(int column) const noexcept
    {
        return sqlite3_column_int64(m_statement.get(), column);
    }

    /// ResultSet.getDouble(): the column as a double (0 for NULL).
    [[nodiscard]] double columnDouble(int column) const noexcept
    {
        return sqlite3_column_double(m_statement.get(), column);
    }

    /// The name of a result column.
    [[nodiscard]] std::string_view columnName(int column) const noexcept
    {
        const char* name = sqlite3_column_name(m_statement.get(), column);
        return name == nullptr ? std::string_view() : std::string_view(name);
    }

    [[nodiscard]] int columnCount() const noexcept
    {
        return sqlite3_column_count(m_statement.get());
    }

private:
    [[nodiscard]] Result<void> check(int status) const
    {
        if (status != SQLITE_OK)
        {
            return sqliteError(m_db);
        }
        return {};
    }

    sqlite3*                                          m_db;
    std::unique_ptr<sqlite3_stmt, StatementFinalizer> m_statement;
    std::map<int, std::string>                        m_texts;
};

/// A database connection (sqlite3), closed when destroyed.
class Connection
{
public:
    /// Opens @p file read-only, or read-write and created when missing, and enables foreign keys
    /// as OpenRocket's openConnection() does.
    [[nodiscard]] static Result<Connection> open(const std::filesystem::path& file, bool writable)
    {
        sqlite3*  raw = nullptr;
        const int flags =
            writable ? (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE) : SQLITE_OPEN_READONLY;
        const int  status = sqlite3_open_v2(pathToUtf8(file).c_str(), &raw, flags, nullptr);
        Connection connection(raw);
        if (status != SQLITE_OK)
        {
            return sqliteError(raw);
        }
        // A database another connection holds locked is waited for, as long as sqlite-jdbc's
        // default busy_timeout, before SQLite gives up with "database is locked".
        sqlite3_busy_timeout(raw, kBusyTimeoutMs);
        if (Result<void> pragma = connection.execute("PRAGMA foreign_keys = ON"); !pragma)
        {
            return propagate(pragma);
        }
        return connection;
    }

    [[nodiscard]] Result<Statement> prepare(const std::string& sql) const
    {
        sqlite3_stmt* statement = nullptr;
        if (sqlite3_prepare_v2(m_db.get(), sql.c_str(), -1, &statement, nullptr) != SQLITE_OK)
        {
            sqlite3_finalize(statement);
            return sqliteError(m_db.get());
        }
        return Statement(m_db.get(), statement);
    }

    /// Runs @p sql, whose rows do not matter.
    [[nodiscard]] Result<void> execute(const std::string& sql) const
    {
        Result<Statement> statement = prepare(sql);
        if (!statement)
        {
            return propagate(statement);
        }
        return statement->run();
    }

    /// The rowid of the last row inserted (Statement.getGeneratedKeys(), read with getInt).
    [[nodiscard]] int lastInsertId() const noexcept
    {
        return static_cast<int>(sqlite3_last_insert_rowid(m_db.get()));
    }

private:
    explicit Connection(sqlite3* db) noexcept : m_db(db) { }

    std::unique_ptr<sqlite3, DatabaseCloser> m_db;
};

/// Binds a string parameter (never NULL).
[[nodiscard]] Result<void> bindText(Statement& statement, int index, std::string_view value)
{
    return statement.bind(index, std::optional<std::string>(value));
}

// ---------------------------------------------------------------- schema validation

/// The five tables every motor database has, in the order OpenRocket checks them.
constexpr std::array<std::string_view, 5> kTables{"meta", "manufacturers", "motors",
                                                  "thrust_curves", "thrust_data"};

constexpr std::array<std::string_view, 2>  kMetaColumns{"key", "value"};
constexpr std::array<std::string_view, 3>  kManufacturerColumns{"id", "name", "abbrev"};
constexpr std::array<std::string_view, 12> kCurveColumns{
    "id",       "motor_id", "tc_simfile_id", "source",     "format",     "license",
    "info_url", "data_url", "total_impulse", "avg_thrust", "max_thrust", "burn_time"};
constexpr std::array<std::string_view, 4> kDataColumns{"id", "curve_id", "time_seconds",
                                                       "force_newtons"};
/// The motors columns of schema version 2; version 3 adds kMotorColumnsV3Extra.
constexpr std::array<std::string_view, 22> kMotorColumnsV2{"id",
                                                           "manufacturer_id",
                                                           "tc_motor_id",
                                                           "designation",
                                                           "common_name",
                                                           "impulse_class",
                                                           "diameter",
                                                           "length",
                                                           "total_impulse",
                                                           "avg_thrust",
                                                           "max_thrust",
                                                           "burn_time",
                                                           "propellant_weight",
                                                           "total_weight",
                                                           "type",
                                                           "delays",
                                                           "case_info",
                                                           "prop_info",
                                                           "sparky",
                                                           "info_url",
                                                           "data_files",
                                                           "updated_on"};
constexpr std::array<std::string_view, 2>  kMotorColumnsV3Extra{"description", "source"};

[[nodiscard]] Result<bool> tableExists(const Connection& connection, std::string_view tableName)
{
    Result<Statement> statement =
        connection.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name = ?");
    if (!statement)
    {
        return propagate(statement);
    }
    if (Result<void> bound = bindText(*statement, 1, tableName); !bound)
    {
        return propagate(bound);
    }
    return statement->step();
}

[[nodiscard]] Result<int> readSchemaVersion(const Connection& connection)
{
    constexpr std::string_view kMissing = "SQLite motor database missing schema_version metadata";
    Result<Statement> statement = connection.prepare("SELECT value FROM meta WHERE key = ?");
    if (!statement)
    {
        return propagate(statement);
    }
    if (Result<void> bound = bindText(*statement, 1, "schema_version"); !bound)
    {
        return propagate(bound);
    }
    Result<bool> row = statement->step();
    if (!row)
    {
        return propagate(row);
    }
    if (!*row)
    {
        return fail(ErrorCode::DATABASE, std::string(kMissing));
    }
    const std::optional<std::string> value = statement->columnText(0);
    if (!value.has_value())
    {
        return fail(ErrorCode::DATABASE, std::string(kMissing));
    }
    const std::optional<int> version = Strings::parseInt(*value);  // Integer.parseInt
    if (!version.has_value())
    {
        return fail(ErrorCode::DATABASE,
                    "SQLite motor database has invalid schema_version metadata");
    }
    return *version;
}

/// The column names PRAGMA table_info gives for @p tableName.
[[nodiscard]] Result<std::vector<std::string>> columnNames(const Connection& connection,
                                                           std::string_view  tableName)
{
    Result<Statement> statement =
        connection.prepare("PRAGMA table_info(" + std::string(tableName) + ")");
    if (!statement)
    {
        return propagate(statement);
    }
    int nameColumn = -1;
    for (int column = 0; column < statement->columnCount(); column++)
    {
        if (statement->columnName(column) == "name")
        {
            nameColumn = column;
        }
    }
    std::vector<std::string> names;
    while (true)
    {
        Result<bool> row = statement->step();
        if (!row)
        {
            return propagate(row);
        }
        if (!*row)
        {
            return names;
        }
        if (nameColumn >= 0)
        {
            names.push_back(statement->columnText(nameColumn).value_or("null"));
        }
    }
}

/// requireColumns(): fails when @p tableName lacks any of the columns in @p requiredColumns.
[[nodiscard]] Result<void> requireColumns(
    const Connection& connection, std::string_view tableName,
    std::initializer_list<std::span<const std::string_view>> requiredColumns)
{
    Result<std::vector<std::string>> existing = columnNames(connection, tableName);
    if (!existing)
    {
        return propagate(existing);
    }
    std::vector<std::string_view> missing;
    for (const std::span<const std::string_view> columns : requiredColumns)
    {
        for (const std::string_view column : columns)
        {
            if (std::ranges::find(*existing, column) == existing->end())
            {
                missing.push_back(column);
            }
        }
    }
    if (!missing.empty())
    {
        // The list as Java's List.toString() writes it: "[a, b]".
        return fail(ErrorCode::DATABASE,
                    std::format("SQLite motor database table '{}' missing required columns: [{}]",
                                tableName, Strings::join(", ", missing)));
    }
    return {};
}

/// validateSchema(): the tables, the schema version and the columns; returns the version.
[[nodiscard]] Result<int> validateSchema(const Connection& connection)
{
    for (const std::string_view table : kTables)
    {
        Result<bool> exists = tableExists(connection, table);
        if (!exists)
        {
            return propagate(exists);
        }
        if (!*exists)
        {
            return fail(ErrorCode::DATABASE,
                        std::format("SQLite motor database missing {} table", table));
        }
    }

    Result<int> schemaVersion = readSchemaVersion(connection);
    if (!schemaVersion)
    {
        return schemaVersion;
    }
    if (*schemaVersion < SqliteMotorDatabaseReader::kMinSupportedSchemaVersion ||
        *schemaVersion > SqliteMotorDatabaseReader::kSchemaVersion)
    {
        return fail(
            ErrorCode::DATABASE,
            std::format("Unsupported thrust curve database schema version: {} "
                        "(supported {}-{})",
                        *schemaVersion, SqliteMotorDatabaseReader::kMinSupportedSchemaVersion,
                        SqliteMotorDatabaseReader::kSchemaVersion));
    }

    const std::array<std::pair<std::string_view, std::span<const std::string_view>>, 4> tables{{
        {"meta", kMetaColumns},
        {"manufacturers", kManufacturerColumns},
        {"thrust_curves", kCurveColumns},
        {"thrust_data", kDataColumns},
    }};
    for (const auto& [table, columns] : tables)
    {
        if (Result<void> required = requireColumns(connection, table, {columns}); !required)
        {
            return propagate(required);
        }
    }

    // Schema v2: no description/source columns.
    Result<void> motors =
        *schemaVersion >= 3
            ? requireColumns(connection, "motors", {kMotorColumnsV2, kMotorColumnsV3Extra})
            : requireColumns(connection, "motors", {kMotorColumnsV2});
    if (!motors)
    {
        return propagate(motors);
    }
    return schemaVersion;
}

/// validateIntegrity(): the SQLite page structure and every declared foreign key.
[[nodiscard]] Result<void> validateIntegrity(const Connection& connection)
{
    {
        Result<Statement> statement = connection.prepare("PRAGMA integrity_check");
        if (!statement)
        {
            return propagate(statement);
        }
        Result<bool> row = statement->step();
        if (!row)
        {
            return propagate(row);
        }
        const std::optional<std::string> result =
            *row ? statement->columnText(0) : std::optional<std::string>();
        const bool   ok   = result.has_value() && Strings::javaEqualsIgnoreCase(*result, "ok");
        Result<bool> more = ok ? statement->step() : Result<bool>(false);
        if (!more)
        {
            return propagate(more);
        }
        if (!ok || *more)
        {
            return fail(ErrorCode::DATABASE, "SQLite motor database failed integrity_check");
        }
    }

    Result<Statement> statement = connection.prepare("PRAGMA foreign_key_check");
    if (!statement)
    {
        return propagate(statement);
    }
    Result<bool> row = statement->step();
    if (!row)
    {
        return propagate(row);
    }
    if (*row)
    {
        return fail(ErrorCode::DATABASE,
                    "SQLite motor database failed foreign_key_check for table " +
                        statement->columnText(0).value_or("null"));
    }
    return {};
}

/// columnExists(): whether @p tableName has a column @p columnName. OpenRocket asks the JDBC
/// metadata, which matches the name as a LIKE pattern and so ignores ASCII case.
[[nodiscard]] Result<bool> columnExists(const Connection& connection, std::string_view tableName,
                                        std::string_view columnName)
{
    Result<std::vector<std::string>> existing = columnNames(connection, tableName);
    if (!existing)
    {
        return propagate(existing);
    }
    return std::ranges::any_of(*existing, [columnName](const std::string& name) {
        return Strings::equalsIgnoreAsciiCase(name, columnName);
    });
}

/// Opens @p dbFile for reading and validates it, as readDatabase() and validateDatabase() do.
[[nodiscard]] Result<Connection> openValidated(const std::filesystem::path& dbFile)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(dbFile, error))
    {
        return fail(ErrorCode::DATABASE, "SQLite motor database not found: " + pathToUtf8(dbFile));
    }
    Result<Connection> connection = Connection::open(dbFile, false);
    if (!connection)
    {
        return connection;
    }
    if (Result<int> schema = validateSchema(*connection); !schema)
    {
        return propagate(schema);
    }
    if (Result<void> integrity = validateIntegrity(*connection); !integrity)
    {
        return propagate(integrity);
    }
    return connection;
}

// ---------------------------------------------------------------- reading motors

/// getDoubleOrDefault(): the column as a number (an integer or a real, or text that
/// Double.parseDouble reads), else @p defaultValue.
[[nodiscard]] double getDoubleOrDefault(const Statement& row, int column, double defaultValue)
{
    switch (row.columnType(column))
    {
        case SQLITE_INTEGER:
            return static_cast<double>(row.columnInt64(column));
        case SQLITE_FLOAT:
            return row.columnDouble(column);
        case SQLITE_TEXT:
            return Strings::javaParseDouble(row.columnText(column).value_or(std::string()))
                .value_or(defaultValue);
        default:  // NULL, or a BLOB (a byte[], whose toString() is no number)
            return defaultValue;
    }
}

/// getIntegerOrNull(): the column as an int (Number.intValue() of the integer or real, or text
/// that Integer.parseInt reads), else nullopt.
[[nodiscard]] std::optional<int> getIntegerOrNull(const Statement& row, int column)
{
    switch (row.columnType(column))
    {
        case SQLITE_INTEGER:
            // Long.intValue() keeps the low 32 bits.
            return static_cast<std::int32_t>(static_cast<std::uint32_t>(row.columnInt64(column)));
        case SQLITE_FLOAT:
            return MathUtil::javaIntCast(row.columnDouble(column));
        case SQLITE_TEXT:
            return Strings::parseInt(row.columnText(column).value_or(std::string()));
        default:
            return std::nullopt;
    }
}

/// getIntOrDefault().
[[nodiscard]] int getIntOrDefault(const Statement& row, int column, int defaultValue)
{
    return getIntegerOrNull(row, column).value_or(defaultValue);
}

/// parseMotorType().
[[nodiscard]] Motor::Type parseMotorType(const std::optional<std::string>& typeCode)
{
    if (!typeCode.has_value() || Strings::isEmpty(*typeCode))
    {
        return Motor::Type::UNKNOWN;
    }
    // Java lower-cases in the default locale; only ASCII letters can become these names, so
    // lower-casing ASCII letters decides the same.
    const std::string code = Strings::toLower(*typeCode);
    if (code == "su" || code == "single" || code == "single-use")
    {
        return Motor::Type::SINGLE;
    }
    if (code == "re" || code == "reload")
    {
        return Motor::Type::RELOAD;
    }
    if (code == "hy" || code == "hybrid")
    {
        return Motor::Type::HYBRID;
    }
    return Motor::Type::UNKNOWN;
}

/// chooseManufacturerName(): the abbreviation, else the name, trimmed, else "Unknown".
[[nodiscard]] std::string chooseManufacturerName(const std::optional<std::string>& name,
                                                 const std::optional<std::string>& abbrev)
{
    for (const std::optional<std::string>* candidate : {&abbrev, &name})
    {
        if (candidate->has_value())
        {
            const std::string_view trimmed = Strings::trim(**candidate);
            if (!trimmed.empty())
            {
                return std::string(trimmed);
            }
        }
    }
    return "Unknown";
}

/// Thrust data normalised to meet ThrustCurveMotor's requirements (NormalizedThrustData).
struct NormalizedThrustData
{
    std::vector<double> timePoints;
    std::vector<double> thrustPoints;
};

/// normalizeThrustData(): starts at time 0, strictly increasing times, at least two points,
/// non-negative thrust; nullopt when fewer than two points remain.
[[nodiscard]] std::optional<NormalizedThrustData> normalizeThrustData(
    std::span<const double> timePoints, std::span<const double> thrustPoints)
{
    if (timePoints.size() < 2)
    {
        return std::nullopt;
    }

    constexpr double     kTimeEpsilon = 0.0001;
    NormalizedThrustData normalized;

    // Always ensure the curve starts at t=0
    const double firstTime = timePoints[0];
    if (std::abs(firstTime) > kTimeEpsilon)
    {
        // Curve doesn't start at 0, prepend a zero point
        normalized.timePoints.push_back(0.0);
        normalized.thrustPoints.push_back(0.0);
    }

    // std::max(a, b) is (a < b) ? b : a, so each use below keeps a NaN where OpenRocket's
    // "if (b > a) a = b" does.
    double lastTime = -std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < timePoints.size(); i++)
    {
        const double t = timePoints[i];
        // Ensure thrust is non-negative
        const double thrust = std::max(thrustPoints[i], 0.0);

        // Ensure time is strictly increasing. If multiple points exist at the same time, keep the
        // maximum thrust. (The prepended zero point does not count as the last time.)
        if (t > lastTime + kTimeEpsilon)
        {
            normalized.timePoints.push_back(t);
            normalized.thrustPoints.push_back(thrust);
            lastTime = t;
        }
        else if (std::abs(t - lastTime) <= kTimeEpsilon && !normalized.thrustPoints.empty())
        {
            normalized.thrustPoints.back() = std::max(normalized.thrustPoints.back(), thrust);
        }
        // Skip decreasing time points
    }

    if (normalized.timePoints.size() < 2)
    {
        return std::nullopt;
    }
    // OpenRocket only logs a warning when the result does not start at 0.
    return normalized;
}

/// calculateCGPoints(): the CG at the centre of the motor, the mass falling from @p totalMass
/// with the propellant burning in proportion to the thrust (constant exhaust velocity).
[[nodiscard]] std::vector<Coordinate> calculateCgPoints(std::span<const double> timePoints,
                                                        std::span<const double> thrustPoints,
                                                        double totalMass, double propellantMass,
                                                        double length)
{
    std::vector<Coordinate> cgPoints;
    cgPoints.reserve(timePoints.size());
    // std::max(a, b) is (a < b) ? b : a, which keeps a NaN as OpenRocket's "if (a < b) a = b"
    // does.
    const double cgX         = length / 2;  // CG at center of motor
    const double burnoutMass = std::max(totalMass - propellantMass, 0.0);

    if (timePoints.empty())
    {
        return cgPoints;
    }

    if (propellantMass <= 0)
    {
        for (std::size_t i = 0; i < timePoints.size(); i++)
        {
            cgPoints.emplace_back(cgX, 0, 0, totalMass);
        }
        return cgPoints;
    }

    // A linear mass decrease over burnTime.
    const auto linear = [&](double burnTime) {
        for (const double t : timePoints)
        {
            const double massAtTime =
                std::max(totalMass - (propellantMass * (t / burnTime)), burnoutMass);
            cgPoints.emplace_back(cgX, 0, 0, massAtTime);
        }
        return cgPoints;
    };

    if (thrustPoints.size() != timePoints.size() || timePoints.size() < 2)
    {
        // Fallback to a linear mass decrease if thrust data is missing or inconsistent.
        return linear(timePoints.back());
    }

    // Calculate mass change between points using trapezoidal integration of thrust.
    double              totalMassChange = 0;
    double              t0              = timePoints[0];
    double              f0              = thrustPoints[0];
    std::vector<double> deltaMass(timePoints.size() - 1);
    for (std::size_t i = 1; i < timePoints.size(); i++)
    {
        const double t1  = timePoints[i];
        const double f1  = thrustPoints[i];
        const double dm  = std::max(0.5 * (f0 + f1) * (t1 - t0), 0.0);
        deltaMass[i - 1] = dm;
        totalMassChange += dm;
        t0 = t1;
        f0 = f1;
    }

    if (totalMassChange <= 0)
    {
        // No thrust -> cannot scale consumption by impulse; fall back to linear.
        return linear(timePoints.back() == 0 ? 1 : timePoints.back());
    }

    const double scale = propellantMass / totalMassChange;

    double mass = totalMass;
    cgPoints.emplace_back(cgX, 0, 0, mass);
    for (std::size_t i = 1; i < timePoints.size(); i++)
    {
        mass -= deltaMass[i - 1] * scale;
        mass = std::max(mass, burnoutMass);
        cgPoints.emplace_back(cgX, 0, 0, mass);
    }
    return cgPoints;
}

/// computeDigest(): the times, masses, CGs and thrusts, as MotorDigest::digestMotor digests them.
[[nodiscard]] std::string computeDigest(std::span<const double>     timePoints,
                                        std::span<const double>     thrustPoints,
                                        std::span<const Coordinate> cgPoints)
{
    MotorDigest motorDigest;
    motorDigest.update(MotorDigest::DataType::TIME_ARRAY, timePoints);

    std::vector<double> cgx;
    std::vector<double> mass;
    cgx.reserve(cgPoints.size());
    mass.reserve(cgPoints.size());
    for (const Coordinate& cg : cgPoints)
    {
        cgx.push_back(cg.x);
        mass.push_back(cg.weight);
    }

    motorDigest.update(MotorDigest::DataType::MASS_PER_TIME, mass);
    motorDigest.update(MotorDigest::DataType::CG_PER_TIME, cgx);
    motorDigest.update(MotorDigest::DataType::FORCE_PER_TIME, thrustPoints);
    return motorDigest.getDigest();
}

/// Appends the thrust points of @p curveId, ordered by time, to @p time and @p thrust.
[[nodiscard]] Result<void> readThrustData(Statement& thrustStatement, int curveId,
                                          std::vector<double>& time, std::vector<double>& thrust)
{
    thrustStatement.reset();
    if (Result<void> bound = thrustStatement.bind(1, curveId); !bound)
    {
        return bound;
    }
    while (true)
    {
        Result<bool> row = thrustStatement.step();
        if (!row)
        {
            return propagate(row);
        }
        if (!*row)
        {
            return {};
        }
        time.push_back(thrustStatement.columnDouble(0));
        thrust.push_back(thrustStatement.columnDouble(1));
    }
}

/// The curve ids of @p motorId, preferring cert, then mfr, then user curves.
[[nodiscard]] Result<std::vector<int>> readCurveIds(Statement& curveStatement, int motorId)
{
    curveStatement.reset();
    if (Result<void> bound = curveStatement.bind(1, motorId); !bound)
    {
        return propagate(bound);
    }
    std::vector<int> curveIds;
    while (true)
    {
        Result<bool> row = curveStatement.step();
        if (!row)
        {
            return propagate(row);
        }
        if (!*row)
        {
            return curveIds;
        }
        curveIds.push_back(curveStatement.columnInt(0));
    }
}

/// The columns of the motor query that readMotors() uses.
struct MotorRow
{
    int                        motorId{0};
    std::optional<std::string> designation;
    std::optional<std::string> commonName;
    double                     diameter{0};
    double                     length{0};
    double                     propellantWeight{0};
    double                     totalWeight{0};
    std::optional<std::string> typeCode;
    std::optional<std::string> delays;
    std::optional<std::string> caseInfo;
    std::optional<std::string> propInfo;
    std::optional<std::string> tcMotorId;
    bool                       sparky{false};
    std::optional<std::string> infoUrl;
    std::optional<int>         dataFiles;
    std::optional<std::string> updatedOn;
    std::optional<std::string> description;
    std::optional<std::string> dataSource;
    std::optional<std::string> manufacturerName;
    std::optional<std::string> manufacturerAbbrev;
};

/// The current row of the motor query, with OpenRocket's unit conversions (mm and g to m and
/// kg).
[[nodiscard]] MotorRow readMotorRow(const Statement& row, bool hasMotorDescriptionSource)
{
    MotorRow motor;
    motor.motorId          = row.columnInt(0);
    motor.designation      = row.columnText(1);
    motor.commonName       = row.columnText(2);
    motor.diameter         = getDoubleOrDefault(row, 3, 0.0) / 1000;  // mm to m
    motor.length           = getDoubleOrDefault(row, 4, 0.0) / 1000;  // mm to m
    motor.propellantWeight = getDoubleOrDefault(row, 8, 0.0) / 1000;  // g to kg
    motor.totalWeight      = getDoubleOrDefault(row, 9, 0.0) / 1000;  // g to kg
    motor.typeCode         = row.columnText(10);
    motor.delays           = row.columnText(11);
    motor.caseInfo         = row.columnText(12);
    motor.propInfo         = row.columnText(13);
    motor.tcMotorId        = row.columnText(14);
    motor.sparky           = getIntOrDefault(row, 15, 0) != 0;
    motor.infoUrl          = row.columnText(16);
    motor.dataFiles        = getIntegerOrNull(row, 17);
    motor.updatedOn        = row.columnText(18);
    if (hasMotorDescriptionSource)
    {
        motor.description        = row.columnText(19);
        motor.dataSource         = row.columnText(20);
        motor.manufacturerName   = row.columnText(21);
        motor.manufacturerAbbrev = row.columnText(22);
    }
    else
    {
        motor.manufacturerName   = row.columnText(19);
        motor.manufacturerAbbrev = row.columnText(20);
    }
    return motor;
}

/// What readMotors() works out once per motor row for all its curves.
struct MotorMetadata
{
    std::string         code;         ///< the designation as stored ("" for NULL)
    std::string         designation;  ///< the code without its delay
    std::vector<double> delays;
    Motor::Type         type{Motor::Type::UNKNOWN};
    double              length{0};  ///< the length, made positive for the CG
};

/// Normalizes the shared motor metadata of @p row.
[[nodiscard]] MotorMetadata normalizeMotorRow(const MotorRow& row)
{
    MotorMetadata metadata;
    metadata.code        = row.designation.value_or("");
    metadata.designation = ThrustCurveMotor::removeDelay(metadata.code);
    if (row.delays.has_value())
    {
        metadata.delays = SqliteMotorDatabaseReader::parseDelays(*row.delays);
    }
    metadata.type = parseMotorType(row.typeCode);
    // Ensure we have valid length (required for CG calculation)
    metadata.length = row.length;
    if (metadata.length <= 0)
    {
        metadata.length = row.diameter > 0 ? row.diameter * 3 : 0.1;
    }
    return metadata;
}

/// Reads the curve @p curveId of the motor @p row and adds it to @p contents: as a motor, or as
/// a skipped curve when it has no points, too few after normalisation, or ThrustCurveMotor
/// rejects it.
[[nodiscard]] Result<void> readCurve(Statement& thrustStatement, const MotorRow& row,
                                     const MotorMetadata& metadata, int curveId,
                                     SqliteMotorDatabaseReader::Contents& contents)
{
    using SkippedCurve = SqliteMotorDatabaseReader::SkippedCurve;
    const auto skip    = [&](SkippedCurve::Reason reason, std::string message) {
        contents.skipped.push_back(SkippedCurve{.reason      = reason,
                                                .motorId     = row.motorId,
                                                .curveId     = curveId,
                                                .designation = metadata.code,
                                                .message     = std::move(message)});
        return Result<void>();
    };

    std::vector<double> timeList;
    std::vector<double> thrustList;
    if (Result<void> read = readThrustData(thrustStatement, curveId, timeList, thrustList); !read)
    {
        return read;
    }
    if (timeList.empty())
    {
        return skip(SkippedCurve::Reason::NO_DATA, {});
    }

    std::optional<NormalizedThrustData> normalized = normalizeThrustData(timeList, thrustList);
    if (!normalized.has_value())
    {
        return skip(SkippedCurve::Reason::INVALID_DATA, {});
    }

    std::vector<Coordinate> cgPoints =
        calculateCgPoints(normalized->timePoints, normalized->thrustPoints, row.totalWeight,
                          row.propellantWeight, metadata.length);
    std::string digest = computeDigest(normalized->timePoints, normalized->thrustPoints, cgPoints);

    ThrustCurveMotor::Builder builder;
    builder
        .setManufacturer(Manufacturer::getManufacturer(
            chooseManufacturerName(row.manufacturerName, row.manufacturerAbbrev)))
        .setCode(metadata.code)
        .setDesignation(metadata.designation)
        .setCommonName(row.commonName.value_or(""))
        .setDescription(row.description.value_or(""))
        .setTcMotorId(row.tcMotorId.value_or(""))
        .setInfoUrl(row.infoUrl.value_or(""))
        .setDataFiles(row.dataFiles)
        .setUpdatedOn(row.updatedOn.value_or(""))
        .setDataSource(row.dataSource.value_or(""))
        .setSparky(row.sparky)
        .setMotorType(metadata.type)
        .setDiameter(row.diameter)
        .setLength(metadata.length)
        .setCaseInfo(row.caseInfo.value_or(""))
        .setPropellantInfo(row.propInfo.value_or(""))
        .setInitialMass(row.totalWeight)
        .setDigest(std::move(digest))
        .setAvailability(true)
        .setStandardDelays(metadata.delays)
        .setTimePoints(std::move(normalized->timePoints))
        .setThrustPoints(std::move(normalized->thrustPoints))
        .setCGPoints(std::move(cgPoints));

    Result<ThrustCurveMotor> motor = builder.build();
    if (!motor)
    {
        return skip(SkippedCurve::Reason::INVALID_MOTOR, std::move(motor.error().message));
    }
    contents.motors.push_back(std::make_shared<const ThrustCurveMotor>(std::move(*motor)));
    return {};
}

/// readMotors(): one motor per usable thrust curve.
[[nodiscard]] Result<SqliteMotorDatabaseReader::Contents> readMotors(const Connection& connection,
                                                                     bool hasMotorDescriptionSource)
{
    using SkippedCurve = SqliteMotorDatabaseReader::SkippedCurve;

    // Query to get motors with their manufacturers
    const std::string motorSql =
        std::string(
            "SELECT m.id, m.designation, m.common_name, m.diameter, m.length, "
            "m.total_impulse, m.avg_thrust, m.burn_time, m.propellant_weight, "
            "m.total_weight, "
            "m.type, m.delays, m.case_info, m.prop_info, "
            "m.tc_motor_id, m.sparky, m.info_url, m.data_files, m.updated_on") +
        (hasMotorDescriptionSource ? ", m.description, m.source" : "") +
        ", "
        "mfr.name, mfr.abbrev "
        "FROM motors m "
        "JOIN manufacturers mfr ON m.manufacturer_id = mfr.id";

    // Get all thrust curves for a motor (prefer cert > mfr > user order)
    const std::string curveSql =
        "SELECT id FROM thrust_curves WHERE motor_id = ? "
        "ORDER BY CASE source "
        "  WHEN 'cert' THEN 1 "
        "  WHEN 'mfr' THEN 2 "
        "  WHEN 'user' THEN 3 "
        "  ELSE 4 END";

    const std::string thrustSql =
        "SELECT time_seconds, force_newtons FROM thrust_data "
        "WHERE curve_id = ? ORDER BY time_seconds";

    Result<Statement> motorStatement = connection.prepare(motorSql);
    if (!motorStatement)
    {
        return propagate(motorStatement);
    }
    Result<Statement> curveStatement = connection.prepare(curveSql);
    if (!curveStatement)
    {
        return propagate(curveStatement);
    }
    Result<Statement> thrustStatement = connection.prepare(thrustSql);
    if (!thrustStatement)
    {
        return propagate(thrustStatement);
    }

    SqliteMotorDatabaseReader::Contents contents;
    while (true)
    {
        Result<bool> hasRow = motorStatement->step();
        if (!hasRow)
        {
            return propagate(hasRow);
        }
        if (!*hasRow)
        {
            break;
        }
        const MotorRow row = readMotorRow(*motorStatement, hasMotorDescriptionSource);

        // Collect all curve IDs for this motor
        Result<std::vector<int>> curveIds = readCurveIds(*curveStatement, row.motorId);
        if (!curveIds)
        {
            return propagate(curveIds);
        }

        if (curveIds->empty())
        {
            contents.skipped.push_back(SkippedCurve{.reason      = SkippedCurve::Reason::NO_CURVES,
                                                    .motorId     = row.motorId,
                                                    .curveId     = std::nullopt,
                                                    .designation = row.designation.value_or(""),
                                                    .message     = {}});
            continue;
        }

        // Create one ThrustCurveMotor per thrust curve
        const MotorMetadata metadata = normalizeMotorRow(row);
        for (const int curveId : *curveIds)
        {
            if (Result<void> read = readCurve(*thrustStatement, row, metadata, curveId, contents);
                !read)
            {
                return propagate(read);
            }
        }
    }

    return contents;
}

// ---------------------------------------------------------------- writing

/// The statements of createSchema(), OpenRocket's text.
constexpr std::array<std::string_view, 13> kSchemaStatements{
    // Meta table for schema version and metadata
    "CREATE TABLE IF NOT EXISTS meta ("
    "key TEXT PRIMARY KEY, "
    "value TEXT NOT NULL"
    ")",

    // Manufacturers table
    "CREATE TABLE IF NOT EXISTS manufacturers ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "name TEXT NOT NULL UNIQUE, "
    "abbrev TEXT"
    ")",

    // Motors table
    "CREATE TABLE IF NOT EXISTS motors ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "manufacturer_id INTEGER NOT NULL, "
    "tc_motor_id TEXT, "
    "designation TEXT NOT NULL, "
    "common_name TEXT, "
    "impulse_class TEXT, "
    "diameter REAL, "
    "length REAL, "
    "total_impulse REAL, "
    "avg_thrust REAL, "
    "max_thrust REAL, "
    "burn_time REAL, "
    "propellant_weight REAL, "
    "total_weight REAL, "
    "type TEXT, "
    "delays TEXT, "
    "case_info TEXT, "
    "prop_info TEXT, "
    "sparky INTEGER, "
    "info_url TEXT, "
    "data_files INTEGER, "
    "updated_on TEXT, "
    "description TEXT, "
    "source TEXT, "
    "FOREIGN KEY (manufacturer_id) REFERENCES manufacturers(id)"
    ")",

    // Thrust curves table (one motor can have multiple curves)
    "CREATE TABLE IF NOT EXISTS thrust_curves ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "motor_id INTEGER NOT NULL, "
    "tc_simfile_id TEXT, "
    "source TEXT, "
    "format TEXT, "
    "license TEXT, "
    "info_url TEXT, "
    "data_url TEXT, "
    "total_impulse REAL, "
    "avg_thrust REAL, "
    "max_thrust REAL, "
    "burn_time REAL, "
    "FOREIGN KEY (motor_id) REFERENCES motors(id) ON DELETE CASCADE"
    ")",

    // Thrust data table
    "CREATE TABLE IF NOT EXISTS thrust_data ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "curve_id INTEGER NOT NULL, "
    "time_seconds REAL NOT NULL, "
    "force_newtons REAL NOT NULL, "
    "FOREIGN KEY (curve_id) REFERENCES thrust_curves(id) ON DELETE CASCADE"
    ")",

    // Create indices for performance
    "CREATE INDEX IF NOT EXISTS idx_motor_mfr ON motors(manufacturer_id)",
    "CREATE INDEX IF NOT EXISTS idx_motor_diameter ON motors(diameter)",
    "CREATE INDEX IF NOT EXISTS idx_motor_impulse ON motors(total_impulse)",
    "CREATE INDEX IF NOT EXISTS idx_motor_impulse_class ON motors(impulse_class)",
    "CREATE INDEX IF NOT EXISTS idx_motor_tc_id ON motors(tc_motor_id)",
    "CREATE INDEX IF NOT EXISTS idx_curve_motor ON thrust_curves(motor_id)",
    "CREATE INDEX IF NOT EXISTS idx_curve_simfile ON thrust_curves(tc_simfile_id)",
    "CREATE INDEX IF NOT EXISTS idx_thrust_curve ON thrust_data(curve_id)",
};

/// Java's Instant.now().toString(): UTC in ISO-8601, the fraction of a second in groups of three
/// digits as needed ("2026-09-24T16:23:45.123456Z"; none when it is zero). The clock is read to
/// the microsecond, as Java's is on Linux.
[[nodiscard]] std::string isoInstantNow()
{
    const auto now =
        std::chrono::floor<std::chrono::microseconds>(std::chrono::system_clock::now());
    // std::chrono::days is in <chrono>, which include-cleaner does not know.
    // NOLINTNEXTLINE(misc-include-cleaner)
    const auto                        day = std::chrono::floor<std::chrono::days>(now);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss       time{now - day};
    std::string                       text =
        std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}", static_cast<int>(date.year()),
                    static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()),
                    time.hours().count(), time.minutes().count(), time.seconds().count());
    const auto micros = time.subseconds().count();
    if (micros % 1000 != 0)
    {
        text += std::format(".{:06}", micros);
    }
    else if (micros != 0)
    {
        text += std::format(".{:03}", micros / 1000);
    }
    return text + "Z";
}

/// nullIfBlank(): @p value trimmed, or SQL NULL when that is empty.
[[nodiscard]] std::optional<std::string> nullIfBlank(std::string_view value)
{
    const std::string_view trimmed = Strings::trim(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }
    return std::string(trimmed);
}

/// normalizeDescriptionForDb(): OpenRocket turns \r into \n, runs of whitespace around a \n and
/// then every other run into one space, and trims; every run of whitespace becomes one space
/// either way, which is Strings::collapseWhitespace. SQL NULL when nothing is left.
[[nodiscard]] std::optional<std::string> normalizeDescriptionForDb(std::string_view value)
{
    std::string trimmed = Strings::collapseWhitespace(value);
    if (trimmed.empty())
    {
        return std::nullopt;
    }
    return trimmed;
}

/// getMotorTypeCode().
[[nodiscard]] std::optional<std::string> getMotorTypeCode(Motor::Type type)
{
    switch (type)
    {
        case Motor::Type::SINGLE:
            return "SU";
        case Motor::Type::RELOAD:
            return "reload";
        case Motor::Type::HYBRID:
            return "hybrid";
        case Motor::Type::UNKNOWN:
            return std::nullopt;
    }
    return std::nullopt;
}

/// calculateTotalImpulse(): trapezoidal integration.
[[nodiscard]] double calculateTotalImpulse(std::span<const double> timePoints,
                                           std::span<const double> thrustPoints)
{
    if (timePoints.size() < 2)
    {
        return 0;
    }
    double impulse = 0;
    for (std::size_t i = 1; i < timePoints.size(); i++)
    {
        const double dt        = timePoints[i] - timePoints[i - 1];
        const double avgThrust = (thrustPoints[i] + thrustPoints[i - 1]) / 2;
        impulse += avgThrust * dt;
    }
    return impulse;
}

/// writeMetadata().
[[nodiscard]] Result<void> writeMetadata(const Connection& connection, std::size_t motorCount)
{
    Result<Statement> statement = connection.prepare("INSERT INTO meta (key, value) VALUES (?, ?)");
    if (!statement)
    {
        return propagate(statement);
    }
    const auto seconds = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const std::array<std::pair<std::string_view, std::string>, 4> entries{{
        // Schema version
        {"schema_version", std::to_string(SqliteMotorDatabaseReader::kSchemaVersion)},
        // Database version (timestamp)
        {"database_version", std::to_string(seconds.time_since_epoch().count())},
        // Generated at
        {"generated_at", isoInstantNow()},
        // Motor count
        {"motor_count", std::to_string(motorCount)},
    }};
    for (const auto& [key, value] : entries)
    {
        statement->reset();
        if (Result<void> bound = bindText(*statement, 1, key); !bound)
        {
            return bound;
        }
        if (Result<void> bound = bindText(*statement, 2, value); !bound)
        {
            return bound;
        }
        if (Result<void> ran = statement->run(); !ran)
        {
            return ran;
        }
    }
    return {};
}

/// The prepared statements of insertMotors().
struct InsertStatements
{
    Statement manufacturerInsert;
    Statement manufacturerSelect;
    Statement motorInsert;
    Statement curveInsert;
    Statement thrustInsert;
};

[[nodiscard]] Result<InsertStatements> prepareInserts(const Connection& connection)
{
    Result<Statement> manufacturerInsert =
        connection.prepare("INSERT OR IGNORE INTO manufacturers (name, abbrev) VALUES (?, ?)");
    if (!manufacturerInsert)
    {
        return propagate(manufacturerInsert);
    }
    Result<Statement> manufacturerSelect =
        connection.prepare("SELECT id FROM manufacturers WHERE name = ?");
    if (!manufacturerSelect)
    {
        return propagate(manufacturerSelect);
    }
    Result<Statement> motorInsert = connection.prepare(
        "INSERT INTO motors ("
        "manufacturer_id, tc_motor_id, designation, common_name, impulse_class, diameter, length, "
        "total_impulse, avg_thrust, max_thrust, burn_time, propellant_weight, total_weight, "
        "type, delays, case_info, prop_info, sparky, info_url, data_files, updated_on, "
        "description, source"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    if (!motorInsert)
    {
        return propagate(motorInsert);
    }
    Result<Statement> curveInsert =
        connection.prepare("INSERT INTO thrust_curves (motor_id, source, format) VALUES (?, ?, ?)");
    if (!curveInsert)
    {
        return propagate(curveInsert);
    }
    Result<Statement> thrustInsert = connection.prepare(
        "INSERT INTO thrust_data (curve_id, time_seconds, force_newtons) VALUES (?, ?, ?)");
    if (!thrustInsert)
    {
        return propagate(thrustInsert);
    }
    return InsertStatements{.manufacturerInsert = std::move(*manufacturerInsert),
                            .manufacturerSelect = std::move(*manufacturerSelect),
                            .motorInsert        = std::move(*motorInsert),
                            .curveInsert        = std::move(*curveInsert),
                            .thrustInsert       = std::move(*thrustInsert)};
}

/// The id of @p motor's manufacturer, inserted first when new.
[[nodiscard]] Result<int> manufacturerId(InsertStatements&                        statements,
                                         std::map<std::string, int, std::less<>>& cache,
                                         const ThrustCurveMotor&                  motor)
{
    const std::string& mfrName   = motor.getManufacturer().getDisplayName();
    const std::string& mfrAbbrev = motor.getManufacturer().getSimpleName();
    if (const auto cached = cache.find(mfrName); cached != cache.end())
    {
        return cached->second;
    }

    Statement& insert = statements.manufacturerInsert;
    insert.reset();
    for (const Result<void>& bound : {bindText(insert, 1, mfrName), bindText(insert, 2, mfrAbbrev)})
    {
        if (!bound)
        {
            return std::unexpected(bound.error());
        }
    }
    if (Result<void> ran = insert.run(); !ran)
    {
        return propagate(ran);
    }

    Statement& select = statements.manufacturerSelect;
    select.reset();
    if (Result<void> bound = bindText(select, 1, mfrName); !bound)
    {
        return propagate(bound);
    }
    Result<bool> row = select.step();
    if (!row)
    {
        return propagate(row);
    }
    if (!*row)
    {
        return fail(ErrorCode::DATABASE, "Failed to retrieve manufacturer ID for: " + mfrName);
    }
    const int id = select.columnInt(0);
    cache.emplace(mfrName, id);
    return id;
}

/// Inserts @p motor's row into motors with manufacturer @p mfrId.
[[nodiscard]] Result<void> insertMotorRow(Statement& insert, int mfrId,
                                          const ThrustCurveMotor& motor)
{
    // Calculate motor properties
    const std::vector<double>& timePoints   = motor.getTimePoints();
    const std::vector<double>& thrustPoints = motor.getThrustPoints();
    const double               impulse      = calculateTotalImpulse(timePoints, thrustPoints);
    const double               burnTime     = !timePoints.empty() ? timePoints.back() : 0;
    const double               avgThrust    = burnTime > 0 ? impulse / burnTime : 0;
    double                     maxThrust    = 0;
    for (const double t : thrustPoints)
    {
        maxThrust = std::max(maxThrust, t);  // keeps maxThrust for a NaN, as "if (t > maxThrust)"
    }
    const double propellantWeight =
        (motor.getInitialMass() - motor.getBurnoutMass()) * 1000;  // kg to g

    insert.reset();
    const std::array<Result<void>, 23> bindings{
        insert.bind(1, mfrId),
        insert.bind(2, nullIfBlank(motor.getTcMotorId())),
        bindText(insert, 3, motor.getDesignation()),
        bindText(insert, 4, motor.getCommonName()),
        insert.bind(5, SqliteMotorDatabaseReader::getImpulseClass(impulse)),
        insert.bind(6, motor.getDiameter() * 1000),  // m to mm
        insert.bind(7, motor.getLength() * 1000),    // m to mm
        insert.bind(8, impulse),
        insert.bind(9, avgThrust),
        insert.bind(10, maxThrust),
        insert.bind(11, burnTime),
        insert.bind(12, propellantWeight),
        insert.bind(13, motor.getInitialMass() * 1000),  // kg to g
        insert.bind(14, getMotorTypeCode(motor.getMotorType())),
        insert.bind(15, SqliteMotorDatabaseReader::formatDelays(motor.getStandardDelays())),
        bindText(insert, 16, motor.getCaseInfo()),
        bindText(insert, 17, motor.getPropellantInfo()),
        insert.bind(18, motor.isSparky() ? 1 : 0),
        insert.bind(19, nullIfBlank(motor.getInfoUrl())),
        motor.getDataFiles().has_value() ? insert.bind(20, *motor.getDataFiles())
                                         : insert.bindNull(20),
        insert.bind(21, nullIfBlank(motor.getUpdatedOn())),
        insert.bind(22, normalizeDescriptionForDb(motor.getDescription())),
        insert.bind(23, nullIfBlank(motor.getDataSource())),
    };
    for (const Result<void>& bound : bindings)
    {
        if (!bound)
        {
            return std::unexpected(bound.error());
        }
    }
    return insert.run();
}

/// Inserts the curve of the motor @p motorId and its points.
[[nodiscard]] Result<void> insertCurve(const Connection& connection, InsertStatements& statements,
                                       int motorId, const ThrustCurveMotor& motor)
{
    Statement& curve = statements.curveInsert;
    curve.reset();
    for (const Result<void>& bound :
         {curve.bind(1, motorId), bindText(curve, 2, "openrocket"),  // source
          bindText(curve, 3, "internal")})                           // format
    {
        if (!bound)
        {
            return std::unexpected(bound.error());
        }
    }
    if (Result<void> ran = curve.run(); !ran)
    {
        return ran;
    }
    const int curveId = connection.lastInsertId();

    // Insert thrust data points
    const std::vector<double>& timePoints   = motor.getTimePoints();
    const std::vector<double>& thrustPoints = motor.getThrustPoints();
    Statement&                 thrust       = statements.thrustInsert;
    for (std::size_t i = 0; i < timePoints.size(); i++)
    {
        thrust.reset();
        for (const Result<void>& bound : {thrust.bind(1, curveId), thrust.bind(2, timePoints[i]),
                                          thrust.bind(3, thrustPoints[i])})
        {
            if (!bound)
            {
                return std::unexpected(bound.error());
            }
        }
        if (Result<void> ran = thrust.run(); !ran)
        {
            return ran;
        }
    }
    return {};
}

/// insertMotors().
[[nodiscard]] Result<void> insertMotors(
    const Connection& connection, std::span<const std::shared_ptr<const ThrustCurveMotor>> motors)
{
    // Cache for manufacturer IDs
    std::map<std::string, int, std::less<>> manufacturerCache;

    Result<InsertStatements> statements = prepareInserts(connection);
    if (!statements)
    {
        return propagate(statements);
    }

    for (const std::shared_ptr<const ThrustCurveMotor>& motor : motors)
    {
        QTROCKET_ASSERT(motor != nullptr);

        // Get or create manufacturer
        Result<int> mfrId = manufacturerId(*statements, manufacturerCache, *motor);
        if (!mfrId)
        {
            return propagate(mfrId);
        }

        // Insert motor
        if (Result<void> inserted = insertMotorRow(statements->motorInsert, *mfrId, *motor);
            !inserted)
        {
            return inserted;
        }
        const int motorId = connection.lastInsertId();

        // Insert thrust curve
        if (Result<void> inserted = insertCurve(connection, *statements, motorId, *motor);
            !inserted)
        {
            return inserted;
        }
    }
    return {};
}

}  // namespace

Result<SqliteMotorDatabaseReader::Contents> SqliteMotorDatabaseReader::readDatabase(
    const std::filesystem::path& dbFile)
{
    Result<Connection> connection = openValidated(dbFile);
    if (!connection)
    {
        return propagate(connection);
    }
    // Only include optional columns in queries if they actually exist.
    Result<bool> hasDescription = columnExists(*connection, "motors", "description");
    if (!hasDescription)
    {
        return propagate(hasDescription);
    }
    Result<bool> hasSource = columnExists(*connection, "motors", "source");
    if (!hasSource)
    {
        return propagate(hasSource);
    }
    return readMotors(*connection, *hasDescription && *hasSource);
}

Result<void> SqliteMotorDatabaseReader::validateDatabase(const std::filesystem::path& dbFile)
{
    Result<Connection> connection = openValidated(dbFile);
    if (!connection)
    {
        return propagate(connection);
    }
    return {};
}

Result<void> SqliteMotorDatabaseReader::writeDatabase(
    const std::filesystem::path&                             dbFile,
    std::span<const std::shared_ptr<const ThrustCurveMotor>> motors)
{
    // ensureParentDirectory()
    std::error_code             error;
    const std::filesystem::path parent = dbFile.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent, error))
    {
        std::filesystem::create_directories(parent, error);
        if (error)
        {
            return fail(ErrorCode::IO, "Unable to create directory: " +
                                           pathToUtf8(std::filesystem::absolute(parent, error)));
        }
    }
    if (std::filesystem::exists(dbFile, error) && !std::filesystem::remove(dbFile, error))
    {
        return fail(ErrorCode::IO, "Unable to delete existing SQLite database: " +
                                       pathToUtf8(std::filesystem::absolute(dbFile, error)));
    }

    Result<Connection> connection = Connection::open(dbFile, true);
    if (!connection)
    {
        return propagate(connection);
    }
    // setAutoCommit(false): one transaction, committed at the end or rolled back.
    if (Result<void> begun = connection->execute("BEGIN"); !begun)
    {
        return begun;
    }
    const auto write = [&connection, motors] -> Result<void> {
        for (const std::string_view sql : kSchemaStatements)
        {
            if (Result<void> created = connection->execute(std::string(sql)); !created)
            {
                return created;
            }
        }
        if (Result<void> metadata = writeMetadata(*connection, motors.size()); !metadata)
        {
            return metadata;
        }
        if (Result<void> inserted = insertMotors(*connection, motors); !inserted)
        {
            return inserted;
        }
        return connection->execute("COMMIT");
    };
    if (Result<void> written = write(); !written)
    {
        // The original failure is the one to report; a failed rollback leaves nothing to undo.
        [[maybe_unused]] const Result<void> rolledBack = connection->execute("ROLLBACK");
        return written;
    }
    return {};
}

std::vector<double> SqliteMotorDatabaseReader::parseDelays(std::string_view delays)
{
    std::vector<double> parsed;
    if (Strings::isEmpty(delays))
    {
        return parsed;
    }
    // Split on commas or dashes: legacy databases used dashes as separators. Java's split keeps a
    // leading empty part, which fails to parse and is skipped like any other empty part.
    std::size_t position = 0;
    while (position <= delays.size())
    {
        std::size_t end = delays.find_first_of(",-", position);
        if (end == std::string_view::npos)
        {
            end = delays.size();
        }
        const std::string_view part = Strings::trim(delays.substr(position, end - position));
        if (Strings::javaEqualsIgnoreCase(part, "P") ||
            Strings::javaEqualsIgnoreCase(part, "plugged"))
        {
            parsed.push_back(Motor::kPluggedDelay);
        }
        else if (const std::optional<double> delay = Strings::javaParseDouble(part))
        {
            parsed.push_back(*delay);
        }
        // Skip invalid delay values (e.g. "S", "M", "L" for hybrid motor delays)
        position = end + 1;
    }
    return parsed;
}

std::optional<std::string> SqliteMotorDatabaseReader::formatDelays(std::span<const double> delays)
{
    if (delays.empty())
    {
        return std::nullopt;
    }
    std::string sb;
    for (std::size_t i = 0; i < delays.size(); i++)
    {
        if (i > 0)
        {
            sb += ',';
        }
        if (std::isinf(delays[i]))
        {
            sb += 'P';
        }
        else if (delays[i] == std::floor(delays[i]))
        {
            sb += std::to_string(MathUtil::javaIntCast(delays[i]));
        }
        else
        {
            sb += Strings::javaDoubleToString(delays[i]);
        }
    }
    return sb;
}

std::optional<std::string> SqliteMotorDatabaseReader::getImpulseClass(double impulse)
{
    if (impulse <= 0)
    {
        return std::nullopt;
    }
    constexpr std::array<std::pair<double, std::string_view>, 16> kClasses{{
        {1.25, "1/4A"},
        {2.5, "1/2A"},
        {5, "A"},
        {10, "B"},
        {20, "C"},
        {40, "D"},
        {80, "E"},
        {160, "F"},
        {320, "G"},
        {640, "H"},
        {1280, "I"},
        {2560, "J"},
        {5120, "K"},
        {10240, "L"},
        {20480, "M"},
        {40960, "N"},
    }};
    for (const auto& [limit, name] : kClasses)
    {
        if (impulse <= limit)
        {
            return std::string(name);
        }
    }
    return "O";
}

}  // namespace QtRocket
