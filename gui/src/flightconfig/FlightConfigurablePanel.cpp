#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QEvent>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLatin1Char>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QString>
#include <QTableView>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/flightconfig/ConfigurationTableView.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RowColumnSelectionModel.h"
#include "QtRocket/gui/flightconfig/TableTransferKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using Lines       = RowColumnSelectionModel::Lines;
using LinesChange = RowColumnSelectionModel::LinesChange;
using Order       = RowColumnSelectionModel::Order;

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Flight configuration table");
}

/// Whether @p event is Enter, Return or the keypad's Enter, without a modifier (Java:
/// KeyStroke.getKeyStroke(VK_ENTER, 0), which the keypad's key is too).
[[nodiscard]] bool isPlainEnter(const QKeyEvent& event)
{
    const Qt::KeyboardModifiers modifiers = event.modifiers() & ~Qt::KeypadModifier;
    return (event.key() == Qt::Key_Return || event.key() == Qt::Key_Enter) &&
           modifiers == Qt::NoModifier;
}

/// The last column of a table of @p columns columns, or column 0 when it has no other (Java:
/// table.getColumnCount() > 1 ? table.getColumnCount() - 1 : 0).
[[nodiscard]] int lastColumn(int columns)
{
    return columns > 1 ? columns - 1 : 0;
}

}  // namespace

FlightConfigurablePanel::FlightConfigurablePanel(FlightConfigurationPanel&          panel,
                                                 FlightConfigurableTableModel::Kind kind,
                                                 QWidget*                           parent)
  : QWidget(parent),
    m_panel(&panel),
    m_model(makeChild<FlightConfigurableTableModel>(this, kind, panel.rocket(),
                                                    panel.services().preferences())),
    m_table(makeChild<ConfigurationTableView>(this)),
    m_selection(makeChild<RowColumnSelectionModel>(m_table, m_model)),
    m_fullPopup(makeChild<QMenu>(this)),
    // A JTable's Cut, Copy and Paste keys: the window's items of those keys do not act while the
    // table has the focus.
    m_transferKeys(makeChild<TableTransferKeys>(
        m_table, TableTransferKeys::CellSource([this] { return transferCells(); })))
{
    m_table->setObjectName(QStringLiteral("table"));
    m_fullPopup->setObjectName(QStringLiteral("popupMenuFull"));

    // Java's initializeTable(): a cell selection of several intervals, the header not
    // reorderable, nothing to edit.
    m_table->setModel(m_model);
    QItemSelectionModel* const original = m_table->selectionModel();
    m_table->setSelectionModel(m_selection);
    original->deleteLater();
    m_table->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    m_table->setCornerButtonEnabled(false);
    m_table->verticalHeader()->hide();
    QHeaderView* const header = m_table->horizontalHeader();
    header->setSectionsMovable(false);
    header->setSectionsClickable(false);
    // A JTable's columns: resizable, of equal widths that fill the table (fitColumns()).
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);

    // Java's doTableInitialization(): the rocket's selected configuration in the last column,
    // before the listeners.
    const int row = m_model->rowOf(panel.rocket().getSelectedConfiguration().getId());
    if (row >= 0)
    {
        m_selection->selectCell(row, lastColumn(m_model->columnCount()));
    }

    // Java's constructor ends with synchronizeConfigurationSelection(), which finds the table as
    // the rocket has it; done before the listeners here, so that no change reaches a subclass
    // that is not made yet.
    synchronizeConfigurationSelection();

    // Java's installTableListener().
    connect(m_selection, &RowColumnSelectionModel::linesChanged, this,
            guarded([this](const LinesChange& change) { linesChanged(change); }, context()));
    connect(m_model, &QAbstractItemModel::modelAboutToBeReset, this,
            guarded([this] { rememberSelection(); }, context()));
    connect(m_model, &QAbstractItemModel::modelReset, this,
            guarded([this] { reselectAfterReset(); }, context()));
    connect(m_table, &QWidget::customContextMenuRequested, this,
            guarded([this](const QPoint& position) { showPopup(position); }, context()));
    connect(m_table, &ConfigurationTableView::leftButtonReleased, this,
            guarded([this](const QPoint& position) { leftButtonReleased(position); }, context()));
    connect(header, &QHeaderView::sectionResized, this,
            guarded([this] { columnResized(); }, context()));
    m_table->installEventFilter(this);
    m_table->viewport()->installEventFilter(this);
    header->viewport()->installEventFilter(this);
}

FlightConfigurablePanel::~FlightConfigurablePanel()
{
    m_table->horizontalHeader()->viewport()->removeEventFilter(this);
    m_table->viewport()->removeEventFilter(this);
    m_table->removeEventFilter(this);
}

TableTransferKeys::Cells FlightConfigurablePanel::transferCells() const
{
    // Java's getValueAt(): the configuration's id in column 0, a Pair of it and the component in
    // the others, whose toString() is "[" + id + ";" + component + "]".
    const Lines&             lines = m_selection->lines();
    TableTransferKeys::Cells cells;
    if (lines.rows.empty() || lines.columns.empty())
    {
        return cells;
    }
    for (const int row : lines.rows)
    {
        const std::optional<FlightConfigurationId> id = m_model->configurationId(row);
        if (!id.has_value())
        {
            continue;
        }
        const QString        key = QtStrings::fromUtf8(id->toString());
        std::vector<QString> values;
        values.reserve(lines.columns.size());
        for (const int column : lines.columns)
        {
            if (column == 0)
            {
                values.push_back(key);
                continue;
            }
            const RocketComponent* const component = m_model->component(column);
            const QString                name =
                component == nullptr ? QString() : QtStrings::fromUtf8(component->toString());
            values.push_back(QLatin1Char('[') + key + QLatin1Char(';') + name + QLatin1Char(']'));
        }
        cells.push_back(std::move(values));
    }
    return cells;
}

FlightConfigurationEditing& FlightConfigurablePanel::editing() const noexcept
{
    return m_panel->editing();
}

Rocket& FlightConfigurablePanel::rocket() const noexcept
{
    return m_panel->rocket();
}

// ---------------------------------------------------------------------------- the selection

RocketComponent* FlightConfigurablePanel::selectedComponent() const
{
    if (m_selection->firstRow() < 0 || m_selection->firstColumn() < 0)
    {
        return nullptr;
    }
    return m_model->component(m_selection->firstColumn());
}

std::vector<Uuid> FlightConfigurablePanel::selectedComponentIds() const
{
    const Lines& lines = m_selection->lines();
    if (lines.rows.empty() || lines.columns.empty())
    {
        return {};
    }
    return componentsOf(lines.columns);
}

std::optional<FlightConfigurationId> FlightConfigurablePanel::selectedConfigurationId() const
{
    if (m_selection->firstRow() < 0 || m_selection->firstColumn() < 0)
    {
        return std::nullopt;
    }
    return m_model->configurationId(m_selection->firstRow());
}

std::vector<FlightConfigurationId> FlightConfigurablePanel::selectedConfigurationIds() const
{
    const Lines& lines = m_selection->lines();
    if (lines.rows.empty() || lines.columns.empty())
    {
        return {};
    }
    std::vector<FlightConfigurationId> ids;
    for (const int row : lines.rows)
    {
        const std::optional<FlightConfigurationId> id = m_model->configurationId(row);
        if (id.has_value() && std::ranges::find(ids, *id) == ids.end())
        {
            ids.push_back(*id);
        }
    }
    return ids;
}

void FlightConfigurablePanel::setSelectedConfigurationIds(
    std::span<const FlightConfigurationId> ids)
{
    if (ids.empty() || m_model->columnCount() == 0)
    {
        return;
    }
    const std::vector<FlightConfigurationId> selected = selectedConfigurationIds();
    if (!selected.empty() && std::ranges::all_of(ids, [&selected](const FlightConfigurationId& id) {
            return std::ranges::find(selected, id) != selected.end();
        }))
    {
        return;
    }
    clearSelection();
    // Java selects in column 1; a table without one selects in column 0 (see the class comment).
    const int column = m_model->columnCount() > 1 ? 1 : 0;
    for (const FlightConfigurationId& id : ids)
    {
        const int row = id.isDefaultId() ? -1 : m_model->rowOf(id);
        if (row >= 0)
        {
            m_selection->addCell(row, column);
        }
    }
}

void FlightConfigurablePanel::clearSelection()
{
    m_selection->clearLines();
}

void FlightConfigurablePanel::synchronizeConfigurationSelection()
{
    const FlightConfigurationId current = rocket().getSelectedConfiguration().getId();
    if (current.isDefaultId())
    {
        clearSelection();
        return;
    }
    const std::optional<FlightConfigurationId> selected = selectedConfigurationId();
    if (selected.has_value() && *selected == current)
    {
        return;
    }
    // The rocket's configuration in the selected column, or the last one.
    int column = m_selection->firstColumn();
    if (column < 0)
    {
        column = lastColumn(m_model->columnCount());
    }
    const int row = m_model->rowOf(current);
    if (row >= 0)
    {
        m_selection->selectCell(row, column);
    }
}

void FlightConfigurablePanel::restoreSelection()
{
    restoreSelection(m_selection->firstRow(), m_selection->firstColumn());
}

void FlightConfigurablePanel::restoreSelection(int row, int column)
{
    // Java's fireTableDataChanged() clears the JTable's selection, then restoreSelection().
    clearSelection();
    if (row <= 0 || column <= 0 || row >= m_model->rowCount() || column >= m_model->columnCount())
    {
        synchronizeConfigurationSelection();
    }
    else
    {
        m_selection->addCell(row, column);
        takeTheSpotlight();
    }
    updateButtonState();
}

void FlightConfigurablePanel::updateRocketViewSelection()
{
    std::vector<Uuid> components = selectedComponentIds();
    if (!components.empty())
    {
        requestRocketViewSelection(std::move(components));
    }
}

bool FlightConfigurablePanel::isStructureChange(const ComponentChangeEvent& event) const
{
    if (event.isMotorChange() || event.isTreeChange())
    {
        return true;
    }
    const RocketComponent* const source = event.getSource();
    return event.isNonFunctionalChange() && source != nullptr && restructuresWith(*source);
}

bool FlightConfigurablePanel::restructuresWith(const RocketComponent& /*source*/) const
{
    return false;
}

void FlightConfigurablePanel::restructure()
{
    // JTable.tableChanged() of a structure change: clearSelectionAndLeadAnchor(), and the columns
    // made again from the model.
    clearSelection();
    m_columnsResizedByHand = false;
    fitColumns();
}

void FlightConfigurablePanel::takeTheSpotlight()
{
    // Java's requestFocusInWindow() does nothing for a table that is not showing.
    if (m_table->isVisible())
    {
        m_table->setFocus();
    }
}

// ------------------------------------------------------------------------ the mouse and keys

bool FlightConfigurablePanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_table && event->type() == QEvent::KeyPress)
    {
        const auto* key = dynamic_cast<const QKeyEvent*>(event);
        if (key != nullptr && isPlainEnter(*key))
        {
            Guard::run([this] { selectValue(); }, context());
            return true;
        }
    }
    else if (watched == m_table->viewport())
    {
        if (event->type() == QEvent::Resize)
        {
            Guard::run([this] { fitColumns(); }, context());
        }
        else if (const auto* mouse = dynamic_cast<const QMouseEvent*>(event); mouse != nullptr)
        {
            return viewportMouseEvent(*mouse);
        }
    }
    else if (watched == m_table->horizontalHeader()->viewport())
    {
        // A resize by hand happens while the left button is pressed in the header.
        if (event->type() == QEvent::MouseButtonPress ||
            event->type() == QEvent::MouseButtonDblClick)
        {
            m_headerPressed = true;
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            m_headerPressed = false;
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool FlightConfigurablePanel::viewportMouseEvent(const QMouseEvent& event)
{
    const QEvent::Type type = event.type();
    if (type != QEvent::MouseButtonPress && type != QEvent::MouseButtonDblClick &&
        type != QEvent::MouseButtonRelease)
    {
        return false;
    }
    if (event.button() != Qt::LeftButton)
    {
        // Java's table selects with the left button only; the right button's popup comes with
        // the context menu event (showPopup()).
        return true;
    }
    const bool outside = !m_table->indexAt(event.position().toPoint()).isValid();
    if (type == QEvent::MouseButtonPress)
    {
        m_pressedOutside = outside;
        m_doubleClicked  = false;
    }
    else if (type == QEvent::MouseButtonDblClick)
    {
        m_pressedOutside = outside;
        m_doubleClicked  = true;
    }
    // The release: leftButtonReleased(), once the table took it.
    return false;
}

void FlightConfigurablePanel::leftButtonReleased(const QPoint& position)
{
    const bool outside = !m_table->indexAt(position).isValid();
    if (std::exchange(m_doubleClicked, false))
    {
        // Java's mouseClicked() with a click count of 2: the Select operation when the selected
        // column is a component's.
        if (m_selection->firstColumn() > 0)
        {
            selectValue();
        }
    }
    else if (m_pressedOutside && outside)
    {
        // A click outside the cells clears the selection.
        clearSelection();
    }
}

void FlightConfigurablePanel::showPopup(const QPoint& position)
{
    const QModelIndex index  = m_table->indexAt(position);
    const int         row    = index.isValid() ? index.row() : -1;
    const int         column = index.isValid() ? index.column() : -1;
    if (!m_selection->isCellSelected(row, column))
    {
        if (!index.isValid())
        {
            clearSelection();
            return;
        }
        // Java: setRowSelectionInterval(r, r), then setColumnSelectionInterval(c, c).
        m_selection->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
        m_selection->setLines(Lines{.rows = {row}, .columns = {column}}, Order::ROWS_FIRST);
    }
    QMenu& menu = column > 0 ? *m_fullPopup : m_panel->configurationPopupMenu();
    menu.popup(m_table->viewport()->mapToGlobal(position));
}

// ------------------------------------------------------------------------- the listeners

void FlightConfigurablePanel::linesChanged(const LinesChange& change)
{
    // Java's listeners of the rows and the columns skip the steps of a gesture of the mouse
    // (getValueIsAdjusting()); its panels' listeners of the buttons do not.
    if (!change.adjusting)
    {
        if (change.order == Order::COLUMNS_FIRST)
        {
            columnsChanged(change);
            rowsChanged(change);
        }
        else
        {
            rowsChanged(change);
            columnsChanged(change);
        }
    }
    updateButtonState();
}

void FlightConfigurablePanel::rowsChanged(const LinesChange& change)
{
    // Java: rocket.setSelectedConfiguration() for a selection of one row, not of several.
    if (change.before.rows == change.after.rows || change.after.rows.size() != 1 || m_reselecting ||
        m_panel->isHandlingRocketChange())
    {
        return;
    }
    const std::optional<FlightConfigurationId> id =
        m_model->configurationId(change.after.rows.front());
    if (id.has_value())
    {
        rocket().setSelectedConfiguration(*id);
    }
}

void FlightConfigurablePanel::columnsChanged(const LinesChange& change)
{
    if (change.before.columns == change.after.columns)
    {
        return;
    }
    // Java's listener of the column selection sees the rows of the moment: those of before when
    // the columns change first.
    const std::vector<int>& rows =
        change.order == Order::COLUMNS_FIRST ? change.before.rows : change.after.rows;
    if (rows.empty())
    {
        return;
    }
    std::vector<Uuid> components = componentsOf(change.after.columns);
    if (!components.empty())
    {
        requestRocketViewSelection(std::move(components));
    }
}

std::vector<Uuid> FlightConfigurablePanel::componentsOf(const std::vector<int>& columns) const
{
    std::vector<Uuid> components;
    for (const int column : columns)
    {
        const std::optional<Uuid> id = m_model->componentId(column);
        if (id.has_value())
        {
            components.push_back(*id);
        }
    }
    return components;
}

void FlightConfigurablePanel::requestRocketViewSelection(std::vector<Uuid> components)
{
    if (!m_reselecting && !m_panel->isHandlingRocketChange())
    {
        m_panel->setSelectedComponents(components);
        return;
    }
    // The rocket delivers its event: the design's selection changes one turn later.
    m_queuedComponents = std::move(components);
    if (m_viewSelectionQueued)
    {
        return;
    }
    m_viewSelectionQueued = true;
    QMetaObject::invokeMethod(this,
                              guarded(
                                  [this] {
                                      m_viewSelectionQueued = false;
                                      const std::vector<Uuid> queuedComponents =
                                          std::exchange(m_queuedComponents, {});
                                      m_panel->setSelectedComponents(queuedComponents);
                                  },
                                  context()),
                              Qt::QueuedConnection);
}

void FlightConfigurablePanel::rememberSelection()
{
    const std::span<const Uuid> columns = m_model->componentIds();
    m_rememberedWidthColumns.assign(columns.begin(), columns.end());
    m_rememberedWidths.clear();
    const QHeaderView* const header = m_table->horizontalHeader();
    for (int section = 0; section < header->count(); ++section)
    {
        m_rememberedWidths.push_back(header->sectionSize(section));
    }

    const Lines& lines = m_selection->lines();
    m_rememberedRows.clear();
    for (const int row : lines.rows)
    {
        const std::optional<FlightConfigurationId> id = m_model->configurationId(row);
        if (id.has_value())
        {
            m_rememberedRows.push_back(*id);
        }
    }
    m_rememberedColumnZero = !lines.columns.empty() && lines.columns.front() == 0;
    m_rememberedColumns    = componentsOf(lines.columns);
}

void FlightConfigurablePanel::reselectAfterReset()
{
    Lines lines;
    for (const FlightConfigurationId& id : std::exchange(m_rememberedRows, {}))
    {
        const int row = m_model->rowOf(id);
        if (row >= 0)
        {
            lines.rows.push_back(row);
        }
    }
    if (std::exchange(m_rememberedColumnZero, false))
    {
        lines.columns.push_back(0);
    }
    for (const Uuid& id : std::exchange(m_rememberedColumns, {}))
    {
        const int column = m_model->columnOf(id);
        if (column >= 0)
        {
            lines.columns.push_back(column);
        }
    }
    // The widths: kept for the same columns after a resize by hand, else equal.
    const std::span<const Uuid> columns = m_model->componentIds();
    std::vector<int>            widths  = std::exchange(m_rememberedWidths, {});
    if (m_columnsResizedByHand &&
        std::ranges::equal(columns, std::exchange(m_rememberedWidthColumns, {})) &&
        static_cast<int>(widths.size()) == m_table->horizontalHeader()->count())
    {
        const QScopedValueRollback<bool> fitting(m_fittingColumns, true);
        for (int section = 0; section + 1 < static_cast<int>(widths.size()); ++section)
        {
            m_table->horizontalHeader()->resizeSection(section,
                                                       widths[static_cast<std::size_t>(section)]);
        }
    }
    else
    {
        m_columnsResizedByHand = false;
        fitColumns();
    }

    const QScopedValueRollback<bool> reselecting(m_reselecting, true);
    if (!lines.rows.empty() && !lines.columns.empty())
    {
        m_selection->setCurrentIndex(m_model->index(*std::ranges::min_element(lines.rows),
                                                    *std::ranges::min_element(lines.columns)),
                                     QItemSelectionModel::NoUpdate);
    }
    m_selection->setLines(lines, Order::COLUMNS_FIRST);
    updateButtonState();
}

// ------------------------------------------------------------------------- for the panels

void FlightConfigurablePanel::runOperation(const std::function<bool()>& operation)
{
    // Java's fireTableDataChanged() reads the first selected cell just before it fires the
    // operation's change event, which restructures and synchronizes the tables.
    int                                     row    = m_selection->firstRow();
    int                                     column = m_selection->firstColumn();
    const QPointer<FlightConfigurablePanel> alive(this);
    bool                                    changed = false;
    {
        const QObject capture;
        connect(&editing(), &FlightConfigurationEditing::changeAboutToFire, &capture,
                [this, &alive, &row, &column] {
                    if (!alive.isNull())
                    {
                        row    = m_selection->firstRow();
                        column = m_selection->firstColumn();
                    }
                });
        changed = operation();
    }
    if (alive.isNull())
    {
        return;  // the tab went during the operation's question
    }
    if (changed)
    {
        restoreSelection(row, column);
    }
    else
    {
        takeTheSpotlight();
    }
}

void FlightConfigurablePanel::fitColumns()
{
    QHeaderView* const header = m_table->horizontalHeader();
    const int          count  = header->count();
    const int          width  = m_table->viewport()->width();
    if (m_columnsResizedByHand || count == 0 || width <= 0)
    {
        return;
    }
    // Equal widths; the last column, stretched, takes what the division leaves.
    const QScopedValueRollback<bool> fitting(m_fittingColumns, true);
    for (int section = 0; section + 1 < count; ++section)
    {
        header->resizeSection(section, width / count);
    }
}

void FlightConfigurablePanel::columnResized()
{
    if (m_headerPressed && !m_fittingColumns)
    {
        m_columnsResizedByHand = true;
    }
}

QAction* FlightConfigurablePanel::makeAction(const QString& text, const QString& objectName,
                                             std::function<void()> operation)
{
    auto* action = makeChild<QAction>(this, text);
    action->setObjectName(objectName);
    connect(action, &QAction::triggered, this,
            guarded([operation = std::move(operation)] { operation(); }, text));
    return action;
}

QPushButton* FlightConfigurablePanel::makeButton(QAction& action, const QString& objectName,
                                                 QWidget* parent)
{
    auto* button = makeChild<QPushButton>(parent, action.text());
    button->setObjectName(objectName);
    button->setEnabled(action.isEnabled());
    connect(&action, &QAction::changed, button, [button, &action] {
        button->setText(action.text());
        button->setEnabled(action.isEnabled());
    });
    connect(button, &QPushButton::clicked, &action, &QAction::trigger);
    return button;
}

void FlightConfigurablePanel::shareWidth(std::span<QPushButton* const> buttons)
{
    int width = 0;
    for (const QPushButton* button : buttons)
    {
        width = std::max(width, button->sizeHint().width());
    }
    for (QPushButton* button : buttons)
    {
        button->setMinimumWidth(width);
    }
}

}  // namespace QtRocket::Gui
