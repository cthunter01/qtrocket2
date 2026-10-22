#include "QtRocket/gui/configdialog/AxialStageConfig.h"

#include <algorithm>
#include <array>
#include <memory>
#include <span>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "AssemblyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ParachuteConfig.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
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

using AxialStageConfigTest = Test::ConfigDialogTest;
using Test::comboItemTexts;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::RecoveryDesign;
using Test::RocketEventRecorder;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;
using SeparationEvent = StageSeparationConfiguration::SeparationEvent;

/// The design of D2cProbe's section "stages": a new rocket whose stage holds a body tube with a
/// booster set, and a second stage with a body tube; the undo history starts with the whole design.
/// Stage numbers: the sustainer 0, the booster set 1, the second stage 2.
struct StageDesign
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    AxialStage*                         sustainer{nullptr};
    ParallelStage*                      boosters{nullptr};
    AxialStage*                         second{nullptr};

    StageDesign()
    {
        sustainer  = dynamic_cast<AxialStage*>(document->getRocket().getChildren().front());
        auto& body = sustainer->addChild(std::make_unique<BodyTube>());
        second     = &document->getRocket().addChild(std::make_unique<AxialStage>());
        second->addChild(std::make_unique<BodyTube>());
        boosters = &body.addChild(std::make_unique<ParallelStage>());
        document->clearUndo();
    }
};

/// The panel of @p created as an AxialStageConfig.
[[nodiscard]] AxialStageConfig& stagePanel(const FactoryPanel& created)
{
    auto* panel = created.as<AxialStageConfig>();
    if (panel == nullptr)
    {
        bug("not a stage's panel");
    }
    return *panel;
}

/// The panel of @p dialog as an AxialStageConfig.
[[nodiscard]] AxialStageConfig& stagePanel(const ComponentConfigDialog& dialog)
{
    auto* panel = dynamic_cast<AxialStageConfig*>(&dialog.configurator());
    if (panel == nullptr)
    {
        bug("not a stage's panel");
    }
    return *panel;
}

/// The separation of @p stage in the rocket's selected configuration.
[[nodiscard]] const StageSeparationConfiguration& selectedSeparation(const AxialStage& stage)
{
    return stage.getSeparationConfigurations().get(
        stage.getRocket().getSelectedConfiguration().getId());
}

/// Chooses @p event in the separation combo box of @p panel as a user does.
void chooseEvent(const AxialStageConfig& panel, SeparationEvent event)
{
    const std::shared_ptr<EnumBinding<SeparationEvent>> binding =
        panel.separationWidgets().event->enumBinding<SeparationEvent>();
    Q_EMIT panel.separationWidgets().event->activated(binding->indexOf(event));
}

/// Whether @p types holds an EVENT change.
[[nodiscard]] bool hasEventChange(const std::vector<int>& types)
{
    return std::ranges::any_of(
        types, [](int type) { return (type & ComponentChangeEvent::kEventChange) != 0; });
}

/// The binding of the separation delay of @p panel.
[[nodiscard]] DoubleBinding& delayOf(const AxialStageConfig& panel)
{
    const std::shared_ptr<DoubleBinding>& binding = panel.separationWidgets().delay->binding();
    return *binding;
}

/// The drogue flags of the devices of @p design, in tree order.
[[nodiscard]] std::vector<bool> drogues(const RecoveryDesign& design)
{
    return {design.main->isDrogue(), design.boosterChute->isDrogue(), design.streamer->isDrogue()};
}

TEST_F(AxialStageConfigTest, TheTopStageHasOverrideCommentAndRecovery)
{
    // D2cGuiProbe: the sustainer's tabbed pane.
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.sustainer);
    const AxialStageConfig&    panel = stagePanel(created);
    EXPECT_EQ(tabTitles(panel), (QStringList{QStringLiteral("Override"), QStringLiteral("Comment"),
                                             QStringLiteral("Recovery")}));
    EXPECT_EQ(panel.tabs().tabToolTip(2), QStringLiteral("Stage recovery deployment options"));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    EXPECT_EQ(panel.separationWidgets().page, nullptr);
    EXPECT_EQ(panel.tabs().widget(2), panel.recoveryWidgets().page);
}

TEST_F(AxialStageConfigTest, ALowerStageOpensOnItsSeparation)
{
    // D2cGuiProbe: the second stage's tabbed pane and separation tab.
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.second);
    const AxialStageConfig&    panel = stagePanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("Separation"), QStringLiteral("Override"),
                           QStringLiteral("Comment"), QStringLiteral("Recovery")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("Stage separation options"));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    const AxialStageConfig::SeparationWidgets& widgets = panel.separationWidgets();
    ASSERT_NE(widgets.page, nullptr);
    EXPECT_EQ(widgets.title->text(), QStringLiteral("Select when this stage separates: \u2020"));
    EXPECT_TRUE(widgets.title->font().bold());
    EXPECT_EQ(labelTextsUnder(*widgets.page),
              (QStringList{QStringLiteral("Select when this stage separates: \u2020"),
                           QStringLiteral("plus"), QStringLiteral("seconds"),
                           QStringLiteral("\u2020 This parameter can be overridden in each "
                                          "flight configuration.")}));
    EXPECT_EQ(
        comboItemTexts(*widgets.event),
        (QStringList{QStringLiteral("Launch"), QStringLiteral("Current stage motor ignition"),
                     QStringLiteral("Current stage motor burnout"),
                     QStringLiteral("Current stage ejection charge"),
                     QStringLiteral("Upper stage motor ignition"),
                     QStringLiteral("Specific altitude during ascent"), QStringLiteral("Apogee"),
                     QStringLiteral("Specific altitude during descent"), QStringLiteral("Never")}));
    EXPECT_EQ(widgets.event->shownText(), QStringLiteral("Current stage ejection charge"));
    EXPECT_EQ(widgets.event->objectName(), QStringLiteral("SeparationEvent"));
    EXPECT_EQ(widgets.delay->objectName(), QStringLiteral("SeparationDelay"));
    EXPECT_EQ(delayOf(panel).value(), 0.0);
    EXPECT_EQ(&delayOf(panel).getUnitGroup(), &unitGroup(UnitGroupId::NONE));
}

TEST_F(AxialStageConfigTest, ABoosterSetHasItsSeparationBeforeItsGeneralTab)
{
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.boosters);
    const AxialStageConfig&    panel = stagePanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("Separation"), QStringLiteral("General"),
                           QStringLiteral("Override"), QStringLiteral("Comment"),
                           QStringLiteral("Recovery")}));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    ASSERT_NE(panel.splitButton(), nullptr);
    EXPECT_EQ(panel.splitButton()->text(), QStringLiteral("Split Boosters"));
}

TEST_F(AxialStageConfigTest, AnEditOfTheSeparationFiresAnEvent)
{
    // The user's decision U4: Java's StageSeparationConfiguration notifies nobody (D2cProbe: the
    // rocket's modification id stays, no event); here the stage fires EVENT_CHANGE.
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.second);
    const AxialStageConfig&    panel = stagePanel(created);
    RocketEventRecorder        events(design.document->getRocket());
    chooseEvent(panel, SeparationEvent::BURNOUT);
    EXPECT_EQ(selectedSeparation(*design.second).getSeparationEvent(), SeparationEvent::BURNOUT);
    EXPECT_TRUE(hasEventChange(events.types()));
    events.clear();
    // The value shown already: nothing set, nothing fired.
    chooseEvent(panel, SeparationEvent::BURNOUT);
    EXPECT_EQ(events.count(), 0U);

    // D2cProbe: the delay is at least 0.
    delayOf(panel).setValue(-1.0);
    EXPECT_EQ(selectedSeparation(*design.second).getSeparationDelay(), 0.0);
    EXPECT_EQ(events.count(), 0U);
    ASSERT_TRUE(delayOf(panel).commitText("2.5"));
    EXPECT_EQ(selectedSeparation(*design.second).getSeparationDelay(), 2.5);
    EXPECT_TRUE(hasEventChange(events.types()));
}

TEST_F(AxialStageConfigTest, AnotherSelectedConfigurationGetsItsOwnSeparation)
{
    // D2cProbe: the first access copies the default for the selected configuration, without an
    // event; the edit then reaches the copy, and the default stays.
    const ConfigDialogServices  services;
    const StageDesign           design;
    Rocket&                     rocket = design.document->getRocket();
    const FlightConfigurationId id     = rocket.createFlightConfiguration().getId();
    rocket.setSelectedConfiguration(id);
    const auto& separations = design.second->getSeparationConfigurations();
    ASSERT_EQ(&separations.get(id), &separations.getDefault());
    RocketEventRecorder events(rocket);
    const FactoryPanel  created(*design.document, services.services(), *design.second);
    EXPECT_NE(&separations.get(id), &separations.getDefault());
    EXPECT_EQ(events.count(), 0U);
    chooseEvent(stagePanel(created), SeparationEvent::APOGEE);
    EXPECT_EQ(separations.get(id).getSeparationEvent(), SeparationEvent::APOGEE);
    EXPECT_EQ(separations.getDefault().getSeparationEvent(), SeparationEvent::EJECTION);
}

TEST_F(AxialStageConfigTest, TheTopStageKeepsTheDefaultUnlessStagesAreEditedTogether)
{
    // Java reads the selected separation for the separation tab (stage number above 0) and in
    // AxialStage.addConfigListener() (every stage of a multi-component edit).
    const ConfigDialogServices  services;
    const StageDesign           design;
    Rocket&                     rocket = design.document->getRocket();
    const FlightConfigurationId id     = rocket.createFlightConfiguration().getId();
    rocket.setSelectedConfiguration(id);
    const auto& top = design.sustainer->getSeparationConfigurations();
    {
        const FactoryPanel created(*design.document, services.services(), *design.sustainer);
        EXPECT_EQ(&top.get(id), &top.getDefault());
    }
    const std::array<RocketComponent*, 1> others{design.second};
    const FactoryPanel created(*design.document, services.services(), *design.sustainer, others);
    EXPECT_NE(&top.get(id), &top.getDefault());
    const auto& lower = design.second->getSeparationConfigurations();
    EXPECT_NE(&lower.get(id), &lower.getDefault());
}

TEST_F(AxialStageConfigTest, TheSeparationOfEverySelectedStageChanges)
{
    const ConfigDialogServices services;
    const StageDesign          design;
    auto& third = design.document->getRocket().addChild(std::make_unique<AxialStage>());
    third.addChild(std::make_unique<BodyTube>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&third};
    const FactoryPanel      created(*design.document, services.services(), *design.second, others);
    const AxialStageConfig& panel = stagePanel(created);
    chooseEvent(panel, SeparationEvent::NEVER);
    delayOf(panel).setValue(1.5);
    for (const AxialStage* stage :
         {static_cast<const AxialStage*>(design.second), static_cast<const AxialStage*>(&third)})
    {
        EXPECT_EQ(selectedSeparation(*stage).getSeparationEvent(), SeparationEvent::NEVER);
        EXPECT_EQ(selectedSeparation(*stage).getSeparationDelay(), 1.5);
    }
}

TEST_F(AxialStageConfigTest, CancelUndoesTheSeparation)
{
    // The user's decision U4: the edits fire events, so the dialog is modified and Cancel undoes
    // them with the session's undo step (Java's Cancel leaves them).
    const ConfigDialogServices services;
    const StageDesign          design;
    QWidget                    window;
    services.services().preferences().setShowDiscardConfirmation(false);
    const Uuid                   id = design.second->getId();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.second);
    ASSERT_NE(dialog, nullptr);
    const AxialStageConfig& panel = stagePanel(*dialog);
    chooseEvent(panel, SeparationEvent::LAUNCH);
    typeInto(*panel.separationWidgets().delay, QStringLiteral("3"));
    EXPECT_TRUE(dialog->configurator().commitAll());
    EXPECT_TRUE(dialog->isModified());
    EXPECT_EQ(selectedSeparation(*design.second).getSeparationEvent(), SeparationEvent::LAUNCH);
    EXPECT_EQ(selectedSeparation(*design.second).getSeparationDelay(), 3.0);

    dialog->configurator().cancelButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const AxialStage*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(selectedSeparation(*restored).getSeparationEvent(), SeparationEvent::EJECTION);
    EXPECT_EQ(selectedSeparation(*restored).getSeparationDelay(), 0.0);
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(AxialStageConfigTest, TheRecoveryTabIsJavas)
{
    // D2cGuiProbe: the texts and tooltips of the second stage's Recovery tab, without devices.
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.second);
    const AxialStageConfig::RecoveryWidgets& widgets = stagePanel(created).recoveryWidgets();
    EXPECT_EQ(
        labelTextsUnder(*widgets.page),
        (QStringList{QStringLiteral("Recovery deployment type"), QStringLiteral("Drogue device")}));
    EXPECT_TRUE(widgets.title->font().bold());
    EXPECT_EQ(widgets.single->text(), QStringLiteral("Single Deployment"));
    EXPECT_EQ(widgets.single->toolTip(),
              QStringLiteral("<html>Standard recovery deployment using either motor ejection or a "
                             "single electronic device.<br>Single-deployment speed warning "
                             "thresholds will apply.</html>"));
    EXPECT_EQ(widgets.dual->text(), QStringLiteral("Dual Deployment"));
    EXPECT_EQ(widgets.dual->toolTip(),
              QStringLiteral("<html>Enable dual recovery for this stage.<br>Select a recovery "
                             "device to act as the drogue.</html>"));
    // No device: Single, Dual and the combo box disabled, its one item the note.
    EXPECT_TRUE(widgets.single->isChecked());
    EXPECT_FALSE(widgets.dual->isEnabled());
    EXPECT_FALSE(widgets.devices->isEnabled());
    EXPECT_EQ(comboItemTexts(*widgets.devices),
              (QStringList{QStringLiteral("No recovery devices in this stage")}));
}

TEST_F(AxialStageConfigTest, TheDevicesAreTheStagesSubtreeInTreeOrder)
{
    // D2cProbe: Java iterates the stage, booster sets included.
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    const FactoryPanel         sustainer(*design.document, services.services(), *design.stage);
    const AxialStageConfig::RecoveryWidgets& widgets = stagePanel(sustainer).recoveryWidgets();
    EXPECT_EQ(comboItemTexts(*widgets.devices),
              (QStringList{QStringLiteral("Main"), QStringLiteral("Booster chute"),
                           QStringLiteral("Streamer")}));
    EXPECT_TRUE(widgets.single->isChecked());
    EXPECT_TRUE(widgets.dual->isEnabled());
    EXPECT_FALSE(widgets.devices->isEnabled());
    EXPECT_EQ(widgets.devices->currentIndex(), 0);

    const FactoryPanel boosters(*design.document, services.services(), *design.boosters);
    EXPECT_EQ(comboItemTexts(*stagePanel(boosters).recoveryWidgets().devices),
              (QStringList{QStringLiteral("Booster chute")}));
}

TEST_F(AxialStageConfigTest, ADrogueOpensTheTabOnDualDeployment)
{
    // D2cGuiProbe: Dual selected, the drogue shown, the combo box enabled.
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    design.streamer->setDrogue(true);
    design.document->clearUndo();
    const FactoryPanel created(*design.document, services.services(), *design.stage);
    const AxialStageConfig::RecoveryWidgets& widgets = stagePanel(created).recoveryWidgets();
    EXPECT_TRUE(widgets.dual->isChecked());
    EXPECT_TRUE(widgets.devices->isEnabled());
    EXPECT_EQ(widgets.devices->currentText(), QStringLiteral("Streamer"));
}

TEST_F(AxialStageConfigTest, DualMakesTheShownDeviceTheOnlyDrogue)
{
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    design.streamer->setDrogue(true);
    design.boosterChute->setDrogue(true);
    design.document->clearUndo();
    const FactoryPanel created(*design.document, services.services(), *design.stage);
    const AxialStageConfig::RecoveryWidgets& widgets = stagePanel(created).recoveryWidgets();
    // The first drogue in tree order is shown.
    EXPECT_EQ(widgets.devices->currentText(), QStringLiteral("Booster chute"));
    RocketEventRecorder events(design.document->getRocket());

    widgets.single->click();
    EXPECT_EQ(drogues(design), (std::vector<bool>{false, false, false}));
    EXPECT_FALSE(widgets.devices->isEnabled());
    // One merged event (Java: one per setDrogue()).
    EXPECT_EQ(events.count(), 1U);

    widgets.dual->click();
    EXPECT_EQ(drogues(design), (std::vector<bool>{false, true, false}));
    EXPECT_TRUE(widgets.devices->isEnabled());

    widgets.devices->setCurrentIndex(0);
    Q_EMIT widgets.devices->activated(0);
    EXPECT_EQ(drogues(design), (std::vector<bool>{true, false, false}));
    events.clear();
    // The device shown already: cleared and set again, as Java does (one merged event here).
    Q_EMIT widgets.devices->activated(0);
    EXPECT_EQ(drogues(design), (std::vector<bool>{true, false, false}));
    EXPECT_EQ(events.count(), 1U);
}

TEST_F(AxialStageConfigTest, ADeviceChosenWhileSingleChangesNothing)
{
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.stage);
    const AxialStageConfig::RecoveryWidgets& widgets = stagePanel(created).recoveryWidgets();
    widgets.devices->setCurrentIndex(2);
    Q_EMIT widgets.devices->activated(2);
    EXPECT_EQ(drogues(design), (std::vector<bool>{false, false, false}));
    // Dual then takes the device shown.
    widgets.dual->click();
    EXPECT_EQ(drogues(design), (std::vector<bool>{false, false, true}));
}

TEST_F(AxialStageConfigTest, WithoutDevicesDualSelectsSingleAgain)
{
    const ConfigDialogServices services;
    const StageDesign          design;
    const FactoryPanel         created(*design.document, services.services(), *design.second);
    AxialStageConfig&          panel = stagePanel(created);
    panel.recoveryWidgets().dual->setChecked(true);
    panel.chooseDualDeployment();
    EXPECT_TRUE(panel.recoveryWidgets().single->isChecked());
    EXPECT_FALSE(panel.recoveryWidgets().devices->isEnabled());
}

TEST_F(AxialStageConfigTest, TheDrogueBoxOfTheDeviceShowsWhatTheStageSet)
{
    // The stage's Recovery tab sets the flag, the device's own panel shows it read-only.
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    {
        const FactoryPanel stage(*design.document, services.services(), *design.stage);
        stagePanel(stage).recoveryWidgets().dual->click();
    }
    const FactoryPanel     device(*design.document, services.services(), *design.main);
    const ParachuteConfig* panel = device.as<ParachuteConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(panel->deploymentWidgets().drogue->isChecked());
    EXPECT_FALSE(panel->deploymentWidgets().drogue->isEnabled());
}

TEST_F(AxialStageConfigTest, CancelUndoesTheDrogue)
{
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    QWidget                    window;
    services.services().preferences().setShowDiscardConfirmation(false);
    const Uuid                   id = design.main->getId();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.stage);
    ASSERT_NE(dialog, nullptr);
    stagePanel(*dialog).recoveryWidgets().dual->click();
    EXPECT_TRUE(design.main->isDrogue());
    EXPECT_TRUE(dialog->isModified());
    dialog->configurator().cancelButton().click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const Parachute*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_FALSE(restored->isDrogue());
}

TEST_F(AxialStageConfigTest, ATreeChangeEndsTheRecoveryTabQuietly)
{
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.stage);
    const AxialStageConfig&    panel = stagePanel(created);
    design.body->addChild(std::make_unique<MassComponent>());
    ASSERT_FALSE(panel.selection()->isValid());
    panel.recoveryWidgets().dual->click();
    EXPECT_EQ(drogues(design), (std::vector<bool>{false, false, false}));
}

TEST_F(AxialStageConfigTest, AnUndoEndsTheRecoveryTabQuietly)
{
    // An undo replaces every component: the tab, which keeps the devices' ids, sets nothing.
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.stage);
    const AxialStageConfig&    panel = stagePanel(created);
    const Uuid                 id    = design.main->getId();
    design.main->setName("Drogue");
    design.document->undo();
    ASSERT_FALSE(panel.selection()->isValid());
    panel.recoveryWidgets().dual->click();
    const auto* main =
        dynamic_cast<const Parachute*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(main, nullptr);
    EXPECT_FALSE(main->isDrogue());
}

TEST_F(AxialStageConfigTest, AComponentThatIsNoStageIsABug)
{
    const ConfigDialogServices services;
    const RecoveryDesign       design;
    QWidget                    host;
    EXPECT_THROW(static_cast<void>(makeChild<AxialStageConfig>(
                     &host, *design.document, services.services(),
                     std::make_shared<ComponentSelection>(*design.body))),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
