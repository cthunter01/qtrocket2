#include "QtRocket/gui/configdialog/TrapezoidFinSetConfig.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numbers>
#include <span>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QGroupBox>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QPushButton>
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
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using TrapezoidFinSetConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::FactoryPanel;
using Test::focusOrder;
using Test::hasLinearSlider;
using Test::missingNumberRows;
using Test::tabTitles;
using Test::takesFromZeroUp;
using Test::typeInto;

/// The trapezoidal fin set of @p design.
[[nodiscard]] TrapezoidFinSet& trapezoid(const EveryKindDesign& design)
{
    return dynamic_cast<TrapezoidFinSet&>(design.component(ComponentKind::TRAPEZOID_FIN_SET));
}

/// Whether the slider of @p widgets is OpenRocket's getSliderModel(0, 0.05, 0.2): quadratic
/// beyond 5 cm at its middle.
[[nodiscard]] bool hasOutlineSlider(const ConfigFields::NumberWidgets& widgets)
{
    const SliderMapping& mapping = widgets.slider->mapping();
    return !mapping.isLinear() && mapping.getMin() == 0.0 && mapping.getMid() == 0.05 &&
           mapping.getLinearPosition() == 0.5 && mapping.getMax() == 0.2;
}

/// The label under @p root that shows @p text; null when there is none.
[[nodiscard]] const QLabel* labelWithText(const QWidget& root, const QString& text)
{
    const QList<QLabel*> labels = root.findChildren<QLabel*>();
    const auto           found  = std::ranges::find_if(
        labels, [&text](const QLabel* label) { return label->text() == text; });
    return found != labels.end() ? *found : nullptr;
}

TEST_F(TrapezoidFinSetConfigTest, ShowsJavasTabsAndFields)
{
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    TrapezoidFinSet&                          fins = trapezoid(design);
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Fin tabs"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel->tabs().tabToolTip(1), QStringLiteral("Through-the-wall fin tabs"));
    EXPECT_EQ(panel->selectedTabIndex(), 0);

    // The rows, named after Java's properties, with their labels and tooltips.
    ASSERT_NE(panel->finCountSpinBox(), nullptr);
    EXPECT_EQ(panel->finCountSpinBox()->objectName(), QStringLiteral("FinCount"));
    EXPECT_EQ(panel->finCountSpinBox()->toolTip(),
              QStringLiteral("The number of fins in the fin set."));
    EXPECT_EQ(panel->cantWidgets().label->text(), QStringLiteral("Fin cant:"));
    EXPECT_EQ(
        panel->cantWidgets().label->toolTip(),
        QStringLiteral("The angle that the fins are canted with respect to the rocket body."));
    EXPECT_EQ(panel->rootChordWidgets().label->text(), QStringLiteral("Root chord:"));
    EXPECT_EQ(panel->tipChordWidgets().label->text(), QStringLiteral("Tip chord:"));
    EXPECT_EQ(panel->heightWidgets().label->text(), QStringLiteral("Height:"));
    EXPECT_EQ(panel->sweepWidgets().label->text(), QStringLiteral("Sweep length:"));
    EXPECT_EQ(panel->sweepAngleWidgets().label->text(), QStringLiteral("Sweep angle:"));
    EXPECT_EQ(panel->thicknessWidgets().label->text(), QStringLiteral("Thickness:"));
    EXPECT_EQ(panel->baseRotationWidgets().label->text(), QStringLiteral("Fin rotation:"));
    EXPECT_EQ(panel->baseRotationWidgets().label->toolTip(),
              QStringLiteral("The angle of the first fin in the fin set."));
    const QWidget& general = *panel->tabs().widget(0);
    EXPECT_EQ(
        missingNumberRows(general, {"CantAngle", "RootChord", "TipChord", "Height", "Sweep",
                                    "SweepAngle", "Thickness", "BaseRotation", "FilletRadius"}),
        "");
    ASSERT_NE(panel->crossSectionComboBox(), nullptr);
    EXPECT_EQ(panel->crossSectionComboBox()->objectName(), QStringLiteral("CrossSection"));
    EXPECT_EQ(panel->crossSectionComboBox()->count(), 3);
    EXPECT_EQ(panel->crossSectionComboBox()->itemText(2), QStringLiteral("Airfoil"));

    // The bounds: 1 to 8 fins, a cant of +-15 degrees, lengths from 0 up, a free sweep, a sweep
    // angle of +-89 degrees, the fin rotation within +-180 degrees.
    const IntBinding& count = *panel->finCountSpinBox()->binding();
    EXPECT_EQ(count.minimum(), 1);
    EXPECT_EQ(count.maximum(), 8);
    const DoubleBinding& cant = *panel->cantWidgets().spinBox->binding();
    EXPECT_EQ(cant.minimum(), -FinSet::kMaxCantRadians);
    EXPECT_EQ(cant.maximum(), FinSet::kMaxCantRadians);
    EXPECT_TRUE(takesFromZeroUp(panel->rootChordWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->tipChordWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->heightWidgets()));
    EXPECT_TRUE(takesFromZeroUp(panel->thicknessWidgets()));
    const DoubleBinding& sweep = *panel->sweepWidgets().spinBox->binding();
    EXPECT_EQ(sweep.minimum(), -std::numeric_limits<double>::infinity());
    EXPECT_EQ(sweep.maximum(), std::numeric_limits<double>::infinity());
    const DoubleBinding& sweepAngle = *panel->sweepAngleWidgets().spinBox->binding();
    EXPECT_EQ(sweepAngle.minimum(), -TrapezoidFinSet::kMaxSweepAngle);
    EXPECT_EQ(sweepAngle.maximum(), TrapezoidFinSet::kMaxSweepAngle);
    const DoubleBinding& rotation = *panel->baseRotationWidgets().spinBox->binding();
    EXPECT_EQ(rotation.minimum(), -std::numbers::pi);
    EXPECT_EQ(rotation.maximum(), std::numbers::pi);

    // The sliders: +-15 degrees; 0, 5 cm, 20 cm; -1.1 tip chord to 1.1 root chord; +-45 degrees;
    // 0 to 1 cm; -180 to 180 degrees.
    EXPECT_TRUE(
        hasLinearSlider(panel->cantWidgets(), -FinSet::kMaxCantRadians, FinSet::kMaxCantRadians));
    EXPECT_TRUE(hasOutlineSlider(panel->rootChordWidgets()));
    EXPECT_TRUE(hasOutlineSlider(panel->tipChordWidgets()));
    EXPECT_TRUE(hasOutlineSlider(panel->heightWidgets()));
    EXPECT_TRUE(hasLinearSlider(panel->sweepWidgets(), -1.1 * fins.getTipChord(),
                                1.1 * fins.getRootChord()));
    EXPECT_TRUE(
        hasLinearSlider(panel->sweepAngleWidgets(), -std::numbers::pi / 4, std::numbers::pi / 4));
    EXPECT_TRUE(hasLinearSlider(panel->thicknessWidgets(), 0.0, 0.01));
    EXPECT_TRUE(hasLinearSlider(panel->baseRotationWidgets(), -std::numbers::pi, std::numbers::pi));

    // The right column: the placement holds the fin rotation, the material is bulk.
    ASSERT_NE(panel->placementPanel(), nullptr);
    EXPECT_TRUE(panel->placementPanel()->isAncestorOf(panel->baseRotationWidgets().spinBox));
    ASSERT_NE(panel->materialPanel(), nullptr);
    EXPECT_EQ(panel->materialPanel()->materialComboBox().binding()->getType(),
              Material::Type::BULK);
}

TEST_F(TrapezoidFinSetConfigTest, ShowsJavasRootFilletsAndButtons)
{
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    TrapezoidFinSet&                          fins = trapezoid(design);
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);

    // Root fillets: Java's tooltip on the spin box, the unit selector, the slider and the border's
    // panel, the material label's own.
    const QString tip = QStringLiteral(
        "<HTML>Adds the predicted mass of fin fillets to the root of the fins. <br>Assumes the "
        "fillet is concave and tangent to the body tube and fin. <br>Zero radius will give no "
        "fillet.  ");
    ASSERT_NE(panel->filletPanel(), nullptr);
    EXPECT_EQ(panel->filletPanel()->title(), QStringLiteral("Root Fillets"));
    EXPECT_EQ(panel->filletPanel()->toolTip(), tip);
    const ConfigFields::NumberWidgets& radius = panel->filletRadiusWidgets();
    EXPECT_EQ(radius.label->text(), QStringLiteral("Fillet radius:"));
    EXPECT_EQ(radius.label->toolTip(), QString());
    EXPECT_EQ(radius.spinBox->toolTip(), tip);
    EXPECT_EQ(radius.unitSelector->toolTip(), tip);
    EXPECT_EQ(radius.slider->toolTip(), tip);
    EXPECT_EQ(radius.spinBox->fixedStep(), 0.1);
    EXPECT_TRUE(takesFromZeroUp(radius));
    EXPECT_TRUE(hasLinearSlider(radius, 0.0, 0.1));
    ASSERT_NE(panel->filletMaterialComboBox(), nullptr);
    EXPECT_EQ(panel->filletMaterialComboBox()->objectName(), QStringLiteral("FilletMaterial"));
    EXPECT_EQ(panel->filletMaterialComboBox()->binding()->getType(), Material::Type::BULK);
    EXPECT_EQ(panel->filletMaterialComboBox()->binding()->value(), fins.getFilletMaterial());
    EXPECT_EQ(panel->filletMaterialComboBox()->toolTip(),
              QStringLiteral("The component material affects the weight of the component."));
    // The fillets are not a MaterialPanel of their own (Java's filletMaterialPanel()).
    EXPECT_FALSE(panel->materialPanel()->isAncestorOf(panel->filletMaterialComboBox()));
    const QLabel* label = labelWithText(*panel->filletPanel(), QStringLiteral("Fillet material:"));
    ASSERT_NE(label, nullptr);
    EXPECT_EQ(label->toolTip(),
              QStringLiteral("The component material affects the weight of the component."));

    // The buttons before Cancel: "Split fins" (three fins: enabled), "Convert to freeform".
    ASSERT_NE(panel->splitButton(), nullptr);
    ASSERT_NE(panel->convertButton(), nullptr);
    EXPECT_EQ(panel->splitButton()->text(), QStringLiteral("Split fins"));
    EXPECT_EQ(panel->splitButton()->toolTip(),
              QStringLiteral("Split the fin set into separate fins."));
    EXPECT_TRUE(panel->splitButton()->isEnabled());
    EXPECT_EQ(panel->convertButton()->text(), QStringLiteral("Convert to freeform"));
    EXPECT_EQ(panel->convertButton()->toolTip(),
              QStringLiteral("Convert this fin set into a freeform fin set"));
    const QWidget* row = panel->cancelButton().parentWidget();
    ASSERT_NE(row, nullptr);
    EXPECT_TRUE(row->isAncestorOf(panel->splitButton()));
    EXPECT_TRUE(row->isAncestorOf(panel->convertButton()));
}

TEST_F(TrapezoidFinSetConfigTest, ShowsJavasFinTabs)
{
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    TrapezoidFinSet&                          fins = trapezoid(design);
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins);
    ASSERT_NE(panel.get(), nullptr);
    const QWidget& tabs = *panel->tabs().widget(1);
    EXPECT_EQ(missingNumberRows(tabs, {"TabLength", "TabHeight", "TabOffset"}), "");
    EXPECT_EQ(panel->tabLengthWidgets().label->text(), QStringLiteral("Tab length:"));
    EXPECT_EQ(panel->tabLengthWidgets().label->toolTip(),
              QStringLiteral("The length of the fin tab."));
    EXPECT_EQ(panel->tabHeightWidgets().label->text(), QStringLiteral("Tab height:"));
    EXPECT_EQ(panel->tabHeightWidgets().label->toolTip(),
              QStringLiteral("The spanwise height of the fin tab."));
    EXPECT_EQ(panel->tabOffsetWidgets().label->text(), QStringLiteral("Tab position:"));
    EXPECT_EQ(panel->tabOffsetWidgets().label->toolTip(),
              QStringLiteral("The position of the fin tab."));
    EXPECT_EQ(panel->autoCalcButton().text(), QStringLiteral("Calculate automatically"));
    EXPECT_TRUE(panel->autoCalcButton().toolTip().startsWith(
        QStringLiteral("<html>Calculates the height and length of the fin tabs")));
    EXPECT_TRUE(tabs.isAncestorOf(&panel->autoCalcButton()));

    // The tab length from 0 up, its slider to the fin's length (which it follows); the tab height
    // at most the largest one when the panel was made, its slider to the largest one as it
    // changes; the tab offset free, its slider within half the fin's length either way.
    EXPECT_TRUE(takesFromZeroUp(panel->tabLengthWidgets()));
    EXPECT_TRUE(hasLinearSlider(panel->tabLengthWidgets(), 0.0, fins.getLength()));
    ASSERT_NE(panel->tabLengthWidgets().slider->dynamicBounds().max, nullptr);
    const DoubleBinding& height = *panel->tabHeightWidgets().spinBox->binding();
    EXPECT_EQ(height.minimum(), 0.0);
    EXPECT_EQ(height.maximum(), fins.getMaxTabHeight());
    EXPECT_TRUE(hasLinearSlider(panel->tabHeightWidgets(), 0.0, fins.getMaxTabHeight()));
    const DoubleBinding& offset = *panel->tabOffsetWidgets().spinBox->binding();
    EXPECT_EQ(offset.minimum(), -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(hasLinearSlider(panel->tabOffsetWidgets(), -0.5 * fins.getLength(),
                                0.5 * fins.getLength()));

    // "relative to": every axial method, After included (Java's EnumModel without a list).
    const EnumComboBox& method = panel->tabOffsetMethodComboBox();
    EXPECT_EQ(method.objectName(), QStringLiteral("TabOffsetMethod"));
    const auto binding = method.enumBinding<AxialMethod>();
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->size(), 5);
    EXPECT_EQ(binding->value(), fins.getTabOffsetMethod());

    // The sliders follow the fin: a longer root chord moves the tab length's maximum.
    ASSERT_TRUE(panel->rootChordWidgets().spinBox->binding()->commitText("10"));
    EXPECT_EQ(panel->tabLengthWidgets().slider->mapping().getMax(), 0.1);
    EXPECT_EQ(panel->tabOffsetWidgets().slider->mapping().getMax(), 0.05);
    EXPECT_EQ(panel->tabOffsetWidgets().slider->mapping().getMin(), -0.05);
    // The tab height's bound stays what it was when the panel was made (Java's constant max),
    // also when a wider body allows a deeper tab; its slider follows.
    const double atOpen = fins.getMaxTabHeight();
    dynamic_cast<BodyTube&>(*fins.getParent()).setOuterRadius(2 * atOpen);
    EXPECT_EQ(fins.getMaxTabHeight(), 2 * atOpen);
    EXPECT_EQ(height.maximum(), atOpen);
    EXPECT_EQ(panel->tabHeightWidgets().slider->mapping().getMax(), 2 * atOpen);
}

TEST_F(TrapezoidFinSetConfigTest, TheTabKeyVisitsTheFieldsInJavasOrder)
{
    const ConfigDialogServices                services;
    const EveryKindDesign                     design;
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, trapezoid(design));
    ASSERT_NE(panel.get(), nullptr);
    EXPECT_EQ(focusOrder(*panel->tabs().widget(0),
                         {"FinCount", "CantAngle", "RootChord", "TipChord", "Height", "Sweep",
                          "SweepAngle", "CrossSection", "Thickness", "AxialMethod", "AxialOffset",
                          "BaseRotation", "FilletRadius", "FilletMaterial"}),
              (std::vector<std::string>{"FinCount", "CantAngle", "RootChord", "TipChord", "Height",
                                        "Sweep", "SweepAngle", "CrossSection", "Thickness",
                                        "AxialMethod", "AxialOffset", "BaseRotation",
                                        "FilletRadius", "FilletMaterial"}));
    EXPECT_EQ(focusOrder(*panel->tabs().widget(1), {"TabLength", "TabHeight", "TabOffset",
                                                    "TabOffsetMethod", "autoCalcButton"}),
              (std::vector<std::string>{"TabLength", "TabHeight", "TabOffset", "TabOffsetMethod",
                                        "autoCalcButton"}));
}

TEST_F(TrapezoidFinSetConfigTest, TheMainComponentMustBeATrapezoidalFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    const auto                 selection =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::ELLIPTICAL_FIN_SET));
    EXPECT_THROW(static_cast<void>(makeChild<TrapezoidFinSetConfig>(
                     &host, *design.document, services.services(), selection)),
                 BugError);
    const auto lug =
        std::make_shared<ComponentSelection>(design.component(ComponentKind::LAUNCH_LUG));
    EXPECT_THROW(static_cast<void>(makeChild<TrapezoidFinSetConfig>(&host, *design.document,
                                                                    services.services(), lug)),
                 BugError);
}

TEST_F(TrapezoidFinSetConfigTest, AnEditSetsEverySelectedTrapezoidalFinSet)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    TrapezoidFinSet&           fins = trapezoid(design);
    auto&                      second =
        design.component(ComponentKind::BODY_TUBE).addChild(std::make_unique<TrapezoidFinSet>());
    second.setHeight(0.02);
    const std::array<RocketComponent*, 1>     others{&second};
    const FactoryPanel<TrapezoidFinSetConfig> panel(*design.document, services, fins, others);
    ASSERT_NE(panel.get(), nullptr);

    ASSERT_TRUE(panel->finCountSpinBox()->binding()->setSpinnerValue(5));
    EXPECT_EQ(fins.getFinCount(), 5);
    EXPECT_EQ(second.getFinCount(), 5);
    ASSERT_TRUE(panel->cantWidgets().spinBox->binding()->commitText("2"));
    EXPECT_DOUBLE_EQ(fins.getCantAngle(), 2 * std::numbers::pi / 180);
    EXPECT_DOUBLE_EQ(second.getCantAngle(), 2 * std::numbers::pi / 180);
    ASSERT_TRUE(panel->rootChordWidgets().spinBox->binding()->commitText("9"));
    EXPECT_DOUBLE_EQ(fins.getRootChord(), 0.09);
    EXPECT_DOUBLE_EQ(second.getRootChord(), 0.09);
    ASSERT_TRUE(panel->tipChordWidgets().spinBox->binding()->commitText("4"));
    EXPECT_DOUBLE_EQ(second.getTipChord(), 0.04);
    ASSERT_TRUE(panel->sweepWidgets().spinBox->binding()->commitText("1"));
    EXPECT_DOUBLE_EQ(second.getSweep(), 0.01);
    ASSERT_TRUE(panel->thicknessWidgets().spinBox->binding()->commitText("0.2"));
    EXPECT_DOUBLE_EQ(second.getThickness(), 0.002);
    ASSERT_TRUE(panel->filletRadiusWidgets().spinBox->binding()->commitText("0.4"));
    EXPECT_DOUBLE_EQ(fins.getFilletRadius(), 0.004);
    EXPECT_DOUBLE_EQ(second.getFilletRadius(), 0.004);
    ASSERT_TRUE(panel->tabLengthWidgets().spinBox->binding()->commitText("3"));
    EXPECT_DOUBLE_EQ(second.getTabLength(), 0.03);
    EXPECT_TRUE(panel->crossSectionComboBox()->enumBinding<FinCrossSection>()->select(
        FinCrossSection::ROUNDED));
    EXPECT_EQ(fins.getCrossSection(), FinCrossSection::ROUNDED);
    EXPECT_EQ(second.getCrossSection(), FinCrossSection::ROUNDED);
    EXPECT_TRUE(
        panel->tabOffsetMethodComboBox().enumBinding<AxialMethod>()->select(AxialMethod::TOP));
    EXPECT_EQ(second.getTabOffsetMethod(), AxialMethod::TOP);

    // The user's decision U3 (cascade 7 of ComponentSelection.h): the sweep angle gives each its
    // sweep from its own height (OpenRocket: the main one's sweep for every fin set).
    ASSERT_TRUE(panel->sweepAngleWidgets().spinBox->binding()->commitText("45"));
    EXPECT_NEAR(fins.getSweep(), fins.getHeight(), 1e-12);
    EXPECT_NEAR(second.getSweep(), 0.02, 1e-12);
}

TEST_F(TrapezoidFinSetConfigTest, TypedTextReachesTheFinSet)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TrapezoidFinSet&           fins = trapezoid(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Trapezoidal Fin Set configuration"));
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    // The fin rotation's field is bounded to 180 degrees (Java: -PI ... PI).
    UnitSpinBox& rotation = *panel->baseRotationWidgets().spinBox;
    typeInto(rotation, QStringLiteral("200"));
    QTest::keyClick(&rotation, Qt::Key_Return);
    EXPECT_DOUBLE_EQ(std::abs(fins.getBaseRotation()), std::numbers::pi);
    UnitSpinBox& height = *panel->heightWidgets().spinBox;
    typeInto(height, QStringLiteral("4.5"));
    QTest::keyClick(&height, Qt::Key_Return);
    EXPECT_DOUBLE_EQ(fins.getHeight(), 0.045);
    EXPECT_TRUE(dialog->isModified());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(TrapezoidFinSetConfigTest, SplitFinsFollowsTheFinCountInTheDialog)
{
    // Java's updateFields(): the dialog refreshes the button on every change of the rocket.
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    TrapezoidFinSet&           fins = trapezoid(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    const auto* panel = qobject_cast<const TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    ASSERT_NE(panel->splitButton(), nullptr);
    EXPECT_TRUE(panel->splitButton()->isEnabled());
    ASSERT_TRUE(panel->finCountSpinBox()->binding()->setSpinnerValue(1));
    EXPECT_FALSE(panel->splitButton()->isEnabled());
    ASSERT_TRUE(panel->finCountSpinBox()->binding()->setSpinnerValue(2));
    EXPECT_TRUE(panel->splitButton()->isEnabled());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(TrapezoidFinSetConfigTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and the root chord typed but not committed reaches none of the components that are
    // gone (proved under the asan preset).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    TrapezoidFinSet& fins  = trapezoid(design);
    const Uuid       id    = fins.getId();
    const double     chord = fins.getRootChord();
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, fins);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    auto* panel = qobject_cast<TrapezoidFinSetConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    typeInto(*panel->rootChordWidgets().spinBox, QStringLiteral("77"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(panel->commitAll());
    EXPECT_FALSE(panel->finCountSpinBox()->binding()->setSpinnerValue(3));
    panel->updateFields();
    panel->calculateAutoTab();
    panel->convertToFreeform();
    panel->splitFins();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto* restored =
        dynamic_cast<const TrapezoidFinSet*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getRootChord(), chord);
}

}  // namespace

}  // namespace QtRocket::Gui
