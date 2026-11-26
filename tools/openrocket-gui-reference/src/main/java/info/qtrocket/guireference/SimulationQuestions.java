package info.qtrocket.guireference;

import java.awt.AWTEvent;
import java.awt.Component;
import java.awt.Container;
import java.awt.EventQueue;
import java.awt.SecondaryLoop;
import java.awt.Toolkit;
import java.awt.Window;
import java.awt.event.AWTEventListener;
import java.awt.event.ComponentEvent;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Deque;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JFileChooser;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JRadioButton;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.SwingWorker;
import javax.swing.UIManager;
import javax.swing.border.TitledBorder;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel.LevelWindModel;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.util.ExpressionParser;
import info.openrocket.core.util.InvalidExpressionException;
import info.openrocket.swing.gui.adaptors.BooleanModel;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.adaptors.EnumModel;
import info.openrocket.swing.gui.components.CollapsiblePanel;
import info.openrocket.swing.gui.components.ConfigurationComboBox;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.simulation.CSVImportSettingsDialog;
import info.openrocket.swing.gui.simulation.MultiLevelWindEditDialog;
import info.openrocket.swing.gui.simulation.MultiLevelWindTable;
import info.openrocket.swing.gui.simulation.SimulationConfigDialog;
import info.openrocket.swing.gui.simulation.SimulationRunDialog;
import info.openrocket.swing.gui.simulation.SimulationWorker;

/**
 * The modal dialogs of OpenRocket's Flight simulations tab, answered by a step of a version 3 scenario from inside
 * their own modal loops, as a user answers them: OpenRocket's own dialogs and their listeners run
 * (SimulationConfigDialog with its three settings panels, MultiLevelWindEditDialog with its table,
 * CSVImportSettingsDialog, AerodynamicLookupDialog, the file choosers they open, SimulationRunDialog and the
 * confirmations of JOptionPane: Delete, the settings dialog's discard question, the wind editor's overwrite
 * question).
 * <p>
 * The technique of {@link Questions} (version 2), for dialogs that open other dialogs and for actions that open one
 * dialog after another: before an operation (or, inside an answer, before the click that opens a dialog), the
 * answers of the dialogs it is to open are queued in their order; a modal dialog that shows takes the next queued
 * answer (an AWT listener of COMPONENT_SHOWN), which runs inside the dialog's modal loop once the dialog's own queued
 * work has run (SwingUtilities.invokeLater from the listener) and closes the dialog with its button. A dialog that
 * opens with no answer queued (a message OpenRocket shows, a question the step does not answer) fails the step and is
 * closed as it shows; an answer left over when its click is done (a dialog that never opened) fails it too; an
 * answer that fails closes its dialog, so that the operation goes on, and the failure is rethrown when the operation
 * is over.
 * <p>
 * Each question is recorded as it opened, and the dialogs with fields also with what the answer left in them before
 * its button ("answered"), for state.json (README.md, "state.json").
 * <p>
 * Where QtRocket fixes one of OpenRocket's bugs of plan 11.4 that a step reaches, the tool either makes QtRocket's
 * fix in OpenRocket's run and lists it in the step's record ("toolFixes"), or fails on the step (README.md,
 * "QtRocket's fixes of OpenRocket's bugs"): bug 31 (a wind level given the altitude of another), 35 (a temperature
 * at or below -80 °C without the standard atmosphere), 37 (a wind CSV file without the extension .csv) and 38 (a
 * new simulation, or the launch conditions' Reset to default, while the geodetic method or a warning threshold of
 * the preferences is not OpenRocket's default) are refused; bug 32 (the levels not sorted after an altitude edit),
 * 33 (Delete level), 34 (a SPACE or TAB separator of the wind CSV import) and the wind table's stale delete buttons
 * are fixed. The run that the settings dialog's OK starts for an outdated simulation is answered with Cancel before
 * any of its simulations starts (the tool holds the run dialog's threads), so that no flight is simulated (no run
 * before Save as) and bug 40 (a cancelled run's partial flight) does not show.
 */
final class SimulationQuestions {

	private static final Translator trans = Application.getTranslator();

	/** The ids of the settings dialog's tabs, by index (SimulationConfigDialog.LAUNCH_CONDITIONS_IDX ... EXPORT_IDX). */
	static final List<String> SETTINGS_TABS = List.of("launchConditions", "simulationOptions", "warnings", "plot",
			"export");

	/** The columns of the wind editor's table that hold numbers, in its order (MultiLevelWindTable.COLUMNS). */
	static final List<String> WIND_COLUMNS = List.of("altitude", "speed", "direction", "standardDeviation",
			"turbulence");

	/** The header unit selectors of the wind editor's table, by column (MultiLevelWindTable's fields). */
	private static final List<String> WIND_UNIT_SELECTORS = List.of("altitudeUnitSelector", "speedUnitSelector",
			"directionUnitSelector", "stdDeviationUnitSelector", "turbulenceUnitSelector");

	/** The rows of the CSV import settings, by their answers' names, and the prefixes of the dialog's fields. */
	private static final List<String> CSV_ROWS = List.of("altitude", "speed", "direction", "standardDeviation");
	private static final List<String> CSV_FIELDS = List.of("altitude", "speed", "direction", "stdDeviation");

	/** The class of the aerodynamic lookup dialog (package-private in OpenRocket). */
	private static final String LOOKUP_DIALOG = "info.openrocket.swing.gui.simulation.AerodynamicLookupDialog";

	/** The two sections of the lookup dialog, by tab index: their answers' names and their fields' prefixes. */
	private static final List<String> LOOKUP_SECTIONS = List.of("drag", "stability");

	/** Something that answers a modal dialog from inside its modal loop. */
	private interface DialogAnswer {
		void answer(JDialog dialog) throws Exception;
	}

	/** A queued answer, with its name for the messages. */
	private record Pending(String name, DialogAnswer answer) {
	}

	private final Rocket rocket;
	/** The repository: the files an answer names are relative to it. */
	private final Path repository;
	private final List<JsonObject> asked = new ArrayList<>();
	private final List<JsonObject> toolFixes = new ArrayList<>();
	private final Deque<Pending> pending = new ArrayDeque<>();
	private final Set<Window> answeredDialogs = Collections.newSetFromMap(new IdentityHashMap<>());
	private Throwable failure;

	SimulationQuestions(OpenRocketDocument document, Path repository) {
		this.rocket = document.getRocket();
		this.repository = repository;
	}

	/** The questions of the last step, each as it opened (with what its answer left, "answered"). */
	List<JsonObject> asked() {
		return asked;
	}

	/** The fixes of QtRocket the tool made in OpenRocket's run during the last step. */
	List<JsonObject> toolFixes() {
		return toolFixes;
	}

	/** Before a step. */
	void clear() {
		asked.clear();
		toolFixes.clear();
	}

	/** Adds the marker of a fix the tool made in OpenRocket's run ("toolFixes" of the record). */
	void addToolFix(int bug, String text) {
		JsonObject fix = new JsonObject();
		if (bug > 0) {
			fix.addProperty("bug", bug);
		}
		fix.addProperty("fix", text);
		toolFixes.add(fix);
	}

	// ------------------------------------------------------------------ the operations

	/** An operation that asks nothing (Duplicate, Copy, Paste). */
	void plainOperation(Runnable operation) {
		operation(operation, List.of());
	}

	/**
	 * Delete or Cut of simulations: the confirmation SimulationPanel.verifyDeleteSimulation() asks while the
	 * preference ConfirmDeleteSimulation is on (SimulationPanel.java:563-592), answered with @p answer (null: none is
	 * to be asked).
	 */
	void deleteOperation(Runnable operation, JsonObject answer) {
		List<Pending> answers = new ArrayList<>();
		if (answer != null) {
			answers.add(new Pending("the answer to the delete confirmation",
					d -> confirm(d, answer, Set.of("ok", "cancel"))));
		}
		operation(operation, answers);
	}

	/**
	 * New or Edit simulation: the settings dialog (SimulationConfigDialog) answered with @p answer. New is refused
	 * while the preferences would give OpenRocket's new simulation other options than QtRocket's (bug 38).
	 */
	void settingsOperation(Runnable operation, JsonObject answer, boolean isNew) {
		if (isNew) {
			requireDefaultSimulationPreferences("New simulation");
		}
		operation(operation, List.of(new Pending("the answer to the settings dialog", d -> settings(d, answer))));
	}

	/** Runs @p operation on the event dispatch thread, its dialogs answered by @p answers in their order. */
	private void operation(Runnable operation, List<Pending> answers) {
		if (!SwingUtilities.isEventDispatchThread()) {
			throw new IllegalStateException("an operation of the simulations tab runs on the event dispatch thread");
		}
		failure = null;
		pending.clear();
		answeredDialogs.clear();
		pending.addAll(answers);
		AWTEventListener listener = this::dialogShown;
		Toolkit.getDefaultToolkit().addAWTEventListener(listener, AWTEvent.COMPONENT_EVENT_MASK);
		try {
			operation.run();
			flushQueue();
		} finally {
			Toolkit.getDefaultToolkit().removeAWTEventListener(listener);
		}
		if (failure != null) {
			Throwable f = failure;
			failure = null;
			throw new IllegalStateException("an answer of the step failed", f);
		}
		requireAllTaken("the operation");
	}

	/** Fails with the names of the queued answers no dialog took, and forgets them. */
	private void requireAllTaken(String what) {
		if (!pending.isEmpty()) {
			List<String> names = new ArrayList<>();
			pending.forEach(p -> names.add(p.name()));
			pending.clear();
			throw new IllegalStateException(what + " opened no dialog for " + names);
		}
	}

	/**
	 * A modal dialog shows: it takes the next queued answer, which runs inside its modal loop after what the dialog
	 * queued itself; with none queued, the step fails and the dialog is closed (a run dialog is cancelled first).
	 */
	private void dialogShown(AWTEvent event) {
		if (event.getID() != ComponentEvent.COMPONENT_SHOWN || !(event.getSource() instanceof JDialog dialog)
				|| !dialog.isModal() || answeredDialogs.contains(dialog)) {
			return;
		}
		answeredDialogs.add(dialog);
		Pending next = pending.poll();
		if (next == null) {
			fail(new IllegalStateException("a dialog opened that the step has no answer for: " + dialog.getTitle()));
			close(dialog);
			return;
		}
		SwingUtilities.invokeLater(() -> answerInLoop(dialog, next));
	}

	private void answerInLoop(JDialog dialog, Pending answer) {
		try {
			answer.answer().answer(dialog);
			// What the closing queued (SimulationConfigDialog.closeDialog() disposes it later).
			flushQueue();
			if (dialog.isVisible()) {
				throw new IllegalStateException(answer.name() + " left the dialog '" + dialog.getTitle() + "' open");
			}
		} catch (Throwable t) {
			fail(new IllegalStateException(answer.name() + " failed", t));
			close(dialog);
		}
	}

	/** Closes a dialog an answer failed in or none took; a run dialog cancels its simulations, none of which started. */
	private static void close(JDialog dialog) {
		if (dialog instanceof SimulationRunDialog run) {
			run.cancelSimulations();
		}
		if (dialog.isVisible()) {
			dialog.setVisible(false);
		}
	}

	private void fail(Throwable t) {
		if (failure == null) {
			failure = t;
		} else if (failure != t) {
			failure.addSuppressed(t);
		}
	}

	/**
	 * A click on @p button inside an answer, the dialogs it opens answered by @p answers in their order: every one of
	 * them must open (a dialog that opens without one fails the step, see dialogShown()).
	 */
	private void clickWith(AbstractButton button, List<Pending> answers) {
		if (!button.isEnabled()) {
			throw new IllegalStateException("the button " + buttonName(button) + " is disabled");
		}
		if (!button.isShowing()) {
			throw new IllegalStateException("the button " + buttonName(button) + " is not shown");
		}
		if (!pending.isEmpty()) {
			throw new IllegalStateException("answers are queued before a click: " + pending);
		}
		pending.addAll(answers);
		button.doClick(0);
		requireAllTaken("the click on " + buttonName(button));
	}

	private static String buttonName(AbstractButton button) {
		return button.getText() == null || button.getText().isEmpty() ? button.getToolTipText() : button.getText();
	}

	/** Runs what is queued on the event dispatch thread now (also inside a modal loop: a nested secondary loop). */
	static void flushQueue() {
		EventQueue queue = Toolkit.getDefaultToolkit().getSystemEventQueue();
		SecondaryLoop loop = queue.createSecondaryLoop();
		SwingUtilities.invokeLater(loop::exit);
		loop.enter();
	}

	// ------------------------------------------------------------------ the settings dialog

	/**
	 * SimulationConfigDialog (SimulationPanel.openDialog: switched to the Launch conditions tab): {"edits": [...] (in
	 * order), "button": "ok" | "cancel", "run": the run dialog's answer (exactly when OK runs outdated simulations),
	 * "discard": the discard question's answer (exactly when Cancel asks it)}.
	 */
	private void settings(JDialog d, JsonObject answer) throws Exception {
		if (!(d instanceof SimulationConfigDialog dialog)) {
			throw new IllegalStateException("not the settings dialog: " + d.getTitle());
		}
		Questions.members(answer, Set.of("button"), Set.of("edits", "button", "run", "discard"));
		JsonObject question = new JsonObject();
		question.addProperty("question", "simulation");
		settingsState(dialog).entrySet().forEach(e -> question.add(e.getKey(), e.getValue()));
		asked.add(question);
		if (answer.has("edits")) {
			JsonArray edits = answer.getAsJsonArray("edits");
			for (int i = 0; i < edits.size(); i++) {
				try {
					settingsEdit(dialog, edits.get(i).getAsJsonObject());
				} catch (RuntimeException e) {
					throw new IllegalStateException("edit " + i + " of the settings dialog failed: " + edits.get(i), e);
				}
				// A user's next action comes after what this one queued.
				flushQueue();
			}
		}
		question.add("answered", settingsState(dialog));
		int tab = tabs(dialog).getSelectedIndex();
		switch (Questions.string(answer, "button")) {
			case "ok" -> {
				if (answer.has("discard")) {
					throw new IllegalArgumentException("OK asks no discard question");
				}
				if (tab >= 3) {
					throw new IllegalStateException("OK on the plot or export tab plots or exports: no step of the tool");
				}
				ok(dialog, answer);
			}
			case "cancel" -> {
				if (answer.has("run")) {
					throw new IllegalArgumentException("Cancel runs nothing");
				}
				if (tab >= 3) {
					throw new IllegalStateException("Cancel on the plot or export tab is Close: no step of the tool");
				}
				cancel(dialog, answer);
			}
			default -> throw new IllegalArgumentException("the settings dialog's buttons are ok and cancel");
		}
	}

	/**
	 * OK (SimulationConfigDialog.java:408-442): the seed controls, the options copied to the other simulations, the
	 * outdated simulations run (a run dialog, answered with Cancel before any simulation starts: the tool holds the
	 * run dialog's threads until its answer is done), the dialog closed.
	 */
	private void ok(SimulationConfigDialog dialog, JsonObject answer) throws Exception {
		JButton ok = (JButton) Reflection.get(SimulationConfigDialog.class, dialog, "okButton");
		List<Pending> answers = new ArrayList<>();
		if (answer.has("run")) {
			JsonObject run = answer.getAsJsonObject("run");
			answers.add(new Pending("the answer to the run dialog", r -> run(r, run)));
		}
		RunHold hold = RunHold.take();
		try {
			clickWith(ok, answers);
		} finally {
			hold.release();
		}
	}

	/**
	 * Cancel (SimulationConfigDialog.cancelSimEdit(), :520-550): the discard question, when the dialog asks it,
	 * answered with Yes (No leaves the dialog open, which an answer cannot go on with); the undo of the dialog's
	 * changes. GuiReference checks that the Cancel left the simulations and the tree as they were before the step
	 * (QtRocket's fix of bugs 29 and 30).
	 */
	private void cancel(SimulationConfigDialog dialog, JsonObject answer) {
		JButton cancel = (JButton) Reflection.get(SimulationConfigDialog.class, dialog, "cancelButton");
		List<Pending> answers = new ArrayList<>();
		if (answer.has("discard")) {
			JsonObject discard = answer.getAsJsonObject("discard");
			if (!"yes".equals(Questions.string(discard, "button"))) {
				throw new IllegalArgumentException("the discard question is answered yes (No leaves the dialog open)");
			}
			answers.add(new Pending("the answer to the discard question", q -> confirm(q, discard, Set.of("yes"))));
		}
		clickWith(cancel, answers);
	}

	/** The run dialog of OK: {"button": "cancel"}, before any of its simulations started. */
	private void run(JDialog d, JsonObject answer) {
		if (!(d instanceof SimulationRunDialog dialog)) {
			throw new IllegalStateException("not the run dialog: " + d.getTitle());
		}
		Questions.members(answer, Set.of("button"), Set.of("button"));
		if (!"cancel".equals(Questions.string(answer, "button"))) {
			throw new IllegalArgumentException("the run dialog is answered with cancel (no run before Save as)");
		}
		Simulation[] simulations = (Simulation[]) Reflection.get(SimulationRunDialog.class, dialog, "simulations");
		SimulationWorker[] workers = (SimulationWorker[]) Reflection.get(SimulationRunDialog.class, dialog,
				"simulationWorkers");
		for (SimulationWorker worker : workers) {
			if (worker.getState() != SwingWorker.StateValue.PENDING) {
				throw new IllegalStateException("a simulation of the run dialog started before its Cancel");
			}
		}
		JsonObject question = new JsonObject();
		question.addProperty("question", "run");
		question.addProperty("title", dialog.getTitle());
		JsonArray names = new JsonArray();
		for (Simulation s : simulations) {
			names.add(s.getName());
		}
		question.add("simulations", names);
		asked.add(question);
		Questions.click(dialog, trans.get("dlg.but.cancel"));
	}

	/**
	 * The run dialog's threads held (SimulationRunDialog's executor, a pool of availableProcessors() threads), so
	 * that the simulations of a run dialog stay queued until its Cancel: a blocker on every thread of the pool,
	 * released when the dialog's answer is done.
	 */
	private static final class RunHold {
		private final CountDownLatch release = new CountDownLatch(1);

		static RunHold take() throws InterruptedException {
			ThreadPoolExecutor executor = (ThreadPoolExecutor) Reflection.get(SimulationRunDialog.class, null,
					"executor");
			int n = executor.getMaximumPoolSize();
			CountDownLatch started = new CountDownLatch(n);
			RunHold hold = new RunHold();
			for (int i = 0; i < n; i++) {
				executor.execute(() -> {
					started.countDown();
					try {
						hold.release.await();
					} catch (InterruptedException e) {
						Thread.currentThread().interrupt();
					}
				});
			}
			if (!started.await(60, TimeUnit.SECONDS)) {
				hold.release();
				throw new IllegalStateException("the run dialog's threads were not all free");
			}
			return hold;
		}

		void release() {
			release.countDown();
		}
	}

	/**
	 * What the settings dialog shows (the question's members): its title (a "* " prefix once the simulation changed),
	 * the name field, the configuration box's selected item, the status label, the selected tab, every tab (its id,
	 * title and enabled state) and the multi-simulation label (null in a single edit).
	 */
	private JsonObject settingsState(SimulationConfigDialog dialog) {
		JsonObject state = new JsonObject();
		state.addProperty("title", dialog.getTitle());
		Container top = topPanel(dialog);
		state.addProperty("name", only(top, JTextField.class).getText());
		state.addProperty("configuration", String.valueOf(only(top, ConfigurationComboBox.class).getSelectedItem()));
		List<JLabel> labels = new ArrayList<>();
		for (Component c : top.getComponents()) {
			if (c instanceof JLabel l) {
				labels.add(l);
			}
		}
		state.addProperty("status", labels.get(labels.size() - 1).getText());
		JTabbedPane tabs = tabs(dialog);
		state.addProperty("tab", SETTINGS_TABS.get(tabs.getSelectedIndex()));
		JsonArray all = new JsonArray();
		for (int i = 0; i < tabs.getTabCount(); i++) {
			JsonObject tab = new JsonObject();
			tab.addProperty("tab", SETTINGS_TABS.get(i));
			tab.addProperty("title", tabs.getTitleAt(i));
			tab.addProperty("enabled", tabs.isEnabledAt(i));
			all.add(tab);
		}
		state.add("tabs", all);
		String multi = null;
		for (Component c : Questions.all(dialog.getContentPane())) {
			if (c instanceof JLabel l && trans.get("simedtdlg.title.MultiSimEdit").equals(l.getText())
					&& !top.isAncestorOf(l)) {
				multi = l.getText();
			}
		}
		state.addProperty("multiSimEdit", multi);
		return state;
	}

	private static JTabbedPane tabs(SimulationConfigDialog dialog) {
		return (JTabbedPane) Reflection.get(SimulationConfigDialog.class, dialog, "tabbedPane");
	}

	/** The top panel of the dialog: the name, the configuration and the status (addTopPanel). */
	private static Container topPanel(SimulationConfigDialog dialog) {
		return (Container) tabs(dialog).getParent().getComponent(0);
	}

	private static <T extends Component> T only(Container container, Class<T> type) {
		List<T> found = new ArrayList<>();
		for (Component c : container.getComponents()) {
			if (type.isInstance(c)) {
				found.add(type.cast(c));
			}
		}
		if (found.size() != 1) {
			throw new IllegalStateException("the panel has " + found.size() + " " + type.getSimpleName());
		}
		return found.get(0);
	}

	/**
	 * One edit of the settings dialog's answer (README.md, "The scenarios", version 3): exactly one of the members
	 * "field" (with "text" and "unit", "checked" or "item"), "name", "configuration", "tab", "windModel",
	 * "randomSeed", "defaults" (with "panel"), "windLevels" and "lookup". Every edit but "defaults": "save",
	 * "windLevels" and "lookup" must change the dialog's simulation or tab (a value the field has checks nothing).
	 */
	private void settingsEdit(SimulationConfigDialog dialog, JsonObject edit) throws Exception {
		Simulation sim = ((Simulation[]) Reflection.get(SimulationConfigDialog.class, dialog, "simulationList"))[0];
		String before = simulationState(sim) + "|" + tabs(dialog).getSelectedIndex();
		boolean mustChange = true;
		if (edit.has("field")) {
			fieldEdit(dialog, sim, edit);
		} else if (edit.has("name")) {
			Questions.members(edit, Set.of("name"), Set.of("name"));
			String name = Questions.string(edit, "name");
			if (name.isEmpty()) {
				throw new IllegalArgumentException("an empty name leaves the simulation's name as it is");
			}
			only(topPanel(dialog), JTextField.class).setText(name);
		} else if (edit.has("configuration")) {
			Questions.members(edit, Set.of("configuration"), Set.of("configuration"));
			configurationEdit(dialog, edit.get("configuration").getAsInt());
		} else if (edit.has("tab")) {
			Questions.members(edit, Set.of("tab"), Set.of("tab"));
			selectTab(dialog, Questions.string(edit, "tab"));
		} else if (edit.has("windModel")) {
			Questions.members(edit, Set.of("windModel"), Set.of("windModel"));
			windModelEdit(dialog, Questions.string(edit, "windModel"));
		} else if (edit.has("randomSeed")) {
			Questions.members(edit, Set.of("randomSeed"), Set.of("randomSeed"));
			randomSeedEdit(dialog, edit.getAsJsonObject("randomSeed"));
		} else if (edit.has("defaults")) {
			Questions.members(edit, Set.of("defaults", "panel"), Set.of("defaults", "panel"));
			mustChange = defaultsEdit(dialog, Questions.string(edit, "defaults"), Questions.string(edit, "panel"));
		} else if (edit.has("windLevels")) {
			Questions.members(edit, Set.of("windLevels"), Set.of("windLevels"));
			JsonObject answer = edit.getAsJsonObject("windLevels");
			JButton button = buttonOnPage(dialog, 0, trans.get("simedtdlg.but.editWindLevels"));
			clickWith(button, List.of(new Pending("the answer to the wind editor", w -> windEditor(w, answer))));
			mustChange = false;
		} else if (edit.has("lookup")) {
			Questions.members(edit, Set.of("lookup"), Set.of("lookup"));
			JsonObject answer = edit.getAsJsonObject("lookup");
			JButton button = buttonOnPage(dialog, 1, trans.get("AerodynamicLookupDialog.btn.configure"));
			clickWith(button, List.of(new Pending("the answer to the lookup dialog", l -> lookup(l, answer))));
			mustChange = false;
		} else {
			throw new IllegalArgumentException("an edit of the settings dialog the tool does not know: " + edit);
		}
		if (mustChange && before.equals(simulationState(sim) + "|" + tabs(dialog).getSelectedIndex())) {
			throw new IllegalStateException("the edit " + edit + " left the simulation as it was: give the field a"
					+ " value it does not have");
		}
	}

	/** The name, the configuration and the option lines of @p sim, for the checks. */
	private String simulationState(Simulation sim) {
		return sim.getName() + "|" + sim.getFlightConfigurationId() + "|"
				+ String.join("|", SimulationLines.optionLines(sim.getOptions()));
	}

	/**
	 * A field of the settings panels by the Java property its model binds: a spinner (its DoubleModel's), a check
	 * box (its BooleanModel's) or a combo box (its EnumModel's), brought into view as a user does it (its tab, the
	 * warnings page's sub-tab, a collapsed section expanded), then set: {"text", "unit"} typed (the unit chosen in the
	 * field's unit selector first), {"checked"} clicked, {"item"} (the Java enum constant) chosen.
	 */
	private void fieldEdit(SimulationConfigDialog dialog, Simulation sim, JsonObject edit) {
		String property = Questions.string(edit, "field");
		Component field = findField(dialog, property);
		bringIntoView(dialog, field);
		if (!field.isShowing()) {
			throw new IllegalStateException("the field " + property + " is not shown (another card of its panel)");
		}
		if (!field.isEnabled()) {
			throw new IllegalStateException("the field " + property + " is disabled");
		}
		SimulationOptions options = sim.getOptions();
		if (field instanceof JSpinner spinner) {
			Questions.members(edit, Set.of("field", "text"), Set.of("field", "text", "unit"));
			DoubleModel model = Questions.spinnerModel(spinner);
			chooseUnit(spinner, model, Questions.string(edit, "unit"));
			double typed = parse(model.getCurrentUnit(), Questions.string(edit, "text"));
			if (property.equals("LaunchTemperature") && !options.isISAAtmosphere() && typed <= 193.15) {
				throw new IllegalStateException("bug 35: OpenRocket's error text of a temperature at or below -80 °C"
						+ " is a missing key (QtRocket shows \"Extremely low temperature\")");
			}
			if (typed == model.getValue()) {
				throw new IllegalStateException("the field " + property + " shows that value already");
			}
			spinner.getModel().setValue(Questions.string(edit, "text"));
		} else if (field instanceof JCheckBox box) {
			Questions.members(edit, Set.of("field", "checked"), Set.of("field", "checked"));
			boolean checked = edit.get("checked").getAsBoolean();
			if (box.isSelected() == checked) {
				throw new IllegalStateException("the check box " + property + " is " + checked + " already");
			}
			if (property.equals("ISAAtmosphere") && !checked && options.getLaunchTemperature() <= 193.15) {
				throw new IllegalStateException("bug 35: OpenRocket's error text of a temperature at or below -80 °C"
						+ " is a missing key");
			}
			box.doClick(0);
		} else if (field instanceof JComboBox<?> combo) {
			Questions.members(edit, Set.of("field", "item"), Set.of("field", "item"));
			String item = Questions.string(edit, "item");
			Object found = null;
			for (int i = 0; i < combo.getItemCount(); i++) {
				if (combo.getItemAt(i) instanceof Enum<?> e && e.name().equals(item)) {
					found = combo.getItemAt(i);
				}
			}
			if (found == null) {
				throw new IllegalArgumentException("the combo box " + property + " has no item " + item);
			}
			if (combo.getSelectedItem() == found) {
				throw new IllegalStateException("the combo box " + property + " shows " + item + " already");
			}
			combo.setSelectedItem(found);
		}
	}

	/** The field of @p property in the dialog's three settings pages; exactly one. */
	private static Component findField(SimulationConfigDialog dialog, String property) {
		JTabbedPane tabs = tabs(dialog);
		List<Component> found = new ArrayList<>();
		for (int page = 0; page < 3; page++) {
			for (Component c : components(tabs.getComponentAt(page))) {
				if (property.equals(boundProperty(c))) {
					found.add(c);
				}
			}
		}
		if (found.size() != 1) {
			throw new IllegalArgumentException("the settings dialog has " + found.size() + " fields of " + property);
		}
		return found.get(0);
	}

	/**
	 * Every component below @p root, depth first, into spinners and combo boxes not, and into the content of a
	 * collapsed section too (CollapsiblePanel removes its content from the hierarchy while collapsed).
	 */
	private static List<Component> components(Component root) {
		List<Component> out = new ArrayList<>();
		out.add(root);
		if (root instanceof Container k && !(root instanceof JSpinner) && !(root instanceof JComboBox)) {
			for (Component c : k.getComponents()) {
				out.addAll(components(c));
			}
			if (root instanceof CollapsiblePanel section && !section.isExpanded()) {
				out.addAll(components(collapsedContent(section)));
			}
		}
		return out;
	}

	/** The content container of a section (CollapsiblePanel.contentContainer). */
	private static Container collapsedContent(CollapsiblePanel section) {
		return (Container) Reflection.get(CollapsiblePanel.class, section, "contentContainer");
	}

	/** The collapsed section of @p dialog whose content container is @p container, or null. */
	private static CollapsiblePanel collapsedOwner(SimulationConfigDialog dialog, Component container) {
		for (Component c : components(dialog.getContentPane())) {
			if (c instanceof CollapsiblePanel section && !section.isExpanded()
					&& collapsedContent(section) == container) {
				return section;
			}
		}
		return null;
	}

	/** The Java property the model of @p c binds (a DoubleModel's spinner, a BooleanModel's or EnumModel's box). */
	private static String boundProperty(Component c) {
		if (c instanceof JSpinner spinner && spinner.getModel().getClass().getEnclosingClass() == DoubleModel.class) {
			return (String) Reflection.get(DoubleModel.class, Questions.spinnerModel(spinner), "valueName");
		}
		if (c instanceof JCheckBox box && box.getAction() instanceof BooleanModel model) {
			return (String) Reflection.get(BooleanModel.class, model, "valueName");
		}
		if (c instanceof JComboBox<?> combo && combo.getModel() instanceof EnumModel<?> model) {
			return (String) Reflection.get(EnumModel.class, model, "valueName");
		}
		return null;
	}

	/**
	 * Brings @p c into view as a user does: the dialog's tab that holds it selected, the page of an inner tabbed pane
	 * that holds it (the warnings page's Messages and Configuration), a collapsed section around it expanded.
	 */
	private static void bringIntoView(SimulationConfigDialog dialog, Component c) {
		JTabbedPane tabs = tabs(dialog);
		Component child = c;
		for (Container parent = parentOf(dialog, c); parent != null; child = parent, parent = parentOf(dialog, parent)) {
			if (parent instanceof JTabbedPane pane) {
				int index = pane.indexOfComponent(child);
				if (index < 0) {
					throw new IllegalStateException("a component of a tabbed pane is on none of its pages");
				}
				if (!pane.isEnabledAt(index)) {
					throw new IllegalStateException("the tab " + pane.getTitleAt(index) + " is disabled");
				}
				if (pane.getSelectedIndex() != index) {
					pane.setSelectedIndex(index);
				}
				if (pane == tabs) {
					return;
				}
			}
		}
		throw new IllegalStateException("the field is on no tab of the settings dialog");
	}

	/**
	 * The parent of @p c; the content of a collapsed section has none until the section is expanded, as a user
	 * expands it with a click on its header (CollapsiblePanel's toggle button).
	 */
	private static Container parentOf(SimulationConfigDialog dialog, Component c) {
		if (c.getParent() == null) {
			CollapsiblePanel section = collapsedOwner(dialog, c);
			if (section != null) {
				section.getHeaderButton().doClick(0);
			}
		}
		return c.getParent();
	}

	/** The unit @p unit chosen in the unit selector of @p spinner's model (a click on its popup's item). */
	private static void chooseUnit(JSpinner spinner, DoubleModel model, String unit) {
		UnitSelector selector = null;
		for (Component c : components(spinner.getParent())) {
			if (c instanceof UnitSelector s && s.getModel() == model) {
				selector = s;
			}
		}
		if (unit == null) {
			if (selector != null) {
				throw new IllegalArgumentException("the field has a unit selector: give the unit");
			}
			return;
		}
		Unit chosen = Questions.unit(model.getUnitGroup(), unit);
		if (selector == null) {
			if (model.getCurrentUnit() != chosen) {
				throw new IllegalArgumentException("the field shows " + model.getCurrentUnit().getUnit() + ", not "
						+ unit + " (it has no unit selector)");
			}
			return;
		}
		if (selector.getSelectedUnit() != chosen) {
			selector.setSelectedUnit(chosen);
		}
	}

	/** @p text parsed as the spinner's ExpressionParser does, in @p unit, converted to SI. */
	private static double parse(Unit unit, String text) {
		try {
			return unit.fromUnit(new ExpressionParser().parse(text));
		} catch (InvalidExpressionException e) {
			throw new IllegalArgumentException("the field does not take '" + text + "'", e);
		}
	}

	/** The configuration box set to the rocket's configuration of @p row (its place among the rocket's). */
	private void configurationEdit(SimulationConfigDialog dialog, int row) {
		List<FlightConfigurationId> ids = rocket.getIds();
		if (row < 0 || row >= ids.size()) {
			throw new IllegalArgumentException("the rocket has no configuration " + row);
		}
		ConfigurationComboBox combo = only(topPanel(dialog), ConfigurationComboBox.class);
		FlightConfiguration item = null;
		for (int i = 0; i < combo.getItemCount(); i++) {
			FlightConfiguration c = combo.getItemAt(i);
			if (c != null && c.getId().equals(ids.get(row))) {
				item = c;
			}
		}
		if (item == null) {
			throw new IllegalStateException("the configuration box has no item of configuration " + row);
		}
		combo.setSelectedItem(item);
	}

	/** A tab of the settings dialog (a click on it): an enabled one that is not the current one. */
	private static void selectTab(SimulationConfigDialog dialog, String tab) {
		int index = SETTINGS_TABS.indexOf(tab);
		if (index < 0 || index >= 3) {
			throw new IllegalArgumentException("the tab " + tab + " is none of launchConditions, simulationOptions and"
					+ " warnings (the plot and export tabs need flight data)");
		}
		JTabbedPane tabs = tabs(dialog);
		if (!tabs.isEnabledAt(index)) {
			throw new IllegalStateException("the tab " + tab + " is disabled");
		}
		if (tabs.getSelectedIndex() == index) {
			throw new IllegalStateException("the tab " + tab + " is the current one already");
		}
		tabs.setSelectedIndex(index);
	}

	/** The wind model's radio button (SimulationConditionsPanel.addWindModelPanel): AVERAGE or MULTI_LEVEL. */
	private void windModelEdit(SimulationConfigDialog dialog, String model) {
		String text = switch (model) {
			case "AVERAGE" -> trans.get("simedtdlg.radio.AverageWind");
			case "MULTI_LEVEL" -> trans.get("simedtdlg.radio.MultiLevelWind");
			default -> throw new IllegalArgumentException("the wind model is AVERAGE or MULTI_LEVEL, not " + model);
		};
		JRadioButton radio = null;
		for (Component c : components(tabs(dialog).getComponentAt(0))) {
			if (c instanceof JRadioButton r && text.equals(r.getText())) {
				radio = r;
			}
		}
		if (radio == null) {
			throw new IllegalStateException("the launch conditions have no radio button " + text);
		}
		bringIntoView(dialog, radio);
		if (radio.isSelected()) {
			throw new IllegalStateException("the wind model is " + model + " already");
		}
		radio.doClick(0);
	}

	/**
	 * The fixed random seed of the options page (SimulationOptionsPanel's check box and field): {"fixed": true,
	 * "text": the seed typed into the field} (the box ticked first when it is not, which draws a random seed the
	 * typed one replaces) or {"fixed": false} (the box cleared).
	 */
	private void randomSeedEdit(SimulationConfigDialog dialog, JsonObject seed) {
		Questions.members(seed, Set.of("fixed"), Set.of("fixed", "text"));
		Object panel = Reflection.get(SimulationConfigDialog.class, dialog, "simulationOptionsTab");
		JCheckBox box = (JCheckBox) Reflection.get(panel.getClass(), panel, "fixedRandomSeedCheckBox");
		JTextField field = (JTextField) Reflection.get(panel.getClass(), panel, "randomSeedField");
		bringIntoView(dialog, box);
		if (!seed.get("fixed").getAsBoolean()) {
			if (seed.has("text")) {
				throw new IllegalArgumentException("a seed that is not fixed has no text");
			}
			if (!box.isSelected()) {
				throw new IllegalStateException("the random seed is not fixed already");
			}
			box.doClick(0);
			return;
		}
		if (!seed.has("text")) {
			throw new IllegalArgumentException("a fixed seed needs its text (the box draws a random one)");
		}
		String text = Questions.string(seed, "text");
		try {
			Integer.parseInt(text.trim());
		} catch (NumberFormatException e) {
			throw new IllegalArgumentException("the seed '" + text + "' is no integer", e);
		}
		if (!box.isSelected()) {
			box.doClick(0);
		}
		field.setText(text);
	}

	/**
	 * "Reset to default" or "Save as default" of a page: "launchConditions" (SimulationConditionsPanel),
	 * "simulationOptions" (SimulationOptionsPanel) or "warnings" (the Configuration sub-tab of
	 * SimulationWarningsPanel). Returns whether the edit must change the simulation (a reset does).
	 */
	private boolean defaultsEdit(SimulationConfigDialog dialog, String what, String panel) {
		int page = SETTINGS_TABS.indexOf(panel);
		if (page < 0 || page >= 3) {
			throw new IllegalArgumentException("the defaults' panel is launchConditions, simulationOptions or warnings");
		}
		String text = switch (what) {
			case "reset" -> trans.get("simedtdlg.but.resettodefault");
			case "save" -> trans.get("simedtdlg.but.savedefault");
			default -> throw new IllegalArgumentException("defaults are reset or save, not " + what);
		};
		if (what.equals("reset") && page == 0) {
			requireDefaultSimulationPreferences("the launch conditions' Reset to default");
		}
		JButton button = buttonOnPage(dialog, page, text);
		button.doClick(0);
		return what.equals("reset");
	}

	/** The button of @p text on the settings page @p page, brought into view. */
	private static JButton buttonOnPage(SimulationConfigDialog dialog, int page, String text) {
		List<JButton> found = new ArrayList<>();
		for (Component c : components(tabs(dialog).getComponentAt(page))) {
			if (c instanceof JButton b && text.equals(b.getText())) {
				found.add(b);
			}
		}
		if (found.size() != 1) {
			throw new IllegalStateException("the page " + SETTINGS_TABS.get(page) + " has " + found.size()
					+ " buttons " + text);
		}
		bringIntoView(dialog, found.get(0));
		if (!found.get(0).isShowing()) {
			throw new IllegalStateException("the button " + text + " is not shown (another card of its panel)");
		}
		return found.get(0);
	}

	/**
	 * Bug 38 (plan 11.4): OpenRocket's new simulation, and the launch conditions' Reset to default, take neither the
	 * geodetic method of the Simulation preferences nor the thresholds of the Warnings preferences, which QtRocket's
	 * take (DefaultSimulationOptionFactory). The tool refuses them while one of those preferences is not the value a
	 * new SimulationOptions has.
	 */
	private static void requireDefaultSimulationPreferences(String what) {
		ApplicationPreferences preferences = Application.getPreferences();
		SimulationOptions fresh = new SimulationOptions();
		if (preferences.getGeodeticComputation() != fresh.getGeodeticComputation()
				|| preferences.getRecoverySpeedWarning() != fresh.getRecoverySpeedWarning()
				|| preferences.getRecoveryDrogueMainHighSpeedWarning() != fresh.getRecoveryDrogueMainHighSpeedWarning()
				|| preferences.getRecoveryDrogueMainLowSpeedWarning() != fresh.getRecoveryDrogueMainLowSpeedWarning()) {
			throw new IllegalStateException("bug 38: " + what + " ignores the preferences' geodetic method and warning"
					+ " thresholds, which QtRocket's takes");
		}
	}

	// ------------------------------------------------------------------ the confirmations

	/**
	 * A JOptionPane confirmation: {"button": one of @p buttons ("ok", "cancel", "yes", "no"), "doNotAskAgain": true
	 * (its "Do not ask me again" box ticked first)}.
	 */
	private void confirm(JDialog dialog, JsonObject answer, Set<String> buttons) {
		List<JOptionPane> panes = Questions.of(dialog, JOptionPane.class);
		if (panes.isEmpty()) {
			throw new IllegalStateException("not a confirmation: " + dialog.getTitle());
		}
		JOptionPane pane = panes.get(0);
		Questions.members(answer, Set.of("button"), Set.of("button", "doNotAskAgain"));
		JsonObject question = new JsonObject();
		question.addProperty("question", "confirm");
		question.addProperty("title", dialog.getTitle());
		JsonArray message = new JsonArray();
		List<JCheckBox> boxes = new ArrayList<>();
		messageParts(pane.getMessage(), message, boxes);
		question.add("message", message);
		JsonArray boxTexts = new JsonArray();
		boxes.forEach(b -> boxTexts.add(b.getText()));
		question.add("checkBoxes", boxTexts);
		JsonArray buttonTexts = new JsonArray();
		for (JButton b : Questions.of(dialog, JButton.class)) {
			buttonTexts.add(b.getText());
		}
		question.add("buttons", buttonTexts);
		asked.add(question);
		String button = Questions.string(answer, "button");
		if (!buttons.contains(button)) {
			throw new IllegalArgumentException("the confirmation's buttons are " + buttons + ", not " + button);
		}
		if (answer.has("doNotAskAgain")) {
			if (!answer.get("doNotAskAgain").getAsBoolean() || boxes.size() != 1) {
				throw new IllegalArgumentException("doNotAskAgain is true or left out, for a question with its box");
			}
			if (!boxes.get(0).isSelected()) {
				boxes.get(0).doClick(0);
			}
		}
		String text = UIManager.getString("OptionPane." + button + "ButtonText", Locale.getDefault());
		Questions.click(dialog, text);
	}

	/** The texts and the check boxes of a JOptionPane's message (a string, an array of parts or a panel). */
	private static void messageParts(Object message, JsonArray texts, List<JCheckBox> boxes) {
		if (message instanceof Object[] parts) {
			for (Object part : parts) {
				messageParts(part, texts, boxes);
			}
		} else if (message instanceof JCheckBox box) {
			boxes.add(box);
		} else if (message instanceof JLabel label) {
			texts.add(label.getText());
		} else if (message instanceof Container container) {
			for (Component c : container.getComponents()) {
				messageParts(c, texts, boxes);
			}
		} else if (message != null) {
			texts.add(String.valueOf(message));
		}
	}

	// ------------------------------------------------------------------ the file choosers

	/**
	 * A JFileChooser's dialog (the wind CSV import's, the lookup's Load from file): {"file": a path relative to the
	 * repository, "button": "open"} (the file selected and approved, as a double click on it does) or {"button":
	 * "cancel"}.
	 */
	private void openFile(JDialog dialog, JsonObject answer) {
		List<JFileChooser> choosers = Questions.of(dialog, JFileChooser.class);
		if (choosers.size() != 1) {
			throw new IllegalStateException("not a file chooser: " + dialog.getTitle());
		}
		JFileChooser chooser = choosers.get(0);
		JsonObject question = new JsonObject();
		question.addProperty("question", "openFile");
		question.addProperty("title", dialog.getTitle());
		asked.add(question);
		switch (Questions.string(answer, "button")) {
			case "cancel" -> {
				Questions.members(answer, Set.of("button"), Set.of("button"));
				chooser.cancelSelection();
			}
			case "open" -> {
				Questions.members(answer, Set.of("button", "file"), Set.of("button", "file"));
				Path file = repositoryFile(Questions.string(answer, "file"));
				question.addProperty("file", Questions.string(answer, "file"));
				chooser.setSelectedFile(file.toFile());
				chooser.approveSelection();
			}
			default -> throw new IllegalArgumentException("a file chooser's buttons are open and cancel");
		}
	}

	/** The file @p name names, a relative path below the repository that exists. */
	private Path repositoryFile(String name) {
		Path relative = Path.of(name);
		if (relative.isAbsolute() || relative.normalize().startsWith("..") || !relative.normalize().equals(relative)) {
			throw new IllegalArgumentException("a file of an answer is a relative path below the repository: " + name);
		}
		Path file = repository.resolve(relative).toAbsolutePath().normalize();
		if (!Files.isRegularFile(file)) {
			throw new IllegalArgumentException("no file " + name + " in the repository");
		}
		return file;
	}

	// ------------------------------------------------------------------ the wind editor

	/**
	 * MultiLevelWindEditDialog ("Edit levels" of the multi-level wind): {"edits": [...] (in order), "button":
	 * "close"}; an edit has exactly one of "altitudeReference" ("MSL" or "AGL"), "addLevel" (true), "deleteLevel" (a
	 * row: selected, then "Delete level"), "deleteRow" (a row: its delete button), "cell" ({"row", "column", "text"},
	 * the text in the column's unit), "columnUnit" ({"column", "unit"}: the header's unit selector), "resetLevels"
	 * (the overwrite question's answer) and "importLevels" ({"settings", "file", "overwrite"}).
	 */
	private void windEditor(JDialog d, JsonObject answer) {
		if (!(d instanceof MultiLevelWindEditDialog editor)) {
			throw new IllegalStateException("not the wind editor: " + d.getTitle());
		}
		Questions.members(answer, Set.of("button"), Set.of("edits", "button"));
		MultiLevelWindTable table = (MultiLevelWindTable) Reflection.get(MultiLevelWindEditDialog.class, editor,
				"windTable");
		MultiLevelPinkNoiseWindModel model = (MultiLevelPinkNoiseWindModel) Reflection.get(MultiLevelWindTable.class,
				table, "windModel");
		JsonObject question = new JsonObject();
		question.addProperty("question", "windLevels");
		windState(editor, table, model).entrySet().forEach(e -> question.add(e.getKey(), e.getValue()));
		asked.add(question);
		if (answer.has("edits")) {
			JsonArray edits = answer.getAsJsonArray("edits");
			for (int i = 0; i < edits.size(); i++) {
				try {
					windEdit(editor, table, model, edits.get(i).getAsJsonObject());
				} catch (RuntimeException e) {
					throw new IllegalStateException("edit " + i + " of the wind editor failed: " + edits.get(i), e);
				}
				// A user's next action comes after what this one queued (the table's highlight of moved rows).
				flushQueue();
			}
		}
		question.add("answered", windState(editor, table, model));
		if (!"close".equals(Questions.string(answer, "button"))) {
			throw new IllegalArgumentException("the wind editor's button is close");
		}
		Questions.click(editor, trans.get("button.close"));
	}

	/**
	 * What the wind editor shows: its title, the altitude reference's radio button, and the lines of the probe
	 * tools/openrocket-probes/wind-editor (WindEditorProbe.header() and rows(): HEADER, ROW and MODEL; QtRocket's
	 * windTableHeaderLines() and windTableRowLines() of tests/gui/dialogs/simulation/WindEditorTranscript.h).
	 */
	private JsonObject windState(MultiLevelWindEditDialog editor, MultiLevelWindTable table,
			MultiLevelPinkNoiseWindModel model) {
		JsonObject state = new JsonObject();
		state.addProperty("title", editor.getTitle());
		String reference = null;
		for (JRadioButton r : Questions.of(editor, JRadioButton.class)) {
			if (r.isSelected()) {
				reference = r.getText().equals(trans.get("WindProfileEditorDlg.radio.MSL")) ? "MSL" : "AGL";
			}
		}
		state.addProperty("altitudeReference", reference);
		JsonArray lines = new JsonArray();
		List<JLabel> labels = new ArrayList<>();
		for (JLabel l : find(table.getHeaderPanel(), JLabel.class)) {
			if (!(l instanceof UnitSelector)) {
				labels.add(l);
			}
		}
		List<UnitSelector> selectors = find(table.getHeaderPanel(), UnitSelector.class);
		for (int i = 0; i < labels.size(); i++) {
			JLabel l = labels.get(i);
			String unit = i < selectors.size() ? selectors.get(i).getSelectedUnit().getUnit() : "none";
			lines.add("HEADER " + i + " [" + clean(l.getText()) + "]"
					+ (l.getToolTipText() != null ? " tip=" + clean(l.getToolTipText()) : "") + " unit=[" + unit + "]");
		}
		List<JComponent> rows = rows(table);
		for (int i = 0; i < rows.size(); i++) {
			JComponent row = rows.get(i);
			StringBuilder sb = new StringBuilder("ROW " + i);
			for (JSpinner spinner : find(row, JSpinner.class)) {
				sb.append(" [").append(((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText())
						.append(']');
			}
			JLabel intensity = (JLabel) Reflection.get(row.getClass(), row, "intensityLabel");
			JButton delete = (JButton) Reflection.get(row.getClass(), row, "deleteButton");
			sb.append(" intensity=[").append(intensity.getText()).append("] delete=")
					.append(delete.isEnabled() ? "enabled" : "disabled");
			lines.add(sb.toString());
		}
		StringBuilder sb = new StringBuilder("MODEL");
		for (LevelWindModel level : model.getLevels()) {
			sb.append(' ').append(level.getAltitude());
		}
		lines.add(sb.toString());
		state.add("lines", lines);
		return state;
	}

	private static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n");
	}

	@SuppressWarnings("unchecked")
	private static List<JComponent> rows(MultiLevelWindTable table) {
		return (List<JComponent>) Reflection.get(MultiLevelWindTable.class, table, "rows");
	}

	private static <T> List<T> find(Component c, Class<T> type) {
		List<T> found = new ArrayList<>();
		if (type.isInstance(c)) {
			found.add(type.cast(c));
		}
		if (c instanceof Container k && !(c instanceof JSpinner)) {
			for (Component child : k.getComponents()) {
				found.addAll(find(child, type));
			}
		}
		return found;
	}

	private void windEdit(MultiLevelWindEditDialog editor, MultiLevelWindTable table,
			MultiLevelPinkNoiseWindModel model, JsonObject edit) {
		if (edit.keySet().size() != 1) {
			throw new IllegalArgumentException("an edit of the wind editor has exactly one member: " + edit);
		}
		String key = edit.keySet().iterator().next();
		String before = windState(editor, table, model).toString();
		switch (key) {
			case "altitudeReference" -> {
				String reference = Questions.string(edit, key);
				String text = switch (reference) {
					case "MSL" -> trans.get("WindProfileEditorDlg.radio.MSL");
					case "AGL" -> trans.get("WindProfileEditorDlg.radio.AGL");
					default -> throw new IllegalArgumentException("the altitude reference is MSL or AGL");
				};
				JRadioButton radio = null;
				for (JRadioButton r : Questions.of(editor, JRadioButton.class)) {
					if (text.equals(r.getText())) {
						radio = r;
					}
				}
				if (radio == null || radio.isSelected()) {
					throw new IllegalStateException("the altitude reference is " + reference + " already");
				}
				radio.doClick(0);
			}
			case "addLevel" -> {
				if (!edit.get(key).getAsBoolean()) {
					throw new IllegalArgumentException("addLevel is true");
				}
				Questions.click(editor, trans.get("WindProfileEditorDlg.but.AddNewLevel"));
			}
			case "deleteLevel" -> {
				JComponent row = row(table, edit.get(key).getAsInt());
				// A click on a field of the row selects it (LevelRow.addToFocusTracking: focusGained). The tool's click
				// on "Delete level" moves no focus, so the row stays selected and is deleted: QtRocket's fix of bug
				// 33 (with the mouse, the button takes the focus first, which deselects the row and disables it).
				Reflection.invoke(table, "selectRow", new Class<?>[] { row.getClass() }, row);
				Questions.click(editor, trans.get("WindProfileEditorDlg.but.DeleteLevel"));
				addToolFix(33, "Delete level deleted the selected level");
			}
			case "deleteRow" -> {
				JComponent row = row(table, edit.get(key).getAsInt());
				JButton delete = (JButton) Reflection.get(row.getClass(), row, "deleteButton");
				if (!delete.isEnabled()) {
					throw new IllegalStateException("the row's delete button is disabled");
				}
				delete.doClick(0);
			}
			case "cell" -> cellEdit(table, model, edit.getAsJsonObject(key));
			case "columnUnit" -> {
				JsonObject unit = edit.getAsJsonObject(key);
				Questions.members(unit, Set.of("column", "unit"), Set.of("column", "unit"));
				int column = windColumn(Questions.string(unit, "column"));
				UnitSelector selector = (UnitSelector) Reflection.get(MultiLevelWindTable.class, table,
						WIND_UNIT_SELECTORS.get(column));
				Unit chosen = Questions.unit(selector.getUnitGroup(), Questions.string(unit, "unit"));
				if (selector.getSelectedUnit() == chosen) {
					throw new IllegalStateException("the column shows " + chosen.getUnit() + " already");
				}
				selector.setSelectedUnit(chosen);
			}
			case "resetLevels" -> {
				JsonObject answer = edit.getAsJsonObject(key);
				JButton reset = button(editor, trans.get("WindProfileEditorDlg.but.ResetLevels"));
				clickWith(reset, List.of(new Pending("the answer to the overwrite question",
						q -> confirm(q, answer, Set.of("yes", "no")))));
				deleteButtonsFix(table);
			}
			case "importLevels" -> importLevels(editor, table, edit.getAsJsonObject(key));
			default -> throw new IllegalArgumentException("an edit of the wind editor the tool does not know: " + key);
		}
		if (before.equals(windState(editor, table, model).toString()) && !key.equals("resetLevels")
				&& !key.equals("importLevels")) {
			throw new IllegalStateException("the edit " + edit + " left the wind editor as it was");
		}
	}

	private static JComponent row(MultiLevelWindTable table, int row) {
		List<JComponent> rows = rows(table);
		if (row < 0 || row >= rows.size()) {
			throw new IllegalArgumentException("the wind table has no row " + row);
		}
		return rows.get(row);
	}

	private static int windColumn(String column) {
		int index = WIND_COLUMNS.indexOf(column);
		if (index < 0) {
			throw new IllegalArgumentException("the wind table's columns are " + WIND_COLUMNS + ", not " + column);
		}
		return index;
	}

	private static JButton button(Container container, String text) {
		for (JButton b : find(container, JButton.class)) {
			if (text.equals(b.getText())) {
				return b;
			}
		}
		throw new IllegalStateException("no button " + text);
	}

	/**
	 * A cell of the wind table: {"row", "column", "text"}, the text typed into the row's spinner in the column's unit
	 * (its header's). An altitude another level has is refused (bug 31); an altitude that puts the levels out of
	 * order gets QtRocket's sorted model (bug 32: OpenRocket sorts the rows, not the model).
	 */
	private void cellEdit(MultiLevelWindTable table, MultiLevelPinkNoiseWindModel model, JsonObject cell) {
		Questions.members(cell, Set.of("row", "column", "text"), Set.of("row", "column", "text"));
		JComponent row = row(table, cell.get("row").getAsInt());
		int column = windColumn(Questions.string(cell, "column"));
		JSpinner spinner = find(row, JSpinner.class).get(column);
		DoubleModel dm = Questions.spinnerModel(spinner);
		String text = Questions.string(cell, "text");
		double typed = parse(dm.getCurrentUnit(), text);
		if (column == 0) {
			double min = (Double) Reflection.get(DoubleModel.class, dm, "minValue");
			double altitude = Math.max(min, typed);
			LevelWindModel edited = (LevelWindModel) Reflection.invoke(row, "getLevel");
			for (LevelWindModel level : model.getLevels()) {
				if (level != edited && level.getAltitude() == altitude) {
					throw new IllegalStateException("bug 31: two wind levels at the altitude " + altitude
							+ " m (both are deleted together, and the saved design cannot be opened)");
				}
			}
		}
		if (typed == dm.getValue()) {
			throw new IllegalStateException("the cell shows that value already");
		}
		spinner.getModel().setValue(text);
		if (column == 0) {
			List<LevelWindModel> levels = model.getLevels();
			for (int i = 1; i < levels.size(); i++) {
				if (levels.get(i - 1).getAltitude() > levels.get(i).getAltitude()) {
					model.sortLevels();
					addToolFix(32, "the wind levels sorted after an altitude edit");
					break;
				}
			}
		}
	}

	/**
	 * The wind table's delete buttons as QtRocket's table shows them after a reset or an import: enabled exactly
	 * while there is more than one level (OpenRocket's resetLevels() and importLevels() make new rows without
	 * updating their buttons, MultiLevelWindTable.java:452-500: a lone level keeps an enabled button).
	 */
	private void deleteButtonsFix(MultiLevelWindTable table) {
		List<JComponent> rows = rows(table);
		boolean stale = false;
		for (JComponent row : rows) {
			JButton delete = (JButton) Reflection.get(row.getClass(), row, "deleteButton");
			stale |= delete.isEnabled() != rows.size() > 1;
		}
		if (stale) {
			Reflection.invoke(table, "updateDeleteButtonsState");
			addToolFix(0, "the wind table's delete buttons updated after a reset or an import");
		}
	}

	/**
	 * Import levels (MultiLevelWindEditDialog.importLevels, :220-306): {"settings": the CSV import settings dialog's
	 * answer, "file": the file chooser's answer (after OK), "overwrite": the overwrite question's answer (asked while
	 * the model has levels)}. A file without the extension .csv is refused (bug 37: OpenRocket appends .csv to it).
	 */
	private void importLevels(MultiLevelWindEditDialog editor, MultiLevelWindTable table, JsonObject answer) {
		Questions.members(answer, Set.of("settings"), Set.of("settings", "file", "overwrite"));
		List<Pending> answers = new ArrayList<>();
		JsonObject settings = answer.getAsJsonObject("settings");
		answers.add(new Pending("the answer to the CSV import settings", s -> csvSettings(s, settings)));
		if (answer.has("file")) {
			JsonObject file = answer.getAsJsonObject("file");
			String name = Questions.string(file, "file");
			if (name != null) {
				int dot = Path.of(name).getFileName().toString().lastIndexOf('.');
				String extension = dot > 0 ? Path.of(name).getFileName().toString().substring(dot + 1) : "";
				if (!extension.toLowerCase(Locale.ENGLISH).equals("csv")) {
					throw new IllegalArgumentException("bug 37: OpenRocket appends .csv to the chosen file " + name);
				}
			}
			answers.add(new Pending("the answer to the file chooser", f -> openFile(f, file)));
		}
		if (answer.has("overwrite")) {
			JsonObject overwrite = answer.getAsJsonObject("overwrite");
			answers.add(new Pending("the answer to the overwrite question",
					q -> confirm(q, overwrite, Set.of("yes", "no"))));
		}
		clickWith(button(editor, trans.get("WindProfileEditorDlg.but.importLevels")), answers);
		deleteButtonsFix(table);
	}

	/**
	 * CSVImportSettingsDialog: {"hasHeader": its box, "columns": {"altitude" | "speed" | "direction" |
	 * "standardDeviation": {"name": the column name field's text, "index": the column index spinner's value,
	 * "unit": the unit selector's unit}}, "separator": the separator itself (",", ";", " " or a tab, chosen as the
	 * box's item; another text typed into the box), "button": "ok" | "cancel"}, every member but the button optional
	 * and applied in this order. A SPACE or TAB separator gets QtRocket's fix of bug 34: the box is given the
	 * separator itself before OK, which the import then splits at (OpenRocket's would split at the word "SPACE" and
	 * store the word as the export separator).
	 */
	private void csvSettings(JDialog d, JsonObject answer) {
		if (!(d instanceof CSVImportSettingsDialog dialog)) {
			throw new IllegalStateException("not the CSV import settings: " + d.getTitle());
		}
		Questions.members(answer, Set.of("button"), Set.of("hasHeader", "columns", "separator", "button"));
		JsonObject question = new JsonObject();
		question.addProperty("question", "csvImportSettings");
		csvState(dialog).entrySet().forEach(e -> question.add(e.getKey(), e.getValue()));
		asked.add(question);
		if (answer.has("hasHeader")) {
			JCheckBox box = (JCheckBox) Reflection.get(CSVImportSettingsDialog.class, dialog, "hasHeaderCheckBox");
			if (box.isSelected() == answer.get("hasHeader").getAsBoolean()) {
				throw new IllegalStateException("the header box is " + box.isSelected() + " already");
			}
			box.doClick(0);
		}
		if (answer.has("columns")) {
			JsonObject columns = answer.getAsJsonObject("columns");
			for (String row : columns.keySet()) {
				int index = CSV_ROWS.indexOf(row);
				if (index < 0) {
					throw new IllegalArgumentException("the CSV import's rows are " + CSV_ROWS + ", not " + row);
				}
				csvColumn(dialog, CSV_FIELDS.get(index), columns.getAsJsonObject(row));
			}
		}
		@SuppressWarnings("unchecked")
		JComboBox<String> separators = (JComboBox<String>) Reflection.get(CSVImportSettingsDialog.class, dialog,
				"separatorComboBox");
		if (answer.has("separator")) {
			chooseSeparator(separators, Questions.string(answer, "separator"));
		}
		question.add("answered", csvState(dialog));
		String button = Questions.string(answer, "button");
		if (button.equals("ok")) {
			String shown = String.valueOf(separators.getSelectedItem());
			if (shown.equals(trans.get("CsvOptionPanel.separator.space"))
					|| shown.equals(trans.get("CsvOptionPanel.separator.tab"))) {
				separators.setSelectedItem(shown.equals(trans.get("CsvOptionPanel.separator.space")) ? " " : "\t");
				addToolFix(34, "the wind CSV import is given the separator itself, not the word " + shown);
			}
			Questions.click(dialog, trans.get("button.ok"));
		} else if (button.equals("cancel")) {
			Questions.click(dialog, trans.get("button.cancel"));
		} else {
			throw new IllegalArgumentException("the CSV import settings' buttons are ok and cancel");
		}
	}

	/** One row of the CSV import settings (@p prefix: altitude, speed, direction, stdDeviation). */
	private static void csvColumn(CSVImportSettingsDialog dialog, String prefix, JsonObject column) {
		Questions.members(column, Set.of(), Set.of("name", "index", "unit"));
		if (column.has("name")) {
			JTextField field = (JTextField) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "ColumnField");
			field.setText(Questions.string(column, "name"));
		}
		if (column.has("index")) {
			JSpinner spinner = (JSpinner) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "ColumnSpinner");
			spinner.setValue(column.get("index").getAsInt());
		}
		if (column.has("unit")) {
			UnitSelector selector = (UnitSelector) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "UnitSelector");
			selector.setSelectedUnit(Questions.unit(selector.getUnitGroup(), Questions.string(column, "unit")));
		}
	}

	/** What the CSV import settings show: the title, the header box, each row's name, index and unit, the separator. */
	private static JsonObject csvState(CSVImportSettingsDialog dialog) {
		JsonObject state = new JsonObject();
		state.addProperty("title", dialog.getTitle());
		state.addProperty("hasHeader", dialog.hasHeader());
		JsonObject columns = new JsonObject();
		for (int i = 0; i < CSV_ROWS.size(); i++) {
			String prefix = CSV_FIELDS.get(i);
			JsonObject column = new JsonObject();
			column.addProperty("name", ((JTextField) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "ColumnField")).getText());
			column.addProperty("index", (Integer) ((JSpinner) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "ColumnSpinner")).getValue());
			column.addProperty("unit", ((UnitSelector) Reflection.get(CSVImportSettingsDialog.class, dialog,
					prefix + "UnitSelector")).getSelectedUnit().getUnit());
			columns.add(CSV_ROWS.get(i), column);
		}
		state.add("columns", columns);
		state.addProperty("separator", String.valueOf(((JComboBox<?>) Reflection.get(CSVImportSettingsDialog.class,
				dialog, "separatorComboBox")).getSelectedItem()));
		return state;
	}

	/**
	 * A separator chosen in a FieldSeparatorComboBox: "," and ";" their items, " " the SPACE item, a tab the TAB item,
	 * another text typed into the editable box.
	 */
	private static void chooseSeparator(JComboBox<?> combo, String separator) {
		Object item = switch (separator) {
			case " " -> trans.get("CsvOptionPanel.separator.space");
			case "\t" -> trans.get("CsvOptionPanel.separator.tab");
			default -> separator;
		};
		if (item.equals(combo.getSelectedItem())) {
			throw new IllegalStateException("the separator box shows " + item + " already");
		}
		combo.setSelectedItem(item);
	}

	// ------------------------------------------------------------------ the lookup dialog

	/**
	 * AerodynamicLookupDialog ("Configure..." of the options page): {"drag": [...], "stability": [...] (the actions of
	 * each section, its tab selected first), "button": "ok" | "cancel"}; an action has exactly one of "separator"
	 * (the separator itself, as chooseSeparator() chooses it), "load" (the file chooser's answer of "Load from
	 * file..."), "text" (the loaded data's text area set to it), "refresh" (true) and "clear" (true).
	 */
	private void lookup(JDialog dialog, JsonObject answer) {
		if (!dialog.getClass().getName().equals(LOOKUP_DIALOG)) {
			throw new IllegalStateException("not the aerodynamic lookup dialog: " + dialog.getTitle());
		}
		Questions.members(answer, Set.of("button"), Set.of("drag", "stability", "button"));
		JsonObject question = new JsonObject();
		question.addProperty("question", "lookup");
		lookupState(dialog).entrySet().forEach(e -> question.add(e.getKey(), e.getValue()));
		asked.add(question);
		JTabbedPane tabs = Questions.of(dialog, JTabbedPane.class).get(0);
		for (int section = 0; section < LOOKUP_SECTIONS.size(); section++) {
			String name = LOOKUP_SECTIONS.get(section);
			if (!answer.has(name)) {
				continue;
			}
			if (tabs.getSelectedIndex() != section) {
				tabs.setSelectedIndex(section);
			}
			for (JsonElement action : answer.getAsJsonArray(name)) {
				try {
					lookupAction(dialog, (Container) tabs.getComponentAt(section), name, action.getAsJsonObject());
				} catch (RuntimeException e) {
					throw new IllegalStateException("the " + name + " action " + action + " failed", e);
				}
				flushQueue();
			}
		}
		question.add("answered", lookupState(dialog));
		switch (Questions.string(answer, "button")) {
			case "ok" -> Questions.click(dialog, trans.get("button.ok"));
			case "cancel" -> Questions.click(dialog, trans.get("button.cancel"));
			default -> throw new IllegalArgumentException("the lookup dialog's buttons are ok and cancel");
		}
	}

	private void lookupAction(JDialog dialog, Container page, String section, JsonObject action) {
		if (action.keySet().size() != 1) {
			throw new IllegalArgumentException("a lookup action has exactly one member");
		}
		String key = action.keySet().iterator().next();
		String prefix = section;
		String upper = section.substring(0, 1).toUpperCase(Locale.ROOT) + section.substring(1);
		switch (key) {
			case "separator" -> chooseSeparator((JComboBox<?>) Reflection.get(dialog.getClass(), dialog,
					prefix + "SeparatorCombo"), Questions.string(action, key));
			case "load" -> {
				JsonObject file = action.getAsJsonObject(key);
				clickWith(button(page, trans.get("AerodynamicLookupDialog.btn.loadFromFile")),
						List.of(new Pending("the answer to the file chooser", f -> openFile(f, file))));
			}
			case "text" -> {
				JTextArea area = (JTextArea) Reflection.get(dialog.getClass(), dialog, prefix + "ExampleArea");
				if (!area.isEditable()) {
					throw new IllegalStateException("the " + section + " text is not editable (no table is loaded)");
				}
				area.setText(Questions.string(action, key));
			}
			case "refresh" -> {
				JButton refresh = (JButton) Reflection.get(dialog.getClass(), dialog, prefix + "RefreshButton");
				clickWith(refresh, List.of());
			}
			case "clear" -> {
				JButton clear = (JButton) Reflection.get(dialog.getClass(), dialog, "clear" + upper + "Button");
				clickWith(clear, List.of());
			}
			default -> throw new IllegalArgumentException("a lookup action the tool does not know: " + key);
		}
	}

	/** The pattern of the column list of a lookup summary (AerodynamicLookupDialog.formatLookupSummary). */
	private static final java.util.regex.Pattern SUMMARY_COLUMNS = java.util.regex.Pattern
			.compile("columns: ([^<(]*?)(?= <i>\\(modified\\)</i>| \\(modified\\)|</html>|$)");

	/**
	 * @p text with the column list of a lookup summary sorted: OpenRocket lists a stability table's columns in the
	 * order of a Set.copyOf(), which changes from one JVM to the next (MachAoALookup.getValueColumns()).
	 */
	static String sortedColumns(String text) {
		if (text == null) {
			return null;
		}
		java.util.regex.Matcher m = SUMMARY_COLUMNS.matcher(text);
		if (!m.find()) {
			return text;
		}
		List<String> columns = new ArrayList<>(List.of(m.group(1).split(", ")));
		Collections.sort(columns);
		return text.substring(0, m.start(1)) + String.join(", ", columns) + text.substring(m.end(1));
	}

	/**
	 * What the lookup dialog shows: its title, the selected section, and per section its summary label (and its tool
	 * tip; the column list sorted, sortedColumns()), the titled border of its text, the text, whether it is editable,
	 * the separator box's item and whether Clear and Refresh are enabled.
	 */
	private static JsonObject lookupState(JDialog dialog) {
		JsonObject state = new JsonObject();
		state.addProperty("title", dialog.getTitle());
		JTabbedPane tabs = Questions.of(dialog, JTabbedPane.class).get(0);
		state.addProperty("tab", LOOKUP_SECTIONS.get(tabs.getSelectedIndex()));
		for (String section : LOOKUP_SECTIONS) {
			String upper = section.substring(0, 1).toUpperCase(Locale.ROOT) + section.substring(1);
			JsonObject s = new JsonObject();
			JLabel summary = (JLabel) Reflection.get(dialog.getClass(), dialog, section + "SummaryLabel");
			s.addProperty("summary", sortedColumns(summary.getText()));
			s.addProperty("summaryToolTip", sortedColumns(summary.getToolTipText()));
			TitledBorder border = (TitledBorder) Reflection.get(dialog.getClass(), dialog, section + "ExampleBorder");
			s.addProperty("border", border.getTitle());
			JTextArea area = (JTextArea) Reflection.get(dialog.getClass(), dialog, section + "ExampleArea");
			s.addProperty("text", area.getText());
			s.addProperty("editable", area.isEditable());
			s.addProperty("separator", String.valueOf(((JComboBox<?>) Reflection.get(dialog.getClass(), dialog,
					section + "SeparatorCombo")).getSelectedItem()));
			s.addProperty("clearEnabled", ((JButton) Reflection.get(dialog.getClass(), dialog,
					"clear" + upper + "Button")).isEnabled());
			s.addProperty("refreshEnabled", ((JButton) Reflection.get(dialog.getClass(), dialog,
					section + "RefreshButton")).isEnabled());
			state.add(section, s);
		}
		return state;
	}
}
