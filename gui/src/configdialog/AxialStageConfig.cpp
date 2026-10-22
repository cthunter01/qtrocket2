#include "QtRocket/gui/configdialog/AxialStageConfig.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLatin1Char>
#include <QList>
#include <QRadioButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>

#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/gui/configdialog/CommonStrings.h"
#include "QtRocket/gui/configdialog/ComponentAssemblyConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BindingScope.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

/// @p component as an AxialStage.
/// @throws BugError when it is none
[[nodiscard]] AxialStage& stageOf(RocketComponent& component)
{
    auto* stage = dynamic_cast<AxialStage*>(&component);
    if (stage == nullptr)
    {
        bug("The configuration of a stage needs a stage");
    }
    return *stage;
}

/// The separation of @p stage in the rocket's selected flight configuration, as the panel shows
/// it: the configuration's own (the copy stored when the panel was made) or the default.
[[nodiscard]] const StageSeparationConfiguration& selectedSeparation(const AxialStage& stage)
{
    return stage.getSeparationConfigurations().get(
        stage.getRocket().getSelectedConfiguration().getId());
}

/// Calls @p change on the separation of @p stage in the rocket's selected flight configuration
/// (copied from the default first when it uses that) and fires EVENT_CHANGE on the stage when
/// the configuration changed (see AxialStageConfig).
template <class Change>
void changeSeparation(AxialStage& stage, const Change& change)
{
    StageSeparationConfiguration&      configuration = stage.getSeparationConfiguration();
    const StageSeparationConfiguration before        = configuration;
    change(configuration);
    if (configuration != before)
    {
        stage.fireComponentChangeEvent(ComponentChangeEvent::kEventChange);
    }
}

/// The selection of @p panel, which must be valid and edit a stage.
[[nodiscard]] const std::shared_ptr<ComponentSelection>& stageSelection(
    const RocketComponentConfig& panel)
{
    const std::shared_ptr<ComponentSelection>& selection = panel.selection();
    if (selection == nullptr || !selection->isValid())
    {
        bug("The configuration of a stage needs a valid selection");
    }
    static_cast<void>(stageOf(selection->getMain()));
    return selection;
}

}  // namespace

EnumSource<SeparationEvent> AxialStageConfig::separationEventSource()
{
    return EnumSource<SeparationEvent>::of<AxialStage>({
        .get =
            [](const AxialStage& stage) { return selectedSeparation(stage).getSeparationEvent(); },
        .set =
            [](AxialStage& stage, const SeparationEvent& event) {
                changeSeparation(stage, [event](StageSeparationConfiguration& configuration) {
                    configuration.setSeparationEvent(event);
                });
            },
    });
}

DoubleSource AxialStageConfig::separationDelaySource()
{
    return DoubleSource::of<AxialStage>({
        .get =
            [](const AxialStage& stage) { return selectedSeparation(stage).getSeparationDelay(); },
        .set =
            [](AxialStage& stage, const double& delay) {
                changeSeparation(stage, [delay](StageSeparationConfiguration& configuration) {
                    configuration.setSeparationDelay(delay);
                });
            },
    });
}

AxialStageConfig::AxialStageConfig(OpenRocketDocument& document, const GuiServices& services,
                                   std::shared_ptr<ComponentSelection> selection, QWidget* parent)
  : ComponentAssemblyConfig(document, services, std::move(selection), parent)
{
    const std::shared_ptr<ComponentSelection>& valid     = stageSelection(*this);
    AxialStage&                                main      = stageOf(valid->getMain());
    const bool                                 separates = main.getStageNumber() > 0;

    // Java's first access to the selected separations: AxialStage.addConfigListener() reads the
    // main stage's and each other stage's, the separation tab the main stage's (see the class
    // comment). The copies fire no event.
    if (separates || valid->isMultiEdit())
    {
        static_cast<void>(main.getSeparationConfiguration());
    }
    for (RocketComponent* other : valid->getOthers())
    {
        if (auto* stage = dynamic_cast<AxialStage*>(other))
        {
            static_cast<void>(stage->getSeparationConfiguration());
        }
    }

    if (separates)
    {
        addSeparationTab();
        setSelectedTabIndex(0);
    }
    addRecoveryTab();
}

void AxialStageConfig::addSeparationTab()
{
    const std::shared_ptr<ComponentSelection>& selection = stageSelection(*this);
    BindingScope&                              bindings  = scope();

    m_separation.page = makeChild<QWidget>(this);
    auto* layout      = makeChild<QVBoxLayout>(m_separation.page);

    m_separation.title = makeChild<StyledLabel>(
        m_separation.page,
        //: ComponentAssemblyConfig.separation.lbl.title
        tr("Select when this stage separates:") + QLatin1Char(' ') + CommonStrings::dagger(),
        StyledLabel::Style::BOLD);
    layout->addWidget(m_separation.title);
    layout->addSpacing(30);

    // The event, "plus", the delay (no unit, at least 0) and "seconds" on one row.
    auto* row       = makeChild<QWidget>(m_separation.page);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    m_separation.event = makeChild<EnumComboBox>(
        row,
        bindings.make<EnumBinding<SeparationEvent>>(
            selection, separationEventSource(),
            std::span<const SeparationEvent>(StageSeparationConfiguration::kAllSeparationEvents)));
    m_separation.event->setObjectName(QStringLiteral("SeparationEvent"));
    rowLayout->addWidget(m_separation.event);
    //: ComponentAssemblyConfig.separation.lbl.plus
    rowLayout->addWidget(makeChild<QLabel>(row, tr("plus")));
    m_separation.delay = makeChild<UnitSpinBox>(
        row, bindings.make<DoubleBinding>(selection, separationDelaySource(),
                                          unitGroup(UnitGroupId::NONE),
                                          DoubleBinding::Options{.min = 0.0}));
    m_separation.delay->setObjectName(QStringLiteral("SeparationDelay"));
    rowLayout->addWidget(m_separation.delay);
    //: ComponentAssemblyConfig.separation.lbl.seconds
    rowLayout->addWidget(makeChild<QLabel>(row, tr("seconds")));
    rowLayout->addStretch(1);
    layout->addWidget(row);

    m_separation.note =
        makeChild<StyledLabel>(m_separation.page, CommonStrings::overrideDescription(), -1.0);
    layout->addWidget(m_separation.note);
    layout->addStretch(1);

    insertTab(0, *m_separation.page,
              //: ComponentAssemblyConfig.tab.Separation
              tr("Separation"),
              //: ComponentAssemblyConfig.tab.Separation.ttip
              tr("Stage separation options"));
}

void AxialStageConfig::addRecoveryTab()
{
    RocketComponent& main = stageSelection(*this)->getMain();

    m_recovery.page = makeChild<QWidget>(this);
    auto* layout    = makeChild<QVBoxLayout>(m_recovery.page);
    m_recovery.title =
        makeChild<StyledLabel>(m_recovery.page,
                               //: AxialStageConfig.recovery.lbl.title
                               tr("Recovery deployment type"), StyledLabel::Style::BOLD);
    layout->addWidget(m_recovery.title);

    // The recovery devices of the stage's subtree, in tree order, and the first drogue.
    std::optional<int> drogue;
    QList<QString>     names;
    for (const RocketComponent& component : main.subtree())
    {
        if (const auto* device = dynamic_cast<const RecoveryDevice*>(&component))
        {
            if (device->isDrogue() && !drogue.has_value())
            {
                drogue = static_cast<int>(m_devices.size());
            }
            m_devices.push_back(device->getId());
            names.append(QtStrings::fromUtf8(device->getName()));
        }
    }
    const bool hasDevices = !m_devices.empty();

    m_recovery.single = makeChild<QRadioButton>(m_recovery.page,
                                                //: AxialStageConfig.recovery.radio.None
                                                tr("Single Deployment"));
    m_recovery.single->setObjectName(QStringLiteral("standardRadio"));
    //: AxialStageConfig.recovery.radio.None.ttip
    m_recovery.single->setToolTip(
        tr("<html>Standard recovery deployment using either motor ejection or a single "
           "electronic device.<br>Single-deployment speed warning thresholds will apply.</html>"));
    m_recovery.dual = makeChild<QRadioButton>(m_recovery.page,
                                              //: AxialStageConfig.recovery.radio.Dual
                                              tr("Dual Deployment"));
    m_recovery.dual->setObjectName(QStringLiteral("dualRadio"));
    //: AxialStageConfig.recovery.radio.Dual.ttip
    m_recovery.dual->setToolTip(
        tr("<html>Enable dual recovery for this stage.<br>Select a "
           "recovery device to act as the drogue.</html>"));
    auto* group = makeChild<QButtonGroup>(m_recovery.page);
    group->addButton(m_recovery.single);
    group->addButton(m_recovery.dual);
    layout->addWidget(m_recovery.single);
    layout->addWidget(m_recovery.dual);

    // "Drogue device" and the devices, indented (Java: "gapleft 20lp").
    auto* row       = makeChild<QWidget>(m_recovery.page);
    auto* rowLayout = makeChild<QHBoxLayout>(row);
    rowLayout->setContentsMargins(20, 0, 0, 0);
    //: AxialStageConfig.recovery.lbl.DrogueDevice
    rowLayout->addWidget(makeChild<QLabel>(row, tr("Drogue device")));
    m_recovery.devices = makeChild<QComboBox>(row);
    m_recovery.devices->setObjectName(QStringLiteral("deviceCombo"));
    m_recovery.devices->addItems(names);
    if (!hasDevices)
    {
        //: AxialStageConfig.recovery.combo.NoDevices
        m_recovery.devices->addItem(tr("No recovery devices in this stage"));
    }
    rowLayout->addWidget(m_recovery.devices, 1);
    layout->addWidget(row);
    layout->addStretch(1);

    // The state the tab opens with (Java sets it once).
    if (drogue.has_value())
    {
        m_recovery.dual->setChecked(true);
        m_recovery.devices->setCurrentIndex(*drogue);
        m_recovery.devices->setEnabled(true);
    }
    else
    {
        m_recovery.single->setChecked(true);
        m_recovery.devices->setCurrentIndex(0);
        m_recovery.devices->setEnabled(false);
    }
    m_recovery.dual->setEnabled(hasDevices);

    connect(m_recovery.single, &QRadioButton::clicked, this,
            guarded([this] { chooseSingleDeployment(); }, QStringLiteral("standardRadio")));
    connect(m_recovery.dual, &QRadioButton::clicked, this,
            guarded([this] { chooseDualDeployment(); }, QStringLiteral("dualRadio")));
    connect(
        m_recovery.devices, &QComboBox::activated, this,
        guarded([this](int index) { chooseDrogueDevice(index); }, QStringLiteral("deviceCombo")));

    insertTab(tabs().count(), *m_recovery.page,
              //: AxialStageConfig.tab.Recovery
              tr("Recovery"),
              //: AxialStageConfig.tab.Recovery.ttip
              tr("Stage recovery deployment options"));
}

void AxialStageConfig::chooseSingleDeployment()
{
    setDrogue(std::nullopt);
    m_recovery.devices->setEnabled(false);
}

void AxialStageConfig::chooseDualDeployment()
{
    if (m_devices.empty())
    {
        m_recovery.single->setChecked(true);
        return;
    }
    m_recovery.devices->setEnabled(true);
    const int index = m_recovery.devices->currentIndex();
    setDrogue(index >= 0 ? std::optional<std::size_t>(static_cast<std::size_t>(index))
                         : std::nullopt);
}

void AxialStageConfig::chooseDrogueDevice(int index)
{
    if (m_recovery.dual->isChecked() && !m_devices.empty() && index >= 0)
    {
        setDrogue(static_cast<std::size_t>(index));
    }
}

void AxialStageConfig::setDrogue(std::optional<std::size_t> index)
{
    const std::shared_ptr<ComponentSelection>& selection = this->selection();
    if (selection == nullptr || !selection->isValid())
    {
        return;
    }
    RocketComponent& stage    = selection->getMain();
    Rocket&          rocket   = selection->getRocket();
    const bool       freezing = !rocket.isFrozen();
    if (freezing)
    {
        rocket.freeze();
    }
    try
    {
        // Java's clearStageDrogue(): every drogue of the stage's subtree.
        for (RocketComponent& component : stage.subtree())
        {
            if (auto* device = dynamic_cast<RecoveryDevice*>(&component);
                device != nullptr && device->isDrogue())
            {
                device->setDrogue(false);
            }
        }
        if (index.has_value() && *index < m_devices.size())
        {
            if (auto* device =
                    dynamic_cast<RecoveryDevice*>(rocket.findComponent(m_devices.at(*index))))
            {
                device->setDrogue(true);
            }
        }
    }
    catch (...)
    {
        if (freezing)
        {
            rocket.thaw();
        }
        throw;
    }
    if (freezing)
    {
        rocket.thaw();
    }
}

}  // namespace QtRocket::Gui
