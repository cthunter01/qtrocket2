#include "QtRocket/gui/configdialog/LaunchLugConfig.h"

#include <array>
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
#include "QtRocket/gui/configdialog/InstancesPanel.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using LaunchLugConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::focusOrder;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::takesFromZeroUp;
using Test::typeInto;

/// The launch lug of @p design.
[[nodiscard]] LaunchLug& launchLug(const EveryKindDesign& design)
{
    return dynamic_cast<LaunchLug&>(design.component(ComponentKind::LAUNCH_LUG));
}

TEST_F(LaunchLugConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices          services;
    const EveryKindDesign               design;
    LaunchLug&                          lug = launchLug(design);
    const FactoryPanel<LaunchLugConfig> panel(*design.document, services, lug);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The rows, named after Java's properties.
    EXPECT_EQ(panel->lengthWidgets().label->text(), QStringLiteral("Length:"));
    EXPECT_EQ(panel->outerDiameterWidgets().label->text(), QStringLiteral("Outer diameter:"));
    EXPECT_EQ(panel->innerDiameterWidgets().label->text(), QStringLiteral("Inner diameter:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Thickness:"));
    EXPECT_EQ(panel->angleWidgets().label->text(), QStringLiteral("Rotation:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"Length", "OuterRadius", "InnerRadius", "Thickness",
                                          "AngleOffset", "InstanceSeparation"}),
              "");
    EXPECT_EQ(panel->instancesPanel().countSpinBox().objectName(), QStringLiteral("InstanceCount"));

    // The values and bounds: diameters as the radius times 2, at least 0; the rotation -pi to pi.
    const DoubleBinding& outer = *panel->outerDiameterWidgets().spinBox->binding();
    EXPECT_EQ(outer.getMultiplier(), 2.0);
    EXPECT_EQ(outer.value(), 2 * lug.getOuterRadius());
    EXPECT_FALSE(outer.isAutomaticAvailable());
    EXPECT_EQ(panel->innerDiameterWidgets().spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_TRUE(takesFromZeroUp(panel->lengthWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->outerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->innerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->thicknessWidgets()));
    const DoubleBinding& angle = *panel->angleWidgets().spinBox->binding();
    EXPECT_EQ(angle.minimum(), -std::numbers::pi);
    EXPECT_EQ(angle.maximum(), std::numbers::pi);
    const IntBinding& count = *panel->instancesPanel().countSpinBox().binding();
    EXPECT_EQ(count.minimum(), 1);

    // The sliders: 0, 2 cm, 10 cm; 0, 4 cm, 20 cm; 0 to the outer diameter; 0 to 1 cm; -pi to pi.
    const SliderMapping& length = panel->lengthWidgets().slider->mapping();
    EXPECT_FALSE(length.isLinear());
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
    EXPECT_EQ(panel->thicknessWidgets().slider->mapping().getMax(), 0.01);
    const SliderMapping& angleSlider = panel->angleWidgets().slider->mapping();
    EXPECT_TRUE(angleSlider.isLinear());
    EXPECT_EQ(angleSlider.getMin(), -std::numbers::pi);
    EXPECT_EQ(angleSlider.getMax(), std::numbers::pi);

    // The instances below the fields; the rotation a row of the placement panel; the material.
    EXPECT_TRUE(general.isAncestorOf(&panel->instancesPanel()));
    EXPECT_TRUE(panel->placementPanel().isAncestorOf(panel->angleWidgets().spinBox));
    EXPECT_EQ(panel->materialPanel().materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(panel->materialPanel().finishComboBox(), nullptr);
}

TEST_F(LaunchLugConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices          services;
    const EveryKindDesign               design;
    const FactoryPanel<LaunchLugConfig> panel(*design.document, services, launchLug(design));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(focusOrder(*panel->tabs().widget(0),
                         {"Length", "OuterRadius", "InnerRadius", "Thickness", "InstanceCount",
                          "InstanceSeparation", "AxialMethod", "AxialOffset", "AngleOffset"}),
              (std::vector<std::string>{"Length", "OuterRadius", "InnerRadius", "Thickness",
                                        "InstanceCount", "InstanceSeparation", "AxialMethod",
                                        "AxialOffset", "AngleOffset"}));
}

TEST_F(LaunchLugConfigTest, TheMainComponentMustBeALaunchLug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::RAIL_BUTTON));
    EXPECT_THROW(static_cast<void>(makeChild<LaunchLugConfig>(&host, *design.document,
                                                              services.services(), selection)),
                 BugError);
}

TEST_F(LaunchLugConfigTest, AnEditSetsEverySelectedLaunchLug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    LaunchLug&                 lug = launchLug(design);
    auto&                      second =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<LaunchLug>());
    const std::array<RocketComponent*, 1> others{&second};
    const FactoryPanel<LaunchLugConfig>   panel(*design.document, services, lug, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->lengthWidgets().spinBox->binding()->commitText("5"));
    EXPECT_DOUBLE_EQ(lug.getLength(), 0.05);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.05);
    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("1.6"));
    EXPECT_DOUBLE_EQ(lug.getOuterRadius(), 0.008);
    EXPECT_DOUBLE_EQ(second.getOuterRadius(), 0.008);
    ASSERT_TRUE(panel->thicknessWidgets().spinBox->binding()->commitText("0.2"));
    EXPECT_DOUBLE_EQ(lug.getThickness(), 0.002);
    EXPECT_DOUBLE_EQ(second.getThickness(), 0.002);
    ASSERT_TRUE(panel->angleWidgets().spinBox->binding()->commitText("90"));
    EXPECT_DOUBLE_EQ(lug.getAngleOffset(), std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(second.getAngleOffset(), std::numbers::pi / 2);
    IntBinding& count = *panel->instancesPanel().countSpinBox().binding();
    ASSERT_TRUE(count.setSpinnerValue(2));
    EXPECT_EQ(lug.getInstanceCount(), 2);
    EXPECT_EQ(second.getInstanceCount(), 2);
    ASSERT_TRUE(panel->instancesPanel().separationWidgets().spinBox->binding()->commitText("8"));
    EXPECT_DOUBLE_EQ(lug.getInstanceSeparation(), 0.08);
    EXPECT_DOUBLE_EQ(second.getInstanceSeparation(), 0.08);
}

TEST_F(LaunchLugConfigTest, TheRotationIsClampedToItsBounds)
{
    // Java: DoubleModel(component, "AngleOffset", UNITS_ANGLE, -Math.PI, Math.PI).
    const ConfigDialogServices          services;
    const EveryKindDesign               design;
    LaunchLug&                          lug = launchLug(design);
    const FactoryPanel<LaunchLugConfig> panel(*design.document, services, lug);
    ASSERT_NE(panel.get(), nullptr);
    const UnitSpinBox& angle = *panel->angleWidgets().spinBox;
    ASSERT_TRUE(angle.binding()->commitText("-45"));
    EXPECT_DOUBLE_EQ(lug.getAngleOffset(), -std::numbers::pi / 4);
    ASSERT_TRUE(angle.binding()->commitText("-300"));
    EXPECT_EQ(lug.getAngleOffset(), -std::numbers::pi);
    EXPECT_EQ(angle.text(), QStringLiteral("-180"));
}

TEST_F(LaunchLugConfigTest, TypedTextReachesTheLaunchLug)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    LaunchLug&                 lug = launchLug(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, lug);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<LaunchLugConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Launch Lug configuration"));
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    UnitSpinBox& inner = *panel->innerDiameterWidgets().spinBox;
    typeInto(inner, QStringLiteral("1"));
    QTest::keyClick(&inner, Qt::Key_Return);
    // The wall stays and the outer radius follows (LaunchLug::setInnerRadius()).
    EXPECT_DOUBLE_EQ(lug.getInnerRadius(), 0.005);
    EXPECT_DOUBLE_EQ(lug.getOuterRadius(), 0.005 + lug.getThickness());
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(LaunchLugConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and the length typed but not committed reaches none of the components that are gone
    // (proved under the asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    LaunchLug&   lug    = launchLug(design);
    const Uuid   id     = lug.getId();
    const double length = lug.getLength();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, lug);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<LaunchLugConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->lengthWidgets().spinBox, QStringLiteral("77"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(panel->commitAll());
    IntBinding& count = *panel->instancesPanel().countSpinBox().binding();
    EXPECT_FALSE(count.setSpinnerValue(3));
    panel->updateFields();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const LaunchLug*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getLength(), length);
    EXPECT_EQ(restored->getInstanceCount(), 1);
}

}  // namespace

}  // namespace QtRocket::Gui
