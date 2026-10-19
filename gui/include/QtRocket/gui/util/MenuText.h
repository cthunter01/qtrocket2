#pragma once

#include <QChar>
#include <QString>

/// The texts of menus and menu items. Qt reads an '&' in the text of a QAction or a QMenu as the
/// mark of the mnemonic (the letter that, underlined, chooses the item from the keyboard), where
/// Swing keeps the text as it is and takes the mnemonic apart (AbstractButton.setMnemonic()). So a
/// text that OpenRocket shows as it is (a component name in "Undo (Modify A&B)") is escaped, and
/// OpenRocket's mnemonic is put into the text: the translated text stays as messages.properties
/// has it, and the mark goes in front of the letter Swing underlines.
namespace QtRocket::Gui::MenuText
{

/// @p text with every '&' doubled, so that Qt shows it as it is and finds no mnemonic in it.
[[nodiscard]] QString escaped(const QString& text);

/// @p text escaped, with the mnemonic mark in front of the first occurrence of @p mnemonic,
/// upper or lower case (SwingUtilities.findDisplayedMnemonicIndex(): Swing underlines the first
/// occurrence of either case). When the text holds neither, it is only escaped: Swing would still
/// answer the key without showing it, which Qt cannot do.
[[nodiscard]] QString withMnemonic(const QString& text, QChar mnemonic);

}  // namespace QtRocket::Gui::MenuText
