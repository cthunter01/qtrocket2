#include "exit/FromScratchScenario.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "TestTempDir.h"
#include "exit/FromScratchPrompts.h"
#include "goldens/GoldenGuiReference.h"

namespace QtRocket::Gui
{

namespace
{

using Gui::Test::FromScratchScenario;
using QtRocket::Test::GuiScenario;
using QtRocket::Test::GuiScenarioStep;
using QtRocket::Test::GuiStepKind;
using QtRocket::Test::GuiStepRecord;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;

/// The fixture: the configuration dialog's state forgotten before and after, the application's
/// component clipboard left empty.
class FromScratchScenarioTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
    }

    void TearDown() override
    {
        OpenRocketClipboard::instance().clear();
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }
};

TEST_F(FromScratchScenarioTest, AMenuTextIsShownWithoutItsMnemonicMarks)
{
    EXPECT_EQ(FromScratchScenario::shownMenuText(QStringLiteral("&Undo (Add Nose Cone)")),
              QStringLiteral("Undo (Add Nose Cone)"));
    EXPECT_EQ(FromScratchScenario::shownMenuText(QStringLiteral("&Redo (Modify A&&B)")),
              QStringLiteral("Redo (Modify A&B)"));
    EXPECT_EQ(FromScratchScenario::shownMenuText(QStringLiteral("Undo")), QStringLiteral("Undo"));
}

TEST_F(FromScratchScenarioTest, AStepIsNamedWithWhatItNames)
{
    GuiScenarioStep type;
    type.index = 12;
    type.kind  = GuiStepKind::TYPE;
    type.field = "OuterRadius";
    type.text  = "4.1";
    type.unit  = "cm";
    EXPECT_EQ(FromScratchScenario::stepText(type), "step 12 (type OuterRadius 4.1 cm)");

    GuiScenarioStep point;
    point.index  = 82;
    point.kind   = GuiStepKind::EDIT_POINT;
    point.row    = 3;
    point.column = "Y";
    point.text   = "3";
    point.unit   = "cm";
    EXPECT_EQ(FromScratchScenario::stepText(point), "step 82 (editPoint 3 row 3 Y cm)");

    GuiScenarioStep select;
    select.index = 1;
    select.kind  = GuiStepKind::SELECT;
    select.paths = {{{.name = "Sustainer"}, {.name = "Parachute", .occurrence = 2}}};
    EXPECT_EQ(FromScratchScenario::stepText(select), "step 1 (select Sustainer/Parachute#2)");

    GuiScenarioStep check;
    check.index   = 5;
    check.kind    = GuiStepKind::CHECK;
    check.field   = "MotorMount";
    check.checked = true;
    EXPECT_EQ(FromScratchScenario::stepText(check), "step 5 (check MotorMount checked)");
}

TEST_F(FromScratchScenarioTest, AStepTheGuiRefusesEndsTheRunWithItsName)
{
    // The first scenario checked the nose cone's Automatic base diameter while the nose cone was
    // alone in its stage. OpenRocket's NoseConeConfig disables that check box then
    // (updateCheckboxAutoBaseRadius(): no next component to take the diameter of), as QtRocket's
    // does, so no user can do that step in either GUI; the reference tool had set the automatic
    // state through the model's action, past the disabled box. The scenario now checks it once a
    // body tube follows the nose cone. The run ends at the step it cannot do, and names it.
    const Result<GuiScenario> scenario = QtRocket::Test::parseGuiScenario(R"({
      "schema": "gui-from-scratch-scenario", "schemaVersion": 1, "title": "refused",
      "steps": [
        {"step": "new"},
        {"step": "select", "paths": [["Sustainer"]]},
        {"step": "add", "component": "NoseCone"},
        {"step": "check", "field": "BaseRadiusAutomatic", "checked": true},
        {"step": "ok"}
      ]})");
    ASSERT_TRUE(scenario.has_value()) << scenario.error().toString();
    const QtRocket::Test::TempDir directory;
    FromScratchScenario           run(directory.path());
    std::vector<std::size_t>      observed;
    std::optional<std::string>    window;
    const std::string             failure = run.run(
        *scenario, [&observed, &window](const GuiScenarioStep& step, const GuiStepRecord& record) {
            observed.push_back(step.index);
            window = record.window;
        });
    EXPECT_EQ(failure,
              "step 3 (check BaseRadiusAutomatic checked): the check box BaseRadiusAutomatic is "
              "disabled");
    EXPECT_EQ(observed, (std::vector<std::size_t>{0, 1, 2}));
    EXPECT_EQ(window, std::optional<std::string>("NoseConeConfig"));
    EXPECT_TRUE(run.prompts().prompts().empty());
}

/// A file chooser of the scenario's: answers @p file, and records the titles it is asked with.
struct RecordingChooser
{
    std::vector<QString>* asked;
    std::filesystem::path file;

    std::optional<UserPrompts::SaveFileAnswer> operator()(
        const UserPrompts::SaveFileRequest& request) const
    {
        asked->push_back(request.title);
        return UserPrompts::SaveFileAnswer{
            .file = file, .directory = {}, .saveSimulationData = std::nullopt};
    }
};

/// The file of @p answer; empty for none.
[[nodiscard]] std::filesystem::path fileOf(const std::optional<UserPrompts::SaveFileAnswer>& answer)
{
    return answer.has_value() ? answer->file : std::filesystem::path();
}

TEST_F(FromScratchScenarioTest, TheFileChooserIsTheScenariosWhileItIsSet)
{
    Gui::Test::ScriptedUserPrompts     scripted;
    Gui::Test::FromScratchPrompts      prompts(scripted);
    const UserPrompts::SaveFileRequest request{.title              = QStringLiteral("Save as"),
                                               .nameFilters        = {},
                                               .directory          = {},
                                               .fileName           = {},
                                               .saveSimulationData = std::nullopt,
                                               .refuseIllegalNames = false};
    std::vector<QString>               asked;
    prompts.setSaveFileChooser(
        RecordingChooser{.asked = &asked, .file = std::filesystem::path("chosen.ork")});
    EXPECT_EQ(fileOf(prompts.askSaveFile(nullptr, request)), std::filesystem::path("chosen.ork"));
    EXPECT_EQ(asked, std::vector<QString>{QStringLiteral("Save as")});
    EXPECT_TRUE(scripted.prompts().empty());

    // Without a chooser, the script answers.
    prompts.setSaveFileChooser({});
    scripted.answerSaveFile(std::nullopt);
    EXPECT_EQ(fileOf(prompts.askSaveFile(nullptr, request)), std::filesystem::path());
    ASSERT_EQ(scripted.prompts().size(), 1U);
    EXPECT_EQ(scripted.prompts().front().kind, PromptKind::SAVE_FILE);
    EXPECT_EQ(asked.size(), 1U);
}

}  // namespace

}  // namespace QtRocket::Gui
