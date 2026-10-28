package info.qtrocket.guireference;

import java.awt.Component;
import java.awt.Container;
import java.awt.Window;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JOptionPane;
import javax.swing.JRadioButton;
import javax.swing.JSpinner;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.SpinnerModel;
import javax.swing.SwingUtilities;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.motor.IgnitionEvent;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration.DeployEvent;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration.SeparationEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.ExpressionParser;
import info.openrocket.core.util.InvalidExpressionException;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.dialogs.flightconfiguration.DeploymentSelectionDialog;
import info.openrocket.swing.gui.dialogs.flightconfiguration.IgnitionSelectionDialog;
import info.openrocket.swing.gui.dialogs.flightconfiguration.RenameConfigDialog;
import info.openrocket.swing.gui.dialogs.flightconfiguration.SeparationSelectionDialog;
import info.openrocket.swing.gui.dialogs.motor.MotorChooserDialog;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorSelectionPanel;

/**
 * The modal questions of OpenRocket's Motors &amp; Configuration tab, answered by a step of a version 2
 * scenario from inside their own modal loops, as a user answers them: OpenRocket's own dialogs and their
 * listeners run (MotorChooserDialog, IgnitionSelectionDialog, DeploymentSelectionDialog,
 * SeparationSelectionDialog, RenameConfigDialog and the message of its "Save as default").
 * <p>
 * The operation runs on the event dispatch thread. Before it starts, the answer is queued on that thread
 * ({@link SwingUtilities#invokeLater}): a modal dialog's {@code setVisible(true)} pumps the queue while it is
 * open, so the answer runs inside the dialog's modal loop, with the dialog shown, and closes it; the
 * operation then goes on with what the dialog left. When the operation asks nothing, the queued answer
 * runs after it and finds no dialog. An answer that fails closes its dialog (so the operation goes on) and
 * the failure is rethrown when the operation is over.
 * <p>
 * Each question is recorded as it opened ({@code question()}: the dialog's title and fields) with what the
 * dialog left when it was answered, for state.json (README.md, "The questions").
 */
final class Questions {

	private static final Translator trans = Application.getTranslator();

	/** The questions the last operation asked, in order (a nested message inside its question). */
	private final List<JsonObject> asked = new ArrayList<>();
	/** What the cell an operation acts on first holds, for the motor chooser's checks (null for none). */
	private MotorConfiguration chooserCell;
	private Throwable failure;

	List<JsonObject> asked() {
		return asked;
	}

	void clear() {
		asked.clear();
	}

	/**
	 * Runs @p operation (on the event dispatch thread) and answers the modal question it asks with @p answer:
	 * a step that has an answer must be asked a question, one without must not be. @p cell is the motor
	 * configuration of the first cell for the motor chooser's checks (null otherwise).
	 */
	void run(Runnable operation, JsonObject answer, MotorConfiguration cell) {
		if (!SwingUtilities.isEventDispatchThread()) {
			throw new IllegalStateException("an operation of the tab runs on the event dispatch thread");
		}
		failure = null;
		chooserCell = cell;
		Set<Window> before = new HashSet<>(Arrays.asList(Window.getWindows()));
		boolean[] answered = new boolean[1];
		SwingUtilities.invokeLater(() -> {
			JDialog dialog = shownDialog(before);
			if (dialog == null) {
				return;
			}
			answered[0] = true;
			answerSafely(dialog, answer);
		});
		operation.run();
		// The queued answer has run inside the dialog's loop; or, when nothing was asked, it is still queued.
		flushQueue();
		if (failure != null) {
			Throwable f = failure;
			failure = null;
			throw new IllegalStateException("the answer to the question failed", f);
		}
		if (answered[0] && answer == null) {
			throw new IllegalStateException("the operation asked a question the step has no answer for");
		}
		if (!answered[0] && answer != null) {
			throw new IllegalStateException("the step has an answer, but the operation asked no question");
		}
	}

	/** Runs what is queued on the event dispatch thread now (the answer queued for an operation that asked nothing). */
	private static void flushQueue() {
		java.awt.EventQueue queue = java.awt.Toolkit.getDefaultToolkit().getSystemEventQueue();
		java.awt.SecondaryLoop loop = queue.createSecondaryLoop();
		SwingUtilities.invokeLater(loop::exit);
		loop.enter();
	}

	/** The modal dialog an operation shows: one that was not open before, or the motor chooser (which is kept). */
	private static JDialog shownDialog(Set<Window> before) {
		JDialog found = null;
		for (Window w : Window.getWindows()) {
			if (w instanceof JDialog d && d.isShowing() && d.isModal()
					&& (!before.contains(w) || d instanceof MotorChooserDialog)) {
				found = d;
			}
		}
		return found;
	}

	private void answerSafely(JDialog dialog, JsonObject answer) {
		try {
			if (answer == null) {
				throw new IllegalStateException("no answer for the dialog " + dialog.getTitle());
			}
			answer(dialog, answer);
		} catch (Throwable t) {
			if (failure == null) {
				failure = t;
			}
		} finally {
			if (dialog.isVisible()) {
				// A failed answer leaves its dialog: closed, so that the operation goes on.
				dialog.setVisible(false);
			}
		}
	}

	private void answer(JDialog dialog, JsonObject answer) throws Exception {
		if (dialog instanceof MotorChooserDialog chooser) {
			motor(chooser, answer);
		} else if (dialog instanceof IgnitionSelectionDialog) {
			ignition(dialog, answer);
		} else if (dialog instanceof DeploymentSelectionDialog) {
			valueDialog(dialog, answer, "deployment");
		} else if (dialog instanceof SeparationSelectionDialog) {
			valueDialog(dialog, answer, "separation");
		} else if (dialog instanceof RenameConfigDialog) {
			rename(dialog, answer);
		} else {
			throw new IllegalStateException("an unexpected dialog opened: " + dialog.getTitle());
		}
	}

	// ------------------------------------------------------------------ the value dialogs

	/**
	 * IgnitionSelectionDialog: the radio button of the scope, the ignition event and the delay (a spinner in
	 * seconds without a unit selector), then OK; or Cancel alone. An OK answer gives every field (bug 2: the
	 * dialog shows the cell's own configuration's values where QtRocket's question shows the mount's default's
	 * when it starts on "all configurations"; with every value given, both end with the same).
	 */
	private void ignition(JDialog dialog, JsonObject answer) {
		JsonObject question = valueQuestion(dialog, "ignition");
		JComboBox<?> combo = of(dialog, JComboBox.class).get(0);
		question.addProperty("event", ((Enum<?>) combo.getSelectedItem()).name());
		question.addProperty("delay", Double.toString(spinnerModel(of(dialog, JSpinner.class).get(0)).getValue()));
		asked.add(question);
		if (cancelled(dialog, answer)) {
			return;
		}
		members(answer, Set.of("scope", "event", "delay", "button"), Set.of("scope", "event", "delay", "button"));
		scope(dialog, string(answer, "scope"));
		choose(combo, IgnitionEvent.valueOf(string(answer, "event")));
		type(of(dialog, JSpinner.class).get(0), answer.getAsJsonObject("delay"), false);
		click(dialog, button(answer, "ok"));
	}

	/**
	 * DeploymentSelectionDialog and SeparationSelectionDialog: the scope, the event, the altitude (a spinner
	 * with a unit selector, which the dialog enables for the altitude events only) and the delay (seconds,
	 * without a selector), then OK; or Cancel alone. An OK answer gives every enabled field.
	 */
	private void valueDialog(JDialog dialog, JsonObject answer, String kind) {
		JsonObject question = valueQuestion(dialog, kind);
		JComboBox<?> combo = of(dialog, JComboBox.class).get(0);
		List<JSpinner> spinners = of(dialog, JSpinner.class);
		question.addProperty("event", ((Enum<?>) combo.getSelectedItem()).name());
		question.addProperty("altitude", Double.toString(spinnerModel(spinners.get(0)).getValue()));
		question.addProperty("altitudeEnabled", spinners.get(0).isEnabled());
		question.addProperty("delay", Double.toString(spinnerModel(spinners.get(1)).getValue()));
		JsonArray events = new JsonArray();
		for (int i = 0; i < combo.getItemCount(); i++) {
			events.add(((Enum<?>) combo.getItemAt(i)).name());
		}
		question.add("events", events);
		asked.add(question);
		if (cancelled(dialog, answer)) {
			return;
		}
		scope(dialog, string(answer, "scope"));
		String event = string(answer, "event");
		choose(combo, kind.equals("deployment") ? DeployEvent.valueOf(event) : SeparationEvent.valueOf(event));
		// DeploymentSelectionDialog.updateState(), SeparationSelectionDialog.updateState(): the altitude
		// fields are enabled for the altitude events alone.
		boolean altitude = spinners.get(0).isEnabled();
		Set<String> required = new HashSet<>(Set.of("scope", "event", "delay", "button"));
		if (altitude) {
			required.add("altitude");
		}
		members(answer, required, required);
		if (altitude) {
			type(spinners.get(0), answer.getAsJsonObject("altitude"), true);
		}
		type(spinners.get(1), answer.getAsJsonObject("delay"), false);
		click(dialog, button(answer, "ok"));
	}

	/** The title and the radio buttons of a value dialog as it opened: the scope it starts on, the override's text. */
	private static JsonObject valueQuestion(JDialog dialog, String kind) {
		JsonObject question = new JsonObject();
		question.addProperty("question", kind);
		question.addProperty("title", dialog.getTitle());
		List<JRadioButton> radios = of(dialog, JRadioButton.class);
		question.addProperty("scope", radios.get(0).isSelected() ? "all" : "this");
		question.addProperty("default", radios.get(0).getText());
		question.addProperty("override", radios.get(1).getText());
		return question;
	}

	/** A Cancel answer ({"button": "cancel"} alone): Cancel clicked, true. */
	private static boolean cancelled(JDialog dialog, JsonObject answer) {
		if (!"cancel".equals(string(answer, "button"))) {
			return false;
		}
		members(answer, Set.of("button"), Set.of("button"));
		click(dialog, trans.get("button.cancel"));
		return true;
	}

	/** The radio button of @p scope ("all": the default, "this": the override), clicked when it is not selected. */
	private static void scope(JDialog dialog, String scope) {
		List<JRadioButton> radios = of(dialog, JRadioButton.class);
		JRadioButton radio = switch (scope) {
			case "all" -> radios.get(0);
			case "this" -> radios.get(1);
			default -> throw new IllegalArgumentException("a scope is all or this, not " + scope);
		};
		if (!radio.isSelected()) {
			radio.doClick(0);
		}
	}

	/** @p item chosen in @p combo when it is not the selected one; only the combo's own items. */
	private static void choose(JComboBox<?> combo, Object item) {
		boolean found = false;
		for (int i = 0; i < combo.getItemCount(); i++) {
			found |= combo.getItemAt(i) == item;
		}
		if (!found) {
			throw new IllegalArgumentException("the combo box has no item " + item);
		}
		if (combo.getSelectedItem() != item) {
			combo.setSelectedItem(item);
		}
	}

	/**
	 * {"text": t, "unit": u} typed into @p spinner: with a unit selector, u is chosen in it first (the model's
	 * setCurrentUnit(), as UnitSelector does); without one, u must be the unit the spinner shows. The text is
	 * committed (the spinner model's setValue(String), as its editor does) when its value, parsed as the
	 * spinner's ExpressionParser does and converted from the unit, differs from the field's: a value the field
	 * already shows is left alone, as a user who leaves the field as it is.
	 */
	private static void type(JSpinner spinner, JsonObject value, boolean unitSelector) {
		if (value == null) {
			throw new IllegalArgumentException("a typed value is {\"text\": ..., \"unit\": ...}");
		}
		members(value, Set.of("text", "unit"), Set.of("text", "unit"));
		if (!spinner.isEnabled()) {
			throw new IllegalStateException("the field is disabled");
		}
		DoubleModel model = spinnerModel(spinner);
		String unitName = string(value, "unit");
		if (unitSelector) {
			model.setCurrentUnit(unit(model.getUnitGroup(), unitName));
		} else if (!model.getCurrentUnit().getUnit().equals(unitName)) {
			throw new IllegalArgumentException("the field shows " + model.getCurrentUnit().getUnit() + ", not "
					+ unitName + " (it has no unit selector)");
		}
		String text = string(value, "text");
		double typed;
		try {
			typed = model.getCurrentUnit().fromUnit(new ExpressionParser().parse(text));
		} catch (InvalidExpressionException e) {
			throw new IllegalArgumentException("the field does not take '" + text + "'", e);
		}
		if (typed != model.getValue()) {
			spinner.getModel().setValue(text);
		}
	}

	private static Unit unit(UnitGroup group, String name) {
		for (int i = 0; i < group.getUnitCount(); i++) {
			if (group.getUnit(i).getUnit().equals(name)) {
				return group.getUnit(i);
			}
		}
		throw new IllegalArgumentException("the unit selector has no unit '" + name + "'");
	}

	/** The DoubleModel of a spinner made with a DoubleModel's spinner model (an inner class of DoubleModel). */
	private static DoubleModel spinnerModel(JSpinner spinner) {
		SpinnerModel model = spinner.getModel();
		return (DoubleModel) Reflection.get(model.getClass(), model, "this$0");
	}

	// ------------------------------------------------------------------ the rename dialog

	/**
	 * RenameConfigDialog: {"name": the text typed into its field (optional), "saveAsDefault": true (optional,
	 * "Save as default" clicked once after the name, its message answered OK), "button": "ok" | "reset" |
	 * "cancel"}.
	 */
	private void rename(JDialog dialog, JsonObject answer) {
		members(answer, Set.of("button"), Set.of("name", "saveAsDefault", "button"));
		JTextField field = textField(dialog);
		JsonObject question = new JsonObject();
		question.addProperty("question", "rename");
		question.addProperty("title", dialog.getTitle());
		question.addProperty("name", field.getText());
		asked.add(question);
		if (answer.has("name")) {
			field.setText(string(answer, "name"));
		}
		if (answer.has("saveAsDefault")) {
			if (!answer.get("saveAsDefault").getAsBoolean()) {
				throw new IllegalArgumentException("saveAsDefault is true or left out");
			}
			// JOptionPane.showMessageDialog (RenameConfigDialog.java:66-67), answered from inside its loop.
			Set<Window> before = new HashSet<>(Arrays.asList(Window.getWindows()));
			SwingUtilities.invokeLater(() -> {
				JDialog message = shownDialog(before);
				try {
					if (message == null) {
						throw new IllegalStateException("Save as default showed no message");
					}
					JOptionPane pane = of(message, JOptionPane.class).get(0);
					JsonObject shown = new JsonObject();
					shown.addProperty("question", "message");
					shown.addProperty("title", message.getTitle());
					shown.addProperty("text", String.valueOf(pane.getMessage()));
					asked.add(shown);
					click(message, javax.swing.UIManager.getString("OptionPane.okButtonText"));
				} catch (Throwable t) {
					if (failure == null) {
						failure = t;
					}
					if (message != null && message.isVisible()) {
						message.setVisible(false);
					}
				}
			});
			click(dialog, trans.get("RenameConfigDialog.but.saveDefault"));
		}
		String button = switch (string(answer, "button")) {
			case "ok" -> trans.get("button.ok");
			case "reset" -> trans.get("RenameConfigDialog.but.reset");
			case "cancel" -> trans.get("button.cancel");
			default -> throw new IllegalArgumentException("the rename dialog's buttons are ok, reset and cancel");
		};
		click(dialog, button);
	}

	private static JTextField textField(JDialog dialog) {
		List<JTextField> fields = new ArrayList<>();
		for (JTextField f : of(dialog, JTextField.class)) {
			if (!(f.getParent() instanceof JSpinner.DefaultEditor)) {
				fields.add(f);
			}
		}
		if (fields.size() != 1) {
			throw new IllegalStateException("the dialog has " + fields.size() + " text fields");
		}
		return fields.get(0);
	}

	// ------------------------------------------------------------------ the motor chooser

	/**
	 * MotorChooserDialog, after MotorConfigurationPanel.selectMotor() opened it (setMotorMountAndConfig() and
	 * open()): {"search": the text typed into the search field, "motor": {"manufacturer": the name the table's
	 * Manufacturer column shows, "designation": the set's designation} (a click on that row of the table),
	 * "curve": the digest of the thrust curve chosen in the curve box, "delay": the text chosen or typed in
	 * the delay box ("Plugged" or a number of seconds), "nozzleExitDiameter": {"text", "unit"}, "button": "ok"
	 * | "cancel"}, every member but the button optional, applied in this order.
	 * <p>
	 * The tool fails where OpenRocket's chooser shows one of the bugs QtRocket fixes (plan 10.3, the user's
	 * U-M3-5 and the answers of 2026-10-26 and 2026-10-27): the chooser opened for a cell with a motor shows
	 * another motor (bug 21, a row left selected from an earlier opening) or another delay (bug 8, a
	 * configured delay that is no standard one becomes the closest standard one); a click on a row or a curve
	 * ends on another curve than the one chosen (bug 20, the curve box's re-entrant select() with "Hide very
	 * similar thrust curves"); a delay text Java parses to NaN or an infinity (bug 11). The window's close
	 * button (bug 7) is never used.
	 */
	private void motor(MotorChooserDialog chooser, JsonObject answer) {
		members(answer, Set.of("button"),
				Set.of("search", "motor", "curve", "delay", "nozzleExitDiameter", "button"));
		ThrustCurveMotorSelectionPanel panel = (ThrustCurveMotorSelectionPanel) Reflection
				.get(MotorChooserDialog.class, chooser, "selectionPanel");
		JsonObject question = new JsonObject();
		question.addProperty("question", "motor");
		question.addProperty("title", chooser.getTitle());
		chooserState(panel).entrySet().forEach(e -> question.add(e.getKey(), e.getValue()));
		asked.add(question);
		if (chooserCell != null && chooserCell.hasMotor()) {
			if (panel.getSelectedMotor() != chooserCell.getMotor()) {
				throw new IllegalStateException("bug 21: the chooser opened for a cell with "
						+ chooserCell.getMotor().getDesignation() + " shows " + motorName(panel.getSelectedMotor()));
			}
			// A NaN delay excepted, which QtRocket's chooser opens as OpenRocket's does.
			if (!Double.isNaN(chooserCell.getEjectionDelay())
					&& Double.compare(panel.getSelectedDelay(), chooserCell.getEjectionDelay()) != 0) {
				throw new IllegalStateException("bug 8: the chooser opened for a cell with the delay "
						+ chooserCell.getEjectionDelay() + " shows the delay " + panel.getSelectedDelay());
			}
		}
		if (answer.has("search")) {
			JTextField search = (JTextField) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "searchField");
			search.setText(string(answer, "search"));
		}
		if (answer.has("motor")) {
			clickMotorRow(panel, answer.getAsJsonObject("motor"));
		}
		if (answer.has("curve")) {
			chooseCurve(panel, string(answer, "curve"));
		}
		if (answer.has("delay")) {
			chooseDelay(panel, string(answer, "delay"));
		}
		if (answer.has("nozzleExitDiameter")) {
			JSpinner nozzle = (JSpinner) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel,
					"nozzleExitDiameterSpinner");
			type(nozzle, answer.getAsJsonObject("nozzleExitDiameter"), true);
		}
		question.add("answered", chooserState(panel));
		String button = switch (string(answer, "button")) {
			case "ok" -> trans.get("dlg.but.ok");
			case "cancel" -> trans.get("dlg.but.cancel");
			default -> throw new IllegalArgumentException("the chooser's buttons are ok and cancel");
		};
		click(chooser, button);
	}

	/** What the chooser shows: the search text, the number of rows, the motor, its delay and nozzle diameter. */
	private static JsonObject chooserState(ThrustCurveMotorSelectionPanel panel) {
		JsonObject state = new JsonObject();
		JTextField search = (JTextField) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "searchField");
		JTable table = (JTable) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "table");
		JComboBox<?> delays = (JComboBox<?>) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "delayBox");
		state.addProperty("search", search.getText());
		state.addProperty("rows", table.getRowCount());
		ThrustCurveMotor motor = (ThrustCurveMotor) panel.getSelectedMotor();
		state.add("motor", motor == null ? com.google.gson.JsonNull.INSTANCE : motorJson(motor));
		Object delay = delays.getSelectedItem();
		state.add("delayText", delay == null ? com.google.gson.JsonNull.INSTANCE
				: new com.google.gson.JsonPrimitive(delay.toString()));
		state.addProperty("delay", Double.toString(panel.getSelectedDelay()));
		state.addProperty("nozzleExitDiameter", Double.toString(panel.getSelectedNozzleExitDiameter()));
		return state;
	}

	static JsonObject motorJson(ThrustCurveMotor motor) {
		JsonObject object = new JsonObject();
		object.addProperty("manufacturer", motor.getManufacturer().getDisplayName());
		object.addProperty("designation", motor.getDesignation());
		object.addProperty("digest", motor.getDigest());
		return object;
	}

	private static String motorName(Motor motor) {
		return motor == null ? "no motor" : motor.getDesignation();
	}

	/** A click on the table row of the motor set, which must be shown (the filters and the search let it through). */
	private static void clickMotorRow(ThrustCurveMotorSelectionPanel panel, JsonObject motor) {
		members(motor, Set.of("manufacturer", "designation"), Set.of("manufacturer", "designation"));
		JTable table = (JTable) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "table");
		Object model = Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "model");
		int row = -1;
		ThrustCurveMotorSet set = null;
		for (int view = 0; view < table.getRowCount(); view++) {
			ThrustCurveMotorSet s = (ThrustCurveMotorSet) Reflection.invoke(model, "getMotorSet",
					new Class<?>[] { int.class }, table.convertRowIndexToModel(view));
			if (s.getManufacturer().getDisplayName().equals(string(motor, "manufacturer"))
					&& s.getDesignation().equals(string(motor, "designation"))) {
				if (set != null) {
					throw new IllegalArgumentException("two rows of the chooser show " + motor);
				}
				row = view;
				set = s;
			}
		}
		if (set == null) {
			throw new IllegalArgumentException("the chooser shows no row of " + motor);
		}
		// ThrustCurveMotorSelectionPanel.selectMotorFromTable(): the set's curve that select() is given.
		Object selectedSet = Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "selectedMotorSet");
		Motor expected = selectedSet == set ? panel.getSelectedMotor()
				: (Motor) Reflection.invoke(panel, "selectMotor", new Class<?>[] { ThrustCurveMotorSet.class }, set);
		// A click on the row (BasicTableUI: changeSelection(row, column, false, false)).
		table.changeSelection(row, 0, false, false);
		if (panel.getSelectedMotor() != expected) {
			throw new IllegalStateException("bug 20: the click on " + motor + " chose " + motorName(expected)
					+ ", the chooser shows " + motorName(panel.getSelectedMotor()));
		}
	}

	/** The curve of @p digest chosen in the curve box (enabled for a set with more than one curve shown). */
	@SuppressWarnings("unchecked")
	private static void chooseCurve(ThrustCurveMotorSelectionPanel panel, String digest) {
		JComboBox<Object> curves = (JComboBox<Object>) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel,
				"curveSelectionBox");
		if (!curves.isEnabled()) {
			throw new IllegalStateException("the curve box is disabled");
		}
		Object holder = null;
		ThrustCurveMotor motor = null;
		for (int i = 0; i < curves.getItemCount(); i++) {
			ThrustCurveMotor m = (ThrustCurveMotor) Reflection.invoke(curves.getItemAt(i), "getMotor");
			if (m.getDigest().equals(digest)) {
				holder = curves.getItemAt(i);
				motor = m;
			}
		}
		if (holder == null) {
			throw new IllegalArgumentException("the curve box has no curve " + digest);
		}
		if (curves.getSelectedItem() != holder) {
			curves.setSelectedItem(holder);
		}
		if (panel.getSelectedMotor() != motor) {
			throw new IllegalStateException("bug 20: the curve " + digest + " was chosen, the chooser shows "
					+ ((ThrustCurveMotor) panel.getSelectedMotor()).getDigest());
		}
	}

	/** @p text chosen or typed in the delay box (an editable JComboBox: setSelectedItem() as its editor commits). */
	private static void chooseDelay(ThrustCurveMotorSelectionPanel panel, String text) {
		JComboBox<?> delays = (JComboBox<?>) Reflection.get(ThrustCurveMotorSelectionPanel.class, panel, "delayBox");
		if (!delays.isEnabled()) {
			throw new IllegalStateException("the delay box is disabled (no motor is selected)");
		}
		double expected;
		if (text.equalsIgnoreCase(trans.get("TCMotorSelPan.delayBox.Plugged"))
				|| text.equalsIgnoreCase(trans.get("TCMotorSelPan.delayBox.PluggedNone"))) {
			expected = Motor.PLUGGED_DELAY;
		} else {
			expected = Double.parseDouble(text);
			if (!Double.isFinite(expected)) {
				throw new IllegalArgumentException("bug 11: OpenRocket takes the delay '" + text
						+ "', which QtRocket refuses");
			}
		}
		delays.setSelectedItem(text);
		if (Double.compare(panel.getSelectedDelay(), expected) != 0) {
			throw new IllegalStateException("the delay box took '" + text + "' as " + panel.getSelectedDelay());
		}
	}

	// ------------------------------------------------------------------ helpers

	/** The text of the button @p answer names, which must be @p only. */
	private static String button(JsonObject answer, String only) {
		if (!only.equals(string(answer, "button"))) {
			throw new IllegalArgumentException("the answer's button is " + only + " or cancel");
		}
		return trans.get("button." + only);
	}

	/** Checks that @p object has every member of @p required and no member outside @p allowed. */
	static void members(JsonObject object, Set<String> required, Set<String> allowed) {
		for (String key : object.keySet()) {
			if (!allowed.contains(key)) {
				throw new IllegalArgumentException("unknown member '" + key + "' (allowed: " + allowed + ")");
			}
		}
		for (String key : required) {
			if (!object.has(key)) {
				throw new IllegalArgumentException("the answer needs '" + key + "'");
			}
		}
	}

	static String string(JsonObject object, String key) {
		JsonElement element = object.get(key);
		return element == null || element.isJsonNull() ? null : element.getAsString();
	}

	static void click(JDialog dialog, String text) {
		for (JButton b : of(dialog, JButton.class)) {
			if (text.equals(b.getText())) {
				if (!b.isEnabled()) {
					throw new IllegalStateException("the button " + text + " is disabled");
				}
				b.doClick(0);
				return;
			}
		}
		throw new IllegalStateException("the dialog " + dialog.getTitle() + " has no button " + text);
	}

	/** Every component of @p container, depth first (not into spinners and combo boxes). */
	static List<Component> all(Container container) {
		List<Component> out = new ArrayList<>();
		for (Component k : container.getComponents()) {
			out.add(k);
			if (k instanceof Container kk && !(k instanceof JSpinner) && !(k instanceof JComboBox)) {
				out.addAll(all(kk));
			}
		}
		return out;
	}

	static <T extends Component> List<T> of(JDialog dialog, Class<T> type) {
		List<T> out = new ArrayList<>();
		for (Component c : all(dialog.getContentPane())) {
			if (type.isInstance(c)) {
				out.add(type.cast(c));
			}
		}
		return out;
	}
}
