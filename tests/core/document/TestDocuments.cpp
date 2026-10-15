#include "document/TestDocuments.h"

#include <array>
#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeCoupler.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/simulation/listeners/AbstractSimulationListener.h"
#include "QtRocket/simulation/listeners/SimulationListener.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Test
{

namespace
{

/// A built motor as a shared constant, or a BugError naming @p what (Java's builder throws).
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> built(Result<ThrustCurveMotor> motor,
                                                            std::string_view         what)
{
    if (!motor.has_value())
    {
        bug(std::string(what) + ": " + motor.error().toString());
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

/// A new rocket named @p name with a stage named @p stageName, which is returned through
/// @p stage (Java: new Rocket(); setName(); new AxialStage(); setName(); addChild()).
[[nodiscard]] std::unique_ptr<Rocket> rocketWithStage(std::string_view name,
                                                      std::string_view stageName,
                                                      AxialStage*&     stage)
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName(name);
    stage = &rocket->addChild(std::make_unique<AxialStage>());
    stage->setName(stageName);
    return rocket;
}

/// The document of @p rocket after rocket.enableEvents(), the end of most makers (Java:
/// rocket.enableEvents(); return OpenRocketDocumentFactory.createDocumentFromRocket(rocket)).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> documentOf(std::unique_ptr<Rocket> rocket)
{
    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

/// A parachute deployed by default at @p event (Java: new Parachute(); new
/// DeploymentConfiguration(); setDeployEvent(); getDeploymentConfigurations().setDefault()).
[[nodiscard]] std::unique_ptr<Parachute> parachuteDeployedAt(
    DeploymentConfiguration::DeployEvent event)
{
    auto                    parachute = std::make_unique<Parachute>();
    DeploymentConfiguration deploymentConfig;
    deploymentConfig.setDeployEvent(event);
    parachute->getDeploymentConfigurations().setDefault(deploymentConfig);
    return parachute;
}

/// The stage, the body tube and the inner tube with getTestMotor() and an ejection delay of 5 s
/// in the selected configuration, named "F12X", of makeTestRocket_v104_withMotor() and
/// _v104_withSimulationData(), on a new rocket named @p name.
[[nodiscard]] std::unique_ptr<Rocket> rocketWithTestMotor(std::string_view name)
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName(name);
    FlightConfiguration&        config = rocket->getSelectedConfiguration();
    const FlightConfigurationId fcid   = config.getFlightConfigurationId();
    config.setName("F12X");

    // make stage
    AxialStage& stage = rocket->addChild(std::make_unique<AxialStage>());
    stage.setName("Stage1");

    // make body tube
    BodyTube& bodyTube = stage.addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    // make inner tube with motor mount flag set
    InnerTube& innerTube = bodyTube.addChild(std::make_unique<InnerTube>());

    // create motor config and add a motor to it, add motor config to inner tube (motor mount)
    addMotor(innerTube, fcid, getTestMotor(), 5);
    return rocket;
}

/// The Falcon 9 Heavy of rocket/TestRockets.h, its rocket handed over.
[[nodiscard]] std::unique_ptr<Rocket> falcon9Heavy()
{
    TestFalcon9Heavy falcon;
    return std::move(falcon.rocket);
}

/// The BODY_TUBE preset of makeTestRocket_v105_withComponentPreset(), made with OpenRocket's
/// materials (Java's ComponentPresetFactory looks the derived material up in the application's
/// database).
[[nodiscard]] std::shared_ptr<const ComponentPreset> v105Preset()
{
    TypedPropertyMap presetspec;
    presetspec.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    presetspec.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("manufacturer"));
    presetspec.put(ComponentPreset::kPartNo, "partno");
    presetspec.put(ComponentPreset::kLength, 2.0);
    presetspec.put(ComponentPreset::kOuterDiameter, 2.0);
    presetspec.put(ComponentPreset::kInnerDiameter, 1.0);
    presetspec.put(ComponentPreset::kMass, 100.0);
    MaterialStorage materials;
    addBuiltinMaterials(materials);
    Result<ComponentPreset> preset = ComponentPresetFactory::create(presetspec, materials);
    if (!preset.has_value())
    {
        // Java: "should never happen"
        bug("the preset of makeTestRocket_v105_withComponentPreset: " + preset.error().toString());
    }
    return std::make_shared<const ComponentPreset>(std::move(*preset));
}

/// The table of testDocumentMakers(): the makers without preferences take them all the same.
constexpr std::array<TestDocumentMaker, 16> kMakers{{
    {.name = "v100", .make = [](Preferences& /*preferences*/) { return makeTestRocketV100(); }},
    {.name = "v101_withFinTabs",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV101WithFinTabs(); }},
    {.name = "v101_withTubeCouplerChild",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV101WithTubeCouplerChild(); }},
    // no version 1.2 file type exists
    // no version 1.3 file type exists
    {.name = "v104_withSimulationData", .make = makeTestRocketV104WithSimulationData},
    {.name = "v104_withMotor",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV104WithMotor(); }},
    {.name = "v105_withComponentPreset",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV105WithComponentPreset(); }},
    {.name = "v105_withCustomExpression",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV105WithCustomExpression(); }},
    {.name = "v105_withLowerStageRecoveryDevice",
     .make =
         [](Preferences& /*preferences*/) {
             return makeTestRocketV105WithLowerStageRecoveryDevice();
         }},
    {.name = "v106_withAppearance",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV106WithAppearance(); }},
    {.name = "v106_withMotorMountIgnitionConfig",
     .make =
         [](Preferences& /*preferences*/) {
             return makeTestRocketV106WithMotorMountIgnitionConfig();
         }},
    {.name = "v106_withRecoveryDeviceDeploymentConfig",
     .make =
         [](Preferences& /*preferences*/) {
             return makeTestRocketV106WithRecoveryDeviceDeploymentConfig();
         }},
    {.name = "v106_withStageSeparationConfig",
     .make =
         [](Preferences& /*preferences*/) {
             return makeTestRocketV106WithStageSeparationConfig();
         }},
    {.name = "v110_withSimulationExtension",
     .make =
         [](Preferences& preferences) {
             return makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript,
                                                              preferences);
         }},
    {.name = "v108_withBoosters",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV108WithBoosters(); }},
    {.name = "v108_withDisabledStage",
     .make = [](Preferences& /*preferences*/) { return makeTestRocketV108WithDisabledStage(); }},
    {.name = "for_estimateFileSize", .make = makeTestRocketForEstimateFileSize},
}};

}  // namespace

// ============================================================================ the motors

std::shared_ptr<const ThrustCurveMotor> getTestMotor()
{
    return built(ThrustCurveMotor::Builder()
                     .setManufacturer(Manufacturer::getManufacturer("A"))
                     .setDesignation("F12X")
                     .setDescription("Desc")
                     .setCaseInfo("info")
                     .setMotorType(Motor::Type::UNKNOWN)
                     .setStandardDelays({})
                     .setDiameter(0.024)
                     .setLength(0.07)
                     .setTimePoints({0, 1, 2})
                     .setThrustPoints({0, 1, 0})
                     .setCGPoints({Coordinate::kNul, Coordinate::kNul, Coordinate::kNul})
                     .setDigest("digestA")
                     .build(),
                 "TestRockets.getTestMotor()");
}

std::shared_ptr<const ThrustCurveMotor> readEstesA8Motor()
{
    const Result<std::vector<std::byte>> a8 = readFile(testDataDir() / "motors" / "Estes_A8.rse");
    if (!a8.has_value())
    {
        ADD_FAILURE() << "Problem in unit test, cannot find Estes_A8.rse";
        return nullptr;
    }
    const Result<std::vector<ThrustCurveMotor::Builder>> builders =
        GeneralMotorLoader().load(*a8, "Estes_A8.rse");
    if (!builders.has_value() || builders->empty())
    {
        ADD_FAILURE() << "Could not load motor";
        return nullptr;
    }
    Result<ThrustCurveMotor> motor = builders->front().build();
    if (!motor.has_value())
    {
        ADD_FAILURE() << motor.error().toString();
        return nullptr;
    }
    return std::make_shared<const ThrustCurveMotor>(std::move(*motor));
}

std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors()
{
    auto database = std::make_unique<ThrustCurveMotorSetDatabase>();
    if (std::shared_ptr<const ThrustCurveMotor> a8 = readEstesA8Motor())
    {
        database->addMotor(std::move(a8));
    }
    Result<ThrustCurveMotor> f12x =
        ThrustCurveMotor::Builder()
            .setManufacturer(Manufacturer::getManufacturer("A"))
            .setDesignation("F12X")
            .setDescription("Desc")
            .setMotorType(Motor::Type::UNKNOWN)
            .setStandardDelays({})
            .setDiameter(0.024)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 1, 0})
            .setCGPoints({Coordinate::kNul, Coordinate::kNul, Coordinate::kNul})
            .setDigest("digestA")
            .build();
    EXPECT_TRUE(f12x.has_value());
    if (f12x.has_value())
    {
        database->addMotor(std::make_shared<const ThrustCurveMotor>(std::move(*f12x)));
    }
    EXPECT_EQ(database->getMotorSets().size(), 2U);
    return database;
}

// ============================================================================ the makers

std::unique_ptr<OpenRocketDocument> makeTestRocketV100()
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName("v100");

    // make stage
    auto stage = std::make_unique<AxialStage>();
    stage->setName("Stage1");

    // make body tube
    stage->addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    rocket->addChild(std::move(stage));
    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV101WithFinTabs()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = rocketWithStage("v101_withFinTabs", "Stage1", stage);

    // make body tube
    BodyTube& bodyTube = stage->addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    // make fins with fin tabs and add to body tube
    auto fins = std::make_unique<TrapezoidFinSet>();
    fins->setFinCount(3);
    fins->setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    fins->setTabHeight(0.25);
    fins->setTabLength(0.25);
    bodyTube.addChild(std::move(fins));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV101WithTubeCouplerChild()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = rocketWithStage("v101_withTubeCouplerChild", "Stage1", stage);

    // make body tube
    BodyTube& bodyTube = stage->addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    // make tube coupler with centering ring, add to stage
    auto tubeCoupler = std::make_unique<TubeCoupler>();
    tubeCoupler->addChild(std::make_unique<CenteringRing>());
    bodyTube.addChild(std::move(tubeCoupler));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV104WithMotor()
{
    return documentOf(rocketWithTestMotor("v104_withMotorConfig"));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV104WithSimulationData(Preferences& preferences)
{
    std::unique_ptr<Rocket>     owned  = rocketWithTestMotor("v104_withSimulationData");
    Rocket&                     rocket = *owned;
    const FlightConfigurationId fcid = rocket.getSelectedConfiguration().getFlightConfigurationId();

    std::unique_ptr<OpenRocketDocument> rocketDoc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(owned));

    // create simulation data
    const auto simulation1 = std::make_shared<Simulation>(rocket, preferences);
    simulation1->setFlightConfigurationId(fcid);

    rocketDoc->addSimulation(simulation1);
    rocketDoc->addSimulation(std::make_shared<Simulation>(rocket, preferences));

    rocket.enableEvents();
    return rocketDoc;
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithCustomExpression()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> owned  = rocketWithStage("v105_withCustomExpression", "Stage1", stage);
    Rocket&                 rocket = *owned;

    // make body tube
    stage->addChild(std::make_unique<BodyTube>());

    std::unique_ptr<OpenRocketDocument> rocketDoc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(owned));

    rocketDoc->addCustomExpression(CustomExpression("name", "symbol", "unit", "expression"));

    rocket.enableEvents();
    return rocketDoc;
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithComponentPreset()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = rocketWithStage("v105_withComponentPreset", "Stage1", stage);

    // make body tube
    auto bodyTube = std::make_unique<BodyTube>();
    bodyTube->loadPreset(v105Preset());
    stage->addChild(std::move(bodyTube));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithLowerStageRecoveryDevice()
{
    AxialStage*             stage1 = nullptr;
    std::unique_ptr<Rocket> rocket =
        rocketWithStage("v105_withLowerStageRecoveryDevice", "Stage1", stage1);

    // make 1st stage body tube
    BodyTube& bodyTube1 = stage1->addChild(std::make_unique<BodyTube>(5, 1, 0.05));

    // make 1st stage recovery device with deployment config in default
    bodyTube1.addChild(
        parachuteDeployedAt(DeploymentConfiguration::DeployEvent::LOWER_STAGE_SEPARATION));

    // make 2nd stage
    AxialStage& stage2 = rocket->addChild(std::make_unique<AxialStage>());
    stage2.setName("Stage2");

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithAppearance()
{
    AxialStage*             stage  = nullptr;
    std::unique_ptr<Rocket> rocket = rocketWithStage("v106_withAppearance", "Stage1", stage);

    // make body tube with an appearance setting
    auto bodyTube = std::make_unique<BodyTube>(12, 1, 0.05);
    bodyTube->setAppearance(Appearance(Color(100, 25, 50), 1));
    stage->addChild(std::move(bodyTube));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithMotorMountIgnitionConfig()
{
    auto rocket = std::make_unique<Rocket>();
    rocket->setName("v106_withwithMotorMountIgnitionConfig");
    const FlightConfigurationId fcid;

    // make stage
    AxialStage& stage = rocket->addChild(std::make_unique<AxialStage>());
    stage.setName("Stage1");

    // make body tube
    BodyTube& bodyTube = stage.addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    // make inner tube with motor mount flag set
    InnerTube& innerTube = bodyTube.addChild(std::make_unique<InnerTube>());

    // make inner tube with motor mount flag set
    MotorConfiguration motorConfig(innerTube, fcid);
    motorConfig.setMotor(getTestMotor());
    innerTube.setMotorConfig(std::move(motorConfig), fcid);

    // set ignition parameters for motor mount (Java sets them on the configuration it stored,
    // which is the one object: here the mount's copy)
    innerTube.getMotorConfig(fcid).setIgnitionDelay(2);

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithRecoveryDeviceDeploymentConfig()
{
    AxialStage*             stage = nullptr;
    std::unique_ptr<Rocket> rocket =
        rocketWithStage("v106_withRecoveryDeviceDeploymentConfig", "Stage1", stage);
    const FlightConfigurationId testFcid = FlightConfigurationId::fromString("testParachute");

    // make body tube
    BodyTube& bodyTube = stage->addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    // make recovery device with deployment config
    auto                    parachute = std::make_unique<Parachute>();
    DeploymentConfiguration deploymentConfig;
    deploymentConfig.setDeployAltitude(1000);
    parachute->getDeploymentConfigurations().set(testFcid, deploymentConfig);
    bodyTube.addChild(std::move(parachute));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithStageSeparationConfig()
{
    AxialStage*             stage1 = nullptr;
    std::unique_ptr<Rocket> rocket =
        rocketWithStage("v106_withStageSeparationConfig", "Stage1", stage1);
    const FlightConfigurationId fcid = FlightConfigurationId::fromString("3SecondDelay");

    // make 1st stage body tube
    BodyTube& bodyTube1 = stage1->addChild(std::make_unique<BodyTube>(5, 1, 0.05));

    // make1st stage recovery device
    bodyTube1.addChild(std::make_unique<Parachute>());

    // set stage separation configuration
    StageSeparationConfiguration stageSepConfig;
    stageSepConfig.setSeparationDelay(3);
    stage1->getSeparationConfigurations().set(fcid, stageSepConfig);

    // make 2nd stage
    AxialStage& stage2 = rocket->addChild(std::make_unique<AxialStage>());
    stage2.setName("Stage2");

    // make 2nd stage body tube
    stage2.addChild(std::make_unique<BodyTube>(12, 1, 0.05));

    return documentOf(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV110WithSimulationExtension(
    std::string_view script, Preferences& preferences)
{
    TestEstesAlphaIII                   alpha;
    Rocket&                             rocket = *alpha.rocket;
    std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    const auto sim = std::make_shared<Simulation>(rocket, preferences);
    const auto ext = std::make_shared<ScriptingExtension>();
    ext->setEnabled(true);
    ext->setLanguage("JavaScript");
    ext->setScript(script);
    sim->getSimulationExtensions().push_back(ext);
    document->addSimulation(sim);

    rocket.enableEvents();
    return document;
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV108WithBoosters()
{
    return OpenRocketDocumentFactory::createDocumentFromRocket(falcon9Heavy());
}

std::unique_ptr<OpenRocketDocument> makeTestRocketV108WithDisabledStage()
{
    std::unique_ptr<Rocket> rocket = falcon9Heavy();
    rocket->getSelectedConfiguration().setStageActive(0, false, false);
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

std::unique_ptr<OpenRocketDocument> makeTestRocketForEstimateFileSize(Preferences& preferences)
{
    AxialStage*             stage1 = nullptr;
    std::unique_ptr<Rocket> owned  = rocketWithStage("for_estimateFileSize", "Stage1", stage1);
    Rocket&                 rocket = *owned;

    // make 1st stage body tube
    BodyTube& bodyTube1 = stage1->addChild(std::make_unique<BodyTube>(5, 1, 0.05));

    auto fins1 = std::make_unique<TrapezoidFinSet>();
    fins1->setFinCount(3);
    fins1->setFinShape(1.5, 1.5, 0.0, 1.5, 0.005);
    bodyTube1.addChild(std::move(fins1));

    // make 1st stage recovery device with deployment config in default
    auto                    parachute = std::make_unique<Parachute>();
    DeploymentConfiguration deploymentConfig;
    deploymentConfig.setDeployEvent(DeploymentConfiguration::DeployEvent::LOWER_STAGE_SEPARATION);
    deploymentConfig.setDeployEvent(DeploymentConfiguration::DeployEvent::ALTITUDE);
    parachute->getDeploymentConfigurations().setDefault(deploymentConfig);
    bodyTube1.addChild(std::move(parachute));

    // make 2nd stage
    AxialStage& stage2 = rocket.addChild(std::make_unique<AxialStage>());
    stage2.setName("Stage2");

    // make 2nd stage nose cone
    stage2.addChild(std::make_unique<NoseCone>(TransitionShape::OGIVE, 6 * 0.5, 0.5));

    // make 2nd stage body tube
    BodyTube& bodyTube2 = stage2.addChild(std::make_unique<BodyTube>(15, 1, 0.05));

    // make 2nd stage fins
    auto fins2 = std::make_unique<TrapezoidFinSet>();
    fins2->setFinCount(3);
    fins2->setFinShape(1.0, 1.0, 0.0, 1.0, 0.005);
    bodyTube2.addChild(std::move(fins2));

    std::unique_ptr<OpenRocketDocument> rocketDoc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(owned));

    // create simulation data
    const auto simulation1 = std::make_shared<Simulation>(rocket, preferences);
    simulation1->getOptions().setIsaAtmosphere(false);  // helps cover code in saveComponent()
    simulation1->getOptions().setTimeStep(0.05);
    simulation1->getOptions().setLaunchPressure(100000);
    simulation1->getOptions().setLaunchTemperature(288);
    rocketDoc->addSimulation(simulation1);

    const auto simulation2 = std::make_shared<Simulation>(rocket, preferences);
    simulation2->getOptions().setIsaAtmosphere(true);  // helps cover code in saveComponent()
    simulation2->getOptions().setTimeStep(0.05);
    rocketDoc->addSimulation(simulation2);

    // Java runs both in one try block and ignores a SimulationException: a failed first run
    // leaves the second unsimulated.
    const std::shared_ptr<SimulationListener> simulationListener =
        std::make_shared<AbstractSimulationListener>();
    if (simulation1->simulate({simulationListener}).has_value())
    {
        std::ignore = simulation2->simulate({simulationListener});
    }

    rocket.enableEvents();
    return rocketDoc;
}

std::span<const TestDocumentMaker> testDocumentMakers()
{
    return kMakers;
}

}  // namespace QtRocket::Test
