#include "QtRocket/gui/dialogs/DeploymentSelectionDialog.h"

#include <optional>
#include <span>

#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QWidget>
#include <Qt>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/dialogs/FlightConfigurationValueDialog.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// The slider of the altitude (getSliderModel(100, 1000)), in m.
constexpr double kSliderMinimum = 100;
constexpr double kSliderMaximum = 1000;

}  // namespace

DeploymentSelectionDialog::DeploymentSelectionDialog(const UserPrompts::DeploymentRequest& request,
                                                     QWidget*                              parent)
  : FlightConfigurationValueDialog(
        //: edtmotorconfdlg.title.Selectdeploymentconf
        tr("Select Deployment Configuration"),
        {//: DeploymentSelectionDialog.opt.title
         .title = tr("Which flight configurations are affected:"),
         //: DeploymentSelectionDialog.opt.default
         .allConfigurations = tr("Change all configurations using the default deployment event"),
         //: DeploymentSelectionDialog.opt.override
         .thisConfiguration = tr("Override for the {0} flight configuration only")},
        request.configurationName, request.initial.scope, parent),
    m_initial(request.initial.deployment),
    // EnumModel(newConfiguration, "DeployEvent"), DoubleModel(newConfiguration,
    // "DeployAltitude", UnitGroup.UNITS_DISTANCE, 0) and DoubleModel(newConfiguration,
    // "DeployDelay", UnitGroup.UNITS_SHORT_TIME, 0).
    m_event(bindings().make<EnumBinding<DeployEvent>>(
        m_initial.getDeployEvent(),
        std::span<const DeployEvent>(DeploymentConfiguration::kAllDeployEvents))),
    m_altitudeBinding(bindings().make<DoubleBinding>(m_initial.getDeployAltitude(),
                                                     unitGroup(UnitGroupId::DISTANCE), 0.0)),
    m_delay(bindings().make<DoubleBinding>(m_initial.getDeployDelay(),
                                           unitGroup(UnitGroupId::SHORT_TIME), 0.0))
{
    setObjectName(QStringLiteral("DeploymentSelectionDialog"));
    QWidget&     values = valueWidget();
    QGridLayout& grid   = valueGrid();

    // "Deploys at:" and the combo box ("spanx 3, growx, wrap"); the bottom stage has no lower
    // stage to separate from.
    //: ParachuteCfg.lbl.Deploysat
    m_eventLabel = makeChild<QLabel>(&values, tr("Deploys at:"));
    m_eventLabel->setObjectName(QStringLiteral("eventLabel"));
    grid.addWidget(m_eventLabel, 0, 0);
    if (request.bottomStage)
    {
        m_event->removeElement(DeployEvent::LOWER_STAGE_SEPARATION);
    }
    m_eventBox = makeChild<EnumComboBox>(&values, m_event);
    m_eventBox->setObjectName(QStringLiteral("DeployEvent"));
    grid.addWidget(m_eventBox, 0, 1, 1, 3);

    // "Altitude:", the spin box ("growx"), the unit selector ("growx") and the slider ("w 100lp,
    // wrap").
    m_altitude = ConfigFields::addNumberRow(
        grid, 1,
        //: ParachuteCfg.lbl.Altitude
        tr("Altitude:"), m_altitudeBinding, QStringLiteral("DeployAltitude"),
        {.slider = SliderMapping::linear(kSliderMinimum, kSliderMaximum)});
    m_altitude.label->setObjectName(QStringLiteral("altText"));

    // "plus" at the right, then the delay ("spanx, split") and "seconds" ("wrap paragraph").
    //: ParachuteCfg.lbl.plusdelay
    m_plusLabel = makeChild<QLabel>(&values, tr("plus"));
    m_plusLabel->setObjectName(QStringLiteral("plusLabel"));
    m_plusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid.addWidget(m_plusLabel, 2, 0);
    auto* delayRow    = makeChild<QWidget>(&values);
    auto* delayLayout = makeChild<QHBoxLayout>(delayRow);
    delayLayout->setContentsMargins(0, 0, 0, 0);
    m_delaySpinBox = makeChild<UnitSpinBox>(delayRow, m_delay);
    m_delaySpinBox->setObjectName(QStringLiteral("DeployDelay"));
    delayLayout->addWidget(m_delaySpinBox);
    //: ParachuteCfg.lbl.seconds
    m_secondsLabel = makeChild<QLabel>(delayRow, tr("seconds"));
    m_secondsLabel->setObjectName(QStringLiteral("secondsLabel"));
    delayLayout->addWidget(m_secondsLabel);
    delayLayout->addStretch(1);
    grid.addWidget(delayRow, 2, 1, 1, 3);
    grid.setColumnStretch(3, 1);

    // The combo box's action listener: updateState().
    bindings().keepConnection(m_event->addChangeListener(
        [this] { Guard::run([this] { updateState(); }, QStringLiteral("DeployEvent")); }));
    updateState();

    addButtons();
}

DeploymentSelectionDialog::~DeploymentSelectionDialog() = default;

std::optional<UserPrompts::DeploymentChoice> DeploymentSelectionDialog::ask(
    QWidget* parent, const UserPrompts::DeploymentRequest& request)
{
    ModalDialog<DeploymentSelectionDialog> dialog(parent, request);
    const int                              result   = dialog.exec();
    const DeploymentSelectionDialog* const answered = dialog.get();
    if (result != QDialog::Accepted || answered == nullptr)
    {
        return std::nullopt;
    }
    return answered->choice();
}

UserPrompts::DeploymentChoice DeploymentSelectionDialog::choice() const
{
    // The clone Java's models edit, through its setters.
    DeploymentConfiguration deployment = m_initial;
    deployment.setDeployEvent(m_event->value());
    deployment.setDeployAltitude(m_altitudeBinding->value());
    deployment.setDeployDelay(m_delay->value());
    return {.scope = scope(), .deployment = deployment};
}

bool DeploymentSelectionDialog::isAltitudeEnabled() const noexcept
{
    return m_event->value() == DeployEvent::ALTITUDE;
}

void DeploymentSelectionDialog::updateState() const
{
    const bool enabled = isAltitudeEnabled();
    m_altitude.label->setEnabled(enabled);
    m_altitude.spinBox->setFieldEnabled(enabled);
    m_altitude.unitSelector->setFieldEnabled(enabled);
    m_altitude.slider->setFieldEnabled(enabled);
}

}  // namespace QtRocket::Gui
