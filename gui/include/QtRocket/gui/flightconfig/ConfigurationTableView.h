#pragma once

#include <QItemSelectionModel>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTableView>

class QEvent;
class QKeyEvent;
class QModelIndex;
class QMouseEvent;
class QWidget;

namespace QtRocket::Gui
{

class RowColumnSelectionModel;

/// The table of a panel of the "Motors & Configuration" tab (FlightConfigurablePanel): a
/// QTableView whose mouse and keys select as a JTable's look and feel does (BasicTableUI), with a
/// RowColumnSelectionModel as its selection model:
/// - a press (or double click) of the left button starts a gesture
///   (RowColumnSelectionModel::setValueIsAdjusting()), and its release ends it once the view has
///   selected, then leftButtonReleased() tells of the release (Java's mouseClicked() listeners run
///   after BasicTableUI's release); Select All (Ctrl+A) is one gesture too (JTable.selectAll():
///   its changes adjusting, then the rows' and the columns' change);
/// - each press, move and key tells the selection whether Ctrl is held (Cmd on macOS: Qt's
///   ControlModifier) and the cell under the mouse (RowColumnSelectionModel::setToggleHeld(),
///   setInputCell()), so that Shift and drags extend from the anchor as a JTable's do;
/// - it has no type-ahead search (keyboardSearch() does nothing): a JTable does nothing for a
///   typed key on a cell it does not edit, where Qt's view moves the selection to the next row
///   whose text starts with it (and so the rocket's selected configuration).
/// With another selection model it is a QTableView without the search.
class ConfigurationTableView final : public QTableView
{
    Q_OBJECT

public:
    explicit ConfigurationTableView(QWidget* parent = nullptr);
    ~ConfigurationTableView() override;

    ConfigurationTableView(const ConfigurationTableView&)            = delete;
    ConfigurationTableView& operator=(const ConfigurationTableView&) = delete;
    ConfigurationTableView(ConfigurationTableView&&)                 = delete;
    ConfigurationTableView& operator=(ConfigurationTableView&&)      = delete;

    /// Nothing (see the class comment).
    void keyboardSearch(const QString& search) override;

Q_SIGNALS:
    /// The left button was released at @p position of the viewport, after the view and the
    /// selection took the release.
    void leftButtonReleased(const QPoint& position);

protected:
    /// Qt's command, but a press of the left button without Shift or Ctrl on a selected cell
    /// selects it alone at once, as a JTable's (Qt waits for the release there, to drag the
    /// selected cells away, which this view does not do): a drag from it extends from it.
    [[nodiscard]] QItemSelectionModel::SelectionFlags selectionCommand(
        const QModelIndex& index, const QEvent* event = nullptr) const override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// The selection model, when it is a RowColumnSelectionModel; null otherwise.
    [[nodiscard]] RowColumnSelectionModel* cells() const;
    /// Tells the selection of the mouse event @p event: Ctrl, the cell under the mouse.
    void tellMouse(const QMouseEvent& event) const;
    /// Tells the selection that no event of the view is being handled.
    void forgetInput() const;
};

}  // namespace QtRocket::Gui
