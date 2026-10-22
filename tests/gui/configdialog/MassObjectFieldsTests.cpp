#include "QtRocket/gui/configdialog/MassObjectFields.h"

#include <array>
#include <memory>

#include <QGridLayout>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using MassObjectFieldsTest = Test::GuiTest;
using Test::labelTextsUnder;
using Test::MassDesign;
using Test::PanelHost;

/// The size rows of @p host's selection in a grid of their own under the host's root.
struct SizeRows
{
    QWidget*                      page = nullptr;
    MassObjectFields::SizeWidgets widgets;
};

[[nodiscard]] SizeRows makeSizeRows(PanelHost& host)
{
    SizeRows rows;
    rows.page    = makeChild<QWidget>(&host.root());
    auto* grid   = makeChild<QGridLayout>(rows.page);
    rows.widgets = MassObjectFields::addSizeRows(host.scope(), *grid, 0, QStringLiteral("Length:"),
                                                 QStringLiteral("Diameter:"));
    return rows;
}

TEST_F(MassObjectFieldsTest, ShowsTheLengthTheDiameterAndItsAutomatic)
{
    const MassDesign                     design;
    PanelHost                            host(*design.mass);
    const SizeRows                       rows    = makeSizeRows(host);
    const MassObjectFields::SizeWidgets& widgets = rows.widgets;
    EXPECT_EQ(labelTextsUnder(*rows.page),
              (QStringList{QStringLiteral("Length:"), QStringLiteral("Diameter:")}));
    EXPECT_EQ(widgets.length.spinBox->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(widgets.length.unitSelector->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(widgets.length.spinBox->binding()->minimum(), 0.0);
    // getSliderModel(0, 0.1, 0.5) and (0, 0.04, 0.2).
    EXPECT_EQ(widgets.length.slider->mapping().getMid(), 0.1);
    EXPECT_EQ(widgets.length.slider->mapping().getMax(), 0.5);
    EXPECT_EQ(widgets.diameter.spinBox->objectName(), QStringLiteral("Radius"));
    EXPECT_EQ(widgets.diameter.spinBox->binding()->getMultiplier(), 2.0);
    EXPECT_EQ(widgets.diameter.spinBox->binding()->minimum(), 0.0);
    EXPECT_EQ(widgets.diameter.slider->mapping().getMid(), 0.04);
    EXPECT_EQ(widgets.diameter.slider->mapping().getMax(), 0.2);
    ASSERT_NE(widgets.automatic, nullptr);
    EXPECT_EQ(widgets.automatic->objectName(), QStringLiteral("RadiusAutomatic"));
    EXPECT_EQ(widgets.automatic->text(), QStringLiteral("Automatic"));
    EXPECT_EQ(widgets.automatic->toolTip(),
              QStringLiteral(
                  "Matches the packed diameter to the inside diameter of the parent component."));
    EXPECT_EQ(widgets.automatic->numberBinding(), widgets.diameter.spinBox->binding());
}

TEST_F(MassObjectFieldsTest, TheDiameterIsTwiceTheRadiusAndCanBeAutomatic)
{
    // D2bProbe: the default 25 mm long, 25 mm diameter mass component in a body tube of inner
    // radius 23 mm; a typed diameter makes the radius manual, the automatic one keeps the volume.
    const MassDesign                     design;
    PanelHost                            host(*design.mass);
    const SizeRows                       rows    = makeSizeRows(host);
    const MassObjectFields::SizeWidgets& widgets = rows.widgets;
    EXPECT_EQ(widgets.length.spinBox->binding()->value(), 0.025);
    EXPECT_EQ(widgets.diameter.spinBox->binding()->value(), 0.025);
    EXPECT_FALSE(widgets.automatic->isChecked());
    EXPECT_TRUE(widgets.automatic->isEnabled());

    widgets.diameter.spinBox->binding()->setValue(0.02);
    EXPECT_EQ(design.mass->getRadius(), 0.01);
    EXPECT_FALSE(design.mass->isRadiusAutomatic());
    EXPECT_EQ(design.mass->getLength(), 0.025);

    widgets.automatic->click();
    EXPECT_TRUE(design.mass->isRadiusAutomatic());
    EXPECT_EQ(design.mass->getRadius(), 0.023);
    EXPECT_EQ(design.mass->getLength(), 0.004725897920604916);
    EXPECT_EQ(widgets.diameter.spinBox->binding()->value(), 0.046);
    EXPECT_TRUE(widgets.automatic->isChecked());

    widgets.length.spinBox->binding()->setValue(-1);
    EXPECT_EQ(design.mass->getLength(), 0.0);
}

TEST_F(MassObjectFieldsTest, AutomaticNeedsAParentThatOffersARadius)
{
    // D2bProbe: a mass component inside a mass component has getMaxParentRadius() 0; inside a
    // nose cone the base radius (25 mm).
    const MassDesign design;
    auto&            inner = design.mass->addChild(std::make_unique<MassComponent>());
    design.document->clearUndo();
    EXPECT_EQ(inner.getMaxParentRadius(), 0.0);
    EXPECT_FALSE(MassObjectFields::isAutomaticRadiusEnabled(inner));
    EXPECT_TRUE(MassObjectFields::isAutomaticRadiusEnabled(*design.mass));
    EXPECT_FALSE(MassObjectFields::isAutomaticRadiusEnabled(*design.body));
    PanelHost      host(inner);
    const SizeRows rows = makeSizeRows(host);
    EXPECT_FALSE(rows.widgets.automatic->isEnabled());

    auto&       nose = design.stage->addChild(std::make_unique<NoseCone>(), 0);
    auto const& cord = nose.addChild(std::make_unique<ShockCord>());
    EXPECT_EQ(cord.getMaxParentRadius(), 0.025);
    EXPECT_TRUE(MassObjectFields::isAutomaticRadiusEnabled(cord));
}

TEST_F(MassObjectFieldsTest, AutomaticFollowsTheParentOnEveryChange)
{
    // Java: a listener of the parent's changes sets the enabled state again.
    const MassDesign design;
    PanelHost        host(*design.mass);
    const SizeRows   rows = makeSizeRows(host);
    EXPECT_TRUE(rows.widgets.automatic->isEnabled());
    // A wall as thick as the tube's radius leaves no room inside.
    design.body->setThickness(design.body->getOuterRadius());
    EXPECT_EQ(design.mass->getMaxParentRadius(), 0.0);
    EXPECT_FALSE(rows.widgets.automatic->isEnabled());
    design.body->setThickness(0.002);
    EXPECT_TRUE(rows.widgets.automatic->isEnabled());
}

TEST_F(MassObjectFieldsTest, TheSizeReachesEverySelectedMassObject)
{
    const MassDesign                      design;
    const std::array<RocketComponent*, 2> others{design.cord, design.parachute};
    PanelHost                             host(*design.mass, others);
    const SizeRows                        rows = makeSizeRows(host);
    rows.widgets.length.spinBox->binding()->setValue(0.03);
    rows.widgets.diameter.spinBox->binding()->setValue(0.04);
    EXPECT_EQ(design.cord->getLength(), 0.03);
    EXPECT_EQ(design.parachute->getLength(), 0.03);
    EXPECT_EQ(design.cord->getRadius(), 0.02);
    EXPECT_EQ(design.parachute->getRadius(), 0.02);
    rows.widgets.automatic->click();
    EXPECT_TRUE(design.cord->isRadiusAutomatic());
    EXPECT_TRUE(design.parachute->isRadiusAutomatic());
}

TEST_F(MassObjectFieldsTest, AComponentThatIsNoMassObjectIsABug)
{
    const MassDesign design;
    PanelHost        host(*design.body);
    EXPECT_THROW(static_cast<void>(makeSizeRows(host)), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
