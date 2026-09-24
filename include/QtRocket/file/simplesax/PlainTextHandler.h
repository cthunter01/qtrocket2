#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The handler of an element that holds only text (OpenRocket's PlainTextHandler): a child
/// element is ignored with the warning "Unknown element <element>, ignoring.", and closeElement()
/// adds nothing (the warning from openElement() is sufficient). The parent reads the text in its
/// own closeElement().
class PlainTextHandler final : public AbstractElementHandler
{
public:
    /// The shared instance (INSTANCE); it has no state.
    [[nodiscard]] static PlainTextHandler& instance() noexcept;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

private:
    PlainTextHandler() = default;
};

}  // namespace QtRocket
