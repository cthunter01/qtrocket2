package info.qtrocket.goldens;

import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.List;
import java.util.Map;

import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.material.Material;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.InstanceContext;
import info.openrocket.core.rocketcomponent.InstanceMap;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.BoundingBox;
import info.openrocket.core.util.CoordinateIF;

/**
 * geometry.json: per-component geometry and mass properties, and per flight configuration the active
 * stages, motors, reference values, bounds and the transform of every active component instance.
 */
final class GeometryDumper {

	/**
	 * Optional getters, read by reflection from every component class that declares them publicly
	 * (JSON key: the getter name without "get"/"is", first letter lower-cased). Their meaning is the
	 * OpenRocket method of the same name.
	 */
	private static final String[] OPTIONAL_GETTERS = {
			// volumes and areas
			"getComponentVolume", "getFullVolume", "getComponentWetArea", "getComponentPlanformArea",
			"getPlanformArea",
			// radii and wall thickness
			"getForeRadius", "getAftRadius", "getMaxRadius", "getOuterRadius", "getInnerRadius", "getThickness",
			"isFilled", "getRadius", "getBodyRadius",
			// transitions and nose cones
			"getShapeType", "getShapeParameter", "isClipped",
			"getForeShoulderRadius", "getForeShoulderLength", "getForeShoulderThickness", "isForeShoulderCapped",
			"getAftShoulderRadius", "getAftShoulderLength", "getAftShoulderThickness", "isAftShoulderCapped",
			// fins
			"getFinCount", "getCantAngle", "getBaseRotation", "getCrossSection", "getSpan", "getRootChord",
			"getTipChord", "getHeight", "getSweep", "getSweepAngle", "getTabHeight", "getTabLength",
			"getTabOffset", "getFilletRadius", "getFinPoints", "getRootPoints", "getTabPoints",
			// external surface
			"getFinish", "getMaterial", "getFilletMaterial",
			// placement of instances
			"getRadiusMethod", "getRadiusOffset", "getAngleMethod", "getAngleOffset", "getRadialPosition",
			"getRadialDirection", "getInstanceSeparation", "getInstanceBoundingBox",
			// motor mounts and clusters
			"isMotorMount", "getMotorOverhang", "getClusterConfiguration", "getClusterScale", "getClusterRotation",
			"getClusterCount",
			// recovery devices and mass objects
			"getDiameter", "getCD", "getLineCount", "getLineLength", "getCordLength", "getStripLength",
			"getStripWidth", "getPackedLength", "getPackedDiameter",
			// the rocket
			"getReferenceType", "getCustomReferenceLength", "isPerfectFinish",
	};

	private GeometryDumper() {
	}

	static Map<String, Object> dump(String inputName, Rocket rocket, ComponentIndex index, WarningSet loadWarnings) {
		Map<String, Object> root = Json.object();
		root.put("schema", "geometry");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("input", inputName);
		root.put("rocketName", rocket.getName());
		root.put("selectedConfiguration", rocket.getSelectedConfiguration().getId().key.toString());
		root.put("loadWarnings", Values.warnings(loadWarnings, index));

		List<Object> components = Json.array();
		for (RocketComponent c : index.components()) {
			components.add(component(c, index));
		}
		root.put("components", components);

		List<Object> configurations = Json.array();
		FlightConfiguration originallySelected = rocket.getSelectedConfiguration();
		int configIndex = 0;
		for (FlightConfiguration config : rocket.getFlightConfigurations()) {
			rocket.setSelectedConfiguration(config.getId());
			configurations.add(configuration(configIndex, config, index));
			configIndex++;
		}
		rocket.setSelectedConfiguration(originallySelected.getId());
		root.put("configurations", configurations);
		return root;
	}

	/** Header fields shared by the per-configuration entries of all golden files. */
	static void putConfigurationHeader(Map<String, Object> o, int configIndex, FlightConfiguration config) {
		o.put("index", configIndex);
		o.put("id", config.getId().key.toString());
		o.put("isDefault", config.getId().isDefaultId());
		o.put("name", config.getName());
	}

	private static Map<String, Object> component(RocketComponent c, ComponentIndex index) {
		Map<String, Object> o = Json.object();
		o.put("path", index.path(c));
		o.put("type", c.getClass().getSimpleName());
		o.put("name", c.getName());
		o.put("id", c.getID().toString());
		o.put("stageNumber", c.getStageNumber());
		o.put("length", c.getLength());
		o.put("axialMethod", c.getAxialMethod() == null ? null : c.getAxialMethod().name());
		o.put("axialOffset", c.getAxialOffset());
		o.put("position", Values.xyz(c.getPosition()));
		o.put("isAerodynamic", c.isAerodynamic());
		o.put("isMassive", c.isMassive());

		o.put("componentMass", c.getComponentMass());
		o.put("componentCG", Values.xyzw(c.getComponentCG()));
		o.put("longitudinalUnitInertia", c.getLongitudinalUnitInertia());
		o.put("rotationalUnitInertia", c.getRotationalUnitInertia());
		o.put("mass", c.getMass());
		o.put("sectionMass", c.getSectionMass());
		o.put("cg", Values.xyzw(c.getCG()));
		o.put("longitudinalInertia", c.getLongitudinalInertia());
		o.put("rotationalInertia", c.getRotationalInertia());

		Map<String, Object> overrides = Json.object();
		overrides.put("massOverridden", c.isMassOverridden());
		overrides.put("overrideMass", c.getOverrideMass());
		overrides.put("cgOverridden", c.isCGOverridden());
		overrides.put("overrideCGX", c.getOverrideCGX());
		overrides.put("cdOverridden", c.isCDOverridden());
		overrides.put("overrideCD", c.getOverrideCD());
		overrides.put("subcomponentsOverriddenMass", c.isSubcomponentsOverriddenMass());
		overrides.put("subcomponentsOverriddenCG", c.isSubcomponentsOverriddenCG());
		overrides.put("subcomponentsOverriddenCD", c.isSubcomponentsOverriddenCD());
		overrides.put("cdOverriddenByAncestor", c.isCDOverriddenByAncestor());
		overrides.put("massOverriddenBy", index.path(c.getMassOverriddenBy()));
		overrides.put("cgOverriddenBy", index.path(c.getCGOverriddenBy()));
		o.put("overrides", overrides);

		o.put("componentBounds", Values.xyzList(c.getComponentBounds()));
		o.put("instanceCount", c.getInstanceCount());
		o.put("instanceOffsets", Values.xyzList(c.getInstanceOffsets()));
		o.put("instanceAngles", Json.numbers(c.getInstanceAngles()));
		o.put("instanceLocations", Values.xyzList(c.getInstanceLocations()));
		o.put("componentLocations", Values.xyzList(c.getComponentLocations()));
		o.put("componentAngles", Values.xyzList(c.getComponentAngles()));

		Map<String, Object> details = Json.object();
		for (String getter : OPTIONAL_GETTERS) {
			Method m = publicGetter(c.getClass(), getter);
			if (m == null) {
				continue;
			}
			Object value;
			try {
				value = m.invoke(c);
			} catch (ReflectiveOperationException e) {
				throw new IllegalStateException("Calling " + c.getClass().getSimpleName() + "." + getter, e);
			}
			details.put(key(getter), toJson(value));
		}
		o.put("details", details);
		return o;
	}

	private static Method publicGetter(Class<?> type, String name) {
		try {
			Method m = type.getMethod(name);
			if (Modifier.isStatic(m.getModifiers()) || m.getReturnType() == void.class) {
				return null;
			}
			return m;
		} catch (NoSuchMethodException e) {
			return null;
		}
	}

	private static String key(String getter) {
		String stripped = getter.startsWith("is") ? getter.substring(2) : getter.substring(3);
		return Character.toLowerCase(stripped.charAt(0)) + stripped.substring(1);
	}

	/** JSON form of a value returned by one of the optional getters. */
	static Object toJson(Object value) {
		if (value == null || value instanceof Number || value instanceof Boolean || value instanceof String) {
			return value;
		}
		if (value instanceof Enum<?> e) {
			return e.name();
		}
		if (value instanceof CoordinateIF c) {
			return Values.xyzw(c);
		}
		if (value instanceof CoordinateIF[] array) {
			List<Object> list = Json.array();
			for (CoordinateIF c : array) {
				list.add(Values.xyz(c));
			}
			return list;
		}
		if (value instanceof double[] array) {
			return Json.numbers(array);
		}
		if (value instanceof BoundingBox box) {
			return Values.boundingBox(box);
		}
		if (value instanceof Material m) {
			Map<String, Object> o = Json.object();
			o.put("name", m.getName());
			o.put("type", m.getType().name());
			o.put("density", m.getDensity());
			return o;
		}
		if (value instanceof Collection<?> collection) {
			List<Object> list = Json.array();
			for (Object element : collection) {
				list.add(toJson(element));
			}
			return list;
		}
		// Other value types (e.g. cluster configurations) are described by their text, which must not be
		// Object.toString()'s identity hash (it would change from run to run).
		try {
			if (value.getClass().getMethod("toString").getDeclaringClass() == Object.class) {
				throw new IllegalStateException("No stable text for a " + value.getClass().getName());
			}
		} catch (NoSuchMethodException e) {
			throw new IllegalStateException(e);
		}
		return value.toString();
	}

	private static Map<String, Object> configuration(int configIndex, FlightConfiguration config,
			ComponentIndex index) {
		Map<String, Object> o = Json.object();
		putConfigurationHeader(o, configIndex, config);
		o.put("stageCount", config.getStageCount());
		List<Object> active = Json.array();
		for (AxialStage stage : config.getActiveStages()) {
			active.add(stage.getStageNumber());
		}
		o.put("activeStages", active);
		o.put("referenceLength", config.getReferenceLength());
		o.put("referenceArea", config.getReferenceArea());
		o.put("length", config.getLength());
		o.put("lengthAerodynamic", config.getLengthAerodynamic());
		o.put("boundingBox", Values.boundingBox(config.getBoundingBox()));
		o.put("boundingBoxAerodynamic", Values.boundingBox(config.getBoundingBoxAerodynamic()));
		o.put("hasMotors", config.hasMotors());
		o.put("hasRecoveryDevice", config.hasRecoveryDevice());
		o.put("motors", motors(config, index));

		List<Object> activeComponents = Json.array();
		for (RocketComponent c : index.components()) {
			if (config.isComponentActive(c)) {
				activeComponents.add(index.path(c));
			}
		}
		o.put("activeComponents", activeComponents);

		InstanceMap map = config.getActiveInstances();
		List<Object> instances = Json.array();
		for (RocketComponent c : index.components()) {
			List<InstanceContext> contexts = map.get(c);
			if (contexts == null) {
				continue;
			}
			Map<String, Object> entry = Json.object();
			entry.put("path", index.path(c));
			List<Object> list = Json.array();
			for (InstanceContext context : contexts) {
				Map<String, Object> ctx = Json.object();
				ctx.put("instanceNumber", context.instanceNumber);
				ctx.put("location", Values.xyz(context.getLocation()));
				ctx.put("transform", Values.transformation(context.transform));
				ctx.put("parentTransform", Values.transformation(context.getParentTransform()));
				list.add(ctx);
			}
			entry.put("instances", list);
			instances.add(entry);
		}
		o.put("instances", instances);
		return o;
	}

	private static List<Object> motors(FlightConfiguration config, ComponentIndex index) {
		List<MotorConfiguration> motors = new ArrayList<>(config.getActiveMotors());
		motors.sort(Comparator.comparing(mc -> index.path((RocketComponent) mc.getMount())));
		List<Object> list = Json.array();
		for (MotorConfiguration mc : motors) {
			MotorMount mount = mc.getMount();
			Motor motor = mc.getMotor();
			Map<String, Object> o = Json.object();
			o.put("mount", index.path((RocketComponent) mount));
			o.put("motorName", mc.toMotorName());
			o.put("designation", motor == null ? null : motor.getDesignation());
			o.put("manufacturer", motor instanceof ThrustCurveMotor t ? t.getManufacturer().getSimpleName() : null);
			o.put("digest", motor == null ? null : motor.getDigest());
			o.put("ejectionDelay", mc.getEjectionDelay());
			o.put("ignitionEvent", mc.getIgnitionEvent() == null ? null : mc.getIgnitionEvent().name());
			o.put("ignitionDelay", mc.getIgnitionDelay());
			o.put("motorCount", mount.getMotorCount());
			o.put("motorCountIncludingAssemblyCopies", mount.getMotorCountIncludingAssemblyCopies());
			o.put("motorOverhang", mount.getMotorOverhang());
			o.put("position", Values.xyz(mc.getPosition()));
			list.add(o);
		}
		return list;
	}
}
