#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"

#include <array>
#include <vector>

#include <QAbstractItemModel>
#include <QAction>
#include <QFont>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMargins>
#include <QMenu>
#include <QObject>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QString>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationEditing.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorCellDelegate.h"
#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

/// The context of the panel's guarded slots.
[[nodiscard]] QString context()
{
    return QStringLiteral("Motors");
}

/// A font that differs from the one it is set on in its weight only: bold or not (a titled
/// border's "<html><b>...</b></html>" title, and the box's contents in the font as it was).
[[nodiscard]] QFont weightOnly(bool bold)
{
    QFont font;
    font.setBold(bold);
    return font;
}

/// MigLayout's "insets n n 5px n": the bottom margin of the box "Motor configurations".
constexpr int kConfigurationBottomInset = 5;

}  // namespace

MotorConfigurationPanel::MotorConfigurationPanel(FlightConfigurationPanel& panel, QWidget* parent)
  : FlightConfigurablePanel(panel, FlightConfigurableTableModel::Kind::MOTOR_MOUNTS, parent),
    // The table's own child: it goes with the table.
    m_delegate(makeChild<MotorCellDelegate>(&table())),
    //: MotorConfigurationPanel.lbl.motorMounts
    m_mountsBox(makeChild<QGroupBox>(this, tr("Motor mounts"))),
    m_mountPanel(makeChild<MotorMountConfigurationPanel>(m_mountsBox, editing())),
    m_cards(makeChild<QStackedWidget>(this)),
    m_helpText(makeChild<QLabel>(m_cards, noMountsText())),
    //: MotorConfigurationPanel.lbl.motorConfiguration
    m_configurationBox(makeChild<QGroupBox>(m_cards, tr("Motor configurations")))
{
    setObjectName(QStringLiteral("motorConfigurationPanel"));
    // Java: setDefaultRenderer(Object.class, new MotorTableCellRenderer()), every cell.
    table().setItemDelegate(m_delegate);
    makeActions();

    // MigLayout("fill"): the box "Motor mounts" ("split, growy, pushy"), then the cards
    // ("gapleft para, grow, pushy"): the help, or the box "Motor configurations".
    auto* layout = makeChild<QHBoxLayout>(this);
    arrangeMountsBox();
    layout->addWidget(m_mountsBox);
    layout->addSpacing(kParagraphGap);
    m_cards->setObjectName(QStringLiteral("cards"));
    m_helpText->setObjectName(QStringLiteral("helpText"));
    m_helpText->setTextFormat(Qt::RichText);
    m_cards->addWidget(m_helpText);
    arrangeConfigurationBox();
    m_cards->addWidget(m_configurationBox);
    layout->addWidget(m_cards, 1);

    // Java's listener of the table's model: a mount added in the component dialog shows the
    // table.
    const auto refresh = guarded([this] { updateButtonState(); }, context());
    connect(&tableModel(), &QAbstractItemModel::modelReset, this, refresh);
    connect(&tableModel(), &QAbstractItemModel::dataChanged, this, refresh);
    connect(&tableModel(), &QAbstractItemModel::headerDataChanged, this, refresh);

    updateButtonState();
}

QString MotorConfigurationPanel::noMountsText()
{
    //: MotorConfigurationPanel.lbl.nomotors (%s: the name of the tab "Rocket design")
    QString help =
        tr("<html>No motor mounts defined.<br>Add a tube in the '%1' tab and select it "
           "from the list on the left.</html>");
    if (help.contains(QStringLiteral("%1")))
    {
        //: BasicFrame.tab.Rocketdesign
        help = help.arg(tr("Rocket design"));
    }
    return help;
}

MotorConfigurationPanel::~MotorConfigurationPanel() = default;

void MotorConfigurationPanel::makeActions()
{
    //: MotorConfigurationPanel.btn.selectMotor
    m_selectMotor = makeAction(tr("Select motor"), QStringLiteral("selectMotorAction"),
                               [this] { selectMotor(); });
    //: MotorConfigurationPanel.btn.deleteMotor
    m_deleteMotor = makeAction(tr("Delete motor"), QStringLiteral("deleteMotorAction"),
                               [this] { deleteMotor(); });
    //: MotorConfigurationPanel.btn.selectIgnition
    m_selectIgnition = makeAction(tr("Select ignition"), QStringLiteral("selectIgnitionAction"),
                                  [this] { selectIgnition(); });
    //: MotorConfigurationPanel.btn.resetIgnition
    m_resetIgnition = makeAction(tr("Reset ignition"), QStringLiteral("resetIgnitionAction"),
                                 [this] { resetIgnition(); });

    // Java's popupMenuFull.
    QMenu&                          menu  = fullPopupMenu();
    const FlightConfigurationPanel& owner = flightConfigurationPanel();
    menu.addAction(m_selectMotor);
    menu.addAction(m_deleteMotor);
    menu.addSeparator();
    menu.addAction(m_selectIgnition);
    menu.addAction(m_resetIgnition);
    menu.addSeparator();
    menu.addAction(owner.renameConfigurationAction());
    menu.addAction(owner.deleteConfigurationAction());
    menu.addAction(owner.duplicateConfigurationAction());
}

void MotorConfigurationPanel::arrangeMountsBox()
{
    m_mountsBox->setObjectName(QStringLiteral("motorMountsBox"));
    m_mountsBox->setFont(weightOnly(true));
    m_mountsBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    // MigLayout("inset 0, fill").
    auto* layout = makeChild<QVBoxLayout>(m_mountsBox);
    layout->setContentsMargins(0, 0, 0, 0);
    m_mountPanel->setFont(weightOnly(false));
    layout->addWidget(m_mountPanel);
}

void MotorConfigurationPanel::arrangeConfigurationBox()
{
    QGroupBox* const box = m_configurationBox;
    box->setObjectName(QStringLiteral("configurationPanel"));
    box->setFont(weightOnly(true));
    auto* boxLayout = makeChild<QVBoxLayout>(box);
    boxLayout->setContentsMargins(0, 0, 0, 0);
    auto* content = makeChild<QWidget>(box);
    content->setFont(weightOnly(false));
    boxLayout->addWidget(content);

    // MigLayout("fill, insets n n 5px n"): the table ("spanx, grow, pushy, wrap"), then the
    // buttons on the right, of one width, Select ignition "gapleft para".
    auto*          layout  = makeChild<QVBoxLayout>(content);
    const QMargins margins = layout->contentsMargins();
    layout->setContentsMargins(margins.left(), margins.top(), margins.right(),
                               kConfigurationBottomInset);
    layout->addWidget(&table(), 1);
    auto* row       = makeChild<QWidget>(content);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addStretch(1);
    m_selectMotorButton = makeButton(*m_selectMotor, QStringLiteral("selectMotorButton"), row);
    m_deleteMotorButton = makeButton(*m_deleteMotor, QStringLiteral("deleteMotorButton"), row);
    m_selectIgnitionButton =
        makeButton(*m_selectIgnition, QStringLiteral("selectIgnitionButton"), row);
    m_resetIgnitionButton =
        makeButton(*m_resetIgnition, QStringLiteral("resetIgnitionButton"), row);
    rowLayout->addWidget(m_selectMotorButton);
    rowLayout->addWidget(m_deleteMotorButton);
    rowLayout->addSpacing(kParagraphGap);
    rowLayout->addWidget(m_selectIgnitionButton);
    rowLayout->addWidget(m_resetIgnitionButton);
    const std::array<QPushButton*, 4> buttons{m_selectMotorButton, m_deleteMotorButton,
                                              m_selectIgnitionButton, m_resetIgnitionButton};
    shareWidth(buttons);
    layout->addWidget(row);
}

bool MotorConfigurationPanel::isShowingHelp() const
{
    return m_cards->currentWidget() == m_helpText;
}

void MotorConfigurationPanel::updateButtonState()
{
    if (m_selectMotor == nullptr)
    {
        return;  // still being made
    }
    bool enabled = false;
    if (tableModel().columnCount() > 1)
    {
        m_cards->setCurrentWidget(m_configurationBox);
        enabled = selectedComponent() != nullptr;
    }
    else
    {
        m_cards->setCurrentWidget(m_helpText);
    }
    m_selectMotor->setEnabled(enabled);
    m_deleteMotor->setEnabled(enabled);
    m_selectIgnition->setEnabled(enabled);
    m_resetIgnition->setEnabled(enabled);
}

bool MotorConfigurationPanel::restructuresWith(const RocketComponent& source) const
{
    return dynamic_cast<const BodyTube*>(&source) != nullptr ||
           dynamic_cast<const InnerTube*>(&source) != nullptr;
}

void MotorConfigurationPanel::selectValue()
{
    selectMotor();
}

void MotorConfigurationPanel::selectMotor()
{
    const std::vector<Uuid>                  mounts = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (mounts.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &mounts] { return editing().selectMotor(ids, mounts); });
}

void MotorConfigurationPanel::deleteMotor()
{
    const std::vector<Uuid>                  mounts = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (mounts.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &mounts] { return editing().deleteMotors(ids, mounts); });
}

void MotorConfigurationPanel::selectIgnition()
{
    const std::vector<Uuid>                  mounts = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (mounts.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &mounts] { return editing().selectIgnition(ids, mounts); });
}

void MotorConfigurationPanel::resetIgnition()
{
    const std::vector<Uuid>                  mounts = selectedComponentIds();
    const std::vector<FlightConfigurationId> ids    = selectedConfigurationIds();
    if (mounts.empty() || ids.empty())
    {
        return;
    }
    runOperation([this, &ids, &mounts] { return editing().resetIgnition(ids, mounts); });
}

}  // namespace QtRocket::Gui
