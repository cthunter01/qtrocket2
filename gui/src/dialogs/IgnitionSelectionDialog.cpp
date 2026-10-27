#include "QtRocket/gui/dialogs/IgnitionSelectionDialog.h"

#include <optional>
#include <span>

#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QWidget>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

IgnitionSelectionDialog::IgnitionSelectionDialog(const UserPrompts::IgnitionRequest& request,
                                                 QWidget*                            parent)
  : FlightConfigurationValueDialog(
        //: edtmotorconfdlg.title.Selectignitionconf
        tr("Select Ignition Configuration"),
        {//: IgnitionSelectionDialog.opt.title
         .title = tr("Which flight configurations are affected:"),
         //: IgnitionSelectionDialog.opt.default
         .allConfigurations = tr("Change all configurations using the default ignition event"),
         //: IgnitionSelectionDialog.opt.override
         .thisConfiguration = tr("Override for the {0} flight configuration only")},
        request.configurationName, request.initial.scope, parent),
    // EnumModel(curMotorInstance, "IgnitionEvent", IgnitionEvent.values()) and
    // DoubleModel(curMotorInstance, "IgnitionDelay", UnitGroup.UNITS_SHORT_TIME, 0).
    m_event(bindings().make<EnumBinding<IgnitionEvent>>(
        request.initial.event, std::span<const IgnitionEvent>(kAllIgnitionEvents))),
    m_delay(bindings().make<DoubleBinding>(request.initial.delay,
                                           unitGroup(UnitGroupId::SHORT_TIME), 0.0)),
    //: MotorCfg.lbl.Ignitionat
    m_eventLabel(makeChild<QLabel>(&valueWidget(), tr("Ignition at:"))),
    m_eventBox(makeChild<EnumComboBox>(&valueWidget(), m_event))
{
    setObjectName(QStringLiteral("IgnitionSelectionDialog"));
    QGridLayout& grid = valueGrid();

    // "Ignition at:" and the combo box ("growx, wrap").
    m_eventLabel->setObjectName(QStringLiteral("eventLabel"));
    grid.addWidget(m_eventLabel, 0, 0);
    m_eventBox->setObjectName(QStringLiteral("IgnitionEvent"));
    grid.addWidget(m_eventBox, 0, 1);

    // Under the combo box ("gap indent, skip 1, span, split"): "plus", the delay ("gap rel rel")
    // and "seconds" ("wrap unrel").
    auto* delayRow    = makeChild<QWidget>(&valueWidget());
    auto* delayLayout = makeChild<QHBoxLayout>(delayRow);
    delayLayout->setContentsMargins(kIndentGap, 0, 0, 0);
    //: MotorCfg.lbl.plus
    m_plusLabel = makeChild<QLabel>(delayRow, tr("plus"));
    m_plusLabel->setObjectName(QStringLiteral("plusLabel"));
    delayLayout->addWidget(m_plusLabel);
    m_delaySpinBox = makeChild<UnitSpinBox>(delayRow, m_delay);
    m_delaySpinBox->setObjectName(QStringLiteral("IgnitionDelay"));
    delayLayout->addWidget(m_delaySpinBox);
    //: MotorCfg.lbl.seconds
    m_secondsLabel = makeChild<QLabel>(delayRow, tr("seconds"));
    m_secondsLabel->setObjectName(QStringLiteral("secondsLabel"));
    delayLayout->addWidget(m_secondsLabel);
    delayLayout->addStretch(1);
    grid.addWidget(delayRow, 1, 1);
    grid.setColumnStretch(1, 1);

    addButtons();
}

IgnitionSelectionDialog::~IgnitionSelectionDialog() = default;

std::optional<UserPrompts::IgnitionChoice> IgnitionSelectionDialog::ask(
    QWidget* parent, const UserPrompts::IgnitionRequest& request)
{
    ModalDialog<IgnitionSelectionDialog> dialog(parent, request);
    const int                            result   = dialog.exec();
    const IgnitionSelectionDialog* const answered = dialog.get();
    if (result != QDialog::Accepted || answered == nullptr)
    {
        return std::nullopt;
    }
    return answered->choice();
}

UserPrompts::IgnitionChoice IgnitionSelectionDialog::choice() const
{
    return {.scope = scope(), .event = m_event->value(), .delay = m_delay->value()};
}

}  // namespace QtRocket::Gui
