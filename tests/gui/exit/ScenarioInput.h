#pragma once

// What the drivers of the GUI's scenarios (FromScratchScenario, FlightConfigurationScenario) do
// with the widgets as a user does: deliver the events a click or a key leaves, make a window the
// active one, give a field the focus, choose a unit in a unit selector, type a number, trigger an
// action. Each function that can fail says what went wrong in a text ("" when nothing did), which
// the scenario puts into the failure of its step.

#include <string>
#include <string_view>

class QAbstractSpinBox;
class QAction;
class QObject;
class QWidget;

namespace QtRocket::Gui
{
class UnitSelector;
}  // namespace QtRocket::Gui

namespace QtRocket::Gui::Test::ScenarioInput
{

/// Delivers the posted events until the windows, the dialogs and their layouts are done.
void settle();

/// settle(), then deletes what was given to deleteLater(), which processEvents() leaves: a
/// window or a dialog that was closed.
void settleAndDelete();

/// The class name of @p object without its namespace ("NoseConeConfig").
[[nodiscard]] std::string shortClassName(const QObject& object);

/// Makes the window of @p widget the active one, which the keyboard focus needs (offscreen as
/// well); whether it is.
[[nodiscard]] bool activateWindowOf(const QWidget& widget);

/// Gives @p widget, the field named @p field in the messages, the keyboard focus, as a click
/// does; what went wrong, or "".
[[nodiscard]] std::string focusOn(QWidget& widget, std::string_view field);

/// Chooses @p unit in @p selector, the unit selector of the field @p field, through its menu when
/// it shows another unit; what went wrong, or "".
[[nodiscard]] std::string chooseUnit(UnitSelector& selector, std::string_view field,
                                     std::string_view unit);

/// Types @p text over the number field @p spinBox (named @p field in the messages) and commits it
/// with Enter; what went wrong, or "".
[[nodiscard]] std::string typeNumber(QAbstractSpinBox& spinBox, std::string_view field,
                                     std::string_view text);

/// Triggers @p action, the item named @p name, as its menu item or its keys do; what went wrong
/// (there is none, or it is disabled), or "".
[[nodiscard]] std::string triggerAction(QAction* action, std::string_view name);

}  // namespace QtRocket::Gui::Test::ScenarioInput
