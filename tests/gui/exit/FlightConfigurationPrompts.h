#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <utility>

#include "QtRocket/gui/util/UserPrompts.h"
#include "exit/FromScratchPrompts.h"

class QWidget;

namespace QtRocket
{
class FlightConfigurationId;
class MotorMount;
}  // namespace QtRocket

namespace QtRocket::Gui
{
class GuiServices;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

/// The questions of the application in the scenarios of the Motors & Configuration tab
/// (FlightConfigurationScenario): those of FromScratchPrompts (the script and its record, the two
/// windows of Save as, the parts library closed), and the questions of the tab, which QtRocket's
/// own dialogs ask (WidgetUserPrompts: the motor chooser, the ignition, deployment, separation and
/// rename dialogs) while the scenario has an answer for them (setDialogAnswer()): the answer is
/// given the dialog once it is open, inside its modal loop (NextModalDialog), as OpenRocket's tool
/// answers OpenRocket's dialogs (tools/openrocket-gui-reference, Questions.java). Without an
/// answer the question goes to the script, which fails the test (no answer scripted); such
/// questions are counted (unansweredQuestions()). The messages go to the script (the record), each
/// first told the scenario (setMessageObserver(): the message of "Save as default").
class FlightConfigurationPrompts final : public FromScratchPrompts
{
public:
    /// What answers the open dialog of a question.
    using DialogAnswer = std::function<void(QWidget& dialog)>;
    /// What is told a message.
    using MessageObserver = std::function<void(const Message& message)>;

    /// The questions of @p scripted, which must outlive this object, but for those above.
    explicit FlightConfigurationPrompts(UserPrompts& scripted) noexcept
      : FromScratchPrompts(scripted)
    {
    }

    /// Answers the dialogs of the next questions of the tab with @p answer; an empty one gives
    /// the questions back to the script.
    void setDialogAnswer(DialogAnswer answer) { m_answer = std::move(answer); }
    /// Tells @p observer every message from now on; an empty one tells none.
    void setMessageObserver(MessageObserver observer) { m_messages = std::move(observer); }

    /// The questions of the tab asked without an answer so far.
    [[nodiscard]] std::size_t unansweredQuestions() const noexcept { return m_unanswered; }

    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(
        QWidget* parent, const IgnitionRequest& request) override;
    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override;
    [[nodiscard]] std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) override;
    [[nodiscard]] RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) override;
    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override;
    void showMessage(QWidget* parent, const Message& message) override;

private:
    /// Whether the scenario answers the question: counts it otherwise.
    [[nodiscard]] bool answered();

    DialogAnswer    m_answer;
    MessageObserver m_messages;
    std::size_t     m_unanswered{0};
};

}  // namespace QtRocket::Gui::Test
