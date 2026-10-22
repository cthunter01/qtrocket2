#include "QtRocket/gui/configdialog/ComponentAssemblyConfig.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "AssemblyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "MassPanelTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/binding/EnumBinding.h"
#include "QtRocket/gui/binding/IntBinding.h"
#include "QtRocket/gui/binding/SliderMapping.h"
#include "QtRocket/gui/configdialog/AxialStageConfig.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/ConfigFields.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/IntSpinBox.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/gui/widgets/ValueSlider.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"
#include "binding/BindingTestSupport.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentAssemblyConfigTest = Test::ConfigDialogTest;
using Test::AssemblyDesign;
using Test::comboItemTexts;
using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::labelTextsUnder;
using Test::RocketEventRecorder;
using Test::showActiveDialog;
using Test::tabTitles;
using Test::typeInto;

/// The panel of @p created as a ComponentAssemblyConfig.
[[nodiscard]] ComponentAssemblyConfig& assemblyPanel(const FactoryPanel& created)
{
    auto* panel = created.as<ComponentAssemblyConfig>();
    if (panel == nullptr)
    {
        bug("not an assembly's panel");
    }
    return *panel;
}

/// The panel of @p dialog as a ComponentAssemblyConfig.
[[nodiscard]] const ComponentAssemblyConfig& assemblyPanel(const ComponentConfigDialog& dialog)
{
    const auto* panel = dynamic_cast<const ComponentAssemblyConfig*>(&dialog.configurator());
    if (panel == nullptr)
    {
        bug("not an assembly's panel");
    }
    return *panel;
}

/// The number of TREE changes in @p types.
[[nodiscard]] std::size_t treeChangeCount(const std::vector<int>& types)
{
    std::size_t count = 0;
    for (const int type : types)
    {
        if ((type & ComponentChangeEvent::kTreeChange) != 0)
        {
            ++count;
        }
    }
    return count;
}

/// What a child of a split shows: its name, instance count, angle offset, override mass and
/// number of children.
struct SplitChild
{
    std::string name;
    int         count;
    double      angle;
    double      overrideMass;
    std::size_t children;
};

/// How the children of @p parent differ from @p expected, one line each; empty when they match
/// exactly.
[[nodiscard]] std::string splitDifferences(const RocketComponent&         parent,
                                           const std::vector<SplitChild>& expected)
{
    std::string                               differences;
    const std::vector<const RocketComponent*> children = parent.getChildren();
    if (children.size() < expected.size())
    {
        return std::format("{} children, expected at least {}\n", children.size(), expected.size());
    }
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        const RocketComponent& child = *children.at(i);
        const SplitChild&      want  = expected.at(i);
        if (child.getName() != want.name || child.getInstanceCount() != want.count ||
            child.getAngleOffset() != want.angle || child.getOverrideMass() != want.overrideMass ||
            child.getChildCount() != want.children)
        {
            differences +=
                std::format("child {}: '{}' count {} angle {} mass {} children {}\n", i,
                            child.getName(), child.getInstanceCount(), child.getAngleOffset(),
                            child.getOverrideMass(), child.getChildCount());
        }
    }
    return differences;
}

/// The number of the children of @p parent whose mass is overridden.
[[nodiscard]] int massOverriddenCount(const RocketComponent& parent)
{
    int count = 0;
    for (const RocketComponent* child : parent.getChildren())
    {
        count += child->isMassOverridden() ? 1 : 0;
    }
    return count;
}

/// The binding of the instance count of @p widgets.
[[nodiscard]] IntBinding& countOf(const ComponentAssemblyConfig::AssemblyWidgets& widgets)
{
    const std::shared_ptr<IntBinding>& binding = widgets.count->binding();
    return *binding;
}

/// The body tube of the first stage of @p document's rocket (found again after an undo).
[[nodiscard]] const RocketComponent& firstBody(const OpenRocketDocument& document)
{
    return *document.getRocket().getChildren().front()->getChildren().front();
}

TEST_F(ComponentAssemblyConfigTest, APodSetGetsJavasGeneralTabFirst)
{
    const ConfigDialogServices     services;
    const AssemblyDesign           design;
    const FactoryPanel             created(*design.document, services.services(), *design.pods);
    const ComponentAssemblyConfig& panel = assemblyPanel(created);
    EXPECT_EQ(tabTitles(panel), (QStringList{QStringLiteral("General"), QStringLiteral("Override"),
                                             QStringLiteral("Comment")}));
    EXPECT_EQ(panel.tabs().tabToolTip(0),
              QStringLiteral("Options for locating stages parallel to other stages"));
    EXPECT_EQ(panel.selectedTabIndex(), 0);
    const ComponentAssemblyConfig::AssemblyWidgets& widgets = panel.assemblyWidgets();
    ASSERT_NE(widgets.page, nullptr);
    EXPECT_EQ(panel.tabs().widget(0), widgets.page);
    // The placement's labels come last.
    EXPECT_EQ(
        labelTextsUnder(*widgets.page).mid(0, 4),
        (QStringList{QStringLiteral("Radius relative to:"), QStringLiteral("Radial Distance:"),
                     QStringLiteral("Angle:"), QStringLiteral("Number of Copies:")}));
    EXPECT_EQ(widgets.page->findChild<PlacementPanel*>(), widgets.placement);
    EXPECT_EQ(widgets.radiusMethod->objectName(), QStringLiteral("RadiusMethod"));
    EXPECT_EQ(widgets.radius.spinBox->objectName(), QStringLiteral("RadiusOffset"));
    EXPECT_EQ(widgets.angle.spinBox->objectName(), QStringLiteral("AngleOffset"));
    EXPECT_EQ(widgets.count->objectName(), QStringLiteral("InstanceCount"));
}

TEST_F(ComponentAssemblyConfigTest, ThePodSetShowsJavasValues)
{
    // D2cProbe: count 2, "Surface of the parent component" (RELATIVE), offset 0, angle 0, the
    // radius slider up to 4 times the body's 2.5 cm, placed at the bottom of the parent.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.pods);
    const ComponentAssemblyConfig::AssemblyWidgets& widgets =
        assemblyPanel(created).assemblyWidgets();
    EXPECT_EQ(comboItemTexts(*widgets.radiusMethod),
              (QStringList{QStringLiteral("Center of the parent component"),
                           QStringLiteral("Surface of the parent component")}));
    EXPECT_EQ(widgets.radiusMethod->shownText(), QStringLiteral("Surface of the parent component"));
    EXPECT_EQ(widgets.count->value(), 2);
    EXPECT_EQ(widgets.radius.spinBox->binding()->value(), 0.0);
    EXPECT_EQ(widgets.angle.spinBox->binding()->value(), 0.0);
    EXPECT_EQ(widgets.radius.slider->mapping().getMin(), 0.0);
    EXPECT_EQ(widgets.radius.slider->mapping().getMax(), 0.1);
    EXPECT_EQ(widgets.angle.slider->mapping().getMin(), -std::numbers::pi);
    EXPECT_EQ(widgets.angle.slider->mapping().getMax(), std::numbers::pi);
    EXPECT_EQ(widgets.placement->methodComboBox().shownText(),
              QStringLiteral("Bottom of the parent component"));
}

TEST_F(ComponentAssemblyConfigTest, TheRadiusSliderFollowsTheParentsRadius)
{
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.pods);
    const ValueSlider&         slider = *assemblyPanel(created).assemblyWidgets().radius.slider;
    design.body->setOuterRadius(0.03);
    EXPECT_DOUBLE_EQ(slider.mapping().getMax(), 0.12);
}

TEST_F(ComponentAssemblyConfigTest, TheFieldsKeepJavasBounds)
{
    // D2cProbe: the count model takes 0 as 1, the angle model 4 rad as pi.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.pods);
    const ComponentAssemblyConfig::AssemblyWidgets& widgets =
        assemblyPanel(created).assemblyWidgets();
    countOf(widgets).setValue(0);
    EXPECT_EQ(design.pods->getInstanceCount(), 1);
    widgets.angle.spinBox->binding()->setValue(4.0);
    EXPECT_EQ(design.pods->getAngleOffset(), std::numbers::pi);
    widgets.radius.spinBox->binding()->setValue(-0.01);
    EXPECT_EQ(design.pods->getRadiusOffset(), 0.0);
}

TEST_F(ComponentAssemblyConfigTest, TheRadiusMethodKeepsTheRadius)
{
    // D2cProbe: 5 cm from the surface of the 2.5 cm body are 7.5 cm from its center.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.pods);
    const ComponentAssemblyConfig::AssemblyWidgets& widgets =
        assemblyPanel(created).assemblyWidgets();
    widgets.radius.spinBox->binding()->setValue(0.05);
    const std::shared_ptr<EnumBinding<RadiusMethod>> method =
        widgets.radiusMethod->enumBinding<RadiusMethod>();
    ASSERT_NE(method, nullptr);
    Q_EMIT widgets.radiusMethod->activated(method->indexOf(RadiusMethod::FREE));
    EXPECT_EQ(design.pods->getRadiusMethod(), RadiusMethod::FREE);
    EXPECT_EQ(design.pods->getRadiusOffset(), 0.07500000000000001);
    EXPECT_EQ(widgets.radius.spinBox->binding()->value(), 0.07500000000000001);
    Q_EMIT widgets.radiusMethod->activated(method->indexOf(RadiusMethod::RELATIVE));
    EXPECT_EQ(design.pods->getRadiusOffset(), 0.05000000000000001);
}

TEST_F(ComponentAssemblyConfigTest, TheEditsReachEverySelectedAssembly)
{
    const ConfigDialogServices            services;
    const AssemblyDesign                  design;
    auto&                                 other = design.body->addChild(std::make_unique<PodSet>());
    const std::array<RocketComponent*, 1> others{&other};
    design.document->clearUndo();
    const FactoryPanel created(*design.document, services.services(), *design.pods, others);
    const ComponentAssemblyConfig::AssemblyWidgets& widgets =
        assemblyPanel(created).assemblyWidgets();
    countOf(widgets).setValue(4);
    widgets.angle.spinBox->binding()->setValue(0.5);
    widgets.radius.spinBox->binding()->setValue(0.02);
    for (const PodSet* pods : {design.pods, &other})
    {
        EXPECT_EQ(pods->getInstanceCount(), 4);
        EXPECT_EQ(pods->getAngleOffset(), 0.5);
        EXPECT_EQ(pods->getRadiusOffset(), 0.02);
    }
}

TEST_F(ComponentAssemblyConfigTest, ABoosterSetGetsSplitBoostersAPodSetSplitPods)
{
    const ConfigDialogServices     services;
    const AssemblyDesign           design;
    const FactoryPanel             pods(*design.document, services.services(), *design.pods);
    const ComponentAssemblyConfig& podPanel = assemblyPanel(pods);
    ASSERT_NE(podPanel.splitButton(), nullptr);
    EXPECT_EQ(podPanel.splitButton()->text(), QStringLiteral("Split Pods"));
    EXPECT_EQ(podPanel.splitButton()->toolTip(),
              QStringLiteral("Split the pod set into separate pod components."));
    EXPECT_EQ(podPanel.splitButton()->objectName(), QStringLiteral("splitButton"));
    EXPECT_TRUE(podPanel.splitButton()->isEnabled());

    const FactoryPanel boosters(*design.document, services.services(), *design.boosters);
    const ComponentAssemblyConfig& boosterPanel = assemblyPanel(boosters);
    EXPECT_NE(dynamic_cast<const AxialStageConfig*>(&boosterPanel), nullptr);
    ASSERT_NE(boosterPanel.splitButton(), nullptr);
    EXPECT_EQ(boosterPanel.splitButton()->text(), QStringLiteral("Split Boosters"));
    EXPECT_EQ(boosterPanel.splitButton()->toolTip(),
              QStringLiteral("Split the booster into separate booster components."));
}

TEST_F(ComponentAssemblyConfigTest, AStageOnTheCenterlineHasNeitherTabNorButton)
{
    const ConfigDialogServices     services;
    const AssemblyDesign           design;
    const FactoryPanel             created(*design.document, services.services(), *design.stage);
    const ComponentAssemblyConfig& panel = assemblyPanel(created);
    EXPECT_EQ(panel.assemblyWidgets().page, nullptr);
    EXPECT_EQ(panel.splitButton(), nullptr);
    EXPECT_FALSE(ComponentAssemblyConfig::isOffCenterline(ComponentKind::AXIAL_STAGE));
    EXPECT_TRUE(ComponentAssemblyConfig::isOffCenterline(ComponentKind::POD_SET));
    EXPECT_TRUE(ComponentAssemblyConfig::isOffCenterline(ComponentKind::PARALLEL_STAGE));
}

TEST_F(ComponentAssemblyConfigTest, TheSplitButtonFollowsTheInstanceCount)
{
    const ConfigDialogServices   services;
    const AssemblyDesign         design;
    QWidget                      window;
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.pods);
    ASSERT_NE(dialog, nullptr);
    const ComponentAssemblyConfig& panel = assemblyPanel(*dialog);
    EXPECT_TRUE(panel.splitButton()->isEnabled());
    countOf(panel.assemblyWidgets()).setValue(1);
    EXPECT_FALSE(panel.splitButton()->isEnabled());
    countOf(panel.assemblyWidgets()).setValue(3);
    EXPECT_TRUE(panel.splitButton()->isEnabled());
    ComponentConfigDialog::disposeDialog();
}

TEST_F(ComponentAssemblyConfigTest, SplitPodsMakesEachPodAComponentAsJavaDoes)
{
    // D2cProbe: three pods at 0.1 rad holding a body tube, their mass overridden to 0.3 kg, split
    // as ComponentAssemblyConfig does (inside "Split assembly").
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    QWidget                    window;
    design.pods->addChild(std::make_unique<BodyTube>());
    design.pods->setInstanceCount(3);
    design.pods->setAngleOffset(0.1);
    design.pods->setMassOverridden(true);
    design.pods->setOverrideMass(0.3);
    design.document->clearUndo();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.pods);
    ASSERT_NE(dialog, nullptr);
    RocketEventRecorder events(design.document->getRocket());
    assemblyPanel(*dialog).splitButton()->click();
    // The dialog closed first (Java's disposeDialog()).
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // D2cProbe: one change, the thaw's, a TREE change.
    EXPECT_EQ(events.count(), 1U);
    EXPECT_EQ(treeChangeCount(events.types()), 1U);
    const RocketComponent& body = firstBody(*design.document);
    EXPECT_EQ(body.getChildCount(), 4U);
    EXPECT_EQ(splitDifferences(body, {{"Pod Set #1", 1, 0.1, 0.09999999999999999, 1},
                                      {"Pod Set #2", 1, 2.1943951023931954, 0.09999999999999999, 1},
                                      {"Pod Set #3", 1, 4.28879020478639, 0.09999999999999999, 1}}),
              "");
    EXPECT_EQ(massOverriddenCount(body), 3);
    EXPECT_EQ(body.getChildren().at(3)->getName(), "Booster Set");

    // One undo step of its own: the pod set comes back.
    EXPECT_EQ(design.document->getUndoDescription(), "Split assembly");
    design.document->undo();
    EXPECT_EQ(splitDifferences(firstBody(*design.document), {{"Pod Set", 3, 0.1, 0.3, 1}}), "");
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(ComponentAssemblyConfigTest, AnEditBeforeTheSplitStaysAStepOfItsOwn)
{
    const ConfigDialogServices   services;
    const AssemblyDesign         design;
    QWidget                      window;
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.pods);
    ASSERT_NE(dialog, nullptr);
    const ComponentAssemblyConfig& panel = assemblyPanel(*dialog);
    // A pending count, committed before the split.
    typeInto(*panel.assemblyWidgets().count, QStringLiteral("3"));
    panel.splitButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(firstBody(*design.document).getChildCount(), 4U);
    EXPECT_EQ(firstBody(*design.document).getChildren().at(2)->getName(), "Pod Set #3");

    design.document->undo();
    EXPECT_EQ(splitDifferences(firstBody(*design.document), {{"Pod Set", 3, 0.0, 0.0, 0}}), "");
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Pod Set");
    design.document->undo();
    EXPECT_EQ(splitDifferences(firstBody(*design.document), {{"Pod Set", 2, 0.0, 0.0, 0}}), "");
}

TEST_F(ComponentAssemblyConfigTest, SplitBoostersMakesStagesWithoutAFreeze)
{
    // D2cProbe: two boosters holding a body tube become stages 1 and 2 of three; Java splits them
    // without a freeze, so every child removed or added tells the rocket.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    QWidget                    window;
    design.boosters->addChild(std::make_unique<BodyTube>());
    design.document->clearUndo();
    const ComponentConfigDialog* dialog =
        showActiveDialog(window, services.services(), *design.document, *design.boosters);
    ASSERT_NE(dialog, nullptr);
    RocketEventRecorder events(design.document->getRocket());
    assemblyPanel(*dialog).splitButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    // D2cProbe: three TREE changes, the removal and the two additions.
    EXPECT_EQ(events.count(), 3U);
    EXPECT_EQ(treeChangeCount(events.types()), 3U);
    const RocketComponent& body = firstBody(*design.document);
    EXPECT_EQ(splitDifferences(body, {{"Pod Set", 2, 0.0, 0.0, 0},
                                      {"Booster Set #1", 1, 0.0, 0.0, 1},
                                      {"Booster Set #2", 1, std::numbers::pi, 0.0, 1}}),
              "");
    EXPECT_EQ(design.document->getRocket().getStageCount(), 3U);
    EXPECT_EQ(body.getChildren().at(1)->getStageNumber(), 1);
    EXPECT_EQ(body.getChildren().at(2)->getStageNumber(), 2);
    EXPECT_EQ(design.document->getUndoDescription(), "Split assembly");
}

TEST_F(ComponentAssemblyConfigTest, EverySelectedPodSetIsSplitTheMainFirst)
{
    // Java splits the main pod set and the first other one, and leaves the second as it was
    // (D2cProbe: its loop removes the listeners it iterates); here every selected one is split.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    QWidget                    window;
    design.pods->setName("P0");
    auto& first = design.body->addChild(std::make_unique<PodSet>());
    first.setName("P1");
    auto& second = design.body->addChild(std::make_unique<PodSet>());
    second.setName("P2");
    design.document->clearUndo();
    const std::array<RocketComponent*, 2> others{&first, &second};
    const ComponentConfigDialog*          dialog =
        showActiveDialog(window, services.services(), *design.document, *design.pods, others);
    ASSERT_NE(dialog, nullptr);
    assemblyPanel(*dialog).splitButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body = firstBody(*design.document);
    EXPECT_EQ(splitDifferences(body, {{"P0 #1", 1, 0.0, 0.0, 0},
                                      {"P0 #2", 1, std::numbers::pi, 0.0, 0},
                                      {"Booster Set", 2, 0.0, 0.0, 0},
                                      {"P1 #1", 1, 0.0, 0.0, 0},
                                      {"P1 #2", 1, std::numbers::pi, 0.0, 0},
                                      {"P2 #1", 1, 0.0, 0.0, 0},
                                      {"P2 #2", 1, std::numbers::pi, 0.0, 0}}),
              "");
    // One undo step for all of them.
    design.document->undo();
    EXPECT_EQ(firstBody(*design.document).getChildCount(), 4U);
}

TEST_F(ComponentAssemblyConfigTest, APodSetInsideASplitOneIsSplitWithItsCopies)
{
    // Java splits the outer pod set and then throws (D2cProbe: IllegalStateException, the inner
    // original has no rocket); here the inner pod set stays in each copy as it was.
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    QWidget                    window;
    design.pods->setName("Outer");
    auto& tube  = design.pods->addChild(std::make_unique<BodyTube>());
    auto& inner = tube.addChild(std::make_unique<PodSet>());
    inner.setName("Inner");
    design.document->clearUndo();
    const std::array<RocketComponent*, 1> others{&inner};
    const ComponentConfigDialog*          dialog =
        showActiveDialog(window, services.services(), *design.document, *design.pods, others);
    ASSERT_NE(dialog, nullptr);
    assemblyPanel(*dialog).splitButton()->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const RocketComponent& body = firstBody(*design.document);
    EXPECT_EQ(splitDifferences(
                  body, {{"Outer #1", 1, 0.0, 0.0, 1}, {"Outer #2", 1, std::numbers::pi, 0.0, 1}}),
              "");
    for (std::size_t i = 0; i < 2; ++i)
    {
        EXPECT_EQ(splitDifferences(*body.getChildren().at(i)->getChildren().front(),
                                   {{"Inner", 2, 0.0, 0.0, 0}}),
                  "");
    }
    EXPECT_EQ(design.document->getUndoDescription(), "Split assembly");
}

TEST_F(ComponentAssemblyConfigTest, APanelOutsideTheDialogSplitsToo)
{
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    const FactoryPanel         created(*design.document, services.services(), *design.pods);
    ComponentAssemblyConfig&   panel = assemblyPanel(created);
    panel.splitInstances();
    EXPECT_FALSE(panel.selection()->isValid());
    EXPECT_EQ(firstBody(*design.document).getChildren().front()->getName(), "Pod Set #1");
    // A second press finds the selection ended: nothing happens.
    panel.splitInstances();
    EXPECT_EQ(firstBody(*design.document).getChildCount(), 3U);
}

TEST_F(ComponentAssemblyConfigTest, AComponentThatIsNoAssemblyIsABug)
{
    const ConfigDialogServices services;
    const AssemblyDesign       design;
    QWidget                    host;
    EXPECT_THROW(static_cast<void>(makeChild<ComponentAssemblyConfig>(
                     &host, *design.document, services.services(),
                     std::make_shared<ComponentSelection>(*design.body))),
                 BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
