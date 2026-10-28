package info.qtrocket.guireference;

import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.motor.IgnitionEvent;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;

/**
 * QtRocket's fix of OpenRocket's bug 6 (plan section 10.4, the user's U-M3-5), made in OpenRocket's
 * undo history for scenarios of version 2, as the flight-config pins of tools/openrocket-gui-pins make
 * it (FlightConfigurationOperationsProbe): a copy of OpenRocket's rocket gives every motor mount a new
 * default ignition (AUTOMATIC without delay and not overridden; MotorConfigurationSet.java:30-40), so
 * that the states of the undo history (copyWithOriginalID()) have lost the mounts' default ignitions and
 * an undo or a redo gives every mount that new default. QtRocket's MotorConfigurationSet keeps a
 * mount's default through the copies.
 * <p>
 * The tool keeps beside each state of the document's undo history the default ignitions the rocket had
 * when the state was stored, which is at the start of the step that stored it (every step that stores a
 * state stores it before it changes a default: addUndoPosition() before the change, undo() and redo()
 * before they load another state), and gives the mounts those defaults back after an undo or a redo, as
 * QtRocket's history has them. Every other step is OpenRocket's as it is.
 */
final class DefaultIgnitions {

	private final OpenRocketDocument document;
	/** The default ignitions (event, delay, override flag) by mount id, of each state of the undo history. */
	private final Map<Rocket, Map<String, Object[]>> history = new IdentityHashMap<>();
	/** The default ignitions at the start of the current step. */
	private Map<String, Object[]> atStepStart = new HashMap<>();

	DefaultIgnitions(OpenRocketDocument document) {
		this.document = document;
	}

	/** Before a step: the default ignitions a state the step stores has. */
	void beginStep() {
		atStepStart = current();
	}

	/** After a step: the states the step stored get the default ignitions of its start. */
	void endStep() {
		recordStates();
	}

	/** After an undo or a redo: the mounts get the default ignitions of the state the document loaded. */
	void restoreAfterUndoOrRedo() {
		recordStates();
		List<Rocket> states = undoHistory();
		int position = (Integer) Reflection.get(OpenRocketDocument.class, document, "undoPosition");
		Map<String, Object[]> stored = history.get(states.get(position));
		if (stored == null) {
			throw new IllegalStateException("no default ignitions are kept for the state an undo or redo loaded");
		}
		for (RocketComponent c : document.getRocket()) {
			if (c instanceof MotorMount mount && stored.containsKey(c.getID().toString())) {
				Object[] values = stored.get(c.getID().toString());
				MotorConfiguration defaults = mount.getDefaultMotorConfig();
				set(defaults, "ignitionEvent", values[0]);
				set(defaults, "ignitionDelay", values[1]);
				set(defaults, "ignitionOveride", values[2]);
			}
		}
	}

	private void recordStates() {
		for (Rocket state : undoHistory()) {
			history.putIfAbsent(state, atStepStart);
		}
	}

	@SuppressWarnings("unchecked")
	private List<Rocket> undoHistory() {
		return (List<Rocket>) Reflection.get(OpenRocketDocument.class, document, "undoHistory");
	}

	/** The default ignition (event, delay, override flag) of every motor mount, by the mount's id. */
	private Map<String, Object[]> current() {
		Map<String, Object[]> map = new HashMap<>();
		for (RocketComponent c : document.getRocket()) {
			if (c instanceof MotorMount mount) {
				MotorConfiguration defaults = mount.getDefaultMotorConfig();
				IgnitionEvent event = defaults.getIgnitionEvent();
				map.put(c.getID().toString(),
						new Object[] { event, defaults.getIgnitionDelay(), defaults.hasIgnitionOverride() });
			}
		}
		return map;
	}

	private static void set(MotorConfiguration configuration, String name, Object value) {
		Reflection.set(MotorConfiguration.class, configuration, name, value);
	}
}
