#pragma once

#include <string_view>

#include "QtRocket/file/openrocket/AppearanceHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Reads the <insideappearance> element of a component, "inside-appearance" in the files of
/// some development versions: what its inside looks like, and how the inside and the edges go
/// with the outside (OpenRocket's file/openrocket/importt/InsideAppearanceHandler). It is an
/// AppearanceHandler whose appearance becomes the component's inside appearance when the
/// element ends (InsideColorComponentHandler::setInsideAppearance(), a NONFUNCTIONAL_CHANGE
/// event), and which knows two children more:
/// - <edgessameasinside>, also spelled "edgesSameAsInside": Boolean.parseBoolean of the text,
///   which is not trimmed, for InsideColorComponentHandler::setEdgesSameAsInside();
/// - <insidesameasoutside>, also spelled "insideSameAsOutside": likewise for
///   setSeparateInsideOutside(). The value is passed on as it is, although the two names say
///   the opposite of each other; OpenRocket's saver writes isSeparateInsideOutside() under this
///   name, so a file comes back as it was saved.
/// Each of the two fires GRAPHIC_CHANGE when it changes its flag.
///
/// A component that has no inside of its own colour (one that is no InsideColorComponent: a
/// stage, a parachute, a centering ring) takes nothing of the element, without a warning. The
/// element is read all the same: its decal's image is registered with the document, and what
/// fails the load of an <appearance> fails it here.
///
/// The deviations are AppearanceHandler's.
class InsideAppearanceHandler final : public AppearanceHandler
{
public:
    /// The handler of the <insideappearance> element of @p component. Both @p component and
    /// @p context, and the document of the context, must outlive the handler.
    /// @throws BugError when @p context has no document
    InsideAppearanceHandler(RocketComponent& component, const DocumentLoadingContext& context);
    /// A temporary context would dangle.
    InsideAppearanceHandler(RocketComponent&               component,
                            const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

protected:
    /// Gives the component the appearance the element holds as its inside appearance, when it
    /// has one.
    void setAppearance() override;
};

}  // namespace QtRocket
