#include "QtRocket/gui/util/GuiServices.h"

#include <type_traits>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/gui/util/MemoryPlotDefaults.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "RecordingSimulationDialogs.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

using GuiServicesTest = Test::GuiTest;

TEST_F(GuiServicesTest, HandsOutTheObjectsItWasMadeWith)
{
    InMemoryPreferences               preferences;
    MaterialStorage                   materials;
    const ThrustCurveMotorSetDatabase motors;
    const ComponentPresetDatabase     presets;
    const DocumentLoadingContext      context;
    Gui::Test::ScriptedUserPrompts    prompts;

    const GuiServices services(preferences, materials, motors, presets, context, prompts);
    EXPECT_EQ(&services.preferences(), &preferences);
    EXPECT_EQ(&services.materials(), &materials);
    EXPECT_EQ(&services.motors(), &motors);
    EXPECT_EQ(&services.presets(), &presets);
    EXPECT_EQ(&services.loadingContext(), &context);
    EXPECT_EQ(&services.prompts(), &prompts);

    // A copy refers to the same objects.
    const GuiServices copy = services;
    EXPECT_EQ(&copy.preferences(), &preferences);
    EXPECT_EQ(&copy.presets(), &presets);
    EXPECT_EQ(&copy.prompts(), &prompts);
}

// A plain value of borrowed references (GuiServices' class comment).
static_assert(std::is_trivially_copyable_v<GuiServices>);

TEST_F(GuiServicesTest, HasNoSimulationServicesUntilTheyAreSet)
{
    InMemoryPreferences               preferences;
    MaterialStorage                   materials;
    const ThrustCurveMotorSetDatabase motors;
    const ComponentPresetDatabase     presets;
    const DocumentLoadingContext      context;
    Gui::Test::ScriptedUserPrompts    prompts;
    GuiServices services(preferences, materials, motors, presets, context, prompts);
    // Without an executor nothing runs a simulation (the services a test makes of its own
    // objects).
    EXPECT_EQ(services.simulationExecutor(), nullptr);
    EXPECT_EQ(services.plotDefaults(), nullptr);
    EXPECT_EQ(services.simulationDialogs(), nullptr);

    Gui::Test::ManualSimulationExecutor   executor;
    MemoryPlotDefaults                    plotDefaults;
    Gui::Test::RecordingSimulationDialogs dialogs;
    const GuiServices                     before = services;
    services.setSimulationExecutor(&executor);
    services.setPlotDefaults(&plotDefaults);
    services.setSimulationDialogs(&dialogs);
    EXPECT_EQ(services.simulationExecutor(), &executor);
    EXPECT_EQ(services.plotDefaults(), &plotDefaults);
    EXPECT_EQ(services.simulationDialogs(), &dialogs);
    // A copy made before keeps what it referred to.
    EXPECT_EQ(before.simulationExecutor(), nullptr);
}

TEST_F(GuiServicesTest, WithOtherPromptsKeepsEverythingElse)
{
    Gui::Test::TestGuiServices     test;
    Gui::Test::ScriptedUserPrompts other;
    const GuiServices              services = test.services().withPrompts(other);
    EXPECT_EQ(&services.prompts(), &other);
    EXPECT_EQ(&test.services().prompts(), &test.prompts());
    EXPECT_EQ(&services.preferences(), &test.preferences());
    EXPECT_EQ(&services.motors(), &test.motors());
    EXPECT_EQ(&services.presets(), &test.presets());
    EXPECT_EQ(&services.loadingContext(), &test.services().loadingContext());
    EXPECT_EQ(&services.startupData(), &test.startupData());
    EXPECT_EQ(services.simulationExecutor(), &test.executor());
    EXPECT_EQ(services.plotDefaults(), &test.plotDefaults());
    EXPECT_EQ(services.simulationDialogs(), &test.simulationDialogs());
}

}  // namespace

}  // namespace QtRocket::Gui
