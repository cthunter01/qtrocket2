#include "QtRocket/gui/dialogs/SeparationSelectionDialog.h"

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
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Gui
{

namespace
{

/// The slider of the altitude (getSliderModel(100, 1000)), in m.
constexpr double kSliderMinimum = 100;
constexpr double kSliderMaximum = 1000;

}  // namespace

SeparationSelectionDialog::SeparationSelectionDialog(const UserPrompts::SeparationRequest& request,
                                                     QWidget*                              parent)
  : FlightConfigurationValueDialog(
        //: edtmotorconfdlg.title.Selectseparationconf
        tr("Select Separation Configuration"),
        {//: SeparationSelectionDialog.opt.title
         .title = tr("Which flight configurations are affected:"),
         //: SeparationSelectionDialog.opt.default
         .allConfigurations = tr("Change all configurations using the default separation event"),
         //: SeparationSelectionDialog.opt.override
         .thisConfiguration = tr("Override for the {0} flight configuration only")},
        request.configurationName, request.initial.scope, parent),
    m_initial(request.initial.separation),
    // EnumModel(newConfiguration, "SeparationEvent"), DoubleModel(newConfiguration,
    // "SeparationAltitude", UnitGroup.UNITS_DISTANCE, 0) and DoubleModel(newConfiguration,
    // "SeparationDelay", UnitGroup.UNITS_SHORT_TIME, 0).
    m_event(bindings().make<EnumBinding<SeparationEvent>>(
        m_initial.getSeparationEvent(),
        std::span<const SeparationEvent>(StageSeparationConfiguration::kAllSeparationEvents))),
    m_altitudeBinding(bindings().make<DoubleBinding>(m_initial.getSeparationAltitude(),
                                                     unitGroup(UnitGroupId::DISTANCE), 0.0)),
    m_delay(bindings().make<DoubleBinding>(m_initial.getSeparationDelay(),
                                           unitGroup(UnitGroupId::SHORT_TIME), 0.0))
{
    setObjectName(QStringLiteral("SeparationSelectionDialog"));
    QWidget&     values = valueWidget();
    QGridLayout& grid   = valueGrid();

    // "Upper stage separates at:" and the combo box ("spanx 3, growx, wrap").
    //: ComponentAssemblyConfig.separation.lbl.SeparatesAt
    m_eventLabel = makeChild<QLabel>(&values, tr("Upper stage separates at:"));
    m_eventLabel->setObjectName(QStringLiteral("eventLabel"));
    grid.addWidget(m_eventLabel, 0, 0);
    m_eventBox = makeChild<EnumComboBox>(&values, m_event);
    m_eventBox->setObjectName(QStringLiteral("SeparationEvent"));
    grid.addWidget(m_eventBox, 0, 1, 1, 3);

    // "Altitude:", the spin box ("growx"), the unit selector ("growx") and the slider ("w 100lp,
    // wrap").
    m_altitude = ConfigFields::addNumberRow(
        grid, 1,
        //: ParachuteCfg.lbl.Altitude
        tr("Altitude:"), m_altitudeBinding, QStringLiteral("SeparationAltitude"),
        {.slider = SliderMapping::linear(kSliderMinimum, kSliderMaximum)});
    m_altitude.label->setObjectName(QStringLiteral("altText"));

    // "plus" at the right ("alignx 100%"), then the delay ("span, split") and "seconds" ("wrap
    // para").
    //: ComponentAssemblyConfig.separation.lbl.plus
    m_plusLabel = makeChild<QLabel>(&values, tr("plus"));
    m_plusLabel->setObjectName(QStringLiteral("plusLabel"));
    m_plusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    grid.addWidget(m_plusLabel, 2, 0);
    auto* delayRow    = makeChild<QWidget>(&values);
    auto* delayLayout = makeChild<QHBoxLayout>(delayRow);
    delayLayout->setContentsMargins(0, 0, 0, 0);
    m_delaySpinBox = makeChild<UnitSpinBox>(delayRow, m_delay);
    m_delaySpinBox->setObjectName(QStringLiteral("SeparationDelay"));
    delayLayout->addWidget(m_delaySpinBox);
    //: ComponentAssemblyConfig.separation.lbl.seconds
    m_secondsLabel = makeChild<QLabel>(delayRow, tr("seconds"));
    m_secondsLabel->setObjectName(QStringLiteral("secondsLabel"));
    delayLayout->addWidget(m_secondsLabel);
    delayLayout->addStretch(1);
    grid.addWidget(delayRow, 2, 1, 1, 3);
    grid.setColumnStretch(3, 1);

    // The combo box's action listener: updateState().
    bindings().keepConnection(m_event->addChangeListener(
        [this] { Guard::run([this] { updateState(); }, QStringLiteral("SeparationEvent")); }));
    updateState();

    addButtons();
}

SeparationSelectionDialog::~SeparationSelectionDialog() = default;

std::optional<UserPrompts::SeparationChoice> SeparationSelectionDialog::ask(
    QWidget* parent, const UserPrompts::SeparationRequest& request)
{
    ModalDialog<SeparationSelectionDialog> dialog(parent, request);
    const int                              result   = dialog.exec();
    const SeparationSelectionDialog* const answered = dialog.get();
    if (result != QDialog::Accepted || answered == nullptr)
    {
        return std::nullopt;
    }
    return answered->choice();
}

UserPrompts::SeparationChoice SeparationSelectionDialog::choice() const
{
    // The configuration Java's models edit, through its setters, and OK's rule for NEVER.
    StageSeparationConfiguration separation = m_initial;
    separation.setSeparationEvent(m_event->value());
    separation.setSeparationAltitude(m_altitudeBinding->value());
    separation.setSeparationDelay(m_delay->value());
    if (separation.getSeparationEvent() == SeparationEvent::NEVER)
    {
        separation.setSeparationDelay(0);
    }
    return {.scope = scope(), .separation = separation};
}

bool SeparationSelectionDialog::isAltitudeEnabled() const noexcept
{
    const SeparationEvent event = m_event->value();
    return event == SeparationEvent::ALTITUDE_ASCENDING ||
           event == SeparationEvent::ALTITUDE_DESCENDING;
}

void SeparationSelectionDialog::updateState() const
{
    const bool enabled = isAltitudeEnabled();
    m_altitude.label->setEnabled(enabled);
    m_altitude.spinBox->setFieldEnabled(enabled);
    m_altitude.unitSelector->setFieldEnabled(enabled);
    m_altitude.slider->setFieldEnabled(enabled);
}

}  // namespace QtRocket::Gui
