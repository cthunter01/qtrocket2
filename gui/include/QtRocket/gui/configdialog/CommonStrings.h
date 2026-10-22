#pragma once

#include <QString>

/// The texts several configuration panels share (OpenRocket's CommonStrings,
/// swing gui/configdialog/CommonStrings.java): the dagger that marks a value each flight
/// configuration can override, and the note that explains it.
namespace QtRocket::Gui::CommonStrings
{

/// The dagger, U+2020 (Java's CommonStrings.dagger).
[[nodiscard]] QString dagger();

/// The dagger, a space and "This parameter can be overridden in each flight configuration."
/// (Java's CommonStrings.override_description; "Configuration.lbl.override").
[[nodiscard]] QString overrideDescription();

}  // namespace QtRocket::Gui::CommonStrings
