#include "QtRocket/gui/configdialog/BulkheadConfig.h"

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
#include "QtRocket/rocket/Bulkhead.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using BulkheadConfigTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::RingDesign;
using Test::rowLabels;
using Test::tabTitles;

TEST_F(BulkheadConfigTest, ABulkheadHasItsThicknessAndDiameterOnly)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.bulkhead);
    const auto*                panel = created.as<BulkheadConfig>();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(tabTitles(*panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                              QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    const RingComponentConfig::GeneralTab& tab = panel->generalTab();
    EXPECT_EQ(rowLabels(tab), (QStringList{QStringLiteral("Thickness:"),
                                           QStringLiteral("Diameter:"), QString(), QString()}));
    EXPECT_EQ(tab.innerRadius.spinBox, nullptr);
    EXPECT_EQ(tab.thickness.spinBox, nullptr);
    EXPECT_EQ(tab.innerRadiusAutomatic, nullptr);
    ASSERT_NE(tab.outerRadiusAutomatic, nullptr);
    EXPECT_TRUE(tab.outerRadiusAutomatic->isChecked());
    ASSERT_NE(tab.material, nullptr);
    EXPECT_EQ(tab.material->materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(tab.placement, nullptr);
}

TEST_F(BulkheadConfigTest, TheDiameterAndTheThicknessAreTheBulkheads)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.bulkhead);
    const auto*                panel = created.as<BulkheadConfig>();
    ASSERT_NE(panel, nullptr);
    const RingComponentConfig::GeneralTab& tab      = panel->generalTab();
    const std::shared_ptr<DoubleBinding>&  diameter = tab.outerRadius.spinBox->binding();
    const std::shared_ptr<DoubleBinding>&  length   = tab.length.spinBox->binding();
    ASSERT_TRUE(diameter->commitText("3"));
    EXPECT_DOUBLE_EQ(design.bulkhead->getOuterRadius(), 0.015);
    EXPECT_FALSE(design.bulkhead->isOuterRadiusAutomatic());
    ASSERT_TRUE(length->commitText("0.5"));
    EXPECT_DOUBLE_EQ(design.bulkhead->getLength(), 0.005);
    tab.outerRadiusAutomatic->click();
    EXPECT_TRUE(design.bulkhead->isOuterRadiusAutomatic());
    EXPECT_EQ(design.bulkhead->getOuterRadius(), 0.023);
}

}  // namespace

}  // namespace QtRocket::Gui
