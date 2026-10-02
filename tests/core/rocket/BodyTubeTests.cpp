#include "QtRocket/rocket/BodyTube.h"

#include <memory>
#include <numbers>
#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BoundingBox.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BodyTube;
using QtRocket::BoundingBox;
using QtRocket::BugError;
using QtRocket::ClusterConfiguration;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentKind;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Coordinate;
using QtRocket::Finish;
using QtRocket::FlightConfigurationId;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::MotorConfiguration;
using QtRocket::MotorMount;
using QtRocket::NoseCone;
using QtRocket::ParallelStage;
using QtRocket::PodSet;
using QtRocket::RadiusMethod;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::SymmetricComponent;
using QtRocket::Transition;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using QtRocket::MathUtil::pow2;

// ===================================================================== BodyTubeComponentTests

/// BodyTubeComponentTests: a BODY_TUBE preset (length 2, outer diameter 2, inner diameter 1,
/// mass 100).
class BodyTubePresetTest : public ::testing::Test
{
protected:
    BodyTubePresetTest()
    {
        TypedPropertyMap presetspec;
        presetspec.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
        presetspec.put(ComponentPreset::kManufacturer,
                       Manufacturer::getManufacturer("manufacturer"));
        presetspec.put(ComponentPreset::kPartNo, "partno");
        presetspec.put(ComponentPreset::kLength, 2.0);
        presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
        presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
        presetspec.put(ComponentPreset::kMass, 100.0);
        auto created = ComponentPresetFactory::create(presetspec, m_materials);
        if (!created)
        {
            ADD_FAILURE() << created.error().message;
            return;
        }
        m_preset = std::make_unique<ComponentPreset>(std::move(*created));
    }

    /// A new body tube with the preset loaded.
    [[nodiscard]] std::unique_ptr<BodyTube> loaded() const
    {
        auto bt = std::make_unique<BodyTube>();
        bt->loadPreset(m_preset.get());
        return bt;
    }

    QtRocket::MaterialStorage        m_materials;
    std::unique_ptr<ComponentPreset> m_preset;
};

TEST_F(BodyTubePresetTest, ComponentType)
{
    const BodyTube bt;

    EXPECT_EQ(ComponentPresetType::BODY_TUBE, bt.getPresetType());
}

TEST_F(BodyTubePresetTest, LoadFromPresetIsSane)
{
    const std::unique_ptr<BodyTube> bt = loaded();

    EXPECT_EQ(2.0, bt->getLength());
    EXPECT_EQ(1.0, bt->getOuterRadius());
    EXPECT_EQ(1.0, bt->getAftRadius());
    EXPECT_EQ(0.5, bt->getInnerRadius());

    EXPECT_FALSE(bt->isAftRadiusAutomatic());
    EXPECT_FALSE(bt->isFilled());
    EXPECT_FALSE(bt->isForeRadiusAutomatic());
    EXPECT_FALSE(bt->isOuterRadiusAutomatic());

    EXPECT_EQ(m_preset->get(ComponentPreset::kMaterial), bt->getMaterial());
    EXPECT_NEAR(100.0, bt->getMass(), 0.05);
}

TEST_F(BodyTubePresetTest, ChangeLengthLeavesPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setLength(1.0);
    EXPECT_EQ(m_preset.get(), bt->getPresetComponent());
}

TEST_F(BodyTubePresetTest, ChangeODClearsPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setOuterRadius(2.0);
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

TEST_F(BodyTubePresetTest, ChangeIDClearsPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setInnerRadius(0.75);
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

TEST_F(BodyTubePresetTest, ChangeThicknessClearsPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setThickness(0.1);
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

TEST_F(BodyTubePresetTest, ChangeMaterialClearsPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setMaterial(Material::newMaterial(Material::Type::BULK, "new", 1.0, true));
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

TEST_F(BodyTubePresetTest, ChangeFinishLeavesPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setFinish(Finish::POLISHED);
    EXPECT_EQ(m_preset.get(), bt->getPresetComponent());
}

TEST_F(BodyTubePresetTest, ChangeFillClearsPreset)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setFilled(true);
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

TEST_F(BodyTubePresetTest, ReloadingTheSamePresetDoesNothing)
{
    const std::unique_ptr<BodyTube> bt = loaded();
    bt->setLength(1.0);  // keeps the preset
    bt->loadPreset(m_preset.get());
    EXPECT_EQ(bt->getLength(), 1.0) << "the same preset is not loaded again";
    bt->loadPreset(nullptr);
    EXPECT_EQ(bt->getPresetComponent(), nullptr);
}

// ===================================================================== behaviour beyond JUnit

TEST(BodyTube, Constructors)
{
    const BodyTube automatic;
    EXPECT_EQ(automatic.kind(), ComponentKind::BODY_TUBE);
    EXPECT_EQ(automatic.getComponentName(), "Body Tube");
    EXPECT_EQ(automatic.getLength(), 8 * SymmetricComponent::kDefaultRadius);
    EXPECT_TRUE(automatic.isOuterRadiusAutomatic());
    EXPECT_EQ(automatic.getOuterRadius(), SymmetricComponent::kDefaultRadius)
        << "no neighbour gives a radius";
    EXPECT_EQ(automatic.getThickness(), SymmetricComponent::kDefaultThickness);
    EXPECT_FALSE(automatic.isFilled());
    EXPECT_EQ(automatic.getDisplayOrderSide(), 0);
    EXPECT_EQ(automatic.getDisplayOrderBack(), 1);

    const BodyTube fixed(0.3, 0.02);
    EXPECT_FALSE(fixed.isOuterRadiusAutomatic());
    EXPECT_EQ(fixed.getLength(), 0.3);
    EXPECT_EQ(fixed.getOuterRadius(), 0.02);

    const BodyTube negative(-1, -2);
    EXPECT_EQ(negative.getLength(), 0.0);
    EXPECT_EQ(negative.getOuterRadius(), 0.0);

    const BodyTube filled(0.3, 0.02, true);
    EXPECT_TRUE(filled.isFilled());
    EXPECT_EQ(filled.getInnerRadius(), 0.0);
    EXPECT_EQ(filled.getThickness(), 0.02);

    const BodyTube thick(0.3, 0.02, 0.005);
    EXPECT_FALSE(thick.isFilled());
    EXPECT_EQ(thick.getThickness(), 0.005);
    EXPECT_NEAR(thick.getInnerRadius(), 0.015, 1e-15);

    // An integer wall thickness is a thickness, not a fill flag (BodyTube(l, r, 0)).
    const BodyTube zero(0.3, 0.02, 0);
    EXPECT_FALSE(zero.isFilled());
    EXPECT_EQ(zero.getThickness(), 0.0);
}

TEST(BodyTube, RadiusAndThickness)
{
    BodyTube tube(0.3, 0.02, 0.005);
    tube.setOuterRadius(0.004);
    EXPECT_EQ(tube.getOuterRadius(), 0.004);
    EXPECT_EQ(tube.getThickness(), 0.004) << "the wall is limited to the radius";
    EXPECT_EQ(tube.getInnerRadius(), 0.0);

    tube.setOuterRadius(0.02);
    tube.setInnerRadius(0.018);
    EXPECT_NEAR(tube.getThickness(), 0.002, 1e-15);
    EXPECT_NEAR(tube.getInnerRadius(), 0.018, 1e-15);
    EXPECT_NEAR(tube.getInnerRadius(0.1), 0.018, 1e-15);
    EXPECT_EQ(tube.getMotorMountDiameter(), 2 * tube.getInnerRadius());

    tube.setOuterRadius(-1);
    EXPECT_EQ(tube.getOuterRadius(), 0.0);

    tube.setOuterRadius(0.02);
    tube.setFilled(true);
    EXPECT_EQ(tube.getInnerRadius(0.1), 0.0);
    EXPECT_EQ(tube.getRadius(0.1), 0.02);
    EXPECT_EQ(tube.getForeRadius(), 0.02);
    EXPECT_EQ(tube.getAftRadius(), 0.02);
}

TEST(BodyTube, ClosedFormMassProperties)
{
    BodyTube tube(0.4, 0.02, 0.002);
    tube.setMaterial(Material::newMaterial(Material::Type::BULK, "test", 1000.0, true));

    const double ro     = 0.02;
    const double ri     = 0.018;
    const double volume = (std::numbers::pi * ro * ro * 0.4) - (std::numbers::pi * ri * ri * 0.4);
    EXPECT_NEAR(tube.getComponentVolume(), volume, volume * 1e-12);
    EXPECT_NEAR(tube.getComponentMass(), 1000.0 * volume, volume * 1e-9);
    EXPECT_EQ(tube.getComponentCG(), Coordinate(0.2, 0, 0, tube.getComponentMass()));
    EXPECT_NEAR(tube.getLongitudinalUnitInertia(), ((3 * (pow2(ro) + pow2(ri))) + pow2(0.4)) / 12,
                1e-15);
    EXPECT_NEAR(tube.getRotationalUnitInertia(), (pow2(ri) + pow2(ro)) / 2, 1e-15);
    EXPECT_NEAR(tube.getComponentWetArea(), std::numbers::pi * ro * 2 * 0.4, 1e-15);
    EXPECT_NEAR(tube.getComponentPlanformArea(), ro * 2 * 0.4, 1e-15);
    EXPECT_NEAR(tube.getComponentPlanformCenter(), 0.2, 1e-12);

    const BoundingBox box = tube.getInstanceBoundingBox();
    EXPECT_EQ(box.min(), Coordinate(0, -ro, -ro));
    EXPECT_EQ(box.max(), Coordinate(0.4, ro, ro));

    tube.setFilled(true);
    EXPECT_NEAR(tube.getComponentVolume(), std::numbers::pi * ro * ro * 0.4, 1e-15);
}

TEST(BodyTube, AutomaticRadiusFromThePreviousThenTheNextComponent)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  nose  = stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.03));
    auto&  tube  = stage.addChild(std::make_unique<BodyTube>());
    auto&  tail  = stage.addChild(std::make_unique<Transition>());
    rocket.enableEvents();
    tail.setForeRadius(0.04);
    tail.setAftRadius(0.02);

    EXPECT_EQ(tube.getOuterRadius(), 0.03) << "the nose cone's base";
    EXPECT_TRUE(tube.usesPreviousCompAutomatic());
    EXPECT_FALSE(tube.usesNextCompAutomatic());

    // A nose cone whose base is automatic offers nothing: the tube asks the next component.
    nose.setAftRadiusAutomatic(true);
    EXPECT_EQ(tube.getOuterRadius(), 0.04) << "the transition's fore radius";
    EXPECT_FALSE(tube.usesPreviousCompAutomatic());
    EXPECT_TRUE(tube.usesNextCompAutomatic());
    // The nose cone takes its base from the tube, which takes it from the transition.
    EXPECT_EQ(nose.getAftRadius(), 0.04);

    // With no offer at all, the default radius.
    tail.setForeRadiusAutomatic(true);
    EXPECT_EQ(tube.getOuterRadius(), SymmetricComponent::kDefaultRadius);

    // A fixed radius is offered as it is; an automatic one passes the neighbour's offer on.
    tube.setOuterRadius(0.05);
    EXPECT_EQ(tube.getFrontAutoRadius(), 0.05);
    EXPECT_EQ(tube.getRearAutoRadius(), 0.05);
    tube.setOuterRadiusAutomatic(true);
    nose.setAftRadius(0.035);
    EXPECT_EQ(tube.getFrontAutoRadius(), 0.035);
    EXPECT_EQ(tube.getRearAutoRadius(), -1.0) << "the transition's fore radius is automatic";
}

TEST(BodyTube, AnAutomaticTubeAloneOffersNothing)
{
    Rocket      rocket;
    auto&       stage = rocket.addChild(std::make_unique<AxialStage>());
    const auto& tube  = stage.addChild(std::make_unique<BodyTube>());
    rocket.enableEvents();
    EXPECT_EQ(tube.getFrontAutoRadius(), -1.0);
    EXPECT_EQ(tube.getRearAutoRadius(), -1.0);
    // Without neighbours and before any automatic radius was taken, the reference matches the
    // missing neighbours (Java: null == null).
    EXPECT_TRUE(tube.usesPreviousCompAutomatic());
    EXPECT_TRUE(tube.usesNextCompAutomatic());
}

TEST(BodyTube, ARocketCopyDoesNotReferToTheOriginalsNeighbours)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.03));
    const auto& tube = stage.addChild(std::make_unique<BodyTube>());
    rocket.enableEvents();
    EXPECT_EQ(tube.getOuterRadius(), 0.03);
    EXPECT_TRUE(tube.usesPreviousCompAutomatic());

    // Java's copy keeps a reference to the original's neighbour, which is none of the copy's.
    const std::unique_ptr<Rocket> copy = rocket.copyRocketWithOriginalId();
    auto& copiedTube                   = dynamic_cast<BodyTube&>(copy->getChild(0).getChild(1));
    EXPECT_FALSE(copiedTube.usesPreviousCompAutomatic());
    EXPECT_FALSE(copiedTube.usesNextCompAutomatic());
    // Reading the radius takes it from the copy's own neighbour.
    EXPECT_EQ(copiedTube.getOuterRadius(), 0.03);
    EXPECT_TRUE(copiedTube.usesPreviousCompAutomatic());
}

TEST(BodyTube, APastedCopyRefersToTheOriginalsReferenceComponent)
{
    // Java's clone keeps the reference to the very component the original took its radius from,
    // so a copy pasted next to that component takes its radius from it before the copy's radius
    // was ever read.
    Rocket rocket;
    auto&  stage  = rocket.addChild(std::make_unique<AxialStage>());
    auto&  nose   = stage.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 0.1, 0.02));
    const auto& a = stage.addChild(std::make_unique<BodyTube>());
    rocket.enableEvents();
    EXPECT_EQ(a.getOuterRadius(), 0.02) << "a takes its radius from the nose cone";

    // Copy and paste (Java's copy()) between the nose cone and the original.
    auto& b = dynamic_cast<BodyTube&>(stage.addChild(a.copyWithNewIds(), 1));
    EXPECT_TRUE(b.usesPreviousCompAutomatic());
    EXPECT_FALSE(nose.canUseNextCompAutomatic());

    // So the nose cone's aft radius cannot become automatic: it stays fixed at 0.02.
    nose.setAftRadiusAutomatic(true, true);
    EXPECT_FALSE(nose.isAftRadiusAutomatic());
    EXPECT_EQ(nose.getAftRadius(), 0.02);
    EXPECT_EQ(b.getOuterRadius(), 0.02);
    EXPECT_EQ(a.getOuterRadius(), 0.02);
}

TEST(BodyTube, ACopyHasItsOwnIdentity)
{
    const BodyTube tube(0.1, 0.01);
    const BodyTube other(0.1, 0.01);
    EXPECT_NE(tube.getIdentity(), other.getIdentity());

    // Java's object identity: a copy with the original id is still another object.
    const std::unique_ptr<RocketComponent> copy = tube.copyWithOriginalId();
    EXPECT_EQ(copy->getId(), tube.getId());
    EXPECT_NE(dynamic_cast<const SymmetricComponent&>(*copy).getIdentity(), tube.getIdentity());
}

TEST(BodyTube, Compatibility)
{
    const BodyTube tube;
    EXPECT_TRUE(tube.allowsChildren());
    EXPECT_TRUE(tube.isCompatible(ComponentKind::PARALLEL_STAGE));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::POD_SET));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::INNER_TUBE));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::CENTERING_RING));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::PARACHUTE));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::TRAPEZOID_FIN_SET));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::LAUNCH_LUG));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::RAIL_BUTTON));
    EXPECT_TRUE(tube.isCompatible(ComponentKind::TUBE_FIN_SET));
    EXPECT_FALSE(tube.isCompatible(ComponentKind::BODY_TUBE));
    EXPECT_FALSE(tube.isCompatible(ComponentKind::NOSE_CONE));
    EXPECT_FALSE(tube.isCompatible(ComponentKind::TRANSITION));
    EXPECT_FALSE(tube.isCompatible(ComponentKind::AXIAL_STAGE));
    EXPECT_FALSE(tube.isCompatible(ComponentKind::ROCKET));
}

/// A rocket with a configuration (TEST_FCID_0) and a stage holding a body tube 0.3 m long,
/// radius 0.012 m, wall 1 mm; events enabled and the last event type recorded.
class BodyTubeMountTest : public ::testing::Test
{
protected:
    BodyTubeMountTest()
    {
        m_rocket.createFlightConfiguration(m_fcid);
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        m_tube      = &stage.addChild(std::make_unique<BodyTube>(0.3, 0.012, 0.001));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_lastType = e.getType(); });
    }

    /// Gives the tube a C6 motor in m_fcid.
    void mountMotor()
    {
        MotorConfiguration config{*m_tube, m_fcid};
        config.setMotor(QtRocket::Test::motorC6());
        m_tube->setMotorConfig(std::move(config), m_fcid);
    }

    Rocket                m_rocket;
    FlightConfigurationId m_fcid =
        FlightConfigurationId::fromString("d010716e-ce0e-469d-ae46-190f3653ebbf");
    BodyTube*                               m_tube{nullptr};
    int                                     m_lastType{0};
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST_F(BodyTubeMountTest, ANewTubeIsNoMotorMount)
{
    EXPECT_FALSE(m_tube->isMotorMount());
    EXPECT_FALSE(m_tube->hasMotor());
    EXPECT_EQ(m_tube->getMotorCount(), 1);
    EXPECT_EQ(&m_tube->getClusterConfiguration(), &ClusterConfiguration::single());
    const MotorMount& mount = *m_tube;
    EXPECT_EQ(&mount.getClusterConfiguration(), &ClusterConfiguration::single())
        << "MotorMount's virtual";
    EXPECT_EQ(m_tube->getMotorCountIncludingAssemblyCopies(), 1);
    EXPECT_EQ(m_tube->getInstanceCount(), 1);
}

TEST_F(BodyTubeMountTest, SettersFireTheirEvents)
{
    m_tube->setMotorMount(true);
    EXPECT_TRUE(m_tube->isMotorMount());
    EXPECT_EQ(m_lastType, ComponentChangeEvent::kMotorChange);

    m_tube->setMotorOverhang(0.01);
    EXPECT_EQ(m_tube->getMotorOverhang(), 0.01);
    EXPECT_EQ(m_lastType, ComponentChangeEvent::kBothChange);
}

TEST_F(BodyTubeMountTest, MotorPosition)
{
    EXPECT_THROW(static_cast<void>(m_tube->getMotorPosition(m_fcid)), BugError) << "no motor";

    m_tube->setMotorOverhang(0.01);
    mountMotor();
    EXPECT_TRUE(m_tube->hasMotor());
    EXPECT_TRUE(m_tube->isMotorMount()) << "setMotorConfig() makes the tube a mount";
    EXPECT_EQ(m_tube->getMotorPosition(m_fcid), Coordinate(0.3 - 0.070 + 0.01));
    EXPECT_EQ(&m_tube->getMotorConfig(m_fcid).getMount(), static_cast<const MotorMount*>(m_tube));
}

TEST_F(BodyTubeMountTest, AConfigurationOfAnotherMountIsRefused)
{
    BodyTube                 other(0.1, 0.01);
    const MotorConfiguration foreign{other, m_fcid};
    EXPECT_THROW(m_tube->setMotorConfig(foreign, m_fcid), BugError);
    EXPECT_FALSE(m_tube->hasMotor());
}

TEST_F(BodyTubeMountTest, ForgettingTheMotorKeepsTheMount)
{
    mountMotor();
    m_tube->setMotorConfig(std::nullopt, m_fcid);
    EXPECT_FALSE(m_tube->hasMotor());
    EXPECT_TRUE(m_tube->isMotorMount());
}

TEST(BodyTube, MotorsCountTheAssemblyInstances)
{
    Rocket rocket;
    auto&  stage    = rocket.addChild(std::make_unique<AxialStage>());
    auto&  core     = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    auto&  boosters = core.addChild(std::make_unique<ParallelStage>());
    boosters.setInstanceCount(3);
    const auto& pods = boosters.addChild(std::make_unique<BodyTube>(0.3, 0.01));
    rocket.enableEvents();
    EXPECT_EQ(pods.getMotorCountIncludingAssemblyCopies(), 3);
}

TEST(BodyTube, TheAssemblyMotorCountWrapsAroundAsJavasInt)
{
    // Not in OpenRocket's tests: the instance counts have no upper bound, and Java's int
    // multiplication wraps around (where a C++ signed overflow would be undefined).
    Rocket rocket;
    auto&  stage    = rocket.addChild(std::make_unique<AxialStage>());
    auto&  core     = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    auto&  boosters = core.addChild(std::make_unique<ParallelStage>());
    boosters.setInstanceCount(70000);
    auto& boosterTube = boosters.addChild(std::make_unique<BodyTube>(0.3, 0.01));
    auto& pods        = boosterTube.addChild(std::make_unique<PodSet>());
    pods.setInstanceCount(70000);
    const auto& podTube = pods.addChild(std::make_unique<BodyTube>(0.1, 0.005));
    // 70000 * 70000 = 4 900 000 000, which is 605 032 704 modulo 2^32.
    EXPECT_EQ(podTube.getMotorCountIncludingAssemblyCopies(), 605032704);
}

TEST(BodyTube, CopiesHaveTheirOwnMotorConfigurations)
{
    const FlightConfigurationId fcid =
        FlightConfigurationId::fromString("f41bee5b-ebb8-4d92-bce7-53001577a313");
    BodyTube tube(0.3, 0.012);
    tube.setMotorMount(true);
    tube.setMotorOverhang(0.005);
    MotorConfiguration config{tube, fcid};
    config.setMotor(QtRocket::Test::motorA8());
    tube.setMotorConfig(std::move(config), fcid);

    const std::unique_ptr<RocketComponent> copy = tube.copyWithOriginalId();
    const auto&                            bt   = dynamic_cast<const BodyTube&>(*copy);
    EXPECT_TRUE(bt.isMotorMount());
    EXPECT_TRUE(bt.hasMotor());
    EXPECT_EQ(bt.getMotorOverhang(), 0.005);
    EXPECT_EQ(&bt.getMotorConfig(fcid).getMount(), static_cast<const MotorMount*>(&bt));
    EXPECT_EQ(&bt.getDefaultMotorConfig().getMount(), static_cast<const MotorMount*>(&bt));
    EXPECT_EQ(bt.getMotorConfig(fcid).getMotor(), tube.getMotorConfig(fcid).getMotor());

    // The motor configurations are copied and reset per flight configuration.
    const FlightConfigurationId other =
        FlightConfigurationId::fromString("3e8d1280-53c2-4234-89a7-de215ef5cd69");
    tube.copyFlightConfiguration(fcid, other);
    EXPECT_EQ(tube.getMotorConfigurationSet().size(), 2U);
    tube.reset(other);
    EXPECT_EQ(tube.getMotorConfigurationSet().size(), 1U);
}

TEST(BodyTube, PodsOnTheSurfaceUseTheOuterRadius)
{
    Rocket rocket;
    auto&  stage = rocket.addChild(std::make_unique<AxialStage>());
    auto&  tube  = stage.addChild(std::make_unique<BodyTube>(0.5, 0.03));
    auto&  pods  = tube.addChild(std::make_unique<PodSet>());
    pods.addChild(std::make_unique<BodyTube>(0.2, 0.01));
    rocket.enableEvents();
    pods.setRadius(RadiusMethod::SURFACE, 0);
    EXPECT_NEAR(pods.getRadiusOffset(RadiusMethod::FREE), 0.03 + 0.01, 1e-15);
}

}  // namespace
