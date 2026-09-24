#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The handler of an element that has attributes but no content (OpenRocket's
/// NullElementHandler): a child element is ignored with the warning "Unknown element <element>,
/// ignoring.", and a child's text is warned of as AbstractElementHandler does, its attributes
/// not.
class NullElementHandler final : public AbstractElementHandler
{
public:
    /// The shared instance (INSTANCE); it has no state.
    [[nodiscard]] static NullElementHandler& instance() noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    NullElementHandler() = default;
};

}  // namespace QtRocket
