#pragma once

#include <string_view>

#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// The handler of an element that the loader ignores with everything in it, and says nothing
/// about: every element within is its own again, and nothing that closes gives a warning.
///
/// It has no counterpart in OpenRocket, whose handlers ignore an element by returning null from
/// openElement(). That way has a slip, which simplesax's DelegatorHandler reproduces: the
/// attributes and the text of the ignored element stay behind, and every enclosing element
/// then closes with those of the element one level deeper, so that an element that was read
/// well is warned of ("Unknown attributes in element 'nosecone', ignoring.") or reads another
/// element's attributes as its own. A handler keeps the null return wherever OpenRocket has it,
/// because the warnings of a file depend on it. Where QtRocket ignores an element that
/// OpenRocket would not ignore (a decal it cannot place, a simulation beyond the bound), there
/// is nothing of OpenRocket's to reproduce, and the element is read by this handler instead:
/// the stacks of the DelegatorHandler stay as they are for every other element.
///
/// The handler that returns it is told of the element's end as of any other element's
/// (closeElement(), with the element's own attributes and text), and decides there what it
/// says of them.
class IgnoredElementHandler final : public ElementHandler
{
public:
    /// The shared instance; it has no state.
    [[nodiscard]] static IgnoredElementHandler& instance() noexcept;

    /// This handler, for every element.
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

    /// Does nothing.
    [[nodiscard]] Result<void> closeElement(std::string_view element, const Attributes& attributes,
                                            std::string_view content,
                                            WarningSet&      warnings) override;

    /// Does nothing.
    [[nodiscard]] Result<void> endHandler(std::string_view element, const Attributes& attributes,
                                          std::string_view content, WarningSet& warnings) override;

private:
    IgnoredElementHandler() = default;
};

}  // namespace QtRocket
