package info.qtrocket.guireference;

import java.lang.reflect.Field;
import java.nio.file.Path;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.UUID;

import info.openrocket.core.aerodynamics.lookup.MachAoALookup;
import info.openrocket.core.document.PlotAppearance;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightEvent;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.simulation.extension.SimulationExtension;
import info.openrocket.core.simulation.montecarlo.MonteCarloParameter;
import info.openrocket.core.simulation.montecarlo.MonteCarloSettings;
import info.openrocket.core.simulation.montecarlo.UncertaintySpec;
import info.openrocket.core.util.Config;

/**
 * The simulations of a loaded design file in the lines QtRocket's tests print for theirs: SimProbe.dumpSimulation
 * of tier 9b (with the digests of the stored columns, as RecordedProbe of tier 9c prints every simulation of a
 * file), from which describeSimulation() of tests/core/file/openrocket/SimulationTestSupport.h was written,
 * copied here line for line (its helpers too: the options as CondProbe prints them, an extension's configuration
 * as EntryProbe prints one, the flight data as FdProbe prints them). simulationLines() of
 * tests/core/file/openrocket/DesignFileState.h is these lines.
 */
final class SimulationLines {

	/** The options of an empty conditions element under OpenRocket's test preferences. */
	static final String[] BASELINE = {
			"rod=0.0 intoWind=false angle=0.0 dir=0.0",
			"wind=AVERAGE avg=(0.0,1.5707963267948966,0.0) multi=MSL(0.0,0.0,0.0,0.0)",
			"site=0.0,0.0,0.0 geo=FLAT",
			"stepper=RK4 dt=0.0 tmax=1200.0 maxAngle=0.05235987755982988",
			"seedFixed=false",
			"atmosphere=isa:false T=288.15 p=101325.0 hum=0.0",
			"gravity=WGS/9.807",
			"thresholds=20.0/3.048/30.48/15.24",
			"drag=path:null table:null rows:null",
			"stability=path:null table:null rows:null",
	};

	private final List<String> lines;

	private SimulationLines(List<String> lines) {
		this.lines = lines;
	}

	/**
	 * Adds the lines of the simulation @p sim, the @p index-th of its document, to @p lines (SimProbe.dumpSimulation
	 * with digest = true and allIds = false). It does to the simulation what the loader's next step does
	 * (syncModID() and getStatus()).
	 */
	static void add(List<String> lines, int index, Simulation sim) throws Exception {
		new SimulationLines(lines).dumpSimulation(index, sim);
	}

	/**
	 * A line in ASCII (SimProbe.say): a line feed as &lt;LF&gt;, a tab as &lt;TAB&gt;, a carriage return as
	 * &lt;CR&gt; and every other character outside the printable ASCII range as &lt;U+XXXX&gt;.
	 */
	private void say(String s) {
		StringBuilder b = new StringBuilder();
		s.codePoints().forEach(c -> {
			if (c == '\n') {
				b.append("<LF>");
			} else if (c == '\t') {
				b.append("<TAB>");
			} else if (c == '\r') {
				b.append("<CR>");
			} else if (c < 32 || c > 126) {
				b.append(String.format("<U+%04X>", c));
			} else {
				b.appendCodePoint(c);
			}
		});
		lines.add(b.toString());
	}

	private static String id(UUID id) {
		return id.version() != 4 ? id.toString() : "(random)";
	}

	private static String source(RocketComponent c) {
		if (c == null) {
			return "null";
		}
		if (c == RocketComponent.REMOVED) {
			return c.getName() + "(REMOVED)";
		}
		return c.getName() + "@" + c.getID();
	}

	// ---- the options, as CondProbe prints them --------------------------------------------

	private static String rows(List<String> rows) {
		return rows == null ? "null" : "[" + String.join("|", rows).replace("\t", "<TAB>") + "]";
	}

	private static String table(MachAoALookup table, String... columns) {
		if (table == null) {
			return "null";
		}
		double mid = (table.getMinMach() + table.getMaxMach()) / 2;
		StringBuilder b = new StringBuilder("[aoa=" + table.hasAoA() + " mach=" + table.getMinMach() + ".."
				+ table.getMaxMach());
		for (String column : columns) {
			b.append(' ').append(column).append('=').append(table.interpolate(mid, 0, column));
		}
		return b + "]";
	}

	private static String path(Path path) {
		return path == null ? "null" : path.toString();
	}

	private static double storedDirection(SimulationOptions o) throws Exception {
		Field f = SimulationOptions.class.getDeclaredField("launchRodDirection");
		f.setAccessible(true);
		return f.getDouble(o);
	}

	private static String[] options(SimulationOptions o) throws Exception {
		StringBuilder levels = new StringBuilder();
		for (MultiLevelPinkNoiseWindModel.LevelWindModel l : o.getMultiLevelWindModel().getLevels()) {
			levels.append("(" + l.getAltitude() + "," + l.getSpeed() + "," + l.getDirection() + ","
					+ l.getStandardDeviation() + ")");
		}
		return new String[] {
				"rod=" + o.getLaunchRodLength() + " intoWind=" + o.getLaunchIntoWind() + " angle="
						+ o.getLaunchRodAngle() + " dir=" + storedDirection(o),
				"wind=" + o.getWindModelType().name() + " avg=(" + o.getAverageWindModel().getAverage() + ","
						+ o.getAverageWindModel().getDirection() + ","
						+ o.getAverageWindModel().getStandardDeviation() + ") multi="
						+ o.getMultiLevelWindModel().getAltitudeReference() + levels,
				"site=" + o.getLaunchAltitude() + "," + o.getLaunchLatitude() + "," + o.getLaunchLongitude()
						+ " geo=" + o.getGeodeticComputation().name(),
				"stepper=" + o.getSimulationStepperMethodChoice().name() + " dt=" + o.getTimeStep() + " tmax="
						+ o.getMaxSimulationTime() + " maxAngle=" + o.getMaximumStepAngle(),
				"seedFixed=" + o.isRandomSeedFixed() + (o.isRandomSeedFixed() ? " seed=" + o.getRandomSeed() : ""),
				"atmosphere=isa:" + o.isISAAtmosphere() + " T=" + o.getLaunchTemperature() + " p="
						+ o.getLaunchPressure() + " hum=" + o.getLaunchRelativeHumidity(),
				"gravity=" + o.getGravityModelType().name() + "/" + o.getConstantGravity(),
				"thresholds=" + o.getRecoverySpeedWarning() + "/" + o.getDrogueLowSpeedWarning() + "/"
						+ o.getRecoveryDrogueMainHighSpeedWarning() + "/"
						+ o.getRecoveryDrogueMainLowSpeedWarning(),
				"drag=path:" + path(o.getDragLookupCsvPath()) + " table:" + table(o.getDragLookupTable(), "cd")
						+ " rows:" + rows(o.getDragLookupCsvRows()),
				"stability=path:" + path(o.getStabilityLookupCsvPath()) + " table:"
						+ table(o.getStabilityLookupTable(), "cn", "cm", "cp") + " rows:"
						+ rows(o.getStabilityLookupCsvRows()),
		};
	}

	// ---- an extension's configuration, as EntryProbe prints one ----------------------------

	private static String describe(Object v) {
		if (v == null) {
			return "null";
		}
		if (v instanceof List<?> list) {
			StringBuilder b = new StringBuilder("List[");
			for (Object o : list) {
				b.append(describe(o)).append(", ");
			}
			return b + "]";
		}
		String extra = "";
		if (v instanceof java.math.BigDecimal d) {
			extra = " unscaled=" + d.unscaledValue() + " scale=" + d.scale();
		}
		return v.getClass().getSimpleName() + " " + v + extra;
	}

	private static String describe(Config config) throws Exception {
		Field f = Config.class.getDeclaredField("map");
		f.setAccessible(true);
		@SuppressWarnings("unchecked")
		Map<String, Object> map = (Map<String, Object>) f.get(config);
		StringBuilder b = new StringBuilder("{");
		for (Map.Entry<String, Object> e : map.entrySet()) {
			b.append(e.getKey()).append(" = ").append(describe(e.getValue())).append("; ");
		}
		return b + "}";
	}

	// ---- the flight data, as FdProbe prints them --------------------------------------------

	private void dumpWarning(Warning w, String indent) {
		StringBuilder src = new StringBuilder();
		if (w.getSources() == null) {
			src.append("<null>");
		} else {
			for (RocketComponent c : w.getSources()) {
				src.append(source(c)).append(";");
			}
		}
		String extra = "";
		if (w instanceof Warning.LargeAOA) {
			extra = " param=" + ((Warning.LargeAOA) w).getAOA();
		} else if (w instanceof Warning.RecoveryHighSpeedDeployment) {
			extra = " param=" + ((Warning.RecoveryHighSpeedDeployment) w).getSpeed();
		} else if (w instanceof Warning.EventAfterLanding) {
			FlightEvent e = ((Warning.EventAfterLanding) w).getEvent();
			extra = " event=" + (e == null ? "null" : e.getType().toString() + "@" + id(e.getID()));
		}
		say(indent + "fw[" + w.getClass().getSimpleName() + "," + w.getPriority() + "] id=" + id(w.getID())
				+ " text='" + w + "' desc='" + w.getMessageDescription() + "' sources=" + src + extra);
	}

	private static String type(FlightDataType t) {
		boolean builtin = FlightDataType.getTypeBySaveKey(t.getSaveKey()) == t;
		return t.getName() + "{" + t.getSymbol() + "," + t.getGroup() + "," + (builtin ? t.getSaveKey() : "-") + ","
				+ t.getUnitGroup().getSIUnit().getUnit() + "}";
	}

	private void dumpBranch(int index, FlightDataBranch branch, String in) {
		say(in + "    branch[" + index + "] '" + branch.getName() + "' rows=" + branch.getLength() + " optAlt="
				+ branch.getOptimumAltitude() + " tOptAlt=" + branch.getTimeToOptimumAltitude() + " optDelay="
				+ branch.getOptimumDelay() + " sepTime=" + branch.getSeparationTime() + " srcId="
				+ branch.getSourceComponentId());
		FlightDataType[] types = branch.getTypes();
		StringBuilder names = new StringBuilder();
		for (FlightDataType t : types) {
			names.append(names.length() == 0 ? "" : " | ").append(type(t));
		}
		say(in + "      types=" + names);
		for (FlightEvent e : branch.getEvents()) {
			say(in + "      event " + e.getType().name() + " t=" + e.getTime() + " id=" + id(e.getID()) + " src="
					+ source(e.getSource()) + " data=["
					+ (e.getData() == null ? "null" : e.getData().getClass().getSimpleName() + ":" + e.getData())
					+ "]");
		}
		for (FlightDataType t : types) {
			List<Double> values = branch.get(t);
			double sum = 0;
			int nans = 0;
			for (double v : values) {
				if (Double.isNaN(v)) {
					nans++;
				} else {
					sum += v;
				}
			}
			say(in + "      col " + t.getSaveKey() + ": min=" + branch.getMinimum(t) + " max="
					+ branch.getMaximum(t) + " first=" + values.get(0) + " last="
					+ values.get(values.size() - 1) + " sum=" + sum + " nans=" + nans);
		}
	}

	private void dumpData(FlightData data, String in) {
		if (data == null) {
			say(in + "  data: null");
			return;
		}
		say(in + "  data: branches=" + data.getBranchCount() + " maxAlt=" + data.getMaxAltitude() + " maxVel="
				+ data.getMaxVelocity() + " maxAcc=" + data.getMaxAcceleration() + " maxMach="
				+ data.getMaxMachNumber() + " tApogee=" + data.getTimeToApogee() + " tFlight="
				+ data.getFlightTime() + " vGround=" + data.getGroundHitVelocity() + " vRod="
				+ data.getLaunchRodVelocity() + " vDeploy=" + data.getDeploymentVelocity() + " optDelay="
				+ data.getOptimumDelay());
		for (Warning w : data.getWarningSet()) {
			dumpWarning(w, in + "    ");
		}
		for (int b = 0; b < data.getBranchCount(); b++) {
			dumpBranch(b, data.getBranch(b), in);
		}
	}

	// ---- a simulation ------------------------------------------------------------------------

	private static String xmlName(Enum<?> e) {
		return e.name().toLowerCase(java.util.Locale.ENGLISH).replace("_", "");
	}

	/**
	 * The landing dispersion settings in the notation the C++ tests print what they keep of the
	 * element: the attributes and the uncertainty children as maps, as a file written by
	 * OpenRocket's saver has them.
	 */
	private static String dispersion(MonteCarloSettings settings) {
		if (settings == null) {
			return "null";
		}
		StringBuilder b = new StringBuilder("{runs=" + settings.getRunCount() + ", seed=" + settings.getSeed() + "} [");
		boolean first = true;
		for (MonteCarloParameter parameter : MonteCarloParameter.values()) {
			UncertaintySpec u = settings.getUncertainties().get(parameter);
			if (u == null) {
				continue;
			}
			b.append(first ? "" : ", ").append("{distribution=" + xmlName(u.distribution()) + ", parameter="
					+ xmlName(parameter) + ", spread=" + Double.toString(u.spread()) + "}");
			first = false;
		}
		return b + "]";
	}

	private void dumpSimulation(int index, Simulation sim) throws Exception {
		Field statusField = Simulation.class.getDeclaredField("status");
		statusField.setAccessible(true);
		Object stored = statusField.get(sim);
		// What getStatus() would say now (asked of a clone, because asking changes the status).
		Simulation.Status presync = sim.clone().getStatus();
		// What OpenRocketLoader does next with every simulation.
		sim.syncModID();
		Simulation.Status status = sim.getStatus();
		SimulationOptions simulated = sim.getSimulatedConditions();
		say("  sim[" + index + "] name='" + sim.getName() + "' stored=" + ((Enum<?>) stored).name() + " presync="
				+ presync.name() + " status=" + status.name() + " fcid="
				+ id(sim.getId().key) + " simulated="
				+ (simulated == null ? "null" : simulated.equals(sim.getOptions()) ? "equal" : "differs"));
		String[] options = options(sim.getOptions());
		for (int i = 0; i < options.length; i++) {
			if (!options[i].equals(BASELINE[i])) {
				say("    " + options[i]);
			}
		}
		for (SimulationExtension e : sim.getSimulationExtensions()) {
			say("    ext " + e.getClass().getSimpleName() + " id=" + e.getId() + " name='" + e.getName() + "' config="
					+ describe(e.getConfig()));
		}
		for (Map.Entry<String, PlotAppearance> e : new TreeMap<>(sim.getPlotAppearances()).entrySet()) {
			PlotAppearance a = e.getValue();
			say("    plot '" + e.getKey() + "': color="
					+ (a.getColor() == null ? "null"
							: a.getColor().getRed() + "," + a.getColor().getGreen() + "," + a.getColor().getBlue() + ","
									+ a.getColor().getAlpha())
					+ " style=" + (a.getLineStyle() == null ? "null" : a.getLineStyle().name()));
		}
		if (sim.getLandingDispersionSettings() != null) {
			say("    dispersion: " + dispersion(sim.getLandingDispersionSettings()));
		}
		dumpData(sim.getSimulatedData(), "  ");
	}
}
