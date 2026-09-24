#pragma once

#include <optional>

#include "QtRocket/rocket/Appearance.h"

namespace QtRocket
{

class InsideColorComponent;
class RocketComponent;

/// The inside-appearance state of an InsideColorComponent and its setters (OpenRocket's
/// InsideColorComponentHandler): the inside appearance, whether inside and outside are drawn
/// separately, and whether the edges take the inside or the outside appearance. The setters fire
/// their change events on the component that owns the handler.
///
/// A handler belongs to one InsideColorComponent (the mixin creates it bound to itself) and finds
/// the RocketComponent to notify from it when it fires; so a copied component's handler notifies
/// the copy, however the copy was made. Handlers are not copied or assigned: copyFrom() copies
/// the state.
///
/// Deviation: Java's setInsideAppearance() also subscribes to the decal image, to fire
/// TEXTURE_CHANGE when the image changes; images are named here and their bytes live in the
/// document's decal registry, which fires that event (see Decal). The multi-edit config
/// listeners are not ported, by decision.
class InsideColorComponentHandler
{
public:
    /// A handler of @p owner, with no inside appearance and both flags false.
    explicit InsideColorComponentHandler(InsideColorComponent& owner) noexcept;

    InsideColorComponentHandler(const InsideColorComponentHandler&)            = delete;
    InsideColorComponentHandler& operator=(const InsideColorComponentHandler&) = delete;
    InsideColorComponentHandler(InsideColorComponentHandler&&)                 = delete;
    InsideColorComponentHandler& operator=(InsideColorComponentHandler&&)      = delete;
    ~InsideColorComponentHandler()                                             = default;

    /// The realistic inside appearance, or nullopt for the material's default.
    [[nodiscard]] const std::optional<Appearance>& getInsideAppearance() const noexcept
    {
        return m_insideAppearance;
    }

    /// Sets the inside appearance (nullopt for the default) and fires NONFUNCTIONAL_CHANGE,
    /// whether or not it changed (as Java).
    void setInsideAppearance(std::optional<Appearance> appearance);

    /// True when the edges take the inside appearance, false for the outside one.
    [[nodiscard]] bool isEdgesSameAsInside() const noexcept { return m_edgesSameAsInside; }

    /// Sets the edge flag; fires GRAPHIC_CHANGE only when it changes.
    void setEdgesSameAsInside(bool newState);

    /// True when the inside and outside have separate appearances.
    [[nodiscard]] bool isSeparateInsideOutside() const noexcept { return m_separateInsideOutside; }

    /// Sets the separate-appearance flag; fires GRAPHIC_CHANGE only when it changes.
    void setSeparateInsideOutside(bool newState);

    /// Copies the inside appearance and both flags of @p source, firing nothing.
    void copyFrom(const InsideColorComponentHandler& source);

private:
    /// The component this handler belongs to (the owner, as a RocketComponent).
    [[nodiscard]] RocketComponent& component() const;

    InsideColorComponent*     m_owner;
    std::optional<Appearance> m_insideAppearance;
    bool                      m_separateInsideOutside{false};
    bool                      m_edgesSameAsInside{false};
};

}  // namespace QtRocket
