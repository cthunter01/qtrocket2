#pragma once

#include <string>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"

namespace QtRocket
{

class OrkWriter;
class Preferences;
class RocketComponent;

/// Chooses the .ork saver of a component (OpenRocketSaver.findGetElementsMethod(), which walks
/// the component's class chain by reflection to the first class with a saver of its name:
/// BodyTube to BodyTubeSaver, ParallelStage to AxialStageSaver, PodSet to
/// ComponentAssemblySaver). Here a table over the ComponentKinds, one entry per kind in
/// kAllComponentKinds' order (checked at compile time), names the function of
/// AssemblySaver, ExternalComponentSaver or InternalComponentSaver that Java's reflection would
/// find.
///
/// getElements() gives the lines of the component's element: the open tag, the parameter lines,
/// the close tag (Java's getElements()), or none when the saver writes nothing for it (a stage
/// that is not what its kind's saver expects). OpenRocketSaver writes them, with the
/// subcomponents between the parameters and the close tag.
class ComponentSaver final
{
public:
    ComponentSaver() = delete;

    /// A saver: the lines of the element of a component of its kind. @p preferences are the
    /// application's, which decide whether a flight configuration's name is its own; texts go
    /// through the writer's escape().
    using GetElements = std::vector<std::string> (*)(const RocketComponent& component,
                                                     OrkWriter&             writer,
                                                     const Preferences&     preferences);

    /// One entry of the table.
    struct Saver
    {
        ComponentKind kind;
        GetElements   getElements;
    };

    /// The saver of @p kind.
    [[nodiscard]] static GetElements saverOf(ComponentKind kind);

    /// The lines of @p component's element, by the saver of its kind (Java: the getElements()
    /// that findGetElementsMethod() finds, invoked on @p component).
    [[nodiscard]] static std::vector<std::string> getElements(const RocketComponent& component,
                                                              OrkWriter&             writer,
                                                              const Preferences&     preferences);
};

}  // namespace QtRocket
