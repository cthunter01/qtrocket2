#include "QtRocket/gui/configdialog/RingComponentConfig.h"

#include <array>
#include <memory>

#include <QCoreApplication>
#include <QEvent>
#include <QString>
#include <QTest>
#include <QWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Bulkhead.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RingComponentConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::RingDesign;
using Test::showActiveDialog;
using Test::typeInto;

/// The General tab of the panel of @p main (and @p others).
[[nodiscard]] const RingComponentConfig::GeneralTab& generalOf(const FactoryPanel& panel)
{
    const auto* ring = panel.as<RingComponentConfig>();
    if (ring == nullptr)
    {
        bug("not a ring component's panel");
    }
    return ring->generalTab();
}

TEST_F(RingComponentConfigTest, TheClassDecidesWhichRadiusIsAutomatic)
{
    // D2aProbe: DoubleModel(c, "OuterRadius" / "InnerRadius").isAutomaticAvailable(), true where
    // the class makes set...Automatic() public.
    const RingDesign design;
    EXPECT_FALSE(RingComponentConfig::hasAutomaticOuterRadius(*design.inner));
    EXPECT_FALSE(RingComponentConfig::hasAutomaticInnerRadius(*design.inner));
    EXPECT_TRUE(RingComponentConfig::hasAutomaticOuterRadius(*design.coupler));
    EXPECT_FALSE(RingComponentConfig::hasAutomaticInnerRadius(*design.coupler));
    EXPECT_TRUE(RingComponentConfig::hasAutomaticOuterRadius(*design.block));
    EXPECT_FALSE(RingComponentConfig::hasAutomaticInnerRadius(*design.block));
    EXPECT_TRUE(RingComponentConfig::hasAutomaticOuterRadius(*design.centering));
    EXPECT_TRUE(RingComponentConfig::hasAutomaticInnerRadius(*design.centering));
    EXPECT_TRUE(RingComponentConfig::hasAutomaticOuterRadius(*design.bulkhead));
    EXPECT_FALSE(RingComponentConfig::hasAutomaticInnerRadius(*design.bulkhead));
    EXPECT_FALSE(RingComponentConfig::hasAutomaticOuterRadius(*design.body));

    EXPECT_FALSE(RingComponentConfig::outerRadiusSource(*design.inner).isAutomaticAvailable());
    EXPECT_TRUE(RingComponentConfig::outerRadiusSource(*design.bulkhead).isAutomaticAvailable());
    EXPECT_TRUE(RingComponentConfig::innerRadiusSource(*design.centering).isAutomaticAvailable());
    EXPECT_FALSE(RingComponentConfig::innerRadiusSource(*design.coupler).isAutomaticAvailable());
    EXPECT_TRUE(RingComponentConfig::lengthSource().isWritable());
    EXPECT_FALSE(RingComponentConfig::thicknessSource().isAutomaticAvailable());
}

TEST_F(RingComponentConfigTest, TheFieldsShowTheDiametersOfTheProbe)
{
    // D2aProbe's defaults: the diameter models (multiplier 2) of each ring of a new body tube.
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         coupler(*design.document, services.services(), *design.coupler);
    EXPECT_EQ(generalOf(coupler).length.spinBox->binding()->value(), 0.06);
    EXPECT_EQ(generalOf(coupler).outerRadius.spinBox->binding()->value(), 0.046);
    EXPECT_EQ(generalOf(coupler).innerRadius.spinBox->binding()->value(), 0.046);
    EXPECT_EQ(generalOf(coupler).thickness.spinBox->binding()->value(), 0.0);
    EXPECT_TRUE(generalOf(coupler).outerRadiusAutomatic->isChecked());
    const FactoryPanel block(*design.document, services.services(), *design.block);
    EXPECT_EQ(generalOf(block).length.spinBox->binding()->value(), 0.005);
    EXPECT_EQ(generalOf(block).outerRadius.spinBox->binding()->value(), 0.018);
    EXPECT_EQ(generalOf(block).innerRadius.spinBox->binding()->value(), 0.018);
    const FactoryPanel centering(*design.document, services.services(), *design.centering);
    EXPECT_EQ(generalOf(centering).length.spinBox->binding()->value(), 0.002);
    EXPECT_EQ(generalOf(centering).outerRadius.spinBox->binding()->value(), 0.046);
    EXPECT_EQ(generalOf(centering).innerRadius.spinBox->binding()->value(), 0.019);
    EXPECT_TRUE(generalOf(centering).outerRadiusAutomatic->isChecked());
    EXPECT_TRUE(generalOf(centering).innerRadiusAutomatic->isChecked());
    const FactoryPanel bulkhead(*design.document, services.services(), *design.bulkhead);
    EXPECT_EQ(generalOf(bulkhead).outerRadius.spinBox->binding()->value(), 0.046);
    EXPECT_EQ(generalOf(bulkhead).innerRadius.spinBox, nullptr);
}

TEST_F(RingComponentConfigTest, TheSlidersAreJavas)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         panel(*design.document, services.services(), *design.coupler);
    const RingComponentConfig::GeneralTab& tab = generalOf(panel);
    // getSliderModel(0, 0.1, 1.0) and (0, 0.04, 0.2): quadratic about the middle value.
    const SliderMapping& length = tab.length.slider->mapping();
    EXPECT_FALSE(length.isLinear());
    EXPECT_EQ(length.getMin(), 0.0);
    EXPECT_EQ(length.getMid(), 0.1);
    EXPECT_EQ(length.getMax(), 1.0);
    const SliderMapping& outer = tab.outerRadius.slider->mapping();
    EXPECT_FALSE(outer.isLinear());
    EXPECT_EQ(outer.getMid(), 0.04);
    EXPECT_EQ(outer.getMax(), 0.2);
    // getSliderModel(new DoubleModel(0), od): from 0 to the outer diameter, following it.
    const SliderMapping& inner = tab.innerRadius.slider->mapping();
    EXPECT_TRUE(inner.isLinear());
    EXPECT_EQ(inner.getMin(), 0.0);
    EXPECT_EQ(inner.getMax(), 0.046);
    EXPECT_EQ(tab.innerRadius.slider->dynamicBounds().min, nullptr);
    EXPECT_EQ(tab.innerRadius.slider->dynamicBounds().max, tab.outerRadius.spinBox->binding());
    // getSliderModel(0, 0.01).
    const SliderMapping& thickness = tab.thickness.slider->mapping();
    EXPECT_TRUE(thickness.isLinear());
    EXPECT_EQ(thickness.getMax(), 0.01);
    // At least 0 everywhere.
    EXPECT_EQ(tab.length.spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(tab.outerRadius.spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(tab.innerRadius.spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(tab.thickness.spinBox->binding()->minimum(), 0.0);
}

TEST_F(RingComponentConfigTest, TheInnerSliderEndsAtTheOuterDiameterAsItChanges)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         panel(*design.document, services.services(), *design.centering);
    const RingComponentConfig::GeneralTab& tab = generalOf(panel);
    ASSERT_TRUE(tab.outerRadius.spinBox->binding()->commitText("5"));
    EXPECT_DOUBLE_EQ(design.centering->getOuterRadius(), 0.025);
    EXPECT_DOUBLE_EQ(tab.innerRadius.slider->mapping().getMax(), 0.05);
}

TEST_F(RingComponentConfigTest, AnEditReachesEverySelectedRing)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<TubeCoupler>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel panel(*design.document, services.services(), *design.coupler, others);
    const RingComponentConfig::GeneralTab& tab = generalOf(panel);
    // A diameter typed sets the radius and switches "Automatic" off on both (the core's setter).
    ASSERT_TRUE(tab.outerRadius.spinBox->binding()->commitText("4"));
    EXPECT_DOUBLE_EQ(design.coupler->getOuterRadius(), 0.02);
    EXPECT_DOUBLE_EQ(other.getOuterRadius(), 0.02);
    EXPECT_FALSE(design.coupler->isOuterRadiusAutomatic());
    EXPECT_FALSE(other.isOuterRadiusAutomatic());
    EXPECT_FALSE(tab.outerRadiusAutomatic->isChecked());
    // "Automatic" again: both take the parent's inner radius.
    tab.outerRadiusAutomatic->click();
    EXPECT_TRUE(design.coupler->isOuterRadiusAutomatic());
    EXPECT_TRUE(other.isOuterRadiusAutomatic());
    EXPECT_TRUE(tab.outerRadiusAutomatic->isChecked());
    EXPECT_EQ(design.coupler->getOuterRadius(), 0.023);
    // The length and the thickness.
    ASSERT_TRUE(tab.length.spinBox->binding()->commitText("8"));
    EXPECT_DOUBLE_EQ(design.coupler->getLength(), 0.08);
    EXPECT_DOUBLE_EQ(other.getLength(), 0.08);
    ASSERT_TRUE(tab.thickness.spinBox->binding()->commitText("0.2"));
    EXPECT_DOUBLE_EQ(design.coupler->getThickness(), 0.002);
    EXPECT_DOUBLE_EQ(other.getThickness(), 0.002);
    // Negative values are 0 (DoubleModel's min 0).
    ASSERT_TRUE(tab.length.spinBox->binding()->commitText("-3"));
    EXPECT_EQ(design.coupler->getLength(), 0.0);
}

TEST_F(RingComponentConfigTest, TheInnerDiameterKeepsEachRingsOwnOuterRadius)
{
    // U3 (ComponentSelection.h, item 4): Java's ThicknessRingComponent.setInnerRadius() gives the
    // others the thickness from the MAIN's outer radius; here each ring keeps its own outer radius
    // and gets the inner radius typed.
    const ConfigDialogServices services;
    const RingDesign           design;
    design.coupler->setOuterRadius(0.02);
    auto& other = design.body->addChild(std::make_unique<TubeCoupler>());
    other.setOuterRadius(0.015);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel panel(*design.document, services.services(), *design.coupler, others);
    const RingComponentConfig::GeneralTab& tab = generalOf(panel);
    ASSERT_TRUE(tab.innerRadius.spinBox->binding()->commitText("2"));
    EXPECT_DOUBLE_EQ(design.coupler->getInnerRadius(), 0.01);
    EXPECT_DOUBLE_EQ(other.getInnerRadius(), 0.01);
    EXPECT_DOUBLE_EQ(design.coupler->getThickness(), 0.01);
    EXPECT_DOUBLE_EQ(other.getThickness(), 0.005);
}

TEST_F(RingComponentConfigTest, TheInnerAutomaticOfCenteringRingsReachesEveryRing)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<CenteringRing>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel panel(*design.document, services.services(), *design.centering, others);
    const RingComponentConfig::GeneralTab& tab = generalOf(panel);
    ASSERT_NE(tab.innerRadiusAutomatic, nullptr);
    tab.innerRadiusAutomatic->click();
    EXPECT_FALSE(design.centering->isInnerRadiusAutomatic());
    EXPECT_FALSE(other.isInnerRadiusAutomatic());
    tab.innerRadiusAutomatic->click();
    EXPECT_TRUE(design.centering->isInnerRadiusAutomatic());
    EXPECT_TRUE(other.isInnerRadiusAutomatic());
    tab.outerRadiusAutomatic->click();
    EXPECT_FALSE(design.centering->isOuterRadiusAutomatic());
    EXPECT_FALSE(other.isOuterRadiusAutomatic());
}

TEST_F(RingComponentConfigTest, TheGeneralTabOfAComponentThatIsNoRingIsABug)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    host;
    RingComponentConfig        panel(*design.document, services.services(),
                                     std::make_shared<ComponentSelection>(*design.body), &host);
    EXPECT_THROW(static_cast<void>(RingComponentConfig::makeGeneralTab(panel, {})), BugError);
}

TEST_F(RingComponentConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingEdit)
{
    // The undo replaces every component object: the dialog closes inside the event, and the
    // pending diameter, committed by the focus loss of the hidden dialog, reaches none of the
    // rings that are gone (proved under the asan preset).
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.bulkhead->setName("Sealed");
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.centering);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const RingComponentConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    UnitSpinBox& outer = *panel->generalTab().outerRadius.spinBox;
    typeInto(outer, QStringLiteral("7"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(dialog->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body =
        *design.document->getRocket().getChildren().front()->getChildren().front();
    const auto& centering = dynamic_cast<const CenteringRing&>(*body.getChildren().at(2));
    EXPECT_TRUE(centering.isOuterRadiusAutomatic());
    EXPECT_EQ(body.getChildren().at(3)->getName(), "Bulkhead");
}

}  // namespace

}  // namespace QtRocket::Gui
