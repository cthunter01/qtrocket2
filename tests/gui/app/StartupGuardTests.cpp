#include "QtRocket/gui/app/StartupGuard.h"

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/BugError.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"

// The start of the application as main() runs it (gui/main.cpp): the start is a function that the
// test gives.

namespace QtRocket::Gui
{

namespace
{

using StartupGuardTest = Test::GuiTest;
using PromptKind       = Gui::Test::ScriptedUserPrompts::Kind;
using Answer           = UserPrompts::InternalErrorAnswer;

constexpr int kStartedExitCode = 7;

int startAndEnd()
{
    return kStartedExitCode;
}

/// A start whose services break (a BugError while the ApplicationContext is made).
int breakingStart()
{
    bug("the services broke");
}

/// A start that reports an error condition (a missing resource) and goes on.
int reportingStart()
{
    Guard::reportErrorCondition(QStringLiteral("Image file :/x.svg not found, ignoring."));
    return kStartedExitCode;
}

TEST_F(StartupGuardTest, ReturnsWhatTheStartReturns)
{
    Gui::Test::ScriptedUserPrompts prompts;
    EXPECT_EQ(StartupGuard::run(prompts, &startAndEnd), kStartedExitCode);
    EXPECT_TRUE(prompts.prompts().empty());
}

TEST_F(StartupGuardTest, AStartThatThrowsIsShownInTheInternalErrorDialogAndFails)
{
    // SwingStartup registers SwingExceptionHandler before the services are made.
    Gui::Test::ScriptedUserPrompts prompts;
    prompts.answerInternalError(Answer::CONTINUE);
    EXPECT_EQ(StartupGuard::run(prompts, &breakingStart), StartupGuard::kStartupFailureExitCode);
    ASSERT_EQ(prompts.prompts().size(), 1U);
    const Gui::Test::ScriptedUserPrompts::Prompt& error = prompts.prompts().front();
    EXPECT_EQ(error.kind, PromptKind::INTERNAL_ERROR);
    ASSERT_EQ(error.texts.size(), 3);
    EXPECT_EQ(error.texts.at(0), QStringLiteral("BUG"));
    EXPECT_TRUE(error.texts.at(1).contains(QStringLiteral("the services broke")));
    EXPECT_EQ(error.texts.at(2), QStringLiteral("Start-up"));
}

TEST_F(StartupGuardTest, AnErrorConditionWhileTheStartRunsIsShownAndTheStartGoesOn)
{
    Gui::Test::ScriptedUserPrompts prompts;
    prompts.answerInternalError(Answer::QUIT);
    EXPECT_EQ(StartupGuard::run(prompts, &reportingStart), kStartedExitCode);
    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts().front().kind, PromptKind::INTERNAL_ERROR);
    EXPECT_EQ(prompts.prompts().front().texts,
              (QStringList{QStringLiteral("ERROR_CONDITION"),
                           QStringLiteral("Image file :/x.svg not found, ignoring."), QString()}));
}

TEST_F(StartupGuardTest, PutsTheHookBeforeBack)
{
    const Gui::Test::RecordingGuardHook before;
    Gui::Test::ScriptedUserPrompts      prompts;
    prompts.answerInternalError(Answer::CONTINUE);
    EXPECT_EQ(StartupGuard::run(prompts, &breakingStart), StartupGuard::kStartupFailureExitCode);
    EXPECT_TRUE(before.failures().empty());

    Guard::reportErrorCondition(QStringLiteral("after the start"));
    ASSERT_EQ(before.failures().size(), 1U);
    EXPECT_EQ(before.failures().front().message, QStringLiteral("after the start"));
    EXPECT_EQ(prompts.prompts().size(), 1U);
}

}  // namespace

}  // namespace QtRocket::Gui
