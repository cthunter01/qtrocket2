package info.qtrocket.goldens;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

import info.openrocket.core.masscalc.CMAnalysisEntry;
import info.openrocket.core.masscalc.MassCalculator;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;

/**
 * mass.json: per flight configuration the STRUCTURE, LAUNCH, BURNOUT and MOTOR rigid bodies of
 * {@link MassCalculator} and its CM analysis ({@code getCMAnalysis}).
 */
final class MassDumper {

	private MassDumper() {
	}

	static Map<String, Object> dump(String inputName, Rocket rocket, ComponentIndex index) {
		Map<String, Object> root = Json.object();
		root.put("schema", "mass");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("input", inputName);

		List<Object> configurations = Json.array();
		FlightConfiguration originallySelected = rocket.getSelectedConfiguration();
		int configIndex = 0;
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			rocket.setSelectedConfiguration(config.getId());
			Map<String, Object> o = Json.object();
			GeometryDumper.putConfigurationHeader(o, configIndex, config);
			o.put("structure", Values.rigidBody(MassCalculator.calculateStructure(config)));
			o.put("launch", Values.rigidBody(MassCalculator.calculateLaunch(config)));
			o.put("burnout", Values.rigidBody(MassCalculator.calculateBurnout(config)));
			o.put("motor", Values.rigidBody(MassCalculator.calculateMotor(config)));
			o.put("cmAnalysis", cmAnalysis(config, index));
			configurations.add(o);
			configIndex++;
		}
		rocket.setSelectedConfiguration(originallySelected.getId());
		root.put("configurations", configurations);
		return root;
	}

	/**
	 * The CM analysis rows: components in tree order, then motors by designation, then the total (the
	 * row OpenRocket stores under the rocket's key). Each row has the mass of one instance and the
	 * CM of all its active instances (weight = their total mass).
	 */
	private static List<Object> cmAnalysis(FlightConfiguration config, ComponentIndex index) {
		Map<Integer, CMAnalysisEntry> analysis = MassCalculator.getCMAnalysis(config);
		Rocket rocket = config.getRocket();

		List<Object> rows = Json.array();
		CMAnalysisEntry total = null;
		List<CMAnalysisEntry> motors = new ArrayList<>();
		List<CMAnalysisEntry> components = new ArrayList<>();
		for (CMAnalysisEntry entry : analysis.values()) {
			if (entry.source == rocket) {
				total = entry;
			} else if (entry.source instanceof Motor) {
				motors.add(entry);
			} else {
				components.add(entry);
			}
		}
		components.sort(Comparator.comparingInt(e -> index.components().indexOf((RocketComponent) e.source)));
		motors.sort(Comparator.comparing(e -> e.name));

		for (CMAnalysisEntry entry : components) {
			rows.add(row("component", index.path((RocketComponent) entry.source), entry));
		}
		for (CMAnalysisEntry entry : motors) {
			rows.add(row("motor", null, entry));
		}
		if (total != null) {
			rows.add(row("total", index.path(rocket), total));
		}
		return rows;
	}

	private static Map<String, Object> row(String kind, String path, CMAnalysisEntry entry) {
		Map<String, Object> o = Json.object();
		o.put("kind", kind);
		o.put("path", path);
		o.put("name", entry.name);
		o.put("eachMass", entry.eachMass);
		o.put("totalCM", Values.xyzw(entry.totalCM));
		return o;
	}
}
