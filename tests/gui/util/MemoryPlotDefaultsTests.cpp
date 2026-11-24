#include "QtRocket/gui/util/MemoryPlotDefaults.h"

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/PlotDefaults.h"

// The keeper of the last plotted configuration (OpenRocket's SimulationPlotPanel.
// DEFAULT_CONFIGURATION). Its configuration type is the core's plot model (Milestone 4), which the
// plot tab's tests give it; here what holds without one.

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

}  // namespace

}  // namespace QtRocket::Gui
