#include "QtRocket/document/ComponentCopy.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket
{

namespace
{

/// Names of decal images, each with what goes with it: the name of the first component that
/// shows the image, or the name the image has in the target.
using DecalNames = std::map<std::string, std::string, std::less<>>;

/// The inside appearance of @p component when it has an inside (an InsideColorComponent), else
/// null.
[[nodiscard]] const std::optional<Appearance>* insideAppearanceOf(const RocketComponent& component)
{
    const auto* const inside = dynamic_cast<const InsideColorComponent*>(&component);
    return inside == nullptr ? nullptr
                             : &inside->getInsideColorComponentHandler().getInsideAppearance();
}

/// Adds the name of the image of @p appearance's decal, when it has one, with @p component's
/// name, unless @p names has it already.
void addDecalName(DecalNames& names, const std::optional<Appearance>& appearance,
                  const RocketComponent& component)
{
    if (appearance.has_value() && appearance->getTexture().has_value())
    {
        names.try_emplace(appearance->getTexture()->getImageName(), component.getName());
    }
}

/// Adds the names of the images the decals of @p tree show, outside and inside, each with the
/// first component of the tree that shows it.
void addDecalNames(DecalNames& names, const RocketComponent& tree)
{
    for (const RocketComponent& component : tree.subtree())
    {
        addDecalName(names, component.getAppearance(), component);
        if (const std::optional<Appearance>* const inside = insideAppearanceOf(component))
        {
            addDecalName(names, *inside, component);
        }
    }
}

/// @p appearance with the image of its decal renamed as @p renames says, or nullopt when it has
/// no decal or the decal's image keeps its name. Every other field stays as it is.
[[nodiscard]] std::optional<Appearance> renamed(const std::optional<Appearance>& appearance,
                                                const DecalNames&                renames)
{
    if (!appearance.has_value() || !appearance->getTexture().has_value())
    {
        return std::nullopt;
    }
    const Decal& decal = *appearance->getTexture();
    const auto   name  = renames.find(decal.getImageName());
    if (name == renames.end())
    {
        return std::nullopt;
    }
    return Appearance(appearance->getPaint(), appearance->getShine(),
                      Decal(decal.getOffset(), decal.getCenter(), decal.getScale(),
                            decal.getRotation(), name->second, decal.getEdgeMode()),
                      appearance->isOpacityAffectsTexture());
}

/// Renames the images of the decals of @p tree as @p renames says. The tree is detached, so the
/// setters fire nothing.
void renameDecals(RocketComponent& tree, const DecalNames& renames)
{
    for (RocketComponent& component : tree.subtree())
    {
        if (std::optional<Appearance> outside = renamed(component.getAppearance(), renames))
        {
            component.setAppearance(std::move(outside));
        }
        auto* const inside = dynamic_cast<InsideColorComponent*>(&component);
        if (inside == nullptr)
        {
            continue;
        }
        InsideColorComponentHandler& handler = inside->getInsideColorComponentHandler();
        if (std::optional<Appearance> insideAppearance =
                renamed(handler.getInsideAppearance(), renames))
        {
            handler.setInsideAppearance(std::move(insideAppearance));
        }
    }
}

/// Whether @p component is a component of @p rocket's tree.
[[nodiscard]] bool isComponentOf(const RocketComponent& component, const Rocket& rocket)
{
    return &component.getRoot() == static_cast<const RocketComponent*>(&rocket);
}

}  // namespace

ComponentCopy::ComponentCopy(std::vector<std::unique_ptr<RocketComponent>>  components,
                             std::vector<std::shared_ptr<const DecalImage>> decalImages) noexcept
  : m_components(std::move(components)), m_decalImages(std::move(decalImages))
{
}

Result<ComponentCopy> ComponentCopy::copyOf(const OpenRocketDocument&                  source,
                                            const std::vector<const RocketComponent*>& components)
{
    std::vector<std::unique_ptr<RocketComponent>> copies;
    copies.reserve(components.size());
    for (const RocketComponent* const component : components)
    {
        QTROCKET_ASSERT(component != nullptr);
        if (!isComponentOf(*component, source.getRocket()))
        {
            bug(
                std::format("ComponentCopy::copyOf(): the component '{}' is not a component of "
                            "the source document's rocket",
                            component->getName()));
        }
        copies.push_back(component->copyWithNewIds());
    }
    return fromDetached(source, std::move(copies));
}

Result<ComponentCopy> ComponentCopy::fromDetached(
    const OpenRocketDocument& source, std::vector<std::unique_ptr<RocketComponent>> components)
{
    DecalNames users;
    for (const std::unique_ptr<RocketComponent>& tree : components)
    {
        QTROCKET_ASSERT(tree != nullptr);
        if (tree->getParent() != nullptr)
        {
            bug(std::format("ComponentCopy::fromDetached(): the component '{}' has a parent",
                            tree->getName()));
        }
        addDecalNames(users, *tree);
    }

    std::vector<std::shared_ptr<const DecalImage>> images;
    images.reserve(users.size());
    for (const auto& [name, user] : users)
    {
        const std::shared_ptr<DecalImage> image = source.findDecalImage(name);
        if (image == nullptr)
        {
            return fail(ErrorCode::NOT_FOUND,
                        std::format("The component '{}' shows the decal image '{}', which its "
                                    "document does not have",
                                    user, name));
        }
        images.push_back(image->copyWithName(image->getName()));
    }
    // The map is in byte order, which is String.compareTo's but for the characters beyond
    // U+FFFF.
    std::ranges::stable_sort(
        images, [](const std::shared_ptr<const DecalImage>& a,
                   const std::shared_ptr<const DecalImage>& b) { return a->compareTo(*b) < 0; });
    return ComponentCopy(std::move(components), std::move(images));
}

const RocketComponent& ComponentCopy::getComponent(std::size_t index) const
{
    if (index >= m_components.size())
    {
        bug(std::format("ComponentCopy::getComponent(): index {} out of range, the copy has {}",
                        index, m_components.size()));
    }
    return *m_components[index];
}

std::vector<const RocketComponent*> ComponentCopy::getComponents() const
{
    std::vector<const RocketComponent*> trees;
    trees.reserve(m_components.size());
    for (const std::unique_ptr<RocketComponent>& tree : m_components)
    {
        trees.push_back(tree.get());
    }
    return trees;
}

std::shared_ptr<const DecalImage> ComponentCopy::findDecalImage(std::string_view name) const
{
    const auto found =
        std::ranges::find_if(m_decalImages, [name](const std::shared_ptr<const DecalImage>& image) {
            return image->getName() == name;
        });
    return found == m_decalImages.end() ? nullptr : *found;
}

ComponentCopy ComponentCopy::copy() const
{
    std::vector<std::unique_ptr<RocketComponent>> copies;
    copies.reserve(m_components.size());
    for (const std::unique_ptr<RocketComponent>& tree : m_components)
    {
        // Null only in a copy whose insertInto() a slot ended half-way.
        QTROCKET_ASSERT(tree != nullptr);
        copies.push_back(tree->copyWithNewIds());
    }
    return {std::move(copies), m_decalImages};
}

void ComponentCopy::checkPlacements(const OpenRocketDocument&     target,
                                    const std::vector<Placement>& placements) const
{
    const Rocket&                                 rocket = target.getRocket();
    std::vector<bool>                             placed(m_components.size(), false);
    std::map<const RocketComponent*, std::size_t> earlier;
    for (const Placement& placement : placements)
    {
        if (placement.component >= m_components.size() ||
            m_components[placement.component] == nullptr)
        {
            bug(std::format("ComponentCopy::insertInto(): no component {} to place",
                            placement.component));
        }
        if (placed[placement.component])
        {
            bug(std::format("ComponentCopy::insertInto(): the component {} is placed twice",
                            placement.component));
        }
        placed[placement.component]      = true;
        const RocketComponent& component = *m_components[placement.component];
        if (placement.parent == nullptr || !isComponentOf(*placement.parent, rocket))
        {
            bug(
                std::format("ComponentCopy::insertInto(): the parent of '{}' is not a component "
                            "of the target's rocket",
                            component.getName()));
        }
        if (!placement.parent->isCompatible(component))
        {
            bug(std::format("ComponentCopy::insertInto(): '{}' cannot be a child of '{}'",
                            component.getName(), placement.parent->getName()));
        }
        std::size_t& before = earlier[placement.parent];
        if (placement.index > placement.parent->getChildCount() + before)
        {
            bug(
                std::format("ComponentCopy::insertInto(): index {} for '{}' out of range, '{}' "
                            "will have {} children",
                            placement.index, component.getName(), placement.parent->getName(),
                            placement.parent->getChildCount() + before));
        }
        ++before;
        for (const RocketComponent& part : component.subtree())
        {
            if (rocket.findComponent(part.getId()) != nullptr)
            {
                bug(
                    std::format("ComponentCopy::insertInto(): the target's rocket has a "
                                "component of the id of '{}' already; copy() first",
                                part.getName()));
            }
        }
    }
}

std::vector<RocketComponent*> ComponentCopy::insertInto(OpenRocketDocument&           target,
                                                        const std::vector<Placement>& placements) &&
{
    checkPlacements(target, placements);

    // The parents by id, for the insertions after the first (see the class comment).
    std::vector<Uuid> parents;
    parents.reserve(placements.size());
    DecalNames users;
    for (const Placement& placement : placements)
    {
        parents.push_back(placement.parent->getId());
        addDecalNames(users, *m_components[placement.component]);
    }

    // The images, then the decals that name them by another name in the target.
    DecalNames renames;
    for (const auto& entry : users)
    {
        const std::string&                      name  = entry.first;
        const std::shared_ptr<const DecalImage> image = findDecalImage(name);
        QTROCKET_ASSERT(image != nullptr);
        const std::shared_ptr<DecalImage> imported = target.importDecalImage(*image);
        if (imported->getName() != name)
        {
            renames.emplace(name, imported->getName());
        }
    }
    if (!renames.empty())
    {
        for (const Placement& placement : placements)
        {
            renameDecals(*m_components[placement.component], renames);
        }
    }

    std::vector<RocketComponent*> inserted;
    inserted.reserve(placements.size());
    for (std::size_t i = 0; i < placements.size(); ++i)
    {
        const Placement& placement = placements[i];
        RocketComponent* parent    = target.getRocket().findComponent(parents[i]);
        if (parent == nullptr || placement.index > parent->getChildCount())
        {
            bug("ComponentCopy::insertInto(): a slot changed the component tree while the "
                "components went in");
        }
        inserted.push_back(
            &parent->addChild(std::move(m_components[placement.component]), placement.index));
    }
    // The components without a placement go with the copy.
    m_components.clear();
    return inserted;
}

}  // namespace QtRocket
