#pragma once

#include <functional>
#include <optional>
#include <utility>

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
/// that the scenario works in itself, as a user does:
/// - the "Save Design Info" dialog is the application's own (WidgetUserPrompts:
///   SaveDesignInfoDialog::edit()), into whose fields the scenario types;
/// - the file chooser goes to the scenario's chooser while one is set (setSaveFileChooser()),
///   which records the state the GUI is in while the chooser is open and gives the file the
///   scenario names; without one it is the script's question.
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

private:
    WidgetUserPrompts m_widgets;
    SaveFileChooser   m_chooser;
};

}  // namespace QtRocket::Gui::Test
