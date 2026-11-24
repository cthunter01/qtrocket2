#include "QtRocket/gui/widgets/CsvExportPanel.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPushButton>
#include <QRect>
#include <QRgb>
#include <QScrollArea>
#include <QScrollBar>
#include <QString>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/CsvOptionPanel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// The table's empty border, px (Java: createEmptyBorder(5, 5, 5, 5) of the content panel).
constexpr int kTableBorder = 5;
/// The padding around each cell's widget, px (Java: each cell's MigLayout "ins 2 3 2 3"); two
/// neighbouring cells' paddings make the grid's spacing.
constexpr int kCellPaddingVertical   = 2;
constexpr int kCellPaddingHorizontal = 3;
/// The width of the line below the headers, px (Java: a matte border 0, 0, 1, 0).
constexpr int kHeaderLineWidth = 1;
/// MigLayout's "para" gap before the count, px (as RocketPanel's).
constexpr int kParagraphGap = 12;

/// @p rows, each with a unit group.
[[nodiscard]] std::vector<CsvExportPanel::Row> checkedRows(std::vector<CsvExportPanel::Row> rows)
{
    if (std::ranges::any_of(
            rows, [](const CsvExportPanel::Row& row) { return row.unitGroup == nullptr; }))
    {
        bug("Every row of a CSV export panel needs a unit group");
    }
    return rows;
}

/// @p csvOptions, which must not be null.
[[nodiscard]] CsvOptionPanel* checkedOptions(CsvOptionPanel* csvOptions)
{
    if (csvOptions == nullptr)
    {
        bug("A CSV export panel needs its CSV options");
    }
    return csvOptions;
}

/// @p widgets, none of them null.
[[nodiscard]] std::vector<QWidget*> checkedWidgets(const std::vector<QWidget*>& widgets)
{
    if (std::ranges::find(widgets, nullptr) != widgets.end())
    {
        bug("An extra widget of a CSV export panel must not be null");
    }
    return widgets;
}

/// The table of a CsvExportPanel: its grid of headers and rows, which it paints behind them (the
/// rows' alternating backgrounds and the line below the headers, Java's cell panels and their
/// borders).
class ExportTable final : public QWidget
{
public:
    /// The table of @p rowCount rows (below the headers).
    ExportTable(std::size_t rowCount, QWidget* parent)
      : QWidget(parent), m_grid(makeChild<QGridLayout>(this)), m_rowCount(rowCount)
    {
        m_grid->setContentsMargins(
            kTableBorder + kCellPaddingHorizontal, kTableBorder + kCellPaddingVertical,
            kTableBorder + kCellPaddingHorizontal, kTableBorder + kCellPaddingVertical);
        m_grid->setHorizontalSpacing(2 * kCellPaddingHorizontal);
        m_grid->setVerticalSpacing(2 * kCellPaddingVertical);
    }

    [[nodiscard]] QGridLayout& grid() const noexcept { return *m_grid; }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        for (std::size_t row = 0; row < m_rowCount; ++row)
        {
            painter.fillRect(rowBand(static_cast<int>(row) + 1),
                             CsvExportPanel::rowBackground(row, palette()));
        }
        // Java: Table.gridColor; QTableView paints its grid in this colour of the style.
        QStyleOption option;
        option.initFrom(this);
        const int   gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &option, this);
        const QRect header   = rowBand(0);
        painter.fillRect(QRect(header.left(), header.bottom() + 1 - kHeaderLineWidth,
                               header.width(), kHeaderLineWidth),
                         QColor::fromRgba(static_cast<QRgb>(gridHint)));
    }

private:
    /// The band of the grid's row @p row: its cells and the paddings around them, as wide as the
    /// table's columns.
    [[nodiscard]] QRect rowBand(int row) const
    {
        const QRect first = m_grid->cellRect(row, 0);
        const QRect last  = m_grid->cellRect(row, m_grid->columnCount() - 1);
        return {first.left() - kCellPaddingHorizontal, first.top() - kCellPaddingVertical,
                last.right() - first.left() + 1 + (2 * kCellPaddingHorizontal),
                first.height() + (2 * kCellPaddingVertical)};
    }

    QGridLayout* m_grid;
    std::size_t  m_rowCount;
};

}  // namespace

CsvExportPanel::CsvExportPanel(std::vector<Row> rows, CsvOptionPanel* csvOptions,
                               OptionsPlacement             placement,
                               const std::vector<QWidget*>& extraWidgets, QWidget* parent)
  : CsvExportPanel(DeferredBuild(), std::move(rows), csvOptions, placement, extraWidgets, parent)
{
    // No subclass to ask: without the extra column (see the class comment).
    buildPanel(false);
}

CsvExportPanel::CsvExportPanel(DeferredBuild /*deferred*/, std::vector<Row> rows,
                               CsvOptionPanel* csvOptions, OptionsPlacement placement,
                               const std::vector<QWidget*>& extraWidgets, QWidget* parent)
  : QWidget(parent),
    m_rows(checkedRows(std::move(rows))),
    m_csvOptions(checkedOptions(csvOptions)),
    m_placement(placement),
    m_extraWidgets(checkedWidgets(extraWidgets))
{
    // The panel owns them from now on, built or not.
    m_csvOptions->setParent(this);
    m_csvOptions->setObjectName(QStringLiteral("csvOptions"));
    for (QWidget* const widget : m_extraWidgets)
    {
        widget->setParent(this);
    }
}

void CsvExportPanel::build()
{
    buildPanel(hasExtraColumn());
}

bool CsvExportPanel::hasExtraColumn() const
{
    return false;
}

QString CsvExportPanel::extraColumnLabel() const
{
    //: CSVExportPanel.lbl.Extra
    return tr("Extra");
}

QWidget* CsvExportPanel::createExtraColumnWidget(std::size_t /*row*/, QWidget* /*parent*/)
{
    return nullptr;
}

void CsvExportPanel::buildPanel(bool withExtraColumn)
{
    if (m_built)
    {
        bug("A CSV export panel is built once");
    }
    m_built = true;

    // Java: MigLayout("fill, ins n n 0 n, wrap 2", "[grow,fill][]", "[grow,fill][]").
    auto* layout = makeChild<QGridLayout>(this);
    {
        const QMargins margins = layout->contentsMargins();
        layout->setContentsMargins(margins.left(), margins.top(), margins.right(), 0);
    }
    layout->setColumnStretch(0, 1);
    layout->setRowStretch(0, 1);
    QWidget* const exportPanel = makeExportPanel(withExtraColumn);
    if (m_placement == OptionsPlacement::SEPARATE_ROW)
    {
        layout->addWidget(exportPanel, 0, 0, 1, 2);
        // Java: "wrap 2", the options ("growx") and each extra widget ("growx, top") in turn.
        std::vector<QWidget*> below{m_csvOptions};
        below.insert(below.end(), m_extraWidgets.begin(), m_extraWidgets.end());
        for (std::size_t index = 0; index < below.size(); ++index)
        {
            const int row    = 1 + static_cast<int>(index / 2);
            const int column = static_cast<int>(index % 2);
            layout->addWidget(below[index], row, column,
                              index == 0 ? Qt::Alignment() : Qt::AlignTop);
        }
    }
    else
    {
        layout->addWidget(exportPanel, 0, 0);
        layout->addWidget(makeOptionsColumn(), 0, 1);
    }
    updateSelectedCount();
}

QWidget* CsvExportPanel::makeExportPanel(bool withExtraColumn)
{
    // The table, its buttons and the count (Java: exportPanel, MigLayout "ins 0, fill").
    auto* exportPanel  = makeChild<QWidget>(this);
    auto* exportLayout = makeChild<QVBoxLayout>(exportPanel);
    exportLayout->setContentsMargins(0, 0, 0, 0);
    m_scrollPane = makeChild<QScrollArea>(exportPanel);
    m_scrollPane->setObjectName(QStringLiteral("scrollPane"));
    m_scrollPane->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollPane->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollPane->setWidgetResizable(true);
    buildTable(withExtraColumn);
    exportLayout->addWidget(m_scrollPane, 1);

    // Select all/none buttons and the selected count (Java: a panel of MigLayout "insets 0").
    // Plain buttons: in a dialog, Return stays the dialog's (its Export button).
    auto* buttonPanel  = makeChild<QWidget>(exportPanel);
    auto* buttonLayout = makeChild<QHBoxLayout>(buttonPanel);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    //: CSVExportPanel.but.Selectall
    m_selectAll = makeChild<QPushButton>(buttonPanel, tr("Select all"));
    m_selectAll->setObjectName(QStringLiteral("selectAllButton"));
    m_selectAll->setAutoDefault(false);
    connect(m_selectAll, &QPushButton::clicked, this, [this] { selectAll(); });
    buttonLayout->addWidget(m_selectAll);
    //: CSVExportPanel.but.Selectnone
    m_selectNone = makeChild<QPushButton>(buttonPanel, tr("Select none"));
    m_selectNone->setObjectName(QStringLiteral("selectNoneButton"));
    m_selectNone->setAutoDefault(false);
    connect(m_selectNone, &QPushButton::clicked, this, [this] { selectNone(); });
    buttonLayout->addWidget(m_selectNone);
    // Java: "gapleft para, growx".
    buttonLayout->addSpacing(kParagraphGap);
    m_selectedCount = makeChild<QLabel>(buttonPanel);
    m_selectedCount->setObjectName(QStringLiteral("selectedCountLabel"));
    buttonLayout->addWidget(m_selectedCount, 1);
    exportLayout->addWidget(buttonPanel);
    fitScrollPaneWidth();
    return exportPanel;
}

QWidget* CsvExportPanel::makeOptionsColumn()
{
    // Java: an options panel (MigLayout "insets 0") of the options and the extra widgets below
    // each other, "growx, wrap", the panel "growx, top".
    auto* optionsPanel = makeChild<QWidget>(this);
    optionsPanel->setObjectName(QStringLiteral("optionsPanel"));
    auto* optionsLayout = makeChild<QVBoxLayout>(optionsPanel);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->addWidget(m_csvOptions);
    for (QWidget* const widget : m_extraWidgets)
    {
        optionsLayout->addWidget(widget);
    }
    optionsLayout->addStretch(1);
    return optionsPanel;
}

void CsvExportPanel::buildTable(bool withExtraColumn)
{
    auto* table    = makeChild<ExportTable>(m_scrollPane, m_rows.size());
    m_contentPanel = table;
    m_contentPanel->setObjectName(QStringLiteral("contentPanel"));
    QGridLayout& grid = table->grid();

    // Header: Java's weights 0, 1, 0 (and 1 for the extra column).
    //: CSVExportPanel.lbl.Export
    addHeader(grid, tr("Export"), 0);
    //: CSVExportPanel.lbl.Variable
    addHeader(grid, tr("Variable"), 1);
    //: CSVExportPanel.lbl.Unit
    addHeader(grid, tr("Unit"), 2);
    grid.setColumnStretch(1, 1);
    if (withExtraColumn)
    {
        addHeader(grid, extraColumnLabel(), 3);
        grid.setColumnStretch(3, 1);
    }

    // Data rows
    m_rowWidgets.reserve(m_rows.size());
    for (std::size_t index = 0; index < m_rows.size(); ++index)
    {
        const Row& row     = m_rows[index];
        const int  gridRow = static_cast<int>(index) + 1;
        RowWidgets widgets;
        widgets.exportCheckBox = makeChild<QCheckBox>(table);
        widgets.exportCheckBox->setObjectName(QStringLiteral("exportCheckBox"));
        widgets.exportCheckBox->setAccessibleName(row.name);
        widgets.exportCheckBox->setChecked(row.selected);
        connect(widgets.exportCheckBox, &QCheckBox::toggled, this,
                [this] { updateSelectedCount(); });
        grid.addWidget(widgets.exportCheckBox, gridRow, 0);

        widgets.nameLabel = makeChild<QLabel>(table, row.name);
        widgets.nameLabel->setObjectName(QStringLiteral("nameLabel"));
        grid.addWidget(widgets.nameLabel, gridRow, 1);

        widgets.unitSelector = makeChild<UnitSelector>(table, *row.unitGroup);
        widgets.unitSelector->setObjectName(QStringLiteral("unitSelector"));
        grid.addWidget(widgets.unitSelector, gridRow, 2);

        if (withExtraColumn)
        {
            widgets.extra = createExtraColumnWidget(index, table);
            if (widgets.extra == nullptr)
            {
                bug("A CSV export panel with an extra column needs its widget for every row");
            }
            grid.addWidget(widgets.extra, gridRow, 3);
        }
        m_rowWidgets.push_back(widgets);
    }
    // The rows at the top when the scroll area is higher than the table.
    grid.setRowStretch(static_cast<int>(m_rows.size()) + 1, 1);
    m_scrollPane->setWidget(table);
}

void CsvExportPanel::addHeader(QGridLayout& grid, const QString& text, int column)
{
    // Java: "<html><b>" + text + "</b></html>".
    auto* label = makeChild<QLabel>(m_contentPanel, text);
    QFont bold  = label->font();
    bold.setBold(true);
    label->setFont(bold);
    grid.addWidget(label, 0, column);
}

void CsvExportPanel::fitScrollPaneWidth()
{
    // No horizontal scroll bar (Java: HORIZONTAL_SCROLLBAR_NEVER, the table clipped when the
    // pane is narrower): the pane is at least as wide as the table beside a vertical scroll bar.
    const int frame = 2 * m_scrollPane->frameWidth();
    m_scrollPane->setMinimumWidth(m_contentPanel->sizeHint().width() +
                                  m_scrollPane->verticalScrollBar()->sizeHint().width() + frame);
}

void CsvExportPanel::updateSelectedCount()
{
    if (m_selectedCount != nullptr)
    {
        m_selectedCount->setText(selectedCountText(selectedCount(), m_rows.size()));
    }
}

QString CsvExportPanel::selectedCountText(std::size_t selected, std::size_t total)
{
    if (selected == 1)
    {
        //: CSVExportPanel.ExportingVar.desc1
        return tr("Exporting 1 variable out of") + QStringLiteral(" %1.").arg(total);
    }
    //: CSVExportPanel.ExportingVar.desc2
    return tr("Exporting") + QStringLiteral(" %1 ").arg(selected) +
           //: CSVExportPanel.ExportingVar.desc3
           tr("variables out of") + QStringLiteral(" %1.").arg(total);
}

QColor CsvExportPanel::rowBackground(std::size_t row, const QPalette& palette)
{
    return row % 2 == 0 ? palette.color(QPalette::Base)
                        : ThemeColors::color(ThemeColors::Key::ROW_LIGHTER, palette);
}

const QString& CsvExportPanel::name(std::size_t row) const
{
    if (row >= m_rows.size())
    {
        bug("A CSV export panel has no such row");
    }
    return m_rows[row].name;
}

const CsvExportPanel::RowWidgets& CsvExportPanel::rowWidgets(std::size_t row) const
{
    if (!m_built)
    {
        bug("A CSV export panel is used before it is built");
    }
    if (row >= m_rowWidgets.size())
    {
        bug("A CSV export panel has no such row");
    }
    return m_rowWidgets[row];
}

bool CsvExportPanel::isSelected(std::size_t row) const
{
    return rowWidgets(row).exportCheckBox->isChecked();
}

void CsvExportPanel::setSelected(std::size_t row, bool selected)
{
    rowWidgets(row).exportCheckBox->setChecked(selected);
}

std::vector<bool> CsvExportPanel::selected() const
{
    std::vector<bool> ticks;
    ticks.reserve(m_rowWidgets.size());
    for (const RowWidgets& widgets : m_rowWidgets)
    {
        ticks.push_back(widgets.exportCheckBox->isChecked());
    }
    return ticks;
}

std::size_t CsvExportPanel::selectedCount() const
{
    return static_cast<std::size_t>(std::ranges::count_if(
        m_rowWidgets,
        [](const RowWidgets& widgets) { return widgets.exportCheckBox->isChecked(); }));
}

const Unit& CsvExportPanel::unit(std::size_t row) const
{
    return rowWidgets(row).unitSelector->getSelectedUnit();
}

std::vector<const Unit*> CsvExportPanel::units() const
{
    std::vector<const Unit*> result;
    result.reserve(m_rowWidgets.size());
    for (const RowWidgets& widgets : m_rowWidgets)
    {
        result.push_back(&widgets.unitSelector->getSelectedUnit());
    }
    return result;
}

void CsvExportPanel::setSelectedUnit(std::size_t row, const Unit& unit)
{
    rowWidgets(row).unitSelector->setSelectedUnit(unit);
}

void CsvExportPanel::selectAll()
{
    for (const RowWidgets& widgets : m_rowWidgets)
    {
        widgets.exportCheckBox->setChecked(true);
    }
    updateSelectedCount();
}

void CsvExportPanel::selectNone()
{
    for (const RowWidgets& widgets : m_rowWidgets)
    {
        widgets.exportCheckBox->setChecked(false);
    }
    updateSelectedCount();
}

QCheckBox* CsvExportPanel::exportCheckBox(std::size_t row) const
{
    return rowWidgets(row).exportCheckBox;
}

QLabel* CsvExportPanel::nameLabel(std::size_t row) const
{
    return rowWidgets(row).nameLabel;
}

UnitSelector* CsvExportPanel::unitSelector(std::size_t row) const
{
    return rowWidgets(row).unitSelector;
}

QWidget* CsvExportPanel::extraColumnWidget(std::size_t row) const
{
    return rowWidgets(row).extra;
}

}  // namespace QtRocket::Gui
