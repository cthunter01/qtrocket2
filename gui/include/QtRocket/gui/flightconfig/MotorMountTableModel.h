#pragma once

#include <optional>
#include <span>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QVariant>
#include <Qt>

#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class MotorMount;
class Rocket;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The table of the "Motor mounts" box of the Motors tab, where the user tells which tubes are
/// motor mounts (OpenRocket's MotorMountTableModel, swing
/// gui/dialogs/flightconfiguration/MotorMountTableModel.java): a row per MotorMount of the rocket,
/// acting or not, in the rocket's order (pre-order); column 0 a check box, checked for an acting
/// motor mount (Qt::CheckStateRole; Java: a Boolean column), column 1 the component's name
/// (Qt::DisplayRole; Java: toString()). No header (Java's table has none). Only the check box can
/// be changed (Qt::ItemIsUserCheckable; Java: isCellEditable() of column 0).
///
/// A change of the check box (setData() with Qt::CheckStateRole on column 0) does not change the
/// rocket itself: it asks for the change with motorMountChangeRequested(), and the panel applies it
/// (Java's setValueAt() calls mount.setMotorMount(value) at once, with no undo position: OpenRocket
/// bug 5, plan 10.3; QtRocket's operation, part F3, adds one). A value equal to the mount's state
/// asks for nothing (setMotorMount() would do nothing).
///
/// The rows are keyed by the mounts' ids (Uuid), never their objects: every data() looks the mount
/// up in the rocket (Rocket::findComponent()), so that nothing reads a component that an undo or a
/// frozen operation replaced or destroyed before its event; a row whose mount has gone answers
/// nothing until the event comes. At every change event of the rocket the model compares its ids
/// with the rocket's mounts: when they differ it resets (beginResetModel() ... endResetModel(),
/// again while changes come during the reset), otherwise it tells the views that every cell may
/// have changed (Java: fireTableStructureChanged() on every MOTOR and TREE event, which the
/// table's header-less view answers by repainting).
///
/// The rocket must outlive the model.
class MotorMountTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// The check box.
    static constexpr int kMotorMountColumn = 0;
    /// The component's name.
    static constexpr int kNameColumn  = 1;
    static constexpr int kColumnCount = 2;

    /// The table of @p rocket's motor mounts.
    explicit MotorMountTableModel(Rocket& rocket, QObject* parent = nullptr);
    ~MotorMountTableModel() override;

    MotorMountTableModel(const MotorMountTableModel&)            = delete;
    MotorMountTableModel& operator=(const MotorMountTableModel&) = delete;
    MotorMountTableModel(MotorMountTableModel&&)                 = delete;
    MotorMountTableModel& operator=(MotorMountTableModel&&)      = delete;

    // ------------------------------------------------------------------ QAbstractTableModel

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// Column 0: Qt::CheckStateRole, Qt::Checked for an acting motor mount; column 1:
    /// Qt::DisplayRole, the name.
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int                role = Qt::DisplayRole) const override;
    /// Nothing: the table has no header (Java: setTableHeader(null)).
    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation,
                                      int role = Qt::DisplayRole) const override;
    /// Enabled, never selectable (Java's table allows no row or column selection); column 0
    /// user-checkable.
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// Asks for the check box of @p index to be @p value (see the class comment): true when the
    /// request was made or was not needed, false for another column or role, a value that is
    /// not a check state, a row the table does not have and a mount that has gone.
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // ---------------------------------------------------------------------------- the keys

    /// The rocket.
    [[nodiscard]] Rocket& rocket() const noexcept { return *m_rocket; }

    /// The mounts' ids, in the order of the rows.
    [[nodiscard]] std::span<const Uuid> mountIds() const noexcept { return m_mountIds; }

    /// The id of @p row's mount; nullopt for a row the table does not have.
    [[nodiscard]] std::optional<Uuid> mountId(int row) const;

    /// The mount of @p row, looked up by its id; null for a row the table does not have and a
    /// mount that is not in the rocket any more.
    [[nodiscard]] MotorMount* mount(int row) const;

    /// The row of the mount @p id; -1 when the table has none.
    [[nodiscard]] int rowOf(const Uuid& id) const noexcept;

    /// Takes the rocket's mounts as a change event does (see the class comment).
    void refresh();

Q_SIGNALS:
    /// The check box of the mount @p mountId was changed to @p motorMount: the mount should become
    /// an acting motor mount (or stop being one).
    void motorMountChangeRequested(const QtRocket::Uuid& mountId, bool motorMount);

private:
    /// Whether @p index is a cell of this model (QAbstractItemModel::checkIndex() without its
    /// warning: views and QAbstractItemModelTester ask for the invalid index).
    [[nodiscard]] bool              owns(const QModelIndex& index) const noexcept;
    [[nodiscard]] std::vector<Uuid> currentMounts() const;
    void                            reset(std::vector<Uuid> mounts);
    void                            emitEverythingChanged();

    Rocket*                                 m_rocket;
    std::vector<Uuid>                       m_mountIds;
    bool                                    m_resetting{false};
    bool                                    m_changedWhileResetting{false};
    ComponentChangeSignal::ScopedConnection m_changes;
};

}  // namespace QtRocket::Gui
