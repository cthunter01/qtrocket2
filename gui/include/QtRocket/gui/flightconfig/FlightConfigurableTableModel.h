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
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class Preferences;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The table of one of the "Motors & Configuration" tab's three panels (OpenRocket's
/// FlightConfigurableTableModel<T> with the subclass each panel makes in its initializeTable(),
/// and the texts of the panel's cell renderer): a row per flight configuration of the rocket, in
/// getIds() order (the default configuration is never a row); column 0 the configuration's name,
/// then a column per component of the kind(), in the rocket's order (pre-order):
/// - MOTOR_MOUNTS (MotorConfigurationPanel): every MotorMount that is an acting motor mount
///   (isMotorMount());
/// - RECOVERY_DEVICES (RecoveryConfigurationPanel): every RecoveryDevice;
/// - STAGES (SeparationConfigurationPanel): every AxialStage of stage number > 0, the boosters'
///   ParallelStages included.
///
/// The texts (FlightConfigurationCellText) are in data(): Qt::DisplayRole has column 0's name, a
/// motor cell's motor label ("None", "2× A10-0"), a recovery cell's deployment and a stage
/// cell's separation; kIgnitionTextRole a motor cell's ignition label; kDefaultRole whether the
/// cell shows a default value (a motor cell's ignition, a recovery or stage cell's whole text),
/// which Java shades. A shaded recovery or stage cell also has an italic Qt::FontRole and the
/// TEXT_DIM colour (ThemeColors, of the application's palette) as Qt::ForegroundRole, so that a
/// plain QStyledItemDelegate draws it as Java's renderer; a motor cell's two labels are drawn by
/// MotorCellDelegate. Column 0 and the unshaded texts take the view's font and palette (Java:
/// plain, UITheme's TEXT). The headers: "Configuration" and the components' names; no vertical
/// header (Java's table has none). Every cell is selectable and enabled, none editable.
///
/// KEYS. The model keeps the configuration ids of its rows and the component ids (Uuid) of its
/// columns, never a component: every data() looks its component up by id in the rocket
/// (Rocket::findComponent()), so that nothing reads a component that Rocket::loadFrom() (an undo
/// or a redo) or a frozen operation replaced or destroyed before the event told of it; a column
/// whose component has gone answers nothing until the event comes.
///
/// EVENTS. At every change event of the rocket the model compares its keys with the rocket's (its
/// ids, its components of the kind, in order): when they differ, it takes the new ones in a model
/// reset (beginResetModel() ... endResetModel()); otherwise it tells the views that every cell and
/// header may have changed (dataChanged(), headerDataChanged()). A change made while the views
/// answer a reset is taken by one more reset after it. refresh() does the same on request.
///
/// Deviations from OpenRocket:
/// - Java answers every MOTOR and TREE event (an undo is TREE | UNDO) with
///   fireTableStructureChanged(), which also clears the JTable's selection, and nothing else. Here
///   the model resets only when its rows or columns change, and keeps the views' selection
///   otherwise; it also resets for the events after which Java's JTable reads a new row count
///   without any model event (a configuration added or removed: NONFUNCTIONAL).
/// - Java refreshes a header on a name change through each panel's own listener (the motor table's
///   structure change on a NONFUNCTIONAL event of a BodyTube or an InnerTube, the stage table's on
///   one of an AxialStage, the recovery table's header values set on any event of a
///   FlightConfigurableComponent); here every event refreshes every header.
/// - Recovery and stage cells are shaded by !containsId(id), not Java's isDefault(id)
///   (FlightConfigurationCellText; OpenRocket bug 1, plan 10.3).
///
/// The rocket and the preferences must outlive the model.
class FlightConfigurableTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    /// Which components the columns are.
    enum class Kind
    {
        MOTOR_MOUNTS,
        RECOVERY_DEVICES,
        STAGES,
    };

    /// Whether the cell shows a default value (a bool; column 0: false): a motor cell's ignition
    /// when its configuration has no ignition override; a recovery or stage cell when the
    /// configuration has no override of the deployment or the separation (!containsId()).
    static constexpr int kDefaultRole = Qt::UserRole + 1;
    /// A motor cell's ignition label (a QString; nothing for the other cells).
    static constexpr int kIgnitionTextRole = Qt::UserRole + 2;

    /// The table of @p kind over @p rocket; the motors' and configurations' names take
    /// @p preferences (designation or common name, the default configuration name).
    FlightConfigurableTableModel(Kind kind, Rocket& rocket, const Preferences& preferences,
                                 QObject* parent = nullptr);
    ~FlightConfigurableTableModel() override;

    FlightConfigurableTableModel(const FlightConfigurableTableModel&)            = delete;
    FlightConfigurableTableModel& operator=(const FlightConfigurableTableModel&) = delete;
    FlightConfigurableTableModel(FlightConfigurableTableModel&&)                 = delete;
    FlightConfigurableTableModel& operator=(FlightConfigurableTableModel&&)      = delete;

    // ------------------------------------------------------------------ QAbstractTableModel

    /// The number of configurations (Java: getConfigurationCount()).
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    /// 1 + the number of components.
    [[nodiscard]] int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index,
                                int                role = Qt::DisplayRole) const override;
    /// "Configuration" and the components' names (Qt::DisplayRole, horizontal only).
    [[nodiscard]] QVariant      headerData(int section, Qt::Orientation orientation,
                                           int role = Qt::DisplayRole) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;

    // ---------------------------------------------------------------------------- the keys

    /// Which components the columns are.
    [[nodiscard]] Kind kind() const noexcept { return m_kind; }

    /// The rocket.
    [[nodiscard]] Rocket& rocket() const noexcept { return *m_rocket; }

    /// Whether @p component has a column in a table of @p kind (Java: the panel's class and its
    /// includeComponent()).
    [[nodiscard]] static bool includes(Kind kind, const RocketComponent& component);

    /// The configuration ids of the rows, in order.
    [[nodiscard]] std::span<const FlightConfigurationId> configurationIds() const noexcept
    {
        return m_configurationIds;
    }

    /// The component ids of the columns 1, 2, ..., in order.
    [[nodiscard]] std::span<const Uuid> componentIds() const noexcept { return m_componentIds; }

    /// The configuration id of @p row; nullopt for a row the table does not have.
    [[nodiscard]] std::optional<FlightConfigurationId> configurationId(int row) const;

    /// The component id of @p column; nullopt for column 0 and a column the table does not have.
    [[nodiscard]] std::optional<Uuid> componentId(int column) const;

    /// The component of @p column, looked up by its id; null for column 0, a column the table
    /// does not have and a component that is not in the rocket any more.
    [[nodiscard]] RocketComponent* component(int column) const;

    /// The row of the configuration @p id; -1 when the table has none.
    [[nodiscard]] int rowOf(const FlightConfigurationId& id) const noexcept;

    /// The column of the component @p id; -1 when the table has none.
    [[nodiscard]] int columnOf(const Uuid& id) const noexcept;

    /// Takes the rocket's keys as a change event does (see the class comment).
    void refresh();

private:
    /// The ids of the rows and the columns the rocket has now.
    struct Keys
    {
        std::vector<FlightConfigurationId> configurations;
        std::vector<Uuid>                  components;
    };

    /// Whether @p index is a cell of this model (QAbstractItemModel::checkIndex() without its
    /// warning: views and QAbstractItemModelTester ask for the invalid index).
    [[nodiscard]] bool owns(const QModelIndex& index) const noexcept;
    [[nodiscard]] Keys currentKeys() const;
    /// What data() answers for the cell of @p component in @p row.
    [[nodiscard]] QVariant componentData(const RocketComponent&       component,
                                         const FlightConfigurationId& id, int role) const;
    void                   componentChanged(const ComponentChangeEvent& event);
    /// Takes @p keys in a model reset, and again while changes come during it.
    void reset(Keys keys);
    /// dataChanged() for every cell and headerDataChanged() for every column.
    void emitEverythingChanged();

    Kind                                    m_kind;
    Rocket*                                 m_rocket;
    const Preferences*                      m_preferences;
    std::vector<FlightConfigurationId>      m_configurationIds;
    std::vector<Uuid>                       m_componentIds;
    bool                                    m_resetting{false};
    bool                                    m_changedWhileResetting{false};
    ComponentChangeSignal::ScopedConnection m_changes;
};

}  // namespace QtRocket::Gui
