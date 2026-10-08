#include "QtRocket/document/DecalRegistry.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/attachments/FileSystemAttachment.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The folder every unique name starts with.
constexpr std::string_view kDecalFolder = "decals/";

/// What Java's fileNamePattern, "(.*?)( \((\d+)\)+)?\.(\w*)", captures when it matches the whole
/// of a name.
struct FileNameParts
{
    std::string_view                baseName;   ///< group 1 (BASE_NAME_INDEX)
    std::optional<std::string_view> number;     ///< group 3 (NUMBER_INDEX); nullopt: no group 2
    std::string_view                extension;  ///< group 4 (EXTENSION_INDEX)
};

/// Java's \w without UNICODE_CHARACTER_CLASS: an ASCII letter, a digit or the underscore.
[[nodiscard]] bool isRegexWordCharacter(char c) noexcept
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

/// Java's \d without UNICODE_CHARACTER_CLASS: an ASCII digit.
[[nodiscard]] bool isRegexDigit(char c) noexcept
{
    return c >= '0' && c <= '9';
}

/// Whether @p text holds a character Java's '.' does not match: one of the line terminators \n,
/// \r, U+0085, U+2028 and U+2029 (the last three in UTF-8).
[[nodiscard]] bool hasLineTerminator(std::string_view text) noexcept
{
    return text.find_first_of("\n\r") != std::string_view::npos || text.contains("\xC2\x85") ||
           text.contains("\xE2\x80\xA8") || text.contains("\xE2\x80\xA9");
}

/// fileNamePattern.matcher(name).matches() with its groups, or nullopt when the pattern does not
/// match the whole of @p name. Written out by hand, as the pattern leaves no choice:
/// - "\.(\w*)" has to take the rest of the name and \w matches no dot, so the dot is the last
///   one and only word characters may follow it;
/// - "(.*?)" and the optional group take everything before that dot, and '.' matches no line
///   terminator, so there must be none;
/// - the reluctant "(.*?)" leaves to the optional group whatever that group can take, and the
///   group has to end at the dot: closing parentheses (one or more), before them digits (one or
///   more), before them " (". Read backwards from the dot each run is the longest there is, so
///   there is at most one way to match the group.
[[nodiscard]] std::optional<FileNameParts> matchFileName(std::string_view name)
{
    const std::size_t dot = name.rfind('.');
    if (dot == std::string_view::npos)
    {
        return std::nullopt;
    }
    const std::string_view extension = name.substr(dot + 1);
    const std::string_view head      = name.substr(0, dot);
    if (!std::ranges::all_of(extension, isRegexWordCharacter) || hasLineTerminator(head))
    {
        return std::nullopt;
    }

    std::size_t digitsEnd = head.size();
    while (digitsEnd > 0 && head[digitsEnd - 1] == ')')
    {
        --digitsEnd;
    }
    std::size_t digitsBegin = digitsEnd;
    while (digitsBegin > 0 && isRegexDigit(head[digitsBegin - 1]))
    {
        --digitsBegin;
    }
    const bool numbered = digitsEnd < head.size() && digitsBegin < digitsEnd && digitsBegin >= 2 &&
                          head[digitsBegin - 1] == '(' && head[digitsBegin - 2] == ' ';
    if (!numbered)
    {
        return FileNameParts{.baseName = head, .number = std::nullopt, .extension = extension};
    }
    return FileNameParts{.baseName  = head.substr(0, digitsBegin - 2),
                         .number    = head.substr(digitsBegin, digitsEnd - digitsBegin),
                         .extension = extension};
}

/// Java's checkPathConsistency(): @p name with "decals/" in front unless it starts with it.
[[nodiscard]] std::string checkPathConsistency(std::string_view name)
{
    std::string consistent;
    if (!name.starts_with(kDecalFolder))
    {
        consistent.append(kDecalFolder);
    }
    consistent.append(name);
    return consistent;
}

/// Adds the number of the registered name @p registered to @p counts, when it has one. A number
/// beyond an int is not counted (Java: Integer.parseInt throws a NumberFormatException).
void addNumber(std::set<int>& counts, const FileNameParts& registered)
{
    if (!registered.number.has_value())
    {
        return;
    }
    if (const std::optional<int> number = Strings::parseInt(*registered.number))
    {
        counts.insert(*number);
    }
}

/// Java's findMissingInteger(): the smallest integer from 1 that is not in @p counts.
[[nodiscard]] int findMissingInteger(const std::set<int>& counts)
{
    int newIndex = 1;
    while (counts.contains(newIndex))
    {
        newIndex++;
    }
    return newIndex;
}

/// Whether @p file ends in a separator that Java's File drops when it is made: "x/" is the File
/// "x". A root has none to drop.
[[nodiscard]] bool hasTrailingSeparator(const std::filesystem::path& file)
{
    return !file.has_filename() && file.has_relative_path();
}

/// @p file as the File Java makes of it (see hasTrailingSeparator()).
[[nodiscard]] std::filesystem::path withoutTrailingSeparator(const std::filesystem::path& file)
{
    return hasTrailingSeparator(file) ? file.parent_path() : file;
}

}  // namespace

std::shared_ptr<DecalImage> DecalRegistry::getDecalImage(
    const std::shared_ptr<const Attachment>& attachment)
{
    QTROCKET_ASSERT(attachment != nullptr);
    if (const auto* const fileAttachment =
            dynamic_cast<const FileSystemAttachment*>(attachment.get()))
    {
        const std::filesystem::path& location = fileAttachment->getLocation();
        if (std::shared_ptr<DecalImage> found = findDecalForFile(m_registeredDecals, location))
        {
            return found;
        }
        // Not OpenRocket's: a removed image of this file comes back under its name, which the
        // rocket or an undo state may still hold (see the class comment).
        if (const std::shared_ptr<DecalImage> removed =
                findDecalForFile(m_detachedDecals, location))
        {
            return registerAgain(removed->getName());
        }

        // It's a new file, generate a name for it (Java: makeUniqueName(location.getName())).
        std::string decalName =
            makeUniqueName(pathToUtf8(withoutTrailingSeparator(location).filename()));
        std::shared_ptr<DecalImage> image = std::make_shared<DecalImage>(decalName, attachment);
        image->setDecalFile(location);
        m_registeredDecals.insert_or_assign(std::move(decalName), image);
        return image;
    }

    const std::string& decalName = attachment->getName();
    if (const auto found = m_registeredDecals.find(decalName); found != m_registeredDecals.end())
    {
        return found->second;
    }
    // Not OpenRocket's: the removed image of this name comes back.
    if (std::shared_ptr<DecalImage> removed = registerAgain(decalName))
    {
        return removed;
    }
    std::shared_ptr<DecalImage> image = std::make_shared<DecalImage>(attachment);
    m_registeredDecals.insert_or_assign(decalName, image);
    return image;
}

std::shared_ptr<DecalImage> DecalRegistry::registerAgain(std::string_view name)
{
    const auto removed = m_detachedDecals.find(name);
    if (removed == m_detachedDecals.end())
    {
        return nullptr;
    }
    std::shared_ptr<DecalImage> image = removed->second;
    m_registeredDecals.insert_or_assign(removed->first, image);
    m_detachedDecals.erase(removed);
    return image;
}

std::shared_ptr<DecalImage> DecalRegistry::makeUniqueImage(
    const std::shared_ptr<DecalImage>& original)
{
    if (original == nullptr)
    {
        return original;
    }

    std::string newName = makeUniqueName(original->getName());

    // Return the old decal if a new one isn't required.
    if (newName == original->getName())
    {
        return original;
    }

    std::shared_ptr<DecalImage> newDecal = original->copyWithName(newName);
    m_registeredDecals.insert_or_assign(std::move(newName), newDecal);
    return newDecal;
}

std::string DecalRegistry::makeUniqueName(std::string_view name) const
{
    std::string                        newName = checkPathConsistency(name);
    const std::optional<FileNameParts> asked   = matchFileName(newName);
    // Java's getGroup(): empty when the name does not match.
    const std::string_view basename  = asked.has_value() ? asked->baseName : std::string_view{};
    const std::string_view extension = asked.has_value() ? asked->extension : std::string_view{};

    std::set<int> counts;
    bool          needsRewrite = false;

    // Java walks the registered images. The removed ones count here too: their names stay
    // taken (see the class comment).
    for (const Images* const images : {&m_registeredDecals, &m_detachedDecals})
    {
        for (const auto& [takenName, image] : *images)
        {
            const std::optional<FileNameParts> taken = matchFileName(takenName);
            if (taken.has_value())
            {
                if (basename == taken->baseName && extension == taken->extension)
                {
                    addNumber(counts, *taken);
                    needsRewrite = true;
                }
            }
            else if (newName == takenName)
            {
                needsRewrite = true;
            }
        }
    }

    if (!needsRewrite)
    {
        return newName;
    }

    // Plain digits (Java: MessageFormat.format("{0} ({1}).{2}", ...), which groups them).
    return std::format("{} ({}).{}", basename, findMissingInteger(counts), extension);
}

std::vector<std::shared_ptr<DecalImage>> DecalRegistry::getDecalList() const
{
    std::vector<std::shared_ptr<DecalImage>> decals;
    decals.reserve(m_registeredDecals.size());
    for (const auto& [name, image] : m_registeredDecals)
    {
        decals.push_back(image);
    }
    // The map is in byte order, which is String.compareTo's but for the characters beyond
    // U+FFFF.
    std::ranges::stable_sort(
        decals, [](const std::shared_ptr<DecalImage>& a, const std::shared_ptr<DecalImage>& b) {
            return a->compareTo(*b) < 0;
        });
    return decals;
}

bool DecalRegistry::removeDecal(const DecalImage* decal)
{
    if (decal == nullptr)
    {
        return false;
    }
    const auto registered = m_registeredDecals.find(decal->getName());
    if (registered == m_registeredDecals.end())
    {
        return false;
    }
    // Java: registeredDecals.remove(name). The image is kept, out of the list (see the class
    // comment).
    m_detachedDecals.insert_or_assign(registered->first, registered->second);
    m_registeredDecals.erase(registered);
    return true;
}

std::shared_ptr<DecalImage> DecalRegistry::find(std::string_view name) const
{
    if (const auto found = m_registeredDecals.find(name); found != m_registeredDecals.end())
    {
        return found->second;
    }
    const auto removed = m_detachedDecals.find(name);
    return removed != m_detachedDecals.end() ? removed->second : nullptr;
}

bool DecalRegistry::isRegistered(std::string_view name) const
{
    return m_registeredDecals.contains(name);
}

std::shared_ptr<DecalImage> DecalRegistry::findDecalForFile(const Images&                images,
                                                            const std::filesystem::path& file)
{
    const std::filesystem::path asked = withoutTrailingSeparator(file);
    // Of several images with that file, the first in name order (see the class comment).
    std::shared_ptr<DecalImage> found;
    for (const auto& [name, image] : images)
    {
        const std::optional<std::filesystem::path>& decalFile = image->getDecalFile();
        if (!decalFile.has_value())
        {
            continue;
        }
        const bool same = hasTrailingSeparator(*decalFile) ? decalFile->parent_path() == asked
                                                           : *decalFile == asked;
        if (same && (found == nullptr || image->compareTo(*found) < 0))
        {
            found = image;
        }
    }
    return found;
}

}  // namespace QtRocket
