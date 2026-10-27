#include "QtRocket/gui/flightconfig/ConfigurationTableView.h"

#include <QEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QString>
#include <QTableView>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"

namespace QtRocket::Gui
{

ConfigurationTableView::ConfigurationTableView(QWidget* parent) : QTableView(parent) { }

ConfigurationTableView::~ConfigurationTableView() = default;

void ConfigurationTableView::keyboardSearch(const QString& /*search*/) { }

QItemSelectionModel::SelectionFlags ConfigurationTableView::selectionCommand(
    const QModelIndex& index, const QEvent* event) const
{
    const QItemSelectionModel::SelectionFlags command = QTableView::selectionCommand(index, event);
    const auto* const                         mouse   = dynamic_cast<const QMouseEvent*>(event);
    if (mouse != nullptr && mouse->type() == QEvent::MouseButtonPress &&
        command == QItemSelectionModel::NoUpdate && index.isValid() &&
        mouse->button() == Qt::LeftButton &&
        (mouse->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) == Qt::NoModifier)
    {
        return QItemSelectionModel::ClearAndSelect;
    }
    return command;
}

RowColumnSelectionModel* ConfigurationTableView::cells() const
{
    return qobject_cast<RowColumnSelectionModel*>(selectionModel());
}

void ConfigurationTableView::tellMouse(const QMouseEvent& event) const
{
    if (RowColumnSelectionModel* const selection = cells(); selection != nullptr)
    {
        selection->setToggleHeld(event.modifiers().testFlag(Qt::ControlModifier));
        selection->setInputCell(indexAt(event.position().toPoint()));
    }
}

void ConfigurationTableView::forgetInput() const
{
    if (RowColumnSelectionModel* const selection = cells(); selection != nullptr)
    {
        selection->setInputCell(QModelIndex());
    }
}

void ConfigurationTableView::mousePressEvent(QMouseEvent* event)
{
    tellMouse(*event);
    if (RowColumnSelectionModel* const selection = cells();
        selection != nullptr && event->button() == Qt::LeftButton)
    {
        // BasicTableUI: a press starts the adjusting of both selection models.
        selection->setValueIsAdjusting(true);
    }
    QTableView::mousePressEvent(event);
    forgetInput();
}

void ConfigurationTableView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // A JTable's second press of a double click is a press.
    tellMouse(*event);
    if (RowColumnSelectionModel* const selection = cells();
        selection != nullptr && event->button() == Qt::LeftButton)
    {
        selection->setValueIsAdjusting(true);
    }
    QTableView::mouseDoubleClickEvent(event);
    forgetInput();
}

void ConfigurationTableView::mouseMoveEvent(QMouseEvent* event)
{
    tellMouse(*event);
    QTableView::mouseMoveEvent(event);
    forgetInput();
}

void ConfigurationTableView::mouseReleaseEvent(QMouseEvent* event)
{
    tellMouse(*event);
    QTableView::mouseReleaseEvent(event);
    forgetInput();
    if (event->button() != Qt::LeftButton)
    {
        return;
    }
    // BasicTableUI's release ends the adjusting (the rows' model first), before the mouseClicked()
    // listeners.
    if (RowColumnSelectionModel* const selection = cells(); selection != nullptr)
    {
        selection->setValueIsAdjusting(false);
    }
    Q_EMIT leftButtonReleased(event->position().toPoint());
}

void ConfigurationTableView::keyPressEvent(QKeyEvent* event)
{
    RowColumnSelectionModel* const selection = cells();
    if (selection == nullptr)
    {
        QTableView::keyPressEvent(event);
        return;
    }
    selection->setToggleHeld(event->modifiers().testFlag(Qt::ControlModifier));
    selection->setInputCell(QModelIndex());
    if (event->matches(QKeySequence::SelectAll))
    {
        // JTable.selectAll(): its changes adjusting, then the rows' and the columns' change.
        selection->setValueIsAdjusting(true);
        QTableView::keyPressEvent(event);
        selection->setValueIsAdjusting(false);
        return;
    }
    QTableView::keyPressEvent(event);
}

}  // namespace QtRocket::Gui
