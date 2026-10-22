#pragma once

#include <map>
#include <utility>
#include <vector>

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyledItemDelegate>

#include "QtRocket/gui/util/Icons.h"

class QAbstractItemView;
class QHelpEvent;
class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

namespace QtRocket::Gui
{

/// How the component tree draws a row (OpenRocket's ComponentTreeRenderer): the component's
/// small icon at its natural size, 4 px, the name, and after another 4 px the markers of
/// ComponentTreeModel::markers(), 3 px apart (BorderLayout.setHgap(4) between the three parts,
/// combineIcons(3, ...)), each marker as high as Icons::iconHeight() makes it next to the
/// row's font. A component in an inactive stage is written in italics and in the muted colour
/// of hidden components (ThemeColors::VISIBILITY_HIDDEN_FOREGROUND), a hidden one in the muted
/// colour alone; a selected row shows neither. The tooltip of a row is the model's
/// ComponentTreeModel::toolTip() for the view's selection: the multi-component tooltip when
/// the hovered row is one of several selected rows.
///
/// For a model other than a ComponentTreeModel it draws the row as QStyledItemDelegate does.
///
/// Deviations from OpenRocket: the selection is drawn by the platform's style over the whole
/// row, where OpenRocket's renderer fills the name's label only; a name that does not fit in
/// the row is elided (OpenRocket's tree grows sideways instead); the font is the row's (the
/// view's) font, plain or italic, where OpenRocket resets it to plain.
class ComponentTreeDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// The gap between the icon, the name and the markers (OpenRocket's BorderLayout hgap).
    static constexpr int kPartGap = 4;
    /// The gap between two markers (OpenRocket's combineIcons(3, ...)).
    static constexpr int kMarkerSpacing = 3;

    /// Where a row's parts go and how its name is written.
    struct Cell
    {
        /// The icon's rectangle (empty, of width 0, for the Rocket, which has no icon).
        QRect icon;
        /// The name's rectangle: as wide as the name, or the space left for it.
        QRect text;
        /// The name as it is drawn: elided when it does not fit.
        QString displayText;
        /// Each marker and its rectangle, left to right.
        std::vector<std::pair<Icons::Id, QRect>> markers;
        /// The name's font: the row's, in italics for an unselected row in an inactive stage.
        QFont font;
        /// The name's colour: the palette's text (highlighted text when selected), or the muted
        /// colour of a hidden component or one in an inactive stage when not selected.
        QColor textColor;
    };

    explicit ComponentTreeDelegate(QObject* parent);
    ~ComponentTreeDelegate() override;

    ComponentTreeDelegate(const ComponentTreeDelegate&)            = delete;
    ComponentTreeDelegate& operator=(const ComponentTreeDelegate&) = delete;
    ComponentTreeDelegate(ComponentTreeDelegate&&)                 = delete;
    ComponentTreeDelegate& operator=(ComponentTreeDelegate&&)      = delete;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// The row's size: as wide as the icon, the name and the markers with their gaps and the
    /// style's margins, as high as the style makes a row of text or the tallest part.
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex&          index) const override;

    /// Shows toolTip() for a tooltip event.
    bool helpEvent(QHelpEvent* event, QAbstractItemView* view, const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    /// The parts of the row at @p index in @p option's rectangle, state and palette (what
    /// paint() draws).
    [[nodiscard]] Cell cell(const QStyleOptionViewItem& option, const QModelIndex& index) const;

    /// The tooltip of the row at @p index of @p view: ComponentTreeModel::toolTip() with the
    /// view's selection, in its order; the model's Qt::ToolTipRole for another model.
    [[nodiscard]] static QString toolTip(const QAbstractItemView& view, const QModelIndex& index);

private:
    /// The icon of @p marker, made once.
    [[nodiscard]] const QIcon& markerIcon(Icons::Id marker) const;

    std::map<Icons::Id, QIcon> m_markerIcons;
    QIcon                      m_noIcon;
};

}  // namespace QtRocket::Gui
