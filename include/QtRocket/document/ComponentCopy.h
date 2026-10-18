#pragma once

#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class DecalImage;
class OpenRocketDocument;

/// Components taken out of a document as detached copies, together with the decal images they
/// show: what the application's clipboard keeps after Copy, Cut and Duplicate, and what a drop
/// from another window carries into its target (OpenRocket: the list of copies
/// OpenRocketClipboard keeps, and the copies ComponentTreeTransferHandler adds). It is Qt-free;
/// its callers are the GUI's OpenRocketClipboard, ComponentEditing and ComponentDrop. A
/// rocket/Decal names its image and the image lives in the decal registry of its document, so a
/// component that goes into another document needs its images to go along; without them the
/// other document cannot be saved ("Could not find decal source file ...").
///
/// Taking a copy (copyOf(), fromDetached()): the copy owns the component trees, each detached
/// (no parent: it fires no events and belongs to no rocket), and gives them out as constants
/// only. With them it takes the decal images they show: for every name a decal of the trees
/// has (the appearance of each component, and the inside appearance of an
/// InsideColorComponent), the source document's image of that name
/// (OpenRocketDocument::findDecalImage(), so also an image that removeDecal() took out of the
/// list and an undo gave back to a component), copied under its name
/// (DecalImage::copyWithName()). The copy shares the attachment, and with it the bytes (the
/// bytes of an archive stay with the attachments read from it), and the decal file, but not the
/// image object: it neither follows a later change of the source's image (its decal file, its
/// flags) nor needs the source document, which may be closed before the copy is pasted. A decal
/// whose image the source does not have is refused (ErrorCode::NOT_FOUND): no part of the core
/// makes one, and the saver refuses such a document with decalNotFound() of the name.
///
/// Putting it into a document (insertInto(), for paste and drop):
/// 1. Every placement is checked before anything changes: the component's index is in range
///    and placed once; the parent is a component of the target's rocket that is compatible with
///    the component (RocketComponent::isCompatible()); the index is at most the parent's child
///    count plus the number of components that earlier placements of the call put into the same
///    parent (the indices are those of RocketComponent::addChild(component, index) called in
///    the order of the placements, as Java's paste computes its positions first and then adds
///    the copies one by one); and no component of the placed trees has an id that a component
///    of the target's rocket has (make the copy with new ids, copy(), before it goes into the
///    document it came from). A failed check is a BugError, and the target is as it was, its
///    decal images included.
/// 2. The images the placed components show are put into the target, in the order of their
///    names, with OpenRocketDocument::importDecalImage() (see DecalRegistry, "Images of another
///    document"): the target's image when it has the same one (the same source, or the same
///    name and the same bytes), else a copy registered under the name or, when the name is
///    taken, under DecalRegistry::makeUniqueName() of it. The decals of the placed components
///    are renamed where the target's image has another name (the decal is the same in every
///    other field). Nothing here can fail: an image whose bytes cannot be read is simply not
///    the same as another, and is copied.
/// 3. The components go in, in the order of the placements, each with addChild(component,
///    index): one TREE event each, as Java's paste and drop add theirs (freeze the rocket around
///    the call for one event: Java does not). The images are in the target before the first
///    event, so every slot finds the images of the components it is told of. Before each
///    insertion the parent is looked up again by its id: a slot of an earlier insertion's event
///    that removed it, or changed its children so that the index no longer fits, ends the call
///    with a BugError (what went in stays in).
/// 4. The inserted components are returned in the order of the placements; a component of the
///    copy without a placement is destroyed with it.
///
/// Undo: call OpenRocketDocument::addUndoPosition() (or startUndo()) before insertInto(), as
/// Java does before a paste or a drop. The images the call registers then belong to that step:
/// its undo takes them out of the decal list again and its redo puts them back
/// (OpenRocketDocument, "Imported decal images").
///
/// Inside one document (Duplicate, a paste back into the document copied from, a copying drop
/// inside one window): the copy finds the document's own images (the same source), so nothing
/// is registered and no decal is renamed. Only an image that was removed from the list since
/// is registered again, and an image whose decal file was changed since (an external edit) is
/// another image now, compared by its bytes.
///
/// Deviations from OpenRocket:
/// - The images go into the target's registry, with unique names where the target has another
///   image of the name, and the decals follow their images. Java's pasted components keep the
///   source's image objects, which the target never registers: its decal list does not show
///   them, and an image of the target with the same name is saved in their place (DecalRegistry,
///   deviations; probe DecalTransferProbe of Milestone 2).
/// - The images are copies taken when the copy is taken (see above). Java's clipboard holds the
///   source's image objects and sees a later change of their decal files.
/// - Every placement is checked before anything changes. Java's paste adds what it can, one by
///   one, and an addChild() that fails half-way leaves the earlier ones in.
/// - Not copyable (copy() makes another copy with new ids); movable.
class ComponentCopy
{
public:
    /// Where insertInto() puts one component of the copy.
    struct Placement
    {
        /// The index of the component in the copy (getComponent()).
        std::size_t component{0};
        /// The new parent: a component of the target document's rocket. Not owned.
        RocketComponent* parent{nullptr};
        /// The position among the parent's children at the moment the component goes in, 0 to
        /// the child count then (RocketComponent::addChild(component, index)).
        std::size_t index{0};
    };

    /// A copy of nothing: no component and no image (the clipboard before anything was
    /// copied).
    ComponentCopy()  = default;
    ~ComponentCopy() = default;

    ComponentCopy(const ComponentCopy&)                = delete;
    ComponentCopy& operator=(const ComponentCopy&)     = delete;
    ComponentCopy(ComponentCopy&&) noexcept            = default;
    ComponentCopy& operator=(ComponentCopy&&) noexcept = default;

    /// A copy of the subtrees of @p components, components of @p source's rocket, in that order,
    /// each made with RocketComponent::copyWithNewIds(), with the decal images they show (see
    /// the class comment). Each subtree is copied whole: pass the top components only (a
    /// component and one of its descendants give two copies of the descendant), or make the
    /// trees yourself (fromDetached()).
    /// @throws BugError when a component is null or not a component of @p source's rocket
    [[nodiscard]] static Result<ComponentCopy> copyOf(
        const OpenRocketDocument& source, const std::vector<const RocketComponent*>& components);

    /// A copy of the trees @p components, taken over as they are, with the decal images they
    /// show, which come from @p source (see the class comment): for trees the caller made from
    /// components of @p source, such as the copies OpenRocket's clipboard keeps, in which a
    /// component whose children were partly selected keeps only those (Java:
    /// RocketActions.copyComponentsMaintainParent() and the clipboard's filter). Their ids are
    /// the caller's.
    /// @throws BugError when a tree is null or has a parent
    [[nodiscard]] static Result<ComponentCopy> fromDetached(
        const OpenRocketDocument& source, std::vector<std::unique_ptr<RocketComponent>> components);

    /// The number of component trees.
    [[nodiscard]] std::size_t size() const noexcept { return m_components.size(); }
    /// Whether there is no component.
    [[nodiscard]] bool empty() const noexcept { return m_components.empty(); }

    /// The tree at @p index, below size().
    /// @throws BugError when @p index is out of range
    [[nodiscard]] const RocketComponent& getComponent(std::size_t index) const;

    /// The trees, in order.
    [[nodiscard]] std::vector<const RocketComponent*> getComponents() const;

    /// The decal images the trees show, one per name, sorted by name (DecalImage::compareTo()).
    [[nodiscard]] const std::vector<std::shared_ptr<const DecalImage>>& getDecalImages()
        const noexcept
    {
        return m_decalImages;
    }

    /// The image named exactly @p name, or null.
    [[nodiscard]] std::shared_ptr<const DecalImage> findDecalImage(std::string_view name) const;

    /// Another copy of the same: every tree copied again with RocketComponent::copyWithNewIds(),
    /// the images shared, which no ComponentCopy changes. What a paste inserts, while the
    /// clipboard keeps its own (Java: component.copy() of each copy on the clipboard).
    [[nodiscard]] ComponentCopy copy() const;

    /// Puts the components into @p target, each where its placement says, with the decal
    /// images they show (see the class comment for the checks, the images, the order and the
    /// events). Returns the inserted components in the order of @p placements; they belong to
    /// the target's rocket, and like every component they do not outlive its next TREE or UNDO
    /// event in the hands of a caller that keeps pointers. Afterwards this copy holds the
    /// images but none of the components.
    /// @throws BugError when a placement fails a check (nothing has changed then), or when a
    ///         slot of an insertion's event removed a later placement's parent or changed its
    ///         children so that the index no longer fits
    std::vector<RocketComponent*> insertInto(OpenRocketDocument&           target,
                                             const std::vector<Placement>& placements) &&;

private:
    ComponentCopy(std::vector<std::unique_ptr<RocketComponent>>  components,
                  std::vector<std::shared_ptr<const DecalImage>> decalImages) noexcept;

    /// Throws a BugError when a placement fails a check of insertInto().
    void checkPlacements(const OpenRocketDocument&     target,
                         const std::vector<Placement>& placements) const;

    std::vector<std::unique_ptr<RocketComponent>>  m_components;
    std::vector<std::shared_ptr<const DecalImage>> m_decalImages;
};

}  // namespace QtRocket
