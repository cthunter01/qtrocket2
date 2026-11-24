#include "QtRocket/gui/simulation/SimulationColumnVisibility.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>
#include <QHeaderView>
#include <QMenu>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/simulation/SimulationTableView.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/Preferences.h"

namespace QtRocket::Gui
{

namespace
{

/// What the chooser's slots report a failure as.
[[nodiscard]] QString context()
{
    return QStringLiteral("Simulations table columns");
}

/// @p text without the characters up to ' ' at its ends (Java's String.trim()).
[[nodiscard]] std::string_view trimmed(std::string_view text)
{
    const auto blank = [](char c) { return static_cast<unsigned char>(c) <= ' '; };
    while (!text.empty() && blank(text.front()))
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && blank(text.back()))
    {
        text.remove_suffix(1);
    }
    return text;
}

/// The stored text of the design, when it has one: a string under the key.
[[nodiscard]] std::optional<std::string> documentText(const DocumentPreferences& preferences)
{
    const DocumentPreferences::DocumentPreference* stored =
        preferences.find(GuiPreferenceKeys::kSimulationTableHiddenColumns);
    if (stored == nullptr || stored->type() != DocumentPreferences::Type::STRING)
    {
        return std::nullopt;
    }
    return std::get<std::string>(stored->value());
}

}  // namespace

SimulationColumnVisibility::SimulationColumnVisibility(SimulationTableView& table,
                                                       DocumentPreferences& documentPreferences,
                                                       Preferences&         applicationPreferences,
                                                       QObject*             parent)
  : QObject(parent),
    m_table(&table),
    m_documentPreferences(&documentPreferences),
    m_applicationPreferences(&applicationPreferences),
    m_hidden(storedHiddenColumns(documentPreferences, applicationPreferences))
{
    apply();
    QHeaderView& header = *table.horizontalHeader();
    header.setContextMenuPolicy(Qt::CustomContextMenu);
    connect(&header, &QHeaderView::customContextMenuRequested, this,
            guarded([this](const QPoint& position) { showMenu(position); }, context()));
}

SimulationColumnVisibility::~SimulationColumnVisibility() = default;

// ---------------------------------------------------------------------------------- the text

std::vector<SimulationColumns::Column> SimulationColumnVisibility::parseHiddenColumns(
    std::string_view text)
{
    std::vector<Column> columns;
    std::size_t         start = 0;
    while (start <= text.size())
    {
        const std::size_t           comma = std::min(text.find(',', start), text.size());
        const std::optional<Column> column =
            SimulationColumns::fromId(trimmed(text.substr(start, comma - start)));
        if (column.has_value() && std::ranges::find(columns, *column) == columns.end())
        {
            columns.push_back(*column);
        }
        start = comma + 1;
    }
    return columns;
}

std::string SimulationColumnVisibility::formatHiddenColumns(std::span<const Column> hidden)
{
    std::string text;
    for (const Column column : hidden)
    {
        if (!text.empty())
        {
            text += ',';
        }
        text += SimulationColumns::id(column);
    }
    return text;
}

std::vector<SimulationColumns::Column> SimulationColumnVisibility::storedHiddenColumns(
    const DocumentPreferences& documentPreferences, const Preferences& applicationPreferences)
{
    std::optional<std::string> text = documentText(documentPreferences);
    if (!text.has_value())
    {
        text = applicationPreferences.get(GuiPreferenceKeys::kSimulationTableHiddenColumnsDefault);
    }
    if (text.has_value())
    {
        return parseHiddenColumns(*text);
    }
    // Add default hidden columns if no preference is set (OpenRocket's comment).
    std::vector<Column> hidden;
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (SimulationColumns::isHiddenByDefault(column))
        {
            hidden.push_back(column);
        }
    }
    return hidden;
}

// ---------------------------------------------------------------------------------- the columns

bool SimulationColumnVisibility::isColumnVisible(Column column) const
{
    return m_table->isColumnVisible(column);
}

int SimulationColumnVisibility::visibleCount() const
{
    return static_cast<int>(m_table->visibleColumns().size());
}

void SimulationColumnVisibility::apply()
{
    QHeaderView& header = *m_table->horizontalHeader();
    for (const Column column : SimulationColumns::kAllColumns)
    {
        if (std::ranges::find(m_hidden, column) != m_hidden.end())
        {
            header.setSectionHidden(SimulationColumns::indexOf(column), true);
        }
    }
    if (visibleCount() == 0)
    {
        const Column first = SimulationColumns::kAllColumns.front();
        std::erase(m_hidden, first);
        header.setSectionHidden(SimulationColumns::indexOf(first), false);
    }
}

void SimulationColumnVisibility::setColumnVisible(Column column, bool visible)
{
    QHeaderView& header = *m_table->horizontalHeader();
    const auto   listed = std::ranges::find(m_hidden, column);
    if (visible)
    {
        if (isColumnVisible(column))
        {
            if (listed != m_hidden.end())
            {
                m_hidden.erase(listed);
                store();
            }
            return;
        }
        if (listed != m_hidden.end())
        {
            m_hidden.erase(listed);
        }
        header.setSectionHidden(SimulationColumns::indexOf(column), false);
        place(column);
        store();
        return;
    }
    if (!isColumnVisible(column))
    {
        if (listed == m_hidden.end())
        {
            m_hidden.push_back(column);
            store();
        }
        return;
    }
    if (visibleCount() <= 1)
    {
        return;
    }
    header.setSectionHidden(SimulationColumns::indexOf(column), true);
    if (listed == m_hidden.end())
    {
        m_hidden.push_back(column);
    }
    store();
}

void SimulationColumnVisibility::place(Column column)
{
    // ColumnVisibilityController.calculateInsertionIndex(): the shown columns before it in the
    // model's order.
    std::size_t target = 0;
    for (const Column before : SimulationColumns::kAllColumns)
    {
        if (before == column)
        {
            break;
        }
        if (isColumnVisible(before))
        {
            ++target;
        }
    }
    // The other shown columns from the left: it goes before the one at that place, or after the
    // last.
    std::vector<Column> others = m_table->visibleColumns();
    std::erase(others, column);
    if (others.empty())
    {
        return;
    }
    QHeaderView& header = *m_table->horizontalHeader();
    const int    from   = header.visualIndex(SimulationColumns::indexOf(column));
    int          to     = 0;
    if (target < others.size())
    {
        const int next = header.visualIndex(SimulationColumns::indexOf(others[target]));
        to             = from < next ? next - 1 : next;
    }
    else
    {
        const int previous = header.visualIndex(SimulationColumns::indexOf(others.back()));
        to                 = from < previous ? previous : previous + 1;
    }
    if (to != from)
    {
        header.moveSection(from, to);
    }
}

void SimulationColumnVisibility::store()
{
    m_documentPreferences->putString(GuiPreferenceKeys::kSimulationTableHiddenColumns,
                                     formatHiddenColumns(m_hidden));
}

void SimulationColumnVisibility::saveAsDefault()
{
    m_applicationPreferences->putString(GuiPreferenceKeys::kSimulationTableHiddenColumnsDefault,
                                        formatHiddenColumns(m_hidden));
}

// ---------------------------------------------------------------------------------- the menu

void SimulationColumnVisibility::populateMenu(QMenu& menu)
{
    const int visible = visibleCount();
    for (const Column column : SimulationColumns::kAllColumns)
    {
        const bool shown = isColumnVisible(column);
        QAction*   item  = menu.addAction(SimulationColumns::chooserText(column));
        item->setObjectName(QString::fromUtf8(SimulationColumns::id(column)));
        item->setCheckable(true);
        item->setChecked(shown);
        if (shown && visible <= 1)
        {
            item->setEnabled(false);
        }
        connect(item, &QAction::triggered, this,
                guarded([this, column](bool checked) { setColumnVisible(column, checked); },
                        context()));
    }
    menu.addSeparator();
    //: simpanel.btn.SaveAsDefault
    QAction* saveDefault = menu.addAction(tr("Save as default"));
    saveDefault->setObjectName(QStringLiteral("saveAsDefault"));
    //: simpanel.btn.SaveAsDefault.ttip
    saveDefault->setToolTip(tr("Save the selected visibility settings for all new designs"));
    connect(saveDefault, &QAction::triggered, this,
            guarded([this] { saveAsDefault(); }, context()));
    // A JMenuItem shows its tooltip.
    menu.setToolTipsVisible(true);
}

void SimulationColumnVisibility::showMenu(const QPoint& position)
{
    if (m_menu == nullptr)
    {
        m_menu = makeChild<QMenu>(m_table);
    }
    m_menu->clear();
    populateMenu(*m_menu);
    m_menu->popup(m_table->horizontalHeader()->viewport()->mapToGlobal(position));
}

}  // namespace QtRocket::Gui
