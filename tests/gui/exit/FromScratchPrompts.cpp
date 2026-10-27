#include "exit/FromScratchPrompts.h"

#include <optional>

#include "QtRocket/gui/util/UserPrompts.h"

class QWidget;

namespace QtRocket
{
class OpenRocketDocument;
}

namespace QtRocket::Gui
{
class GuiServices;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test
{

std::optional<UserPrompts::SaveFileAnswer> FromScratchPrompts::askSaveFile(
    QWidget* parent, const SaveFileRequest& request)
{
    if (m_chooser)
    {
        return m_chooser(request);
    }
    return delegate().askSaveFile(parent, request);
}

void FromScratchPrompts::editDesignInformation(QWidget* parent, const GuiServices& services,
                                               OpenRocketDocument& document)
{
    m_widgets.editDesignInformation(parent, services, document);
}

UserPrompts::PresetAnswer FromScratchPrompts::choosePreset(QWidget* /*parent*/,
                                                           const GuiServices& /*services*/,
                                                           const PresetRequest& request)
{
    m_presetRequests.push_back(request);
    return {.outcome = PresetOutcome::CLOSED, .preset = nullptr};
}

}  // namespace QtRocket::Gui::Test
