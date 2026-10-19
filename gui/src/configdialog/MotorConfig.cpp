#include "QtRocket/gui/configdialog/MotorConfig.h"

#include <cstddef>
#include <memory>
#include <span>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/binding/BoolBinding.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/CommonStrings.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

/// @p component as a MotorMount.
/// @throws BugError when it is none
[[nodiscard]] const MotorMount& mountOf(const RocketComponent& component)
{
    const auto* mount = dynamic_cast<const MotorMount*>(&component);
    if (mount == nullptr)
    {
        bug("The motor tab needs a motor mount");
    }
    return *mount;
}

/// Calls @p change on the default motor configuration of @p component when it is a MotorMount
/// (Java forwards the setters to the selected mounts), and fires kDefaultMotorEventType on the
/// mount when the configuration changed (see MotorConfig).
template <class Change>
void changeDefaultIgnition(RocketComponent& component, const Change& change)
{
    auto* mount = dynamic_cast<MotorMount*>(&component);
    if (mount == nullptr)
    {
        return;
    }
    MotorConfiguration& configuration = mount->getDefaultMotorConfig();
    const IgnitionEvent event         = configuration.getIgnitionEvent();
    const double        delay         = configuration.getIgnitionDelay();
    const bool          overridden    = configuration.hasIgnitionOverride();
    change(configuration);
    if (configuration.getIgnitionEvent() != event || configuration.getIgnitionDelay() != delay ||
        configuration.hasIgnitionOverride() != overridden)
    {
        component.fireComponentChangeEvent(MotorConfigurationSet::kDefaultMotorEventType);
    }
}

}  // namespace

BoolSource MotorConfig::motorMountSource()
{
    return BoolSource::of<RocketComponent>({
        .get = [](const RocketComponent& component) { return mountOf(component).isMotorMount(); },
        .set =
            [](RocketComponent& component, const bool& acting) {
                if (auto* mount = dynamic_cast<MotorMount*>(&component))
                {
                    mount->setMotorMount(acting);
                }
            },
    });
}

DoubleSource MotorConfig::motorOverhangSource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) { return mountOf(component).getMotorOverhang(); },
        .set =
            [](RocketComponent& component, const double& overhang) {
                if (auto* mount = dynamic_cast<MotorMount*>(&component))
                {
                    mount->setMotorOverhang(overhang);
                }
            },
    });
}

EnumSource<IgnitionEvent> MotorConfig::ignitionEventSource()
{
    return EnumSource<IgnitionEvent>::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return mountOf(component).getDefaultMotorConfig().getIgnitionEvent();
            },
        .set =
            [](RocketComponent& component, const IgnitionEvent& event) {
                changeDefaultIgnition(component, [event](MotorConfiguration& configuration) {
                    configuration.setIgnitionEvent(event);
                });
            },
    });
}

DoubleSource MotorConfig::ignitionDelaySource()
{
    return DoubleSource::of<RocketComponent>({
        .get =
            [](const RocketComponent& component) {
                return mountOf(component).getDefaultMotorConfig().getIgnitionDelay();
            },
        .set =
            [](RocketComponent& component, const double& delay) {
                changeDefaultIgnition(component, [delay](MotorConfiguration& configuration) {
                    configuration.setIgnitionDelay(delay);
                });
            },
    });
}

MotorConfig::MotorConfig(BindingScope& scope, QWidget* parent) : QWidget(parent)
{
    const std::shared_ptr<ComponentSelection>& selection = scope.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The motor tab needs a valid selection");
    }
    static_cast<void>(mountOf(selection->getMain()));
    auto* layout = makeChild<QVBoxLayout>(this);

    //: MotorCfg.checkbox.compmotormount
    m_mount = makeChild<BoolCheckBox>(this, scope.make<BoolBinding>(selection, motorMountSource()),
                                      tr("This component is a motor mount"));
    m_mount->setObjectName(QStringLiteral("MotorMount"));
    layout->addWidget(m_mount);

    m_panel = makeChild<QWidget>(this);
    layout->addWidget(m_panel);
    layout->addStretch(1);
    auto* grid = makeChild<QGridLayout>(m_panel);

    //  Overhang
    const auto overhang = scope.make<DoubleBinding>(
        selection, motorOverhangSource(), unitGroup(UnitGroupId::LENGTH), DoubleBinding::Options{});
    m_overhang =
        ConfigFields::addNumberRow(*grid, 0,
                                   //: MotorCfg.lbl.Motoroverhang
                                   tr("Motor overhang:"), overhang, QStringLiteral("MotorOverhang"),
                                   {.slider = SliderMapping::linear(-0.02, 0.06)});

    // The ignition event and delay of the default motor configuration.
    //: MotorCfg.lbl.Ignitionat
    grid->addWidget(
        makeChild<QLabel>(m_panel, tr("Ignition at:") + QLatin1Char(' ') + CommonStrings::dagger()),
        1, 0);
    m_event = makeChild<EnumComboBox>(
        m_panel,
        scope.make<EnumBinding<IgnitionEvent>>(selection, ignitionEventSource(),
                                               std::span<const IgnitionEvent>(kAllIgnitionEvents)));
    m_event->setObjectName(QStringLiteral("IgnitionEvent"));
    grid->addWidget(m_event, 1, 1, 1, 3);

    // "plus <delay> seconds", under the event (Java: a dimensionless spinner, at least 0).
    auto* delayRow    = makeChild<QWidget>(m_panel);
    auto* delayLayout = makeChild<QHBoxLayout>(delayRow);
    delayLayout->setContentsMargins(0, 0, 0, 0);
    //: MotorCfg.lbl.plus
    delayLayout->addWidget(makeChild<QLabel>(delayRow, tr("plus")));
    m_delay.spinBox = makeChild<UnitSpinBox>(
        delayRow,
        scope.make<DoubleBinding>(selection, ignitionDelaySource(), unitGroup(UnitGroupId::NONE),
                                  DoubleBinding::Options{.min = 0.0}));
    m_delay.spinBox->setObjectName(QStringLiteral("IgnitionDelay"));
    delayLayout->addWidget(m_delay.spinBox);
    //: MotorCfg.lbl.seconds
    delayLayout->addWidget(makeChild<QLabel>(delayRow, tr("seconds")));
    delayLayout->addStretch(1);
    grid->addWidget(delayRow, 2, 1, 1, 3);

    grid->addWidget(makeChild<StyledLabel>(m_panel, CommonStrings::overrideDescription(), -1.0), 3,
                    0, 1, 4);

    // The number of stages, counted now (Java).
    const std::size_t stages = selection->getRocket().getChildCount();
    QString           stagesText;
    if (stages == 1)
    {
        //: MotorCfg.lbl.longA1
        stagesText = tr("The current design has only one stage.") + QLatin1Char(' ') +
                     //: MotorCfg.lbl.longA2
                     tr("Stages can be added by clicking \"New stage\".");
    }
    else
    {
        //: MotorCfg.lbl.longB1
        stagesText = tr("The current design has") + QLatin1Char(' ') + QString::number(stages) +
                     QLatin1Char(' ') +
                     //: MotorCfg.lbl.longB2
                     tr("stages.");
    }
    m_stages = makeChild<StyledLabel>(m_panel, stagesText, -1.0);
    grid->addWidget(m_stages, 4, 0, 1, 4);
    grid->setColumnStretch(3, 1);

    // The rest is enabled only while the component is a motor mount (Java: setDeepEnabled()).
    m_mount->addEnableFollower(*m_panel);
}

}  // namespace QtRocket::Gui
