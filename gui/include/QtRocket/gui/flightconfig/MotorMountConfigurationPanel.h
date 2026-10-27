#pragma once

#include <QModelIndex>
#include <QObject>
#include <QPersistentModelIndex>
#include <QSize>
#include <QWidget>

class QEvent;
class QTableView;

namespace QtRocket::Gui
{

class FlightConfigurationEditing;
class MotorMountTableModel;
class TableTransferKeys;

/// The table of the "Motor mounts" box of the Motors tab, where the user tells which tubes act as
/// motor mounts (OpenRocket's MotorMountConfigurationPanel, swing
/// gui/dialogs/flightconfiguration/MotorMountConfigurationPanel.java): a table ("table") of
/// MotorMountTableModel, a row per motor mount of the rocket with its check box and its name,
/// without a header, a grid or a selection, its check box column as wide as a row is high plus 2
/// pixels and its name column the rest; 200 x 150 pixels as it would be (its size hint), growing
/// in height ("w 200lp, h 150lp, grow, pushy").
///
/// A click of the left button on a row's check box or name toggles the check box (Java: the
/// check box's cell editor, and GUIUtil.BooleanTableClickListener on the name), and so does each
/// click of a double click, as in Java. The model asks for the change and the panel makes it
/// through FlightConfigurationEditing::setMotorMount() (its own undo step "Modify motor mount":
/// OpenRocket's bug 5, plan 10.3; Java's table changes the mount at once, with no undo step).
///
/// Cut, Copy and Paste (Ctrl+X, C and V) are the table's while it has the focus, as a JTable's
/// (TableTransferKeys), and do nothing (a table that allows no selection exports nothing): the
/// window's items of those keys (the components' Cut, Copy and Paste) do not act then.
///
/// Deviation from OpenRocket: the table has no grid lines (Java hides its vertical ones, and its
/// look and feel, FlatLaf, draws no horizontal ones; Qt draws both or none).
class MotorMountConfigurationPanel final : public QWidget
{
    Q_OBJECT

public:
    /// The table over @p editing's rocket, whose mounts it changes through @p editing (which
    /// outlives it), a child of @p parent.
    MotorMountConfigurationPanel(FlightConfigurationEditing& editing, QWidget* parent);
    ~MotorMountConfigurationPanel() override;

    MotorMountConfigurationPanel(const MotorMountConfigurationPanel&)            = delete;
    MotorMountConfigurationPanel& operator=(const MotorMountConfigurationPanel&) = delete;
    MotorMountConfigurationPanel(MotorMountConfigurationPanel&&)                 = delete;
    MotorMountConfigurationPanel& operator=(MotorMountConfigurationPanel&&)      = delete;

    /// The table ("table").
    [[nodiscard]] QTableView& table() const noexcept { return *m_table; }
    /// The table's model.
    [[nodiscard]] MotorMountTableModel& model() const noexcept { return *m_model; }
    /// The table's Cut, Copy and Paste keys.
    [[nodiscard]] TableTransferKeys& transferKeys() const noexcept { return *m_transferKeys; }

    /// The size the table would be ("w 200lp, h 150lp").
    static constexpr int kTableWidth  = 200;
    static constexpr int kTableHeight = 150;

    /// The table's size (kTableWidth x kTableHeight) with the margins.
    [[nodiscard]] QSize sizeHint() const override;

    /// The left button's clicks in the table's viewport (see the class comment).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// The check box column's width: a row's height plus 2.
    void sizeCheckColumn();
    /// Toggles the check box of @p index's row.
    void toggle(const QModelIndex& index);

    FlightConfigurationEditing* m_editing;
    MotorMountTableModel*       m_model;
    QTableView*                 m_table;
    TableTransferKeys*          m_transferKeys;
    /// The cell of the left button's last press (a click toggles when it ends there).
    QPersistentModelIndex m_pressed;
};

}  // namespace QtRocket::Gui
