#include "exit/SimulationsPrompts.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <QDialog>

#include "NextModalDialog.h"
#include "QtRocket/gui/dialogs/ModalDialog.h"
#include "QtRocket/gui/dialogs/simulation/CsvImportSettingsDialog.h"
#include "QtRocket/gui/dialogs/simulation/SimulationRunDialog.h"
#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket::Gui::Test
{

void SimulationsPrompts::queueAnswer(std::string name, DialogAnswer answer)
{
    m_answers.emplace_back(std::move(name), std::move(answer));
}

std::vector<std::string> SimulationsPrompts::takeUnusedAnswers()
{
    std::vector<std::string> names;
    names.reserve(m_answers.size());
    for (const auto& [name, answer] : m_answers)
    {
        names.push_back(name);
    }
    m_answers.clear();
    return names;
}

std::optional<SimulationsPrompts::DialogAnswer> SimulationsPrompts::nextAnswer()
{
    if (m_answers.empty())
    {
        ++m_unanswered;
        return std::nullopt;
    }
    DialogAnswer answer = std::move(m_answers.front().second);
    m_answers.pop_front();
    return answer;
}

UserPrompts::DiscardAnswer SimulationsPrompts::askDiscardChanges(QWidget*        parent,
                                                                 DiscardQuestion question)
{
    std::optional<DialogAnswer> answer = nextAnswer();
    if (!answer.has_value())
    {
        return delegate().askDiscardChanges(parent, question);
    }
    const NextModalDialog next(std::move(*answer));
    return widgets().askDiscardChanges(parent, question);
}

bool SimulationsPrompts::askYesNo(QWidget* parent, const Question& question)
{
    std::optional<DialogAnswer> answer = nextAnswer();
    if (!answer.has_value())
    {
        return delegate().askYesNo(parent, question);
    }
    const NextModalDialog next(std::move(*answer));
    return widgets().askYesNo(parent, question);
}

std::optional<UserPrompts::OpenFileAnswer> SimulationsPrompts::askOpenFiles(
    QWidget* parent, const OpenFileRequest& request)
{
    if (m_openFiles)
    {
        m_openFiles(request);
    }
    return delegate().askOpenFiles(parent, request);
}

UserPrompts::DeleteSimulationsAnswer SimulationsPrompts::askDeleteSimulations(QWidget*    parent,
                                                                              std::size_t count)
{
    std::optional<DialogAnswer> answer = nextAnswer();
    if (!answer.has_value())
    {
        return delegate().askDeleteSimulations(parent, count);
    }
    const NextModalDialog next(std::move(*answer));
    return widgets().askDeleteSimulations(parent, count);
}

UserPrompts::RunSimulationsAnswer SimulationsPrompts::runSimulations(
    QWidget* parent, const RunSimulationsRequest& request)
{
    std::optional<DialogAnswer> answer = nextAnswer();
    if (!answer.has_value())
    {
        return delegate().runSimulations(parent, request);
    }
    const NextModalDialog next(std::move(*answer));
    // WidgetUserPrompts::runSimulations(), with the failures asked of these prompts.
    return SimulationRunDialog::run(parent, *this, request);
}

std::optional<UserPrompts::WindCsvImportSettings> SimulationsPrompts::askWindCsvImportSettings(
    QWidget* parent, const WindCsvImportSettings& initial)
{
    std::optional<DialogAnswer> answer = nextAnswer();
    if (!answer.has_value())
    {
        return delegate().askWindCsvImportSettings(parent, initial);
    }
    const NextModalDialog next(std::move(*answer));
    // WidgetUserPrompts::askWindCsvImportSettings(), with the dialog's messages asked of these
    // prompts.
    ModalDialog<CsvImportSettingsDialog> dialog(parent, initial, *this);
    if (dialog.exec() != QDialog::Accepted)
    {
        return std::nullopt;
    }
    const CsvImportSettingsDialog* const answered = dialog.get();
    if (answered == nullptr)
    {
        return std::nullopt;
    }
    return answered->settings();
}

}  // namespace QtRocket::Gui::Test
