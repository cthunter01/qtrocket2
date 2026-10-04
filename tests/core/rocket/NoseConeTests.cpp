#include "QtRocket/rocket/NoseCone.h"

#include <memory>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/MathUtil.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;

/// NoseConeTest's tolerance (MathUtil.EPSILON * 1000).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon * 1000;

// =============================================================================== NoseConeTest

TEST(NoseCone, NormalNoseCone)
{
    NoseCone noseCone;

    // First set the parameters using the normal transition setters (i.e. using AftRadius and
    // AftShoulder instead of Base and Shoulder)
    noseCone.setShapeType(TransitionShape::OGIVE);
    noseCone.setLength(0.06);
    noseCone.setAftRadius(0.1);
    noseCone.setAftShoulderLength(0.01);
    noseCone.setAftShoulderRadius(0.05);
    noseCone.setAftShoulderCapped(false);
    noseCone.setAftShoulderThickness(0.001);

    EXPECT_EQ(TransitionShape::OGIVE, noseCone.getShapeType());
    EXPECT_NEAR(0.06, noseCone.getLength(), kEpsilon);
    EXPECT_NEAR(0.1, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.1, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getAftShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.05, noseCone.getAftShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0.05, noseCone.getShoulderRadius(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftShoulderCapped());
    EXPECT_FALSE(noseCone.isShoulderCapped());
    EXPECT_NEAR(0.001, noseCone.getAftShoulderThickness(), kEpsilon);
    EXPECT_NEAR(0.001, noseCone.getShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderLength(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeShoulderCapped());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());

    // Test setting the specific nose cone setters
    noseCone.setBaseRadius(0.2);
    noseCone.setShoulderLength(0.03);
    noseCone.setShoulderRadius(0.04);
    noseCone.setShoulderCapped(true);
    noseCone.setShoulderThickness(0.005);

    EXPECT_NEAR(0.2, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.2, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getAftShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getAftShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getShoulderRadius(), kEpsilon);
    EXPECT_TRUE(noseCone.isAftShoulderCapped());
    EXPECT_TRUE(noseCone.isShoulderCapped());
    EXPECT_NEAR(0.005, noseCone.getAftShoulderThickness(), kEpsilon);
    EXPECT_NEAR(0.005, noseCone.getShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderLength(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeShoulderCapped());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
}

TEST(NoseCone, FlippedNoseCone)
{
    NoseCone noseCone;

    // First set the parameters using the normal transition setters (i.e. using AftRadius and
    // AftShoulder instead of Base and Shoulder)
    noseCone.setShapeType(TransitionShape::OGIVE);
    noseCone.setLength(0.06);
    noseCone.setAftRadius(0.1);
    noseCone.setAftShoulderLength(0.01);
    noseCone.setAftShoulderRadius(0.05);
    noseCone.setAftShoulderCapped(false);
    noseCone.setAftShoulderThickness(0.001);
    noseCone.setFlipped(true);

    EXPECT_EQ(TransitionShape::OGIVE, noseCone.getShapeType());
    EXPECT_NEAR(0.06, noseCone.getLength(), kEpsilon);
    EXPECT_NEAR(0.1, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.1, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getForeShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.05, noseCone.getForeShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0.05, noseCone.getShoulderRadius(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeShoulderCapped());
    EXPECT_FALSE(noseCone.isShoulderCapped());
    EXPECT_NEAR(0.001, noseCone.getForeShoulderThickness(), kEpsilon);
    EXPECT_NEAR(0.001, noseCone.getShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());

    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderLength(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftShoulderCapped());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    // Test setting the specific nose cone setters
    noseCone.setBaseRadius(0.2);
    noseCone.setShoulderLength(0.03);
    noseCone.setShoulderRadius(0.04);
    noseCone.setShoulderCapped(true);
    noseCone.setShoulderThickness(0.005);

    EXPECT_NEAR(0.2, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.2, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getForeShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getForeShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getShoulderRadius(), kEpsilon);
    EXPECT_TRUE(noseCone.isForeShoulderCapped());
    EXPECT_TRUE(noseCone.isShoulderCapped());
    EXPECT_NEAR(0.005, noseCone.getForeShoulderThickness(), kEpsilon);
    EXPECT_NEAR(0.005, noseCone.getShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());

    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderLength(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftShoulderCapped());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    // Flip back to normal
    noseCone.setFlipped(false);

    EXPECT_NEAR(0.2, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.2, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getAftShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.03, noseCone.getShoulderLength(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getAftShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0.04, noseCone.getShoulderRadius(), kEpsilon);
    EXPECT_TRUE(noseCone.isAftShoulderCapped());
    EXPECT_TRUE(noseCone.isShoulderCapped());
    EXPECT_NEAR(0.005, noseCone.getAftShoulderThickness(), kEpsilon);
    EXPECT_NEAR(0.005, noseCone.getShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderLength(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeShoulderThickness(), kEpsilon);
    EXPECT_FALSE(noseCone.isForeShoulderCapped());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
}

/// OpenRocketDocumentFactory.createNewRocket().getRocket(): a rocket with one stage, events
/// enabled.
struct NewRocket
{
    std::unique_ptr<Rocket> rocket = std::make_unique<Rocket>();
    AxialStage*             stage{nullptr};

    NewRocket()
    {
        stage = &rocket->addChild(std::make_unique<AxialStage>());
        rocket->enableEvents();
    }
};

TEST(NoseCone, NormalNoseConeRadiusAutomatic)
{
    const NewRocket newRocket;
    AxialStage&     stage = *newRocket.stage;

    auto noseConeOwner = std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.06, 0.01);
    auto tube1Owner    = std::make_unique<BodyTube>(0.06, 0.023);
    tube1Owner->setOuterRadiusAutomatic(false);
    auto tube2Owner = std::make_unique<BodyTube>(0.06, 0.03);
    tube2Owner->setOuterRadiusAutomatic(false);

    // Test no previous or next component
    NoseCone& noseCone = stage.addChild(std::move(noseConeOwner));

    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&stage, noseCone.getPreviousComponent());
    EXPECT_EQ(noseCone.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextSymmetricComponent(), nullptr);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    // Java compares each radius with itself, here and below: the getters, which refresh an
    // automatic radius, are called twice and must answer the same.
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    noseCone.setAftRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    noseCone.setForeRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());

    // Test with next component
    BodyTube& tube1 = stage.addChild(std::move(tube1Owner));

    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&stage, noseCone.getPreviousComponent());
    EXPECT_EQ(noseCone.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(&tube1, noseCone.getNextComponent());
    EXPECT_EQ(&tube1, noseCone.getNextSymmetricComponent());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    noseCone.setAftRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_TRUE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&stage, noseCone.getPreviousComponent());
    EXPECT_EQ(noseCone.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(&tube1, noseCone.getNextComponent());
    EXPECT_EQ(&tube1, noseCone.getNextSymmetricComponent());
    EXPECT_TRUE(noseCone.isAftRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.023, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.023, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    noseCone.setAftRadiusAutomatic(false, true);
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    noseCone.setBaseRadiusAutomatic(true);
    EXPECT_NEAR(0.023, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.023, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    noseCone.setForeRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());

    // Test with previous component
    BodyTube& tube2 = stage.addChild(std::move(tube2Owner), 0);

    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_TRUE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&tube2, noseCone.getPreviousComponent());
    EXPECT_EQ(&tube2, noseCone.getPreviousSymmetricComponent());
    EXPECT_EQ(&tube1, noseCone.getNextComponent());
    EXPECT_EQ(&tube1, noseCone.getNextSymmetricComponent());
    EXPECT_TRUE(noseCone.isAftRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.023, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.023, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getForeRadius(), kEpsilon);

    // Do a flip
    noseCone.setFlipped(true);
    EXPECT_TRUE(noseCone.isForeRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
}

TEST(NoseCone, FlippedNoseConeRadiusAutomatic)
{
    const NewRocket newRocket;
    AxialStage&     stage = *newRocket.stage;

    auto noseConeOwner = std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.06, 0.01);
    noseConeOwner->setFlipped(true);
    auto tube1Owner = std::make_unique<BodyTube>(0.06, 0.02);
    tube1Owner->setOuterRadiusAutomatic(false);
    auto tube2Owner = std::make_unique<BodyTube>(0.06, 0.03);
    tube2Owner->setOuterRadiusAutomatic(false);

    // Test no previous or next component
    NoseCone& noseCone = stage.addChild(std::move(noseConeOwner));

    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&stage, noseCone.getPreviousComponent());
    EXPECT_EQ(noseCone.getPreviousSymmetricComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextSymmetricComponent(), nullptr);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0.01, noseCone.getForeRadius(), kEpsilon);

    noseCone.setAftRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    noseCone.setForeRadiusAutomatic(true, true);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());

    // Test with previous component
    BodyTube& tube1 = stage.addChild(std::move(tube1Owner), 0);

    EXPECT_FALSE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&tube1, noseCone.getPreviousComponent());
    EXPECT_EQ(&tube1, noseCone.getPreviousSymmetricComponent());
    EXPECT_EQ(noseCone.getNextComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextSymmetricComponent(), nullptr);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_FALSE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);

    noseCone.setBaseRadiusAutomatic(true);
    EXPECT_TRUE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&tube1, noseCone.getPreviousComponent());
    EXPECT_EQ(&tube1, noseCone.getPreviousSymmetricComponent());
    EXPECT_EQ(noseCone.getNextComponent(), nullptr);
    EXPECT_EQ(noseCone.getNextSymmetricComponent(), nullptr);
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_TRUE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(tube1.getAftRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getAftRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);

    noseCone.setForeRadiusAutomatic(false, true);
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getBaseRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getForeRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getForeRadius(), kEpsilon);

    noseCone.setBaseRadiusAutomatic(true);
    EXPECT_NEAR(tube1.getAftRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getAftRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);

    EXPECT_TRUE(noseCone.isForeRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());

    // Test with next component
    BodyTube& tube2 = stage.addChild(std::move(tube2Owner));

    EXPECT_TRUE(noseCone.usesPreviousCompAutomatic());
    EXPECT_FALSE(noseCone.usesNextCompAutomatic());
    EXPECT_EQ(&tube1, noseCone.getPreviousComponent());
    EXPECT_EQ(&tube1, noseCone.getPreviousSymmetricComponent());
    EXPECT_EQ(&tube2, noseCone.getNextComponent());
    EXPECT_EQ(&tube2, noseCone.getNextSymmetricComponent());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_TRUE(noseCone.isBaseRadiusAutomatic());
    EXPECT_TRUE(noseCone.isForeRadiusAutomatic());
    EXPECT_NEAR(tube1.getAftRadius(), noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getForeRadius(), kEpsilon);
    EXPECT_NEAR(tube1.getForeRadius(), noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(0.02, noseCone.getBaseRadius(), kEpsilon);
    EXPECT_NEAR(noseCone.getAftRadius(), noseCone.getAftRadius(), kEpsilon);
    EXPECT_NEAR(0, noseCone.getAftRadius(), kEpsilon);
}

// ===================================================================== NoseConeComponentTests

/// NoseConeComponentTests: a NOSE_CONE preset (length 2, conical, aft diameter 2, an aft
/// shoulder 1 long of diameter 1, filled, mass 100, material "test").
class NoseConePresetTest : public ::testing::Test
{
protected:
    NoseConePresetTest()
    {
        TypedPropertyMap presetspec;
        presetspec.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
        presetspec.put(ComponentPreset::kManufacturer,
                       Manufacturer::getManufacturer("manufacturer"));
        presetspec.put(ComponentPreset::kPartNo, "partno");
        presetspec.put(ComponentPreset::kLength, 2.0);
        presetspec.put(ComponentPreset::kShape, TransitionShape::CONICAL);
        presetspec.put(ComponentPreset::kAftOuterDiameter, 2.0);
        presetspec.put(ComponentPreset::kAftShoulderLength, 1.0);
        presetspec.put(ComponentPreset::kAftShoulderDiameter, 1.0);
        presetspec.put(ComponentPreset::kFilled, true);
        presetspec.put(ComponentPreset::kMass, 100.0);
        presetspec.put(ComponentPreset::kMaterial,
                       Material::newMaterial(Material::Type::BULK, "test", 2.0, true));
        auto created = ComponentPresetFactory::create(presetspec, m_materials);
        if (!created)
        {
            ADD_FAILURE() << created.error().message;
            return;
        }
        m_preset = std::make_unique<ComponentPreset>(std::move(*created));
    }

    /// A new nose cone with the preset loaded.
    [[nodiscard]] std::unique_ptr<NoseCone> loaded() const
    {
        auto nc = std::make_unique<NoseCone>();
        nc->loadPreset(m_preset.get());
        return nc;
    }

    QtRocket::MaterialStorage        m_materials;
    std::unique_ptr<ComponentPreset> m_preset;
};

TEST_F(NoseConePresetTest, ComponentType)
{
    const NoseCone nc;

    EXPECT_EQ(ComponentPresetType::NOSE_CONE, nc.getPresetType());
}

TEST_F(NoseConePresetTest, LoadFromPresetIsSane)
{
    const std::unique_ptr<NoseCone> nc = loaded();

    EXPECT_EQ(2.0, nc->getLength());
    EXPECT_EQ(TransitionShape::CONICAL, nc->getShapeType());
    EXPECT_EQ(1.0, nc->getAftRadius());
    EXPECT_EQ(0.0, nc->getForeShoulderLength());
    EXPECT_EQ(0.0, nc->getForeShoulderRadius());
    EXPECT_EQ(1.0, nc->getAftShoulderLength());
    EXPECT_EQ(0.5, nc->getAftShoulderRadius());
    EXPECT_EQ(0.5, nc->getAftShoulderThickness());

    EXPECT_FALSE(nc->isForeRadiusAutomatic());
    EXPECT_FALSE(nc->isAftRadiusAutomatic());
    EXPECT_TRUE(nc->isFilled());

    EXPECT_EQ(m_preset->get(ComponentPreset::kMaterial), nc->getMaterial());
    EXPECT_NEAR(100.0, nc->getMass(), 0.05);
}

TEST_F(NoseConePresetTest, ChangeLengthClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setLength(1.0);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeAftRadiusClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setAftRadius(2.0);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeAftRadiusAutomaticClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setAftRadiusAutomatic(true);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeAftShoulderRadiusClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setAftShoulderRadius(2.0);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeAftShoulderLengthLeavesPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setAftShoulderLength(2.0);
    EXPECT_EQ(nc->getPresetComponent(), m_preset.get());
}

TEST_F(NoseConePresetTest, ChangeThicknessClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setThickness(0.1);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeFilledClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setFilled(false);
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeMaterialClearsPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(nc->getPresetComponent(), nullptr);
}

TEST_F(NoseConePresetTest, ChangeFinishLeavesPreset)
{
    const std::unique_ptr<NoseCone> nc = loaded();
    nc->setFinish(Finish::POLISHED);
    EXPECT_EQ(nc->getPresetComponent(), m_preset.get());
}

TEST_F(NoseConePresetTest, AFlippedNoseConeLoadsThePresetAtItsBase)
{
    // Not in OpenRocket's tests: the preset describes a normal nose cone; a flipped one gets it
    // at its front.
    NoseCone nc;
    nc.setFlipped(true);
    nc.loadPreset(m_preset.get());

    EXPECT_TRUE(nc.isFlipped());
    EXPECT_EQ(nc.getBaseRadius(), 1.0);
    EXPECT_EQ(nc.getForeRadius(), 1.0);
    EXPECT_EQ(nc.getAftRadius(), 0.0);
    EXPECT_EQ(nc.getShoulderLength(), 1.0);
    EXPECT_EQ(nc.getForeShoulderLength(), 1.0);
    EXPECT_EQ(nc.getShoulderRadius(), 0.5);
    EXPECT_EQ(nc.getAftShoulderLength(), 0.0);
    EXPECT_EQ(nc.getPresetComponent(), m_preset.get());
}

// ===================================================================== behaviour beyond JUnit

TEST(NoseCone, Defaults)
{
    const NoseCone noseCone;
    EXPECT_EQ(noseCone.kind(), ComponentKind::NOSE_CONE);
    EXPECT_EQ(noseCone.getComponentName(), "Nose Cone");
    EXPECT_EQ(noseCone.getName(), "Nose Cone");
    EXPECT_EQ(noseCone.getShapeType(), TransitionShape::OGIVE);
    EXPECT_EQ(noseCone.getShapeParameter(), 1.0);
    EXPECT_EQ(noseCone.getLength(), 6 * SymmetricComponent::kDefaultRadius);
    EXPECT_EQ(noseCone.getBaseRadius(), SymmetricComponent::kDefaultRadius);
    EXPECT_EQ(noseCone.getForeRadius(), 0.0);
    EXPECT_FALSE(noseCone.isForeRadiusAutomatic());
    EXPECT_FALSE(noseCone.isAftRadiusAutomatic());
    EXPECT_EQ(noseCone.getThickness(), 0.002);
    EXPECT_FALSE(noseCone.isFlipped());
    EXPECT_EQ(noseCone.getDisplayOrderSide(), 1);
    EXPECT_EQ(noseCone.getDisplayOrderBack(), 0);
    EXPECT_EQ(noseCone.getMaterial().getName(), "Cardboard");
}

TEST(NoseCone, IsNeverClipped)
{
    NoseCone noseCone(TransitionShape::ELLIPSOID, 0.1, 0.02);
    EXPECT_TRUE(noseCone.isClippedEnabled()) << "the shape allows it";
    EXPECT_FALSE(noseCone.isClipped());
    noseCone.setClipped(true);
    EXPECT_FALSE(noseCone.isClipped());
    noseCone.setShapeType(TransitionShape::HAACK);
    EXPECT_FALSE(noseCone.isClipped());
    // The profile is the unclipped one.
    EXPECT_EQ(noseCone.getRadius(0.05), QtRocket::getTransitionRadius(TransitionShape::HAACK, 0.05,
                                                                      0.0, 0.02, 0.1, 0.0, false));
}

TEST(NoseCone, FlippingFiresOneEvent)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  nose  = stage.addChild(std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.1, 0.02));
    rocket.enableEvents();
    int        count      = 0;
    int        lastType   = 0;
    const auto connection = ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&](const ComponentChangeEvent& e) {
            ++count;
            lastType = e.getType();
        })};

    nose.setFlipped(true);
    EXPECT_EQ(count, 1) << "the changes made while flipping are bypassed";
    EXPECT_EQ(lastType, ComponentChangeEvent::kBothChange);
    EXPECT_FALSE(nose.isBypassComponentChangeEvent());

    nose.setFlipped(true);
    EXPECT_EQ(count, 1) << "no change";
}

TEST(NoseCone, FlippingReversesTheProfile)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  nose  = stage.addChild(std::make_unique<NoseCone>(TransitionShape::CONICAL, 0.1, 0.02));
    rocket.enableEvents();

    const double volume = nose.getComponentVolume();
    nose.setFlipped(true);
    EXPECT_EQ(nose.getRadius(0.0), 0.02);
    EXPECT_EQ(nose.getRadius(0.1), 0.0);
    EXPECT_NEAR(nose.getComponentVolume(), volume, volume * 1e-12);
}

TEST(NoseCone, CopiesKeepTheOrientation)
{
    NoseCone noseCone(TransitionShape::POWER, 0.1, 0.02);
    noseCone.setFlipped(true);
    const std::unique_ptr<RocketComponent> copy = noseCone.copyWithOriginalId();
    const auto*                            nc   = dynamic_cast<const NoseCone*>(copy.get());
    ASSERT_NE(nc, nullptr);
    EXPECT_TRUE(nc->isFlipped());
    EXPECT_EQ(nc->getBaseRadius(), 0.02);
    EXPECT_EQ(nc->getShapeType(), TransitionShape::POWER);
    EXPECT_EQ(nc->kind(), ComponentKind::NOSE_CONE);
}

}  // namespace
