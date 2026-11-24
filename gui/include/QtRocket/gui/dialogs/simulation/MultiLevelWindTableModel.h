#pragma once

#include <array>
#include <chrono>
#include <memory>
#include <vector>

#include <QAbstractTableModel>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/models/MultiLevelPinkNoiseWindModel.h"
#include "QtRocket/models/WindModel.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{
class Preferences;
class Unit;
class UnitGroup;
}  // namespace QtRocket

namespace QtRocket::Gui
{

class DoubleBinding;

/// The table of the wind editor (OpenRocket's MultiLevelWindTable, swing gui/simulation/): a row
/// per level of a MultiLevelPinkNoiseWindModel, in the model's order, which this table keeps sorted
/// by altitude (OpenRocket bug 32, below), and the columns of Java's ColumnDefinitions:
/// - "Altitude MSL" or "Altitude AGL", as the model's altitude reference says (with its tooltip
///   "Altitude above mean sea level" or "Altitude above ground level (the launch site)"), in a unit
///   of UnitGroup DISTANCE, from -100 m (Java's DoubleModel minimum);
/// - "Speed", WINDSPEED, from 0; "Direction" (with Java's tooltip of the compass), ANGLE, 0 to
///   2 pi; "Deviation", WINDSPEED, from 0 to the level's speed (Java: the speed's DoubleModel as
///   the maximum); "Turbulence", RELATIVE, 0 to 1;
/// - "Intensity": the description of the level's turbulence intensity
///   (TurbulenceIntensityText); "Delete": the trash icon (Icons EDIT_DELETE, tooltip "Delete the
///   selected wind level"), enabled while there is more than one level.
/// A number cell's text is its value in its column's unit without the unit's name (Java: the
/// spinner's text, the unit's toString()); every number column has a unit of its own
/// (setColumnUnit(): the header's unit selector), the group's default unit to begin with.
///
/// Editing: a number cell is edited through makeCellBinding(), a DoubleBinding over the level
/// (Java: the row's DoubleModel of the level's property), which the table's spin box editor shows
/// and commits (MultiLevelWindTableView); setData() takes a text the same way (an expression in the
/// column's unit, clamped to the bounds). The bounds are Java's minima and, where Java has none,
/// the maxima of the simulation's input ranges (SimulationInputRanges: an altitude up to
/// 1000000 m, a speed up to 1000 m/s; the user's decision U-M4-7). The level's setter does the
/// rest, as in Java: a new speed keeps the turbulence intensity (the deviation scales along), a
/// deviation or a turbulence intensity sets the other.
///
/// The rows follow the wind model: on every change it announces (changed(): a level added,
/// removed, edited, the levels cleared, reset, imported, the altitude reference) the table takes
/// the model's levels again: rows inserted or removed where levels were, rows moved for a new
/// order (with the persistent indexes, so an open editor and the selection move along), the whole
/// table reset otherwise, and every cell told that it may have changed. It holds its levels by
/// std::weak_ptr, so a level the model dropped without a word (loadFrom()) is a row that shows
/// nothing until the next change. The model must stay at its address while the table shows it; the
/// table learns that it has gone from its change signal, from then on has no rows and touches it no
/// more (hasModel()).
///
/// The actions of Java's buttons: addLevel() (Add new level), deleteLevel() (a row's trash button,
/// the popup's "Delete level", the dialog's "Delete level"), resetLevels() (Reset levels, after the
/// dialog's question). A row the table changes flashes for kFlashDuration in the colour
/// OR.colors.table.flash (Java's flashMovement(): the added row, the row that takes a deleted
/// row's place, and the rows around a level an altitude edit moved), the rows otherwise
/// alternating OR.colors.row.lighter and OR.colors.row.darker (the BackgroundRole); the view
/// paints the selected row in OR.colors.table.selection.
///
/// OpenRocket bugs fixed (Milestone 4):
/// - Bug 31: Java lets two levels have one altitude (setAltitude() checks nothing), and then
///   removes both when one is deleted (removeWindLevel() by altitude), and the design saved with
///   the two cannot be opened ("Wind level already exists for altitude"). Here an altitude that
///   another level has is refused (the cell keeps the level's altitude), and a row is deleted by
///   its index.
/// - Bug 32: Java re-sorts the table's rows after an altitude edit but not the model's levels, so
///   the wind is interpolated between the wrong levels (and the levels are seeded in the wrong
///   order) until the design is saved and opened again. Here the levels are sorted again
///   (MultiLevelPinkNoiseWindModel::sortLevels()) whenever a change leaves them out of order.
///
/// Deviations from OpenRocket:
/// - A table model under a QTableView with editors opened on demand (the user's decision U-M4-6),
///   where Java has a panel of rows of spinners.
/// - The Delete column is enabled whenever there is more than one level; Java sets its rows'
///   buttons when it adds or deletes a row, not after Reset levels or an import, so a lone level
///   left by those keeps an enabled button that does nothing.
class MultiLevelWindTableModel final : public QAbstractTableModel
{
    Q_OBJECT

public:
    using Level = MultiLevelPinkNoiseWindModel::LevelWindModel;

    static constexpr int kAltitudeColumn   = 0;
    static constexpr int kSpeedColumn      = 1;
    static constexpr int kDirectionColumn  = 2;
    static constexpr int kDeviationColumn  = 3;
    static constexpr int kTurbulenceColumn = 4;
    static constexpr int kIntensityColumn  = 5;
    static constexpr int kDeleteColumn     = 6;
    static constexpr int kColumnCount      = 7;
    /// The columns of numbers, each with a unit: the first five.
    static constexpr int kNumberColumnCount = 5;

    /// How much higher than the highest level addLevel() adds one, m (ALTITUDE_INCREASE).
    static constexpr double kAltitudeIncrease = 100;
    /// The lowest altitude a cell takes, m (the altitude's DoubleModel minimum).
    static constexpr double kMinimumAltitude = -100;
    /// How long a changed row flashes (FLASH_DURATION_MS).
    static constexpr std::chrono::milliseconds kFlashDuration{800};

    /// The table of @p model's levels. @p preferences give addLevel() its speed when there is no
    /// level and resetLevels() the level it makes (the preferences' average wind); they must
    /// outlive the table.
    MultiLevelWindTableModel(MultiLevelPinkNoiseWindModel& model, const Preferences& preferences,
                             QObject* parent);
    ~MultiLevelWindTableModel() override;
    MultiLevelWindTableModel(const MultiLevelWindTableModel&)            = delete;
    MultiLevelWindTableModel& operator=(const MultiLevelWindTableModel&) = delete;
    MultiLevelWindTableModel(MultiLevelWindTableModel&&)                 = delete;
    MultiLevelWindTableModel& operator=(MultiLevelWindTableModel&&)      = delete;

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    /// The display and edit roles: a number cell's text (cellText()), the intensity's description;
    /// the Delete column's icon (DecorationRole) and tooltip; the row's background (see the class
    /// comment).
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    /// The column names (columnName()) with their tooltips, aligned to the top of the header (the
    /// view puts the unit selectors below them).
    [[nodiscard]] QVariant      headerData(int section, Qt::Orientation orientation,
                                           int role) const override;
    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override;
    /// Reads @p value as the text of a number cell (an expression in the column's unit) and sets
    /// it as the editor does (makeCellBinding()): true when the text was read and set, false for
    /// another column or role, text that is no expression, a row that has gone. An altitude
    /// another level has is refused all the same (the level keeps its own; bug 31).
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    /// Whether the wind model is still there (see the class comment).
    [[nodiscard]] bool hasModel() const noexcept;

    /// The level of row @p row, or null when there is none (out of range, gone, or the model
    /// has). The pointer is valid until the wind model's next change.
    [[nodiscard]] Level* levelAt(int row) const;
    /// The row of @p level, or -1.
    [[nodiscard]] int rowOf(const Level* level) const;

    /// The header text of @p column: the altitude's by @p reference (MultiLevelWindTable.col.*).
    [[nodiscard]] static QString columnName(int column, WindModel::AltitudeReference reference);
    /// The unit group of a number column.
    /// @throws BugError for a column that is no number column
    [[nodiscard]] static const UnitGroup& columnUnitGroup(int column);
    /// The unit a number column shows its values in.
    /// @throws BugError for a column that is no number column
    [[nodiscard]] const Unit& columnUnit(int column) const;
    /// Shows the values of a number column in @p unit from now on (the header's unit selector;
    /// Java: updateAltitudeUnits() and the others), which tells of it with columnUnitChanged().
    /// @throws BugError for a column that is no number column, or a unit not of its group
    void setColumnUnit(int column, const Unit& unit);

    /// The text of the cell @p row, @p column: a number cell's value in its column's unit, the
    /// intensity's description; empty for the Delete column and a row that has gone.
    [[nodiscard]] QString cellText(int row, int column) const;

    /// A binding of the number cell @p row, @p column: over the level (an ObjectSelection of it),
    /// with the column's bounds, in the column's unit; what the cell's editor shows and commits.
    /// Null for another column or a row that has gone. The binding writes the level as long as it
    /// is there, and refuses an altitude another level has (bug 31).
    [[nodiscard]] std::shared_ptr<DoubleBinding> makeCellBinding(int row, int column);

    /// Java's addRow(): adds a level kAltitudeIncrease above the highest (at 0 m when there is
    /// none) with the speed, direction and deviation of the last row (the highest; with no level,
    /// the preferences' average speed, direction 0 and deviation 0); the new row flashes. The row
    /// of the new level, or -1 when none was added: no wind model, or an altitude outside the
    /// input ranges (beyond 1000000 m, which the run would refuse; Java has no maximum) or one a
    /// level has already.
    int addLevel();

    /// Java's deleteRow(): removes the level of row @p row (by its index: bug 31) unless it is the
    /// last level; the row that takes its place flashes. Whether a level was removed.
    bool deleteLevel(int row);

    /// Java's resetLevels(): the model's levels replaced by the one level of the preferences'
    /// average wind (MultiLevelPinkNoiseWindModel::resetLevels()).
    void resetLevels();

    /// Whether row @p row is flashing (see the class comment).
    [[nodiscard]] bool isFlashing(int row) const;
    /// How long rows flash from now on (kFlashDuration by default; a test gives 0).
    void setFlashDuration(std::chrono::milliseconds duration) noexcept
    {
        m_flashDuration = duration;
    }

Q_SIGNALS:
    /// The table took the model's levels again (see the class comment).
    void levelsChanged();
    /// setColumnUnit() gave @p column a unit.
    void columnUnitChanged(int column);

private:
    /// The rows: the levels of the last refresh, and their addresses (identity: a weak pointer
    /// keeps a level's storage, std::make_shared's, so the address of a level that has gone is
    /// never another's while it is listed).
    struct Rows
    {
        std::vector<std::weak_ptr<Level>> levels;
        std::vector<const Level*>         ids;
    };

    /// The model changed: sorts its levels again when they are out of order (bug 32) and takes them
    /// (refresh()).
    void followModel();
    /// Takes the model's levels as the rows (see the class comment).
    void refresh();
    /// The rows become @p next, levels other than the rows' or in another order: moved,
    /// removed, inserted, or all new (see the class comment).
    void takeRows(Rows next);
    /// Rows for @p levels.
    [[nodiscard]] static Rows rowsOf(const std::vector<Level*>& levels);
    /// The rows become @p next, a new order of the same levels.
    void reorder(Rows next);
    /// The rows become @p next, the levels of the rows without some of them.
    void removeMissing(Rows next);
    /// The rows become @p next, the levels of the rows and others.
    void insertNew(Rows next);
    /// Tells the views that every cell and the altitude's header may have changed.
    void announceAll();

    /// Sets @p altitude on @p level unless another level has it (bug 31); the rows around the
    /// level flash when it moves.
    void setLevelAltitude(Level& level, double altitude);

    /// Flashes rows @p first to @p last (those that exist).
    void flashRows(int first, int last);
    /// One flash of @p level ended.
    void endFlash(const std::weak_ptr<Level>& level);

    [[nodiscard]] QVariant             background(int row) const;
    [[nodiscard]] static Qt::Alignment headerAlignment(int section);
    [[nodiscard]] QVariant             headerToolTip(int section) const;

    MultiLevelPinkNoiseWindModel*               m_model;
    const Preferences*                          m_preferences;
    Signal<>::ScopedConnection                  m_connection;
    Rows                                        m_rows;
    WindModel::AltitudeReference                m_reference;
    std::array<const Unit*, kNumberColumnCount> m_units{};
    /// The levels flashing, one entry per flash (a level flashed twice is listed twice; a weak
    /// pointer, so that the storage of a level that went is no other's while it is listed).
    std::vector<std::weak_ptr<Level>> m_flashing;
    std::chrono::milliseconds         m_flashDuration{kFlashDuration};
    /// The level an altitude edit is setting, while it does (its rows flash if it moves).
    const Level* m_movingLevel{nullptr};
};

}  // namespace QtRocket::Gui
