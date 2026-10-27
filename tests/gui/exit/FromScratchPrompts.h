#pragma once

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include "ForwardingUserPrompts.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
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

/// The questions of the application in the from-scratch scenario (FromScratchScenario): those of
/// the test's ScriptedUserPrompts (its script and its record), but for the two windows of Save as
/// that the scenario works in itself, as a user does, and the parts library:
/// - the "Save Design Info" dialog is the application's own (WidgetUserPrompts:
///   SaveDesignInfoDialog::edit()), into whose fields the scenario types;
/// - the file chooser goes to the scenario's chooser while one is set (setSaveFileChooser()),
///   which records the state the GUI is in while the chooser is open and gives the file the
///   scenario names; without one it is the script's question;
/// - the parts library, which the application opens for each new component of a kind that takes
///   presets (its default preferences: AutoOpenPartsLibrary), is closed without a choice
///   (PresetOutcome::CLOSED), as the scenario's golden assumes (OpenRocket's tool,
///   tools/openrocket-gui-reference, takes it as closed: QtRocket had no parts library when the
///   scenario was recorded); its requests are kept (presetRequests()).
class FromScratchPrompts final : public ForwardingUserPrompts
{
public:
    /// What answers the file chooser of a save (UserPrompts::askSaveFile()).
    using SaveFileChooser = std::function<std::optional<SaveFileAnswer>(const SaveFileRequest&)>;

    /// The questions of @p scripted, which must outlive this object, but for the two of Save as.
    explicit FromScratchPrompts(UserPrompts& scripted) noexcept
      : ForwardingUserPrompts(scripted) { }

    /// Answers the next file choosers of a save with @p chooser; an empty one gives them back to
    /// the script.
    void setSaveFileChooser(SaveFileChooser chooser) { m_chooser = std::move(chooser); }

    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(
        QWidget* parent, const SaveFileRequest& request) override;
    /// The application's "Save Design Info" dialog (WidgetUserPrompts).
    void editDesignInformation(QWidget* parent, const GuiServices& services,
                               OpenRocketDocument& document) override;
    /// Closed without a choice; the request is kept.
    [[nodiscard]] PresetAnswer choosePreset(QWidget* parent, const GuiServices& services,
                                            const PresetRequest& request) override;

    /// The requests of choosePreset() so far, in order.
    [[nodiscard]] const std::vector<PresetRequest>& presetRequests() const noexcept
    {
        return m_presetRequests;
    }

private:
    WidgetUserPrompts          m_widgets;
    SaveFileChooser            m_chooser;
    std::vector<PresetRequest> m_presetRequests;
};

}  // namespace QtRocket::Gui::Test
