package info.qtrocket.goldens;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import info.openrocket.core.aerodynamics.AerodynamicForces;
import info.openrocket.core.aerodynamics.BarrowmanCalculator;
import info.openrocket.core.aerodynamics.FlightConditions;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.masscalc.MassCalculator;
import info.openrocket.core.models.atmosphere.AtmosphericConditions;
import info.openrocket.core.rocketcomponent.ComponentAssembly;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.MathUtil;

/**
 * aero.json: for every flight configuration, the extended-Barrowman results of
 * {@link BarrowmanCalculator} at a fixed set of flight conditions ("points"): CP and CNa
 * ({@code getCP}), the total forces ({@code getAerodynamicForces}) and the per-component force
 * analysis ({@code getForceAnalysis}), plus the worst CP over all lateral wind directions per Mach
 * number ({@code getWorstCP}) and the geometry warnings ({@code checkGeometry}).
 */
final class AeroDumper {

	/** Mach numbers of the point grid (plan section 6.4). */
	static final double[] MACHS = { 0.05, 0.3, 0.6, 0.9, 1.1, 1.5, 2.0 };
	/** Angles of attack of the point grid, in degrees (converted with Math.toRadians). */
	static final double[] AOAS_DEG = { 0.0, 2.0, 10.0 };

	private AeroDumper() {
	}

	/** The nozzle exit diameter the nozzle point gives every motor, as a fraction of its mount's diameter. */
	static final double NOZZLE_EXIT_DIAMETER_FRACTION = 0.5;

	/** One set of flight conditions. */
	private record Point(double mach, double aoa, double theta, double rollRate, double pitchRate,
			double yawRate, boolean pitchCenterAtStructureCG, boolean thrustingNozzles) {
	}

	/**
	 * The points: the Mach x AoA grid at theta 0 without rotation, then four off-grid cases. None of
	 * them depends on the motors, so configurations that differ only in their motors keep identical
	 * results.
	 */
	private static List<Point> points() {
		List<Point> points = new ArrayList<>();
		for (double mach : MACHS) {
			for (double aoaDeg : AOAS_DEG) {
				points.add(new Point(mach, Math.toRadians(aoaDeg), 0.0, 0.0, 0.0, 0.0, false, false));
			}
		}
		// A lateral wind direction off the fin planes.
		points.add(new Point(0.3, Math.toRadians(5.0), Math.PI / 4, 0.0, 0.0, 0.0, false, false));
		// Rotation: roll forcing and damping, pitch and yaw damping, first about the default pitch
		// centre (the nose tip, the only one the simulation uses), then about the structure CG.
		points.add(new Point(0.8, Math.toRadians(2.0), 1.0, 20.0, 2.0, 1.0, false, false));
		points.add(new Point(0.8, Math.toRadians(2.0), 1.0, 20.0, 2.0, 1.0, true, false));
		// Powered base drag: thrusting nozzle exit areas in every assembly with a motor mount.
		points.add(new Point(0.6, 0.0, 0.0, 0.0, 0.0, 0.0, false, true));
		return points;
	}

	/**
	 * The thrusting nozzle exit areas of the nozzle point, by component assembly: every active motor
	 * mount contributes {@code getMotorCount()} nozzles of diameter {@link #NOZZLE_EXIT_DIAMETER_FRACTION}
	 * times {@code getMotorMountDiameter()} to its assembly, as the simulation sums the thrusting motors
	 * (AbstractSimulationStepper.setThrustingNozzleExitAreas). The area depends on the mounts only, not
	 * on the motors, and a mount counts whether or not the configuration gives it a motor.
	 */
	private static Map<ComponentAssembly, Double> nozzleExitAreas(FlightConfiguration config, ComponentIndex index) {
		Map<ComponentAssembly, Double> areas = new LinkedHashMap<>();
		for (RocketComponent c : index.components()) {
			if (c instanceof MotorMount mount && mount.isMotorMount() && config.isComponentActive(c)) {
				double radius = NOZZLE_EXIT_DIAMETER_FRACTION * mount.getMotorMountDiameter() / 2;
				double area = mount.getMotorCount() * Math.PI * MathUtil.pow2(radius);
				areas.merge(c.getAssembly(), area, Double::sum);
			}
		}
		return areas;
	}

	static Map<String, Object> dump(String inputName, Rocket rocket, ComponentIndex index) {
		Map<String, Object> root = Json.object();
		root.put("schema", "aero");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("input", inputName);
		AtmosphericConditions atmosphere = new FlightConditions(null).getAtmosphericConditions();
		Map<String, Object> atmosphereJson = Json.object();
		atmosphereJson.put("temperature", atmosphere.getTemperature());
		atmosphereJson.put("pressure", atmosphere.getPressure());
		atmosphereJson.put("relativeHumidity", atmosphere.getRelativeHumidity());
		atmosphereJson.put("machSpeed", atmosphere.getMachSpeed());
		atmosphereJson.put("density", atmosphere.getDensity());
		atmosphereJson.put("kinematicViscosity", atmosphere.getKinematicViscosity());
		root.put("atmosphere", atmosphereJson);

		root.put("stallAngle", new BarrowmanCalculator().getStallAngle());

		List<Object> configurations = Json.array();
		List<Map<String, Object>> distinctResults = new ArrayList<>();
		List<Integer> distinctIndices = new ArrayList<>();
		FlightConfiguration originallySelected = rocket.getSelectedConfiguration();
		int configIndex = 0;
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			rocket.setSelectedConfiguration(config.getId());
			Map<String, Object> o = Json.object();
			GeometryDumper.putConfigurationHeader(o, configIndex, config);
			o.put("referenceLength", config.getReferenceLength());
			o.put("referenceArea", config.getReferenceArea());
			// A fresh calculator per configuration: BarrowmanStabilityCalculator caches the damping
			// geometry of the active components and voids the cache only on a change of the rocket's
			// modification IDs, which selecting another configuration does not make.
			Map<String, Object> results = results(config, new BarrowmanCalculator(), index);
			// Configurations that differ only in their motors have identical aerodynamics; their
			// results are stored once and referenced by the index of the first such configuration.
			int same = distinctResults.indexOf(results);
			if (same >= 0) {
				o.put("sameResultsAs", distinctIndices.get(same));
			} else {
				o.put("sameResultsAs", null);
				o.putAll(results);
				distinctResults.add(results);
				distinctIndices.add(configIndex);
			}
			configurations.add(o);
			configIndex++;
		}
		rocket.setSelectedConfiguration(originallySelected.getId());
		root.put("configurations", configurations);
		return root;
	}

	/** The geometry warnings, the points and the worst CPs of one configuration. */
	private static Map<String, Object> results(FlightConfiguration config, BarrowmanCalculator calculator,
			ComponentIndex index) {
		Map<String, Object> o = Json.object();
		WarningSet geometryWarnings = new WarningSet();
		calculator.checkGeometry(config, config.getRocket(), geometryWarnings);
		o.put("geometryWarnings", Values.warnings(geometryWarnings, index));

		double structureCGX = MassCalculator.calculateStructure(config).getCM().getX();
		List<Object> points = Json.array();
		for (Point p : points()) {
			points.add(point(p, config, calculator, index, structureCGX));
		}
		o.put("points", points);

		List<Object> worst = Json.array();
		for (double mach : MACHS) {
			FlightConditions conditions = new FlightConditions(config);
			conditions.setMach(mach);
			conditions.setAOA(0.0);
			WarningSet warnings = new WarningSet();
			CoordinateIF cp = calculator.getWorstCP(config, conditions, warnings);
			Map<String, Object> w = Json.object();
			w.put("mach", mach);
			w.put("cp", Values.xyzw(cp));
			w.put("theta", conditions.getTheta());
			worst.add(w);
		}
		o.put("worstCP", worst);
		return o;
	}

	private static Map<String, Object> point(Point p, FlightConfiguration config, BarrowmanCalculator calculator,
			ComponentIndex index, double structureCGX) {
		FlightConditions conditions = new FlightConditions(config);
		conditions.setMach(p.mach());
		conditions.setAOA(p.aoa());
		conditions.setTheta(p.theta());
		conditions.setRollRate(p.rollRate());
		conditions.setPitchRate(p.pitchRate());
		conditions.setYawRate(p.yawRate());
		if (p.pitchCenterAtStructureCG()) {
			conditions.setPitchCenter(new Coordinate(structureCGX, 0, 0));
		}
		if (p.thrustingNozzles()) {
			conditions.setThrustingNozzleExitAreas(nozzleExitAreas(config, index));
		}

		Map<String, Object> o = Json.object();
		Map<String, Object> c = Json.object();
		c.put("mach", conditions.getMach());
		c.put("aoa", conditions.getAOA());
		c.put("theta", conditions.getTheta());
		c.put("rollRate", conditions.getRollRate());
		c.put("pitchRate", conditions.getPitchRate());
		c.put("yawRate", conditions.getYawRate());
		c.put("pitchCenter", Values.xyz(conditions.getPitchCenter()));
		c.put("refLength", conditions.getRefLength());
		c.put("refArea", conditions.getRefArea());
		c.put("velocity", conditions.getVelocity());
		c.put("beta", conditions.getBeta());
		List<Object> nozzles = Json.array();
		for (RocketComponent component : index.components()) {
			if (component instanceof ComponentAssembly assembly
					&& conditions.getThrustingNozzleExitAreas().containsKey(assembly)) {
				Map<String, Object> nozzle = Json.object();
				nozzle.put("assembly", index.path(assembly));
				nozzle.put("area", conditions.getThrustingNozzleExitArea(assembly));
				nozzles.add(nozzle);
			}
		}
		c.put("thrustingNozzleExitAreas", nozzles);
		o.put("conditions", c);

		WarningSet warnings = new WarningSet();
		CoordinateIF cp = calculator.getCP(config, conditions, warnings);
		o.put("cp", Values.xyzw(cp));

		AerodynamicForces total = calculator.getAerodynamicForces(config, conditions, warnings);
		o.put("forces", forces(total));

		Map<RocketComponent, AerodynamicForces> analysis = calculator.getForceAnalysis(config, conditions, warnings);
		List<Object> components = Json.array();
		for (RocketComponent component : index.components()) {
			AerodynamicForces f = analysis.get(component);
			if (f == null) {
				continue;
			}
			Map<String, Object> entry = Json.object();
			entry.put("path", index.path(component));
			entry.putAll(forces(f));
			components.add(entry);
		}
		o.put("components", components);
		o.put("warnings", Values.warnings(warnings, index));
		return o;
	}

	/** The coefficients of one AerodynamicForces, read through its getters (overrides applied). */
	private static Map<String, Object> forces(AerodynamicForces f) {
		Map<String, Object> o = Json.object();
		o.put("cp", Values.xyzw(f.getCP()));
		o.put("cn", f.getCN());
		o.put("cm", f.getCm());
		o.put("cside", f.getCside());
		o.put("cyaw", f.getCyaw());
		o.put("croll", f.getCroll());
		o.put("crollDamp", f.getCrollDamp());
		o.put("crollForce", f.getCrollForce());
		o.put("cd", f.getCD());
		o.put("cdAxial", f.getCDaxial());
		o.put("pressureCD", f.getPressureCD());
		o.put("baseCD", f.getBaseCD());
		o.put("frictionCD", f.getFrictionCD());
		o.put("overrideCD", f.getOverrideCD());
		o.put("pitchDampingMoment", f.getPitchDampingMoment());
		o.put("yawDampingMoment", f.getYawDampingMoment());
		o.put("axisymmetric", f.isAxisymmetric());
		return o;
	}
}
