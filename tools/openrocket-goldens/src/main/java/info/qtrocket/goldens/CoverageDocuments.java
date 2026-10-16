package info.qtrocket.goldens;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Supplier;

import info.openrocket.core.aerodynamics.lookup.CsvMachAoALookup;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.PlotAppearance;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.logging.SimulationAbort;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.models.gravity.GravityModelType;
import info.openrocket.core.models.wind.WindModelType;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.ReferenceType;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.simulation.customexpression.CustomExpression;
import info.openrocket.core.simulation.montecarlo.MonteCarloDistribution;
import info.openrocket.core.simulation.montecarlo.MonteCarloParameter;
import info.openrocket.core.simulation.montecarlo.MonteCarloSettings;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.LineStyle;
import info.openrocket.core.util.ORColor;
import info.openrocket.core.util.TestRockets;

/**
 * The saver-coverage documents of the goldens ({@code coverage-<name>/resave/rocket.ork}): designs built here
 * that hold the element and attribute names OpenRocket's .ork saver can write and that no other OpenRocket
 * output among the goldens holds (the re-saves of the examples, of the test rockets and of the legacy files,
 * and the examples' re-saves with simulation data). Together with those they hold every name the saver
 * writes. See README.md for the list.
 * <p>
 * Each document is a fixed point of OpenRocket's own save and load ({@code GoldenDumper.dumpCoverage} checks
 * it): its re-save, loaded and saved again, is the re-save. That is what a test of a port can compare its
 * save of the loaded re-save with, so the documents avoid what OpenRocket does not read back as it wrote it:
 * every motor comes from the bundled motor database (a synthetic motor of TestRockets is not found again),
 * the document preferences are strings and booleans (a number written with its explicit type is not read
 * back), and there is no component preset (an unresolved reference is dropped) and no script.
 */
final class CoverageDocuments {

	/** A coverage document: how it is built and whether it is saved with its simulation data. */
	record Document(String method, Supplier<OpenRocketDocument> builder, boolean saveSimulationData) {
	}

	/** The documents, in the order of the goldens, by the name of their input without "coverage-". */
	static final Map<String, Document> DOCUMENTS = new LinkedHashMap<>();
	static {
		DOCUMENTS.put("kitchen-sink", new Document("kitchenSink", CoverageDocuments::kitchenSink, false));
		DOCUMENTS.put("stored-flight", new Document("storedFlight", CoverageDocuments::storedFlight, true));
	}

	private CoverageDocuments() {
	}

	/**
	 * The rocket of both documents: TestRockets' Estes Alpha III with the motors of the bundled database that
	 * have the designations of its synthetic ones (A8, B4 and C6 of Estes, the one with the smallest digest of
	 * each), with every recovery device a drogue and with the nozzle exit diameter of its first motor set.
	 */
	private static Rocket alphaWithDatabaseMotors() {
		Rocket rocket = TestRockets.makeEstesAlphaIII();
		ThrustCurveMotorSetDatabase motors = Application.getInjector().getInstance(ThrustCurveMotorSetDatabase.class);
		boolean first = true;
		for (RocketComponent c : rocket) {
			if (c instanceof RecoveryDevice device) {
				device.setDrogue(true);
			}
			if (!(c instanceof MotorMount mount) || !mount.isMotorMount()) {
				continue;
			}
			for (FlightConfigurationId fcid : rocket.getIds()) {
				MotorConfiguration config = mount.getMotorConfig(fcid);
				if (config.isEmpty()) {
					continue;
				}
				config.setMotor(databaseMotor(motors, config.getMotor()));
				if (first) {
					config.setNozzleExitDiameter(0.006);
					first = false;
				}
			}
		}
		return rocket;
	}

	/** The motor of {@code motors} with the manufacturer and designation of {@code motor} and the smallest digest. */
	private static ThrustCurveMotor databaseMotor(ThrustCurveMotorSetDatabase motors, Motor motor) {
		String manufacturer = ((ThrustCurveMotor) motor).getManufacturer().getSimpleName();
		// (Motor itself names no manufacturer: the makers' motors are ThrustCurveMotors.)
		List<ThrustCurveMotor> found = new ArrayList<>();
		for (ThrustCurveMotorSet set : motors.getMotorSets()) {
			for (ThrustCurveMotor candidate : set.getMotors()) {
				if (candidate.getManufacturer().getSimpleName().equals(manufacturer)
						&& candidate.getDesignation().equals(motor.getDesignation())) {
					found.add(candidate);
				}
			}
		}
		return found.stream().min(Comparator.comparing(ThrustCurveMotor::getDigest)).orElseThrow(
				() -> new IllegalStateException("No motor " + manufacturer + " " + motor.getDesignation()
						+ " in the motor database"));
	}

	/**
	 * The design-level names: a kit name, a custom reference length, the CG and CD overrides of a body tube
	 * with their subcomponent flags, its line style and colour, a drogue, a nozzle exit diameter, a custom
	 * expression, document preferences (a string and a boolean), all 29 photo settings, and a simulation
	 * (never run) with the extended ISA atmosphere, a constant gravity, a fixed random seed, a multi-level
	 * wind, landing dispersion settings, plot appearances and drag and stability lookup tables.
	 */
	static OpenRocketDocument kitchenSink() {
		Rocket rocket = alphaWithDatabaseMotors();
		rocket.setKitName("Kit & <name>");
		rocket.setReferenceType(ReferenceType.CUSTOM);
		rocket.setCustomReferenceLength(0.0251);
		RocketComponent body = rocket.getChild(0).getChild(1);
		body.setCGOverridden(true);
		body.setOverrideCGX(0.123);
		body.setSubcomponentsOverriddenCG(true);
		body.setCDOverridden(true);
		body.setOverrideCD(0.61);
		body.setSubcomponentsOverriddenCD(true);
		body.setLineStyle(LineStyle.DASHDOT);
		body.setColor(new ORColor(10, 20, 30, 40));

		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
		doc.addCustomExpression(new CustomExpression(doc, "Kinetic energy", "Ek", "J", "0.5*m*Vt^2"));
		doc.getDocumentPreferences().putString("comment", "kitchen & sink");
		doc.getDocumentPreferences().putBoolean("checked", true);
		doc.setPhotoSettings(photoSettings());

		Simulation sim = new Simulation(doc, rocket);
		sim.setName("Kitchen sink");
		sim.setFlightConfigurationId(TestRockets.TEST_FCID_2);
		SimulationOptions options = sim.getOptions();
		options.copyConditionsFrom(GoldenDumper.applicationDefaultOptions());
		options.setTimeStep(0.01);
		options.setISAAtmosphere(false);
		options.setLaunchTemperature(280.5);
		options.setLaunchPressure(95000.25);
		options.setLaunchRelativeHumidity(0.55);
		options.setGravityModelType(GravityModelType.CONSTANT);
		options.setConstantGravity(9.7);
		options.setRandomSeed(-123456789);
		options.setRandomSeedFixed(true);
		options.getMultiLevelWindModel().clearLevels();
		options.getMultiLevelWindModel().addWindLevel(0, 2.0, Math.PI / 2, 0.1);
		options.getMultiLevelWindModel().addWindLevel(150, 4.5, 2.2, 0.3);
		options.setWindModelType(WindModelType.MULTI_LEVEL);
		List<String> dragRows = List.of("Mach,AoA,Cd", "0.1,0,0.5", "0.1,4,0.55", "0.8,0,0.6", "0.8,4,0.65");
		options.setDragLookup(Path.of("/lookups/drag.csv"), CsvMachAoALookup.parse(dragRows, List.of("cd"), ','),
				dragRows);
		List<String> stabilityRows = List.of("Mach;AoA;CN;Cm;Cp", "0.1;0;0.0;0.0;0.4", "0.1;4;0.6;-0.1;0.42",
				"0.8;0;0.0;0.0;0.41", "0.8;4;0.7;-0.12;0.43");
		options.setStabilityLookup(null, CsvMachAoALookup.parse(stabilityRows, List.of("cn", "cm", "cp"), ';'),
				stabilityRows);
		sim.setLandingDispersionSettings(MonteCarloSettings.builder().runCount(750).seed(-5).threadCount(3)
				.uncertainty(MonteCarloParameter.WIND_SPEED, MonteCarloDistribution.UNIFORM, 1.25)
				.uncertainty(MonteCarloParameter.AIR_DENSITY, MonteCarloDistribution.LOG_NORMAL, 0.015).build());
		sim.setPlotAppearance(FlightDataType.TYPE_ALTITUDE,
				new PlotAppearance(new ORColor(255, 0, 128, 200), LineStyle.DASHED));
		sim.setPlotAppearance(FlightDataType.TYPE_VELOCITY_Z, new PlotAppearance(null, LineStyle.DOTTED));
		sim.setPlotAppearance(FlightDataType.TYPE_TIME, new PlotAppearance(new ORColor(0, 64, 0, 255), null));
		doc.addSimulation(sim);
		return doc;
	}

	/** Every key PhotoStudioSaver writes, with values of the form the photo studio stores. */
	private static Map<String, String> photoSettings() {
		Map<String, String> photo = new LinkedHashMap<>();
		photo.put("roll", "3.14");
		photo.put("yaw", "0.25");
		photo.put("pitch", "2.05");
		photo.put("advance", "0.1");
		photo.put("viewAlt", "-0.166");
		photo.put("viewAz", "3.38");
		photo.put("viewDistance", "0.44");
		photo.put("fov", "1.4");
		photo.put("lightAlt", "0.35");
		photo.put("lightAz", "-1.0");
		photo.put("lightStrength", "0.8");
		photo.put("sunlight", "255 250 240 255");
		photo.put("ambiance", "0.3");
		photo.put("skyColor", "55 95 155 255");
		photo.put("backgroundType", "GRADIENT");
		photo.put("gradientTopColor", "1 2 3 255");
		photo.put("gradientBottomColor", "4 5 6 128");
		photo.put("motionBlurred", "true");
		photo.put("motionBlurAmount", "0.3");
		photo.put("flame", "true");
		photo.put("flameColor", "255 100 50 255");
		photo.put("smoke", "true");
		photo.put("smokeColor", "230 230 230 102");
		photo.put("sparks", "true");
		photo.put("exhaustScale", "1.5");
		photo.put("flameAspectRatio", "2.0");
		photo.put("sparkConcentration", "0.2");
		photo.put("sparkWeight", "0.1");
		photo.put("sky", "info.openrocket.swing.gui.figure3d.photo.sky.builtin.Mountains");
		return photo;
	}

	/**
	 * The names of stored flight data no example's data holds: a simulation with flight data made here (it
	 * is not simulated) whose one branch has an event after the landing (the warning's event, eventid) and
	 * ends in an abort (the event's cause; with it the simulation's status is ABORTED, as
	 * Simulation.getStatus() finds it). Saved with the simulation data, which a save without it leaves out.
	 */
	static OpenRocketDocument storedFlight() {
		Rocket rocket = alphaWithDatabaseMotors();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
		RocketComponent chute = null;
		for (RocketComponent c : rocket) {
			if (c instanceof RecoveryDevice) {
				chute = c;
			}
		}

		FlightDataBranch branch = new FlightDataBranch("Stage", FlightDataType.TYPE_TIME,
				FlightDataType.TYPE_ALTITUDE, FlightDataType.TYPE_VELOCITY_Z);
		double[][] rows = { { 0.0, 0.0, 0.0 }, { 0.5, 2.25, 4.5 }, { 1.0, 1.5, -1.5 }, { 1.25, 0.0, -6.0 },
				{ 1.5, 0.0, 0.0 } };
		for (double[] row : rows) {
			branch.addPoint();
			branch.setValue(FlightDataType.TYPE_TIME, row[0]);
			branch.setValue(FlightDataType.TYPE_ALTITUDE, row[1]);
			branch.setValue(FlightDataType.TYPE_VELOCITY_Z, row[2]);
		}
		branch.addEvent(new FlightEvent(FlightEvent.Type.LAUNCH, 0.0, rocket));
		branch.addEvent(new FlightEvent(FlightEvent.Type.APOGEE, 0.5, rocket));
		branch.addEvent(new FlightEvent(FlightEvent.Type.GROUND_HIT, 1.25));
		FlightEvent late = new FlightEvent(FlightEvent.Type.RECOVERY_DEVICE_DEPLOYMENT, 1.5, chute);
		branch.addEvent(late);
		Warning.EventAfterLanding afterLanding = new Warning.EventAfterLanding(late);
		branch.addEvent(new FlightEvent(FlightEvent.Type.SIM_WARN, 1.5, null, afterLanding));
		branch.addEvent(new FlightEvent(FlightEvent.Type.SIM_ABORT, 1.5, null,
				new SimulationAbort(SimulationAbort.Cause.NO_MOTORS_FIRED)));
		FlightData data = new FlightData(branch);
		data.getWarningSet().add(afterLanding);

		SimulationOptions options = new SimulationOptions();
		options.copyConditionsFrom(GoldenDumper.applicationDefaultOptions());
		Simulation sim = new Simulation(doc, rocket, Simulation.Status.LOADED, "Stored flight", options, List.of(),
				data);
		sim.setFlightConfigurationId(TestRockets.TEST_FCID_2);
		doc.addSimulation(sim);
		return doc;
	}
}
