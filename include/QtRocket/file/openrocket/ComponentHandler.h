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
/// - the new component is given the default materials the context's preferences name for its
///   class, which OpenRocket's constructors ask the application's preferences for: the
///   material of an external and of a structural component and the fillets of a fin set
///   (BULK), the canopy of a parachute or streamer (SURFACE; asked for RecoveryDevice whatever
///   the device, so a default stored for Parachute or Streamer does not reach it), the lines
///   of a parachute and the material of a shock cord (LINE). The nearest class of the
///   component's Java classes that has a default decides, and one of another type than asked
///   for gives the built-in default (getDefaultComponentMaterial() in MaterialPreferences.h);
///   a rail button is made of Delrin whatever the preferences say. This is done before the
///   component is attached, so nothing is fired, as nothing is by a Java constructor. A
///   <material> of the element then replaces the default as any setter does;
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
/// - Nor may a file give the rocket component instances without bound: a component whose
///   instances (as many as the instance counts of the components above it multiply to, times
///   its own count as its constructor gives it: 2 for a pod set, 3 for a fin set) would take
///   the rocket beyond DocumentConfig::kMaxInstances over all its flight configurations is
///   ignored, with everything in it, with "<component name> would give the rocket too many
///   component instances; ignoring this component and its subcomponents." (QtRocket's own
///   text; DocumentConfig::childFits()). The setters of the counts refuse a number that would
///   do the same (IntSetter, ClusterConfigurationSetter). OpenRocket has no bound and runs
///   out of memory: see DocumentConfig::kMaxInstances.
/// - The two bounds above leave the time of a load open. With the rocket's events on, every
///   element that changes the rocket makes every flight configuration build its instances
///   anew, so the time grows with the square of what a file holds, also within the bounds:
///   measured in a release build through the test root, 3200 body tubes side by side (35 KB)
///   took 34 s, 1600 empty flight configurations behind a stage of 20 tubes 65 s, 1600 motors
///   in one mount 10 s, 100000 separation configurations of one stage 17.5 s, and 80000
///   different unknown elements 28 s (WarningSet looks for a warning it has before it adds
///   one). OpenRocket is as slow or slower (108 s for 1600 tubes, 108 s for the 1600
///   configurations). Whether the number of components, of configurations and of warnings of
///   a file should have bounds of their own is an open question.
/// - The default materials of the preferences are given by this handler (decision D2: what
///   Java asks the application for is handed in, here with the loading context), where
///   OpenRocket's constructors take them themselves: a component of this library is made with
///   the built-in defaults (see ExternalComponent, StructuralComponent, RecoveryDevice,
///   Parachute). The handler does it only when the context has preferences and application
///   materials, and those hold the three built-in materials the lookup falls back on
///   (OpenRocket's databases always do; getDefaultComponentMaterial() has a BugError for a
///   storage without one). Otherwise a new component keeps the built-in defaults, which are
///   what preferences that name no default give.
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
