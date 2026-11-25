#include "QtRocket/gui/util/MemoryPlotDefaults.h"

#include <memory>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/PlotDefaults.h"
#include "QtRocket/plot/SimulationPlotConfiguration.h"

// The keeper of the last plotted configuration (OpenRocket's SimulationPlotPanel.
// DEFAULT_CONFIGURATION). The plot tab's own use of it (doPlot() keeps the configuration without
// its looks, the next tab starts from it) is tests/gui/dialogs/plot/SimulationPlotPanelTests.cpp's.

namespace QtRocket::Gui
{

namespace
{

using MemoryPlotDefaultsTest = Test::GuiTest;

TEST_F(MemoryPlotDefaultsTest, HasNoConfigurationBeforeTheFirstPlot)
{
    MemoryPlotDefaults defaults;
    // Before the first plot the plot tab starts from the first preset.
    EXPECT_EQ(defaults.lastPlotted(), nullptr);
    PlotDefaults& asInterface = defaults;
    asInterface.setLastPlotted(nullptr);
    EXPECT_EQ(asInterface.lastPlotted(), nullptr);
}

TEST_F(MemoryPlotDefaultsTest, KeepsTheConfigurationLastPlotted)
{
    MemoryPlotDefaults defaults;
    PlotDefaults&      asInterface = defaults;
    const auto         plotted     = std::make_shared<const SimulationPlotConfiguration>(
        SimulationPlotConfiguration::defaultConfigurations().at(4));
    asInterface.setLastPlotted(plotted);
    // The same object, shared (the tab copies it).
    EXPECT_EQ(asInterface.lastPlotted(), plotted);
    ASSERT_NE(defaults.lastPlotted(), nullptr);
    EXPECT_EQ(defaults.lastPlotted()->getName(), "Stability vs. time");
    EXPECT_EQ(defaults.lastPlotted()->getDataCount(), 3U);

    const auto next = std::make_shared<const SimulationPlotConfiguration>(
        SimulationPlotConfiguration::customConfiguration());
    asInterface.setLastPlotted(next);
    EXPECT_EQ(defaults.lastPlotted(), next);
    asInterface.setLastPlotted(nullptr);
    EXPECT_EQ(defaults.lastPlotted(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
