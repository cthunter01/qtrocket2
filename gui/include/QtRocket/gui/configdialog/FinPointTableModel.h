#pragma once

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"

namespace QtRocket::Gui
{

/// The point table of the freeform fin editor (OpenRocket's FreeformFinSetConfig.FinPointTableModel
/// and its Columns, swing gui/configdialog/FreeformFinSetConfig.java): a row per point of the
/// main fin set (getFinPoints()), the columns "X / <unit>" and "Y / <unit>" with the coordinates
/// in the default length unit, without the unit ("2.5"). Every cell is editable but the Y of the
/// first and the last point, which lie on the body.
///
/// Editing a cell (setData()) parses the text with UnitGroup::fromString() (a number, in the
/// default unit unless a unit follows it: "45 mm"); a text that does not parse, or a row the fin
/// set no longer has, is ignored (Java logs it). Otherwise pointEntered() gives the row and the
/// point with the new coordinate (the other one the point's own), which the editor sets
/// (FreeformFinSetConfig: an edit of its history around FinPointSelection::setPoint()).
///
/// The rows follow the fin set's points: the model refreshes itself on every change event of the
/// rocket (refresh(): rows added or removed at the end, as Java's table, whose rows are the
/// model's live point count, keeps the selected row's index), and the editor's updateFields()
/// refreshes it too (Java: fireTableDataChanged()); the cells read the fin set at every call.
/// While the rocket is frozen (a drag), the rows wait for the thaw's event: a drag adds no point
/// but on a segment, before the freeze.
///
/// Deviation from OpenRocket: the column headers follow the default unit (Java's JTable keeps the
/// header texts it was made with; its cells and its CSV header follow the unit).
class FinPointTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    static constexpr int kXColumn     = 0;
    static constexpr int kYColumn     = 1;
    static constexpr int kColumnCount = 2;

    /// The table of the main fin set of @p target.
    FinPointTableModel(FinPointSelection target, QObject* parent);

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// The coordinate's text for the display and the edit roles.
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// "X / cm" and "Y / cm" (the default length unit's symbol) for the display role.
    [[nodiscard]] QVariant      headerData(int section, Qt::Orientation orientation,
                                           int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// Parses @p value (see the class comment); true when it gave a point.
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    /// Takes the rows the fin set has now and tells the views that every cell may have changed
    /// (fireTableDataChanged()).
    void refresh();

    /// The header text of @p column (Columns.toString()).
    [[nodiscard]] static QString columnName(int column);
    /// The text of the cell @p row, @p column (Columns.getValue()); empty for a cell the fin set
    /// does not have.
    [[nodiscard]] QString valueText(int row, int column) const;

    /// What "Export CSV" writes (FreeformFinSetConfig.writeCSVFile()): a line of the column
    /// names, then one per row, each text followed by ", " ("X / cm, Y / cm, ", "2.5, 5, ").
    /// Deviation from OpenRocket: the lines end with '\n' (Java: "\r\n").
    [[nodiscard]] QString csvText() const;

Q_SIGNALS:
    /// The cell editor gave the point @p row the coordinates (@p x, @p y), in m.
    void pointEntered(int row, double x, double y);

private:
    [[nodiscard]] int pointCount() const;

    FinPointSelection                       m_target;
    int                                     m_rows{0};
    ComponentChangeSignal::ScopedConnection m_rocketConnection;
};

}  // namespace QtRocket::Gui
