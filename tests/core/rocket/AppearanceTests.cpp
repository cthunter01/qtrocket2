#include "QtRocket/rocket/Appearance.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::AppearanceBuilder;
using QtRocket::Color;
using QtRocket::Coordinate;
using QtRocket::Decal;
using EdgeMode = Decal::EdgeMode;

Decal sampleDecal()
{
    return Decal{Coordinate{0.1, 0.2}, Coordinate{0.5, 0.5}, Coordinate{2.0, 3.0}, 0.25,
                 "decals/logo.png",    EdgeMode::MIRROR};
}

// ---- Appearance ----

TEST(Appearance, ClampsTheShine)
{
    EXPECT_EQ(Appearance(Color{1, 2, 3}, 1.5).getShine(), 1.0);
    EXPECT_EQ(Appearance(Color{1, 2, 3}, -0.5).getShine(), 0.0);
    EXPECT_EQ(Appearance(Color{1, 2, 3}, 0.3).getShine(), 0.3);
    EXPECT_TRUE(std::isnan(
        Appearance(Color{1, 2, 3}, std::numeric_limits<double>::quiet_NaN()).getShine()));
}

TEST(Appearance, KeepsItsValues)
{
    const Appearance appearance{Color{10, 20, 30, 40}, 0.5, sampleDecal(), true};
    EXPECT_EQ(appearance.getPaint(), (Color{10, 20, 30, 40}));
    EXPECT_EQ(appearance.getShine(), 0.5);
    ASSERT_TRUE(appearance.getTexture().has_value());
    EXPECT_EQ(appearance.getTexture().transform([](const Decal& d) { return d.getImageName(); }),
              std::optional<std::string>{"decals/logo.png"});
    EXPECT_TRUE(appearance.isOpacityAffectsTexture());

    const Appearance plain{Color{1, 2, 3}, 0.2};
    EXPECT_FALSE(plain.getTexture().has_value());
    EXPECT_FALSE(plain.isOpacityAffectsTexture());
}

TEST(Appearance, MissingIsBlackAndShiny)
{
    EXPECT_EQ(Appearance::missing().getPaint(), Color::black());
    EXPECT_EQ(Appearance::missing().getShine(), 1.0);
    EXPECT_FALSE(Appearance::missing().getTexture().has_value());
    EXPECT_EQ(&Appearance::missing(), &Appearance::missing());
}

TEST(Appearance, ToStringAsJava)
{
    EXPECT_EQ(Appearance(Color{187, 187, 187}, 0.3).toString(),
              "Appearance [paint=Color [r=187, g=187, b=187, a=255], shine=0.3, texture=null, "
              "opacityAffectsTexture=false]");
}

TEST(Appearance, Equality)
{
    EXPECT_EQ(Appearance(Color{1, 2, 3}, 0.3, sampleDecal()),
              Appearance(Color{1, 2, 3}, 0.3, sampleDecal()));
    EXPECT_NE(Appearance(Color{1, 2, 3}, 0.3), Appearance(Color{1, 2, 3}, 0.4));
    EXPECT_NE(Appearance(Color{1, 2, 3}, 0.3), Appearance(Color{1, 2, 3}, 0.3, sampleDecal()));
}

// ---- Decal ----

TEST(Decal, KeepsItsValues)
{
    const Decal decal = sampleDecal();
    EXPECT_TRUE(decal.getOffset().exactlyEquals(Coordinate{0.1, 0.2}));
    EXPECT_TRUE(decal.getCenter().exactlyEquals(Coordinate{0.5, 0.5}));
    EXPECT_TRUE(decal.getScale().exactlyEquals(Coordinate{2.0, 3.0}));
    EXPECT_EQ(decal.getRotation(), 0.25);
    EXPECT_EQ(decal.getEdgeMode(), EdgeMode::MIRROR);
    EXPECT_EQ(decal.getImageName(), "decals/logo.png");
}

TEST(Decal, ToString)
{
    EXPECT_EQ(sampleDecal().toString(),
              "Texture [offset=(0.10000,0.20000,0.00000), center=(0.50000,0.50000,0.00000), "
              "scale=(2.00000,3.00000,0.00000), rotation=0.25, image=decals/logo.png]");
}

TEST(Decal, EdgeModeNames)
{
    EXPECT_EQ(QtRocket::edgeModeName(EdgeMode::REPEAT), "REPEAT");
    EXPECT_EQ(QtRocket::edgeModeName(EdgeMode::STICKER), "STICKER");
    EXPECT_TRUE(std::ranges::all_of(Decal::kAllEdgeModes, [](EdgeMode mode) {
        return QtRocket::edgeModeFromName(QtRocket::edgeModeName(mode)) == mode;
    }));
    // EdgeMode.valueOf is exact.
    EXPECT_EQ(QtRocket::edgeModeFromName("repeat"), std::nullopt);
}

TEST(Decal, EdgeModeDisplayNames)
{
    EXPECT_EQ(QtRocket::displayKey(EdgeMode::MIRROR), "TextureWrap.Mirror");
    EXPECT_EQ(QtRocket::displayName(EdgeMode::MIRROR), "Repeat & Mirror");
    EXPECT_EQ(QtRocket::displayName(EdgeMode::CLAMP), "Clamp Edge Pixels");
}

// ---- AppearanceBuilder ----

TEST(AppearanceBuilder, DefaultsAsJava)
{
    const AppearanceBuilder builder;
    EXPECT_EQ(builder.getPaint(), (Color{187, 187, 187}));
    EXPECT_EQ(builder.getShine(), 0.3);
    EXPECT_EQ(builder.getOffsetU(), 0.0);
    EXPECT_EQ(builder.getOffsetV(), 0.0);
    EXPECT_EQ(builder.getCenterU(), 0.0);
    EXPECT_EQ(builder.getCenterV(), 0.0);
    EXPECT_EQ(builder.getScaleU(), 1.0);
    EXPECT_EQ(builder.getScaleV(), 1.0);
    EXPECT_EQ(builder.getRotation(), 0.0);
    EXPECT_FALSE(builder.getImage().has_value());
    EXPECT_EQ(builder.getEdgeMode(), EdgeMode::REPEAT);
    EXPECT_FALSE(builder.isOpacityAffectsTexture());

    const Appearance built = builder.getAppearance();
    EXPECT_EQ(built, Appearance(Color{187, 187, 187}, 0.3));
}

TEST(AppearanceBuilder, RoundTripsAnAppearance)
{
    const Appearance        original{Color{1, 2, 3, 4}, 0.7, sampleDecal(), true};
    const AppearanceBuilder builder{original};
    EXPECT_EQ(builder.getAppearance(), original);
    EXPECT_EQ(builder.getOffsetU(), 0.1);
    EXPECT_EQ(builder.getScaleV(), 3.0);
    EXPECT_EQ(builder.getImage(), std::optional<std::string>{"decals/logo.png"});

    // Without a texture, no decal is made.
    const AppearanceBuilder plain{Appearance{Color{1, 2, 3}, 0.1}};
    EXPECT_FALSE(plain.getAppearance().getTexture().has_value());
    // nullopt gives the defaults.
    const AppearanceBuilder none{std::optional<Appearance>{}};
    EXPECT_EQ(none.getAppearance(), Appearance(Color{187, 187, 187}, 0.3));
}

TEST(AppearanceBuilder, SetDecalOfNothingKeepsTheImage)
{
    AppearanceBuilder builder{Appearance{Color{1, 2, 3}, 0.1, sampleDecal()}};
    builder.setDecal(std::nullopt);
    EXPECT_TRUE(builder.getImage().has_value());
    builder.setImage(std::nullopt);
    EXPECT_FALSE(builder.getAppearance().getTexture().has_value());
}

TEST(AppearanceBuilder, EverySetterNotifies)
{
    AppearanceBuilder builder;
    int               changes = 0;
    const auto        connection =
        QtRocket::Signal<>::ScopedConnection{builder.changed().connect([&changes] { ++changes; })};
    builder.setPaint(Color{1, 2, 3});
    builder.setShine(0.5);
    builder.setOffsetU(1);
    builder.setOffsetV(1);
    builder.setCenterU(1);
    builder.setCenterV(1);
    builder.setScaleU(2);
    builder.setScaleV(2);
    builder.setRotation(1);
    builder.setImage("x.png");
    builder.setEdgeMode(EdgeMode::STICKER);
    builder.setOpacityAffectsTexture(true);
    EXPECT_EQ(changes, 12);
    builder.setOffset(1, 2);  // two setters
    EXPECT_EQ(changes, 14);
}

TEST(AppearanceBuilder, BatchNotifiesOnce)
{
    AppearanceBuilder builder;
    int               changes = 0;
    const auto        connection =
        QtRocket::Signal<>::ScopedConnection{builder.changed().connect([&changes] { ++changes; })};
    builder.batch([&builder] {
        builder.setShine(0.9);
        builder.setRotation(2);
    });
    EXPECT_EQ(changes, 1);
    builder.setAppearance(Appearance{Color{4, 5, 6}, 0.2, sampleDecal()});
    EXPECT_EQ(changes, 2);
    EXPECT_EQ(builder.getShine(), 0.2);
}

TEST(AppearanceBuilder, Opacity)
{
    AppearanceBuilder builder;
    EXPECT_EQ(builder.getOpacity(), 1.0);
    builder.setOpacity(0.5);
    EXPECT_EQ(builder.getPaint().alpha(), 127);  // (int) (0.5 * 255)
    EXPECT_EQ(builder.getPaint().red(), 187);
    EXPECT_DOUBLE_EQ(builder.getOpacity(), 127.0 / 255);
    builder.setOpacity(2.0);
    EXPECT_EQ(builder.getPaint().alpha(), 255);
    builder.setOpacity(-1.0);
    EXPECT_EQ(builder.getPaint().alpha(), 0);
    // Java's clamp keeps a NaN, and (int) NaN is 0.
    builder.setOpacity(1.0);
    builder.setOpacity(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(builder.getPaint().alpha(), 0);
}

TEST(AppearanceBuilder, ScaleXAndYAreReciprocals)
{
    AppearanceBuilder builder;
    builder.setScaleX(4.0);
    builder.setScaleY(0.5);
    EXPECT_EQ(builder.getScaleU(), 0.25);
    EXPECT_EQ(builder.getScaleV(), 2.0);
    EXPECT_EQ(builder.getScaleX(), 4.0);
    EXPECT_EQ(builder.getScaleY(), 0.5);
}

}  // namespace
