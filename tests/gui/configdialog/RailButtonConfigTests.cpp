#include "QtRocket/gui/configdialog/RailButtonConfig.h"

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
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RailButtonConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::focusOrder;
using Test::hasLinearSlider;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::takesFromZeroUp;
using Test::typeInto;

/// The rail button of @p design.
[[nodiscard]] RailButton& railButton(const EveryKindDesign& design)
{
    return dynamic_cast<RailButton&>(design.component(ComponentKind::RAIL_BUTTON));
}

TEST_F(RailButtonConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    RailButton&                          button = railButton(design);
    const FactoryPanel<RailButtonConfig> panel(*design.document, services, button);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The rows, named after Java's properties.
    EXPECT_EQ(panel->outerDiameterWidgets().label->text(), QStringLiteral("Outer Diameter:"));
    EXPECT_EQ(panel->innerDiameterWidgets().label->text(), QStringLiteral("Inner Diameter:"));
    EXPECT_EQ(panel->baseHeightWidgets().label->text(), QStringLiteral("Base Height:"));
    EXPECT_EQ(panel->flangeHeightWidgets().label->text(), QStringLiteral("Flange Height:"));
    EXPECT_EQ(panel->totalHeightWidgets().label->text(),
              QStringLiteral("Total Height (excl. screw):"));
    EXPECT_EQ(panel->screwHeightWidgets().label->text(), QStringLiteral("Screw Height:"));
    EXPECT_EQ(panel->angleWidgets().label->text(), QStringLiteral("Rotation:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(
                  general, {"OuterDiameter", "InnerDiameter", "BaseHeight", "FlangeHeight",
                            "TotalHeight", "ScrewHeight", "AngleOffset", "InstanceSeparation"}),
              "");
    EXPECT_EQ(panel->instancesPanel().countSpinBox().objectName(), QStringLiteral("InstanceCount"));

    // The values and bounds: each at least 0, the diameters as they are (no multiplier); the
    // rotation's bounds are OpenRocket's -180 and 180 radians.
    EXPECT_EQ(panel->outerDiameterWidgets().spinBox->binding()->getMultiplier(), 1.0);
    EXPECT_EQ(panel->innerDiameterWidgets().spinBox->binding()->getMultiplier(), 1.0);
    EXPECT_TRUE(takesFromZeroUp(panel->outerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->innerDiameterWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->baseHeightWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->flangeHeightWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->totalHeightWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->screwHeightWidgets()));
    EXPECT_EQ(panel->outerDiameterWidgets().spinBox->binding()->value(), button.getOuterDiameter());
    const DoubleBinding& angle = *panel->angleWidgets().spinBox->binding();
    EXPECT_EQ(angle.minimum(), -180.0);
    EXPECT_EQ(angle.maximum(), 180.0);

    // The sliders: 0 to 2 cm, but the base and flange heights' up to what the other heights
    // leave, and the total height's from the base and flange heights; the rotation's -pi to pi.
    EXPECT_TRUE(hasLinearSlider(panel->outerDiameterWidgets(), 0.0, 0.02));
    EXPECT_TRUE(hasLinearSlider(panel->innerDiameterWidgets(), 0.0, 0.02));
    EXPECT_TRUE(hasLinearSlider(panel->screwHeightWidgets(), 0.0, 0.02));
    const SliderMapping& base = panel->baseHeightWidgets().slider->mapping();
    EXPECT_TRUE(base.isLinear());
    EXPECT_EQ(base.getMin(), 0.0);
    EXPECT_EQ(base.getMax(), button.getMaxBaseHeight());
    EXPECT_NE(panel->baseHeightWidgets().slider->dynamicBounds().max, nullptr);
    EXPECT_EQ(panel->baseHeightWidgets().slider->dynamicBounds().min, nullptr);
    const SliderMapping& flange = panel->flangeHeightWidgets().slider->mapping();
    EXPECT_EQ(flange.getMax(), button.getMaxFlangeHeight());
    EXPECT_NE(panel->flangeHeightWidgets().slider->dynamicBounds().max, nullptr);
    const SliderMapping& total = panel->totalHeightWidgets().slider->mapping();
    EXPECT_TRUE(total.isLinear());
    EXPECT_EQ(total.getMin(), button.getMinTotalHeight());
    EXPECT_EQ(total.getMax(), 0.02);
    EXPECT_NE(panel->totalHeightWidgets().slider->dynamicBounds().min, nullptr);
    EXPECT_EQ(panel->totalHeightWidgets().slider->dynamicBounds().max, nullptr);
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

TEST_F(RailButtonConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    const FactoryPanel<RailButtonConfig> panel(*design.document, services, railButton(design));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(focusOrder(*panel->tabs().widget(0),
                         {"OuterDiameter", "InnerDiameter", "BaseHeight", "FlangeHeight",
                          "TotalHeight", "ScrewHeight", "InstanceCount", "InstanceSeparation",
                          "AxialMethod", "AxialOffset", "AngleOffset"}),
              (std::vector<std::string>{"OuterDiameter", "InnerDiameter", "BaseHeight",
                                        "FlangeHeight", "TotalHeight", "ScrewHeight",
                                        "InstanceCount", "InstanceSeparation", "AxialMethod",
                                        "AxialOffset", "AngleOffset"}));
}

TEST_F(RailButtonConfigTest, TheMainComponentMustBeARailButton)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::TUBE_FIN_SET));
    EXPECT_THROW(static_cast<void>(makeChild<RailButtonConfig>(&host, *design.document,
                                                               services.services(), selection)),
                 BugError);
}

TEST_F(RailButtonConfigTest, TheHeightSlidersFollowTheirBounds)
{
    // Java: getSliderModel(0, DoubleModel(component, "MaxBaseHeight")), the same for the flange,
    // and getSliderModel(DoubleModel(component, "MinTotalHeight"), 0.02).
    const ConfigDialogServices           services;
    const EveryKindDesign                design;
    RailButton&                          button = railButton(design);
    const FactoryPanel<RailButtonConfig> panel(*design.document, services, button);
    ASSERT_NE(panel.get(), nullptr);
    ASSERT_TRUE(panel->totalHeightWidgets().spinBox->binding()->commitText("1.5"));
    EXPECT_DOUBLE_EQ(button.getTotalHeight(), 0.015);
    EXPECT_EQ(panel->baseHeightWidgets().slider->mapping().getMax(), button.getMaxBaseHeight());
    EXPECT_EQ(panel->flangeHeightWidgets().slider->mapping().getMax(), button.getMaxFlangeHeight());
    ASSERT_TRUE(panel->flangeHeightWidgets().spinBox->binding()->commitText("0.5"));
    EXPECT_DOUBLE_EQ(button.getFlangeHeight(), 0.005);
    EXPECT_EQ(panel->baseHeightWidgets().slider->mapping().getMax(), button.getMaxBaseHeight());
    EXPECT_EQ(panel->totalHeightWidgets().slider->mapping().getMin(), button.getMinTotalHeight());
    // A base height beyond what the others leave is clamped by the setter.
    ASSERT_TRUE(panel->baseHeightWidgets().spinBox->binding()->commitText("5"));
    EXPECT_EQ(button.getBaseHeight(), button.getMaxBaseHeight());
    EXPECT_EQ(panel->baseHeightWidgets().slider->value(), SliderMapping::kMaximum);
}

TEST_F(RailButtonConfigTest, AnEditSetsEverySelectedRailButton)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    RailButton&                button = railButton(design);
    auto&                      second =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<RailButton>());
    const std::array<RocketComponent*, 1> others{&second};
    const FactoryPanel<RailButtonConfig>  panel(*design.document, services, button, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->outerDiameterWidgets().spinBox->binding()->commitText("1.2"));
    EXPECT_DOUBLE_EQ(button.getOuterDiameter(), 0.012);
    EXPECT_DOUBLE_EQ(second.getOuterDiameter(), 0.012);
    ASSERT_TRUE(panel->innerDiameterWidgets().spinBox->binding()->commitText("0.7"));
    EXPECT_DOUBLE_EQ(button.getInnerDiameter(), 0.007);
    EXPECT_DOUBLE_EQ(second.getInnerDiameter(), 0.007);
    ASSERT_TRUE(panel->totalHeightWidgets().spinBox->binding()->commitText("1.4"));
    EXPECT_DOUBLE_EQ(button.getTotalHeight(), 0.014);
    EXPECT_DOUBLE_EQ(second.getTotalHeight(), 0.014);
    ASSERT_TRUE(panel->baseHeightWidgets().spinBox->binding()->commitText("0.3"));
    EXPECT_DOUBLE_EQ(button.getBaseHeight(), 0.003);
    EXPECT_DOUBLE_EQ(second.getBaseHeight(), 0.003);
    ASSERT_TRUE(panel->flangeHeightWidgets().spinBox->binding()->commitText("0.25"));
    EXPECT_DOUBLE_EQ(button.getFlangeHeight(), 0.0025);
    EXPECT_DOUBLE_EQ(second.getFlangeHeight(), 0.0025);
    ASSERT_TRUE(panel->screwHeightWidgets().spinBox->binding()->commitText("0.4"));
    EXPECT_DOUBLE_EQ(button.getScrewHeight(), 0.004);
    EXPECT_DOUBLE_EQ(second.getScrewHeight(), 0.004);
    ASSERT_TRUE(panel->angleWidgets().spinBox->binding()->commitText("-90"));
    EXPECT_DOUBLE_EQ(button.getAngleOffset(), -std::numbers::pi / 2);
    EXPECT_DOUBLE_EQ(second.getAngleOffset(), -std::numbers::pi / 2);
    IntBinding& count = *panel->instancesPanel().countSpinBox().binding();
    ASSERT_TRUE(count.setSpinnerValue(3));
    EXPECT_EQ(button.getInstanceCount(), 3);
    EXPECT_EQ(second.getInstanceCount(), 3);
}

TEST_F(RailButtonConfigTest, TypedTextReachesTheRailButton)
{
    // OpenRocket's bounds of -180 and 180 radians (kept) pass 200 degrees on to the setter, which
    // clamps it to pi.
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    RailButton&                button = railButton(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, button);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<RailButtonConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Rail Button configuration"));
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    ASSERT_TRUE(panel->angleWidgets().spinBox->binding()->commitText("0"));
    UnitSpinBox& angle = *panel->angleWidgets().spinBox;
    typeInto(angle, QStringLiteral("200"));
    QTest::keyClick(&angle, Qt::Key_Return);
    EXPECT_EQ(button.getAngleOffset(), std::numbers::pi);
    EXPECT_EQ(angle.text(), QStringLiteral("180"));
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(RailButtonConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and the height typed but not committed reaches none of the components that are gone
    // (proved under the asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    RailButton&  button = railButton(design);
    const Uuid   id     = button.getId();
    const double height = button.getTotalHeight();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, button);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<RailButtonConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->totalHeightWidgets().spinBox, QStringLiteral("1.9"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(panel->commitAll());
    panel->updateFields();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const RailButton*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getTotalHeight(), height);
}

}  // namespace

}  // namespace QtRocket::Gui
