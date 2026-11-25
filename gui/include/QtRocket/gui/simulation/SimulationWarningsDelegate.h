#pragma once

#include <map>
#include <optional>
#include <vector>

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyledItemDelegate>

#include "QtRocket/gui/simulation/SimulationColumns.h"
#include "QtRocket/gui/util/Icons.h"

class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

namespace QtRocket::Gui
{

/// How the simulations table draws its warnings column (OpenRocket's WarningsBox in its
/// WarningsBoxRenderer, SimulationPanel.java:1387-1485): for each priority that has warnings,
/// critical, then normal, then informational, the count followed by a space and then the
/// priority's icon (Icons' WARNING_HIGH, WARNING_NORMAL, WARNING_LOW), the groups separated by
/// ", ": "1 [!], 2 [i]". The parts start kLeftPadding pixels from the cell's left edge (the
/// renderer's empty border) and follow one another at their natural widths (a BoxLayout of
/// JLabels), each centred vertically in the cell; a text is as wide as the font makes it, an icon
/// as high and wide as Icons::iconHeight() makes it next to the cell's font. A simulation without
/// data or without warnings shows nothing. The counts are the model's
/// SimulationTableModel::kCriticalWarningsRole, kNormalWarningsRole and
/// kInformationalWarningsRole (nothing: no data).
///
/// The background, the selection and the focus frame are the style's (as the row's other cells
/// have them); the texts are in the palette's text colour, or its highlighted text colour when the
/// row is selected (the renderer's table foreground and selection foreground). sizeHint()'s width
/// is the renderer's preferred width, the padding and the parts: SimulationTableView (T2) sizes
/// the column to the widest cell plus SimulationColumns::kAutoSizePadding
/// (ColumnTable.setupAutoSizeColumns()), at least SimulationColumns::defaultWidth().
///
/// It is made for the warnings column, for which SimulationTableView (T2) sets it alone
/// (QAbstractItemView::setItemDelegateForColumn()): a cell without the counts' roles draws its
/// background only. The cell's tooltip is the model's (QStyledItemDelegate's):
/// SimulationColumns::warningsToolTip().
///
/// Deviation: the parts are clipped to the cell when it is narrower than they are (Swing's
/// BoxLayout shrinks nothing either, and its panel clips them too); the icons are drawn by Qt's
/// SVG renderer in the palette's colours of the moment (Icons), where OpenRocket's are made once
/// per theme.
class SimulationWarningsDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// The renderer's empty border at the left of the parts.
    static constexpr int kLeftPadding = 10;

    /// One part of a cell: a text (the count and its space, or ", ") or an icon, in the cell.
    struct Part
    {
        /// The text, empty for an icon.
        QString text;
        /// The icon, nullopt for a text.
        std::optional<Icons::Id> icon;
        /// Where the part is drawn.
        QRect rect;

        [[nodiscard]] bool operator==(const Part&) const = default;
    };

    /// What a cell draws.
    struct Cell
    {
        /// The parts, left to right (none without warnings).
        std::vector<Part> parts;
        /// The texts' font: the cell's.
        QFont font;
        /// The texts' colour: the palette's text, or highlighted text when the row is selected.
        QColor textColor;
        /// The renderer's preferred width: kLeftPadding and the parts' widths.
        int width = 0;
    };

    explicit SimulationWarningsDelegate(QObject* parent);
    ~SimulationWarningsDelegate() override;

    SimulationWarningsDelegate(const SimulationWarningsDelegate&)            = delete;
    SimulationWarningsDelegate& operator=(const SimulationWarningsDelegate&) = delete;
    SimulationWarningsDelegate(SimulationWarningsDelegate&&)                 = delete;
    SimulationWarningsDelegate& operator=(SimulationWarningsDelegate&&)      = delete;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// The cell's size: cell()'s width, as high as the style makes a row of text or the tallest
    /// part.
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex&          index) const override;

    /// What the cell at @p index draws in @p option's rectangle, font, state and palette.
    [[nodiscard]] static Cell cell(const QStyleOptionViewItem& option, const QModelIndex& index);

    /// The counts of the cell at @p index (the model's roles); nullopt without data.
    [[nodiscard]] static std::optional<SimulationColumns::WarningCounts> counts(
        const QModelIndex& index);

    /// The parts of @p counts, laid out from @p rect's left edge (after kLeftPadding) and centred
    /// in its height, with @p font's metrics.
    [[nodiscard]] static std::vector<Part> layOut(const SimulationColumns::WarningCounts& counts,
                                                  const QRect& rect, const QFont& font);

private:
    /// The icon @p id (one of the three warning levels), made with the delegate.
    [[nodiscard]] const QIcon& icon(Icons::Id id) const;

    std::map<Icons::Id, QIcon> m_icons;
};

}  // namespace QtRocket::Gui
