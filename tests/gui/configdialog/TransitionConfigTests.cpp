#include "QtRocket/gui/configdialog/TransitionConfig.h"

#include <array>
#include <memory>
#include <span>

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
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
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/ShoulderPanel.h"
#include "QtRocket/gui/util/EnumText.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using TransitionConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::typeInto;

/// Picks @p shape in @p combo as the user does (QComboBox::activated).
void chooseShape(EnumComboBox& combo, TransitionShape shape)
{
    Q_EMIT combo.activated(static_cast<int>(shape));
}

TEST_F(TransitionConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& transition = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    const FactoryPanel<TransitionConfig> panel(*design.document, services, transition);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Shoulder"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->tabs().tabToolTip(1), QStringLiteral("Shoulder properties"));
    EXPECT_EQ(qobject_cast<ShoulderPanel*>(panel->tabs().widget(1)), &panel->shoulderPanel());
    EXPECT_NE(panel->shoulderPanel().foreShoulder(), nullptr);

    EXPECT_EQ(panel->shapeComboBox().objectName(), QStringLiteral("ShapeType"));
    EXPECT_EQ(panel->shapeComboBox().shownText(), enumText(transition.getShapeType()));
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(transitionDescription(transition.getShapeType())));
    EXPECT_EQ(panel->clippedCheckBox().objectName(), QStringLiteral("Clipped"));
    EXPECT_EQ(panel->clippedCheckBox().text(), QStringLiteral("Clipped"));
    EXPECT_EQ(panel->clippedCheckBox().toolTip(),
              QStringLiteral("Changes the transition end tangency point from rounded to sharp."));

    const auto& parameter = panel->shapeParameterWidgets();
    EXPECT_EQ(parameter.label->text(), QStringLiteral("Shape parameter:"));
    EXPECT_EQ(parameter.unitSelector, nullptr);
    EXPECT_EQ(parameter.spinBox->fixedStep(), 0.1);

    EXPECT_EQ(panel->lengthWidgets().label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(panel->foreDiameterWidgets().label->text(), QStringLiteral("Fore diameter:"));
    EXPECT_EQ(panel->aftDiameterWidgets().label->text(), QStringLiteral("Aft diameter:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Wall thickness:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"Length", "ForeRadius", "AftRadius", "Thickness"}), "");
    EXPECT_EQ(panel->foreAutomaticCheckBox().objectName(), QStringLiteral("ForeRadiusAutomatic"));
    EXPECT_EQ(panel->aftAutomaticCheckBox().objectName(), QStringLiteral("AftRadiusAutomatic"));
    EXPECT_EQ(panel->foreDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_EQ(panel->aftDiameterWidgets().spinBox->binding()->value(),
              2 * transition.getAftRadius());
    EXPECT_EQ(panel->filledCheckBox().toolTip(),
              QStringLiteral("Makes this component solid instead of hollow."));

    // The sliders: 0, 5 cm, 30 cm; 0, 4 cm, 20 cm twice; 0 to the largest radius.
    const SliderMapping& length = panel->lengthWidgets().slider->mapping();
    EXPECT_FALSE(length.isLinear());
    EXPECT_EQ(length.getMid(), 0.05);
    EXPECT_EQ(length.getMax(), 0.3);
    EXPECT_EQ(panel->foreDiameterWidgets().slider->mapping().getMid(), 0.04);
    EXPECT_EQ(panel->foreDiameterWidgets().slider->mapping().getMax(), 0.2);
    EXPECT_EQ(panel->aftDiameterWidgets().slider->mapping().getMid(), 0.04);
    EXPECT_EQ(panel->aftDiameterWidgets().slider->mapping().getMax(), 0.2);
    EXPECT_EQ(panel->thicknessWidgets().slider->mapping().getMax(), transition.getMaxRadius());

    EXPECT_EQ(panel->materialPanel().materialComboBox().binding()->getType(), Material::Type::BULK);
}

TEST_F(TransitionConfigTest, ClippedAndTheParameterFollowTheShape)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& transition = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    const FactoryPanel<TransitionConfig> panel(*design.document, services, transition);
    ASSERT_NE(panel.get(), nullptr);

    // A cone can be clipped no more than it uses a parameter.
    chooseShape(panel->shapeComboBox(), TransitionShape::CONICAL);
    EXPECT_EQ(transition.getShapeType(), TransitionShape::CONICAL);
    EXPECT_FALSE(panel->clippedCheckBox().isEnabled());
    EXPECT_FALSE(panel->shapeParameterWidgets().spinBox->isEnabled());
    EXPECT_FALSE(panel->shapeParameterWidgets().label->isEnabled());
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(transitionDescription(TransitionShape::CONICAL)));

    // An ellipsoid can be clipped (and is, as a new shape that can be), and uses no parameter.
    chooseShape(panel->shapeComboBox(), TransitionShape::ELLIPSOID);
    EXPECT_TRUE(panel->clippedCheckBox().isEnabled());
    EXPECT_TRUE(panel->clippedCheckBox().isChecked());
    EXPECT_FALSE(panel->shapeParameterWidgets().slider->isEnabled());
    panel->clippedCheckBox().click();
    EXPECT_FALSE(transition.isClipped());

    // A power series uses it.
    chooseShape(panel->shapeComboBox(), TransitionShape::POWER);
    EXPECT_TRUE(panel->shapeParameterWidgets().spinBox->isEnabled());
    EXPECT_TRUE(panel->shapeParameterWidgets().slider->isEnabled());
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(transitionDescription(TransitionShape::POWER)));
}

TEST_F(TransitionConfigTest, AnEditSetsEverySelectedTransition)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&       first  = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    Transition& second = design.booster->addChild(std::make_unique<Transition>());
    const std::array<RocketComponent*, 1> others{&second};
    const FactoryPanel<TransitionConfig>  panel(*design.document, services, first, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("7"));
    EXPECT_DOUBLE_EQ(first.getLength(), 0.07);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.07);
    ASSERT_TRUE(panel->foreDiameterWidgets().spinBox->binding()->commitText("4"));
    EXPECT_DOUBLE_EQ(first.getForeRadius(), 0.02);
    EXPECT_DOUBLE_EQ(second.getForeRadius(), 0.02);
    EXPECT_FALSE(first.isForeRadiusAutomatic());
    EXPECT_FALSE(second.isForeRadiusAutomatic());
    ASSERT_TRUE(panel->aftDiameterWidgets().spinBox->binding()->commitText("6"));
    EXPECT_DOUBLE_EQ(first.getAftRadius(), 0.03);
    EXPECT_DOUBLE_EQ(second.getAftRadius(), 0.03);
    ASSERT_TRUE(panel->thicknessWidgets().spinBox->binding()->commitText("0.25"));
    EXPECT_DOUBLE_EQ(first.getThickness(), 0.0025);
    EXPECT_DOUBLE_EQ(second.getThickness(), 0.0025);
    panel->filledCheckBox().click();
    EXPECT_TRUE(first.isFilled());
    EXPECT_TRUE(second.isFilled());
}

TEST_F(TransitionConfigTest, WithoutNeighboursBothAutomaticsAreOff)
{
    // TransitionConfig.updateCheckboxAutoForeRadius() and updateCheckboxAutoAftRadius(): no
    // previous and no next symmetric component.
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    RocketComponent& stage      = *document->getRocket().getChildren().front();
    auto&            transition = stage.addChild(std::make_unique<Transition>());
    ASSERT_TRUE(transition.isForeRadiusAutomatic());
    ASSERT_TRUE(transition.isAftRadiusAutomatic());
    const FactoryPanel<TransitionConfig> panel(*document, services, transition);
    ASSERT_NE(panel.get(), nullptr);
    const QString noReference =
        QStringLiteral("There is no previous/next component to take the diameter of.");
    EXPECT_FALSE(panel->foreAutomaticCheckBox().isEnabled());
    EXPECT_FALSE(panel->foreAutomaticCheckBox().isChecked());
    EXPECT_EQ(panel->foreAutomaticCheckBox().toolTip(), noReference);
    EXPECT_FALSE(panel->aftAutomaticCheckBox().isEnabled());
    EXPECT_FALSE(panel->aftAutomaticCheckBox().isChecked());
    EXPECT_EQ(panel->aftAutomaticCheckBox().toolTip(), noReference);
    EXPECT_FALSE(transition.isForeRadiusAutomatic());
    EXPECT_FALSE(transition.isAftRadiusAutomatic());
}

TEST_F(TransitionConfigTest, AnAutomaticRadiusFollowsItsCheckBox)
{
    // A transition before a body tube of its own diameter: the aft radius can come from the tube,
    // the fore radius from nothing.
    const ConfigDialogServices                services;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    RocketComponent& stage      = *document->getRocket().getChildren().front();
    auto&            transition = stage.addChild(std::make_unique<Transition>());
    auto&            tube       = stage.addChild(std::make_unique<BodyTube>());
    tube.setOuterRadius(0.05);
    transition.setAftRadius(0.01);
    ASSERT_FALSE(transition.isAftRadiusAutomatic());
    const FactoryPanel<TransitionConfig> panel(*document, services, transition);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_FALSE(panel->foreAutomaticCheckBox().isEnabled());
    BoolCheckBox& aft = panel->aftAutomaticCheckBox();
    ASSERT_TRUE(aft.isEnabled());
    EXPECT_EQ(aft.toolTip(), QStringLiteral("Uses the diameter of the previous/next component."));
    aft.click();
    EXPECT_TRUE(transition.isAftRadiusAutomatic());
    EXPECT_DOUBLE_EQ(transition.getAftRadius(), 0.05);
    EXPECT_DOUBLE_EQ(panel->aftDiameterWidgets().spinBox->binding()->value(), 0.1);
}

TEST_F(TransitionConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object: the dialog closes inside the event, and neither
    // the shape listener nor a pending edit reaches a component that is gone (asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::BODY_TUBE).setName("Renamed");
    auto&      transition = dynamic_cast<Transition&>(design.component(ComponentKind::TRANSITION));
    const Uuid id         = transition.getId();
    const TransitionShape shape = transition.getShapeType();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, transition);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<TransitionConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->foreDiameterWidgets().spinBox, QStringLiteral("9"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    chooseShape(panel->shapeComboBox(), TransitionShape::HAACK);
    EXPECT_FALSE(panel->commitAll());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const Transition*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getShapeType(), shape);
    EXPECT_NE(restored->getForeRadius(), 0.045);
}

TEST_F(TransitionConfigTest, AComponentThatIsNoTransitionIsABug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::BODY_TUBE));
    EXPECT_THROW(static_cast<void>(makeChild<TransitionConfig>(&host, *design.document,
                                                               services.services(), selection)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
