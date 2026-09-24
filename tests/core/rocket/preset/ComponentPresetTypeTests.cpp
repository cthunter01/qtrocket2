#include "QtRocket/rocket/preset/ComponentPresetType.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "QtRocket/rocket/ComponentKind.h"

namespace
{

using QtRocket::ComponentKind;
using QtRocket::ComponentPresetType;

/// A preset type, its Java name and the component it is made for.
struct TypeExpectation
{
    ComponentPresetType type;
    std::string_view    name;
    ComponentKind       kind;
};

// In Java's declaration order.
constexpr std::array<TypeExpectation, 11> kTypes{{
    {.type = ComponentPresetType::BODY_TUBE, .name = "BODY_TUBE", .kind = ComponentKind::BODY_TUBE},
    {.type = ComponentPresetType::NOSE_CONE, .name = "NOSE_CONE", .kind = ComponentKind::NOSE_CONE},
    {.type = ComponentPresetType::TRANSITION,
     .name = "TRANSITION",
     .kind = ComponentKind::TRANSITION},
    {.type = ComponentPresetType::TUBE_COUPLER,
     .name = "TUBE_COUPLER",
     .kind = ComponentKind::TUBE_COUPLER},
    {.type = ComponentPresetType::BULK_HEAD, .name = "BULK_HEAD", .kind = ComponentKind::BULKHEAD},
    {.type = ComponentPresetType::CENTERING_RING,
     .name = "CENTERING_RING",
     .kind = ComponentKind::CENTERING_RING},
    {.type = ComponentPresetType::ENGINE_BLOCK,
     .name = "ENGINE_BLOCK",
     .kind = ComponentKind::ENGINE_BLOCK},
    {.type = ComponentPresetType::LAUNCH_LUG,
     .name = "LAUNCH_LUG",
     .kind = ComponentKind::LAUNCH_LUG},
    {.type = ComponentPresetType::RAIL_BUTTON,
     .name = "RAIL_BUTTON",
     .kind = ComponentKind::RAIL_BUTTON},
    {.type = ComponentPresetType::STREAMER, .name = "STREAMER", .kind = ComponentKind::STREAMER},
    {.type = ComponentPresetType::PARACHUTE, .name = "PARACHUTE", .kind = ComponentKind::PARACHUTE},
}};

void expectType(const TypeExpectation& e)
{
    SCOPED_TRACE(std::string(e.name));
    EXPECT_EQ(QtRocket::componentPresetTypeName(e.type), e.name);
    EXPECT_EQ(QtRocket::componentPresetTypeFromName(e.name), e.type);
    EXPECT_EQ(QtRocket::componentKind(e.type), e.kind);
    EXPECT_EQ(QtRocket::presetTypeOf(e.kind), e.type);
}

TEST(ComponentPresetType, DeclarationOrderIsJavas)
{
    ASSERT_EQ(QtRocket::kAllComponentPresetTypes.size(), kTypes.size());
    for (std::size_t i = 0; i < kTypes.size(); i++)
    {
        EXPECT_EQ(QtRocket::kAllComponentPresetTypes.at(i), kTypes.at(i).type);
        EXPECT_EQ(static_cast<std::size_t>(kTypes.at(i).type), i);
    }
}

TEST(ComponentPresetType, NamesAndComponentKinds)
{
    for (const TypeExpectation& e : kTypes)
    {
        expectType(e);
    }
    EXPECT_EQ(QtRocket::componentPresetTypeFromName("body_tube"), std::nullopt);
    EXPECT_EQ(QtRocket::componentPresetTypeFromName("BULKHEAD"), std::nullopt);
}

TEST(ComponentPresetType, PresetTypeOfEveryKind)
{
    // RocketComponent.getPresetType(): InnerTube and TubeFinSet use body tube presets; the rest
    // without an override return null.
    EXPECT_EQ(QtRocket::presetTypeOf(ComponentKind::INNER_TUBE), ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(QtRocket::presetTypeOf(ComponentKind::TUBE_FIN_SET), ComponentPresetType::BODY_TUBE);
    for (const ComponentKind kind :
         {ComponentKind::ROCKET, ComponentKind::AXIAL_STAGE, ComponentKind::PARALLEL_STAGE,
          ComponentKind::POD_SET, ComponentKind::TRAPEZOID_FIN_SET,
          ComponentKind::ELLIPTICAL_FIN_SET, ComponentKind::FREEFORM_FIN_SET,
          ComponentKind::MASS_COMPONENT, ComponentKind::SHOCK_CORD})
    {
        EXPECT_EQ(QtRocket::presetTypeOf(kind), std::nullopt) << QtRocket::xmlName(kind);
    }
}

}  // namespace
