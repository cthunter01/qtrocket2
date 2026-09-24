#pragma once

#include <functional>
#include <optional>
#include <string>

#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket
{

/// Builds immutable Appearance objects one value at a time (OpenRocket's
/// appearance.AppearanceBuilder): set the values with the setters, then call getAppearance(),
/// which returns a new Appearance each time. The builder can be reused after resetToDefaults()
/// (done by setAppearance()).
///
/// Every setter emits changed() (Java: fireChangeEvent() of the AbstractChangeSource), except
/// inside batch(), which emits once at its end.
///
/// Deviations from OpenRocket:
/// - No AWT and no DecalImage: the image is its name (see Decal), and the paint is never null
///   (Java allows setPaint(null), which the loader never does), so getOpacity() never gives
///   Java's null-paint answer of 100.
/// - The multi-edit config listeners (addConfigListener() and friends, and the event bypass they
///   switch) are not ported, by decision: the GUI applies an edit to each selected component.
class AppearanceBuilder
{
public:
    /// A builder with the default values (see resetToDefaults()).
    AppearanceBuilder();

    /// A builder holding the values of @p appearance (Java: new AppearanceBuilder(Appearance)).
    explicit AppearanceBuilder(const std::optional<Appearance>& appearance);

    AppearanceBuilder(const AppearanceBuilder&)            = delete;
    AppearanceBuilder& operator=(const AppearanceBuilder&) = delete;
    AppearanceBuilder(AppearanceBuilder&&)                 = delete;
    AppearanceBuilder& operator=(AppearanceBuilder&&)      = delete;
    ~AppearanceBuilder()                                   = default;

    /// Resets to the defaults and copies @p appearance's paint, shine, decal and opacity flag,
    /// emitting changed() once (Java: setAppearance(), through batch()). nullopt only resets.
    void setAppearance(const std::optional<Appearance>& appearance);

    /// Copies the decal's offset, center, scale, rotation, edge mode and image; nullopt changes
    /// nothing (as in Java: the image is not cleared). Emits changed().
    void setDecal(const std::optional<Decal>& decal);

    /// A new Appearance of the current values, with a decal when an image is set.
    [[nodiscard]] Appearance getAppearance() const;

    [[nodiscard]] const Color& getPaint() const noexcept { return m_paint; }
    void                       setPaint(const Color& paint);

    [[nodiscard]] double getShine() const noexcept { return m_shine; }
    void                 setShine(double shine);

    /// The paint's alpha as a fraction, 0 (transparent) to 1 (opaque).
    [[nodiscard]] double getOpacity() const noexcept;
    /// Sets the paint's alpha to @p opacity (clamped to [0, 1]) times 255, truncated, through
    /// setPaint() with a new colour.
    void setOpacity(double opacity);

    [[nodiscard]] bool isOpacityAffectsTexture() const noexcept { return m_opacityAffectsTexture; }
    void               setOpacityAffectsTexture(bool opacityAffectsTexture);

    [[nodiscard]] double getOffsetU() const noexcept { return m_offsetU; }
    void                 setOffsetU(double offsetU);
    [[nodiscard]] double getOffsetV() const noexcept { return m_offsetV; }
    void                 setOffsetV(double offsetV);
    /// setOffsetU(u) then setOffsetV(v).
    void setOffset(double u, double v);

    [[nodiscard]] double getCenterU() const noexcept { return m_centerU; }
    void                 setCenterU(double centerU);
    [[nodiscard]] double getCenterV() const noexcept { return m_centerV; }
    void                 setCenterV(double centerV);
    /// setCenterU(u) then setCenterV(v).
    void setCenter(double u, double v);

    [[nodiscard]] double getScaleU() const noexcept { return m_scaleU; }
    void                 setScaleU(double scaleU);
    [[nodiscard]] double getScaleV() const noexcept { return m_scaleV; }
    void                 setScaleV(double scaleV);
    /// setScaleU(u) then setScaleV(v).
    void setScaleUV(double u, double v);

    /// 1 / getScaleU().
    [[nodiscard]] double getScaleX() const noexcept { return 1.0 / m_scaleU; }
    /// setScaleU(1 / scaleX).
    void setScaleX(double scaleX);
    /// 1 / getScaleV().
    [[nodiscard]] double getScaleY() const noexcept { return 1.0 / m_scaleV; }
    /// setScaleV(1 / scaleY).
    void setScaleY(double scaleY);

    /// The decal rotation, in radians.
    [[nodiscard]] double getRotation() const noexcept { return m_rotation; }
    void                 setRotation(double rotation);

    /// The decal image's name, or nullopt for no decal.
    [[nodiscard]] const std::optional<std::string>& getImage() const noexcept { return m_image; }
    void                                            setImage(std::optional<std::string> image);

    [[nodiscard]] Decal::EdgeMode getEdgeMode() const noexcept { return m_edgeMode; }
    void                          setEdgeMode(Decal::EdgeMode edgeMode);

    /// Runs @p changes with the change notifications held back, then emits changed() once.
    void batch(const std::function<void()>& changes);

    /// Emitted when a value changes (see the class comment).
    [[nodiscard]] Signal<>& changed() noexcept { return m_changed; }

private:
    /// Paint (187, 187, 187), shine 0.3, offsets and centers 0, scales 1, rotation 0, no image,
    /// REPEAT, opacity not affecting the texture; emits changed().
    void resetToDefaults();

    /// Emits changed() unless inside batch().
    void fireChangeEvent();

    Color                      m_paint{187, 187, 187};
    double                     m_shine{0.3};
    double                     m_offsetU{0.0};
    double                     m_offsetV{0.0};
    double                     m_centerU{0.0};
    double                     m_centerV{0.0};
    double                     m_scaleU{1.0};
    double                     m_scaleV{1.0};
    double                     m_rotation{0.0};
    std::optional<std::string> m_image;
    Decal::EdgeMode            m_edgeMode{Decal::EdgeMode::REPEAT};
    bool                       m_opacityAffectsTexture{false};
    bool                       m_batch{false};
    Signal<>                   m_changed;
};

}  // namespace QtRocket
