#include "QtRocket/rocket/MassComponent.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <numbers>
#include <optional>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::MassComponent;
using QtRocket::Parachute;
using QtRocket::PodSet;
using QtRocket::Rocket;
using Type = MassComponent::MassComponentType;

TEST(MassComponent, Defaults)
{
    const MassComponent mc;
    EXPECT_EQ(mc.kind(), ComponentKind::MASS_COMPONENT);
    EXPECT_EQ(mc.getName(), "Mass Component");
    EXPECT_EQ(mc.getComponentMass(), 0.0);
    EXPECT_EQ(mc.getMassComponentType(), Type::MASSCOMPONENT);
    EXPECT_EQ(mc.getLength(), 0.025);
    EXPECT_EQ(mc.getRadius(), 0.0125);
    EXPECT_EQ(mc.getDisplayOrderSide(), 13);
    EXPECT_EQ(mc.getDisplayOrderBack(), 10);
    EXPECT_EQ(mc.getPresetType(), std::nullopt);
    EXPECT_TRUE(mc.getAllMaterials().empty());

    const MassComponent sized{0.1, 0.02, -3.0};  // stored as given, as Java does
    EXPECT_EQ(sized.getLength(), 0.1);
    EXPECT_EQ(sized.getRadius(), 0.02);
    EXPECT_EQ(sized.getComponentMass(), -3.0);
    EXPECT_EQ(sized.getDisplayOrderSide(), 13);
}

TEST(MassComponent, AcceptsInternalComponents)
{
    MassComponent mc;
    EXPECT_TRUE(mc.allowsChildren());
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        EXPECT_EQ(mc.isCompatible(kind), QtRocket::isInternal(kind))
            << QtRocket::componentKindName(kind);
    }
    mc.addChild(std::make_unique<Parachute>());
    EXPECT_THROW(mc.addChild(std::make_unique<PodSet>()), BugError);
}

TEST(MassComponent, MassAndDensity)
{
    MassComponent mc{0.1, 0.02, 0.0};
    mc.setComponentMass(-1.0);
    EXPECT_EQ(mc.getComponentMass(), 0.0);
    EXPECT_EQ(mc.getDensity(), 0.0);

    const double volume = std::numbers::pi * 0.02 * 0.02 * 0.1;
    mc.setComponentMass(0.25);
    EXPECT_NEAR(mc.getDensity(), 0.25 / volume, 1e-9);
    EXPECT_EQ(mc.getMass(), 0.25);

    mc.setDensity(1000);
    EXPECT_NEAR(mc.getComponentMass(), 1000 * volume, 1e-12);

    // The mass is clamped to 0 ... 1e6 and NaN becomes 0.
    mc.setDensity(1e15);
    EXPECT_EQ(mc.getComponentMass(), 1000000.0);
    mc.setDensity(-5);
    EXPECT_EQ(mc.getComponentMass(), 0.0);
    mc.setDensity(std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(mc.getComponentMass(), 0.0);

    // No volume: 0 / 0 is NaN, which gives 0; a mass without volume is infinitely dense.
    MassComponent flat{0.0, 0.02, 0.0};
    EXPECT_EQ(flat.getDensity(), 0.0);
    flat.setComponentMass(1.0);
    EXPECT_TRUE(std::isinf(flat.getDensity()));
}

TEST(MassComponent, SettersFireOnChange)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  body  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    auto&  mc    = body.addChild(std::make_unique<MassComponent>());
    rocket.enableEvents();
    std::vector<int>                              types;
    const ComponentChangeSignal::ScopedConnection connection{rocket.addComponentChangeListener(
        [&types](const ComponentChangeEvent& e) { types.push_back(e.getType()); })};

    mc.setComponentMass(0.0);
    mc.setMassComponentType(Type::MASSCOMPONENT);
    EXPECT_TRUE(types.empty());
    mc.setComponentMass(0.1);
    mc.setMassComponentType(Type::ALTIMETER);
    EXPECT_EQ(types, (std::vector<int>{ComponentChangeEvent::kMassChange,
                                       ComponentChangeEvent::kNonFunctionalChange}));
    EXPECT_EQ(mc.getMassComponentType(), Type::ALTIMETER);
}

TEST(MassComponent, CopiesKeepTheMassAndType)
{
    MassComponent mc{0.1, 0.02, 0.3};
    mc.setMassComponentType(Type::BATTERY);
    const std::unique_ptr<MassComponent> copy =
        QtRocket::componentCast<MassComponent>(mc.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getComponentMass(), 0.3);
    EXPECT_EQ(copy->getMassComponentType(), Type::BATTERY);
    EXPECT_EQ(copy->getLength(), 0.1);
    EXPECT_EQ(copy->getRadius(), 0.02);
}

/// The names of one MassComponentType.
struct TypeNames
{
    Type             type;
    std::string_view name;
    std::string_view ork;
    std::string_view key;
    std::string_view title;
};

/// Every type's names, in declaration order (the titles from OpenRocket's messages.properties).
constexpr std::array<TypeNames, 8> kTypeNames{{
    {.type  = Type::MASSCOMPONENT,
     .name  = "MASSCOMPONENT",
     .ork   = "masscomponent",
     .key   = "MassComponent.MassComponent",
     .title = "Mass Component"},
    {.type  = Type::ALTIMETER,
     .name  = "ALTIMETER",
     .ork   = "altimeter",
     .key   = "MassComponent.Altimeter",
     .title = "Altimeter"},
    {.type  = Type::FLIGHTCOMPUTER,
     .name  = "FLIGHTCOMPUTER",
     .ork   = "flightcomputer",
     .key   = "MassComponent.FlightComputer",
     .title = "Flight Computer"},
    {.type  = Type::DEPLOYMENTCHARGE,
     .name  = "DEPLOYMENTCHARGE",
     .ork   = "deploymentcharge",
     .key   = "MassComponent.DeploymentCharge",
     .title = "Deployment Charge"},
    {.type  = Type::TRACKER,
     .name  = "TRACKER",
     .ork   = "tracker",
     .key   = "MassComponent.Tracker",
     .title = "Tracker"},
    {.type  = Type::PAYLOAD,
     .name  = "PAYLOAD",
     .ork   = "payload",
     .key   = "MassComponent.Payload",
     .title = "Payload"},
    {.type  = Type::RECOVERYHARDWARE,
     .name  = "RECOVERYHARDWARE",
     .ork   = "recoveryhardware",
     .key   = "MassComponent.RecoveryHardware",
     .title = "Recovery Hardware"},
    {.type  = Type::BATTERY,
     .name  = "BATTERY",
     .ork   = "battery",
     .key   = "MassComponent.Battery",
     .title = "Battery"},
}};

class MassComponentTypeTest : public ::testing::TestWithParam<std::size_t>
{ };

TEST_P(MassComponentTypeTest, NamesAndOrkSpelling)
{
    const TypeNames& e = kTypeNames.at(GetParam());
    EXPECT_EQ(MassComponent::kAllMassComponentTypes.at(GetParam()), e.type);
    EXPECT_EQ(QtRocket::massComponentTypeName(e.type), e.name);
    EXPECT_EQ(QtRocket::orkName(e.type), e.ork);
    // The saver writes the lower-cased name, which is what findEnum() matches.
    EXPECT_EQ(QtRocket::orkName(e.type), QtRocket::Strings::toOrkEnumName(e.name));
    EXPECT_EQ(QtRocket::displayKey(e.type), e.key);
    EXPECT_EQ(QtRocket::displayName(e.type), e.title);
    EXPECT_EQ(QtRocket::massComponentTypeFromOrkName(e.ork), e.type);
}

INSTANTIATE_TEST_SUITE_P(MassComponentType, MassComponentTypeTest,
                         ::testing::Range<std::size_t>(0, kTypeNames.size()));

TEST(MassComponentType, OrkNamesAreMatchedAsFindEnumDoes)
{
    EXPECT_EQ(kTypeNames.size(), MassComponent::kAllMassComponentTypes.size());
    EXPECT_EQ(QtRocket::massComponentTypeFromOrkName("  flightcomputer\n"), Type::FLIGHTCOMPUTER);
    EXPECT_EQ(QtRocket::massComponentTypeFromOrkName("FLIGHTCOMPUTER"), std::nullopt);
    EXPECT_EQ(QtRocket::massComponentTypeFromOrkName("flight_computer"), std::nullopt);
    EXPECT_EQ(QtRocket::massComponentTypeFromOrkName(""), std::nullopt);
}

}  // namespace
