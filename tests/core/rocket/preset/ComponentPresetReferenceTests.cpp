#include "QtRocket/rocket/preset/ComponentPresetReference.h"

#include <optional>
#include <string>
#include <type_traits>

#include <gtest/gtest.h>

// What a design file says about the preset of a component, as texts. QtRocket's own: OpenRocket
// has no such class (see the header).

namespace
{

using QtRocket::ComponentPresetReference;

/// The <preset> element of the body tube of the example "3D printable nose cone and fins".
[[nodiscard]] ComponentPresetReference tube()
{
    return {.type         = "BODY_TUBE",
            .manufacturer = "Estes",
            .partNo       = "BT-50FE, 30359",
            .digest       = "e9c3e97d32e96762f65e051d74cb9fd7"};
}

TEST(ComponentPresetReference, HoldsTheFourAttributesOfAPresetElement)
{
    const ComponentPresetReference reference = tube();
    EXPECT_EQ(reference.type, "BODY_TUBE");
    EXPECT_EQ(reference.manufacturer, "Estes");
    EXPECT_EQ(reference.partNo, "BT-50FE, 30359");
    EXPECT_EQ(reference.digest, "e9c3e97d32e96762f65e051d74cb9fd7");
}

// An element need not have a type or a digest.
TEST(ComponentPresetReference, StartsWithoutATypeAndADigest)
{
    const ComponentPresetReference reference;
    EXPECT_EQ(reference.type, std::nullopt);
    EXPECT_EQ(reference.manufacturer, "");
    EXPECT_EQ(reference.partNo, "");
    EXPECT_EQ(reference.digest, std::nullopt);
}

TEST(ComponentPresetReference, TwoAreEqualWhenTheirFourTextsAre)
{
    EXPECT_EQ(tube(), tube());

    ComponentPresetReference other = tube();
    other.type                     = "NOSE_CONE";
    EXPECT_NE(other, tube());
    other              = tube();
    other.manufacturer = "estes";
    EXPECT_NE(other, tube()) << "the texts are compared exactly";
    other        = tube();
    other.partNo = "BT-50FE, 30359 ";
    EXPECT_NE(other, tube());
    other        = tube();
    other.digest = "E9C3E97D32E96762F65E051D74CB9FD7";
    EXPECT_NE(other, tube());
}

// An attribute that is missing is not an empty one: a save writes back what the file had.
TEST(ComponentPresetReference, AMissingAttributeIsNotAnEmptyOne)
{
    ComponentPresetReference without = tube();
    without.type                     = std::nullopt;
    ComponentPresetReference empty   = tube();
    empty.type                       = "";
    EXPECT_NE(without, empty);

    without        = tube();
    without.digest = std::nullopt;
    empty          = tube();
    empty.digest   = "";
    EXPECT_NE(without, empty);
}

TEST(ComponentPresetReference, IsAValue)
{
    static_assert(std::is_copy_constructible_v<ComponentPresetReference>);
    static_assert(std::is_copy_assignable_v<ComponentPresetReference>);
    static_assert(std::is_move_constructible_v<ComponentPresetReference>);
    static_assert(std::is_default_constructible_v<ComponentPresetReference>);

    const ComponentPresetReference original = tube();
    ComponentPresetReference       copy     = original;
    copy.partNo                             = "another";
    EXPECT_EQ(original, tube());
    EXPECT_NE(copy, original);
}

}  // namespace
