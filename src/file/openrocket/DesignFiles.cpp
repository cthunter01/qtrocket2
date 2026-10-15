#include "QtRocket/file/openrocket/DesignFiles.h"

#include <algorithm>
#include <array>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Whether @p name, one element of a path, is made of dots and blanks only and is not ".".
/// ".." is one, and so is what Windows may read as "." or ".." once it has dropped the dots and
/// blanks at the end of a name ("...", ".. ", ". .").
[[nodiscard]] bool isDotsAndBlanks(std::string_view name) noexcept
{
    return name != "." && !name.empty() && name.find_first_not_of(". ") == std::string_view::npos;
}

/// The names Windows reserves for a device, but for the numbered ones.
constexpr std::array<std::string_view, 6> kDeviceNames{"CON", "PRN",    "AUX",
                                                       "NUL", "CONIN$", "CONOUT$"};

/// The digits of a numbered device that are no ASCII digit: the superscript one, two and three
/// of Latin-1, as UTF-8.
constexpr std::array<std::string_view, 3> kSuperscriptDigits{"\xC2\xB9", "\xC2\xB2", "\xC2\xB3"};

/// Whether @p text is one digit after which Windows numbers a serial or a parallel port.
[[nodiscard]] bool isDeviceDigit(std::string_view text) noexcept
{
    if (text.size() == 1)
    {
        return text.front() >= '0' && text.front() <= '9';
    }
    const std::span<const std::string_view> superscripts(kSuperscriptDigits);
    return std::ranges::find(superscripts, text) != superscripts.end();
}

}  // namespace

std::expected<std::filesystem::path, DesignFiles::Refusal> DesignFiles::fileOfDesign(
    const std::optional<std::filesystem::path>& designDirectory, const std::filesystem::path& file)
{
    if (!designDirectory.has_value())
    {
        return std::unexpected(Refusal::NO_DIRECTORY);
    }
    if (!isRelativeName(file))
    {
        return std::unexpected(Refusal::NOT_RELATIVE);
    }
    // The name alone decides, in its normal form: every ".." that is left in it stands at its
    // front and leads out of the directory.
    const std::filesystem::path normal = file.lexically_normal();
    bool                        device = false;
    for (const std::filesystem::path& element : normal)
    {
        const std::string name = pathToUtf8(element);
        if (isDotsAndBlanks(name))
        {
            return std::unexpected(Refusal::LEAVES_DIRECTORY);
        }
        device = device || isDeviceName(name);
    }
    // Only where such a name opens the device (see the class comment).
    if (kRefusesDeviceNames && device)
    {
        return std::unexpected(Refusal::DEVICE_NAME);
    }
    return *designDirectory / normal;
}

bool DesignFiles::isRelativeName(const std::filesystem::path& file)
{
    return !file.has_root_name() && !file.has_root_directory();
}

bool DesignFiles::isDeviceName(std::string_view name)
{
    // Windows looks at the name in front of its first dot or colon, without the blanks at the
    // end of that, and compares without regard to case.
    std::string_view stem = name.substr(0, name.find_first_of(".:"));
    while (stem.ends_with(' '))
    {
        stem.remove_suffix(1);
    }
    const std::string                       upper = Strings::toUpper(stem);
    const std::span<const std::string_view> names(kDeviceNames);
    if (std::ranges::find(names, upper) != names.end())
    {
        return true;
    }
    const std::string_view numbered = upper;
    return (numbered.starts_with("COM") || numbered.starts_with("LPT")) &&
           isDeviceDigit(numbered.substr(3));
}

}  // namespace QtRocket
