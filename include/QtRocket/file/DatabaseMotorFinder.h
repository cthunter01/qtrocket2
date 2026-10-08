#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/file/MotorFinder.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDatabase.h"

namespace QtRocket
{

/// A MotorFinder that searches a motor database (OpenRocket's file/DatabaseMotorFinder).
///
/// findMotor(), step by step as in OpenRocket:
/// 1. No designation: the warning "No motor specified, ignoring." and no motor.
/// 2. The candidates are MotorDatabase::findMotors(digest, type, manufacturer, designation,
///    diameter, length), in the database's order (see ThrustCurveMotorSetDatabase for which
///    motors that is: the ones that match the digest and the description, else the digest, else
///    the description).
/// 3. No candidate: handleMissingMotor(), which adds a Warning::MissingMotor with the query's
///    fields (priority HIGH: "No motor with designation '<d>' for manufacturer '<m>' found.",
///    the manufacturer part only when one is given) and returns no motor.
/// 4. One candidate: it, without a warning, whatever its digest.
/// 5. Several candidates and a digest (an empty one counts as given): the first whose digest is
///    compatible with it (MotorDigest::isDigestCompatible(), the digests of older file formats
///    included), else the first whose designation equals the one asked for ignoring case
///    (String.equalsIgnoreCase: "B6" is preferred to "B6-0"), else the first. No warning in any
///    of the three cases: a motor whose thrust curve is not the file's is taken silently, which
///    is what OpenRocket does and what its example designs rely on.
/// 6. Several candidates and no digest (a file older than format 1.4): the first, with the
///    warning "Multiple motors with designation '<d>' for manufacturer '<m>' found, one chosen
///    arbitrarily." (the manufacturer part only when one is given).
///
/// Which of several candidates is "the first" is decided by the order the database was filled
/// in, so a database must be filled in the same order to choose as OpenRocket does.
///
/// Deviations from OpenRocket:
/// - Java asks the application for the database (Application.getMotorSetDatabase()); here the
///   finder is made over the database it searches, which must outlive it.
/// - Java's debug log lines are not written.
class DatabaseMotorFinder : public MotorFinder
{
public:
    /// A finder over @p database, which must outlive it.
    explicit DatabaseMotorFinder(const MotorDatabase& database) noexcept;
    /// A temporary database would dangle.
    explicit DatabaseMotorFinder(const MotorDatabase&& database) = delete;

    [[nodiscard]] std::shared_ptr<const Motor> findMotor(
        std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
        std::optional<std::string_view> designation, double diameter, double length,
        std::optional<std::string_view> digest, WarningSet& warnings) const override;

protected:
    /// What findMotor() returns when the database has no candidate (handleMissingMotor()): this
    /// adds a Warning::MissingMotor with the query's fields to @p warnings and returns null. A
    /// subclass may return a stand-in instead.
    [[nodiscard]] virtual std::shared_ptr<const Motor> handleMissingMotor(
        std::optional<Motor::Type> type, std::optional<std::string_view> manufacturer,
        std::string_view designation, double diameter, double length,
        std::optional<std::string_view> digest, WarningSet& warnings) const;

private:
    const MotorDatabase* m_database;
};

}  // namespace QtRocket
