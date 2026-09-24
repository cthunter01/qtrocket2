#pragma once

#include <optional>
#include <string>

#include "QtRocket/rocket/Decal.h"
#include "QtRocket/util/Color.h"

namespace QtRocket
{

/// The realistic (3D) appearance of a component: paint colour, shine and an optional decal
/// (OpenRocket's appearance.Appearance). Immutable; AppearanceBuilder makes new ones.
///
/// A component with no appearance of its own holds std::nullopt (Java: null, "use the default
/// for the material").
class Appearance
{
public:
    /// The appearance used when one is missing: black, shine 1, no texture (Java: MISSING).
    [[nodiscard]] static const Appearance& missing();

    /// @param paint                 the paint colour
    /// @param shine                 the shine, clamped to [0, 1] (a NaN stays NaN)
    /// @param texture               the decal, if any
    /// @param opacityAffectsTexture whether the paint's opacity also applies to the decal's
    ///                              pixels
    Appearance(const Color& paint, double shine, std::optional<Decal> texture = std::nullopt,
               bool opacityAffectsTexture = false);

    [[nodiscard]] const Color&                getPaint() const noexcept { return m_paint; }
    [[nodiscard]] double                      getShine() const noexcept { return m_shine; }
    [[nodiscard]] const std::optional<Decal>& getTexture() const noexcept { return m_texture; }
    [[nodiscard]] bool isOpacityAffectsTexture() const noexcept { return m_opacityAffectsTexture; }

    /// "Appearance [paint=Color [...], shine=0.3, texture=null, opacityAffectsTexture=false]",
    /// the texture as Decal::toString() and the shine as Java prints a double.
    [[nodiscard]] std::string toString() const;

    /// Field-by-field equality (Java has none: Appearance compares by identity). An addition for
    /// tests and undo comparisons.
    [[nodiscard]] bool operator==(const Appearance& other) const noexcept;

private:
    Color                m_paint;
    double               m_shine;
    std::optional<Decal> m_texture;
    bool                 m_opacityAffectsTexture;
};

}  // namespace QtRocket
