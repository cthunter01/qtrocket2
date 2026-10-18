#include "QtRocket/gui/util/GuiServices.h"

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "ScriptedUserPrompts.h"

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
    const DocumentLoadingContext      context;
    Gui::Test::ScriptedUserPrompts    prompts;

    const GuiServices services(preferences, materials, motors, context, prompts);
    EXPECT_EQ(&services.preferences(), &preferences);
    EXPECT_EQ(&services.materials(), &materials);
    EXPECT_EQ(&services.motors(), &motors);
    EXPECT_EQ(&services.loadingContext(), &context);
    EXPECT_EQ(&services.prompts(), &prompts);

    // A copy refers to the same objects.
    const GuiServices copy = services;
    EXPECT_EQ(&copy.preferences(), &preferences);
    EXPECT_EQ(&copy.prompts(), &prompts);
}

}  // namespace

}  // namespace QtRocket::Gui
