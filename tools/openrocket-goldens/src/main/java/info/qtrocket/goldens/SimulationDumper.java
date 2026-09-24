package info.qtrocket.goldens;

import java.io.BufferedWriter;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;
import java.util.zip.Deflater;
import java.util.zip.GZIPOutputStream;

import info.openrocket.core.document.Simulation;
import info.openrocket.core.logging.SimulationAbort;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.models.wind.PinkNoiseWindModel;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.BasicEventSimulationEngine;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.simulation.MotorClusterState;
import info.openrocket.core.simulation.SimulationConditions;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.simulation.exception.SimulationException;
import info.openrocket.core.simulation.extension.SimulationExtension;
import info.openrocket.core.simulation.extension.impl.ScriptingExtension;
import info.openrocket.core.util.Config;

/**
 * sim_&lt;sim&gt;.json and sim_&lt;sim&gt;_branch&lt;i&gt;.csv.gz: one simulation run with calm wind, a
 * fixed seed and without the pitch/yaw jitter (see {@link JitterRemoval}).
 */
final class SimulationDumper {

	/** The random seed every golden simulation runs with. */
	static final int RANDOM_SEED = 0;

	private SimulationDumper() {
	}

	/**
	 * Makes the simulation reproducible: fixed seed {@link #RANDOM_SEED} and zero standard deviation of
	 * every wind model (the average wind speed and direction are kept). Returns what was changed.
	 */
	static Map<String, Object> makeReproducible(SimulationOptions options) {
		Map<String, Object> harness = Json.object();
		harness.put("documentRandomSeed", options.isRandomSeedFixed() ? (Object) options.getRandomSeed() : null);
		harness.put("documentAverageWindStandardDeviation", options.getAverageWindModel().getStandardDeviation());
		List<Object> levels = Json.array();
		for (MultiLevelPinkNoiseWindModel.LevelWindModel level : options.getMultiLevelWindModel().getLevels()) {
			levels.add(level.getStandardDeviation());
		}
		harness.put("documentMultiLevelWindStandardDeviations", levels);

		options.setRandomSeedFixed(true);
		options.setRandomSeed(RANDOM_SEED);
		options.getAverageWindModel().setStandardDeviation(0);
		for (MultiLevelPinkNoiseWindModel.LevelWindModel level : options.getMultiLevelWindModel().getLevels()) {
			level.setStandardDeviation(0);
		}
		harness.put("randomSeed", RANDOM_SEED);
		harness.put("windStandardDeviation", 0.0);
		harness.put("pitchYawJitterRemoved", true);
		return harness;
	}

	/**
	 * Runs simulation {@code simIndex} of the document (unless it is skipped) and writes its files.
	 *
	 * @param variant what a harness-defined variant simulation changed in the default options, or null
	 * @return the written file names, relative to {@code inputDir}
	 */
	static List<String> dump(Path inputDir, String inputName, int simIndex, Simulation sim, Rocket rocket,
			ComponentIndex index, String optionsSource, String variant, Map<String, Object> harness)
			throws IOException {
		String base = String.format("sim_%02d_%s", simIndex, GoldenDumper.slug(sim.getName()));
		List<String> files = new ArrayList<>();

		Map<String, Object> root = Json.object();
		root.put("schema", "simulation");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("input", inputName);
		root.put("index", simIndex);
		root.put("name", sim.getName());

		FlightConfigurationId fcid = sim.getFlightConfigurationId();
		Map<String, Object> config = Json.object();
		int configIndex = 0;
		for (FlightConfiguration c : rocket.getFlightConfigurations()) {
			if (c.getId().equals(fcid)) {
				break;
			}
			configIndex++;
		}
		config.put("index", configIndex);
		config.put("id", fcid.key.toString());
		config.put("name", rocket.getFlightConfiguration(fcid).getName());
		root.put("flightConfiguration", config);
		root.put("optionsSource", optionsSource);
		root.put("variant", variant);
		root.put("options", options(sim.getOptions()));
		root.put("harness", harness);
		root.put("extensions", extensions(sim));

		String skipReason = skipReason(sim);
		root.put("skipped", skipReason != null);
		root.put("skipReason", skipReason);
		if (skipReason != null) {
			String json = base + ".json";
			Json.write(inputDir.resolve(json), root);
			files.add(json);
			return files;
		}

		// Simulation.simulate(), with the jitter-removal listeners placed first and last.
		SimulationConditions conditions = sim.getOptions().toSimulationConditions();
		conditions.setSimulation(sim);
		JitterRemoval jitterRemoval = new JitterRemoval();
		conditions.getSimulationListenerList().add(jitterRemoval.forcesListener());
		BasicEventSimulationEngine engine = new BasicEventSimulationEngine();
		SimulationException exception = null;
		try {
			for (SimulationExtension extension : sim.getSimulationExtensions()) {
				extension.initialize(conditions);
			}
			conditions.getSimulationListenerList().add(jitterRemoval.conditionsListener());
			engine.simulate(conditions);
		} catch (SimulationException e) {
			exception = e;
		}
		// A failure of the harness itself (also one inside the nested optimum-coast simulation, whose
		// exceptions OpenRocket only logs) aborts the run instead of being recorded as OpenRocket's.
		jitterRemoval.checkNoFailure(inputName + " " + base);
		FlightData data = engine.getFlightData();

		Map<String, Object> result = Json.object();
		result.put("status", exception == null ? "completed" : "exception");
		result.put("exceptionType", exception == null ? null : exception.getClass().getSimpleName());
		result.put("exceptionMessage", exception == null ? null : exception.getMessage());
		result.put("jitterReplacements", jitterRemoval.replacements());
		root.put("result", result);

		if (data == null) {
			root.put("summary", null);
			root.put("warnings", Json.array());
			root.put("branches", Json.array());
		} else {
			root.put("summary", summary(data));
			root.put("warnings", Values.warnings(data.getWarningSet(), index));
			List<Object> branches = Json.array();
			for (int i = 0; i < data.getBranchCount(); i++) {
				FlightDataBranch branch = data.getBranch(i);
				String csv = base + "_branch" + i + ".csv.gz";
				branches.add(branch(i, branch, csv, index));
				writeCsv(inputDir.resolve(csv), branch);
				files.add(csv);
			}
			root.put("branches", branches);
		}
		String json = base + ".json";
		Json.write(inputDir.resolve(json), root);
		files.add(0, json);
		return files;
	}

	/**
	 * Why the golden run of this simulation is skipped, or null. Only an enabled scripting extension
	 * skips it: a disabled one adds no listener ({@code ScriptingExtension.initialize}), so OpenRocket
	 * simulates it like a plain simulation, and so does the harness.
	 */
	private static String skipReason(Simulation sim) {
		if (sim.getStatus() == Simulation.Status.EXTERNAL) {
			return "imported (external) simulation data cannot be re-simulated";
		}
		for (SimulationExtension extension : sim.getSimulationExtensions()) {
			if (extension instanceof ScriptingExtension script && script.isEnabled()) {
				return "uses an enabled JavaScript scripting extension (out of scope for QtRocket)";
			}
		}
		return null;
	}

	private static Map<String, Object> options(SimulationOptions options) {
		Map<String, Object> o = Json.object();
		o.put("launchRodLength", options.getLaunchRodLength());
		o.put("launchIntoWind", options.getLaunchIntoWind());
		o.put("launchRodAngle", options.getLaunchRodAngle());
		o.put("launchRodDirection", options.getLaunchRodDirection());
		o.put("windModelType", options.getWindModelType().name());
		PinkNoiseWindModel average = options.getAverageWindModel();
		Map<String, Object> averageWind = Json.object();
		averageWind.put("average", average.getAverage());
		averageWind.put("standardDeviation", average.getStandardDeviation());
		averageWind.put("turbulenceIntensity", average.getTurbulenceIntensity());
		averageWind.put("direction", average.getDirection());
		o.put("averageWind", averageWind);
		MultiLevelPinkNoiseWindModel multiLevel = options.getMultiLevelWindModel();
		Map<String, Object> multiLevelWind = Json.object();
		multiLevelWind.put("altitudeReference", multiLevel.getAltitudeReference().name());
		List<Object> levels = Json.array();
		for (MultiLevelPinkNoiseWindModel.LevelWindModel level : multiLevel.getLevels()) {
			Map<String, Object> l = Json.object();
			l.put("altitude", level.getAltitude());
			l.put("speed", level.getSpeed());
			l.put("direction", level.getDirection());
			l.put("standardDeviation", level.getStandardDeviation());
			levels.add(l);
		}
		multiLevelWind.put("levels", levels);
		o.put("multiLevelWind", multiLevelWind);
		o.put("launchAltitude", options.getLaunchAltitude());
		o.put("launchLatitude", options.getLaunchLatitude());
		o.put("launchLongitude", options.getLaunchLongitude());
		o.put("geodeticComputation", options.getGeodeticComputation().name());
		o.put("isaAtmosphere", options.isISAAtmosphere());
		o.put("launchTemperature", options.getLaunchTemperature());
		o.put("launchPressure", options.getLaunchPressure());
		o.put("launchRelativeHumidity", options.getLaunchRelativeHumidity());
		o.put("timeStep", options.getTimeStep());
		o.put("maxSimulationTime", options.getMaxSimulationTime());
		o.put("maximumStepAngle", options.getMaximumStepAngle());
		o.put("randomSeed", options.getRandomSeed());
		o.put("randomSeedFixed", options.isRandomSeedFixed());
		o.put("gravityModelType", options.getGravityModelType().name());
		o.put("constantGravity", options.getConstantGravity());
		o.put("stepperMethod", options.getSimulationStepperMethodChoice().name());
		o.put("recoverySpeedWarning", options.getRecoverySpeedWarning());
		o.put("drogueLowSpeedWarning", options.getDrogueLowSpeedWarning());
		o.put("recoveryDrogueMainHighSpeedWarning", options.getRecoveryDrogueMainHighSpeedWarning());
		o.put("recoveryDrogueMainLowSpeedWarning", options.getRecoveryDrogueMainLowSpeedWarning());
		o.put("hasDragLookup", options.hasDragLookup());
		o.put("hasStabilityLookup", options.hasStabilityLookup());
		return o;
	}

	private static List<Object> extensions(Simulation sim) {
		List<Object> list = Json.array();
		for (SimulationExtension extension : sim.getSimulationExtensions()) {
			Map<String, Object> o = Json.object();
			o.put("id", extension.getId());
			o.put("type", extension.getClass().getSimpleName());
			o.put("name", extension.getName());
			Map<String, Object> config = Json.object();
			Config c = extension.getConfig();
			if (c != null) {
				for (String key : new TreeSet<>(c.keySet())) {
					config.put(key, GeometryDumper.toJson(c.get(key, null)));
				}
			}
			o.put("config", config);
			list.add(o);
		}
		return list;
	}

	private static Map<String, Object> summary(FlightData data) {
		Map<String, Object> o = Json.object();
		o.put("maxAltitude", data.getMaxAltitude());
		o.put("maxVelocity", data.getMaxVelocity());
		o.put("maxAcceleration", data.getMaxAcceleration());
		o.put("maxMachNumber", data.getMaxMachNumber());
		o.put("timeToApogee", data.getTimeToApogee());
		o.put("flightTime", data.getFlightTime());
		o.put("groundHitVelocity", data.getGroundHitVelocity());
		o.put("launchRodVelocity", data.getLaunchRodVelocity());
		o.put("deploymentVelocity", data.getDeploymentVelocity());
		o.put("optimumDelay", data.getOptimumDelay());
		o.put("branchCount", data.getBranchCount());
		return o;
	}

	/**
	 * Data types left out of the CSV files because they are not reproducible: computation_time is the
	 * wall-clock time since the simulation started (System.nanoTime()).
	 */
	static final Map<FlightDataType, String> EXCLUDED_TYPES = Map.of(FlightDataType.TYPE_COMPUTATION_TIME,
			"wall-clock time (System.nanoTime), not reproducible");

	/** The branch's data types in OpenRocket's order (getTypes()), without the excluded ones. */
	static List<FlightDataType> csvTypes(FlightDataBranch branch) {
		List<FlightDataType> types = new ArrayList<>();
		for (FlightDataType type : branch.getTypes()) {
			if (!EXCLUDED_TYPES.containsKey(type)) {
				types.add(type);
			}
		}
		return types;
	}

	/** Whether {@code type} is one of OpenRocket's built-in types (it has a registered save key). */
	static boolean isBuiltin(FlightDataType type) {
		return FlightDataType.getTypeBySaveKey(type.getSaveKey()) == type;
	}

	/**
	 * The CSV column key of a data type: the save key of a built-in type ("altitude"), "custom:" + the
	 * name for other types (extension and custom-expression types; a branch identifies its columns by
	 * name, and getSaveKey() falls back to the name for them).
	 */
	static String columnKey(FlightDataType type) {
		return isBuiltin(type) ? type.getSaveKey() : "custom:" + type.getName();
	}

	private static Map<String, Object> branch(int branchIndex, FlightDataBranch branch, String csv,
			ComponentIndex index) {
		Map<String, Object> o = Json.object();
		o.put("index", branchIndex);
		o.put("name", branch.getName());
		o.put("sourceComponent", index.path(branch.getSourceComponentId()));
		o.put("rows", branch.getLength());
		o.put("optimumAltitude", branch.getOptimumAltitude());
		o.put("timeToOptimumAltitude", branch.getTimeToOptimumAltitude());
		o.put("optimumDelay", branch.getOptimumDelay());
		o.put("separationTime", branch.getSeparationTime());
		o.put("csv", csv);

		List<Object> excluded = Json.array();
		for (FlightDataType type : branch.getTypes()) {
			if (EXCLUDED_TYPES.containsKey(type)) {
				excluded.add(columnKey(type));
			}
		}
		o.put("excludedColumns", excluded);

		List<Object> columns = Json.array();
		for (FlightDataType type : csvTypes(branch)) {
			Map<String, Object> c = Json.object();
			c.put("key", columnKey(type));
			c.put("name", type.getName());
			c.put("symbol", type.getSymbol());
			c.put("builtin", isBuiltin(type));
			c.put("min", branch.getMinimum(type));
			c.put("max", branch.getMaximum(type));
			columns.add(c);
		}
		o.put("columns", columns);

		List<Object> events = Json.array();
		for (FlightEvent event : branch.getEvents()) {
			events.add(event(event, index));
		}
		o.put("events", events);
		return o;
	}

	private static Map<String, Object> event(FlightEvent event, ComponentIndex index) {
		Map<String, Object> o = Json.object();
		o.put("time", event.getTime());
		o.put("type", event.getType().name());
		RocketComponent source = event.getSource();
		o.put("source", index.path(source));
		Object data = event.getData();
		Object dataJson;
		if (data == null) {
			dataJson = null;
		} else if (data instanceof MotorClusterState state) {
			Map<String, Object> d = Json.object();
			d.put("mount", index.path((RocketComponent) state.getMount()));
			d.put("designation", state.getMotor().getDesignation());
			d.put("motorCount", state.getMotorCount());
			dataJson = d;
		} else if (data instanceof SimulationAbort abort) {
			Map<String, Object> d = Json.object();
			d.put("cause", abort.getCause().name());
			d.put("description", abort.getMessageDescription());
			dataJson = d;
		} else if (data instanceof Warning warning) {
			dataJson = Values.warning(warning, index);
		} else if (data instanceof String s) {
			dataJson = s;
		} else {
			dataJson = data.toString();
		}
		o.put("data", dataJson);
		return o;
	}

	/** Writes the branch as gzip-compressed CSV (header = column keys, values = Double.toString). */
	private static void writeCsv(Path file, FlightDataBranch branch) throws IOException {
		List<FlightDataType> types = csvTypes(branch);
		List<List<Double>> columns = new ArrayList<>(types.size());
		for (FlightDataType type : types) {
			columns.add(branch.get(type));
		}
		Files.createDirectories(file.getParent());
		try (OutputStream out = Files.newOutputStream(file);
				GZIPOutputStream gzip = new GZIPOutputStream(out, 1 << 16) {
					{
						def.setLevel(Deflater.BEST_COMPRESSION);
					}
				};
				Writer writer = new BufferedWriter(new OutputStreamWriter(gzip, StandardCharsets.UTF_8))) {
			StringBuilder line = new StringBuilder();
			for (int j = 0; j < types.size(); j++) {
				if (j > 0) {
					line.append(',');
				}
				line.append(columnKey(types.get(j)));
			}
			line.append('\n');
			writer.write(line.toString());
			int rows = branch.getLength();
			for (int i = 0; i < rows; i++) {
				line.setLength(0);
				for (int j = 0; j < columns.size(); j++) {
					if (j > 0) {
						line.append(',');
					}
					Double value = columns.get(j).get(i);
					line.append(Double.toString(value == null ? Double.NaN : value));
				}
				line.append('\n');
				writer.write(line.toString());
			}
		}
	}
}
