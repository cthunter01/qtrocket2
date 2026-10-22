#pragma once

#include <filesystem>
#include <optional>

#include <QChar>
#include <QString>

/// The rules of OpenRocket's save choosers about the name of the file (SaveFileChooser, which
/// every save chooser of OpenRocket's file operations extends, and FileUtils): which name a save
/// dialog is given to propose, and which names it refuses.
///
/// SaveFileChooser takes the name of a file as the text of its path relative to the chooser's
/// directory (getFilenameInput()), and that name must not hold one of the characters
/// / \ : * ? " < > | (FileUtils.getIllegalFilenameChar()). So a proposed file outside the
/// chooser's directory (another directory, a subdirectory) is not proposed at all
/// (setSelectedFile()), and a typed name with one of these characters is refused with "Illegal
/// filename" (approveSelection()), on every platform.
namespace QtRocket::Gui::SaveFileNames
{

/// FileUtils.getIllegalFilenameChar(@p name): the first of / \ : * ? " < > | (in this order) that
/// @p name holds; nullopt for none (and for an empty name).
[[nodiscard]] std::optional<QChar> illegalCharacter(const QString& name);

/// The directory a file chooser opens in for @p directory: @p directory, or the user's home
/// directory when it is empty (JFileChooser.setCurrentDirectory(null)).
[[nodiscard]] std::filesystem::path chooserDirectory(const std::filesystem::path& directory);

/// The text of java.io.File(@p path).getAbsolutePath(): @p path resolved against the current
/// directory, with the platform's separators, no separator repeated (but the two of a Windows
/// network path) and none at the end (but the root's).
[[nodiscard]] QString absolutePathText(const std::filesystem::path& path);

/// SaveFileChooser.getFilenameInput(@p file, @p directory): the name of @p file in a chooser in
/// @p directory. Text arithmetic, as Java's: the absolute path of @p file (absolutePathText()),
/// or both canonical paths when that does not start with the absolute path of @p directory,
/// without the first occurrence of the directory's path, trimmed (String.trim(): the characters
/// up to U+0020 at both ends), without the first separator. "x.ork" for a file of the directory;
/// "sub/x.ork" for one of a subdirectory; for one of another directory its whole path without
/// its first separator ("home/u/other/x.ork": the directory's path is not in it).
[[nodiscard]] QString filenameInput(const std::filesystem::path& file,
                                    const std::filesystem::path& directory);

/// SaveFileChooser.setSelectedFile(): whether a save chooser in @p directory proposes @p file,
/// that is whether filenameInput() holds no illegal character.
[[nodiscard]] bool isProposable(const std::filesystem::path& file,
                                const std::filesystem::path& directory);

}  // namespace QtRocket::Gui::SaveFileNames
