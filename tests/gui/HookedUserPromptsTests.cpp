#include "HookedUserPrompts.h"

#include <cstddef>
#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "ScriptedUserPrompts.h"

// The prompts that run a test's hook before the next message they show, once; the questions go
// to their scripted prompts.

namespace QtRocket::Gui::Test
{

namespace
{

using HookedUserPromptsTest = GuiTest;
using Kind                  = ScriptedUserPrompts::Kind;

/// The test's hook: counts its calls and how many prompts were recorded at the first.
struct CountingHook
{
    const HookedUserPrompts* prompts;
    int*                     calls;
    std::size_t*             recorded;

    void operator()() const
    {
        ++*calls;
        *recorded = prompts->scripted().prompts().size();
    }
};

TEST_F(HookedUserPromptsTest, RunsTheHookOnceBeforeTheNextMessage)
{
    HookedUserPrompts prompts;
    int               hooked   = 0;
    std::size_t       recorded = 0;
    prompts.beforeNextMessage(
        CountingHook{.prompts = &prompts, .calls = &hooked, .recorded = &recorded});
    prompts.scripted().answerIgnition(std::nullopt);
    EXPECT_FALSE(
        prompts.askIgnition(nullptr, {.configurationName = QStringLiteral("A"), .initial = {}})
            .has_value());
    EXPECT_EQ(hooked, 0);
    const std::vector<UserPrompts::DatabaseProblem> problems{
        {.database = UserPrompts::DatabaseKind::COMPONENT_PRESETS,
         .file     = "a.orc",
         .message  = QStringLiteral("broken")}};
    prompts.showDatabaseProblems(nullptr, problems);
    EXPECT_EQ(hooked, 1);
    // Before the message: only the question before it was recorded.
    EXPECT_EQ(recorded, 1U);
    prompts.showMessage(nullptr, {.severity = UserPrompts::Severity::INFORMATION,
                                  .title    = QStringLiteral("Save as default"),
                                  .lines    = {QStringLiteral("only new configurations")}});
    EXPECT_EQ(hooked, 1);
    ASSERT_EQ(prompts.scripted().prompts().size(), 3U);
    EXPECT_EQ(prompts.scripted().prompts().at(1).kind, Kind::DATABASE_PROBLEMS);
    EXPECT_EQ(prompts.scripted().prompts().at(2).kind, Kind::MESSAGE);
}

}  // namespace

}  // namespace QtRocket::Gui::Test
