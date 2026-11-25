#pragma once

#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>

#include "QtRocket/simulation/FlightEvent.h"

namespace QtRocket
{
class FlightData;
class SimulationPlotConfiguration;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The table of the flight events a simulation plot marks (OpenRocket's FlightEventTableModel, a
/// private class of swing gui/simulation/SimulationPlotPanel.java): a row for each type of event
/// the flight has (eventTypesOf(): in any stage, ALTITUDE left out, in FlightEvent::Type's
/// declaration order), and two columns: kSelectedColumn, a check box, checked while the plot
/// configuration marks events of the row's type (SimulationPlotConfiguration::isEventActive()), and
/// the only one that can be changed (Java's Boolean column, isCellEditable()), which marks them or
/// stops (setEvent()); and kNameColumn, the type's name (util/EnumText; Java: Type.toString()).
/// The table has no header (Java: setTableHeader(null)).
///
/// The configuration is the plot tab's own (SimulationPlotPanel's, which outlives the model and is
/// changed in place by the tab's other controls): refresh() tells the view that every check may
/// have changed (Java: fireTableDataChanged(), after All, None and a new configuration). toggle()
/// is a click on a row's name (Java: GUIUtil.BooleanTableClickListener).
///
/// Deviations from OpenRocket: the check is the column's Qt::CheckStateRole (Qt::Checked,
/// Qt::Unchecked), set through setData() with that role; Java's setValueAt() of another column or
/// of a value that is not a Boolean throws IllegalArgumentException, here setData() refuses it
/// (false).
class FlightEventTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// The check box column (Java's column 0).
    static constexpr int kSelectedColumn = 0;
    /// The name column (Java's column 1).
    static constexpr int kNameColumn = 1;
    /// The number of columns.
    static constexpr int kColumnCount = 2;

    /// The types of @p data's events, every stage's, ALTITUDE left out, each once, in
    /// FlightEvent::Type's declaration order (Java: FlightEventTableModel()'s EnumSet).
    [[nodiscard]] static std::vector<FlightEvent::Type> eventTypesOf(const FlightData& data);

    /// A table of @p eventTypes, checked as @p configuration marks them; @p configuration must
    /// outlive the model.
    FlightEventTableModel(std::vector<FlightEvent::Type> eventTypes,
                          SimulationPlotConfiguration& configuration, QObject* parent = nullptr);
    ~FlightEventTableModel() override;
    FlightEventTableModel(const FlightEventTableModel&)            = delete;
    FlightEventTableModel& operator=(const FlightEventTableModel&) = delete;
    FlightEventTableModel(FlightEventTableModel&&)                 = delete;
    FlightEventTableModel& operator=(FlightEventTableModel&&)      = delete;

    /// The rows' types.
    [[nodiscard]] const std::vector<FlightEvent::Type>& eventTypes() const noexcept
    {
        return m_eventTypes;
    }
    /// The type of row @p row.
    /// @throws BugError when there is no row @p row
    [[nodiscard]] FlightEvent::Type eventType(int row) const;

    /// Whether row @p row is checked.
    /// @throws BugError when there is no row @p row
    [[nodiscard]] bool isChecked(int row) const;

    /// Checks row @p row if it is not, else unchecks it (a click on its name).
    /// @throws BugError when there is no row @p row
    void toggle(int row);

    /// Tells the views that every row's check may have changed.
    void refresh();

    [[nodiscard]] int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int                role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private:
    /// Whether @p index is a cell of this table.
    [[nodiscard]] bool isCell(const QModelIndex& index) const;
    /// @throws BugError when there is no row @p row
    void checkRow(int row) const;

    std::vector<FlightEvent::Type> m_eventTypes;
    SimulationPlotConfiguration*   m_configuration;
};

}  // namespace QtRocket::Gui
