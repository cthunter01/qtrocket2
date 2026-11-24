#include "TestGuiServices.h"

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "ManualSimulationExecutor.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"

// The services of the GUI's tests: what they turn off and what they give the simulations.

namespace QtRocket::Gui::Test
{

namespace
{

using TestGuiServicesTest = GuiTest;

TEST_F(TestGuiServicesTest, TurnOffTheAutoOpenedPartsLibraryAndTheBackgroundEstimate)
{
    const TestGuiServices services;
    // No window of a test asks for a preset or runs a simulation on its own (GuiTestSupport.h).
    EXPECT_EQ(services.services().preferences().get(GuiPreferenceKeys::kAutoOpenPartsLibrary),
              "false");
    EXPECT_EQ(services.services().preferences().get(GuiPreferenceKeys::kBackgroundFlight), "false");
}

TEST_F(TestGuiServicesTest, GiveTheSimulationsTheirOwnServices)
{
    TestGuiServices services;
    EXPECT_EQ(services.services().simulationExecutor(), &services.executor());
    EXPECT_EQ(services.executor().mode(), ManualSimulationExecutor::Mode::IMMEDIATE);
    EXPECT_EQ(services.services().plotDefaults(), &services.plotDefaults());
    EXPECT_EQ(services.plotDefaults().lastPlotted(), nullptr);
    EXPECT_EQ(services.services().simulationDialogs(), &services.simulationDialogs());
    EXPECT_TRUE(services.simulationDialogs().requests().empty());
}

}  // namespace

}  // namespace QtRocket::Gui::Test
