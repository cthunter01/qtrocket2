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
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/file/simplesax/PlainTextHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
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
            return nullptr;
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
        warnings.add(Warning::kFileInvalidParameter);
        return false;
    }

    // From here on as Java, step by step: a load that fails has registered the image. (The
    // name is there, or the decal was ignored above.)
    const std::shared_ptr<const Attachment> attachment =
        m_context->getAttachmentFactory()->getAttachment(name.value_or(std::string_view{}));
    const std::shared_ptr<DecalImage> image =
        m_context->getOpenRocketDocument()->getDecalImage(attachment);
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

Result<void> AppearanceHandler::closeElement(std::string_view element, const Attributes& attributes,
                                             std::string_view content, WarningSet& warnings)
{
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
        warnings.add(Warning::kFileInvalidParameter);
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
        warnings.add(Warning::kFileInvalidParameter);
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
