#include "QtRocket/gui/configdialog/ParachuteConfig.h"

#include <array>
#include <memory>

#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMargins>
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
#include "QtRocket/gui/binding/IntBinding.h"
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
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ParachuteConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;

/// The panel of @p created as a ParachuteConfig.
[[nodiscard]] ParachuteConfig& parachutePanel(const FactoryPanel& created)
{
    auto* panel = created.as<ParachuteConfig>();
    if (panel == nullptr)
    {
        bug("not a parachute's panel");
    }
    return *panel;
}

/// The first material @p binding offers that is not its value; null when there is none.
[[nodiscard]] const Material* otherMaterial(const MaterialBinding& binding)
{
    for (int i = 0; i < binding.size(); ++i)
    {
        const Material* material = binding.materialAt(i);
        if (material != nullptr && !(*material == binding.value()))
        {
            return material;
        }
    }
    return nullptr;
}

TEST_F(ParachuteConfigTest, TheTabsAreJavas)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Radial position"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel.tabs().widget(1), &panel.radialPosition());
}

TEST_F(ParachuteConfigTest, TheCanopyHasJavasFields)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(panel.canopyGroup().title(), QStringLiteral("Canopy"));
    EXPECT_EQ(labelTextsUnder(panel.canopyGroup()),
              (QStringList{QStringLiteral("Diameter:"), QStringLiteral("Material:"),
                           QStringLiteral("<html>Drag coefficient C<sub>D</sub>:")}));
    EXPECT_EQ(panel.diameterWidgets().spinBox->objectName(), QStringLiteral("Diameter"));
    EXPECT_EQ(panel.diameterWidgets().slider->mapping().getMid(), 0.4);
    EXPECT_EQ(panel.diameterWidgets().slider->mapping().getMax(), 1.5);
    EXPECT_EQ(panel.materialComboBox().objectName(), QStringLiteral("Material"));
    EXPECT_EQ(panel.materialComboBox().binding()->getType(), Material::Type::SURFACE);
    EXPECT_EQ(panel.materialComboBox().binding()->value().getName(), "Ripstop nylon");
    EXPECT_EQ(panel.materialComboBox().toolTip(),
              QStringLiteral("The component material affects the weight of the component."));

    const QString tip = QStringLiteral(
        "<html>The drag coefficient relative to the total area of the parachute. "
        "<br>A larger drag coefficient yields a slowed descent rate.  A typical "
        "value for parachutes is 0.8.");
    EXPECT_EQ(panel.cdLabel().toolTip(), tip);
    EXPECT_EQ(panel.cdWidgets().spinBox->toolTip(), tip);
    EXPECT_EQ(panel.cdWidgets().spinBox->objectName(), QStringLiteral("CD"));
    EXPECT_EQ(panel.cdWidgets().unitSelector, nullptr);
    EXPECT_EQ(panel.cdWidgets().slider, nullptr);
    EXPECT_EQ(panel.resetCdButton().text(), QStringLiteral("Reset"));
    // D2bProbe: String.format("%.2f", DEFAULT_CD).
    EXPECT_EQ(panel.resetCdButton().toolTip(),
              QStringLiteral("Resets the drag coefficient (C<sub>D</sub>) to a value of 0.80."));
    EXPECT_EQ(ParachuteConfig::resetCdToolTip(), panel.resetCdButton().toolTip());
}

TEST_F(ParachuteConfigTest, TheShroudLinesHaveJavasFields)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(panel.shroudGroup().title(), QStringLiteral("Shroud lines"));
    EXPECT_EQ(labelTextsUnder(panel.shroudGroup()),
              (QStringList{QStringLiteral("Number of lines:"), QStringLiteral("Line length:"),
                           QStringLiteral("Material:")}));
    EXPECT_EQ(panel.lineCountSpinBox().objectName(), QStringLiteral("LineCount"));
    EXPECT_EQ(panel.lineLengthWidgets().spinBox->objectName(), QStringLiteral("LineLength"));
    EXPECT_EQ(panel.lineLengthAutomatic().objectName(), QStringLiteral("LineLengthAutomatic"));
    EXPECT_EQ(panel.lineLengthAutomatic().toolTip(),
              QStringLiteral("Sets the line length to 1.5x the canopy diameter."));
    EXPECT_EQ(panel.lineMaterialComboBox().objectName(), QStringLiteral("LineMaterial"));
    EXPECT_EQ(panel.lineMaterialComboBox().binding()->getType(), Material::Type::LINE);
    EXPECT_EQ(panel.lineMaterialComboBox().binding()->value().getName(),
              "Elastic cord (round 2 mm, 1/16 in)");
    EXPECT_TRUE(panel.lineMaterialComboBox().toolTip().isEmpty());
}

TEST_F(ParachuteConfigTest, TheRightSideHasThePackedSizeAndTheDeployment)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(panel.sizeWidgets().length.label->text(), QStringLiteral("Packed length:"));
    EXPECT_EQ(panel.sizeWidgets().diameter.label->text(), QStringLiteral("Packed diameter:"));
    EXPECT_GE(panel.placement().grid().indexOf(panel.sizeWidgets().automatic), 0);
    ASSERT_NE(panel.deploymentWidgets().group, nullptr);
    EXPECT_EQ(panel.deploymentWidgets().group->parentWidget(), panel.placement().parentWidget());
}

TEST_F(ParachuteConfigTest, TheValuesAreJavas)
{
    // D2bProbe: a new parachute: 30 cm, CD 0.8 (automatic), 6 lines of 45 cm (automatic).
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(panel.diameterWidgets().spinBox->binding()->value(), 0.3);
    EXPECT_EQ(panel.cdWidgets().spinBox->binding()->value(), 0.8);
    const std::shared_ptr<IntBinding>& lineCount = panel.lineCountSpinBox().binding();
    EXPECT_EQ(lineCount->value(), 6);
    EXPECT_EQ(panel.lineLengthWidgets().spinBox->binding()->value(), 0.44999999999999996);
    EXPECT_TRUE(panel.lineLengthAutomatic().isChecked());

    panel.diameterWidgets().spinBox->binding()->setValue(0.5);
    EXPECT_EQ(design.parachute->getLineLength(), 0.75);
    EXPECT_TRUE(design.parachute->isLineLengthAutomatic());
    panel.diameterWidgets().spinBox->binding()->setValue(-0.5);
    EXPECT_EQ(design.parachute->getDiameter(), 0.0);
    lineCount->setValue(-2);
    EXPECT_EQ(design.parachute->getLineCount(), 0);
}

TEST_F(ParachuteConfigTest, ResetMakesTheDefaultDragCoefficientManual)
{
    // D2bProbe: setValue(1.5) makes it manual; Reset's setCD(DEFAULT_CD) gives 0.8, manual; a
    // negative CD is 0.
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<Parachute>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel created(*design.document, services.services(), *design.parachute, others);
    const ParachuteConfig& panel = parachutePanel(created);
    panel.cdWidgets().spinBox->binding()->setValue(1.5);
    EXPECT_EQ(design.parachute->getCD(), 1.5);
    EXPECT_FALSE(design.parachute->isCDAutomatic());
    EXPECT_EQ(other.getCD(), 1.5);
    panel.resetCdButton().click();
    EXPECT_EQ(design.parachute->getCD(), 0.8);
    EXPECT_FALSE(design.parachute->isCDAutomatic());
    EXPECT_EQ(other.getCD(), 0.8);
    EXPECT_FALSE(other.isCDAutomatic());
    panel.cdWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.parachute->getCD(), 0.0);
}

TEST_F(ParachuteConfigTest, TheLineMaterialIsTheParachutesOwn)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel               = parachutePanel(created);
    const std::shared_ptr<MaterialBinding>& lines  = panel.lineMaterialComboBox().binding();
    const Material*                         chosen = otherMaterial(*lines);
    ASSERT_NE(chosen, nullptr);
    ASSERT_TRUE(lines->select(*chosen));
    EXPECT_EQ(design.parachute->getLineMaterial(), *chosen);
    EXPECT_EQ(design.parachute->getMaterial().getName(), "Ripstop nylon");
}

TEST_F(ParachuteConfigTest, TheRadialResetKeepsJavasLiteralKeyAsItsTooltip)
{
    // ParachuteConfig.java:364 sets the key itself, "ParachuteCfg.but.ResetRadial.ttip", as the
    // tooltip (an OpenRocket slip kept); the tab has Java's 25 px margin.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.parachute);
    const ParachuteConfig&     panel = parachutePanel(created);
    EXPECT_EQ(panel.radialPosition().resetButton().toolTip(),
              QStringLiteral("ParachuteCfg.but.ResetRadial.ttip"));
    EXPECT_EQ(panel.radialPosition().grid().contentsMargins().left(), 25);
}

TEST_F(ParachuteConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingEdit)
{
    // Proved under the asan preset: the pending delay, committed by the focus loss of the hidden
    // dialog, reaches none of the components the undo replaced, and neither the packed diameter's
    // "Automatic" nor the altitude's enabling reads them.
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.mass->setName("Payload");
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.parachute);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const ParachuteConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    typeInto(*panel->deploymentWidgets().delay, QStringLiteral("4"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body =
        *design.document->getRocket().getChildren().front()->getChildren().front();
    const auto* parachute = dynamic_cast<const Parachute*>(body.getChildren().at(2));
    ASSERT_NE(parachute, nullptr);
    EXPECT_EQ(parachute->getDeploymentConfigurations().getDefault().getDeployDelay(), 0.0);
    EXPECT_EQ(body.getChildren().at(0)->getName(), "Mass Component");
}

}  // namespace

}  // namespace QtRocket::Gui
