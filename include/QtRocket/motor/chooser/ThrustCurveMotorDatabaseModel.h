#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"

namespace QtRocket
{

class Preferences;
class ThrustCurveMotorSet;

/// The rows of the motor chooser's table (OpenRocket's swing ThrustCurveMotorDatabaseModel, a
/// Swing AbstractTableModel; here without Qt): a list of motor sets, a row each, in the list's
/// order (the model order, to which the table's sort falls back), and the columns of
/// ThrustCurveMotorColumns. The motor table of the GUI shows it through a filter and a sort
/// (MotorRowFilter, MotorTableSorter: ThrustCurveMotorSelection::getViewRows()).
///
/// The model does not own the sets: they belong to the motor database (whose sets keep their
/// addresses while it lives, ThrustCurveMotorSetDatabase) or to the ThrustCurveMotorSelection
/// that added one (a motor that is in no set of the database), which must outlive the model.
class ThrustCurveMotorDatabaseModel
{
public:
    /// A model of the sets @p database, in that order.
    /// @throws BugError when a set is null.
    explicit ThrustCurveMotorDatabaseModel(std::vector<const ThrustCurveMotorSet*> database);

    /// Replaces the list of sets (setDatabase(); OpenRocket then tells the table that all its
    /// data changed).
    /// @throws BugError when a set is null.
    void setDatabase(std::vector<const ThrustCurveMotorSet*> database);

    /// The sets, in model order.
    [[nodiscard]] std::span<const ThrustCurveMotorSet* const> getMotorSets() const noexcept
    {
        return m_database;
    }

    /// getRowCount(): the number of sets.
    [[nodiscard]] std::size_t getRowCount() const noexcept { return m_database.size(); }

    /// getColumnCount(): the number of ThrustCurveMotorColumns.
    [[nodiscard]] static constexpr std::size_t getColumnCount() noexcept
    {
        return kAllThrustCurveMotorColumns.size();
    }

    /// getMotorSet(row): the set of the row.
    /// @throws BugError when @p row is not below getRowCount() (OpenRocket:
    ///         IndexOutOfBoundsException).
    [[nodiscard]] const ThrustCurveMotorSet& getMotorSet(std::size_t row) const;

    /// getIndex(set): the row of @p set, the first one holding that very set (identity, as Java's
    /// List.indexOf with a class that does not override equals()), or nullopt (Java's -1).
    [[nodiscard]] std::optional<std::size_t> getIndex(
        const ThrustCurveMotorSet& set) const noexcept;

    /// getValueAt(row, column).toString(): getValueText(column, getMotorSet(row), preferences).
    [[nodiscard]] std::string getValueText(std::size_t row, ThrustCurveMotorColumns column,
                                           const Preferences& preferences) const;

private:
    std::vector<const ThrustCurveMotorSet*> m_database;
};

}  // namespace QtRocket
