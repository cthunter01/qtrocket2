#pragma once

#include <memory>
#include <string_view>

#include "QtRocket/file/simplesax/AbstractElementHandler.h"
#include "QtRocket/file/simplesax/ElementHandler.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DocumentLoadingContext;
class RocketComponent;

/// Reads a <subcomponents> element: it makes the component each child element names and puts
/// it into the tree (OpenRocket's file/openrocket/importt/ComponentHandler). What the element
/// of a component holds is then read by a ComponentParameterHandler.
///
/// For a child element:
/// - a name the component table does not have (DocumentConfig::createComponent(); "rocket" is
///   none of them) is ignored with "Unknown element <name>, ignoring.";
/// - the new component must fit below the parent (RocketComponent::isCompatible()): a fin set
///   below a stage, a stage below a body tube or a body tube below the rocket is ignored, with
///   everything in it, with "<component name> cannot be attached to <parent's component name>;
///   ignoring this component and its subcomponents." (the names being getComponentName():
///   "Trapezoidal Fin Set", "Stage"). The elements that follow are read as usual;
/// - the component is added as the parent's last child, with the values its constructor gives
///   it, BEFORE anything of its element is read. So every setter finds the component in the
///   tree and fires its events into the rocket, whose events are on while a file loads, and the
///   state a load ends in depends on it (a stage has its number, an automatic radius sees the
///   neighbours that are there so far), as in OpenRocket.
/// A component element's own text and attributes get AbstractElementHandler's warnings
/// ("Unknown text in element 'bodytube', ignoring.").
///
/// Deviations from OpenRocket:
/// - Components may not be nested without bound: a component that would stand more than
///   kMaxDepth levels below the rocket is ignored, with everything in it, with "<component
///   name> is nested too deeply; ignoring this component and its subcomponents." (QtRocket's
///   own text). OpenRocket has no bound. With the rocket's events on, every component that is
///   attached makes the whole tree update, and each update walks from every component up to
///   the rocket, so the time to read a chain of inner tubes within inner tubes, a few bytes
///   per level, grows with about the cube of its depth: measured here in a debug build, 0.24 s
///   for 100 levels and 89 s for 1000, and 3000 levels did not end within five minutes. Every
///   walk of the tree that calls itself per level, the destruction of the rocket among them,
///   goes as deep as the file says too; no stack overflow was seen up to 1000 levels, and
///   deeper trees were not reached.
/// - A new component has the built-in default materials (see
///   DocumentConfig::createComponent()); Java's constructors ask the application's preferences
///   for the default material of the component's class. It shows only in a component whose
///   element has no <material>, which no file OpenRocket wrote has.
class ComponentHandler final : public AbstractElementHandler
{
public:
    /// The deepest a component of a file may stand below the rocket: a stage is at level 1,
    /// its body tube at 2. (The deepest component of the 16 example designs and of the 18
    /// designs of tests/data/ork is at level 6, in "Parallel booster staging".)
    static constexpr int kMaxDepth = 100;

    /// The handler of the <subcomponents> element of @p parent. Both @p parent and @p context
    /// must outlive the handler.
    ComponentHandler(RocketComponent& parent, const DocumentLoadingContext& context) noexcept;
    /// A temporary context would dangle.
    ComponentHandler(RocketComponent& parent, const DocumentLoadingContext&& context) = delete;

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes,
                                                      WarningSet&       warnings) override;

private:
    RocketComponent*              m_parent;
    const DocumentLoadingContext* m_context;
    /// The handler of the component read last (a ComponentParameterHandler); the next
    /// component replaces it.
    std::unique_ptr<ElementHandler> m_componentHandler;
};

}  // namespace QtRocket
