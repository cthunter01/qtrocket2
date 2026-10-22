#include "QtRocket/gui/configdialog/InnerTubeConfig.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QLabel>
#include <QPoint>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/ClusterButton.h"
#include "QtRocket/gui/configdialog/ClusterSelectionPanel.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/RingComponentConfig.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "binding/BindingTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using InnerTubeConfigTest = Test::ConfigDialogTest;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::RingDesign;
using Test::RocketEventRecorder;
using Test::rowLabels;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;

/// The layout @p index of ClusterConfiguration::configurations() (Java's CONFIGURATIONS).
[[nodiscard]] const ClusterConfiguration& layout(std::size_t index)
{
    return ClusterConfiguration::configurations()[index];
}

/// The panel of @p created as an InnerTubeConfig.
[[nodiscard]] InnerTubeConfig& tubePanel(const FactoryPanel& created)
{
    auto* panel = created.as<InnerTubeConfig>();
    if (panel == nullptr)
    {
        bug("not an inner tube's panel");
    }
    return *panel;
}

/// The texts of the labels under @p widget, in the order they were made.
[[nodiscard]] QStringList labelTexts(const QWidget& widget)
{
    QStringList texts;
    for (const QLabel* label : widget.findChildren<QLabel*>())
    {
        texts.append(label->text());
    }
    return texts;
}

/// A tube of D2aProbe's output after a split.
struct ProbeTube
{
    const char* name;
    double      radialPosition;
    double      radialDirection;
    double      shiftY;
    double      shiftZ;
    double      outerRadius;
    std::size_t children;
};

/// How the first children of @p parent differ from the split tubes @p expected (each a single
/// tube, scale 1, rotation 0, positioned as the probe says to 1e-12 m or rad), one line each.
[[nodiscard]] std::string splitDifferences(const RocketComponent&     parent,
                                           std::span<const ProbeTube> expected)
{
    std::string differences;
    if (parent.getChildCount() < expected.size())
    {
        return std::format("{} children\n", parent.getChildCount());
    }
    const auto closeEnough = [](double a, double b) { return std::abs(a - b) <= 1e-12; };
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const ProbeTube& want = expected[i];
        const auto*      tube = dynamic_cast<const InnerTube*>(parent.getChildren().at(i));
        if (tube == nullptr)
        {
            differences += std::format("child {} is no inner tube\n", i);
            continue;
        }
        if (tube->getName() != want.name || &tube->getClusterConfiguration() != &layout(0) ||
            tube->getClusterScale() != 1.0 || tube->getClusterRotation() != 0.0 ||
            !closeEnough(tube->getRadialPosition(), want.radialPosition) ||
            !closeEnough(tube->getRadialDirection(), want.radialDirection) ||
            !closeEnough(tube->getRadialShiftY(), want.shiftY) ||
            !closeEnough(tube->getRadialShiftZ(), want.shiftZ) ||
            tube->getOuterRadius() != want.outerRadius || tube->getChildCount() != want.children)
        {
            differences += std::format(
                "{}: '{}' {} scale {} rotation {} at {} / {} ({}, {}) outer {}, {} children\n", i,
                tube->getName(), tube->getClusterConfiguration().getXmlName(),
                tube->getClusterScale(), tube->getClusterRotation(), tube->getRadialPosition(),
                tube->getRadialDirection(), tube->getRadialShiftY(), tube->getRadialShiftZ(),
                tube->getOuterRadius(), tube->getChildCount());
        }
    }
    return differences;
}

/// D2aProbe: the 3-ring inner tube (scale 1.5, rotation 0.4, radial position 3 mm at 0.2 rad)
/// split into its three tubes, each with a copy of the engine block.
[[nodiscard]] std::array<ProbeTube, 3> probeThreeRingSplit()
{
    return {{
        {.name            = "Inner Tube #1",
         .radialPosition  = 0.013482442394308056,
         .radialDirection = -2.7905582434258944,
         .shiftY          = -0.01266024953121705,
         .shiftZ          = -0.004636198304985843,
         .outerRadius     = 0.0095,
         .children        = 1},
        {.name            = "Inner Tube #2",
         .radialPosition  = 0.0184194919281268,
         .radialDirection = -0.5932957166586459,
         .shiftY          = 0.015271647937258333,
         .shiftZ          = -0.010298274232645083,
         .outerRadius     = 0.0095,
         .children        = 1},
        {.name            = "Inner Tube #3",
         .radialPosition  = 0.017838051020048023,
         .radialDirection = 1.215266187455748,
         .shiftY          = 0.00620920079452989,
         .shiftZ          = 0.016722496514786474,
         .outerRadius     = 0.0095,
         .children        = 1},
    }};
}

/// Makes the probe's 3-ring of @p design's inner tube, the undo history starting there.
void makeProbeCluster(const RingDesign& design)
{
    design.inner->setClusterConfiguration(layout(4));
    design.inner->setClusterScale(1.5);
    design.inner->setClusterRotation(0.4);
    design.inner->setRadialPosition(0.003);
    design.inner->setRadialDirection(0.2);
    design.document->clearUndo();
}

/// How the first three children of @p body differ from the copies "Inner Tube #1" to "#3" of a
/// tube that held an engine block and the 2-tube inner tube "Nested", one line each.
[[nodiscard]] std::string nestedCopyDifferences(const RocketComponent& body)
{
    std::string differences;
    for (std::size_t i = 0; i < 3 && i < body.getChildCount(); ++i)
    {
        const RocketComponent& copy = *body.getChildren().at(i);
        const auto* inside          = copy.getChildCount() == 2
                                          ? dynamic_cast<const InnerTube*>(copy.getChildren().at(1))
                                          : nullptr;
        if (copy.getName() != std::format("Inner Tube #{}", i + 1) || inside == nullptr ||
            inside->getName() != "Nested" || &inside->getClusterConfiguration() != &layout(1))
        {
            differences += std::format("child {}: '{}' with {} children\n", i, copy.getName(),
                                       copy.getChildCount());
        }
    }
    return differences;
}

/// How the children of @p body differ from the split "Outer #1" to "#3", each holding the split
/// "Nested #1" to "#6" (single tubes) and nothing else, one line each.
[[nodiscard]] std::string nestedSplitDifferences(const RocketComponent& body)
{
    std::string differences;
    if (body.getChildCount() != 3)
    {
        return std::format("{} children\n", body.getChildCount());
    }
    for (std::size_t i = 0; i < 3; ++i)
    {
        const RocketComponent& copy = *body.getChildren().at(i);
        const auto*            tube = dynamic_cast<const InnerTube*>(&copy);
        if (tube == nullptr || copy.getName() != std::format("Outer #{}", i + 1) ||
            &tube->getClusterConfiguration() != &layout(0) || copy.getChildCount() != 6)
        {
            differences += std::format("child {}: '{}' with {} children\n", i, copy.getName(),
                                       copy.getChildCount());
            continue;
        }
        for (std::size_t j = 0; j < 6; ++j)
        {
            const auto* inside = dynamic_cast<const InnerTube*>(copy.getChildren().at(j));
            if (inside == nullptr || inside->getName() != std::format("Nested #{}", j + 1) ||
                &inside->getClusterConfiguration() != &layout(0))
            {
                differences +=
                    std::format("child {}.{}: '{}'\n", i, j, copy.getChildren().at(j)->getName());
            }
        }
    }
    return differences;
}

/// The body tube of @p document's rocket made by RingDesign (found again after an undo).
[[nodiscard]] const RocketComponent& bodyOf(const OpenRocketDocument& document)
{
    return *document.getRocket().getChildren().front()->getChildren().front();
}

TEST_F(InnerTubeConfigTest, TheTabsAreJavas)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel = tubePanel(created);
    EXPECT_EQ(tabTitles(panel),
              (QStringList{QStringLiteral("General"), QStringLiteral("Motor"),
                           QStringLiteral("Cluster"), QStringLiteral("Radial position"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0), QStringLiteral("General properties"));
    EXPECT_EQ(panel.tabs().tabToolTip(1), QStringLiteral("Motor mount configuration"));
    EXPECT_EQ(panel.tabs().tabToolTip(2), QStringLiteral("Cluster configuration"));
    EXPECT_EQ(panel.tabs().tabToolTip(3), QStringLiteral("Radial position"));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    EXPECT_EQ(panel.tabs().widget(0), panel.generalTab().page);
    EXPECT_EQ(panel.tabs().widget(1), &panel.motorConfig());
}

TEST_F(InnerTubeConfigTest, TheGeneralTabIsATubesWithoutAutomaticRadii)
{
    // D2aProbe: an inner tube's OuterRadius and InnerRadius models have no automatic state.
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const RingComponentConfig::GeneralTab& tab = tubePanel(created).generalTab();
    EXPECT_EQ(rowLabels(tab),
              (QStringList{QStringLiteral("Length:"), QStringLiteral("Outer diameter:"),
                           QStringLiteral("Inner diameter:"), QStringLiteral("Wall thickness:")}));
    EXPECT_EQ(tab.outerRadiusAutomatic, nullptr);
    EXPECT_EQ(tab.innerRadiusAutomatic, nullptr);
    EXPECT_EQ(tab.length.spinBox->binding()->value(), 0.07);
    EXPECT_EQ(tab.outerRadius.spinBox->binding()->value(), 0.019);
    EXPECT_EQ(tab.innerRadius.spinBox->binding()->value(), 0.018);
    EXPECT_EQ(tab.thickness.spinBox->binding()->value(), 5.000000000000004E-4);
    EXPECT_EQ(tab.innerRadius.slider->mapping().getMax(), 0.019);
    ASSERT_NE(tab.material, nullptr);
    EXPECT_EQ(tab.material->materialComboBox().binding()->getType(), Material::Type::BULK);
    EXPECT_NE(tab.placement, nullptr);
}

TEST_F(InnerTubeConfigTest, TheClusterTabHasJavasTexts)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel              = tubePanel(created);
    const ConfigFields::NumberWidgets& separation = panel.separationWidgets();
    EXPECT_EQ(labelTexts(*panel.tabs().widget(2)),
              (QStringList{QStringLiteral("Select cluster configuration:"),
                           QStringLiteral("Tube separation:"), QStringLiteral("Rotation:")}));
    EXPECT_EQ(separation.label->toolTip(),
              QStringLiteral("The separation of the tubes, 1.0 = touching each other"));
    EXPECT_EQ(separation.spinBox->toolTip(),
              QStringLiteral("The separation of the tubes, 1.0 = touching each other"));
    EXPECT_EQ(panel.relativeButton().text(), QStringLiteral("Relative"));
    EXPECT_EQ(panel.relativeButton().toolTip(),
              QStringLiteral("The separation is measured relative to the outer diameter of the "
                             "inner tube"));
    EXPECT_EQ(panel.absoluteButton().text(), QStringLiteral("Absolute"));
    EXPECT_EQ(panel.absoluteButton().toolTip(),
              QStringLiteral("The separation is measured in length units"));
    const QString rotation = QStringLiteral("Rotation angle of the cluster configuration");
    EXPECT_EQ(panel.rotationWidgets().label->toolTip(), rotation);
    EXPECT_EQ(panel.rotationWidgets().spinBox->toolTip(), rotation);
    EXPECT_EQ(panel.rotationWidgets().slider->toolTip(), rotation);
    EXPECT_EQ(panel.rotationWidgets().spinBox->objectName(), QStringLiteral("ClusterRotation"));
    EXPECT_EQ(panel.splitButton().text(), QStringLiteral("Split cluster"));
    EXPECT_EQ(panel.splitButton().toolTip(),
              QStringLiteral("<html>Split the cluster into separate components. <br>This also "
                             "duplicates all components attached to this inner tube."));
    EXPECT_EQ(panel.resetClusterButton().text(), QStringLiteral("Reset settings"));
    EXPECT_EQ(panel.resetClusterButton().toolTip(),
              QStringLiteral("Reset the separation and rotation to the default values"));
    EXPECT_EQ(panel.clusterSelection().buttonCount(), 14);
}

TEST_F(InnerTubeConfigTest, TheSeparationIsRelativeByDefault)
{
    ConfigDialogServices   services;
    const RingDesign       design;
    const FactoryPanel     created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig& panel                  = tubePanel(created);
    const ConfigFields::NumberWidgets& separation = panel.separationWidgets();
    EXPECT_TRUE(panel.isSeparationRelative());
    EXPECT_TRUE(panel.relativeButton().isChecked());
    EXPECT_FALSE(panel.absoluteButton().isChecked());
    EXPECT_EQ(separation.spinBox->binding(), panel.relativeSeparation());
    EXPECT_EQ(separation.unitSelector->binding(), panel.relativeSeparation());
    EXPECT_EQ(separation.slider->binding(), panel.relativeSeparation());
    EXPECT_EQ(separation.spinBox->objectName(), QStringLiteral("ClusterScale"));
    EXPECT_EQ(separation.slider->objectName(), QStringLiteral("ClusterScale"));
    // getSliderModel(0, 1, 4): quadratic about 1.
    const SliderMapping& mapping = separation.slider->mapping();
    EXPECT_FALSE(mapping.isLinear());
    EXPECT_EQ(mapping.getMin(), 0.0);
    EXPECT_EQ(mapping.getMid(), 1.0);
    EXPECT_EQ(mapping.getMax(), 4.0);
    // Opening the dialog stores nothing.
    EXPECT_FALSE(
        services.preferences().get(GuiPreferenceKeys::kInnerTubeSeparationRelative).has_value());
}

TEST_F(InnerTubeConfigTest, TheRelativeSeparationIsAtLeastZero)
{
    // D2aProbe: DoubleModel(inner, "ClusterScale", 1, UNITS_NONE, 0).
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel = tubePanel(created);
    panel.relativeSeparation()->setValue(-3);
    EXPECT_EQ(design.inner->getClusterScale(), 0.0);
    panel.relativeSeparation()->setValue(1.5);
    EXPECT_EQ(design.inner->getClusterScale(), 1.5);
    EXPECT_EQ(panel.absoluteSeparation()->value(), 0.0095);
}

TEST_F(InnerTubeConfigTest, AbsoluteStoresFalseAndRelativeStoresTrue)
{
    // The user's decision U4: Java's "Relative" stores InnerTubeSeparationRelative = false
    // (InnerTubeConfig.java:353) as "Absolute" does; here "Relative" stores true.
    ConfigDialogServices   services;
    const RingDesign       design;
    const FactoryPanel     created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig& panel                  = tubePanel(created);
    const ConfigFields::NumberWidgets& separation = panel.separationWidgets();
    panel.absoluteButton().click();
    EXPECT_EQ(services.preferences().get(GuiPreferenceKeys::kInnerTubeSeparationRelative),
              std::optional<std::string>("false"));
    EXPECT_FALSE(panel.isSeparationRelative());
    EXPECT_EQ(separation.spinBox->binding(), panel.absoluteSeparation());
    EXPECT_EQ(separation.unitSelector->binding(), panel.absoluteSeparation());
    EXPECT_EQ(separation.slider->binding(), panel.absoluteSeparation());
    EXPECT_EQ(separation.spinBox->objectName(), QStringLiteral("ClusterScaleAbsolute"));
    EXPECT_EQ(separation.unitSelector->objectName(), QStringLiteral("ClusterScaleAbsolute"));
    EXPECT_EQ(separation.spinBox->toolTip(),
              QStringLiteral("The separation of the tubes, 0 = touching each other"));
    // D2aProbe: DoubleModel(inner, "OuterRadius", -2) and (..., 6): -0.019 and 0.057.
    const SliderMapping& mapping = separation.slider->mapping();
    EXPECT_TRUE(mapping.isLinear());
    EXPECT_EQ(mapping.getMin(), -0.019);
    EXPECT_EQ(mapping.getMax(), 0.056999999999999995);

    panel.relativeButton().click();
    EXPECT_EQ(services.preferences().get(GuiPreferenceKeys::kInnerTubeSeparationRelative),
              std::optional<std::string>("true"));
    EXPECT_TRUE(panel.isSeparationRelative());
    EXPECT_EQ(separation.spinBox->binding(), panel.relativeSeparation());
    EXPECT_EQ(separation.spinBox->objectName(), QStringLiteral("ClusterScale"));
    EXPECT_FALSE(separation.slider->mapping().isLinear());
}

TEST_F(InnerTubeConfigTest, TheStoredChoiceOpensTheAbsoluteSeparation)
{
    ConfigDialogServices services;
    services.preferences().putBoolean(GuiPreferenceKeys::kInnerTubeSeparationRelative, false);
    const RingDesign       design;
    const FactoryPanel     created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig& panel = tubePanel(created);
    EXPECT_FALSE(panel.isSeparationRelative());
    EXPECT_TRUE(panel.absoluteButton().isChecked());
    EXPECT_FALSE(panel.relativeButton().isChecked());
    EXPECT_EQ(panel.separationWidgets().spinBox->binding(), panel.absoluteSeparation());
    EXPECT_EQ(panel.separationWidgets().slider->mapping().getMin(), -0.019);
}

TEST_F(InnerTubeConfigTest, TheAbsoluteSeparationIsTheGapBetweenTheTubes)
{
    // D2aProbe: DoubleModel(inner, "ClusterScaleAbsolute", 1, UNITS_LENGTH), no bounds.
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel = tubePanel(created);
    EXPECT_EQ(panel.absoluteSeparation()->value(), 0.0);
    panel.absoluteSeparation()->setValue(-0.03);
    EXPECT_EQ(design.inner->getClusterScale(), 0.0);
    EXPECT_EQ(panel.absoluteSeparation()->value(), -0.019);
    panel.absoluteSeparation()->setValue(0.005);
    EXPECT_EQ(design.inner->getClusterScale(), 1.263157894736842);
    EXPECT_EQ(panel.absoluteSeparation()->value(), 0.004999999999999998);
}

TEST_F(InnerTubeConfigTest, TheAbsoluteSliderFollowsTheOuterRadius)
{
    ConfigDialogServices services;
    services.preferences().putBoolean(GuiPreferenceKeys::kInnerTubeSeparationRelative, false);
    const RingDesign       design;
    const FactoryPanel     created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig& panel = tubePanel(created);
    panel.generalTab().outerRadius.spinBox->binding()->setValue(0.04);
    EXPECT_EQ(design.inner->getOuterRadius(), 0.02);
    EXPECT_EQ(panel.separationWidgets().slider->mapping().getMin(), -0.04);
    EXPECT_EQ(panel.separationWidgets().slider->mapping().getMax(), 0.12);
}

TEST_F(InnerTubeConfigTest, TheRotationIsAtMostHalfATurn)
{
    // D2aProbe: DoubleModel(inner, "ClusterRotation", 1, UNITS_ANGLE, -pi, pi) clamps 4.0 to pi.
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel            = tubePanel(created);
    const ConfigFields::NumberWidgets& rotation = panel.rotationWidgets();
    EXPECT_EQ(rotation.spinBox->binding()->minimum(), -std::numbers::pi);
    EXPECT_EQ(rotation.spinBox->binding()->maximum(), std::numbers::pi);
    EXPECT_TRUE(rotation.slider->mapping().isLinear());
    EXPECT_EQ(rotation.slider->mapping().getMin(), -std::numbers::pi);
    EXPECT_EQ(rotation.slider->mapping().getMax(), std::numbers::pi);
    rotation.spinBox->binding()->setValue(4.0);
    EXPECT_EQ(design.inner->getClusterRotation(), std::numbers::pi);
    rotation.spinBox->binding()->setValue(-1.0);
    EXPECT_EQ(design.inner->getClusterRotation(), -1.0);
}

TEST_F(InnerTubeConfigTest, TheClusterEditsReachEverySelectedTube)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<InnerTube>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel     created(*design.document, services.services(), *design.inner, others);
    const InnerTubeConfig& panel = tubePanel(created);
    panel.clusterSelection().select(layout(5));
    EXPECT_EQ(&design.inner->getClusterConfiguration(), &layout(5));
    EXPECT_EQ(&other.getClusterConfiguration(), &layout(5));
    panel.relativeSeparation()->setValue(1.5);
    panel.rotationWidgets().spinBox->binding()->setValue(0.4);
    EXPECT_EQ(other.getClusterScale(), 1.5);
    EXPECT_EQ(other.getClusterRotation(), 0.4);
    // Reset settings: scale 1 and rotation 0 for both.
    panel.resetClusterButton().click();
    EXPECT_EQ(design.inner->getClusterScale(), 1.0);
    EXPECT_EQ(design.inner->getClusterRotation(), 0.0);
    EXPECT_EQ(other.getClusterScale(), 1.0);
    EXPECT_EQ(other.getClusterRotation(), 0.0);
    EXPECT_EQ(&other.getClusterConfiguration(), &layout(5));
}

TEST_F(InnerTubeConfigTest, TheAbsoluteSeparationGivesEachTubeTheGapTyped)
{
    // The user's decision U3: Java's setClusterScaleAbsolute() computes the relative scale from
    // the MAIN tube's outer radius and forwards that scale, so the other tube (outer radius 2 cm)
    // gets 1.526315789473684 too, a gap of 0.021052631578947364 (D2aProbe); here each tube gets
    // the gap typed.
    const ConfigDialogServices services;
    const RingDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<InnerTube>());
    other.setOuterRadius(0.02);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel     created(*design.document, services.services(), *design.inner, others);
    const InnerTubeConfig& panel = tubePanel(created);
    panel.absoluteSeparation()->setValue(0.01);
    EXPECT_EQ(design.inner->getClusterScale(), 1.526315789473684);
    EXPECT_EQ(design.inner->getClusterScaleAbsolute(), 0.009999999999999997);
    EXPECT_EQ(other.getClusterScale(), 1.25);
    EXPECT_EQ(other.getClusterScaleAbsolute(), 0.01);
}

TEST_F(InnerTubeConfigTest, TheRadialPositionTabIsJavas)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    const FactoryPanel         created(*design.document, services.services(), *design.inner);
    const InnerTubeConfig&     panel             = tubePanel(created);
    const ConfigFields::NumberWidgets& position  = panel.radialPositionWidgets();
    const ConfigFields::NumberWidgets& direction = panel.radialDirectionWidgets();
    EXPECT_EQ(position.label->text(), QStringLiteral("Radial distance:"));
    EXPECT_EQ(position.label->toolTip(), QStringLiteral("Distance from the rocket centerline"));
    EXPECT_EQ(position.spinBox->toolTip(), QStringLiteral("Distance from the rocket centerline"));
    EXPECT_EQ(position.slider->toolTip(), QStringLiteral("Distance from the rocket centerline"));
    EXPECT_EQ(position.spinBox->objectName(), QStringLiteral("RadialPosition"));
    EXPECT_EQ(position.spinBox->binding()->minimum(), 0.0);
    EXPECT_FALSE(position.slider->mapping().isLinear());
    EXPECT_EQ(position.slider->mapping().getMid(), 0.1);
    EXPECT_EQ(position.slider->mapping().getMax(), 1.0);
    EXPECT_EQ(direction.label->text(), QStringLiteral("Radial direction:"));
    EXPECT_EQ(direction.spinBox->toolTip(),
              QStringLiteral("The radial direction from the rocket centerline"));
    EXPECT_EQ(direction.spinBox->objectName(), QStringLiteral("RadialDirection"));
    EXPECT_TRUE(std::isinf(direction.spinBox->binding()->minimum()));
    EXPECT_TRUE(direction.slider->mapping().isLinear());
    EXPECT_EQ(direction.slider->mapping().getMin(), -std::numbers::pi);
    EXPECT_EQ(panel.resetRadialButton().text(), QStringLiteral("Reset"));
    EXPECT_EQ(panel.resetRadialButton().toolTip(),
              QStringLiteral("Resets the component to the rocket centerline."));
    EXPECT_EQ(panel.radialNote().toPlainText(),
              QStringLiteral("Note: An inner tube will not affect the aerodynamics of the rocket "
                             "even if it is located outside of the body tube."));
}

TEST_F(InnerTubeConfigTest, TheRadialResetCentersEverySelectedTube)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    auto&                      other = design.body->addChild(std::make_unique<InnerTube>());
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const FactoryPanel     created(*design.document, services.services(), *design.inner, others);
    const InnerTubeConfig& panel = tubePanel(created);
    panel.radialPositionWidgets().spinBox->binding()->setValue(0.004);
    panel.radialDirectionWidgets().spinBox->binding()->setValue(0.5);
    EXPECT_EQ(other.getRadialPosition(), 0.004);
    EXPECT_EQ(other.getRadialDirection(), 0.5);
    panel.radialPositionWidgets().spinBox->binding()->setValue(-1.0);
    EXPECT_EQ(design.inner->getRadialPosition(), 0.0);
    panel.radialPositionWidgets().spinBox->binding()->setValue(0.004);
    panel.resetRadialButton().click();
    EXPECT_EQ(design.inner->getRadialPosition(), 0.0);
    EXPECT_EQ(design.inner->getRadialDirection(), 0.0);
    EXPECT_EQ(other.getRadialPosition(), 0.0);
    EXPECT_EQ(other.getRadialDirection(), 0.0);
}

TEST_F(InnerTubeConfigTest, AClusterChosenInTheDialogIsUndoneWithItsSession)
{
    const ConfigDialogServices   services;
    const RingDesign             design;
    QWidget                      window;
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->tabs().setCurrentIndex(2);
    QTest::mouseClick(&panel->clusterSelection().button(6), Qt::LeftButton, Qt::NoModifier,
                      QPoint(25, 25));
    EXPECT_EQ(&design.inner->getClusterConfiguration(), &layout(6));
    EXPECT_EQ(design.inner->getInstanceCount(), 5);
    EXPECT_TRUE(panel->clusterSelection().button(6).isSelected());
    EXPECT_FALSE(panel->clusterSelection().button(0).isSelected());
    EXPECT_TRUE(dialog->isModified());
    panel->okButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_EQ(design.document->getUndoDescription(), "Modify Inner Tube");
    design.document->undo();
    const auto* tube =
        dynamic_cast<const InnerTube*>(bodyOf(*design.document).getChildren().front());
    ASSERT_NE(tube, nullptr);
    EXPECT_EQ(&tube->getClusterConfiguration(), &ClusterConfiguration::single());
}

TEST_F(InnerTubeConfigTest, SplitClusterMakesEachTubeAComponentAsJavaDoes)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    makeProbeCluster(design);
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    RocketEventRecorder events(design.document->getRocket());
    panel->splitButton().click();
    // The TREE change closed the dialog.
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // D2FixerProbe: no freeze, as Java; the removal and the three additions each tell the rocket
    // a TREE and mass change of the body tube.
    constexpr int kAddRemove =
        ComponentChangeEvent::kTreeChange | ComponentChangeEvent::kMassChange;
    EXPECT_EQ(events.types(), (std::vector<int>{kAddRemove, kAddRemove, kAddRemove, kAddRemove}));
    EXPECT_EQ(events.sources(), (std::vector<std::string>(4, "Body Tube")));

    const RocketComponent& body = bodyOf(*design.document);
    EXPECT_EQ(body.getChildCount(), 6U);
    EXPECT_EQ(splitDifferences(body, probeThreeRingSplit()), "");
    EXPECT_EQ(body.getChildren().at(0)->getChildren().front()->getName(), "Engine Block");
    EXPECT_NE(body.getChildren().at(0)->getChildren().front()->getId(),
              body.getChildren().at(1)->getChildren().front()->getId());
    EXPECT_EQ(body.getChildren().at(3)->getName(), "Tube Coupler");

    // One undo step of its own: the cluster comes back.
    EXPECT_EQ(design.document->getUndoDescription(), "Split cluster");
    design.document->undo();
    const RocketComponent& restored = bodyOf(*design.document);
    EXPECT_EQ(restored.getChildCount(), 4U);
    const auto* tube = dynamic_cast<const InnerTube*>(restored.getChildren().front());
    ASSERT_NE(tube, nullptr);
    EXPECT_EQ(&tube->getClusterConfiguration(), &layout(4));
    EXPECT_EQ(tube->getClusterScale(), 1.5);
    EXPECT_EQ(tube->getChildCount(), 1U);
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(InnerTubeConfigTest, AnEditBeforeTheSplitStaysAStepOfItsOwn)
{
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    makeProbeCluster(design);
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->tabs().setCurrentIndex(2);
    // A pending rotation (in degrees), committed by the split.
    typeInto(*panel->rotationWidgets().spinBox, QStringLiteral("90"));
    panel->splitButton().click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(bodyOf(*design.document).getChildCount(), 6U);

    EXPECT_EQ(design.document->getUndoDescription(), "Split cluster");
    design.document->undo();
    const auto* tube =
        dynamic_cast<const InnerTube*>(bodyOf(*design.document).getChildren().front());
    ASSERT_NE(tube, nullptr);
    EXPECT_EQ(&tube->getClusterConfiguration(), &layout(4));
    EXPECT_DOUBLE_EQ(tube->getClusterRotation(), std::numbers::pi / 2);
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Inner Tube");
    design.document->undo();
    const auto* original =
        dynamic_cast<const InnerTube*>(bodyOf(*design.document).getChildren().front());
    ASSERT_NE(original, nullptr);
    EXPECT_EQ(original->getClusterRotation(), 0.4);
}

TEST_F(InnerTubeConfigTest, EverySelectedTubeIsSplitTheMainFirst)
{
    // D2aProbe: a main 4-ring "Main" and a selected 2-tube "Other" (outer radius 12 mm) in a
    // second body tube.
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    RocketComponent&           stage = *design.document->getRocket().getChildren().front();
    auto&                      body2 = stage.addChild(std::make_unique<BodyTube>());
    auto&                      main  = body2.addChild(std::make_unique<InnerTube>());
    main.setName("Main");
    main.setClusterConfiguration(layout(5));
    auto& other = body2.addChild(std::make_unique<InnerTube>());
    other.setName("Other");
    other.setClusterConfiguration(layout(1));
    other.setOuterRadius(0.012);
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&other};
    const ComponentConfigDialog*          dialog =
        showActiveDialog(window, services.services(), *design.document, main, others);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->splitButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const RocketComponent& split =
        *design.document->getRocket().getChildren().front()->getChildren().at(1);
    EXPECT_EQ(split.getChildCount(), 6U);
    const std::array<ProbeTube, 6> expected{{
        {.name            = "Main #1",
         .radialPosition  = 0.013435028842544402,
         .radialDirection = 2.356194490192345,
         .shiftY          = -0.009499999999999998,
         .shiftZ          = 0.0095,
         .outerRadius     = 0.0095,
         .children        = 0},
        {.name            = "Main #2",
         .radialPosition  = 0.013435028842544402,
         .radialDirection = 0.7853981633974483,
         .shiftY          = 0.0095,
         .shiftZ          = 0.009499999999999998,
         .outerRadius     = 0.0095,
         .children        = 0},
        {.name            = "Main #3",
         .radialPosition  = 0.013435028842544402,
         .radialDirection = -0.7853981633974483,
         .shiftY          = 0.0095,
         .shiftZ          = -0.009499999999999998,
         .outerRadius     = 0.0095,
         .children        = 0},
        {.name            = "Main #4",
         .radialPosition  = 0.013435028842544402,
         .radialDirection = -2.356194490192345,
         .shiftY          = -0.009499999999999998,
         .shiftZ          = -0.0095,
         .outerRadius     = 0.0095,
         .children        = 0},
        {.name            = "Other #1",
         .radialPosition  = 0.012,
         .radialDirection = std::numbers::pi,
         .shiftY          = -0.012,
         .shiftZ          = 1.4695761589768239E-18,
         .outerRadius     = 0.012,
         .children        = 0},
        {.name            = "Other #2",
         .radialPosition  = 0.012,
         .radialDirection = 0.0,
         .shiftY          = 0.012,
         .shiftZ          = 0.0,
         .outerRadius     = 0.012,
         .children        = 0},
    }};
    EXPECT_EQ(splitDifferences(split, expected), "");
    EXPECT_EQ(design.document->getUndoDescription(), "Split cluster");
}

TEST_F(InnerTubeConfigTest, ATubeSelectedInsideASplitTubeStaysWholeInTheCopies)
{
    // D2aProbe (body4), as Java's split button: the main tube is split first, its copies taking the
    // selected tube inside it as it is. Java then splits that tube inside the removed original,
    // which changes nothing in the rocket; here it is no longer found in the rocket and is passed
    // over. So the copies keep the inner cluster.
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    design.inner->setClusterConfiguration(layout(4));
    auto& nested = design.inner->addChild(std::make_unique<InnerTube>());
    nested.setName("Nested");
    nested.setClusterConfiguration(layout(1));
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&nested};
    const ComponentConfigDialog*          dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner, others);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->splitButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    EXPECT_EQ(bodyOf(*design.document).getChildCount(), 6U);
    EXPECT_EQ(nestedCopyDifferences(bodyOf(*design.document)), "");
}

TEST_F(InnerTubeConfigTest, ATubeSelectedAroundTheMainIsSplitAfterIt)
{
    // D2FixerProbe: the main 2-tube "Nested" inside the selected 3-ring "Outer" is split first, at
    // its six absolute locations (three of the outer one's instances times two); the outer one,
    // found again after the TREE changes of that split, is then split with copies of the six.
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    RocketComponent&           stage = *design.document->getRocket().getChildren().front();
    auto&                      body5 = stage.addChild(std::make_unique<BodyTube>());
    auto&                      outer = body5.addChild(std::make_unique<InnerTube>());
    outer.setName("Outer");
    outer.setClusterConfiguration(layout(4));
    auto& nested = outer.addChild(std::make_unique<InnerTube>());
    nested.setName("Nested");
    nested.setClusterConfiguration(layout(1));
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&outer};
    const ComponentConfigDialog*          dialog =
        showActiveDialog(window, services.services(), *design.document, nested, others);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->splitButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const RocketComponent& split =
        *design.document->getRocket().getChildren().front()->getChildren().at(1);
    EXPECT_EQ(nestedSplitDifferences(split), "");
    EXPECT_EQ(design.document->getUndoDescription(), "Split cluster");
    design.document->undo();
    const RocketComponent& restored =
        *design.document->getRocket().getChildren().front()->getChildren().at(1);
    ASSERT_EQ(restored.getChildCount(), 1U);
    EXPECT_EQ(restored.getChildren().front()->getName(), "Outer");
    EXPECT_EQ(restored.getChildren().front()->getChildCount(), 1U);
}

TEST_F(InnerTubeConfigTest, ASingleTubeStaysAsItIs)
{
    // Java's splitAction() returns for a single tube; the undo position "Split cluster" is added
    // all the same (with nothing changed it only names the next step).
    const ConfigDialogServices   services;
    const RingDesign             design;
    QWidget                      window;
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->splitButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
    EXPECT_EQ(bodyOf(*design.document).getChildCount(), 4U);
    EXPECT_EQ(bodyOf(*design.document).getChildren().front()->getName(), "Inner Tube");
    EXPECT_FALSE(design.document->isUndoAvailable());
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Split cluster");
    ComponentConfigDialog::disposeDialog();
}

TEST_F(InnerTubeConfigTest, ATubeWithoutParentCannotBeSplit)
{
    InnerTube tube;
    tube.setClusterConfiguration(layout(4));
    EXPECT_THROW(static_cast<void>(InnerTubeConfig::splitClusterOf(tube)), BugError);
}

TEST_F(InnerTubeConfigTest, AnUndoWhileOpenClosesTheDialogAndDropsThePendingEdit)
{
    // Proved under the asan preset: the pending rotation, committed by the focus loss of the
    // hidden dialog, reaches none of the components the undo replaced.
    const ConfigDialogServices services;
    const RingDesign           design;
    QWidget                    window;
    design.document->addUndoPosition("Change");
    design.coupler->setName("Joint");
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.inner);
    ASSERT_NE(dialog, nullptr);
    const auto* panel = dynamic_cast<const InnerTubeConfig*>(&dialog->configurator());
    ASSERT_NE(panel, nullptr);
    panel->tabs().setCurrentIndex(2);
    typeInto(*panel->rotationWidgets().spinBox, QStringLiteral("45"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(dialog->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body = bodyOf(*design.document);
    const auto*            tube = dynamic_cast<const InnerTube*>(body.getChildren().front());
    ASSERT_NE(tube, nullptr);
    EXPECT_EQ(tube->getClusterRotation(), 0.0);
    EXPECT_EQ(body.getChildren().at(1)->getName(), "Tube Coupler");
}

}  // namespace

}  // namespace QtRocket::Gui
