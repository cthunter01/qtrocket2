#include "QtRocket/gui/configdialog/BodyTubeConfig.h"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using BodyTubeConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::typeInto;

/// A design whose body tube "Lone" has no symmetric neighbour: Rocket > Stage > Lone, which holds
/// a pod set with the body tube "Pod tube" (not in line with Lone while Lone's diameter is
/// automatic, SymmetricComponent::isInline()); both automatic.
struct LoneTubeDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    BodyTube*                           lone{nullptr};
    BodyTube*                           podTube{nullptr};

    LoneTubeDesign()
    {
        auto        rocket = std::make_unique<Rocket>();
        AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
        lone               = &stage.addChild(std::make_unique<BodyTube>());
        lone->setName("Lone");
        PodSet& pods = lone->addChild(std::make_unique<PodSet>());
        podTube      = &pods.addChild(std::make_unique<BodyTube>());
        podTube->setName("Pod tube");
        lone->setOuterRadiusAutomatic(true);
        podTube->setOuterRadiusAutomatic(true);
        rocket->enableEvents();
        document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
        document->clearUndo();
    }
};

TEST_F(BodyTubeConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const FactoryPanel<BodyTubeConfig> panel(*design.document, services, tube);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Motor"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->tabs().tabToolTip(1), QStringLiteral("Motor mount configuration"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);
    EXPECT_EQ(qobject_cast<MotorConfig*>(panel->tabs().widget(1)), &panel->motorConfig());

    // The rows, named after Java's properties.
    EXPECT_EQ(panel->lengthWidgets().label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(panel->outerDiameterWidgets().label->text(), QStringLiteral("Outer diameter:"));
    EXPECT_EQ(panel->innerDiameterWidgets().label->text(), QStringLiteral("Inner diameter:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Wall thickness:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"Length", "OuterRadius", "InnerRadius", "Thickness"}),
              "");
    EXPECT_EQ(panel->automaticCheckBox().objectName(), QStringLiteral("OuterRadiusAutomatic"));
    EXPECT_EQ(panel->automaticCheckBox().text(), QStringLiteral("Automatic"));
    EXPECT_EQ(panel->filledCheckBox().objectName(), QStringLiteral("Filled"));
    EXPECT_EQ(panel->filledCheckBox().text(), QStringLiteral("Filled"));
    EXPECT_EQ(panel->filledCheckBox().toolTip(),
              QStringLiteral("Changes the body tube from a hollow tube to a solid rod."));

    // The values: diameters shown as the radius times 2.
    EXPECT_EQ(panel->outerDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_EQ(panel->innerDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_EQ(panel->outerDiameterWidgets().spinBox->binding()->value(), 2 * tube.getOuterRadius());
    EXPECT_EQ(panel->lengthWidgets().spinBox->binding()->value(), tube.getLength());
    EXPECT_EQ(panel->thicknessWidgets().spinBox->binding()->value(), tube.getThickness());
    EXPECT_EQ(panel->lengthWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel->outerDiameterWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel->innerDiameterWidgets().spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(panel->thicknessWidgets().spinBox->binding()->minimum(), 0.0);

    // The sliders: 0, 50 cm, 2 m; 0, 4 cm, 20 cm; 0 to the outer diameter; 0 to 1 cm.
    const SliderMapping& length = panel->lengthWidgets().slider->mapping();
    EXPECT_FALSE(length.isLinear());
    EXPECT_EQ(length.getMin(), 0.0);
    EXPECT_EQ(length.getLinearPosition(), 0.5);
    EXPECT_EQ(length.getMid(), 0.5);
    EXPECT_EQ(length.getMax(), 2.0);
    const SliderMapping& outer = panel->outerDiameterWidgets().slider->mapping();
    EXPECT_FALSE(outer.isLinear());
    EXPECT_EQ(outer.getMid(), 0.04);
    EXPECT_EQ(outer.getMax(), 0.2);
    const SliderMapping& inner = panel->innerDiameterWidgets().slider->mapping();
    EXPECT_TRUE(inner.isLinear());
    EXPECT_EQ(inner.getMax(), 2 * tube.getOuterRadius());
    EXPECT_EQ(panel->innerDiameterWidgets().slider->dynamicBounds().max,
              panel->outerDiameterWidgets().spinBox->binding());
    const SliderMapping& thickness = panel->thicknessWidgets().slider->mapping();
    EXPECT_TRUE(thickness.isLinear());
    EXPECT_EQ(thickness.getMax(), 0.01);

    // The material: bulk materials, and the finish of an external component.
    EXPECT_EQ(panel->materialPanel().materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(panel->materialPanel().finishComboBox(), nullptr);
}

TEST_F(BodyTubeConfigTest, TheInnerDiameterSliderFollowsTheOuterDiameter)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const FactoryPanel<BodyTubeConfig> panel(*design.document, services, tube);
    ASSERT_NE(panel.get(), nullptr);
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("8"));
    EXPECT_DOUBLE_EQ(tube.getOuterRadius(), 0.04);
    EXPECT_DOUBLE_EQ(panel->innerDiameterWidgets().slider->mapping().getMax(), 0.08);
}

TEST_F(BodyTubeConfigTest, AnEditSetsEverySelectedTube)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&     body    = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    BodyTube& booster = *design.boosterTube;
    const std::array<RocketComponent*, 1> others{&booster};
    const FactoryPanel<BodyTubeConfig>    panel(*design.document, services, body, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("50"));
    EXPECT_DOUBLE_EQ(body.getLength(), 0.5);
    EXPECT_DOUBLE_EQ(booster.getLength(), 0.5);
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("6"));
    EXPECT_DOUBLE_EQ(body.getOuterRadius(), 0.03);
    EXPECT_DOUBLE_EQ(booster.getOuterRadius(), 0.03);
    EXPECT_FALSE(body.isOuterRadiusAutomatic());
    EXPECT_FALSE(booster.isOuterRadiusAutomatic());
    ASSERT_TRUE(panel->thicknessWidgets().spinBox->binding()->commitText("0.3"));
    EXPECT_DOUBLE_EQ(body.getThickness(), 0.003);
    EXPECT_DOUBLE_EQ(booster.getThickness(), 0.003);
    // The inner diameter gives each tube the thickness of its own outer radius (U3).
    ASSERT_TRUE(panel->innerDiameterWidgets().spinBox->binding()->commitText("5"));
    EXPECT_DOUBLE_EQ(body.getInnerRadius(), 0.025);
    EXPECT_DOUBLE_EQ(booster.getInnerRadius(), 0.025);
    panel->filledCheckBox().click();
    EXPECT_TRUE(body.isFilled());
    EXPECT_TRUE(booster.isFilled());
}

TEST_F(BodyTubeConfigTest, AutomaticSwitchesTheDiameterOfEverySelectedTube)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&     body    = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    BodyTube& booster = *design.boosterTube;
    body.setOuterRadius(0.03);
    booster.setOuterRadius(0.02);
    const std::array<RocketComponent*, 1> others{&booster};
    const FactoryPanel<BodyTubeConfig>    panel(*design.document, services, body, others);
    ASSERT_NE(panel.get(), nullptr);
    BoolCheckBox& automatic = panel->automaticCheckBox();
    ASSERT_TRUE(automatic.isEnabled());
    EXPECT_EQ(automatic.toolTip(),
              QStringLiteral("Uses the diameter of the previous/next component."));
    EXPECT_FALSE(automatic.isChecked());
    automatic.click();
    EXPECT_TRUE(body.isOuterRadiusAutomatic());
    EXPECT_TRUE(booster.isOuterRadiusAutomatic());
    EXPECT_TRUE(automatic.isChecked());
    // A typed diameter ends the automatic one (the setter's rule).
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("5"));
    EXPECT_FALSE(body.isOuterRadiusAutomatic());
    EXPECT_FALSE(booster.isOuterRadiusAutomatic());
    EXPECT_FALSE(automatic.isChecked());
}

TEST_F(BodyTubeConfigTest, WithoutANeighbourAutomaticIsOffOnEverySelectedTube)
{
    // BodyTubeConfig.updateCheckboxAutoAftRadius(): no previous and no next symmetric component,
    // so the check box is disabled and the main tube's setOuterRadiusAutomatic(false) reaches
    // every selected tube (Java: its config listeners), whatever their own neighbours.
    const ConfigDialogServices services;
    const LoneTubeDesign       design;
    ASSERT_EQ(design.lone->getPreviousSymmetricComponent(), nullptr);
    ASSERT_EQ(design.lone->getNextSymmetricComponent(), nullptr);
    const std::array<RocketComponent*, 1> others{design.podTube};
    const FactoryPanel<BodyTubeConfig>    panel(*design.document, services, *design.lone, others);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_FALSE(panel->automaticCheckBox().isEnabled());
    EXPECT_FALSE(panel->automaticCheckBox().isChecked());
    EXPECT_EQ(panel->automaticCheckBox().toolTip(),
              QStringLiteral("There is no previous/next component to take the diameter of."));
    EXPECT_FALSE(design.lone->isOuterRadiusAutomatic());
    EXPECT_FALSE(design.podTube->isOuterRadiusAutomatic());
    // The check box stays disabled after a change of the rocket (decided once, as Java).
    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("40"));
    EXPECT_FALSE(panel->automaticCheckBox().isEnabled());
}

TEST_F(BodyTubeConfigTest, TypedTextReachesTheTube)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, tube);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<BodyTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    UnitSpinBox& length = *panel->lengthWidgets().spinBox;
    typeInto(length, QStringLiteral("45"));
    QTest::keyClick(&length, Qt::Key_Return);
    EXPECT_DOUBLE_EQ(tube.getLength(), 0.45);
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(BodyTubeConfigTest, AnUnchangedCancelKeepsWhatThePanelSwitchedOff)
{
    // T3 and the user's decision U4: the panel of a tube without neighbours switches its automatic
    // diameter off while it is made, before the dialog's undo position; that is no modification,
    // so an unchanged Cancel closes without an undo. (OpenRocket counted it, and its Cancel undid
    // the step before the dialog: "Add Body Tube", and the new tube was gone.)
    ConfigDialogServices                      services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    QWidget window;
    services.services().preferences().setShowDiscardConfirmation(false);
    document->addUndoPosition("Add Body Tube");
    RocketComponent& stage = *document->getRocket().getChildren().front();
    auto&            tube  = stage.addChild(std::make_unique<BodyTube>());
    tube.setOuterRadiusAutomatic(true);
    const Uuid id = tube.getId();
    ASSERT_EQ(tube.getPreviousSymmetricComponent(), nullptr);
    ASSERT_EQ(tube.getNextSymmetricComponent(), nullptr);

    ComponentConfigDialog::showDialog(window, services.services(), *document, tube);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_FALSE(tube.isOuterRadiusAutomatic());
    EXPECT_FALSE(dialog->isModified());
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Body Tube configuration"));
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    const auto* kept = dynamic_cast<const BodyTube*>(document->getRocket().findComponent(id));
    ASSERT_NE(kept, nullptr);
    EXPECT_FALSE(kept->isOuterRadiusAutomatic());
    EXPECT_EQ(document->getUndoDescription(), "Add Body Tube");
}

TEST_F(BodyTubeConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and the length typed but not committed reaches none of the components that are gone
    // (proved under the asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    auto&        tube   = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const Uuid   id     = tube.getId();
    const double length = tube.getLength();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, tube);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<BodyTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->lengthWidgets().spinBox, QStringLiteral("77"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(panel->commitAll());
    panel->updateFields();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getLength(), length);
}

}  // namespace

}  // namespace QtRocket::Gui
