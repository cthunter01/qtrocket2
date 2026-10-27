#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QEvent>
#include <QHeaderView>
#include <QLayout>
#include <QMargins>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPersistentModelIndex>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QTableView>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/flightconfig/TableTransferKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motor mounts");
}

}  // namespace

MotorMountConfigurationPanel::MotorMountConfigurationPanel(FlightConfigurationEditing& editing,
                                                           QWidget*                    parent)
  : QWidget(parent),
    m_editing(&editing),
    m_model(makeChild<MotorMountTableModel>(this, editing.rocket())),
    m_table(makeChild<QTableView>(this)),
    // A JTable's Cut, Copy and Paste keys, which copy nothing here: the window's items of those
    // keys do not act while the table has the focus.
    m_transferKeys(makeChild<TableTransferKeys>(m_table, TableTransferKeys::CellSource()))
{
    setObjectName(QStringLiteral("motorMountConfigurationPanel"));
    m_table->setObjectName(QStringLiteral("table"));
    m_table->setModel(m_model);
    // Java: setTableHeader(null), setShowVerticalLines(false), no row or column selection,
    // AUTO_RESIZE_LAST_COLUMN.
    m_table->horizontalHeader()->hide();
    m_table->verticalHeader()->hide();
    m_table->setShowGrid(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setWordWrap(false);
    m_table->setCornerButtonEnabled(false);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sizeCheckColumn();

    // MigLayout(""): the scroll pane "w 200lp, h 150lp, grow, pushy".
    auto* layout = makeChild<QVBoxLayout>(this);
    layout->addWidget(m_table);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    // The check box asks; the mount changes with its own undo step (bug 5).
    connect(m_model, &MotorMountTableModel::motorMountChangeRequested, this,
            guarded([this](const Uuid& mount,
                           bool        motorMount) { m_editing->setMotorMount(mount, motorMount); },
                    context()));
    m_table->viewport()->installEventFilter(this);
}

MotorMountConfigurationPanel::~MotorMountConfigurationPanel()
{
    m_table->viewport()->removeEventFilter(this);
}

QSize MotorMountConfigurationPanel::sizeHint() const
{
    const QMargins margins = layout() == nullptr ? QMargins() : layout()->contentsMargins();
    return {kTableWidth + margins.left() + margins.right(),
            kTableHeight + margins.top() + margins.bottom()};
}

bool MotorMountConfigurationPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_table->viewport())
    {
        return QWidget::eventFilter(watched, event);
    }
    const auto* mouse = dynamic_cast<const QMouseEvent*>(event);
    if (mouse == nullptr || mouse->button() != Qt::LeftButton)
    {
        return QWidget::eventFilter(watched, event);
    }
    const QModelIndex index = m_table->indexAt(mouse->position().toPoint());
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonDblClick:
            m_pressed = index;
            return false;
        case QEvent::MouseButtonRelease:
        {
            // A click (its press and release on one cell) toggles the row's check box; the view
            // does not get the release, so that its check box does not toggle again.
            const bool click = index.isValid() && QModelIndex(m_pressed) == index;
            m_pressed        = QPersistentModelIndex();
            if (click)
            {
                Guard::run([this, &index] { toggle(index); }, context());
            }
            return index.isValid();
        }
        default:
            return QWidget::eventFilter(watched, event);
    }
}

void MotorMountConfigurationPanel::sizeCheckColumn()
{
    // Java: the check box column as wide as a row is high plus 2 pixels.
    const int    width  = m_table->verticalHeader()->defaultSectionSize() + 2;
    QHeaderView* header = m_table->horizontalHeader();
    header->setMinimumSectionSize(std::min(header->minimumSectionSize(), width));
    header->resizeSection(MotorMountTableModel::kMotorMountColumn, width);
}

void MotorMountConfigurationPanel::toggle(const QModelIndex& index)
{
    const QModelIndex check = m_model->index(index.row(), MotorMountTableModel::kMotorMountColumn);
    const bool        checked =
        m_model->data(check, Qt::CheckStateRole).toInt() == static_cast<int>(Qt::Checked);
    m_model->setData(check, static_cast<int>(checked ? Qt::Unchecked : Qt::Checked),
                     Qt::CheckStateRole);
}

}  // namespace QtRocket::Gui
