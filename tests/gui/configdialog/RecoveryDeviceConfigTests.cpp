#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include <QChar>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/ParachuteConfig.h"
#include "QtRocket/gui/configdialog/StreamerConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "RingPanelTestSupport.h"
#include "binding/BindingTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RecoveryDeviceConfigTest = Test::ConfigDialogTest;
using Test::addBoosterStage;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::RocketEventRecorder;
using Test::showActiveDialog;
using Test::typeInto;
using DeployEvent = DeploymentConfiguration::DeployEvent;

/// The panel of @p created as a RecoveryDeviceConfig.
[[nodiscard]] RecoveryDeviceConfig& devicePanel(const FactoryPanel& created)
{
    auto* panel = created.as<RecoveryDeviceConfig>();
    if (panel == nullptr)
    {
        bug("not a recovery device's panel");
    }
    return *panel;
}

/// The events the deploy combo box of @p panel offers, in order.
[[nodiscard]] std::vector<DeployEvent> offeredEvents(const RecoveryDeviceConfig& panel)
{
    const std::shared_ptr<EnumBinding<DeployEvent>> binding =
        panel.deploymentWidgets().event->enumBinding<DeployEvent>();
    if (binding == nullptr)
    {
        bug("no deploy event binding");
    }
    const std::span<const DeployEvent> shown = binding->displayedValues();
    return {shown.begin(), shown.end()};
}

/// Every event but LOWER_STAGE_SEPARATION, in order (D2bProbe: a device of the bottom stage).
[[nodiscard]] std::vector<DeployEvent> bottomStageEvents()
{
    return {DeployEvent::LAUNCH, DeployEvent::EJECTION, DeployEvent::APOGEE, DeployEvent::ALTITUDE,
            DeployEvent::NEVER};
}

/// Every event, in order (D2bProbe: a device of a stage above another).
[[nodiscard]] std::vector<DeployEvent> everyEvent()
{
    const auto& all = DeploymentConfiguration::kAllDeployEvents;
    return {all.begin(), all.end()};
}

/// Whether @p types holds an EVENT change.
[[nodiscard]] bool hasEventChange(const std::vector<int>& types)
{
    return std::ranges::any_of(
        types, [](int type) { return (type & ComponentChangeEvent::kEventChange) != 0; });
}

/// The default deployment configuration of @p device.
[[nodiscard]] const DeploymentConfiguration& deploymentOf(const RecoveryDevice& device)
{
    return device.getDeploymentConfigurations().getDefault();
}

/// Chooses @p event in the deploy combo box of @p panel as a user does.
void chooseEvent(const RecoveryDeviceConfig& panel, DeployEvent event)
{
    const std::shared_ptr<EnumBinding<DeployEvent>> binding =
        panel.deploymentWidgets().event->enumBinding<DeployEvent>();
    Q_EMIT panel.deploymentWidgets().event->activated(binding->indexOf(event));
}

TEST_F(RecoveryDeviceConfigTest, ShowsJavasDeploymentGroup)
{
    const ConfigDialogServices  services;
    const MassDesign            design;
    const FactoryPanel          created(*design.document, services.services(), *design.parachute);
    const RecoveryDeviceConfig& panel                      = devicePanel(created);
    const RecoveryDeviceConfig::DeploymentWidgets& widgets = panel.deploymentWidgets();
    ASSERT_NE(widgets.group, nullptr);
    EXPECT_EQ(widgets.group->title(), QStringLiteral("Deployment"));
    const QString dagger(QChar(0x2020));
    EXPECT_EQ(labelTextsUnder(*widgets.group),
              (QStringList{QStringLiteral("Deploys at: ") + dagger, QStringLiteral("plus"),
                           QStringLiteral("seconds"), QStringLiteral("Altitude:") + dagger,
                           dagger + QStringLiteral(" This parameter can be overridden in each "
                                                   "flight configuration."),
                           QStringLiteral("Set from the stage Recovery tab.")}));
    EXPECT_EQ(widgets.event->objectName(), QStringLiteral("DeployEvent"));
    EXPECT_EQ(widgets.event->shownText(), QStringLiteral("First ejection charge of this stage"));
    EXPECT_EQ(widgets.delay->objectName(), QStringLiteral("DeployDelay"));
    EXPECT_EQ(widgets.altitude.spinBox->objectName(), QStringLiteral("DeployAltitude"));
    EXPECT_EQ(widgets.drogue->text(), QStringLiteral("This is a drogue"));
    EXPECT_EQ(widgets.drogue->objectName(), QStringLiteral("Drogue"));
    EXPECT_EQ(widgets.drogue->toolTip(),
              QStringLiteral("<html>The drogue designation is managed from the stage Recovery "
                             "configuration tab."));
    EXPECT_EQ(widgets.drogueNote->font().italic(), true);
}

TEST_F(RecoveryDeviceConfigTest, TheDelayAndTheAltitudeAreAtLeastZero)
{
    // D2bProbe: DoubleModel(config, "DeployDelay", 0) has no unit; both clamp a negative value to
    // 0; the defaults are 0 s and 200 m.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const RecoveryDeviceConfig::DeploymentWidgets& widgets =
        devicePanel(created).deploymentWidgets();
    const std::shared_ptr<DoubleBinding>& delay    = widgets.delay->binding();
    const std::shared_ptr<DoubleBinding>& altitude = widgets.altitude.spinBox->binding();
    EXPECT_EQ(&delay->getUnitGroup(), &unitGroup(UnitGroupId::NONE));
    EXPECT_EQ(delay->value(), 0.0);
    EXPECT_EQ(&altitude->getUnitGroup(), &unitGroup(UnitGroupId::DISTANCE));
    EXPECT_EQ(altitude->value(), 200.0);
    EXPECT_TRUE(widgets.altitude.slider->mapping().isLinear());
    EXPECT_EQ(widgets.altitude.slider->mapping().getMin(), 100.0);
    EXPECT_EQ(widgets.altitude.slider->mapping().getMax(), 1000.0);
    delay->setValue(-2);
    altitude->setValue(-5);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployDelay(), 0.0);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployAltitude(), 0.0);
    delay->setValue(1.5);
    altitude->setValue(300);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployDelay(), 1.5);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployAltitude(), 300.0);
}

TEST_F(RecoveryDeviceConfigTest, TheAltitudeIsEnabledOnlyForTheAltitudeEvent)
{
    // Java: RecoveryDeviceConfig.updateFields(), called by the combo box's item listener.
    const ConfigDialogServices  services;
    const MassDesign            design;
    const FactoryPanel          created(*design.document, services.services(), *design.streamer);
    const RecoveryDeviceConfig& panel                      = devicePanel(created);
    const RecoveryDeviceConfig::DeploymentWidgets& widgets = panel.deploymentWidgets();
    EXPECT_FALSE(panel.isAltitudeEnabled());
    EXPECT_FALSE(widgets.altitude.label->isEnabled());
    EXPECT_FALSE(widgets.altitude.spinBox->isEnabled());
    EXPECT_FALSE(widgets.altitude.unitSelector->isEnabled());
    EXPECT_FALSE(widgets.altitude.slider->isEnabled());
    EXPECT_TRUE(widgets.delay->isEnabled());

    chooseEvent(panel, DeployEvent::ALTITUDE);
    EXPECT_EQ(deploymentOf(*design.streamer).getDeployEvent(), DeployEvent::ALTITUDE);
    EXPECT_TRUE(panel.isAltitudeEnabled());
    EXPECT_TRUE(widgets.altitude.label->isEnabled());
    EXPECT_TRUE(widgets.altitude.spinBox->isEnabled());
    EXPECT_TRUE(widgets.altitude.unitSelector->isEnabled());
    EXPECT_TRUE(widgets.altitude.slider->isEnabled());

    chooseEvent(panel, DeployEvent::APOGEE);
    EXPECT_FALSE(widgets.altitude.spinBox->isEnabled());
}

TEST_F(RecoveryDeviceConfigTest, TheBottomStageHasNoLowerStageSeparation)
{
    // D2bProbe: (stageNumber + 1) == stageCount removes LOWER_STAGE_SEPARATION.
    const ConfigDialogServices services;
    const MassDesign           design;
    EXPECT_TRUE(RecoveryDeviceConfig::isInBottomStage(*design.parachute));
    const FactoryPanel only(*design.document, services.services(), *design.parachute);
    EXPECT_EQ(offeredEvents(devicePanel(only)), bottomStageEvents());
}

TEST_F(RecoveryDeviceConfigTest, AStageAboveAnotherOffersEveryEvent)
{
    // D2bProbe: a parachute in the upper of two stages, a streamer in the bottom one; then a
    // booster set in the upper stage, which is stage 1 of 3 and offers every event.
    const ConfigDialogServices services;
    const MassDesign           design;
    BodyTube&                  boosterTube     = addBoosterStage(*design.document);
    auto&                      boosterStreamer = boosterTube.addChild(std::make_unique<Streamer>());
    design.document->clearUndo();
    EXPECT_FALSE(RecoveryDeviceConfig::isInBottomStage(*design.parachute));
    EXPECT_TRUE(RecoveryDeviceConfig::isInBottomStage(boosterStreamer));
    {
        const FactoryPanel upper(*design.document, services.services(), *design.parachute);
        EXPECT_EQ(offeredEvents(devicePanel(upper)), everyEvent());
        const FactoryPanel bottom(*design.document, services.services(), boosterStreamer);
        EXPECT_EQ(offeredEvents(devicePanel(bottom)), bottomStageEvents());
    }

    auto& boosters = design.body->addChild(std::make_unique<ParallelStage>());
    auto& podBody  = boosters.addChild(std::make_unique<BodyTube>());
    auto& podChute = podBody.addChild(std::make_unique<Parachute>());
    design.document->clearUndo();
    EXPECT_EQ(design.document->getRocket().getStageCount(), 3U);
    EXPECT_EQ(podChute.getStageNumber(), 1);
    EXPECT_EQ(design.parachute->getStageNumber(), 0);
    EXPECT_EQ(boosterStreamer.getStageNumber(), 2);
    const FactoryPanel pod(*design.document, services.services(), podChute);
    EXPECT_EQ(offeredEvents(devicePanel(pod)), everyEvent());
    const FactoryPanel bottom(*design.document, services.services(), boosterStreamer);
    EXPECT_EQ(offeredEvents(devicePanel(bottom)), bottomStageEvents());
}

TEST_F(RecoveryDeviceConfigTest, AHiddenEventIsShownApart)
{
    // A loaded design may deploy a device of the bottom stage at a lower stage's separation.
    const ConfigDialogServices services;
    const MassDesign           design;
    design.parachute->getDeploymentConfigurations().getDefault().setDeployEvent(
        DeployEvent::LOWER_STAGE_SEPARATION);
    const FactoryPanel created(*design.document, services.services(), *design.parachute);
    const RecoveryDeviceConfig::DeploymentWidgets& widgets =
        devicePanel(created).deploymentWidgets();
    EXPECT_EQ(widgets.event->currentIndex(), -1);
    EXPECT_EQ(widgets.event->shownText(), QStringLiteral("Lower stage separation"));
}

TEST_F(RecoveryDeviceConfigTest, AnEditOfTheDefaultDeploymentFiresAnEvent)
{
    // The user's decision U4: Java's DeploymentConfiguration setters notify nobody (D2bProbe: the
    // rocket's modification id stays); here the device fires EVENT_CHANGE when it changed.
    const ConfigDialogServices  services;
    const MassDesign            design;
    const FactoryPanel          created(*design.document, services.services(), *design.parachute);
    const RecoveryDeviceConfig& panel = devicePanel(created);
    RocketEventRecorder         events(design.document->getRocket());
    chooseEvent(panel, DeployEvent::APOGEE);
    EXPECT_TRUE(hasEventChange(events.types()));
    // The value shown already: nothing set, nothing fired.
    events.clear();
    chooseEvent(panel, DeployEvent::APOGEE);
    EXPECT_EQ(events.count(), 0U);

    ASSERT_TRUE(panel.deploymentWidgets().delay->binding()->commitText("2"));
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployDelay(), 2.0);
    EXPECT_TRUE(hasEventChange(events.types()));
    events.clear();
    panel.deploymentWidgets().delay->binding()->setValue(2.0);
    EXPECT_FALSE(hasEventChange(events.types()));
    panel.deploymentWidgets().altitude.spinBox->binding()->setValue(250);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployAltitude(), 250.0);
    EXPECT_TRUE(hasEventChange(events.types()));
}

TEST_F(RecoveryDeviceConfigTest, TheDeploymentOfEverySelectedDeviceChanges)
{
    // D2bProbe: Java links the default configurations of the selected devices (both APOGEE).
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<Parachute>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel created(*design.document, services.services(), *design.parachute, others);
    const RecoveryDeviceConfig& panel = devicePanel(created);
    chooseEvent(panel, DeployEvent::APOGEE);
    panel.deploymentWidgets().delay->binding()->setValue(1.0);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(deploymentOf(other).getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(deploymentOf(other).getDeployDelay(), 1.0);
}

TEST_F(RecoveryDeviceConfigTest, CancelUndoesTheDefaultDeployment)
{
    // The user's decision U4: the edits fire events, so the dialog is modified and Cancel undoes
    // them with the session's undo step (Java's Cancel leaves them).
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    services.services().preferences().setShowDiscardConfirmation(false);
    const Uuid                   id = design.parachute->getId();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.parachute);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const ParachuteConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    chooseEvent(*panel, DeployEvent::ALTITUDE);
    typeInto(*panel->deploymentWidgets().delay, QStringLiteral("3"));
    typeInto(*panel->deploymentWidgets().altitude.spinBox, QStringLiteral("150"));
    EXPECT_TRUE(dialog->configurator().commitAll());
    EXPECT_TRUE(dialog->isModified());
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployEvent(), DeployEvent::ALTITUDE);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployDelay(), 3.0);
    EXPECT_EQ(deploymentOf(*design.parachute).getDeployAltitude(), 150.0);

    dialog->configurator().cancelButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const Parachute*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(deploymentOf(*restored).getDeployEvent(), DeployEvent::EJECTION);
    EXPECT_EQ(deploymentOf(*restored).getDeployDelay(), 0.0);
    EXPECT_EQ(deploymentOf(*restored).getDeployAltitude(), 200.0);
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(RecoveryDeviceConfigTest, ATreeChangeEndsTheAltitudeQuietly)
{
    // The TREE change ends the selection: the event's binding tells its end, and the altitude's
    // widgets go disabled without reading a component.
    const ConfigDialogServices  services;
    const MassDesign            design;
    const FactoryPanel          created(*design.document, services.services(), *design.parachute);
    const RecoveryDeviceConfig& panel = devicePanel(created);
    chooseEvent(panel, DeployEvent::ALTITUDE);
    ASSERT_TRUE(panel.isAltitudeEnabled());
    design.body->addChild(std::make_unique<MassComponent>());
    EXPECT_FALSE(panel.isAltitudeEnabled());
    EXPECT_FALSE(panel.deploymentWidgets().altitude.label->isEnabled());
    EXPECT_FALSE(panel.deploymentWidgets().altitude.slider->isEnabled());
}

TEST_F(RecoveryDeviceConfigTest, TheDrogueBoxShowsTheDeviceAndCannotBeChanged)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    design.streamer->setDrogue(true);
    const FactoryPanel streamer(*design.document, services.services(), *design.streamer);
    const QCheckBox&   drogue = *devicePanel(streamer).deploymentWidgets().drogue;
    EXPECT_TRUE(drogue.isChecked());
    EXPECT_FALSE(drogue.isEnabled());
    const FactoryPanel parachute(*design.document, services.services(), *design.parachute);
    EXPECT_FALSE(devicePanel(parachute).deploymentWidgets().drogue->isChecked());
    EXPECT_FALSE(devicePanel(parachute).deploymentWidgets().drogue->isEnabled());
}

TEST_F(RecoveryDeviceConfigTest, AComponentThatIsNoRecoveryDeviceIsABug)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    host;
    EXPECT_THROW(static_cast<void>(makeChild<RecoveryDeviceConfig>(
                     &host, *design.document, services.services(),
                     std::make_shared<ComponentSelection>(*design.mass))),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
