#include "QtRocket/gui/configdialog/MassComponentConfig.h"

#include <array>
#include <memory>
#include <numbers>

#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MassObjectFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MassComponentConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::tabTitles;
using MassComponentType = MassComponent::MassComponentType;

/// The panel of @p created as a MassComponentConfig.
[[nodiscard]] MassComponentConfig& massPanel(const FactoryPanel& created)
{
    auto* panel = created.as<MassComponentConfig>();
    if (panel == nullptr)
    {
        bug("not a mass component's panel");
    }
    return *panel;
}

/// The texts of the items of @p combo, in order.
[[nodiscard]] QStringList itemTexts(const EnumComboBox& combo)
{
    QStringList texts;
    for (int i = 0; i < combo.count(); ++i)
    {
        texts.append(combo.itemText(i));
    }
    return texts;
}

TEST_F(MassComponentConfigTest, TheTabsAreJavas)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.mass);
    const MassComponentConfig& panel = massPanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Radial position"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel.tabs().tabToolTip(1), QStringLiteral("Radial position configuration"));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    EXPECT_EQ(panel.tabs().widget(1), &panel.radialPosition());
}

TEST_F(MassComponentConfigTest, TheGeneralTabHasJavasFieldsAndNoMaterial)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.mass);
    const MassComponentConfig& panel   = massPanel(created);
    const QWidget&             general = *panel.tabs().widget(0);
    const QStringList          labels  = labelTextsUnder(general);
    // The placement's labels come last (its box is on the right).
    EXPECT_EQ(labels.mid(0, 5),
              (QStringList{QStringLiteral("Mass type:"), QStringLiteral("Mass:"),
                           QStringLiteral("Approximate density:"), QStringLiteral("Length:"),
                           QStringLiteral("Diameter:")}));
    EXPECT_EQ(general.findChild<MaterialPanel*>(), nullptr);
    EXPECT_EQ(general.findChild<PlacementPanel*>(), &panel.placement());
    EXPECT_EQ(panel.massWidgets().spinBox->objectName(), QStringLiteral("ComponentMass"));
    EXPECT_EQ(&panel.massWidgets().spinBox->binding()->getUnitGroup(),
              &unitGroup(UnitGroupId::MASS));
    EXPECT_EQ(panel.massWidgets().slider->mapping().getMid(), 0.05);
    EXPECT_EQ(panel.massWidgets().slider->mapping().getMax(), 0.5);
    EXPECT_EQ(panel.densityWidgets().spinBox->objectName(), QStringLiteral("Density"));
    EXPECT_EQ(&panel.densityWidgets().spinBox->binding()->getUnitGroup(),
              &unitGroup(UnitGroupId::DENSITY_BULK));
    EXPECT_EQ(panel.densityWidgets().slider->mapping().getMin(), 500.0);
    EXPECT_EQ(panel.densityWidgets().slider->mapping().getMid(), 2000.0);
    EXPECT_EQ(panel.densityWidgets().slider->mapping().getMax(), 10000.0);
    EXPECT_EQ(panel.sizeWidgets().diameter.spinBox->objectName(), QStringLiteral("Radius"));
    EXPECT_NE(panel.sizeWidgets().automatic, nullptr);
}

TEST_F(MassComponentConfigTest, TheTypeComboOffersTheEightTypes)
{
    // D2bProbe: the EnumModel of MassComponentConfig's list, its toString()s.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.mass);
    const MassComponentConfig& panel = massPanel(created);
    EXPECT_EQ(panel.typeComboBox().objectName(), QStringLiteral("MassComponentType"));
    EXPECT_EQ(itemTexts(panel.typeComboBox()),
              (QStringList{QStringLiteral("Mass Component"), QStringLiteral("Altimeter"),
                           QStringLiteral("Flight Computer"), QStringLiteral("Deployment Charge"),
                           QStringLiteral("Tracker"), QStringLiteral("Payload"),
                           QStringLiteral("Recovery Hardware"), QStringLiteral("Battery")}));
    EXPECT_EQ(panel.typeComboBox().currentIndex(), 0);
    Q_EMIT panel.typeComboBox().activated(7);
    EXPECT_EQ(design.mass->getMassComponentType(), MassComponentType::BATTERY);
    EXPECT_EQ(panel.typeComboBox().currentIndex(), 7);
}

TEST_F(MassComponentConfigTest, TheDensityGivesTheMassOfTheVolume)
{
    // D2bProbe: density 1000 kg/m3 of the 25 mm by 25 mm component; a negative mass becomes 0.
    const ConfigDialogServices services;
    const MassDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.mass);
    const MassComponentConfig& panel = massPanel(created);
    EXPECT_EQ(panel.massWidgets().spinBox->binding()->value(), 0.0);
    EXPECT_EQ(panel.densityWidgets().spinBox->binding()->value(), 0.0);
    panel.densityWidgets().spinBox->binding()->setValue(1000);
    EXPECT_EQ(design.mass->getComponentMass(), 0.012271846303085131);
    EXPECT_EQ(panel.densityWidgets().spinBox->binding()->value(), 999.9999999999999);
    EXPECT_EQ(panel.massWidgets().spinBox->binding()->value(), 0.012271846303085131);
    panel.massWidgets().spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.mass->getComponentMass(), 0.0);
}

TEST_F(MassComponentConfigTest, TheDensityGivesEachMassComponentItsOwnMass)
{
    // The user's decision U3: Java's setDensity() computes the mass from the MAIN component's
    // volume and forwards that mass, so the other component (20 mm radius, 50 mm long) gets
    // 0.012271846303085131 too, a density of 195.3125 (D2bProbe); here it gets its own.
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<MassComponent>());
    other.setRadius(0.02);
    other.setLength(0.05);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel         created(*design.document, services.services(), *design.mass, others);
    const MassComponentConfig& panel = massPanel(created);
    panel.densityWidgets().spinBox->binding()->setValue(1000);
    EXPECT_EQ(design.mass->getComponentMass(), 0.012271846303085131);
    // MassComponent.getVolume(): pi * r^2 * length, in Java's order.
    const double radiusSquared = 0.02 * 0.02;
    const double volume        = std::numbers::pi * radiusSquared * 0.05;
    EXPECT_EQ(other.getComponentMass(), 1000 * volume);
    EXPECT_DOUBLE_EQ(other.getDensity(), 1000.0);
}

TEST_F(MassComponentConfigTest, EveryFieldReachesEverySelectedMassComponent)
{
    const ConfigDialogServices services;
    const MassDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<MassComponent>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel         created(*design.document, services.services(), *design.mass, others);
    const MassComponentConfig& panel = massPanel(created);
    Q_EMIT panel.typeComboBox().activated(1);
    panel.massWidgets().spinBox->binding()->setValue(0.02);
    EXPECT_EQ(other.getMassComponentType(), MassComponentType::ALTIMETER);
    EXPECT_EQ(other.getComponentMass(), 0.02);
}

}  // namespace

}  // namespace QtRocket::Gui
