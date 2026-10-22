#include "QtRocket/gui/configdialog/ShockCordConfig.h"

#include <array>
#include <memory>

#include <QCoreApplication>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
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
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RadialPositionPanel.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ShockCordConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;

/// The panel of @p created as a ShockCordConfig.
[[nodiscard]] ShockCordConfig& cordPanel(const FactoryPanel& created)
{
    auto* panel = created.as<ShockCordConfig>();
    if (panel == nullptr)
    {
        bug("not a shock cord's panel");
    }
    return *panel;
}

TEST_F(ShockCordConfigTest, TheTabsAreJavas)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.cord);
    const ShockCordConfig&     panel = cordPanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Radial position"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel.tabs().widget(1), &panel.radialPosition());
}

TEST_F(ShockCordConfigTest, TheGeneralTabHasJavasFields)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.cord);
    const ShockCordConfig&     panel = cordPanel(created);
    EXPECT_EQ(panel.cordLengthWidgets().label->text(), QStringLiteral("Shock cord length:"));
    EXPECT_EQ(panel.cordLengthWidgets().spinBox->objectName(), QStringLiteral("CordLength"));
    EXPECT_EQ(panel.cordLengthWidgets().spinBox->binding()->minimum(), 0.0);
    // getSliderModel(0, 1, 10).
    EXPECT_EQ(panel.cordLengthWidgets().slider->mapping().getMid(), 1.0);
    EXPECT_EQ(panel.cordLengthWidgets().slider->mapping().getMax(), 10.0);
    EXPECT_EQ(panel.cordLengthAutomatic().text(), QStringLiteral("Automatic"));
    EXPECT_EQ(panel.cordLengthAutomatic().objectName(), QStringLiteral("CordLengthAutomatic"));
    EXPECT_EQ(panel.cordLengthAutomatic().toolTip(),
              QStringLiteral("Sets the shock cord length to 3x the rocket length."));

    // The line material, without a finish.
    EXPECT_EQ(labelTextsUnder(panel.material()),
              QStringList{QStringLiteral("Shock cord material:")});
    EXPECT_EQ(panel.material().materialComboBox().binding()->getType(), Material::Type::LINE);
    EXPECT_EQ(panel.material().finishComboBox(), nullptr);
    EXPECT_EQ(panel.material().materialComboBox().binding()->value().getName(),
              "Elastic cord (round 2 mm, 1/16 in)");

    // The packed size in the placement's box.
    const MassObjectFields::SizeWidgets& size = panel.sizeWidgets();
    EXPECT_EQ(size.length.label->text(), QStringLiteral("Packed length:"));
    EXPECT_EQ(size.diameter.label->text(), QStringLiteral("Packed diameter:"));
    EXPECT_EQ(size.length.spinBox->parentWidget(), &panel.placement());
    EXPECT_EQ(size.automatic->parentWidget(), &panel.placement());
    EXPECT_GE(panel.placement().grid().indexOf(size.length.spinBox), 0);
}

TEST_F(ShockCordConfigTest, TheCordLengthIsAutomaticUntilTyped)
{
    // D2bProbe: a new cord is three times the 20 cm rocket, automatic; a typed length is manual.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.cord);
    const ShockCordConfig&     panel = cordPanel(created);
    EXPECT_EQ(panel.cordLengthWidgets().spinBox->binding()->value(), 0.6000000000000001);
    EXPECT_TRUE(panel.cordLengthAutomatic().isChecked());
    panel.cordLengthWidgets().spinBox->binding()->setValue(0.5);
    EXPECT_EQ(design.cord->getCordLength(), 0.5);
    EXPECT_FALSE(design.cord->isCordLengthAutomatic());
    EXPECT_FALSE(panel.cordLengthAutomatic().isChecked());
    panel.cordLengthAutomatic().click();
    EXPECT_TRUE(design.cord->isCordLengthAutomatic());
    EXPECT_EQ(design.cord->getCordLength(), 0.6000000000000001);
    panel.cordLengthWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.cord->getCordLength(), 0.0);
}

TEST_F(ShockCordConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingEdit)
{
    // Proved under the asan preset: the pending cord length, committed by the focus loss of the
    // hidden dialog, reaches none of the components the undo replaced.
    const ConfigDialogServices services;
    const MassDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.mass->setName("Payload");
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.cord);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const ShockCordConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    typeInto(*panel->cordLengthWidgets().spinBox, QStringLiteral("150"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body =
        *design.document->getRocket().getChildren().front()->getChildren().front();
    const auto* cord = dynamic_cast<const ShockCord*>(body.getChildren().at(1));
    ASSERT_NE(cord, nullptr);
    EXPECT_TRUE(cord->isCordLengthAutomatic());
    EXPECT_EQ(body.getChildren().at(0)->getName(), "Mass Component");
}

TEST_F(ShockCordConfigTest, EveryFieldReachesEverySelectedShockCord)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<ShockCord>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel     created(*design.document, services.services(), *design.cord, others);
    const ShockCordConfig& panel = cordPanel(created);
    panel.cordLengthWidgets().spinBox->binding()->setValue(1.2);
    EXPECT_EQ(other.getCordLength(), 1.2);
    EXPECT_FALSE(other.isCordLengthAutomatic());
    panel.cordLengthAutomatic().click();
    EXPECT_TRUE(other.isCordLengthAutomatic());
}

}  // namespace

}  // namespace QtRocket::Gui
