#include "QtRocket/rocket/Transition.h"

#include <cmath>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/Coordinate.h"
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
using QtRocket::Coordinate;
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::NoseCone;
using QtRocket::ReferenceType;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;

/// TransitionTest's tolerance (MathUtil.EPSILON * 1000).
constexpr double kEpsilon = QtRocket::MathUtil::kEpsilon * 1000;

// ============================================================================= TransitionTest

TEST(Transition, VerifyConicNose)
{
    const NoseCone nose(TransitionShape::CONICAL, 0.06, 0.01);
    EXPECT_NEAR(0.06, nose.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(0.00, nose.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(0.01, nose.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::CONICAL, nose.getShapeType()) << "nose cone shape type is wrong ";
    EXPECT_NEAR(0.0, nose.getShapeParameter(), kEpsilon) << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(0.0, nose.getRadius(0.00), kEpsilon) << "bad shape - conical forward ";
    EXPECT_NEAR(0.0025, nose.getRadius(0.015), kEpsilon) << "bad shape - conical forward ";
    EXPECT_NEAR(0.005, nose.getRadius(0.03), kEpsilon) << "bad shape - conical forward ";
    EXPECT_NEAR(0.0075, nose.getRadius(0.045), kEpsilon) << "bad shape - conical forward ";
    EXPECT_NEAR(0.01, nose.getRadius(0.06), kEpsilon) << "bad shape - conical forward ";
}

TEST(Transition, VerifyForwardConicTransition)
{
    Transition nose;
    nose.setShapeType(TransitionShape::CONICAL);
    nose.setForeRadius(0.5);
    nose.setAftRadius(1.0);
    nose.setLength(5.0);

    EXPECT_NEAR(5.0, nose.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(0.5, nose.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(1.0, nose.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::CONICAL, nose.getShapeType()) << "nose cone shape type is wrong ";
    EXPECT_NEAR(0.0, nose.getShapeParameter(), kEpsilon) << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(0.5, nose.getRadius(0.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.6, nose.getRadius(1.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.7, nose.getRadius(2.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.8, nose.getRadius(3.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.9, nose.getRadius(4.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(1.0, nose.getRadius(5.0), kEpsilon) << "bad shape - conical forward transition";
}

TEST(Transition, VerifyBackwardConicTransition)
{
    Transition tail;
    tail.setShapeType(TransitionShape::CONICAL);
    tail.setForeRadius(1.0);
    tail.setAftRadius(0.5);
    tail.setLength(5.0);

    EXPECT_NEAR(5.0, tail.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(1.0, tail.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(0.5, tail.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::CONICAL, tail.getShapeType()) << "nose cone shape type is wrong ";
    EXPECT_NEAR(0.0, tail.getShapeParameter(), kEpsilon) << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(1.0, tail.getRadius(0.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.9, tail.getRadius(1.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.8, tail.getRadius(2.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.7, tail.getRadius(3.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.6, tail.getRadius(4.0), kEpsilon) << "bad shape - conical forward transition";
    EXPECT_NEAR(0.5, tail.getRadius(5.0), kEpsilon) << "bad shape - conical forward transition";
}

TEST(Transition, VerifyOgiveNoseCone)
{
    Transition nose;
    nose.setShapeType(TransitionShape::OGIVE);
    nose.setForeRadius(0.0);
    nose.setAftRadius(1.0);
    nose.setLength(8.0);

    EXPECT_NEAR(8.0, nose.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(0.0, nose.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(1.0, nose.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::OGIVE, nose.getShapeType()) << "nose cone shape type is wrong ";
    EXPECT_NEAR(1.0, nose.getShapeParameter(), kEpsilon) << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(0.0, nose.getRadius(0.0), kEpsilon);
    EXPECT_NEAR(0.23720214511, nose.getRadius(1.0), kEpsilon);
    EXPECT_NEAR(0.44135250736, nose.getRadius(2.0), kEpsilon);
    EXPECT_NEAR(0.61308144666, nose.getRadius(3.0), kEpsilon);
    EXPECT_NEAR(0.75290684574, nose.getRadius(4.0), kEpsilon);
    EXPECT_NEAR(0.86124225056, nose.getRadius(5.0), kEpsilon);
    EXPECT_NEAR(0.93840316661, nose.getRadius(6.0), kEpsilon);
    EXPECT_NEAR(0.98461174156, nose.getRadius(7.0), kEpsilon);
    EXPECT_NEAR(1.0, nose.getRadius(8.0), kEpsilon);
}

TEST(Transition, VerifyForwardOgiveTransition)
{
    Transition transition;
    transition.setShapeType(TransitionShape::OGIVE);
    transition.setForeRadius(0.44135);
    transition.setAftRadius(1.0);
    transition.setLength(6.0);

    EXPECT_NEAR(6.0, transition.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(0.44135, transition.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(1.0, transition.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::OGIVE, transition.getShapeType())
        << "nose cone shape type is wrong ";
    EXPECT_NEAR(1.0, transition.getShapeParameter(), kEpsilon)
        << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(0.44135250736, transition.getRadius(0.0), kEpsilon);
    EXPECT_NEAR(0.61308144666, transition.getRadius(1.0), kEpsilon);
    EXPECT_NEAR(0.75290684574, transition.getRadius(2.0), kEpsilon);
    EXPECT_NEAR(0.86124225056, transition.getRadius(3.0), kEpsilon);
    EXPECT_NEAR(0.93840316661, transition.getRadius(4.0), kEpsilon);
    EXPECT_NEAR(0.98461174156, transition.getRadius(5.0), kEpsilon);
    EXPECT_NEAR(1.0, transition.getRadius(6.0), kEpsilon);
}

TEST(Transition, VerifyBackwardOgiveTransition)
{
    Transition transition;
    transition.setShapeType(TransitionShape::OGIVE);
    transition.setForeRadius(1.0);
    transition.setAftRadius(0.44135);
    transition.setLength(6.0);

    EXPECT_NEAR(6.0, transition.getLength(), kEpsilon) << "nose cone length is wrong ";
    EXPECT_NEAR(1.0, transition.getForeRadius(), kEpsilon) << "nose cone fore radius is wrong ";
    EXPECT_NEAR(0.44135, transition.getAftRadius(), kEpsilon) << "nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::OGIVE, transition.getShapeType())
        << "nose cone shape type is wrong ";
    EXPECT_NEAR(1.0, transition.getShapeParameter(), kEpsilon)
        << "nose cone shape parameter is wrong ";

    EXPECT_NEAR(1.0, transition.getRadius(0.0), kEpsilon);
    EXPECT_NEAR(0.98461174156, transition.getRadius(1.0), kEpsilon);
    EXPECT_NEAR(0.93840316661, transition.getRadius(2.0), kEpsilon);
    EXPECT_NEAR(0.86124225056, transition.getRadius(3.0), kEpsilon);
    EXPECT_NEAR(0.75290684574, transition.getRadius(4.0), kEpsilon);
    EXPECT_NEAR(0.61308144666, transition.getRadius(5.0), kEpsilon);
    EXPECT_NEAR(0.44135250736, transition.getRadius(6.0), kEpsilon);
}

TEST(Transition, StockIntegration)
{
    // The nose cone of TestRockets.makeEstesAlphaIII(), built and placed as there (only the
    // stage's first two components matter to it).
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.setName("Stage");

    auto nosecone = std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.07, 0.012);
    nosecone->setAftShoulderLength(0.02);
    // Test with a thickness set to 0 (changed by setAftShoulderLength)
    nosecone->setAftShoulderThickness(0);
    nosecone->setAftShoulderRadius(0.011);
    nosecone->setName("Nose Cone");
    const NoseCone& nose = stage.addChild(std::move(nosecone));
    stage.addChild(std::make_unique<BodyTube>(0.20, 0.012, 0.0003)).setName("Body Tube");
    rocket.enableEvents();

    EXPECT_NEAR(0.07, nose.getLength(), kEpsilon) << "Alpha3 nose cone length is wrong ";
    EXPECT_NEAR(0.00, nose.getForeRadius(), kEpsilon) << "Alpha3 nose cone fore radius is wrong ";
    EXPECT_NEAR(0.012, nose.getAftRadius(), kEpsilon) << "Alpha3 nose cone aft radius is wrong ";
    EXPECT_EQ(TransitionShape::OGIVE, nose.getShapeType())
        << "Alpha3 nose cone shape type is wrong ";
    EXPECT_NEAR(1.0, nose.getShapeParameter(), kEpsilon)
        << "Alpha3 nose cone shape parameter is wrong ";

    EXPECT_NEAR(0.02, nose.getAftShoulderLength(), kEpsilon)
        << "Alpha3 nose cone aft shoulder length is wrong ";
    EXPECT_NEAR(0.011, nose.getAftShoulderRadius(), kEpsilon)
        << "Alpha3 nose cone aft shoulder radius is wrong ";
}

/// A transition whose length is always 0 (TransitionTest's anonymous subclass), with
/// calculateProperties() callable.
class ZeroLengthTransition : public Transition
{
public:
    [[nodiscard]] double getLength() const override { return 0.0; }
    using Transition::calculateProperties;
};

TEST(Transition, ZeroLengthTransitionCalculations)
{
    // Test the calculation of the properties of a zero-length transition. This is to ensure no
    // regressions with GitHub issue #2626.
    ZeroLengthTransition transition;
    transition.setAftShoulderLength(0.1);
    transition.calculateProperties();

    // Beyond the JUnit test (which only checks that nothing throws): only the shoulder counts.
    EXPECT_TRUE(std::isfinite(transition.getComponentVolume()));
    EXPECT_GT(transition.getComponentVolume(), 0.0);
    EXPECT_TRUE(std::isfinite(transition.getComponentCG().x));
    EXPECT_TRUE(std::isfinite(transition.getLongitudinalUnitInertia()));
    EXPECT_TRUE(std::isfinite(transition.getRotationalUnitInertia()));
    EXPECT_EQ(transition.getComponentWetArea(), 0.0);
}

// =================================================================== TransitionComponentTests

/// TransitionComponentTests: a TRANSITION preset (length 2, conical, aft diameter 2, fore
/// diameter 1, shoulders 1 long of diameters 1 and 0.5, filled, mass 100, material "test").
class TransitionPresetTest : public ::testing::Test
{
protected:
    TransitionPresetTest()
    {
        TypedPropertyMap presetspec;
        presetspec.put(ComponentPreset::kType, ComponentPresetType::TRANSITION);
        presetspec.put(ComponentPreset::kManufacturer,
                       Manufacturer::getManufacturer("manufacturer"));
        presetspec.put(ComponentPreset::kPartNo, "partno");
        presetspec.put(ComponentPreset::kLength, 2.0);
        presetspec.put(ComponentPreset::kShape, TransitionShape::CONICAL);
        presetspec.put(ComponentPreset::kAftOuterDiameter, 2.0);
        presetspec.put(ComponentPreset::kForeOuterDiameter, 1.0);
        presetspec.put(ComponentPreset::kAftShoulderLength, 1.0);
        presetspec.put(ComponentPreset::kAftShoulderDiameter, 1.0);
        presetspec.put(ComponentPreset::kForeShoulderLength, 1.0);
        presetspec.put(ComponentPreset::kForeShoulderDiameter, 0.5);
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

    /// A new transition with the preset loaded.
    [[nodiscard]] std::unique_ptr<Transition> loaded() const
    {
        auto tr = std::make_unique<Transition>();
        tr->loadPreset(m_preset.get());
        return tr;
    }

    QtRocket::MaterialStorage        m_materials;
    std::unique_ptr<ComponentPreset> m_preset;
};

TEST_F(TransitionPresetTest, ComponentType)
{
    const Transition tr;

    EXPECT_EQ(ComponentPresetType::TRANSITION, tr.getPresetType());
}

TEST_F(TransitionPresetTest, LoadFromPresetIsSane)
{
    const std::unique_ptr<Transition> tr = loaded();

    EXPECT_EQ(2.0, tr->getLength());
    EXPECT_EQ(TransitionShape::CONICAL, tr->getShapeType());
    EXPECT_EQ(1.0, tr->getAftRadius());
    EXPECT_EQ(1.0, tr->getForeShoulderLength());
    EXPECT_EQ(0.25, tr->getForeShoulderRadius());
    EXPECT_EQ(0.25, tr->getForeShoulderThickness());
    EXPECT_EQ(1.0, tr->getAftShoulderLength());
    EXPECT_EQ(0.5, tr->getAftShoulderRadius());
    EXPECT_EQ(0.5, tr->getAftShoulderThickness());

    EXPECT_FALSE(tr->isForeRadiusAutomatic());
    EXPECT_FALSE(tr->isAftRadiusAutomatic());
    EXPECT_TRUE(tr->isFilled());

    EXPECT_EQ(m_preset->get(ComponentPreset::kMaterial), tr->getMaterial());
    EXPECT_NEAR(100.0, tr->getMass(), 1.0);
    EXPECT_EQ(tr->getPresetComponent(), m_preset.get());
}

TEST_F(TransitionPresetTest, ChangeLengthClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setLength(1.0);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeAftRadiusClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setAftRadius(2.0);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeAftRadiusAutomaticClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setAftRadiusAutomatic(true);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeForeRadiusClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setForeRadius(2.0);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeForeRadiusAutomaticClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setForeRadiusAutomatic(true);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeForeShoulderRadiusClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setForeShoulderRadius(2.0);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeAftShoulderRadiusClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setAftShoulderRadius(2.0);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeAftShoulderLengthLeavesPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setAftShoulderLength(2.0);
    EXPECT_EQ(tr->getPresetComponent(), m_preset.get());
}

TEST_F(TransitionPresetTest, ChangeThicknessClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setThickness(0.1);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeFilledClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setFilled(false);
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeMaterialClearsPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(tr->getPresetComponent(), nullptr);
}

TEST_F(TransitionPresetTest, ChangeFinishLeavesPreset)
{
    const std::unique_ptr<Transition> tr = loaded();
    tr->setFinish(Finish::POLISHED);
    EXPECT_EQ(tr->getPresetComponent(), m_preset.get());
}

TEST_F(TransitionPresetTest, AThicknessBecomesTheShoulderThickness)
{
    // Not in OpenRocket's tests: a preset with a wall thickness gives the shoulders that
    // thickness (a preset shoulder length would then set up the shoulder from scratch).
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::TRANSITION);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "thick");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kAftOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kForeOuterDiameter, 1.0);
    presetspec.put(ComponentPreset::kThickness, 0.05);
    auto created = ComponentPresetFactory::create(presetspec, m_materials);
    ASSERT_TRUE(created.has_value());

    Transition tr;
    tr.setFilled(true);
    tr.loadPreset(&*created);
    EXPECT_FALSE(tr.isFilled());
    EXPECT_EQ(tr.getThickness(), 0.05);
    EXPECT_EQ(tr.getForeShoulderThickness(), 0.05);
    EXPECT_EQ(tr.getAftShoulderThickness(), 0.05);
    EXPECT_EQ(tr.getShapeType(), TransitionShape::CONICAL) << "no shape in the preset";
}

// ===================================================================== behaviour beyond JUnit

TEST(Transition, Defaults)
{
    const Transition transition;
    EXPECT_EQ(transition.kind(), ComponentKind::TRANSITION);
    EXPECT_EQ(transition.getComponentName(), "Transition");
    EXPECT_EQ(transition.getLength(), SymmetricComponent::kDefaultRadius * 3);
    EXPECT_TRUE(transition.isForeRadiusAutomatic());
    EXPECT_TRUE(transition.isAftRadiusAutomatic());
    EXPECT_EQ(transition.getForeRadius(), SymmetricComponent::kDefaultRadius);
    EXPECT_EQ(transition.getAftRadius(), SymmetricComponent::kDefaultRadius);
    EXPECT_EQ(transition.getShapeType(), TransitionShape::CONICAL);
    EXPECT_EQ(transition.getShapeParameter(), 0.0);
    EXPECT_FALSE(transition.isClipped()) << "a cone is not clippable";
    EXPECT_FALSE(transition.isClippedEnabled());
    EXPECT_EQ(transition.getForeShoulderLength(), 0.0);
    EXPECT_EQ(transition.getAftShoulderLength(), 0.0);
    EXPECT_FALSE(transition.isForeShoulderCapped());
    EXPECT_FALSE(transition.isAftShoulderCapped());
    EXPECT_EQ(transition.getDisplayOrderSide(), 2);
    EXPECT_EQ(transition.getDisplayOrderBack(), 2);
    EXPECT_EQ(transition.getMaterial().getName(), "Cardboard");
    EXPECT_EQ(transition.getFinish(), Finish::NORMAL);
}

TEST(Transition, ShapeTypeSetsClippingAndTheDefaultParameter)
{
    Transition transition;
    transition.setShapeType(TransitionShape::ELLIPSOID);
    EXPECT_TRUE(transition.isClipped());
    EXPECT_TRUE(transition.isClippedEnabled());
    EXPECT_EQ(transition.getShapeParameter(), 0.0);

    transition.setShapeType(TransitionShape::POWER);
    EXPECT_EQ(transition.getShapeParameter(), 0.5);
    transition.setClipped(false);
    EXPECT_FALSE(transition.isClipped());

    transition.setShapeType(TransitionShape::OGIVE);
    EXPECT_EQ(transition.getShapeParameter(), 1.0);
    EXPECT_FALSE(transition.isClipped());
    transition.setClipped(true);
    EXPECT_FALSE(transition.isClipped()) << "an ogive is not clippable";

    transition.setShapeType(TransitionShape::HAACK);
    EXPECT_EQ(transition.getShapeParameterMin(), 0.0);
    EXPECT_EQ(transition.getShapeParameterMax(), 1.0 / 3.0);
    transition.setShapeParameter(0.9);
    EXPECT_EQ(transition.getShapeParameter(), 1.0 / 3.0) << "clamped to the shape's range";
    transition.setShapeParameter(-1);
    EXPECT_EQ(transition.getShapeParameter(), 0.0);
}

TEST(Transition, ClippedProfileAndItsCachedClipLength)
{
    Rocket rocket;
    auto&  stage      = rocket.addChild(std::make_unique<AxialStage>());
    auto&  transition = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();
    transition.setShapeType(TransitionShape::ELLIPSOID);
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.04);
    transition.setLength(0.1);
    ASSERT_TRUE(transition.isClipped());

    const double clipped = transition.getRadius(0.05);
    EXPECT_EQ(clipped, QtRocket::getTransitionRadius(TransitionShape::ELLIPSOID, 0.05, 0.02, 0.04,
                                                     0.1, 0.0, true));
    transition.setClipped(false);
    EXPECT_EQ(transition.getRadius(0.05),
              QtRocket::getTransitionRadius(TransitionShape::ELLIPSOID, 0.05, 0.02, 0.04, 0.1, 0.0,
                                            false));
    EXPECT_NE(transition.getRadius(0.05), clipped);

    // The clip length is solved again after a change (the cached one would give a wrong radius).
    transition.setClipped(true);
    transition.setForeRadius(0.01);
    EXPECT_EQ(transition.getRadius(0.05),
              QtRocket::getTransitionRadius(TransitionShape::ELLIPSOID, 0.05, 0.01, 0.04, 0.1, 0.0,
                                            true));
}

TEST(Transition, InnerRadiusIsTheProfileLessTheWall)
{
    Transition transition;
    transition.setForeRadius(0.01);
    transition.setAftRadius(0.03);
    transition.setLength(0.2);
    transition.setThickness(0.005);
    EXPECT_NEAR(transition.getInnerRadius(0.1), 0.02 - 0.005, 1e-15);
    EXPECT_EQ(transition.getInnerRadius(-1.0), 0.005);
    transition.setThickness(0.02);
    EXPECT_EQ(transition.getInnerRadius(0.0), 0.0) << "not below 0";
}

TEST(Transition, AutomaticRadiiComeFromTheNeighbours)
{
    Rocket rocket;
    auto&  stage      = rocket.addChild(std::make_unique<AxialStage>());
    auto&  front      = stage.addChild(std::make_unique<BodyTube>(0.1, 0.03));
    auto&  transition = stage.addChild(std::make_unique<Transition>());
    auto&  back       = stage.addChild(std::make_unique<BodyTube>(0.1, 0.02));
    rocket.enableEvents();

    EXPECT_EQ(transition.getForeRadius(), 0.03);
    EXPECT_EQ(transition.getAftRadius(), 0.02);
    EXPECT_TRUE(transition.usesPreviousCompAutomatic());
    EXPECT_TRUE(transition.usesNextCompAutomatic());
    EXPECT_EQ(transition.getFrontAutoRadius(), -1.0) << "the aft radius is automatic";
    EXPECT_EQ(transition.getRearAutoRadius(), -1.0) << "the fore radius is automatic";

    front.setOuterRadius(0.035);
    back.setOuterRadius(0.015);
    EXPECT_EQ(transition.getForeRadius(), 0.035);
    EXPECT_EQ(transition.getAftRadius(), 0.015);

    // A fixed radius is offered to the neighbours.
    transition.setAftRadius(0.025);
    EXPECT_FALSE(transition.isAftRadiusAutomatic());
    EXPECT_EQ(transition.getFrontAutoRadius(), 0.025);
    back.setOuterRadiusAutomatic(true);
    EXPECT_EQ(back.getOuterRadius(), 0.025);
}

TEST(Transition, AnAutomaticRadiusWithoutAnOfferIsMinusOne)
{
    // OpenRocket takes the neighbour's offer as it is: a transition whose previous transition has
    // an automatic aft radius gets -1.
    Rocket      rocket;
    auto&       stage  = rocket.addChild(std::make_unique<AxialStage>());
    const auto& first  = stage.addChild(std::make_unique<Transition>());
    const auto& second = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();

    EXPECT_EQ(first.getForeRadius(), SymmetricComponent::kDefaultRadius) << "no previous one";
    EXPECT_EQ(second.getForeRadius(), -1.0);
}

TEST(Transition, SanityCheckedAutomaticRadii)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  tube  = stage.addChild(std::make_unique<BodyTube>(0.1, 0.03));
    auto&  tail  = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();
    tail.setForeRadius(0.03);
    tail.setAftRadius(0.02);

    EXPECT_FALSE(tail.canUseNextCompAutomatic()) << "no next component";
    tail.setAftRadiusAutomatic(true, true);
    EXPECT_FALSE(tail.isAftRadiusAutomatic());

    EXPECT_TRUE(tail.canUsePreviousCompAutomatic());
    tail.setForeRadiusAutomatic(true, true);
    EXPECT_TRUE(tail.isForeRadiusAutomatic());

    // A tube that takes its radius from the transition cannot give it one.
    tail.setForeRadius(0.03);
    tube.setOuterRadiusAutomatic(true);
    EXPECT_EQ(tube.getOuterRadius(), 0.03) << "the transition's fixed fore radius";
    EXPECT_TRUE(tube.usesNextCompAutomatic());
    EXPECT_FALSE(tube.usesPreviousCompAutomatic());
    EXPECT_FALSE(tail.canUsePreviousCompAutomatic());
    tail.setForeRadiusAutomatic(true, true);
    EXPECT_FALSE(tail.isForeRadiusAutomatic());
}

TEST(Transition, RadiusSettersClampTheThicknessAndTheShoulder)
{
    Transition transition;
    transition.setForeRadius(0.05);
    transition.setAftRadius(0.05);
    transition.setThickness(0.04);
    transition.setForeShoulderLength(0.02);
    transition.setForeShoulderRadius(0.045);
    transition.setForeShoulderThickness(0.01);

    transition.setForeRadius(0.03);
    EXPECT_EQ(transition.getThickness(), 0.04) << "still within the aft radius";
    EXPECT_EQ(transition.getForeShoulderRadius(), 0.03) << "clamped to the fore radius";
    EXPECT_EQ(transition.getForeShoulderThickness(), 0.01);

    transition.setAftRadius(0.02);
    EXPECT_EQ(transition.getThickness(), 0.03) << "beyond both radii: the larger one";

    transition.setForeRadius(0.01, false);
    EXPECT_EQ(transition.getThickness(), 0.03) << "no clamping";
    EXPECT_EQ(transition.getForeShoulderRadius(), 0.03) << "no clamping";

    transition.setForeRadius(-1);
    EXPECT_EQ(transition.getForeRadius(), 0.0);
}

TEST(Transition, ShoulderLengthSetsUpAndResetsTheShoulder)
{
    Rocket      rocket;
    auto&       stage = rocket.addChild(std::make_unique<AxialStage>());
    const auto& tube  = stage.addChild(std::make_unique<BodyTube>(0.1, 0.03, 0.001));
    auto&       tail  = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();

    // The first non-zero length takes the previous component's radius less its wall.
    tail.setForeShoulderLength(0.02);
    EXPECT_EQ(tail.getForeShoulderLength(), 0.02);
    EXPECT_NEAR(tail.getForeShoulderRadius(), 0.03 - 0.001, 1e-15);
    EXPECT_EQ(tail.getForeShoulderThickness(), tube.getThickness());
    EXPECT_TRUE(tail.isForeShoulderCapped());

    // A later length keeps the shoulder.
    tail.setForeShoulderRadius(0.025);
    tail.setForeShoulderLength(0.03);
    EXPECT_EQ(tail.getForeShoulderRadius(), 0.025);

    // A zero length resets it.
    tail.setForeShoulderLength(0);
    EXPECT_EQ(tail.getForeShoulderLength(), 0.0);
    EXPECT_EQ(tail.getForeShoulderRadius(), 0.0);
    EXPECT_EQ(tail.getForeShoulderThickness(), 0.0);
    EXPECT_FALSE(tail.isForeShoulderCapped());

    // Without a next component the aft shoulder takes the default radius less this wall, clamped
    // to the aft radius, and this wall.
    tail.setAftRadius(0.02);
    tail.setAftShoulderLength(0.01);
    EXPECT_EQ(tail.getAftShoulderRadius(), 0.02);
    EXPECT_EQ(tail.getAftShoulderThickness(), tail.getThickness());
    EXPECT_TRUE(tail.isAftShoulderCapped());
}

TEST(Transition, ComponentBoundsIncludeTheShoulders)
{
    Transition transition;
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.03);
    transition.setLength(0.1);
    EXPECT_EQ(transition.getComponentBounds().size(), 24U);

    transition.setForeShoulderLength(0.02);
    transition.setForeShoulderRadius(0.015);
    transition.setAftShoulderLength(0.001);  // not longer than kMinFeature
    std::vector<Coordinate> bounds = transition.getComponentBounds();
    ASSERT_EQ(bounds.size(), 28U);
    EXPECT_EQ(bounds[24], Coordinate(-0.02, -0.015, -0.015));
    EXPECT_EQ(bounds[26], Coordinate(-0.02, 0.015, 0.015));

    transition.setAftShoulderLength(0.03);
    transition.setAftShoulderRadius(0.025);
    bounds = transition.getComponentBounds();
    ASSERT_EQ(bounds.size(), 32U);
    EXPECT_EQ(bounds[30], Coordinate(0.1 + 0.03, 0.025, 0.025));
}

TEST(Transition, Compatibility)
{
    const Transition transition;
    EXPECT_TRUE(transition.allowsChildren());
    EXPECT_TRUE(transition.isCompatible(ComponentKind::INNER_TUBE));
    EXPECT_TRUE(transition.isCompatible(ComponentKind::MASS_COMPONENT));
    EXPECT_TRUE(transition.isCompatible(ComponentKind::PARACHUTE));
    EXPECT_TRUE(transition.isCompatible(ComponentKind::FREEFORM_FIN_SET));
    EXPECT_FALSE(transition.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_FALSE(transition.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_FALSE(transition.isCompatible(ComponentKind::POD_SET));
    EXPECT_FALSE(transition.isCompatible(ComponentKind::LAUNCH_LUG));
}

TEST(Transition, SettersFireTheirEvents)
{
    Rocket rocket;
    auto&  stage      = rocket.addChild(std::make_unique<AxialStage>());
    auto&  transition = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();
    int        lastType   = 0;
    int        count      = 0;
    const auto connection = ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&](const ComponentChangeEvent& e) {
            lastType = e.getType();
            ++count;
        })};

    transition.setShapeType(TransitionShape::OGIVE);
    EXPECT_EQ(lastType, ComponentChangeEvent::kBothChange);
    transition.setForeShoulderCapped(true);
    EXPECT_EQ(lastType, ComponentChangeEvent::kMassChange);
    transition.setFinish(Finish::SMOOTH);
    EXPECT_EQ(lastType,
              ComponentChangeEvent::kAerodynamicChange | ComponentChangeEvent::kGraphicChange);

    count = 0;
    transition.setShapeType(TransitionShape::OGIVE);
    transition.setForeShoulderCapped(true);
    transition.setFinish(Finish::SMOOTH);
    transition.setShapeParameter(transition.getShapeParameter());
    EXPECT_EQ(count, 0) << "unchanged values fire nothing";
}

TEST(Transition, CopiesKeepTheShapeAndShoulders)
{
    Transition transition;
    transition.setShapeType(TransitionShape::HAACK);
    transition.setShapeParameter(0.2);
    transition.setForeRadius(0.02);
    transition.setAftRadius(0.03);
    transition.setAftShoulderLength(0.02);
    transition.setAftShoulderRadius(0.028);
    transition.setFilled(true);
    transition.getInsideColorComponentHandler().setEdgesSameAsInside(true);

    const std::unique_ptr<RocketComponent> copy = transition.copyWithOriginalId();
    const auto*                            tr   = dynamic_cast<const Transition*>(copy.get());
    ASSERT_NE(tr, nullptr);
    EXPECT_EQ(tr->getId(), transition.getId());
    EXPECT_EQ(tr->getShapeType(), TransitionShape::HAACK);
    EXPECT_EQ(tr->getShapeParameter(), 0.2);
    EXPECT_TRUE(tr->isClipped());
    EXPECT_EQ(tr->getForeRadius(), 0.02);
    EXPECT_EQ(tr->getAftRadius(), 0.03);
    EXPECT_EQ(tr->getAftShoulderLength(), 0.02);
    EXPECT_EQ(tr->getAftShoulderRadius(), 0.028);
    EXPECT_TRUE(tr->isFilled());
    EXPECT_TRUE(tr->getInsideColorComponentHandler().isEdgesSameAsInside());
    EXPECT_EQ(tr->getComponentVolume(), transition.getComponentVolume());
    EXPECT_EQ(tr->getComponentCG(), transition.getComponentCG());

    const std::unique_ptr<RocketComponent> fresh = transition.copyWithNewIds();
    EXPECT_NE(fresh->getId(), transition.getId());
    EXPECT_EQ(fresh->kind(), ComponentKind::TRANSITION);
}

TEST(Transition, ANoseConeCannotBeSlicedIntoATransition)
{
    // The copy constructor is protected; copies come from cloneShallow(), which keeps the class.
    static_assert(!std::is_copy_constructible_v<Transition>);
    static_assert(std::is_copy_constructible_v<NoseCone>);

    NoseCone nose(TransitionShape::OGIVE, 0.1, 0.02);
    nose.setFlipped(true);
    const std::unique_ptr<RocketComponent> copy = nose.copyWithOriginalId();
    EXPECT_EQ(copy->kind(), ComponentKind::NOSE_CONE);
    const auto& copiedNose = dynamic_cast<const NoseCone&>(*copy);
    EXPECT_TRUE(copiedNose.isFlipped());
    EXPECT_EQ(copiedNose.getForeRadius(), 0.02);
    EXPECT_EQ(copiedNose.getAftRadius(), 0.0);
}

/// A rocket with one stage holding a transition with fixed radii 0.02 (fore) and 0.04 (aft).
class TransitionInStageTest : public ::testing::Test
{
protected:
    TransitionInStageTest()
    {
        m_stage      = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_transition = &m_stage->addChild(std::make_unique<Transition>());
        m_transition->setForeRadius(0.02);
        m_transition->setAftRadius(0.04);
        m_rocket.enableEvents();
    }

    Rocket      m_rocket;
    AxialStage* m_stage{nullptr};
    Transition* m_transition{nullptr};
};

TEST_F(TransitionInStageTest, BoundingRadiusAndReferenceLengthReadTheRadii)
{
    EXPECT_EQ(m_stage->getBoundingRadius(), 0.04);
    EXPECT_EQ(m_rocket.getReferenceType(), ReferenceType::MAXIMUM);
    EXPECT_EQ(m_rocket.getSelectedConfiguration().getReferenceLength(), 0.08);
    m_rocket.setReferenceType(ReferenceType::NOSECONE);
    EXPECT_EQ(m_rocket.getSelectedConfiguration().getReferenceLength(), 0.04);

    // A nose cone is a transition.
    m_stage->addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.05), 0);
    EXPECT_EQ(m_stage->getBoundingRadius(), 0.05);
    EXPECT_EQ(m_rocket.getSelectedConfiguration().getReferenceLength(), 0.1)
        << "the nose cone's aft radius: its fore radius is 0";
}

TEST_F(TransitionInStageTest, ANaNLengthKeepsTheRadii)
{
    // Java's setLength() keeps a NaN (Math.max), and the bounding radius and the reference
    // length read the fore and aft radii, not the profile at the ends (which is NaN then).
    m_transition->setLength(std::numeric_limits<double>::quiet_NaN());
    ASSERT_TRUE(std::isnan(m_transition->getLength()));
    EXPECT_EQ(m_stage->getBoundingRadius(), 0.04);
    EXPECT_EQ(
        QtRocket::getReferenceLength(ReferenceType::MAXIMUM, m_rocket.getSelectedConfiguration()),
        0.08);
    EXPECT_EQ(
        QtRocket::getReferenceLength(ReferenceType::NOSECONE, m_rocket.getSelectedConfiguration()),
        0.04);
}

}  // namespace
