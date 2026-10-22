#include "QtRocket/gui/configdialog/NoseConeConfig.h"

#include <array>
#include <memory>
#include <span>
#include <utility>

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
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using NoseConeConfigTest = Test::ConfigDialogTest;
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

/// A rocket of one stage: a nose cone and a body tube whose diameter is not automatic.
struct NoseAndTubeDesign
{
    std::unique_ptr<OpenRocketDocument> document;
    NoseCone*                           nose{nullptr};
    BodyTube*                           tube{nullptr};

    NoseAndTubeDesign()
    {
        auto        rocket = std::make_unique<Rocket>();
        AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
        nose               = &stage.addChild(std::make_unique<NoseCone>());
        tube               = &stage.addChild(std::make_unique<BodyTube>());
        tube->setOuterRadiusAutomatic(false);
        rocket->enableEvents();
        document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    }
};

TEST_F(NoseConeConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& nose = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    const FactoryPanel<NoseConeConfig> panel(*design.document, services, nose);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Shoulder"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->tabs().tabToolTip(1), QStringLiteral("Shoulder properties"));
    EXPECT_EQ(qobject_cast<ShoulderPanel*>(panel->tabs().widget(1)), &panel->shoulderPanel());
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The shape: its combo box and its description.
    EXPECT_EQ(panel->shapeComboBox().objectName(), QStringLiteral("ShapeType"));
    EXPECT_EQ(panel->shapeComboBox().count(), static_cast<int>(kAllTransitionShapes.size()));
    EXPECT_EQ(panel->shapeComboBox().shownText(), QStringLiteral("Ogive"));
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(noseConeDescription(TransitionShape::OGIVE)));
    EXPECT_TRUE(
        panel->description().description().startsWith(QStringLiteral("An ogive nose cone")));

    // The shape parameter: 0 to 1 by 0.1, no unit selector, its slider over the shape's range.
    const auto& parameter = panel->shapeParameterWidgets();
    EXPECT_EQ(parameter.label->text(), QStringLiteral("Shape parameter:"));
    EXPECT_EQ(parameter.spinBox->objectName(), QStringLiteral("ShapeParameter"));
    EXPECT_EQ(parameter.unitSelector, nullptr);
    EXPECT_EQ(parameter.spinBox->fixedStep(), 0.1);
    EXPECT_EQ(parameter.spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(parameter.spinBox->binding()->maximum(), 1.0);
    EXPECT_TRUE(parameter.slider->mapping().isLinear());
    EXPECT_EQ(parameter.slider->mapping().getMax(), 1.0);
    EXPECT_TRUE(parameter.spinBox->isEnabled());

    // The other rows, named after Java's properties.
    EXPECT_EQ(panel->lengthWidgets().label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(panel->baseDiameterWidgets().label->text(), QStringLiteral("Base diameter:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Wall thickness:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"Length", "BaseRadius", "Thickness"}), "");
    EXPECT_EQ(panel->baseDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_EQ(panel->baseDiameterWidgets().spinBox->binding()->value(), 2 * nose.getBaseRadius());
    EXPECT_EQ(panel->automaticCheckBox().objectName(), QStringLiteral("BaseRadiusAutomatic"));
    EXPECT_EQ(panel->filledCheckBox().objectName(), QStringLiteral("Filled"));
    EXPECT_EQ(panel->filledCheckBox().toolTip(),
              QStringLiteral("Makes this component solid instead of hollow."));
    EXPECT_EQ(panel->flipCheckBox().objectName(), QStringLiteral("Flipped"));
    EXPECT_EQ(panel->flipCheckBox().text(), QStringLiteral("Flip to tail cone"));
    EXPECT_EQ(panel->flipCheckBox().toolTip(),
              QStringLiteral("Flips the nose cone direction to a tail cone."));

    // The sliders: 0, 10 cm, 70 cm; 0, 4 cm, 20 cm; 0 to the largest radius.
    const SliderMapping& length = panel->lengthWidgets().slider->mapping();
    EXPECT_FALSE(length.isLinear());
    EXPECT_EQ(length.getMid(), 0.1);
    EXPECT_EQ(length.getMax(), 0.7);
    const SliderMapping& base = panel->baseDiameterWidgets().slider->mapping();
    EXPECT_EQ(base.getMid(), 0.04);
    EXPECT_EQ(base.getMax(), 0.2);
    const SliderMapping& thickness = panel->thicknessWidgets().slider->mapping();
    EXPECT_TRUE(thickness.isLinear());
    EXPECT_EQ(thickness.getMax(), nose.getMaxRadius());

    EXPECT_EQ(panel->materialPanel().materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(panel->materialPanel().finishComboBox(), nullptr);
}

TEST_F(NoseConeConfigTest, TheShapeChoosesTheDescriptionAndTheParameter)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto& nose = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    const FactoryPanel<NoseConeConfig> panel(*design.document, services, nose);
    ASSERT_NE(panel.get(), nullptr);
    const auto& parameter = panel->shapeParameterWidgets();

    // A cone uses no parameter: the label, the spin box and the slider are disabled.
    chooseShape(panel->shapeComboBox(), TransitionShape::CONICAL);
    EXPECT_EQ(nose.getShapeType(), TransitionShape::CONICAL);
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(noseConeDescription(TransitionShape::CONICAL)));
    EXPECT_FALSE(parameter.label->isEnabled());
    EXPECT_FALSE(parameter.spinBox->isEnabled());
    EXPECT_FALSE(parameter.slider->isEnabled());

    // A Haack series does, up to 1/3 (the slider's maximum follows the shape).
    chooseShape(panel->shapeComboBox(), TransitionShape::HAACK);
    EXPECT_EQ(panel->description().description(),
              QtStrings::fromUtf8(noseConeDescription(TransitionShape::HAACK)));
    EXPECT_TRUE(parameter.label->isEnabled());
    EXPECT_TRUE(parameter.spinBox->isEnabled());
    EXPECT_TRUE(parameter.slider->isEnabled());
    EXPECT_EQ(parameter.slider->mapping().getMax(), 1.0 / 3.0);
}

TEST_F(NoseConeConfigTest, AnEditSetsEverySelectedNoseCone)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    auto&     first  = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    NoseCone& second = design.booster->addChild(std::make_unique<NoseCone>());
    const std::array<RocketComponent*, 1> others{&second};
    const FactoryPanel<NoseConeConfig>    panel(*design.document, services, first, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("12"));
    EXPECT_DOUBLE_EQ(first.getLength(), 0.12);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.12);
    ASSERT_TRUE(panel->baseDiameterWidgets().spinBox->binding()->commitText("5"));
    EXPECT_DOUBLE_EQ(first.getBaseRadius(), 0.025);
    EXPECT_DOUBLE_EQ(second.getBaseRadius(), 0.025);
    chooseShape(panel->shapeComboBox(), TransitionShape::POWER);
    EXPECT_EQ(first.getShapeType(), TransitionShape::POWER);
    EXPECT_EQ(second.getShapeType(), TransitionShape::POWER);
    ASSERT_TRUE(panel->shapeParameterWidgets().spinBox->binding()->commitText("0.3"));
    EXPECT_DOUBLE_EQ(first.getShapeParameter(), 0.3);
    EXPECT_DOUBLE_EQ(second.getShapeParameter(), 0.3);
    panel->filledCheckBox().click();
    EXPECT_TRUE(first.isFilled());
    EXPECT_TRUE(second.isFilled());
}

TEST_F(NoseConeConfigTest, FlippingDecidesAutomaticAgainForTheNewBase)
{
    // NoseConeConfig.updateCheckboxAutoBaseRadius() on every toggle of the flip: the base's
    // neighbour is the next component, once flipped the previous one, of which a nose cone at the
    // front has none.
    const ConfigDialogServices         services;
    const NoseAndTubeDesign            design;
    const FactoryPanel<NoseConeConfig> panel(*design.document, services, *design.nose);
    ASSERT_NE(panel.get(), nullptr);
    BoolCheckBox& automatic = panel->automaticCheckBox();
    EXPECT_TRUE(automatic.isEnabled());
    EXPECT_EQ(automatic.toolTip(), QStringLiteral("Uses the diameter of the next component."));
    automatic.click();
    EXPECT_TRUE(design.nose->isBaseRadiusAutomatic());

    panel->flipCheckBox().click();
    EXPECT_TRUE(design.nose->isFlipped());
    EXPECT_FALSE(automatic.isEnabled());
    EXPECT_EQ(automatic.toolTip(),
              QStringLiteral("There is no next component to take the diameter of."));
    EXPECT_FALSE(design.nose->isBaseRadiusAutomatic());
    EXPECT_FALSE(automatic.isChecked());

    panel->flipCheckBox().click();
    EXPECT_FALSE(design.nose->isFlipped());
    EXPECT_TRUE(automatic.isEnabled());
    EXPECT_EQ(automatic.toolTip(), QStringLiteral("Uses the diameter of the next component."));
    EXPECT_FALSE(design.nose->isBaseRadiusAutomatic());
}

TEST_F(NoseConeConfigTest, AFlipFromElsewhereLeavesAutomaticAsItIs)
{
    // Only the user's toggle decides again; a flip that reaches the panel through the rocket
    // (which nothing else makes while the dialog is open) refreshes the check box alone.
    const ConfigDialogServices         services;
    const NoseAndTubeDesign            design;
    const FactoryPanel<NoseConeConfig> panel(*design.document, services, *design.nose);
    ASSERT_NE(panel.get(), nullptr);
    design.nose->setFlipped(true);
    EXPECT_TRUE(panel->flipCheckBox().isChecked());
    EXPECT_TRUE(panel->automaticCheckBox().isEnabled());
    EXPECT_EQ(panel->automaticCheckBox().toolTip(),
              QStringLiteral("Uses the diameter of the next component."));
}

TEST_F(NoseConeConfigTest, ANoseConeWhoseNextComponentTakesItsDiameterHasNoAutomatic)
{
    // The design's body tube takes its diameter from the nose cone ahead of it: "Automatic" is
    // disabled (Java: Automatic_alreadyAuto) and the nose cone's automatic base switched off.
    const ConfigDialogServices services;
    const NoseAndTubeDesign    design;
    design.tube->setOuterRadiusAutomatic(true);
    static_cast<void>(design.tube->getOuterRadius());  // takes its reference, the nose cone
    ASSERT_TRUE(design.tube->usesPreviousCompAutomatic());
    const FactoryPanel<NoseConeConfig> panel(*design.document, services, *design.nose);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_FALSE(panel->automaticCheckBox().isEnabled());
    EXPECT_EQ(panel->automaticCheckBox().toolTip(),
              QStringLiteral("The next component already has its auto setting turned on."));
    EXPECT_FALSE(design.nose->isBaseRadiusAutomatic());
}

TEST_F(NoseConeConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object: the dialog closes inside the event, and neither
    // its shape listener nor its flip check box reaches a component that is gone (asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::BODY_TUBE).setName("Renamed");
    auto&      nose = dynamic_cast<NoseCone&>(design.component(ComponentKind::NOSE_CONE));
    const Uuid id   = nose.getId();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, nose);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<NoseConeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->lengthWidgets().spinBox, QStringLiteral("33"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    panel->flipCheckBox().click();
    chooseShape(panel->shapeComboBox(), TransitionShape::CONICAL);
    EXPECT_FALSE(panel->commitAll());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const NoseCone*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_FALSE(restored->isFlipped());
    EXPECT_EQ(restored->getShapeType(), TransitionShape::OGIVE);
    EXPECT_NE(restored->getLength(), 0.33);
}

TEST_F(NoseConeConfigTest, AComponentThatIsNoNoseConeIsABug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::TRANSITION));
    EXPECT_THROW(static_cast<void>(makeChild<NoseConeConfig>(&host, *design.document,
                                                             services.services(), selection)),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
