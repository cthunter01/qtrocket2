#pragma once

#include <array>
#include <memory>

#include <QObject>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPointer>
#include <QTableView>

#include "QtRocket/gui/dialogs/simulation/MultiLevelWindTableModel.h"

class QAction;
class QContextMenuEvent;
class QKeyEvent;
class QMenu;
class QMouseEvent;
class QWidget;

namespace QtRocket::Gui
{

class UnitSelector;

/// The table of the wind editor's levels (OpenRocket's MultiLevelWindTable, swing gui/simulation/,
/// as the dialog shows it): a QTableView of a MultiLevelWindTableModel with Java's column widths
/// (kColumnWidths), a unit selector under the name of each number column in the header (Java: the
/// header cell's UnitSelector, the group's default unit to begin with), which gives the column its
/// unit (MultiLevelWindTableModel::setColumnUnit(), and the profile plot through
/// columnUnitChanged()), and the editing, selection and deletion of Java's rows:
/// - A number cell is edited in a UnitSpinBox over the cell's binding
///   (MultiLevelWindTableModel::makeCellBinding(); Java: the row's JSpinner over its DoubleModel),
///   opened on demand (the user's decision U-M4-6): by a press of the left button on the cell (as
///   a click into Java's spinner), a double click, F2 or typing. Enter, the arrows and a focus loss
///   commit it, Tab moves to the next cell's editor, Escape drops the edit.
/// - One row is selected at a time (Java: the row whose spinner has the focus), drawn in
///   OR.colors.table.selection; selectedLevelChanged() tells of a new selection (the profile
///   plot's highlighted level and the dialog's "Delete level" button follow it).
/// - The Delete column's trash icon deletes its row when clicked (Java: the row's delete button),
///   as do the popup menu's "Delete level" (a right click on a row, which selects it; Java's
///   popupmenu.Delete) and the Delete key (QtRocket's; the selected row), unless it is the last
///   level. The selection is cleared after a deletion (Java: selectRow(null)).
///
/// OpenRocket bug 33, fixed (Milestone 4): Java selects a row while one of its spinners has the
/// focus and clears the selection when the focus leaves, so the dialog's "Delete level" button,
/// which takes the focus when it is pressed, is disabled before it is released and never deletes
/// with the mouse. Here the selection stays when the focus leaves the table.
///
/// Deviations from OpenRocket: a QTableView with editors opened on demand (U-M4-6), with Qt's
/// header and grid; the Delete key; the selection kept when the popup menu closes (Java clears it).
class MultiLevelWindTableView final : public QTableView
{
    Q_OBJECT

public:
    /// Java's ColumnDefinition widths.
    static constexpr std::array<int, MultiLevelWindTableModel::kColumnCount> kColumnWidths{
        100, 100, 90, 100, 90, 85, 60};
    /// The gap between a column's name and its unit selector (Java: Box.createVerticalStrut(3)).
    static constexpr int kHeaderGap = 3;

    /// The table of @p model, which must outlive it.
    MultiLevelWindTableView(MultiLevelWindTableModel& model, QWidget* parent);
    ~MultiLevelWindTableView() override;
    MultiLevelWindTableView(const MultiLevelWindTableView&)            = delete;
    MultiLevelWindTableView& operator=(const MultiLevelWindTableView&) = delete;
    MultiLevelWindTableView(MultiLevelWindTableView&&)                 = delete;
    MultiLevelWindTableView& operator=(MultiLevelWindTableView&&)      = delete;

    [[nodiscard]] MultiLevelWindTableModel& levelModel() const noexcept { return *m_model; }

    /// The header's unit selector of a number column (object names Java's fields:
    /// altitudeUnitSelector, speedUnitSelector, directionUnitSelector, stdDeviationUnitSelector,
    /// turbulenceUnitSelector).
    /// @throws BugError for a column that is no number column
    [[nodiscard]] UnitSelector& unitSelector(int column) const;

    /// The selected row, or -1.
    [[nodiscard]] int selectedRow() const;
    /// The level of the selected row, or null (valid until the wind model's next change).
    [[nodiscard]] MultiLevelWindTableModel::Level* selectedLevel() const;

    /// Java's addRow() for the dialog's "Add new level": commits an open editor
    /// (commitEditing()), adds the level (MultiLevelWindTableModel::addLevel()) and scrolls to its
    /// row. The row, or -1 when none was added.
    int addLevel();

    /// Java's deleteSelectedRow(): deletes the selected row's level (deleteLevel()); false when no
    /// row is selected or it is the last level.
    bool deleteSelectedLevel();

    /// Deletes the level of row @p row unless it is the last (MultiLevelWindTableModel::
    /// deleteLevel()), dropping an open editor first, and clears the selection: whether a level
    /// was deleted.
    bool deleteLevel(int row);

    /// The open cell editor, or null.
    [[nodiscard]] QWidget* openEditor() const;

    /// Commits the open cell editor's text (as its focus loss would) and closes it: whether one
    /// was open. The dialog calls it before its buttons act and when it closes (a button that
    /// takes no focus leaves the edit pending: macOS).
    bool commitEditing();

    /// The popup menu of the rows (a right click on a row; objectName "popupMenu") and its
    /// "Delete level" (popupmenu.Delete, objectName "deleteItem", Java's field).
    [[nodiscard]] QMenu&   popupMenu() const noexcept { return *m_popupMenu; }
    [[nodiscard]] QAction& deleteAction() const noexcept { return *m_deleteAction; }

    /// What a right click on row @p row does: selects the row and opens the popup menu at
    /// @p globalPosition, its "Delete level" enabled while there is more than one level.
    void showPopupMenu(int row, const QPoint& globalPosition);

Q_SIGNALS:
    /// The selected row, or its level, may have changed.
    void selectedLevelChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void updateGeometries() override;
    void scrollContentsBy(int dx, int dy) override;

private:
    /// Puts each unit selector under its column's name.
    void layOutUnitSelectors();
    /// The delegate opened @p editor for @p index.
    void editorOpened(QWidget& editor, const QModelIndex& index);
    /// The popup menu's "Delete level".
    void deletePopupLevel();

    MultiLevelWindTableModel*                                               m_model;
    std::array<UnitSelector*, MultiLevelWindTableModel::kNumberColumnCount> m_unitSelectors{};
    QMenu*                                                                  m_popupMenu{nullptr};
    QAction*                                                                m_deleteAction{nullptr};
    /// The level the popup menu was opened on.
    std::weak_ptr<MultiLevelWindTableModel::Level> m_popupLevel;
    /// The editor the delegate opened last, and its cell.
    QPointer<QWidget>     m_editor;
    QPersistentModelIndex m_editorIndex;
};

}  // namespace QtRocket::Gui
