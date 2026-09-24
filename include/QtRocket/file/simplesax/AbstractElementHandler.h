#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// A base for element handlers (OpenRocket's AbstractElementHandler): closeElement() warns of
/// content it does not expect, and endHandler() does nothing.
class AbstractElementHandler : public ElementHandler
{
public:
    /// Adds "Unknown text in element '<element>', ignoring." when @p content is not blank
    /// (String.trim()) and "Unknown attributes in element '<element>', ignoring." when there are
    /// attributes; never fails.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Does nothing.
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

protected:
    AbstractElementHandler() = default;

    /// @p text as a number (Double.parseDouble), or NaN with @p warning added to @p warnings when
    /// it is not one.
    [[nodiscard]] static double parseDouble(std::string_view text, WarningSet& warnings,
                                            const Warning& warning);
};

}  // namespace QtRocket
