#include "ForwardingUserPrompts.h"

#include <memory>
#include <optional>
#include <vector>

#include <QColor>
#include <QString>
#include <QStringList>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
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

/// The kinds of what @p prompts recorded, in order.
[[nodiscard]] std::vector<Kind> kindsOf(const ScriptedUserPrompts& prompts)
{
    std::vector<Kind> kinds;
    kinds.reserve(prompts.prompts().size());
    for (const ScriptedUserPrompts::Prompt& prompt : prompts.prompts())
    {
        kinds.push_back(prompt.kind);
    }
    return kinds;
}

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

TEST_F(ForwardingUserPromptsTest, ForwardsTheMilestoneThreeQuestionsAndMessages)
{
    const TestGuiServices services;
    ScriptedUserPrompts   scripted;
    ForwardingUserPrompts forwarding(scripted);
    scripted.answerIgnition(std::nullopt);
    scripted.answerDeployment(std::nullopt);
    scripted.answerSeparation(std::nullopt);
    scripted.answerRenameConfiguration(
        {.action = UserPrompts::RenameAction::RESET, .name = QString()});
    scripted.answerMotor(std::nullopt);
    scripted.answerPreset({.outcome = UserPrompts::PresetOutcome::NONE, .preset = nullptr});

    EXPECT_FALSE(
        forwarding.askIgnition(nullptr, {.configurationName = QStringLiteral("A"), .initial = {}})
            .has_value());
    EXPECT_FALSE(forwarding
                     .askDeployment(nullptr, {.configurationName = QStringLiteral("B"),
                                              .initial           = {},
                                              .bottomStage       = false})
                     .has_value());
    EXPECT_FALSE(
        forwarding.askSeparation(nullptr, {.configurationName = QStringLiteral("C"), .initial = {}})
            .has_value());
    EXPECT_EQ(
        forwarding
            .askRenameConfiguration(nullptr, {.name = QStringLiteral("D"), .saveAsDefault = {}})
            .action,
        UserPrompts::RenameAction::RESET);
    const InnerTube mount;
    EXPECT_FALSE(forwarding
                     .chooseMotor(nullptr, services.services(), mount,
                                  FlightConfigurationId::defaultValueId())
                     .has_value());
    EXPECT_EQ(forwarding
                  .choosePreset(nullptr, services.services(),
                                {.componentKind   = ComponentKind::PARACHUTE,
                                 .foreDiameter    = std::nullopt,
                                 .aftDiameter     = std::nullopt,
                                 .flippedNoseCone = false})
                  .outcome,
              UserPrompts::PresetOutcome::NONE);
    const std::vector<UserPrompts::DatabaseProblem> problems{
        {.database = UserPrompts::DatabaseKind::THRUST_CURVES,
         .file     = "a.eng",
         .message  = QStringLiteral("broken")}};
    forwarding.showDatabaseProblems(nullptr, problems);

    EXPECT_EQ(kindsOf(scripted),
              (std::vector<Kind>{Kind::IGNITION, Kind::DEPLOYMENT, Kind::SEPARATION,
                                 Kind::RENAME_CONFIGURATION, Kind::MOTOR, Kind::PRESET,
                                 Kind::DATABASE_PROBLEMS}));
}

TEST_F(ForwardingUserPromptsTest, ForwardsTheMilestoneFourQuestions)
{
    ScriptedUserPrompts   scripted;
    ForwardingUserPrompts forwarding(scripted);
    scripted.answerDeleteSimulations({.confirmed = true, .doNotAskAgain = false});
    scripted.answerRunSimulations({.cancelled = false, .allSuccessful = true});
    scripted.answerWindCsvImportSettings(std::nullopt);
    scripted.answerColor(QColor(10, 20, 30));

    EXPECT_TRUE(forwarding.askDeleteSimulations(nullptr, 2).confirmed);
    EXPECT_TRUE(forwarding.runSimulations(nullptr, {}).allSuccessful);
    EXPECT_FALSE(forwarding.askWindCsvImportSettings(nullptr, {}).has_value());
    EXPECT_EQ(forwarding.askColor(nullptr, QColor(), QStringLiteral("Select color")),
              std::optional<QColor>(QColor(10, 20, 30)));

    EXPECT_EQ(kindsOf(scripted), (std::vector<Kind>{Kind::DELETE_SIMULATIONS, Kind::RUN_SIMULATIONS,
                                                    Kind::WIND_CSV_IMPORT_SETTINGS, Kind::COLOR}));
}

TEST_F(ForwardingUserPromptsTest, TheServicesWithOtherPromptsKeepTheSimulationsServices)
{
    // withPrompts() (a double's services) keeps the executor, the plot defaults and the
    // simulation dialogs of the services it copies.
    const TestGuiServices services;
    ScriptedUserPrompts   scripted;
    ForwardingUserPrompts forwarding(scripted);
    const GuiServices     own = withPrompts(services.services(), forwarding);
    EXPECT_EQ(&own.prompts(), &forwarding);
    EXPECT_EQ(own.simulationExecutor(), services.services().simulationExecutor());
    EXPECT_EQ(own.plotDefaults(), services.services().plotDefaults());
    EXPECT_EQ(own.simulationDialogs(), services.services().simulationDialogs());
}

}  // namespace

}  // namespace QtRocket::Gui::Test
