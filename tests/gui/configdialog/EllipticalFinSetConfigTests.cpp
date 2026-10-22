#include "QtRocket/gui/configdialog/EllipticalFinSetConfig.h"

#include <array>
#include <limits>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

namespace
{

using EllipticalFinSetConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::focusOrder;
using Test::hasLinearSlider;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::takesFromZeroUp;

/// The elliptical fin set of @p design.
[[nodiscard]] EllipticalFinSet& ellipse(const EveryKindDesign& design)
{
    return dynamic_cast<EllipticalFinSet&>(design.component(ComponentKind::ELLIPTICAL_FIN_SET));
}

/// Whether the slider of @p widgets is OpenRocket's getSliderModel(0, 0.05, 0.2).
[[nodiscard]] bool hasOutlineSlider(const ConfigFields::NumberWidgets& widgets)
{
    const SliderMapping& mapping = widgets.slider->mapping();
    return !mapping.isLinear() && mapping.getMin() == 0.0 && mapping.getMid() == 0.05 &&
           mapping.getMax() == 0.2;
}

TEST_F(EllipticalFinSetConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices                 services;
    const EveryKindDesign                      design;
    EllipticalFinSet&                          fins = ellipse(design);
    const FactoryPanel<EllipticalFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Fin tabs"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The rows: the fin count without a tooltip (unlike the trapezoidal fin set's), the root
    // chord is the fin set's length.
    ASSERT_NE(panel->finCountSpinBox(), nullptr);
    EXPECT_EQ(panel->finCountSpinBox()->toolTip(), QString());
    const IntBinding& count = *panel->finCountSpinBox()->binding();
    EXPECT_EQ(count.maximum(), 8);
    EXPECT_EQ(
        panel->cantWidgets().label->toolTip(),
        QStringLiteral("The angle that the fins are canted with respect to the rocket body."));
    EXPECT_EQ(panel->rootChordWidgets().label->text(), QStringLiteral("Root chord:"));
    EXPECT_EQ(panel->rootChordWidgets().spinBox->objectName(), QStringLiteral("Length"));
    EXPECT_EQ(panel->rootChordWidgets().spinBox->binding()->value(), fins.getLength());
    EXPECT_EQ(panel->heightWidgets().label->text(), QStringLiteral("Height:"));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(missingNumberRows(general, {"CantAngle", "Length", "Height", "Thickness",
                                          "BaseRotation", "FilletRadius"}),
              "");
    ASSERT_NE(panel->crossSectionComboBox(), nullptr);
    EXPECT_TRUE(takesFromZeroUp(panel->rootChordWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->heightWidgets()));
    EXPECT_TRUE(hasOutlineSlider(panel->rootChordWidgets()));
    EXPECT_TRUE(hasOutlineSlider(panel->heightWidgets()));
    EXPECT_TRUE(hasLinearSlider(panel->thicknessWidgets(), 0.0, 0.01));
    EXPECT_TRUE(
        hasLinearSlider(panel->cantWidgets(), -FinSet::kMaxCantRadians, FinSet::kMaxCantRadians));

    // The fin rotation is unbounded here (Java: DoubleModel(component, "BaseRotation",
    // UNITS_ANGLE)), its slider -180 to 180 degrees.
    const DoubleBinding& rotation = *panel->baseRotationWidgets().spinBox->binding();
    EXPECT_EQ(rotation.minimum(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(rotation.maximum(), std::numeric_limits<double>::infinity());
    EXPECT_TRUE(hasLinearSlider(panel->baseRotationWidgets(), -std::numbers::pi, std::numbers::pi));
    ASSERT_NE(panel->placementPanel(), nullptr);
    EXPECT_TRUE(panel->placementPanel()->isAncestorOf(panel->baseRotationWidgets().spinBox));
    EXPECT_NE(panel->filletPanel(), nullptr);
    EXPECT_NE(panel->splitButton(), nullptr);
    EXPECT_NE(panel->convertButton(), nullptr);
}

TEST_F(EllipticalFinSetConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices                 services;
    const EveryKindDesign                      design;
    const FactoryPanel<EllipticalFinSetConfig> panel(*design.document, services, ellipse(design));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(focusOrder(
                  *panel->tabs().widget(0),
                  {"FinCount", "CantAngle", "Length", "Height", "CrossSection", "Thickness",
                   "AxialMethod", "AxialOffset", "BaseRotation", "FilletRadius", "FilletMaterial"}),
              (std::vector<std::string>{"FinCount", "CantAngle", "Length", "Height", "CrossSection",
                                        "Thickness", "AxialMethod", "AxialOffset", "BaseRotation",
                                        "FilletRadius", "FilletMaterial"}));
}

TEST_F(EllipticalFinSetConfigTest, TheMainComponentMustBeAnEllipticalFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_THROW(static_cast<void>(makeChild<EllipticalFinSetConfig>(
                     &host, *design.document, services.services(), selection)),
                 BugError);
}

TEST_F(EllipticalFinSetConfigTest, AnEditSetsEverySelectedEllipticalFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    EllipticalFinSet&          fins = ellipse(design);
    auto&                      second =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<EllipticalFinSet>());
    const std::array<RocketComponent*, 1>      others{&second};
    const FactoryPanel<EllipticalFinSetConfig> panel(*design.document, services, fins, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->rootChordWidgets().spinBox->binding()->commitText("6"));
    EXPECT_DOUBLE_EQ(fins.getLength(), 0.06);
    EXPECT_DOUBLE_EQ(second.getLength(), 0.06);
    ASSERT_TRUE(panel->heightWidgets().spinBox->binding()->commitText("4"));
    EXPECT_DOUBLE_EQ(fins.getHeight(), 0.04);
    EXPECT_DOUBLE_EQ(second.getHeight(), 0.04);
    ASSERT_TRUE(panel->baseRotationWidgets().spinBox->binding()->commitText("200"));
    // Unbounded: the setter reduces 200 degrees to -160.
    EXPECT_DOUBLE_EQ(fins.getBaseRotation(), -160 * std::numbers::pi / 180);
    EXPECT_DOUBLE_EQ(second.getBaseRotation(), -160 * std::numbers::pi / 180);
    ASSERT_TRUE(panel->tabHeightWidgets().spinBox->binding()->commitText("0.5"));
    EXPECT_DOUBLE_EQ(second.getTabHeight(), 0.005);
}

}  // namespace

}  // namespace QtRocket::Gui
