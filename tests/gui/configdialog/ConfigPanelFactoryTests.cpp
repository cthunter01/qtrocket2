#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"

#include <array>
#include <exception>
#include <memory>
#include <span>
#include <string>

#include <QLayout>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QWidget>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/MaterialBinding.h"
#include "QtRocket/gui/configdialog/InstancesPanel.h"
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/PlacementPanel.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/ShoulderPanel.h"
#include "QtRocket/gui/widgets/MaterialComboBox.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using ConfigPanelFactoryTest = Test::GuiTest;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::tabTitles;

/// A panel the factory made, with the widget that owns it.
struct CreatedPanel
{
    std::unique_ptr<QWidget> host = std::make_unique<QWidget>();
    RocketComponentConfig*   panel{nullptr};

    RocketComponentConfig* operator->() const noexcept { return panel; }
    RocketComponentConfig& operator*() const noexcept { return *panel; }
};

/// The panel of @p main (and @p others).
[[nodiscard]] CreatedPanel createdPanel(OpenRocketDocument&         document,
                                        const ConfigDialogServices& services, RocketComponent& main,
                                        std::span<RocketComponent* const> others = {})
{
    CreatedPanel created;
    created.panel = ConfigPanelFactory::create(document, services.services(),
                                               std::make_shared<ComponentSelection>(main, others),
                                               created.host.get());
    return created;
}

TEST_F(ConfigPanelFactoryTest, TheSharedPanelsOfEachKindAreThoseOfItsJavaPanel)
{
    using SharedPanels     = ConfigPanelFactory::SharedPanels;
    const SharedPanels lug = ConfigPanelFactory::sharedPanels(ComponentKind::LAUNCH_LUG);
    EXPECT_TRUE(lug.instances && lug.placement && lug.material && !lug.motor);
    const SharedPanels body = ConfigPanelFactory::sharedPanels(ComponentKind::BODY_TUBE);
    EXPECT_TRUE(!body.placement && body.material && body.motor && !body.shoulder);
    const SharedPanels nose = ConfigPanelFactory::sharedPanels(ComponentKind::NOSE_CONE);
    EXPECT_TRUE(nose.material && nose.shoulder && !nose.placement);
    const SharedPanels cord = ConfigPanelFactory::sharedPanels(ComponentKind::SHOCK_CORD);
    EXPECT_TRUE(cord.placement && cord.material && cord.materialFirst && cord.shockCordMaterial);
    EXPECT_EQ(cord.materialType, Material::Type::LINE);
    const SharedPanels pods = ConfigPanelFactory::sharedPanels(ComponentKind::POD_SET);
    EXPECT_TRUE(pods.placement && pods.assemblyTab && !pods.material);
    const SharedPanels chute = ConfigPanelFactory::sharedPanels(ComponentKind::PARACHUTE);
    EXPECT_TRUE(chute.placement && !chute.material);
    const SharedPanels stage = ConfigPanelFactory::sharedPanels(ComponentKind::AXIAL_STAGE);
    EXPECT_FALSE(stage.instances || stage.placement || stage.material || stage.motor ||
                 stage.shoulder);
}

TEST_F(ConfigPanelFactoryTest, TheGeneralTabHoldsTheSharedPanelsInJavasOrder)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto                 lug =
        createdPanel(*design.document, services, design.component(ComponentKind::LAUNCH_LUG));
    ASSERT_GE(lug->tabs().count(), 1);
    const QWidget* general   = lug->tabs().widget(0);
    const auto*    instances = general->findChild<InstancesPanel*>();
    const auto*    placement = general->findChild<PlacementPanel*>();
    const auto*    material  = general->findChild<MaterialPanel*>();
    ASSERT_NE(instances, nullptr);
    ASSERT_NE(placement, nullptr);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(general->layout()->indexOf(instances), 0);
    EXPECT_EQ(general->layout()->indexOf(placement), 1);
    EXPECT_EQ(general->layout()->indexOf(material), 2);
    EXPECT_EQ(lug->tabs().tabToolTip(0), QStringLiteral("General properties"));

    // The shared panels of a shock cord (ShockCordConfig is the factory's panel of the kind).
    CreatedPanel cord;
    cord.panel = ConfigPanelFactory::createWithSharedPanels(
        *design.document, services.services(),
        std::make_shared<ComponentSelection>(design.component(ComponentKind::SHOCK_CORD)),
        cord.host.get());
    const QWidget* cordGeneral   = cord->tabs().widget(0);
    const auto*    cordMaterial  = cordGeneral->findChild<MaterialPanel*>();
    const auto*    cordPlacement = cordGeneral->findChild<PlacementPanel*>();
    ASSERT_NE(cordMaterial, nullptr);
    ASSERT_NE(cordPlacement, nullptr);
    EXPECT_EQ(cordGeneral->layout()->indexOf(cordMaterial), 0);
    EXPECT_EQ(cordGeneral->layout()->indexOf(cordPlacement), 1);
    EXPECT_EQ(cordMaterial->materialComboBox().binding()->getType(), Material::Type::LINE);

    const auto pods =
        createdPanel(*design.document, services, design.component(ComponentKind::POD_SET));
    EXPECT_EQ(pods->tabs().tabToolTip(0),
              QStringLiteral("Options for locating stages parallel to other stages"));

    const auto body =
        createdPanel(*design.document, services, design.component(ComponentKind::BODY_TUBE));
    EXPECT_NE(qobject_cast<MotorConfig*>(body->tabs().widget(1)), nullptr);
    EXPECT_EQ(body->tabs().tabToolTip(1), QStringLiteral("Motor mount configuration"));
    const auto nose =
        createdPanel(*design.document, services, design.component(ComponentKind::NOSE_CONE));
    EXPECT_NE(qobject_cast<ShoulderPanel*>(nose->tabs().widget(1)), nullptr);
    EXPECT_EQ(nose->tabs().tabToolTip(1), QStringLiteral("Shoulder properties"));
}

TEST_F(ConfigPanelFactoryTest, SeveralClassesGetThePlainConfiguration)
{
    const ConfigDialogServices            services;
    const EveryKindDesign                 design;
    const std::array<RocketComponent*, 2> others{&design.component(ComponentKind::NOSE_CONE),
                                                 &design.component(ComponentKind::INNER_TUBE)};
    const auto panel = createdPanel(*design.document, services,
                                    design.component(ComponentKind::BODY_TUBE), others);
    EXPECT_EQ(tabTitles(*panel),
              (QStringList{QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(panel->findChild<MaterialPanel*>(), nullptr);
}

TEST_F(ConfigPanelFactoryTest, AnInvalidSelectionIsABug)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    QWidget                    host;
    EXPECT_THROW(static_cast<void>(ConfigPanelFactory::create(*design.document, services.services(),
                                                              nullptr, &host)),
                 BugError);
}

/// The panel of every component of @p document, each made, refreshed and released: the names
/// of the components whose panel threw, each with what it threw.
[[nodiscard]] std::string panelFailures(OpenRocketDocument&         document,
                                        const ConfigDialogServices& services)
{
    std::string failures;
    for (RocketComponent& component : document.getRocket().subtree())
    {
        try
        {
            const auto panel = createdPanel(document, services, component);
            panel->updateFields();
            if (panel->tabs().count() < 2)
            {
                failures += component.getName() + ": fewer than two tabs\n";
            }
        }
        catch (const std::exception& exception)
        {
            failures += component.getName() + ": " + exception.what() + '\n';
        }
    }
    return failures;
}

TEST_F(ConfigPanelFactoryTest, EveryComponentOfTheTestRocketsGetsItsPanel)
{
    const ConfigDialogServices services;
    for (const QtRocket::Test::TestRocketMaker& maker : QtRocket::Test::testRocketMakers())
    {
        const auto document = OpenRocketDocumentFactory::createDocumentFromRocket(maker.make());
        EXPECT_EQ(panelFailures(*document, services), "") << maker;
    }
}

/// Seven of OpenRocket's example designs (data/examples), which hold every kind of component but
/// the transition between them (the test rockets have it). Not all sixteen: each panel runs a
/// force analysis of the rocket for its CD, which takes a few seconds for these seven in a Debug
/// build and several times that under the sanitizers.
constexpr std::array<const char*, 7> kExamples{
    "A simple model rocket.ork",
    "Clustered motors.ork",
    "Dual parachute deployment.ork",
    "Parallel booster staging.ork",
    "Pods--airframes and winglets.ork",
    "Pods--powered with recovery deployment.ork",
    "Tube fin rocket.ork",
};

/// panelFailures() of kExamples, each named.
[[nodiscard]] std::string examplePanelFailures(const ConfigDialogServices& services)
{
    const QtRocket::Test::DesignFileEnvironment environment;
    std::string                                 failures;
    for (const char* file : kExamples)
    {
        Result<LoadedDocument> loaded = GeneralRocketLoader(environment.context())
                                            .load(QtRocket::Test::dataDir() / "examples" / file);
        if (!loaded.has_value())
        {
            failures += std::string(file) + ": " + loaded.error().message + '\n';
            continue;
        }
        const std::string panels = panelFailures(*loaded->document, services);
        if (!panels.empty())
        {
            failures += std::string(file) + ":\n" + panels;
        }
    }
    return failures;
}

TEST_F(ConfigPanelFactoryTest, EveryComponentOfSevenExamplesGetsItsPanel)
{
    const ConfigDialogServices services;
    EXPECT_EQ(examplePanelFailures(services), "");
}

}  // namespace

}  // namespace QtRocket::Gui
