#include "QtRocket/gui/configdialog/RadialPositionPanel.h"

#include <array>
#include <cmath>
#include <format>
#include <memory>
#include <numbers>
#include <string>

#include <QLabel>
#include <QObject>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using RadialPositionPanelTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::PanelHost;

TEST_F(RadialPositionPanelTest, ShowsJavasRows)
{
    // MassComponentConfig.positionTab() (ShockCordConfig's, ParachuteConfig's and
    // StreamerConfig's are the same): no tooltips.
    const MassDesign design;
    PanelHost        host(*design.mass);
    const auto&      panel = host.make<RadialPositionPanel>();
    EXPECT_EQ(labelTextsUnder(panel), (QStringList{QStringLiteral("Radial distance:"),
                                                   QStringLiteral("Radial direction:")}));
    const ConfigFields::NumberWidgets& position = panel.positionWidgets();
    EXPECT_EQ(position.spinBox->objectName(), QStringLiteral("RadialPosition"));
    EXPECT_EQ(position.unitSelector->objectName(), QStringLiteral("RadialPosition"));
    EXPECT_EQ(position.slider->objectName(), QStringLiteral("RadialPosition"));
    EXPECT_EQ(position.spinBox->binding()->minimum(), 0.0);
    EXPECT_TRUE(std::isinf(position.spinBox->binding()->maximum()));
    // getSliderModel(0, 0.1, 1.0): quadratic about 10 cm.
    EXPECT_FALSE(position.slider->mapping().isLinear());
    EXPECT_EQ(position.slider->mapping().getMin(), 0.0);
    EXPECT_EQ(position.slider->mapping().getMid(), 0.1);
    EXPECT_EQ(position.slider->mapping().getMax(), 1.0);
    EXPECT_TRUE(position.label->toolTip().isEmpty());
    EXPECT_TRUE(position.spinBox->toolTip().isEmpty());

    const ConfigFields::NumberWidgets& direction = panel.directionWidgets();
    EXPECT_EQ(direction.spinBox->objectName(), QStringLiteral("RadialDirection"));
    EXPECT_TRUE(std::isinf(direction.spinBox->binding()->minimum()));
    EXPECT_TRUE(std::isinf(direction.spinBox->binding()->maximum()));
    EXPECT_TRUE(direction.slider->mapping().isLinear());
    EXPECT_EQ(direction.slider->mapping().getMin(), -std::numbers::pi);
    EXPECT_EQ(direction.slider->mapping().getMax(), std::numbers::pi);

    EXPECT_EQ(panel.resetButton().text(), QStringLiteral("Reset"));
    EXPECT_EQ(panel.resetButton().objectName(), QStringLiteral("resetRadialButton"));
    EXPECT_TRUE(panel.resetButton().toolTip().isEmpty());
}

TEST_F(RadialPositionPanelTest, TheDistanceIsAtLeastZeroAndTheDirectionReduced)
{
    // D2bProbe: setValue(-0.01) gives 0; a direction of 4.0 rad is -2.2831853071795862.
    const MassDesign design;
    PanelHost        host(*design.mass);
    const auto&      panel = host.make<RadialPositionPanel>();
    panel.positionWidgets().spinBox->binding()->setValue(-0.01);
    EXPECT_EQ(design.mass->getRadialPosition(), 0.0);
    panel.positionWidgets().spinBox->binding()->setValue(0.005);
    panel.directionWidgets().spinBox->binding()->setValue(4.0);
    EXPECT_EQ(design.mass->getRadialPosition(), 0.005);
    EXPECT_EQ(design.mass->getRadialDirection(), -2.2831853071795862);
    EXPECT_EQ(panel.directionWidgets().spinBox->binding()->value(), -2.2831853071795862);
}

TEST_F(RadialPositionPanelTest, ResetCentersEverySelectedMassObject)
{
    const MassDesign design;
    auto&            other = design.body->addChild(std::make_unique<MassComponent>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    PanelHost                             host(*design.mass, others);
    const auto&                           panel = host.make<RadialPositionPanel>();
    panel.positionWidgets().spinBox->binding()->setValue(0.004);
    panel.directionWidgets().spinBox->binding()->setValue(0.5);
    EXPECT_EQ(other.getRadialPosition(), 0.004);
    EXPECT_EQ(other.getRadialDirection(), 0.5);
    panel.resetButton().click();
    EXPECT_EQ(design.mass->getRadialPosition(), 0.0);
    EXPECT_EQ(design.mass->getRadialDirection(), 0.0);
    EXPECT_EQ(other.getRadialPosition(), 0.0);
    EXPECT_EQ(other.getRadialDirection(), 0.0);
    EXPECT_EQ(panel.positionWidgets().spinBox->binding()->value(), 0.0);
}

TEST_F(RadialPositionPanelTest, ResetDoesNothingOnceTheSelectionHasEnded)
{
    const MassDesign design;
    PanelHost        host(*design.mass);
    auto&            panel = host.make<RadialPositionPanel>();
    panel.positionWidgets().spinBox->binding()->setValue(0.004);
    host.scope().release();
    panel.resetRadialPosition();
    EXPECT_EQ(design.mass->getRadialPosition(), 0.004);
}

/// How the second tab of @p panel differs from the radial position tab, one line each.
[[nodiscard]] std::string radialTabDifferences(const RocketComponentConfig& panel)
{
    if (panel.tabs().count() < 2)
    {
        return "fewer than two tabs\n";
    }
    std::string differences;
    if (qobject_cast<const RadialPositionPanel*>(panel.tabs().widget(1)) == nullptr)
    {
        differences += "the second tab is no RadialPositionPanel\n";
    }
    if (panel.tabs().tabText(1) != QStringLiteral("Radial position") ||
        panel.tabs().tabToolTip(1) != QStringLiteral("Radial position configuration"))
    {
        differences += std::format("the second tab is '{}' ('{}')\n",
                                   QtStrings::toUtf8(panel.tabs().tabText(1)),
                                   QtStrings::toUtf8(panel.tabs().tabToolTip(1)));
    }
    return differences;
}

TEST_F(RadialPositionPanelTest, EveryMassObjectKindHasItAsItsSecondTab)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         mass(*design.document, services.services(), *design.mass);
    const FactoryPanel         cord(*design.document, services.services(), *design.cord);
    const FactoryPanel         parachute(*design.document, services.services(), *design.parachute);
    const FactoryPanel         streamer(*design.document, services.services(), *design.streamer);
    EXPECT_EQ(radialTabDifferences(mass.panel()), "");
    EXPECT_EQ(radialTabDifferences(cord.panel()), "");
    EXPECT_EQ(radialTabDifferences(parachute.panel()), "");
    EXPECT_EQ(radialTabDifferences(streamer.panel()), "");
}

TEST_F(RadialPositionPanelTest, AComponentThatIsNoMassObjectIsABug)
{
    const MassDesign design;
    PanelHost        host(*design.body);
    EXPECT_THROW(static_cast<void>(&host.make<RadialPositionPanel>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
