#include "QtRocket/gui/configdialog/ConfigPanelFactory.h"

#include <array>
#include <exception>
#include <memory>
#include <span>
#include <string>

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
#include "QtRocket/gui/configdialog/MaterialPanel.h"
#include "QtRocket/gui/configdialog/MotorConfig.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/configdialog/ShoulderPanel.h"
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

TEST_F(ConfigPanelFactoryTest, TheBodyTubeAndTheNoseConeHaveTheirSecondTabs)
{
    const ConfigDialogServices services;
    const EveryKindDesign      design;
    const auto                 body =
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
