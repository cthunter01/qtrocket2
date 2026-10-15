#include "QtRocket/file/openrocket/AppearanceHandler.h"

#include <expected>
#include <format>
#include <memory>
#include <optional>
#include <string_view>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/AttachmentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/openrocket/CsvLookupHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/IgnoredElementHandler.h"
#include "QtRocket/file/openrocket/LoaderWarnings.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

AppearanceHandler::AppearanceHandler(RocketComponent&              component,
                                     const DocumentLoadingContext& context)
  : m_component(&component), m_context(&context)
{
    if (m_context->getOpenRocketDocument() == nullptr)
    {
        bug("The loading context of an appearance handler has no document");
    }
}

Result<ElementHandler*> AppearanceHandler::openElement(std::string_view  element,
                                                       const Attributes& attributes,
                                                       WarningSet&       warnings)
{
    if (element == "decal")
    {
        const Result<bool> opened = openDecal(attributes, warnings);
        if (!opened)
        {
            return std::unexpected(opened.error());
        }
        if (!*opened)
        {
            // Not OpenRocket's, which dies of such a decal: it is read by a handler that says
            // nothing, so that the elements around it close with what is theirs (see the
            // class comment).
            m_isIgnoringDecal = true;
            return &IgnoredElementHandler::instance();
        }
        m_isInDecal = true;
        return this;
    }
    return &PlainTextHandler::instance();
}

Result<bool> AppearanceHandler::openDecal(const Attributes& attributes, WarningSet& warnings)
{
    const std::optional<std::string_view> name = DocumentConfig::attribute(attributes, "name");
    const std::optional<std::string_view> rotationText =
        DocumentConfig::attribute(attributes, "rotation");
    const std::optional<std::string_view> edgeModeName =
        DocumentConfig::attribute(attributes, "edgemode");

    // Java reads the name, the rotation and the edge mode in this order and dies of a
    // NullPointerException at the first one that is missing, unless a rotation that is no
    // number has failed the load before. A decal it dies of is ignored here, and nothing of it
    // is applied.
    const Result<double> rotation =
        rotationText.has_value() ? DocumentConfig::parseDouble(*rotationText) : Result<double>(0.0);
    const bool diesInJava = !name.has_value() || !rotationText.has_value() ||
                            (rotation.has_value() && !edgeModeName.has_value());
    if (diesInJava)
    {
        warnings.add(LoaderWarnings::kDecalIgnored);
        return false;
    }
    // The name is there, or the decal was ignored above.
    const std::string_view decalName = name.value_or(std::string_view{});
    // Not OpenRocket's (decision D8 of 2026-10-14; see the class comment).
    if (!mayReadImageFile(decalName, warnings))
    {
        return false;
    }

    // From here on as Java, step by step: a load that fails has registered the image.
    const std::shared_ptr<const Attachment> attachment =
        m_context->getAttachmentFactory()->getAttachment(decalName);
    OpenRocketDocument& document = *m_context->getOpenRocketDocument();
    // Not OpenRocket's: the images of a document are bounded (see the class comment).
    if (document.getDecalImageCount() >= DocumentConfig::kMaxDecalImages &&
        !document.hasDecalImage(attachment))
    {
        warnings.add(LoaderWarnings::tooManyDecalImages());
        return false;
    }
    const std::shared_ptr<DecalImage> image = document.getDecalImage(attachment);
    // The decal names the image, and the registry may have given it another name than the
    // file's.
    m_builder.setImage(image->getName());

    if (!rotation)
    {
        return std::unexpected(rotation.error());
    }
    m_builder.setRotation(*rotation);

    // EdgeMode.valueOf(edgeModeName); the name is there, or the decal was ignored above.
    const std::string_view               edgeModeText = edgeModeName.value_or(std::string_view{});
    const std::optional<Decal::EdgeMode> edgeMode     = edgeModeFromName(edgeModeText);
    if (!edgeMode.has_value())
    {
        return fail(
            ErrorCode::INVALID_ARGUMENT,
            std::format("No enum constant info.openrocket.core.appearance.Decal.EdgeMode.{}",
                        edgeModeText));
    }
    m_builder.setEdgeMode(*edgeMode);
    return true;
}

bool AppearanceHandler::mayReadImageFile(std::string_view name, WarningSet& warnings) const
{
    if (!m_context->getAttachmentFactory()->readsFileSystem())
    {
        // The entries of an archive, whatever their names.
        return true;
    }
    const auto file =
        CsvLookupHandler::fileOfDesign(m_context->getDesignDirectory(), pathFromUtf8(name));
    if (file.has_value())
    {
        return true;
    }
    warnings.add(file.error() == CsvLookupHandler::Refusal::DEVICE_NAME
                     ? LoaderWarnings::decalFileOfDeviceName(name)
                     : LoaderWarnings::decalFileOutsideDesign(name));
    return false;
}

Result<void> AppearanceHandler::closeElement(std::string_view element, const Attributes& attributes,
                                             std::string_view content, WarningSet& warnings)
{
    if (m_isIgnoringDecal)
    {
        // The decal that was ignored ends (IgnoredElementHandler has read everything in it):
        // the one warning it got when it opened is all that is said of it.
        m_isIgnoringDecal = false;
        return {};
    }
    if (element == "paint")
    {
        closePaint(attributes);
        return {};
    }
    if (element == "shine")
    {
        return closeShine(content);
    }
    if (element == "opacityaffectstexture")
    {
        // Boolean.parseBoolean(content)
        m_builder.setOpacityAffectsTexture(Strings::javaEqualsIgnoreCase(content, "true"));
        return {};
    }
    if (m_isInDecal && element == "center")
    {
        return closePair(attributes, warnings, &AppearanceBuilder::setCenter);
    }
    if (m_isInDecal && element == "offset")
    {
        return closePair(attributes, warnings, &AppearanceBuilder::setOffset);
    }
    if (m_isInDecal && element == "scale")
    {
        return closePair(attributes, warnings, &AppearanceBuilder::setScaleUV);
    }
    if (m_isInDecal && element == "decal")
    {
        m_isInDecal = false;
        return {};
    }

    if (element == "decal")
    {
        // Java removed the three attributes from the map when the decal opened; here the map is
        // const, so the others are told from a copy without them.
        Attributes others = attributes;
        others.erase("name");
        others.erase("rotation");
        others.erase("edgemode");
        return AbstractElementHandler::closeElement(element, others, content, warnings);
    }
    return AbstractElementHandler::closeElement(element, attributes, content, warnings);
}

void AppearanceHandler::closePaint(const Attributes& attributes)
{
    // ORColor.fromXMLAttributes(attributes)
    const std::optional<Color> paint =
        Color::fromXmlAttributes(DocumentConfig::attribute(attributes, "red"),
                                 DocumentConfig::attribute(attributes, "green"),
                                 DocumentConfig::attribute(attributes, "blue"),
                                 DocumentConfig::attribute(attributes, "alpha"));
    if (paint.has_value())
    {
        m_builder.setPaint(*paint);
    }
}

Result<void> AppearanceHandler::closeShine(std::string_view content)
{
    const Result<double> shine = DocumentConfig::parseDouble(content);
    if (!shine)
    {
        return std::unexpected(shine.error());
    }
    m_builder.setShine(*shine);
    return {};
}

Result<void> AppearanceHandler::closePair(const Attributes& attributes, WarningSet& warnings,
                                          void (AppearanceBuilder::*set)(double, double))
{
    // x before y, as Java reads them: a missing one is a NullPointerException there.
    const std::optional<std::string_view> xText = DocumentConfig::attribute(attributes, "x");
    if (!xText.has_value())
    {
        warnings.add(LoaderWarnings::kDecalPairIgnored);
        return {};
    }
    const Result<double> x = DocumentConfig::parseDouble(*xText);
    if (!x)
    {
        return std::unexpected(x.error());
    }
    const std::optional<std::string_view> yText = DocumentConfig::attribute(attributes, "y");
    if (!yText.has_value())
    {
        warnings.add(LoaderWarnings::kDecalPairIgnored);
        return {};
    }
    const Result<double> y = DocumentConfig::parseDouble(*yText);
    if (!y)
    {
        return std::unexpected(y.error());
    }
    (m_builder.*set)(*x, *y);
    return {};
}

Result<void> AppearanceHandler::endHandler(std::string_view element, const Attributes& attributes,
                                           std::string_view content, WarningSet& warnings)
{
    if (element == "decal")
    {
        m_isInDecal = false;
        return {};
    }
    setAppearance();
    return AbstractElementHandler::endHandler(element, attributes, content, warnings);
}

void AppearanceHandler::setAppearance()
{
    m_component->setAppearance(m_builder.getAppearance());
}

}  // namespace QtRocket
