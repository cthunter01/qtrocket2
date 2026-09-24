package info.qtrocket.goldens;

import java.lang.reflect.Method;
import java.nio.DoubleBuffer;
import java.util.Collection;
import java.util.List;
import java.util.Map;

import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.masscalc.RigidBody;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.BoundingBox;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.Transformation;

/** Conversions of OpenRocket values to the JSON shapes documented in README.md. */
final class Values {

	private Values() {
	}

	/** A position: {@code [x, y, z]} (metres, rocket body frame: x aft from the nose tip). */
	static List<Object> xyz(CoordinateIF c) {
		return Json.numbers(c.getX(), c.getY(), c.getZ());
	}

	/** A weighted coordinate: {@code [x, y, z, weight]} (weight: mass for CGs, CNa for CPs). */
	static List<Object> xyzw(CoordinateIF c) {
		return Json.numbers(c.getX(), c.getY(), c.getZ(), c.getWeight());
	}

	static List<Object> xyzList(CoordinateIF[] coordinates) {
		List<Object> list = Json.array();
		for (CoordinateIF c : coordinates) {
			list.add(xyz(c));
		}
		return list;
	}

	static List<Object> xyzList(Collection<CoordinateIF> coordinates) {
		List<Object> list = Json.array();
		for (CoordinateIF c : coordinates) {
			list.add(xyz(c));
		}
		return list;
	}

	static Map<String, Object> boundingBox(BoundingBox box) {
		Map<String, Object> o = Json.object();
		o.put("min", xyz(box.min));
		o.put("max", xyz(box.max));
		return o;
	}

	/**
	 * A transformation {@code p -> R p + t}: {@code rotation} is R in row-major order (9 numbers),
	 * {@code translation} is t.
	 */
	static Map<String, Object> transformation(Transformation t) {
		DoubleBuffer gl = t.getGLMatrix(); // column-major 4x4: element (row i, column j) at i + 4 j
		List<Object> rotation = Json.array();
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				rotation.add(gl.get(i + 4 * j));
			}
		}
		Map<String, Object> o = Json.object();
		o.put("rotation", rotation);
		o.put("translation", xyz(t.getTranslationVector()));
		return o;
	}

	static Map<String, Object> rigidBody(RigidBody body) {
		Map<String, Object> o = Json.object();
		o.put("mass", body.getMass());
		o.put("cm", xyzw(body.getCM()));
		o.put("ixx", body.getIxx());
		o.put("iyy", body.getIyy());
		o.put("izz", body.getIzz());
		o.put("longitudinalInertia", body.getLongitudinalInertia());
		o.put("rotationalInertia", body.getRotationalInertia());
		return o;
	}

	/**
	 * A warning: its class ({@code type}), priority, description, full text, the paths of its source
	 * components and, for the parameterised warnings, the parameter (speed in m/s or angle in rad).
	 */
	static Map<String, Object> warning(Warning w, ComponentIndex index) {
		Map<String, Object> o = Json.object();
		o.put("type", w.getClass().getSimpleName());
		o.put("priority", w.getPriority() == null ? null : w.getPriority().name());
		o.put("description", w.getMessageDescription());
		o.put("text", w.toString());
		List<Object> sources = Json.array();
		RocketComponent[] components = w.getSources();
		if (components != null) {
			for (RocketComponent c : components) {
				if (c != null) {
					sources.add(index.path(c));
				}
			}
		}
		o.put("sources", sources);
		for (String getter : new String[] { "getAOA", "getSpeed" }) {
			try {
				Method m = w.getClass().getMethod(getter);
				o.put("parameter", ((Number) m.invoke(w)).doubleValue());
			} catch (NoSuchMethodException e) {
				// not a parameterised warning of this kind
			} catch (ReflectiveOperationException e) {
				throw new IllegalStateException(e);
			}
		}
		return o;
	}

	static List<Object> warnings(WarningSet set, ComponentIndex index) {
		List<Object> list = Json.array();
		if (set != null) {
			for (Warning w : set) {
				list.add(warning(w, index));
			}
		}
		return list;
	}
}
