#include "QtRocket/gui/tree/ComponentLimits.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/ComponentHandler.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

ComponentLimits::Addition& ComponentLimits::Addition::operator+=(const Addition& other) noexcept
{
    components += other.components;
    assemblies += other.assemblies;
    decalImages += other.decalImages;
    deepestLevel = std::max(deepestLevel, other.deepestLevel);
    return *this;
}

std::size_t ComponentLimits::treeSize(const RocketComponent& tree)
{
    return 1 + tree.getAllChildren().size();
}

std::size_t ComponentLimits::levelOf(const RocketComponent& component)
{
    std::size_t level = 0;
    for (const RocketComponent* parent = component.getParent(); parent != nullptr;
         parent                        = parent->getParent())
    {
        ++level;
    }
    return level;
}

std::size_t ComponentLimits::heightOf(const RocketComponent& tree)
{
    std::size_t height = 0;
    for (const RocketComponent* component : tree.getAllChildren())
    {
        // The levels below the tree's root, from the component up to it.
        std::size_t below = 0;
        for (const RocketComponent* up = component; up != &tree; up = up->getParent())
        {
            ++below;
        }
        height = std::max(height, below);
    }
    return height;
}

ComponentLimits::Addition ComponentLimits::placing(const RocketComponent& tree,
                                                   const RocketComponent& parent)
{
    Addition addition   = moving(tree, parent);
    addition.components = treeSize(tree);
    addition.assemblies = isAssembly(tree.kind()) ? 1U : 0U;
    for (const RocketComponent* component : tree.getAllChildren())
    {
        if (isAssembly(component->kind()))
        {
            ++addition.assemblies;
        }
    }
    return addition;
}

ComponentLimits::Addition ComponentLimits::moving(const RocketComponent& tree,
                                                  const RocketComponent& parent)
{
    return {.deepestLevel = levelOf(parent) + 1 + heightOf(tree)};
}

std::optional<UserPrompts::Message> ComponentLimits::refusal(const OpenRocketDocument& document,
                                                             const Addition&           addition,
                                                             const QString&            title)
{
    const Rocket&     rocket = document.getRocket();
    QStringList       lines;
    const std::size_t components = DocumentConfig::componentCount(rocket) + addition.components;
    if (addition.components > 0 && components > DocumentConfig::kMaxComponents)
    {
        //: QtRocket's own text (decision O27): a paste, duplicate, drop or add refused
        lines.append(tr("The rocket would have %1 components. QtRocket opens at most %2 "
                        "components of a design and would ignore the ones beyond when it opens "
                        "this design again.")
                         .arg(components)
                         .arg(DocumentConfig::kMaxComponents));
    }
    if (std::cmp_greater(addition.deepestLevel, ComponentHandler::kMaxDepth))
    {
        //: QtRocket's own text (decision O27, extended by the M2 run 2 review): a paste,
        //: duplicate, drop or add that nests components too deeply refused
        lines.append(tr("Components would stand %1 levels below the rocket. QtRocket opens "
                        "components at most %2 levels deep and would ignore the deeper ones, with "
                        "everything in them, when it opens this design again.")
                         .arg(addition.deepestLevel)
                         .arg(ComponentHandler::kMaxDepth));
    }
    const std::size_t assemblies = DocumentConfig::assemblyCount(rocket) + addition.assemblies;
    if (addition.assemblies > 0 && assemblies > DocumentConfig::kMaxAssemblies)
    {
        //: QtRocket's own text (decision O27, extended by the M2 run 2 review): a paste,
        //: duplicate, drop or add of stages, booster sets or pod sets refused
        lines.append(tr("The rocket would have %1 stages, booster sets and pod sets. QtRocket "
                        "opens at most %2 of them in a design and would ignore the ones beyond, "
                        "with everything in them, when it opens this design again.")
                         .arg(assemblies)
                         .arg(DocumentConfig::kMaxAssemblies));
    }
    const std::size_t images = document.getDecalImageCount() + addition.decalImages;
    if (addition.decalImages > 0 && images > DocumentConfig::kMaxDecalImages)
    {
        //: QtRocket's own text (decision O27): a paste or drop of decal images refused
        lines.append(tr("The design would have %1 decal images. QtRocket opens at most %2 decal "
                        "images of a design and would ignore the decals of the ones beyond when "
                        "it opens this design again.")
                         .arg(images)
                         .arg(DocumentConfig::kMaxDecalImages));
    }
    if (lines.isEmpty())
    {
        return std::nullopt;
    }
    return UserPrompts::Message{
        .severity = UserPrompts::Severity::WARNING, .title = title, .lines = lines};
}

}  // namespace QtRocket::Gui
