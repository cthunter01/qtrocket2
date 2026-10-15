#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/Preferences.h"

/// The document makers of OpenRocket's TestRockets (core/src/main/java/.../util/
/// TestRockets.java: makeTestRocket_v100() ... makeTestRocket_for_estimateFileSize(), with
/// getTestMotor(), which only they use), built call for call as the Java makers build them, from
/// the rocket makers of rocket/TestRockets.h where Java calls those, and the motor database of
/// OpenRocketSaverTest, the one test class that uses them. The definitions are in
/// TestDocuments.cpp.
///
/// Where Java's `new Simulation(rocket)` reads the application preferences (OpenRocket's test
/// preferences), a maker that makes simulations takes the preference store they are to read, as
/// `Simulation(rocket, preferences)`: give it one with storeJavaTestPreferences()
/// (simulation/SimulationRunSupport.h), which must outlive the document. The time step of those
/// preferences is 0, as OpenRocket's, so a simulation of makeTestRocketV104WithSimulationData()
/// cannot run; makeTestRocketForEstimateFileSize() sets 0.05 s itself, as Java does.
///
/// The ids the makers draw (the components', the flight configurations' of
/// makeTestRocketV106WithMotorMountIgnitionConfig() and the events' of a simulation) are random,
/// as in Java: a test never pins them.
namespace QtRocket::Test
{

/// OpenRocketSaverTest.SIMULATION_EXTENSION_SCRIPT, which tests the escaping of a script.
inline constexpr std::string_view kSimulationExtensionScript = "// Test <  &\n// >\n// <![CDATA[";

/// The digest OpenRocket computes for the body tube preset of
/// makeTestRocketV105WithComponentPreset() (written in its <preset> element).
inline constexpr std::string_view kV105PresetDigest = "bca1f5bf3caeec8d8fa3bf5100a68808";

/// TestRockets.getTestMotor(): a minimal motor without useful numbers, the F12X of the
/// manufacturer "A" (AeroTech's code; description "Desc", case "info", of the type UNKNOWN, without
/// standard delays, 24 by 70 mm, a thrust of 1 N at 1 s over 2 s, every CG point Coordinate.NUL),
/// of the digest "digestA".
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> getTestMotor();

/// OpenRocketSaverTest.readMotor(): the motor of tests/data/motors/Estes_A8.rse, or null with a
/// test failure.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> readEstesA8Motor();

/// OpenRocketSaverTest's motor database (MotorDbProvider): exactly the Estes A8 of
/// readEstesA8Motor() and an F12X of the manufacturer "A" with the digest "digestA" (getTestMotor()
/// without its case info), two motor sets. A test failure when either is missing.
[[nodiscard]] std::unique_ptr<ThrustCurveMotorSetDatabase> openRocketSaverTestMotors();

/// TestRockets.makeTestRocket_v100(): "v100", a stage "Stage1" holding a body tube (12 m,
/// radius 1 m, wall 5 cm).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV100();

/// TestRockets.makeTestRocket_v101_withFinTabs(): "v101_withFinTabs", v100's body tube with
/// three trapezoidal fins (chords 1 m, no sweep, height 1 m, 5 mm thick) with tabs of 0.25 by
/// 0.25 m.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV101WithFinTabs();

/// TestRockets.makeTestRocket_v101_withTubeCouplerChild(): "v101_withTubeCouplerChild", v100's
/// body tube holding a tube coupler that holds a centering ring.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV101WithTubeCouplerChild();

/// TestRockets.makeTestRocket_v104_withMotor(): "v104_withMotorConfig", v100's body tube holding
/// an inner tube with getTestMotor() and an ejection delay of 5 s in the selected (the default)
/// configuration, named "F12X".
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV104WithMotor();

/// TestRockets.makeTestRocket_v104_withSimulationData(): "v104_withSimulationData", the rocket of
/// makeTestRocketV104WithMotor() with two new simulations of @p preferences, the first set to the
/// selected configuration. @p preferences must outlive the document.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV104WithSimulationData(
    Preferences& preferences);

/// TestRockets.makeTestRocket_v105_withCustomExpression(): "v105_withCustomExpression", a stage
/// holding a default body tube, and the custom expression "name", "symbol", "unit",
/// "expression".
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithCustomExpression();

/// TestRockets.makeTestRocket_v105_withComponentPreset(): "v105_withComponentPreset", a stage
/// holding a body tube loaded from a BODY_TUBE preset of "manufacturer" "partno" (2 m long,
/// diameters 2 and 1 m, 100 kg), made by ComponentPresetFactory with OpenRocket's materials
/// (digest kV105PresetDigest).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithComponentPreset();

/// TestRockets.makeTestRocket_v105_withLowerStageRecoveryDevice():
/// "v105_withLowerStageRecoveryDevice", a stage "Stage1" whose body tube (5 m, radius 1 m, wall
/// 5 cm) holds a parachute deployed at LOWER_STAGE_SEPARATION by default, and an empty stage
/// "Stage2".
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV105WithLowerStageRecoveryDevice();

/// TestRockets.makeTestRocket_v106_withAppearance(): "v106_withAppearance", v100's body tube with
/// an appearance (paint 100, 25, 50, shine 1, no decal).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithAppearance();

/// TestRockets.makeTestRocket_v106_withMotorMountIgnitionConfig():
/// "v106_withwithMotorMountIgnitionConfig" (sic), v100's body tube holding an inner tube with
/// getTestMotor() in a configuration of a new random id that the rocket does not have, whose
/// ignition delay is then set to 2 s.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithMotorMountIgnitionConfig();

/// TestRockets.makeTestRocket_v106_withRecoveryDeviceDeploymentConfig():
/// "v106_withRecoveryDeviceDeploymentConfig", v100's body tube holding a parachute deployed at an
/// altitude of 1000 m in the configuration "testParachute" (an id made from the text).
[[nodiscard]] std::unique_ptr<OpenRocketDocument>
makeTestRocketV106WithRecoveryDeviceDeploymentConfig();

/// TestRockets.makeTestRocket_v106_withStageSeparationConfig(): "v106_withStageSeparationConfig",
/// a stage "Stage1" whose body tube (5 m, radius 1 m, wall 5 cm) holds a parachute, separating 3 s
/// after its event in the configuration "3SecondDelay", and a stage "Stage2" with v100's body
/// tube.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithStageSeparationConfig();

/// TestRockets.makeTestRocket_v110_withSimulationExtension(@p script): the Estes Alpha III with
/// one new simulation of @p preferences holding an enabled JavaScript extension of @p script.
/// @p preferences must outlive the document.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV110WithSimulationExtension(
    std::string_view script, Preferences& preferences);

/// TestRockets.makeTestRocket_v108_withBoosters(): the Falcon 9 Heavy.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV108WithBoosters();

/// TestRockets.makeTestRocket_v108_withDisabledStage(): the Falcon 9 Heavy with stage 0 of its
/// selected configuration inactive (Java: _setStageActive(0, false, false)).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV108WithDisabledStage();

/// TestRockets.makeTestRocket_for_estimateFileSize(): "for_estimateFileSize", a stage "Stage1"
/// whose body tube (5 m, radius 1 m, wall 5 cm) holds three fins (chords 1.5 m, height 1.5 m)
/// and a parachute deployed at an ALTITUDE by default, and a stage "Stage2" with an ogive nose
/// cone (3 m, radius 0.5 m) and a body tube (15 m, radius 1 m, wall 5 cm) with three fins (chords
/// 1 m, height 1 m); two new simulations of @p preferences with a time step of 0.05 s, the first
/// with the launch conditions entered by hand (100000 Pa, 288 K), the second with the ISA
/// atmosphere, each simulated once with an AbstractSimulationListener. The rocket has no motor,
/// so both abort at once ("No motors defined"); a failed run is ignored, as in Java.
/// @p preferences must outlive the document.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketForEstimateFileSize(
    Preferences& preferences);

/// One document maker of TestRockets.java.
struct TestDocumentMaker
{
    /// The maker's name without its "makeTestRocket_" prefix ("v104_withSimulationData"), which
    /// is also the name of OpenRocket's save of the document in
    /// tests/data/goldens/test-documents/.
    std::string_view name;
    /// Makes the document; @p preferences as the makers that take them (the others do not use
    /// them), with kSimulationExtensionScript for makeTestRocketV110WithSimulationExtension().
    std::unique_ptr<OpenRocketDocument> (*make)(Preferences& preferences);
};

/// The sixteen makers in the order of OpenRocketSaverTest.testCreateLoadSave.
[[nodiscard]] std::span<const TestDocumentMaker> testDocumentMakers();

}  // namespace QtRocket::Test
