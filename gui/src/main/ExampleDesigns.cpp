#include "QtRocket/gui/main/ExampleDesigns.h"

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <QChar>
#include <QString>
#include <Qt>
#include <QtTypes>

#include "QtRocket/gui/util/QtStrings.h"

namespace QtRocket::Gui
{

namespace
{

/// The length of ".ork".
constexpr qsizetype kExtensionLength = 4;

/// Whether @p character ends a line for java.util.regex's '.' (Pattern's default mode).
[[nodiscard]] bool isLineTerminator(QChar character)
{
    const char16_t unit = character.unicode();
    return unit == u'\n' || unit == u'\r' || unit == u'\u0085' || unit == u'\u2028' ||
           unit == u'\u2029';
}

/// Whether @p character is @p lower or its ASCII upper case (a class such as "[oO]").
[[nodiscard]] bool isAsciiLetter(QChar character, char16_t lower)
{
    const char16_t unit = character.unicode();
    return unit == lower || unit == static_cast<char16_t>(lower - u'a' + u'A');
}

/// The designs @p directory holds, in the order of the listing.
[[nodiscard]] std::vector<ExampleDesigns::Design> listDesigns(
    const std::filesystem::path& directory)
{
    std::vector<ExampleDesigns::Design> designs;
    std::error_code                     error;
    std::filesystem::directory_iterator entries(directory, error);
    if (error)
    {
        return designs;
    }
    for (; entries != std::filesystem::directory_iterator(); entries.increment(error))
    {
        if (error)
        {
            break;
        }
        std::error_code typeError;
        if (!entries->is_regular_file(typeError))
        {
            continue;
        }
        const QString fileName = QtStrings::fromPath(entries->path().filename());
        if (ExampleDesigns::isDesignFileName(fileName))
        {
            designs.push_back(
                {.name = ExampleDesigns::designName(fileName), .file = entries->path()});
        }
    }
    return designs;
}

/// Java's String.compareTo() of the names (the UTF-16 code units), then the file names.
[[nodiscard]] bool isBefore(const ExampleDesigns::Design& first,
                            const ExampleDesigns::Design& second)
{
    const int names = first.name.compare(second.name, Qt::CaseSensitive);
    if (names != 0)
    {
        return names < 0;
    }
    return QtStrings::fromPath(first.file.filename())
               .compare(QtStrings::fromPath(second.file.filename()), Qt::CaseSensitive) < 0;
}

}  // namespace

ExampleDesigns::ExampleDesigns(const std::filesystem::path& directory)
  : m_designs(listDesigns(directory))
{
    // getExampleDesigns(): Arrays.sort(designs).
    std::ranges::sort(m_designs, &isBefore);
}

std::vector<std::optional<ExampleDesigns::Design>> ExampleDesigns::menuEntries() const
{
    // ExampleDesignFileAction.updateMenu(): the items in the order of exampleFileOrder, then
    // the remaining (unordered) ones after a separator.
    std::vector<Design>                remaining = m_designs;
    std::vector<std::optional<Design>> entries;
    entries.reserve(remaining.size() + kMenuOrder.size() + 1);
    for (const std::string_view name : kMenuOrder)
    {
        if (name.empty())
        {
            entries.emplace_back(std::nullopt);
            continue;
        }
        // Java: item.getText().equals(s); decision U4: else ignoring case.
        const QString wanted = QtStrings::fromUtf8(name);
        auto          found  = std::ranges::find(remaining, wanted, &Design::name);
        if (found == remaining.end())
        {
            found = std::ranges::find_if(remaining, [&wanted](const Design& design) {
                return design.name.compare(wanted, Qt::CaseInsensitive) == 0;
            });
        }
        if (found != remaining.end())
        {
            entries.emplace_back(*found);
            remaining.erase(found);
        }
    }
    if (!remaining.empty())
    {
        entries.emplace_back(std::nullopt);
        for (Design& design : remaining)
        {
            entries.emplace_back(std::move(design));
        }
    }
    return entries;
}

bool ExampleDesigns::isDesignFileName(const QString& fileName)
{
    const qsizetype length = fileName.size();
    if (length < kExtensionLength)
    {
        return false;
    }
    const qsizetype dot = length - kExtensionLength;
    if (fileName.at(dot) != QChar(u'.') || !isAsciiLetter(fileName.at(dot + 1), u'o') ||
        !isAsciiLetter(fileName.at(dot + 2), u'r') || !isAsciiLetter(fileName.at(dot + 3), u'k'))
    {
        return false;
    }
    return std::ranges::none_of(fileName.first(dot), &isLineTerminator);
}

QString ExampleDesigns::designName(const QString& fileName)
{
    return fileName.first(std::max<qsizetype>(fileName.size() - kExtensionLength, 0));
}

}  // namespace QtRocket::Gui
