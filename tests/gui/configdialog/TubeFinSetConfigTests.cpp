#include "QtRocket/gui/configdialog/TubeFinSetConfig.h"

#include <array>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
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
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using TubeFinSetConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::focusOrder;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::takesFromZeroUp;
using Test::typeInto;

/// The tube fin set of @p design.
[[nodiscard]] TubeFinSet& tubeFins(const EveryKindDesign& design)
{
    return dynamic_cast<TubeFinSet&>(design.component(ComponentKind::TUBE_FIN_SET));
}

TEST_F(TubeFinSetConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    TubeFinSet&                          fins = tubeFins(design);
    const FactoryPanel<TubeFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The rows, named after Java's properties.
    EXPECT_EQ(panel->finCountSpinBox().objectName(), QStringLiteral("FinCount"));
    EXPECT_EQ(panel->lengthWidgets().label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(panel->outerDiameterWidgets().label->text(), QStringLiteral("Outer diameter:"));
    EXPECT_EQ(panel->innerDiameterWidgets().label->text(), QStringLiteral("Inner diameter:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Thickness:"));
    EXPECT_EQ(panel->baseRotationWidgets().label->text(), QStringLiteral("Fin rotation:"));
    EXPECT_EQ(panel->baseRotationWidgets().label->toolTip(),
              QStringLiteral("The angle of the first fin in the fin set."));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(
                  general, {"Length", "OuterRadius", "InnerRadius", "Thickness", "BaseRotation"}),
              "");
    EXPECT_EQ(panel->automaticCheckBox().objectName(), QStringLiteral("OuterRadiusAutomatic"));
    EXPECT_EQ(panel->automaticCheckBox().text(), QStringLiteral("Automatic"));
    EXPECT_TRUE(panel->automaticCheckBox().isEnabled());

    // The values and bounds: 1 to 8 fins, diameters as the radius times 2, at least 0; the fin
    // rotation unbounded.
    const IntBinding& count = *panel->finCountSpinBox().binding();
    EXPECT_EQ(count.minimum(), 1);
    EXPECT_EQ(count.maximum(), 8);
    EXPECT_EQ(count.value(), fins.getFinCount());
    const DoubleBinding& outer = *panel->outerDiameterWidgets().spinBox->binding();
    EXPECT_EQ(outer.getMultiplier(), 2.0);
    EXPECT_EQ(outer.value(), 2 * fins.getOuterRadius());
    EXPECT_TRUE(outer.isAutomaticAvailable());
    EXPECT_EQ(panel->innerDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_TRUE(takesFromZeroUp(panel->lengthWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->outerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->innerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->thicknessWidgets()));
    const DoubleBinding& rotation = *panel->baseRotationWidgets().spinBox->binding();
    EXPECT_EQ(rotation.minimum(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(rotation.maximum(), std::numeric_limits<double>::infinity());

    // The sliders: 0, 2 cm, 10 cm; 0, 4 cm, 20 cm; 0 to the outer diameter; 0 to 1 cm; -pi to pi.
    const SliderMapping& length = panel->lengthWidgets().slider->mapping();
    EXPECT_FALSE(length.isLinear());
    EXPECT_EQ(length.getMin(), 0.0);
    EXPECT_EQ(length.getLinearPosition(), 0.5);
    EXPECT_EQ(length.getMid(), 0.02);
    EXPECT_EQ(length.getMax(), 0.1);
    const SliderMapping& outerSlider = panel->outerDiameterWidgets().slider->mapping();
    EXPECT_FALSE(outerSlider.isLinear());
    EXPECT_EQ(outerSlider.getMid(), 0.04);
    EXPECT_EQ(outerSlider.getMax(), 0.2);
    const SliderMapping& inner = panel->innerDiameterWidgets().slider->mapping();
    EXPECT_TRUE(inner.isLinear());
    EXPECT_EQ(inner.getMax(), outer.value());
    EXPECT_EQ(panel->innerDiameterWidgets().slider->dynamicBounds().max,
              panel->outerDiameterWidgets().spinBox->binding());
    const SliderMapping& thickness = panel->thicknessWidgets().slider->mapping();
    EXPECT_TRUE(thickness.isLinear());
    EXPECT_EQ(thickness.getMax(), 0.01);
    const SliderMapping& rotationSlider = panel->baseRotationWidgets().slider->mapping();
    EXPECT_TRUE(rotationSlider.isLinear());
    EXPECT_EQ(rotationSlider.getMin(), -std::numbers::pi);
    EXPECT_EQ(rotationSlider.getMax(), std::numbers::pi);

    // The fin rotation is a row of the placement panel, below its offset.
    EXPECT_TRUE(panel->placementPanel().isAncestorOf(panel->baseRotationWidgets().spinBox));
    // The material: bulk materials, and the finish of an external component.
    EXPECT_EQ(panel->materialPanel().materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(panel->materialPanel().finishComboBox(), nullptr);
}

TEST_F(TubeFinSetConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    const FactoryPanel<TubeFinSetConfig> panel(*design.document, services, tubeFins(design));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(
        focusOrder(*panel->tabs().widget(0),
                   {"FinCount", "Length", "OuterRadius", "OuterRadiusAutomatic", "InnerRadius",
                    "Thickness", "AxialMethod", "AxialOffset", "BaseRotation"}),
        (std::vector<std::string>{"FinCount", "Length", "OuterRadius", "OuterRadiusAutomatic",
                                  "InnerRadius", "Thickness", "AxialMethod", "AxialOffset",
                                  "BaseRotation"}));
}

TEST_F(TubeFinSetConfigTest, TheMainComponentMustBeATubeFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::LAUNCH_LUG));
    EXPECT_THROW(static_cast<void>(makeChild<TubeFinSetConfig>(&host, *design.document,
                                                               services.services(), selection)),
                 BugError);
}

TEST_F(TubeFinSetConfigTest, AnEditSetsEverySelectedTubeFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    TubeFinSet&                fins = tubeFins(design);
    auto&                      second =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<TubeFinSet>());
    second.setOuterRadius(0.008);
    second.setThickness(0.002);
    const std::array<RocketComponent*, 1> others{&second};
    const FactoryPanel<TubeFinSetConfig>  panel(*design.document, services, fins, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->finCountSpinBox().binding()->setSpinnerValue(4));
    EXPECT_EQ(fins.getFinCount(), 4);
    EXPECT_EQ(second.getFinCount(), 4);
    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("7"));
    EXPECT_DOUBLE_EQ(fins.getLength(), 0.07);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.07);
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("3"));
    EXPECT_DOUBLE_EQ(fins.getOuterRadius(), 0.015);
    EXPECT_DOUBLE_EQ(second.getOuterRadius(), 0.015);
    EXPECT_FALSE(fins.isOuterRadiusAutomatic());
    EXPECT_FALSE(second.isOuterRadiusAutomatic());
    ASSERT_TRUE(panel->thicknessWidgets().spinBox->binding()->commitText("0.3"));
    EXPECT_DOUBLE_EQ(fins.getThickness(), 0.003);
    EXPECT_DOUBLE_EQ(second.getThickness(), 0.003);
    ASSERT_TRUE(panel->baseRotationWidgets().spinBox->binding()->commitText("90"));
    EXPECT_DOUBLE_EQ(fins.getBaseRotation(), std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(second.getBaseRotation(), std::numbers::pi / 2);
    panel->automaticCheckBox().click();
    EXPECT_TRUE(fins.isOuterRadiusAutomatic());
    EXPECT_TRUE(second.isOuterRadiusAutomatic());
    EXPECT_TRUE(panel->automaticCheckBox().isChecked());
}

TEST_F(TubeFinSetConfigTest, TheInnerDiameterSliderFollowsTheOuterDiameter)
{
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    TubeFinSet&                          fins = tubeFins(design);
    const FactoryPanel<TubeFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("5"));
    EXPECT_DOUBLE_EQ(fins.getOuterRadius(), 0.025);
    EXPECT_DOUBLE_EQ(panel->innerDiameterWidgets().slider->mapping().getMax(), 0.05);
    // The fin count changes an automatic outer diameter, and the slider follows it too.
    panel->automaticCheckBox().click();
    ASSERT_TRUE(panel->finCountSpinBox().binding()->setSpinnerValue(2));
    EXPECT_EQ(panel->innerDiameterWidgets().slider->mapping().getMax(), 2 * fins.getOuterRadius());
    EXPECT_EQ(fins.getOuterRadius(), fins.getBodyRadius());
}

TEST_F(TubeFinSetConfigTest, TypedTextReachesTheTubeFinSet)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TubeFinSet&                fins = tubeFins(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<TubeFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Tube Fin Set configuration"));
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    UnitSpinBox& rotation = *panel->baseRotationWidgets().spinBox;
    typeInto(rotation, QStringLiteral("200"));
    QTest::keyClick(&rotation, Qt::Key_Return);
    // The setter reduces the angle to -180 ... 180 degrees.
    EXPECT_DOUBLE_EQ(fins.getBaseRotation(), -160 * std::numbers::pi / 180);
    EXPECT_EQ(rotation.text(), QStringLiteral("-160"));
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(TubeFinSetConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and the count typed but not committed reaches none of the components that are gone
    // (proved under the asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    TubeFinSet&  fins   = tubeFins(design);
    const Uuid   id     = fins.getId();
    const int    count  = fins.getFinCount();
    const double length = fins.getLength();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<TubeFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->lengthWidgets().spinBox, QStringLiteral("77"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(panel->commitAll());
    EXPECT_FALSE(panel->finCountSpinBox().binding()->setSpinnerValue(3));
    panel->updateFields();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const TubeFinSet*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getFinCount(), count);
    EXPECT_EQ(restored->getLength(), length);
}

}  // namespace

}  // namespace QtRocket::Gui
