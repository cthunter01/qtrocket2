#include "QtRocket/gui/configdialog/CenteringRingConfig.h"

#include <memory>

#include <QString>
#include <QStringList>
#include <QTabWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/material/Material.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using CenteringRingConfigTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::RingDesign;
using Test::rowLabels;
using Test::tabTitles;

TEST_F(CenteringRingConfigTest, ACenteringRingHasBothDiametersAutomaticAndNoWall)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.centering);
    const auto*                panel = created.as<CenteringRingConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    const RingComponentConfig::GeneralTab& tab = panel->generalTab();
    EXPECT_EQ(rowLabels(tab),
              (QStringList{QStringLiteral("Thickness:"), QStringLiteral("Outer diameter:"),
                           QStringLiteral("Inner diameter:"), QString()}));
    EXPECT_EQ(tab.thickness.spinBox, nullptr);
    ASSERT_NE(tab.outerRadiusAutomatic, nullptr);
    ASSERT_NE(tab.innerRadiusAutomatic, nullptr);
    EXPECT_EQ(tab.innerRadiusAutomatic->text(), QStringLiteral("Automatic"));
    EXPECT_EQ(tab.innerRadiusAutomatic->objectName(), QStringLiteral("InnerRadiusAutomatic"));
    EXPECT_EQ(tab.innerRadiusAutomatic->toolTip(),
              QStringLiteral("Matches inner diameter to the outer diameter of the child inner "
                             "tube."));
    ASSERT_NE(tab.material, nullptr);
    EXPECT_EQ(tab.material->materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(tab.placement, nullptr);
}

TEST_F(CenteringRingConfigTest, TheAutomaticInnerDiameterIsTheInnerTubes)
{
    // D2aProbe: the centering ring beside the default inner tube has the tube's outer diameter
    // as its inner one (0.019).
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.centering);
    const auto*                panel = created.as<CenteringRingConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_TRUE(panel->generalTab().innerRadiusAutomatic->isChecked());
    const std::shared_ptr<DoubleBinding>& inner =
        panel->generalTab().innerRadius.spinBox->binding();
    EXPECT_EQ(inner->value(), 0.019);
}

}  // namespace

}  // namespace QtRocket::Gui
