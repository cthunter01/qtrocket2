#include "QtRocket/gui/configdialog/StreamerConfig.h"

#include <array>
#include <memory>

#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/configdialog/RecoveryDeviceConfig.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/HtmlLabel.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using StreamerConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::MassDesign;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;

/// The panel of @p created as a StreamerConfig.
[[nodiscard]] StreamerConfig& streamerPanel(const FactoryPanel& created)
{
    auto* panel = created.as<StreamerConfig>();
    if (panel == nullptr)
    {
        bug("not a streamer's panel");
    }
    return *panel;
}

TEST_F(StreamerConfigTest, TheTabsAreJavas)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.streamer);
    const StreamerConfig&      panel = streamerPanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Radial position"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel.tabs().widget(1), &panel.radialPosition());
    EXPECT_TRUE(panel.radialPosition().resetButton().toolTip().isEmpty());
}

TEST_F(StreamerConfigTest, TheStripHasJavasFields)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.streamer);
    const StreamerConfig&      panel = streamerPanel(created);
    EXPECT_EQ(panel.stripLengthWidgets().label->text(), QStringLiteral("Strip length:"));
    EXPECT_EQ(panel.stripLengthWidgets().spinBox->objectName(), QStringLiteral("StripLength"));
    // DoubleModel(component, "StripLength", UNITS_LENGTH, 0), getSliderModel(0, 0.6, 1.5).
    EXPECT_EQ(panel.stripLengthWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel.stripLengthWidgets().slider->mapping().getMin(), 0.0);
    EXPECT_EQ(panel.stripLengthWidgets().slider->mapping().getMid(), 0.6);
    EXPECT_EQ(panel.stripLengthWidgets().slider->mapping().getMax(), 1.5);
    EXPECT_EQ(panel.stripWidthWidgets().label->text(), QStringLiteral("Strip width:"));
    // DoubleModel(component, "StripWidth", UNITS_LENGTH, 0), getSliderModel(0, 0.2).
    EXPECT_EQ(panel.stripWidthWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_TRUE(panel.stripWidthWidgets().slider->mapping().isLinear());
    EXPECT_EQ(panel.stripWidthWidgets().slider->mapping().getMin(), 0.0);
    EXPECT_EQ(panel.stripWidthWidgets().slider->mapping().getMax(), 0.2);
    EXPECT_EQ(panel.areaWidgets().label->text(), QStringLiteral("Strip area:"));
    EXPECT_EQ(panel.areaWidgets().spinBox->objectName(), QStringLiteral("Area"));
    EXPECT_EQ(&panel.areaWidgets().spinBox->binding()->getUnitGroup(),
              &unitGroup(UnitGroupId::AREA));
    // DoubleModel(component, "Area", UNITS_AREA, 0), getSliderModel(0, 0.04, 0.25).
    EXPECT_EQ(panel.areaWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel.areaWidgets().slider->mapping().getMin(), 0.0);
    EXPECT_EQ(panel.areaWidgets().slider->mapping().getMid(), 0.04);
    EXPECT_EQ(panel.areaWidgets().slider->mapping().getMax(), 0.25);
    // Java comments the unit selector out; the slider runs from 2 to 15.
    EXPECT_EQ(panel.aspectRatioWidgets().label->text(), QStringLiteral("Aspect ratio:"));
    EXPECT_EQ(panel.aspectRatioWidgets().unitSelector, nullptr);
    EXPECT_TRUE(panel.aspectRatioWidgets().slider->mapping().isLinear());
    EXPECT_EQ(panel.aspectRatioWidgets().slider->mapping().getMin(), 2.0);
    EXPECT_EQ(panel.aspectRatioWidgets().slider->mapping().getMax(), 15.0);
    EXPECT_EQ(panel.aspectRatioWidgets().spinBox->binding()->minimum(), 0.0);
}

TEST_F(StreamerConfigTest, TheMaterialAndTheDragCoefficientHaveJavasTexts)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.streamer);
    const StreamerConfig&      panel = streamerPanel(created);
    EXPECT_EQ(panel.materialComboBox().binding()->getType(), Material::Type::SURFACE);
    EXPECT_EQ(panel.materialComboBox().binding()->value().getName(), "Ripstop nylon");
    EXPECT_EQ(panel.materialComboBox().toolTip(),
              QStringLiteral("The component material affects the weight of the component."));
    EXPECT_EQ(panel.cdLabel().text(), QStringLiteral("<html>Drag coefficient C<sub>D</sub>:"));
    const QString tip = QStringLiteral(
        "<html>The drag coefficient relative to the total area of the streamer. "
        "<br>A larger drag coefficient yields a slowed descent rate.");
    EXPECT_EQ(panel.cdLabel().toolTip(), tip);
    EXPECT_EQ(panel.cdWidgets().spinBox->toolTip(), tip);
    // DoubleModel(component, "CD", UNITS_COEFFICIENT, 0).
    EXPECT_EQ(panel.cdWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel.cdWidgets().unitSelector, nullptr);
    EXPECT_EQ(panel.cdWidgets().slider, nullptr);
    EXPECT_EQ(panel.cdAutomatic().objectName(), QStringLiteral("CDAutomatic"));
    EXPECT_EQ(panel.cdAutomatic().toolTip(),
              QStringLiteral(
                  "Calculate the streamer drag coefficient (C<sub>D</sub>) using its dimensions."));
    EXPECT_EQ(panel.cdNote().text(),
              QStringLiteral("The drag coefficient is relative to the area of the streamer."));
    EXPECT_TRUE(panel.cdNote().font().italic());
    EXPECT_EQ(panel.sizeWidgets().length.label->text(), QStringLiteral("Packed length:"));
    EXPECT_GE(panel.placement().grid().indexOf(panel.sizeWidgets().diameter.spinBox), 0);
    EXPECT_NE(panel.deploymentWidgets().group, nullptr);
}

TEST_F(StreamerConfigTest, TheStripKeepsItsAreaOrItsAspectRatio)
{
    // D2bProbe: a new streamer is 50 cm by 5 cm, CD automatic; setArea(0.1) keeps the aspect
    // ratio, setAspectRatio(5) keeps the area.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.streamer);
    const StreamerConfig&      panel = streamerPanel(created);
    EXPECT_EQ(panel.stripLengthWidgets().spinBox->binding()->value(), 0.5);
    EXPECT_EQ(panel.stripWidthWidgets().spinBox->binding()->value(), 0.05);
    EXPECT_EQ(panel.areaWidgets().spinBox->binding()->value(), 0.025);
    EXPECT_EQ(panel.aspectRatioWidgets().spinBox->binding()->value(), 10.0);
    EXPECT_EQ(panel.cdWidgets().spinBox->binding()->value(), 0.08937142857142857);
    EXPECT_TRUE(panel.cdAutomatic().isChecked());

    panel.areaWidgets().spinBox->binding()->setValue(0.1);
    EXPECT_EQ(design.streamer->getStripLength(), 1.0);
    EXPECT_EQ(design.streamer->getStripWidth(), 0.1);
    EXPECT_EQ(design.streamer->getCD(), 0.05958095238095239);
    panel.aspectRatioWidgets().spinBox->binding()->setValue(5);
    EXPECT_EQ(design.streamer->getStripLength(), 0.7071067811865475);
    EXPECT_EQ(design.streamer->getStripWidth(), 0.1414213562373095);
    EXPECT_EQ(design.streamer->getArea(), 0.09999999999999999);
    EXPECT_EQ(design.streamer->getCD(), 0.0719205716486004);
}

TEST_F(StreamerConfigTest, NoValueOfTheStripGoesBelowZero)
{
    // Java's DoubleModels have the minimum 0, and the core's setters take any value.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.streamer);
    const StreamerConfig&      panel = streamerPanel(created);
    panel.stripLengthWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.streamer->getStripLength(), 0.0);
    panel.stripWidthWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.streamer->getStripWidth(), 0.0);
    design.streamer->setStripLength(0.5);
    design.streamer->setStripWidth(0.05);
    panel.areaWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.streamer->getArea(), 0.0);
    EXPECT_EQ(design.streamer->getStripWidth(), 0.0);
    panel.cdWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.streamer->getCD(), 0.0);
    EXPECT_FALSE(design.streamer->isCDAutomatic());
}

TEST_F(StreamerConfigTest, ATypedDragCoefficientIsManualUntilAutomaticAgain)
{
    // D2bProbe: setValue(0.9) is manual; setAutomatic(true) computes 0.0719205716486004 again.
    const ConfigDialogServices services;
    const MassDesign           design;
    design.streamer->setArea(0.1);
    design.streamer->setAspectRatio(5);
    design.document->clearUndo();
    const FactoryPanel    created(*design.document, services.services(), *design.streamer);
    const StreamerConfig& panel = streamerPanel(created);
    panel.cdWidgets().spinBox->binding()->setValue(0.9);
    EXPECT_EQ(design.streamer->getCD(), 0.9);
    EXPECT_FALSE(design.streamer->isCDAutomatic());
    EXPECT_FALSE(panel.cdAutomatic().isChecked());
    panel.cdAutomatic().click();
    EXPECT_TRUE(design.streamer->isCDAutomatic());
    EXPECT_EQ(panel.cdWidgets().spinBox->binding()->value(), 0.0719205716486004);
}

TEST_F(StreamerConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingEdit)
{
    // Proved under the asan preset: the pending strip length, committed by the focus loss of the
    // hidden dialog, reaches none of the components the undo replaced.
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.mass->setName("Payload");
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.streamer);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const StreamerConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    typeInto(*panel->stripLengthWidgets().spinBox, QStringLiteral("80"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body =
        *design.document->getRocket().getChildren().front()->getChildren().front();
    const auto* streamer = dynamic_cast<const Streamer*>(body.getChildren().at(3));
    ASSERT_NE(streamer, nullptr);
    EXPECT_EQ(streamer->getStripLength(), 0.5);
    EXPECT_EQ(body.getChildren().at(0)->getName(), "Mass Component");
}

TEST_F(StreamerConfigTest, EveryFieldReachesEverySelectedStreamer)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<Streamer>());
    other.setStripWidth(0.1);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel    created(*design.document, services.services(), *design.streamer, others);
    const StreamerConfig& panel = streamerPanel(created);
    // Each keeps its own aspect ratio (5 for the other, 10 for the main).
    panel.areaWidgets().spinBox->binding()->setValue(0.1);
    EXPECT_EQ(design.streamer->getStripWidth(), 0.1);
    EXPECT_DOUBLE_EQ(other.getAspectRatio(), 5.0);
    EXPECT_DOUBLE_EQ(other.getArea(), 0.1);
    panel.cdAutomatic().click();
    EXPECT_FALSE(other.isCDAutomatic());
    EXPECT_FALSE(design.streamer->isCDAutomatic());
}

}  // namespace

}  // namespace QtRocket::Gui
