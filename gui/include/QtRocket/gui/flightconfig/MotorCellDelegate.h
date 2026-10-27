#pragma once

#include <QColor>
#include <QFont>
#include <QObject>
#include <QRect>
#include <QSize>
#include <QString>
#include <QStyledItemDelegate>

class QModelIndex;
class QPainter;
class QStyleOptionViewItem;

namespace QtRocket::Gui
{

/// How the Motors tab's table draws a motor mount's cell (OpenRocket's
/// MotorConfigurationPanel.MotorTableCellRenderer.format()): two labels side by side in a
/// horizontal BoxLayout, 10 px apart (Box.createRigidArea(new Dimension(10, 0))), the motor
/// (FlightConfigurableTableModel's Qt::DisplayRole: "None", "2× A10-0") and its ignition
/// (kIgnitionTextRole), left-aligned and centred in the cell's height. The motor is written in the
/// cell's font and the palette's text colour; the ignition too, but in italics and in ThemeColors'
/// TEXT_DIM when it shows the default ignition (kDefaultRole, Java's shaded()). In a selected cell
/// both take the palette's highlighted text colour and the ignition keeps its italics (Java's
/// setSelected() sets Table.selectionForeground on both labels and leaves their fonts). Each label
/// is as wide as its text (a JLabel's minimum width is its preferred width, so that BoxLayout
/// never narrows it): what does not fit in the cell is cut off at its edge, never elided.
///
/// Every other cell (one without an ignition text: column 0, and the cells of any other model) is
/// drawn as QStyledItemDelegate draws it, which shows a shaded recovery or stage cell through its
/// Qt::FontRole and Qt::ForegroundRole.
///
/// Deviation from OpenRocket: the labels start after the style's text margin (Java's cell border
/// is 1 px; the focus frame's).
class MotorCellDelegate final : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// The gap between the two labels (Java's rigid area).
    static constexpr int kLabelGap = 10;

    /// Where a motor cell's labels go and how they are written.
    struct Cell
    {
        /// The motor label: as wide as its text, from the cell's left edge after the style's
        /// margin, as high as the cell.
        QRect   motor;
        QString motorText;
        QFont   motorFont;
        QColor  motorColor;
        /// The ignition label: kLabelGap after the motor label, as wide as its text.
        QRect   ignition;
        QString ignitionText;
        QFont   ignitionFont;
        QColor  ignitionColor;
    };

    explicit MotorCellDelegate(QObject* parent = nullptr);
    ~MotorCellDelegate() override;

    MotorCellDelegate(const MotorCellDelegate&)            = delete;
    MotorCellDelegate& operator=(const MotorCellDelegate&) = delete;
    MotorCellDelegate(MotorCellDelegate&&)                 = delete;
    MotorCellDelegate& operator=(MotorCellDelegate&&)      = delete;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// A motor cell's size: both labels with their gap and the style's margins wide, as high as
    /// the style makes a row of text.
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex&          index) const override;

    /// Whether @p index is a motor cell: its kIgnitionTextRole holds a text.
    [[nodiscard]] static bool isMotorCell(const QModelIndex& index);

    /// The labels of the motor cell @p index in @p option's rectangle, state, font and palette
    /// (what paint() draws).
    [[nodiscard]] Cell cell(const QStyleOptionViewItem& option, const QModelIndex& index) const;
};

}  // namespace QtRocket::Gui
