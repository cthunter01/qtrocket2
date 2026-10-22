#include "QtRocket/gui/main/SaveFileNames.h"

#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>

#include <QChar>
#include <QDir>
#include <QString>
#include <QtTypes>

#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui::SaveFileNames
{

namespace
{

/// FileUtils.ILLEGAL_CHARS, in their order.
constexpr std::u16string_view kIllegalCharacters = u"/\\:*?\"<>|";

/// The last character String.trim() removes.
constexpr char16_t kLastTrimmed = u' ';

/// @p text without the characters up to U+0020 at its ends (Java's String.trim(); QString's
/// trimmed() removes Unicode's spaces instead).
[[nodiscard]] QString javaTrim(const QString& text)
{
    qsizetype first = 0;
    qsizetype end   = text.size();
    while (first < end && text.at(first).unicode() <= kLastTrimmed)
    {
        ++first;
    }
    while (end > first && text.at(end - 1).unicode() <= kLastTrimmed)
    {
        --end;
    }
    return text.mid(first, end - first);
}

/// @p text (an absolute path with the platform's separators) as java.io.File normalises it: no
/// repeated separator, but the two that start a Windows network path, and none at the end, but a
/// root's ("/", "C:\").
[[nodiscard]] QString javaNormalized(const QString& text)
{
    const QChar separator = QDir::separator();
    const bool  windows   = separator == QChar(u'\\');
    QString     normalized;
    normalized.reserve(text.size());
    for (qsizetype index = 0; index < text.size(); ++index)
    {
        const QChar character = text.at(index);
        const bool  repeated  = character == separator && !normalized.isEmpty() &&
                                normalized.back() == separator && (!windows || index != 1);
        if (!repeated)
        {
            normalized.append(character);
        }
    }
    const bool driveRoot = windows && normalized.size() == 3 && normalized.at(1) == QChar(u':');
    if (normalized.size() > 1 && normalized.endsWith(separator) && !driveRoot)
    {
        normalized.chop(1);
    }
    return normalized;
}

/// The absolute path text of the canonical form of @p path (File.getCanonicalPath(), which
/// resolves what exists of it), or nullopt when it cannot be worked out (Java's IOException).
[[nodiscard]] std::optional<QString> canonicalPathText(const std::filesystem::path& path)
{
    std::error_code             error;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
    if (error || canonical.empty())
    {
        return std::nullopt;
    }
    return absolutePathText(canonical);
}

}  // namespace

std::optional<QChar> illegalCharacter(const QString& name)
{
    for (const char16_t character : kIllegalCharacters)
    {
        if (name.contains(QChar(character)))
        {
            return QChar(character);
        }
    }
    return std::nullopt;
}

std::filesystem::path chooserDirectory(const std::filesystem::path& directory)
{
    return directory.empty() ? QtStrings::toPath(QDir::homePath()) : directory;
}

QString absolutePathText(const std::filesystem::path& path)
{
    std::filesystem::path absolute = path;
    if (!absolute.is_absolute())
    {
        std::error_code             error;
        const std::filesystem::path current = std::filesystem::current_path(error);
        if (!error)
        {
            absolute = current / path;
        }
    }
    return javaNormalized(QDir::toNativeSeparators(QtStrings::fromPath(absolute)));
}

QString filenameInput(const std::filesystem::path& file, const std::filesystem::path& directory)
{
    QString full = absolutePathText(file);
    QString base = absolutePathText(directory);
    if (!full.startsWith(base))
    {
        // "Windows may normalize the chooser directory (case or 8.3 short names), so retry with
        // canonical paths before treating the input as malformed": the file's first, then the
        // directory's, each kept when it could be worked out.
        if (const std::optional<QString> canonicalFile = canonicalPathText(file);
            canonicalFile.has_value())
        {
            full = *canonicalFile;
            if (const std::optional<QString> canonicalDirectory = canonicalPathText(directory);
                canonicalDirectory.has_value())
            {
                base = *canonicalDirectory;
            }
        }
    }
    // String.replaceFirst(Pattern.quote(cwdPath), ""), trim(), then the first File.separator.
    if (const qsizetype at = full.indexOf(base); at >= 0)
    {
        full.remove(at, base.size());
    }
    QString relative = javaTrim(full);
    if (const qsizetype at = relative.indexOf(QDir::separator()); at >= 0)
    {
        relative.remove(at, 1);
    }
    return relative;
}

bool isProposable(const std::filesystem::path& file, const std::filesystem::path& directory)
{
    return !illegalCharacter(filenameInput(file, directory)).has_value();
}

}  // namespace QtRocket::Gui::SaveFileNames
