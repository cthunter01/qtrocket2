#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMenu>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QTableView>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/dialogs/preset/ComponentPresetFilterModel.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTableModel.h"
#include "QtRocket/gui/settings/TableUiPreferences.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/TableSortKeys.h"
#include "QtRocket/gui/widgets/TableRowTraversal.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using TableModel = ComponentPresetTableModel;
using SortOrder  = TableSortKeys::SortOrder;

/// @throws BugError for a column that is not the model's
void checkColumn(int column)
{
    if (column < 0 || column >= TableModel::kColumnCount)
    {
        bug("ComponentPresetTable: no column " + std::to_string(column));
    }
}

/// The identifiers of the table of presets of @p type (see columnIdentifiers()).
[[nodiscard]] std::vector<std::optional<std::string>> identifiersOf(ComponentPresetType type)
{
    std::vector<std::optional<std::string>> identifiers;
    identifiers.reserve(TableModel::kColumnCount);
    for (int column = 0; column < TableModel::kColumnCount; ++column)
    {
        if (TableModel::isDisplayed(type, column))
        {
            identifiers.emplace_back(TableModel::columnIdentifier(column));
        }
        else
        {
            identifiers.emplace_back(std::nullopt);
        }
    }
    return identifiers;
}

}  // namespace

ComponentPresetTable::ComponentPresetTable(ComponentPresetType type, std::vector<PresetPtr> presets,
                                           Preferences& preferences, QWidget* parent)
  : QTableView(parent),
    m_preferences(&preferences),
    m_model(makeChild<TableModel>(this, type, std::move(presets), preferences)),
    m_filter(makeChild<ComponentPresetFilterModel>(this, *m_model)),
    m_identifiers(identifiersOf(type))
{
    setModel(m_filter);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    verticalHeader()->hide();

    QHeaderView& header = *horizontalHeader();
    header.setSectionsMovable(true);
    header.setSectionsClickable(true);
    header.setSortIndicatorShown(true);
    header.setContextMenuPolicy(Qt::CustomContextMenu);
    header.setMinimumSectionSize(kMinimumColumnWidth);
    // The columns of the keys the type does not display are no columns of the table.
    for (int column = 0; column < TableModel::kColumnCount; ++column)
    {
        if (!isTableColumn(column))
        {
            header.setSectionHidden(column, true);
        }
    }
    // Make the first column (the favorite column) as small as possible (OpenRocket's comment):
    // the row height and 4 pixels, its minimum and maximum too.
    header.setSectionResizeMode(TableModel::kFavoriteColumn, QHeaderView::Fixed);
    header.resizeSection(TableModel::kFavoriteColumn, verticalHeader()->defaultSectionSize() + 4);
    // The normal left/right and tab/shift-tab key action traverses each cell/column of the table
    // instead of going to the next row (OpenRocket's comment).
    TableRowTraversal::install(*this);

    connect(&header, &QHeaderView::sectionClicked, this,
            guarded([this](int column) { m_filter->toggleSortOrder(column); },
                    QStringLiteral("ComponentPresetTable")));
    connect(&header, &QHeaderView::customContextMenuRequested, this,
            guarded([this](const QPoint& position) { showHeaderMenu(position); },
                    QStringLiteral("ComponentPresetTable")));
    connect(m_filter, &ComponentPresetFilterModel::sortKeysChanged, this,
            guarded([this] { updateSortIndicator(); }, QStringLiteral("ComponentPresetTable")));
    updateSortIndicator();
    connect(selectionModel(), &QItemSelectionModel::selectionChanged, this,
            guarded([this] { rememberSelection(); }, QStringLiteral("ComponentPresetTable")));
    connect(m_filter, &QAbstractItemModel::rowsInserted, this,
            guarded([this] { restoreSelection(); }, QStringLiteral("ComponentPresetTable")));
    connect(m_filter, &QAbstractItemModel::layoutChanged, this,
            guarded([this] { restoreSelection(); }, QStringLiteral("ComponentPresetTable")));
    // As the dialog opens: the Legacy column hidden, the legacy rows filtered out.
    applyLegacy(false);
}

ComponentPresetTable::~ComponentPresetTable()
{
    // QWidget's destructor deletes the models and the selection model after this object's members
    // are gone: none of their signals may reach this table's slots then.
    disconnect(m_filter, nullptr, this, nullptr);
    disconnect(selectionModel(), nullptr, this, nullptr);
    disconnect(horizontalHeader(), nullptr, this, nullptr);
}

bool ComponentPresetTable::isTableColumn(int column) const
{
    checkColumn(column);
    return m_identifiers[static_cast<std::size_t>(column)].has_value();
}

bool ComponentPresetTable::isColumnVisible(int column) const
{
    return isTableColumn(column) && !horizontalHeader()->isSectionHidden(column);
}

void ComponentPresetTable::setColumnVisible(int column, bool visible)
{
    if (!isTableColumn(column))
    {
        bug("ComponentPresetTable: column " + std::to_string(column) + " is not the table's");
    }
    if (column == TableModel::kLegacyColumn)
    {
        applyLegacy(visible);
        return;
    }
    horizontalHeader()->setSectionHidden(column, !visible);
}

std::vector<int> ComponentPresetTable::visibleColumns() const
{
    return TableUiPreferences::visibleColumns(*horizontalHeader(), m_identifiers);
}

int ComponentPresetTable::viewColumn(int column) const
{
    checkColumn(column);
    const std::vector<int> shown = visibleColumns();
    const auto             found = std::ranges::find(shown, column);
    return found == shown.end() ? -1 : static_cast<int>(found - shown.begin());
}

void ComponentPresetTable::setLegacyShown(bool shown)
{
    if (shown == m_legacyShown && isColumnVisible(TableModel::kLegacyColumn) == shown)
    {
        return;
    }
    applyLegacy(shown);
    if (shown)
    {
        // Let's say the optimal width is 100 (you can adjust this as needed) (OpenRocket's
        // comment; its width is 50).
        horizontalHeader()->resizeSection(TableModel::kLegacyColumn, kLegacyColumnWidth);
    }
}

void ComponentPresetTable::populateHeaderMenu(QMenu& menu, int column)
{
    if (column >= 0)
    {
        checkColumn(column);
        //: ComponentPresetChooserDialog.menu.sortAsc
        QAction* ascending = menu.addAction(tr("Sort Ascending"));
        ascending->setCheckable(true);
        connect(ascending, &QAction::triggered, this, guarded([this, column] {
                    m_filter->setSortKeys({{.column = column, .order = SortOrder::ASCENDING}});
                }));
        //: ComponentPresetChooserDialog.menu.sortDesc
        QAction* descending = menu.addAction(tr("Sort Descending"));
        descending->setCheckable(true);
        connect(descending, &QAction::triggered, this, guarded([this, column] {
                    m_filter->setSortKeys({{.column = column, .order = SortOrder::DESCENDING}});
                }));
        menu.addSeparator();
        if (const std::optional<UnitGroupId> group = TableModel::columnUnitGroup(column);
            group.has_value())
        {
            //: ComponentPresetChooserDialog.menu.units
            QMenu* units  = menu.addMenu(tr("Units"));
            auto*  radios = makeChild<QActionGroup>(units);
            for (const Unit* unit : unitGroup(*group).getUnits())
            {
                QAction* item = units->addAction(QtStrings::unitSymbol(*unit));
                item->setCheckable(true);
                item->setChecked(unit == m_model->columnUnit(column));
                item->setActionGroup(radios);
                connect(item, &QAction::triggered, this,
                        guarded([this, column, unit] { m_model->setColumnUnit(column, *unit); }));
            }
            menu.addSeparator();
        }
    }
    for (int each = 0; each < TableModel::kColumnCount; ++each)
    {
        if (!isTableColumn(each))
        {
            continue;
        }
        QAction* item = menu.addAction(TableModel::columnName(each));
        item->setCheckable(true);
        item->setChecked(isColumnVisible(each));
        connect(item, &QAction::triggered, this,
                guarded([this, each] { setColumnVisible(each, !isColumnVisible(each)); }));
    }
}

void ComponentPresetTable::loadUiPreferences(std::string_view tableName)
{
    // The dialog keeps the box's state through the load, which may show the Legacy column
    // ("Restore legacy state (may change during UI preference loading)", OpenRocket's comment).
    const bool legacyShown = m_legacyShown;
    TableUiPreferences::load(*this, m_identifiers, tableName, *m_preferences);
    applyLegacy(legacyShown);
}

void ComponentPresetTable::storeUiPreferences(std::string_view tableName)
{
    TableUiPreferences::store(*this, m_identifiers, tableName, *m_preferences);
}

ComponentPresetTable::PresetPtr ComponentPresetTable::selectedPreset() const
{
    const int row = m_filter->modelRow(TableRowTraversal::selectedRow(*this));
    return row < 0 ? nullptr : m_model->preset(row);
}

void ComponentPresetTable::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    // QAbstractItemView makes the next row current and selected when the current one goes in a
    // single selection; JTable's selection stays with its model row, which the filter hides.
    const QModelIndex current = currentIndex();
    if (current.isValid() && current.parent() == parent && current.row() >= start &&
        current.row() <= end)
    {
        selectionModel()->setCurrentIndex(QModelIndex(), QItemSelectionModel::NoUpdate);
    }
    QTableView::rowsAboutToBeRemoved(parent, start, end);
}

void ComponentPresetTable::keyPressEvent(QKeyEvent* event)
{
    // JTable's ENTER and shift ENTER; the keypad's Enter too (Java's VK_ENTER for both keys).
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        (modifiers == Qt::NoModifier || modifiers == Qt::ShiftModifier))
    {
        moveToCell(modifiers == Qt::NoModifier);
        event->accept();
        return;
    }
    QTableView::keyPressEvent(event);
}

void ComponentPresetTable::moveToCell(bool forward)
{
    const std::vector<int> columns = visibleColumns();
    const std::int64_t     rows    = m_filter->rowCount();
    if (rows == 0 || columns.empty())
    {
        return;
    }
    // The cells column after column: the current one, or none without a selected row.
    const std::int64_t cells   = rows * static_cast<std::int64_t>(columns.size());
    std::int64_t       current = -1;
    const QModelIndex  lead    = currentIndex();
    if (TableRowTraversal::selectedRow(*this) >= 0 && lead.isValid())
    {
        const auto column = std::ranges::find(columns, lead.column());
        if (column != columns.end())
        {
            current = ((column - columns.begin()) * rows) + lead.row();
        }
    }
    std::int64_t next = 0;
    if (current < 0)
    {
        next = forward ? 0 : cells - 1;
    }
    else
    {
        next = forward ? (current + 1) % cells : (current + cells - 1) % cells;
    }
    selectionModel()->setCurrentIndex(
        m_filter->index(static_cast<int>(next % rows),
                        columns[static_cast<std::size_t>(next / rows)]),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void ComponentPresetTable::applyLegacy(bool shown)
{
    const bool changed = shown != m_legacyShown;
    m_legacyShown      = shown;
    horizontalHeader()->setSectionHidden(TableModel::kLegacyColumn, !shown);
    m_filter->setHideLegacy(!shown);
    if (changed)
    {
        Q_EMIT legacyShownChanged(shown);
    }
}

void ComponentPresetTable::updateSortIndicator()
{
    const std::optional<TableSortKeys::SortKey> primary = m_filter->sortKeys().primaryKey();
    if (!primary.has_value())
    {
        horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);
        return;
    }
    horizontalHeader()->setSortIndicator(primary->column, primary->order == SortOrder::ASCENDING
                                                              ? Qt::AscendingOrder
                                                              : Qt::DescendingOrder);
}

void ComponentPresetTable::rememberSelection()
{
    const int row = TableRowTraversal::selectedRow(*this);
    if (row >= 0)
    {
        m_selectedModelRow = m_filter->modelRow(row);
    }
    else if (m_selectedModelRow.has_value() && m_filter->acceptsRow(*m_selectedModelRow))
    {
        m_selectedModelRow.reset();
    }
}

void ComponentPresetTable::restoreSelection()
{
    if (!m_selectedModelRow.has_value() || TableRowTraversal::selectedRow(*this) >= 0)
    {
        return;
    }
    const int row = m_filter->viewRow(*m_selectedModelRow);
    if (row < 0)
    {
        return;
    }
    const std::vector<int> shown = visibleColumns();
    selectionModel()->setCurrentIndex(
        m_filter->index(row, shown.empty() ? 0 : shown.front()),
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void ComponentPresetTable::showHeaderMenu(const QPoint& position)
{
    const QHeaderView& header = *horizontalHeader();
    QMenu              menu(this);
    populateHeaderMenu(menu, header.logicalIndexAt(position));
    menu.exec(header.viewport()->mapToGlobal(position));
}

}  // namespace QtRocket::Gui
