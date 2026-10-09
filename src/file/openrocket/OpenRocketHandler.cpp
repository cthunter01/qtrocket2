#include "QtRocket/file/openrocket/OpenRocketHandler.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/OpenRocketContentHandler.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// Whether @p text is what Java's pattern [0-9]+ matches: ASCII digits, at least one.
[[nodiscard]] bool isDigits(std::string_view text) noexcept
{
    return !text.empty() && std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
}

/// The warning for a version the loader does not know.
[[nodiscard]] std::string unsupportedVersionText(std::optional<std::string_view> docVersion,
                                                 std::optional<std::string_view> creator)
{
    std::string text = "Unsupported document version";
    if (docVersion.has_value())
    {
        text += ' ';
        text += *docVersion;
    }
    if (creator.has_value() && !Strings::trim(*creator).empty())
    {
        text += " (written using '";
        text += Strings::trim(*creator);
        text += "')";
    }
    text += ", attempting to read file anyway.";
    return text;
}

}  // namespace

OpenRocketHandler::OpenRocketHandler(DocumentLoadingContext& context) noexcept : m_context(&context)
{
}

Result<ElementHandler*> OpenRocketHandler::openElement(std::string_view  element,
                                                       const Attributes& attributes,
                                                       WarningSet&       warnings)
{
    // Check for unknown elements
    if (element != "openrocket")
    {
        warnings.add(
            Warning::fromString("Unknown element " + std::string(element) + ", ignoring."));
        return nullptr;
    }

    // Check for first call
    if (m_handler != nullptr)
    {
        warnings.add(Warning::fromString("Multiple document elements found, ignoring later ones."));
        return nullptr;
    }

    // Check version number
    const std::optional<std::string_view> creator =
        DocumentConfig::attribute(attributes, "creator");
    const std::optional<std::string_view> docVersion =
        DocumentConfig::attribute(attributes, "version");
    if (!docVersion.has_value() || !DocumentConfig::isSupportedVersion(*docVersion))
    {
        warnings.add(unsupportedVersionText(docVersion, creator));
    }

    Result<int> fileVersion = parseVersion(docVersion);
    if (!fileVersion)
    {
        return std::unexpected(std::move(fileVersion.error()));
    }
    m_context->setFileVersion(*fileVersion);

    m_handler = std::make_unique<OpenRocketContentHandler>(*m_context);
    return m_handler.get();
}

Result<int> OpenRocketHandler::parseVersion(std::optional<std::string_view> docVersion)
{
    if (!docVersion.has_value())
    {
        return 0;
    }
    // Java: the pattern ^([0-9]+)\.([0-9]+)$, which has to match the whole text.
    const std::size_t dot = docVersion->find('.');
    if (dot == std::string_view::npos)
    {
        return 0;
    }
    const std::string_view majorText = docVersion->substr(0, dot);
    const std::string_view minorText = docVersion->substr(dot + 1);
    if (!isDigits(majorText) || !isDigits(minorText))
    {
        return 0;
    }
    const Result<int> major = DocumentConfig::parseInt(majorText);
    if (!major)
    {
        return std::unexpected(major.error());
    }
    const Result<int> minor = DocumentConfig::parseInt(minorText);
    if (!minor)
    {
        return std::unexpected(minor.error());
    }
    // Java's int arithmetic wraps; here it is done unsigned, which wraps the same way.
    const std::uint32_t version =
        (static_cast<std::uint32_t>(*major) *
         static_cast<std::uint32_t>(DocumentConfig::kFileVersionDivisor)) +
        static_cast<std::uint32_t>(*minor);
    return static_cast<int>(version);
}

Result<void> OpenRocketHandler::closeElement(std::string_view element, const Attributes& attributes,
                                             std::string_view content, WarningSet& warnings)
{
    // Java removes the two from the map it was given, which is const here.
    Attributes others = attributes;
    others.erase("version");
    others.erase("creator");
    return AbstractElementHandler::closeElement(element, others, content, warnings);
}

}  // namespace QtRocket
