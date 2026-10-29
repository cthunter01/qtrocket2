#include "exit/FlightConfigurationPrompts.h"

#include <optional>

#include "NextModalDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"

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

bool FlightConfigurationPrompts::answered()
{
    if (m_answer)
    {
        return true;
    }
    ++m_unanswered;
    return false;
}

std::optional<UserPrompts::IgnitionChoice> FlightConfigurationPrompts::askIgnition(
    QWidget* parent, const IgnitionRequest& request)
{
    if (!answered())
    {
        return delegate().askIgnition(parent, request);
    }
    const NextModalDialog answer(m_answer);
    return widgets().askIgnition(parent, request);
}

std::optional<UserPrompts::DeploymentChoice> FlightConfigurationPrompts::askDeployment(
    QWidget* parent, const DeploymentRequest& request)
{
    if (!answered())
    {
        return delegate().askDeployment(parent, request);
    }
    const NextModalDialog answer(m_answer);
    return widgets().askDeployment(parent, request);
}

std::optional<UserPrompts::SeparationChoice> FlightConfigurationPrompts::askSeparation(
    QWidget* parent, const SeparationRequest& request)
{
    if (!answered())
    {
        return delegate().askSeparation(parent, request);
    }
    const NextModalDialog answer(m_answer);
    return widgets().askSeparation(parent, request);
}

UserPrompts::RenameConfigurationAnswer FlightConfigurationPrompts::askRenameConfiguration(
    QWidget* parent, const RenameConfigurationRequest& request)
{
    if (!answered())
    {
        return delegate().askRenameConfiguration(parent, request);
    }
    const NextModalDialog answer(m_answer);
    return widgets().askRenameConfiguration(parent, request);
}

std::optional<UserPrompts::MotorChoice> FlightConfigurationPrompts::chooseMotor(
    QWidget* parent, const GuiServices& services, const MotorMount& mount,
    const FlightConfigurationId& configurationId)
{
    if (!answered())
    {
        return delegate().chooseMotor(parent, services, mount, configurationId);
    }
    const NextModalDialog answer(m_answer);
    return widgets().chooseMotor(parent, services, mount, configurationId);
}

void FlightConfigurationPrompts::showMessage(QWidget* parent, const Message& message)
{
    if (m_messages)
    {
        m_messages(message);
    }
    delegate().showMessage(parent, message);
}

}  // namespace QtRocket::Gui::Test
