#include "ForwardingUserPrompts.h"

#include <memory>

#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "ScriptedUserPrompts.h"
#include "TestGuiServices.h"

// The base of the tests' prompt doubles that act on one question of their own: every other
// question and message reaches the prompts it forwards to, as it was asked.

namespace QtRocket::Gui::Test
{

namespace
{

using ForwardingUserPromptsTest = GuiTest;
using Kind                      = ScriptedUserPrompts::Kind;

TEST_F(ForwardingUserPromptsTest, ForwardsTheErrorsAndWarningsAsTheyAre)
{
    // Not as warnings, which UserPrompts' own showErrorsAndWarnings() would make them.
    ScriptedUserPrompts   scripted;
    ForwardingUserPrompts forwarding(scripted);
    forwarding.showErrorsAndWarnings(
        nullptr, {.title    = QStringLiteral("Errors/Warnings while saving file"),
                  .lines    = {QStringLiteral("The following problems were encountered")},
                  .errors   = {QStringLiteral("An error")},
                  .warnings = {QStringLiteral("A warning")}});
    ASSERT_EQ(scripted.prompts().size(), 1U);
    const ScriptedUserPrompts::Prompt& shown = scripted.prompts().front();
    EXPECT_EQ(shown.kind, Kind::ERRORS_AND_WARNINGS);
    EXPECT_EQ(shown.title, QStringLiteral("Errors/Warnings while saving file"));
    EXPECT_EQ(shown.texts, (QStringList{QStringLiteral("The following problems were encountered"),
                                        QStringLiteral("An error"), QStringLiteral("A warning")}));
}

TEST_F(ForwardingUserPromptsTest, ForwardsTheDesignInformationWithItsServices)
{
    const TestGuiServices                     services;
    ScriptedUserPrompts                       scripted;
    ForwardingUserPrompts                     forwarding(scripted);
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    scripted.answerDesignInformation(
        [](OpenRocketDocument& edited) { edited.getRocket().setDesigner("Sampo"); });
    forwarding.editDesignInformation(nullptr, services.services(), *document);
    ASSERT_EQ(scripted.prompts().size(), 1U);
    EXPECT_EQ(scripted.prompts().front().kind, Kind::DESIGN_INFORMATION);
    EXPECT_EQ(document->getRocket().getDesigner(), "Sampo");
}

}  // namespace

}  // namespace QtRocket::Gui::Test
