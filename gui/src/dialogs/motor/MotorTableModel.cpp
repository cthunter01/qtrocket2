#include "QtRocket/gui/dialogs/motor/MotorTableModel.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QList>
#include <QModelIndex>
#include <QObject>
#include <QString>
#include <QVariant>
#include <Qt>

#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/chooser/MotorTableSorter.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorColumns.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorDatabaseModel.h"
#include "QtRocket/motor/chooser/ThrustCurveMotorSelection.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The default unit of the motor dimensions, in which the Diameter and Length columns are shown.
[[nodiscard]] const Unit* dimensionUnit()
{
    return &unitGroup(UnitGroupId::MOTOR_DIMENSIONS).getDefaultUnit();
}

}  // namespace

MotorTableModel::MotorTableModel(ThrustCurveMotorSelection& selection,
                                 const Preferences& preferences, QObject* parent)
  : QAbstractTableModel(parent), m_selection(&selection), m_preferences(&preferences)
{
    resetTexts();
    setRows(m_selection->getViewRows());
}

MotorTableModel::~MotorTableModel() = default;

int MotorTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

int MotorTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kColumnCount;
}

QVariant MotorTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount() ||
        index.column() >= kColumnCount)
    {
        return {};
    }
    const RowTexts& texts = textsOf(m_rows.at(static_cast<std::size_t>(index.row())));
    return texts.at(static_cast<std::size_t>(index.column()));
}

QVariant MotorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 ||
        section >= kColumnCount)
    {
        return {};
    }
    switch (columnAt(section))
    {
        case ThrustCurveMotorColumns::MANUFACTURER:
            //: TCurveMotorCol.MANUFACTURER
            return tr("Manufacturer");
        case ThrustCurveMotorColumns::NAME:
            //: TCurveMotorCol.NAME
            return tr("Name");
        case ThrustCurveMotorColumns::TOTAL_IMPULSE:
            //: TCurveMotorCol.TOTAL_IMPULSE
            return tr("Total Impulse");
        case ThrustCurveMotorColumns::CASEINFO:
            //: TCurveMotorCol.CASEINFO
            return tr("Case");
        case ThrustCurveMotorColumns::DIAMETER:
            //: TCurveMotorCol.DIAMETER
            return tr("Diameter");
        case ThrustCurveMotorColumns::LENGTH:
            //: TCurveMotorCol.LENGTH
            return tr("Length");
    }
    QTROCKET_UNREACHABLE();
}

// ------------------------------------------------------------------------------ the rows

void MotorTableModel::refresh()
{
    const bool               sameSets  = setsAreCurrent();
    const bool               sameTexts = sameSets && textsAreCurrent();
    std::vector<std::size_t> rows      = m_selection->getViewRows();
    if (sameSets && rows == m_rows)
    {
        if (!sameTexts)
        {
            // Java's fireTableDataChanged() of the name column: the same rows, other texts.
            resetTexts();
            if (!m_rows.empty())
            {
                Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, kColumnCount - 1),
                                   {Qt::DisplayRole});
            }
        }
        updateSortIndicator();
        return;
    }

    // A sort of the same rows, or a new set of rows (a filter changed, or the list of sets).
    bool sorted = sameSets && rows.size() == m_rows.size();
    for (std::size_t r = 0; sorted && r < rows.size(); ++r)
    {
        sorted = row(rows[r]) >= 0;
    }
    const QAbstractItemModel::LayoutChangeHint hint =
        sorted ? QAbstractItemModel::VerticalSortHint : QAbstractItemModel::NoLayoutChangeHint;
    Q_EMIT layoutAboutToBeChanged({}, hint);
    const std::vector<std::size_t>                oldRows = m_rows;
    const std::vector<const ThrustCurveMotorSet*> oldSets = m_sets;
    if (!sameTexts)
    {
        resetTexts();
    }
    setRows(std::move(rows));
    movePersistentIndexes(oldRows, oldSets, sameSets);
    Q_EMIT layoutChanged({}, hint);
    updateSortIndicator();
}

void MotorTableModel::setRows(std::vector<std::size_t> rows)
{
    m_rows = std::move(rows);
    m_rowOfModelRow.assign(m_sets.size(), -1);
    for (std::size_t r = 0; r < m_rows.size(); ++r)
    {
        m_rowOfModelRow.at(m_rows[r]) = static_cast<int>(r);
    }
}

void MotorTableModel::movePersistentIndexes(const std::vector<std::size_t>&                oldRows,
                                            const std::vector<const ThrustCurveMotorSet*>& oldSets,
                                            bool                                           sameSets)
{
    const QModelIndexList from = persistentIndexList();
    QModelIndexList       to;
    to.reserve(from.size());
    for (const QModelIndex& old : from)
    {
        int moved = -1;
        if (old.isValid() && old.row() < static_cast<int>(oldRows.size()))
        {
            const std::size_t oldModelRow = oldRows[static_cast<std::size_t>(old.row())];
            if (sameSets)
            {
                moved = row(oldModelRow);
            }
            else if (const ThrustCurveMotorSet* set = oldSets.at(oldModelRow); set != nullptr)
            {
                moved = rowOf(*set);
            }
        }
        to.append(moved < 0 ? QModelIndex() : index(moved, old.column()));
    }
    changePersistentIndexList(from, to);
}

std::size_t MotorTableModel::modelRow(int row) const
{
    checkRow(row);
    return m_rows[static_cast<std::size_t>(row)];
}

int MotorTableModel::row(std::size_t modelRow) const noexcept
{
    return modelRow < m_rowOfModelRow.size() ? m_rowOfModelRow[modelRow] : -1;
}

int MotorTableModel::rowOf(const ThrustCurveMotorSet& set) const
{
    // The sets the rows show (the core's model may have taken another list since: refresh()).
    for (std::size_t modelRow = 0; modelRow < m_sets.size(); ++modelRow)
    {
        if (m_sets[modelRow] == &set)
        {
            return row(modelRow);
        }
    }
    return -1;
}

const ThrustCurveMotorSet& MotorTableModel::motorSet(int row) const
{
    return *m_sets.at(modelRow(row));
}

void MotorTableModel::checkRow(int row) const
{
    if (row < 0 || row >= rowCount())
    {
        bug("The motor table has no row " + std::to_string(row) + " (it has " +
            std::to_string(rowCount()) + ")");
    }
}

// ------------------------------------------------------------------------------ the texts

bool MotorTableModel::setsAreCurrent() const
{
    const std::span<const ThrustCurveMotorSet* const> sets = m_selection->getModel().getMotorSets();
    if (sets.size() != m_sets.size())
    {
        return false;
    }
    for (std::size_t modelRow = 0; modelRow < sets.size(); ++modelRow)
    {
        if (sets[modelRow] != m_sets[modelRow])
        {
            return false;
        }
    }
    return true;
}

bool MotorTableModel::textsAreCurrent() const
{
    return m_dimensionUnit == dimensionUnit() &&
           m_designation == m_preferences->getMotorNameColumn();
}

void MotorTableModel::resetTexts()
{
    const std::span<const ThrustCurveMotorSet* const> sets = m_selection->getModel().getMotorSets();
    m_sets.assign(sets.begin(), sets.end());
    m_dimensionUnit = dimensionUnit();
    m_designation   = m_preferences->getMotorNameColumn();
    m_texts.assign(m_sets.size(), std::nullopt);
}

const MotorTableModel::RowTexts& MotorTableModel::textsOf(std::size_t modelRow) const
{
    std::optional<RowTexts>& kept = m_texts.at(modelRow);
    if (!kept.has_value())
    {
        const ThrustCurveMotorSet& set = *m_sets.at(modelRow);
        RowTexts                   texts;
        for (std::size_t column = 0; column < kAllThrustCurveMotorColumns.size(); ++column)
        {
            texts.at(column) = QtStrings::fromUtf8(
                getValueText(kAllThrustCurveMotorColumns.at(column), set, *m_preferences));
        }
        kept = std::move(texts);
    }
    return *kept;
}

// ------------------------------------------------------------------------------ the sort

void MotorTableModel::toggleSortOrder(int column)
{
    m_selection->getSorter().toggleSortOrder(columnAt(column));
    refresh();
}

void MotorTableModel::connectHeader(QHeaderView& header)
{
    if (!m_header.isNull())
    {
        QObject::disconnect(m_header, nullptr, this, nullptr);
    }
    m_header = &header;
    header.setSectionsClickable(true);
    header.setSortIndicatorShown(true);
    connect(&header, &QHeaderView::sectionClicked, this,
            guarded([this](int section) { toggleSortOrder(section); },
                    QStringLiteral("Motor table header")));
    updateSortIndicator();
}

void MotorTableModel::updateSortIndicator()
{
    if (m_header.isNull())
    {
        return;
    }
    const std::vector<MotorTableSorter::SortKey>& keys = m_selection->getSorter().getSortKeys();
    if (keys.empty())
    {
        m_header->setSortIndicator(-1, Qt::AscendingOrder);
        return;
    }
    const MotorTableSorter::SortKey& primary = keys.front();
    m_header->setSortIndicator(static_cast<int>(primary.column),
                               primary.sortOrder == MotorTableSorter::SortOrder::ASCENDING
                                   ? Qt::AscendingOrder
                                   : Qt::DescendingOrder);
}

ThrustCurveMotorColumns MotorTableModel::columnAt(int column)
{
    if (column < 0 || column >= kColumnCount)
    {
        bug("The motor table has no column " + std::to_string(column));
    }
    return kAllThrustCurveMotorColumns.at(static_cast<std::size_t>(column));
}

int MotorTableModel::preferredWidth(int column)
{
    return getWidth(columnAt(column));
}

}  // namespace QtRocket::Gui
