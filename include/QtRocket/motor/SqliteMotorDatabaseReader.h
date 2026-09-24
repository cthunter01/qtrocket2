#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/util/Error.h"

namespace QtRocket
{

class ThrustCurveMotor;

/// Reads and writes the thrust-curve motor database, an SQLite file such as the bundled
/// data/motors/initial_motors.db (OpenRocket's ThrustCurveMotorSQLiteDatabase). The schema has
/// the tables meta (key/value pairs, among them schema_version), manufacturers, motors,
/// thrust_curves (one motor can have several) and thrust_data (time/thrust points per curve);
/// versions 2 and 3 are read (3 adds motors.description and motors.source).
///
/// Every failure is ErrorCode::DATABASE with OpenRocket's message, except that SQLite's own errors
/// carry SQLite's message (OpenRocket's come from its JDBC driver) and file-system failures of
/// writeDatabase() are ErrorCode::IO. Where OpenRocket logs, this class reports: readDatabase()
/// returns the curves it skips.
class SqliteMotorDatabaseReader
{
public:
    /// The schema version writeDatabase() writes (SCHEMA_VERSION).
    static constexpr int kSchemaVersion = 3;
    /// The oldest schema version readDatabase() reads (MIN_SUPPORTED_SCHEMA_VERSION).
    static constexpr int kMinSupportedSchemaVersion = 2;

    /// A motor or thrust curve readDatabase() leaves out, and why (OpenRocket logs these).
    struct SkippedCurve
    {
        enum class Reason
        {
            NO_CURVES,  ///< the motor has no thrust curve ("Skipping motor with no thrust curves")
            NO_DATA,    ///< the curve has no points ("Skipping curve with no thrust data")
            INVALID_DATA,   ///< fewer than two points are left after normalisation
            INVALID_MOTOR,  ///< ThrustCurveMotor rejected the curve ("Skipping invalid curve")
        };

        Reason reason{Reason::NO_CURVES};
        int    motorId{0};
        /// The curve's id; none for NO_CURVES.
        std::optional<int> curveId;
        /// The motor's designation as stored ("" when NULL).
        std::string designation;
        /// ThrustCurveMotor's message for INVALID_MOTOR; empty otherwise.
        std::string message;

        bool operator==(const SkippedCurve&) const = default;
    };

    /// What readDatabase() returns: the motors in the order OpenRocket makes them (the motors in
    /// the order SQLite returns them from the join, each motor's curves by source: cert, mfr,
    /// user, then the rest) and the curves it left out, in the same order.
    struct Contents
    {
        std::vector<std::shared_ptr<const ThrustCurveMotor>> motors;
        std::vector<SkippedCurve>                            skipped;
    };

    SqliteMotorDatabaseReader() = delete;

    /// Reads every motor of @p dbFile (readDatabase()): validateDatabase()'s checks, then the
    /// motors joined with their manufacturers and one ThrustCurveMotor per curve. Each curve's
    /// points come ordered by time, are normalised (a (0, 0) point is put in front of a curve
    /// that starts more than 0.1 ms after zero; negative thrust becomes zero; of points within
    /// 0.1 ms of the previous kept one, only a higher thrust is kept; points that go back in time
    /// are dropped; fewer than two points left skips the curve), and get the CG at half the length
    /// (a length of zero or less becomes three diameters, or 0.1 m) and a mass that falls with the
    /// trapezoidal impulse from the total weight by the propellant weight (linearly in time when
    /// the curve has no impulse; constant without propellant), floored at the burnout mass. The
    /// digest covers the times, masses, CGs and thrusts (as MotorDigest::digestMotor). The
    /// designation loses its delay (ThrustCurveMotor::removeDelay), the code keeps it; delays are
    /// read from a list separated by ',' or '-' ("P" or "plugged" for a plugged motor, other
    /// words skipped); the type from "SU"/"single"/"single-use", "re"/"reload" and "hy"/"hybrid"
    /// (lower-cased, not trimmed; anything else is UNKNOWN); the manufacturer by its abbreviation,
    /// else its name, else "Unknown". Every motor is available.
    [[nodiscard]] static Result<Contents> readDatabase(const std::filesystem::path& dbFile);

    /// Checks that @p dbFile is a motor database this class can read (validateDatabase()): the
    /// file exists; the five tables exist; schema_version is an integer from
    /// kMinSupportedSchemaVersion to kSchemaVersion; each table has its columns (for version 3,
    /// motors also description and source); PRAGMA integrity_check says "ok" and
    /// foreign_key_check finds nothing.
    [[nodiscard]] static Result<void> validateDatabase(const std::filesystem::path& dbFile);

    /// Writes @p motors to a new database at @p dbFile in the current schema (writeDatabase()),
    /// replacing any file there and creating its directory: the metadata (schema_version,
    /// database_version as Unix seconds, generated_at as an ISO-8601 instant, motor_count), each
    /// motor's manufacturer once, the motor with its totals computed from the curve (trapezoidal
    /// impulse, the last time as the burn time, the impulse class letter), and the curve (source
    /// "openrocket", format "internal") with its points. All of it is one transaction, rolled back
    /// on failure.
    [[nodiscard]] static Result<void> writeDatabase(
        const std::filesystem::path&                             dbFile,
        std::span<const std::shared_ptr<const ThrustCurveMotor>> motors);

    /// The delays of the motors.delays column (parseDelays()): parts separated by runs of ',' or
    /// '-', trimmed; "P" or "plugged" (any case) is Motor::kPluggedDelay, a number (Java's
    /// Double.parseDouble) is a delay, anything else is skipped. An empty or blank text gives none.
    [[nodiscard]] static std::vector<double> parseDelays(std::string_view delays);

    /// The motors.delays text for @p delays (formatDelays()), comma-separated: "P" for an infinite
    /// delay, an integral one as an integer ("5"), any other as Java's Double.toString ("2.5");
    /// nullopt (SQL NULL) for none.
    [[nodiscard]] static std::optional<std::string> formatDelays(std::span<const double> delays);

    /// The impulse class of a total impulse in Ns (getImpulseClass()): "1/4A" up to 1.25, "1/2A"
    /// up to 2.5, then "A" to "N" doubling from 5, and "O" beyond 40960; nullopt for zero or less.
    [[nodiscard]] static std::optional<std::string> getImpulseClass(double impulse);
};

}  // namespace QtRocket
