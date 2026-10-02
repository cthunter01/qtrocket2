// RecoveryDevice, through Parachute and Streamer: the drag coefficient, the drogue flag, the
// surface material and the deployment configurations. OpenRocket has no JUnit test of the class
// of its own.

#include "QtRocket/rocket/RecoveryDevice.h"

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfigurableComponent.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "rocket/TestBodyComponent.h"
#include "rocket/TestRockets.h"

namespace
{

using QtRocket::AxialStage;
using QtRocket::BugError;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentChangeSignal;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::DeploymentConfiguration;
using QtRocket::FlightConfigurableComponent;
using QtRocket::FlightConfigurationId;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::Parachute;
using QtRocket::RecoveryDevice;
using QtRocket::Rocket;
using QtRocket::Streamer;
using QtRocket::TypedPropertyMap;
using QtRocket::Test::TestBodyComponent;
using DeployEvent = DeploymentConfiguration::DeployEvent;

/// The properties of a streamer preset (strip 1 m by 0.1 m) without a material.
[[nodiscard]] TypedPropertyMap streamerProps()
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kType, ComponentPresetType::STREAMER);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    props.put(ComponentPreset::kPartNo, "partno");
    props.put(ComponentPreset::kLength, 1.0);
    props.put(ComponentPreset::kWidth, 0.1);
    return props;
}

/// The factory's preset of @p props (with an empty material storage).
[[nodiscard]] ComponentPreset makePreset(const TypedPropertyMap& props)
{
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

/// A streamer preset (strip 1 m by 0.1 m) with @p material.
[[nodiscard]] ComponentPreset streamerPreset(const Material& material)
{
    TypedPropertyMap props = streamerProps();
    props.put(ComponentPreset::kMaterial, material);
    return makePreset(props);
}

/// A rocket with a stage holding a body tube stand-in with a parachute; events enabled and
/// recorded.
class RecoveryDeviceTest : public ::testing::Test
{
protected:
    RecoveryDeviceTest()
    {
        auto& stage = m_rocket.addChild(std::make_unique<AxialStage>());
        auto& body  = stage.addChild(TestBodyComponent::make(0.5, 0.03));
        m_chute     = &body.addChild(std::make_unique<Parachute>());
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    Parachute*                              m_chute{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

TEST(RecoveryDevice, Defaults)
{
    const Streamer        streamer;
    const RecoveryDevice& device = streamer;
    EXPECT_TRUE(device.isCDAutomatic());
    EXPECT_FALSE(device.isDrogue());
    EXPECT_EQ(device.getMaterial().getName(), "Ripstop nylon");
    EXPECT_EQ(device.getMaterial().getType(), Material::Type::SURFACE);
    EXPECT_EQ(device.getMaterial().getDensity(), 0.067);
    EXPECT_FALSE(device.isAerodynamic());
    EXPECT_TRUE(device.isMassive());

    const DeploymentConfiguration& deployment = device.getDeploymentConfigurations().getDefault();
    EXPECT_EQ(deployment.getDeployEvent(), DeployEvent::EJECTION);
    EXPECT_EQ(deployment.getDeployAltitude(), 200.0);
    EXPECT_EQ(deployment.getDeployDelay(), 0.0);
    EXPECT_EQ(device.getDeploymentConfigurations().size(), 0U);
}

TEST(RecoveryDevice, AutomaticCdIsStored)
{
    Streamer     streamer;
    const double estimate = streamer.getComponentCD(0);
    EXPECT_EQ(streamer.getCD(), estimate);
    EXPECT_EQ(streamer.getCD(0.5), estimate);

    // A manual CD keeps its value; switching back to automatic recomputes it.
    streamer.setCD(1.1);
    EXPECT_FALSE(streamer.isCDAutomatic());
    EXPECT_EQ(streamer.getCD(), 1.1);
    streamer.setCDAutomatic(true);
    EXPECT_EQ(streamer.getCD(), estimate);
    streamer.setCDAutomatic(false);
    EXPECT_EQ(streamer.getCD(), estimate);  // the stored value
}

TEST_F(RecoveryDeviceTest, CdSettersFire)
{
    m_types.clear();
    m_chute->setCDAutomatic(true);  // unchanged
    EXPECT_TRUE(m_types.empty());
    m_chute->setCD(0.8);  // equal, but it makes the CD manual
    EXPECT_FALSE(m_chute->isCDAutomatic());
    m_chute->setCD(0.8);  // manual and equal: nothing
    m_chute->setCDAutomatic(true);
    EXPECT_EQ(m_types, std::vector<int>(2, ComponentChangeEvent::kAerodynamicChange));
}

TEST_F(RecoveryDeviceTest, DrogueFlag)
{
    m_types.clear();
    m_chute->setDrogue(false);
    EXPECT_TRUE(m_types.empty());
    m_chute->setDrogue(true);
    EXPECT_TRUE(m_chute->isDrogue());
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kNonFunctionalChange});
}

TEST_F(RecoveryDeviceTest, SurfaceMaterial)
{
    EXPECT_THROW(
        m_chute->setMaterial(Material::newMaterial(Material::Type::BULK, "bulk", 1.0, true)),
        BugError);
    EXPECT_THROW(
        m_chute->setMaterial(Material::newMaterial(Material::Type::LINE, "line", 1.0, true)),
        BugError);

    m_types.clear();
    m_chute->setMaterial(m_chute->getMaterial());
    EXPECT_TRUE(m_types.empty());
    const Material silk = Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true);
    m_chute->setMaterial(silk);
    EXPECT_EQ(m_types, std::vector<int>{ComponentChangeEvent::kMassChange});
    EXPECT_EQ(m_chute->getMaterial(), silk);

    // The canopy material comes first, then the line material.
    const std::vector<Material> materials = m_chute->getAllMaterials();
    ASSERT_EQ(materials.size(), 2U);
    EXPECT_EQ(materials[0], silk);
    EXPECT_EQ(materials[1], m_chute->getLineMaterial());
}

TEST(RecoveryDevice, MassIsAreaTimesSurfaceDensity)
{
    Streamer streamer;
    streamer.setStripLength(2.0);
    streamer.setStripWidth(0.1);
    EXPECT_NEAR(streamer.getComponentMass(), 0.2 * 0.067, 1e-15);
}

TEST(RecoveryDevice, DeploymentConfigurations)
{
    Parachute                   chute;
    const FlightConfigurationId fcid  = QtRocket::Test::testFcid(0);
    const FlightConfigurationId other = QtRocket::Test::testFcid(1);

    DeploymentConfiguration apogee;
    apogee.setDeployEvent(DeployEvent::APOGEE);
    apogee.setDeployDelay(1.5);
    chute.getDeploymentConfigurations().set(fcid, apogee);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(fcid).getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(other).getDeployEvent(),
              DeployEvent::EJECTION);

    FlightConfigurableComponent& configurable = chute;
    configurable.copyFlightConfiguration(fcid, other);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(other).getDeployEvent(), DeployEvent::APOGEE);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(other).getDeployDelay(), 1.5);
    configurable.reset(fcid);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(fcid).getDeployEvent(),
              DeployEvent::EJECTION);
    EXPECT_EQ(chute.getDeploymentConfigurations().size(), 1U);
}

TEST(RecoveryDevice, CopiesCloneTheDeploymentsAndTheDrogueFlag)
{
    Parachute chute;
    chute.setDrogue(true);
    chute.setCD(1.3);
    const FlightConfigurationId fcid = QtRocket::Test::testFcid(2);
    DeploymentConfiguration     altitude;
    altitude.setDeployEvent(DeployEvent::ALTITUDE);
    altitude.setDeployAltitude(150);
    chute.getDeploymentConfigurations().set(fcid, altitude);

    const std::unique_ptr<Parachute> copy =
        QtRocket::componentCast<Parachute>(chute.copyWithOriginalId());
    ASSERT_NE(copy, nullptr);
    EXPECT_TRUE(copy->isDrogue());
    EXPECT_FALSE(copy->isCDAutomatic());
    EXPECT_EQ(copy->getCD(), 1.3);
    EXPECT_EQ(copy->getDeploymentConfigurations().get(fcid).getDeployAltitude(), 150.0);
    // A clone: editing the copy leaves the original alone.
    copy->getDeploymentConfigurations().get(fcid).setDeployAltitude(300);
    EXPECT_EQ(chute.getDeploymentConfigurations().get(fcid).getDeployAltitude(), 150.0);
}

TEST(RecoveryDevice, PresetMaterialNeedsALongEnoughDescription)
{
    // Java tests Material.toString().length() > 12 ("NEED a better way to set preset if field
    // is empty"): a shorter one gives the default surface material.
    const Material silk = Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true);
    ASSERT_GT(QtRocket::Strings::javaLength(silk.toString()), 12U);
    const ComponentPreset withSilk = streamerPreset(silk);
    Streamer              streamer;
    streamer.loadPreset(&withSilk);
    EXPECT_EQ(streamer.getMaterial(), silk);

    const Material unnamed = Material::newMaterial(Material::Type::SURFACE, "", 0.001, true);
    ASSERT_LE(QtRocket::Strings::javaLength(unnamed.toString()), 12U);
    const ComponentPreset withUnnamed = streamerPreset(unnamed);
    streamer.loadPreset(&withUnnamed);
    EXPECT_EQ(streamer.getMaterial().getName(), "Ripstop nylon");
}

TEST(RecoveryDevice, PresetMaterialMustBeASurfaceMaterial)
{
    // Java accepts the preset and loading it throws a ClassCastException (the cast to
    // Material.Surface); here the factory refuses it, a recoverable error (see
    // ComponentPresetFactory), so loadFromPreset() can rely on the type.
    TypedPropertyMap props = streamerProps();
    props.put(ComponentPreset::kMaterial,
              Material::newMaterial(Material::Type::BULK, "Some bulk material", 1, true));
    const QtRocket::MaterialStorage         materials;
    const QtRocket::Result<ComponentPreset> preset =
        ComponentPresetFactory::create(props, materials);
    ASSERT_FALSE(preset.has_value());
    EXPECT_TRUE(preset.error().message.contains(
        R"(Material "Some bulk material" is not a SURFACE material)"))
        << preset.error().message;
}

TEST(RecoveryDevice, PresetWithoutAMaterialLeavesTheDefaultMaterial)
{
    // Java's defaultMaterial, which its constructor reads from the preferences.
    Streamer streamer;
    EXPECT_EQ(streamer.getDefaultMaterial().getName(), "Ripstop nylon");
    EXPECT_EQ(streamer.getDefaultMaterial(), streamer.getMaterial());
    EXPECT_THROW(
        streamer.setDefaultMaterial(Material::newMaterial(Material::Type::BULK, "b", 1.0, true)),
        BugError);
    EXPECT_THROW(
        streamer.setDefaultMaterial(Material::newMaterial(Material::Type::LINE, "l", 1.0, true)),
        BugError);

    const Material mylar = Material::newMaterial(Material::Type::SURFACE, "Mylar", 0.021, true);
    streamer.setDefaultMaterial(mylar);
    EXPECT_EQ(streamer.getDefaultMaterial(), mylar);
    EXPECT_EQ(streamer.getMaterial().getName(), "Ripstop nylon");  // the material stays

    // A preset without a MATERIAL, and one whose material's description is too short.
    const ComponentPreset without = makePreset(streamerProps());
    streamer.setMaterial(Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true));
    streamer.loadPreset(&without);
    EXPECT_EQ(streamer.getMaterial(), mylar);
    const ComponentPreset unnamed =
        streamerPreset(Material::newMaterial(Material::Type::SURFACE, "", 0.001, true));
    streamer.setMaterial(Material::newMaterial(Material::Type::SURFACE, "Silk", 0.05, true));
    streamer.loadPreset(&unnamed);
    EXPECT_EQ(streamer.getMaterial(), mylar);

    // A copy keeps it, as Java's clone keeps the final field.
    const std::unique_ptr<Streamer> copy =
        QtRocket::componentCast<Streamer>(streamer.copyWithNewIds());
    ASSERT_NE(copy, nullptr);
    EXPECT_EQ(copy->getDefaultMaterial(), mylar);
}

TEST_F(RecoveryDeviceTest, SetDefaultMaterialFiresNothingAndKeepsThePreset)
{
    const ComponentPreset preset =
        streamerPreset(Material::newMaterial(Material::Type::SURFACE, "Silk (preset)", 0.05, true));
    Streamer& streamer = m_chute->getParent()->addChild(std::make_unique<Streamer>());
    streamer.loadPreset(&preset);
    m_types.clear();
    streamer.setDefaultMaterial(
        Material::newMaterial(Material::Type::SURFACE, "Mylar", 0.021, true));
    m_chute->setDefaultMaterial(
        Material::newMaterial(Material::Type::SURFACE, "Mylar", 0.021, true));
    EXPECT_TRUE(m_types.empty());
    EXPECT_EQ(streamer.getPresetComponent(), &preset);
    EXPECT_EQ(streamer.getMaterial().getName(), "Silk (preset)");
}

}  // namespace
