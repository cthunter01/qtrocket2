#include "QtRocket/gui/configdialog/ThicknessRingComponentConfig.h"

#include <QString>
#include <QStringList>
#include <QTabWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ThicknessRingComponentConfigTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::RingDesign;
using Test::rowLabels;
using Test::tabTitles;

TEST_F(ThicknessRingComponentConfigTest, ATubeCouplerHasEveryRow)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.coupler);
    const auto*                panel = created.as<ThicknessRingComponentConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);
    const RingComponentConfig::GeneralTab& tab = panel->generalTab();
    EXPECT_EQ(panel->tabs().widget(0), tab.page);
    EXPECT_EQ(rowLabels(tab),
              (QStringList{QStringLiteral("Length:"), QStringLiteral("Outer diameter:"),
                           QStringLiteral("Inner diameter:"), QStringLiteral("Wall thickness:")}));
    EXPECT_EQ(tab.length.spinBox->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(tab.outerRadius.unitSelector->objectName(), QStringLiteral("OuterRadius"));
    EXPECT_EQ(tab.innerRadius.slider->objectName(), QStringLiteral("InnerRadius"));
    EXPECT_EQ(tab.thickness.spinBox->objectName(), QStringLiteral("Thickness"));
    // "Automatic" for the outer radius only (Java: TubeCoupler makes only
    // setOuterRadiusAutomatic() public).
    ASSERT_NE(tab.outerRadiusAutomatic, nullptr);
    EXPECT_EQ(tab.outerRadiusAutomatic->text(), QStringLiteral("Automatic"));
    EXPECT_EQ(tab.outerRadiusAutomatic->objectName(), QStringLiteral("OuterRadiusAutomatic"));
    EXPECT_EQ(tab.outerRadiusAutomatic->toolTip(),
              QStringLiteral("Matches outer diameter to the inner diameter of the parent "
                             "component."));
    EXPECT_EQ(tab.innerRadiusAutomatic, nullptr);
    ASSERT_NE(tab.placement, nullptr);
    ASSERT_NE(tab.material, nullptr);
    EXPECT_EQ(tab.material->materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_EQ(tab.material->finishComboBox(), nullptr);
}

TEST_F(ThicknessRingComponentConfigTest, AnEngineBlockHasTheSameRows)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.block);
    const auto*                panel = created.as<ThicknessRingComponentConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(rowLabels(panel->generalTab()),
              (QStringList{QStringLiteral("Length:"), QStringLiteral("Outer diameter:"),
                           QStringLiteral("Inner diameter:"), QStringLiteral("Wall thickness:")}));
    EXPECT_NE(panel->generalTab().outerRadiusAutomatic, nullptr);
    EXPECT_EQ(panel->generalTab().innerRadiusAutomatic, nullptr);
    // The engine block sits in the inner tube: "Automatic" gives it the tube's inner diameter.
    EXPECT_TRUE(panel->generalTab().outerRadiusAutomatic->isChecked());
}

}  // namespace

}  // namespace QtRocket::Gui
