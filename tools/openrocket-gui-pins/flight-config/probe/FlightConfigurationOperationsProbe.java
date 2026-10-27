package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.IgnitionEvent;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.ComponentChangeEvent;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration.DeployEvent;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration.SeparationEvent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.Pair;
import info.openrocket.swing.gui.dialogs.motor.MotorChooserDialog;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorSelectionPanel;
import info.openrocket.swing.gui.main.BasicFrame;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.FlightConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurablePanel;
import info.openrocket.swing.gui.main.flightconfigpanel.MotorConfigurationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;
import sun.misc.Unsafe;

import java.awt.Component;
import java.awt.Container;
import java.awt.Font;
import java.awt.Window;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.IdentityHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.function.Consumer;
import javax.swing.AbstractAction;
import javax.swing.DefaultListSelectionModel;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JRadioButton;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.tree.DefaultTreeSelectionModel;

/**
 * The operations of OpenRocket's "Motors &amp; Configuration" tab, run by OpenRocket's own code: what QtRocket's
 * FlightConfigurationEditing (part F3 of Milestone 3, run 2) must do. A generalisation of the M3 config-tab scout's
 * TabProbe: OpenRocket's real FlightConfigurationPanel with its three sub-panels, in a frame on a virtual display
 * (xvfb-run; nothing is shown on a desktop), with a BasicFrame allocated without its constructor (only its
 * document, rocket and selection model set, and registered in BasicFrame's list of frames, which New and Duplicate
 * look the document up in). Each scenario selects cells of the panels' tables (JTable.changeSelection(), as a click
 * does), switches the sub-tabs, calls the panels' operations (their private methods, which the buttons' actions
 * call, by reflection), undoes and redoes, and answers every modal dialog the operation opens from a watcher thread
 * (inside the dialog's modal loop, on the event dispatch thread): the value dialogs through their radio buttons,
 * combo boxes, spinners and buttons, the motor chooser through its private select() and its delay field (as the
 * scout's TabProbe did), the rename dialog through its text field and buttons, the message of "Save as default"
 * through its OK button.
 * <p>
 * QtRocket's fix of OpenRocket's bug 5 (plan 10.3): New, Duplicate and the motor mount check box add an undo position
 * of their own in QtRocket ("New Configuration", "Duplicate Configuration", "Modify motor mount"), OpenRocket's none.
 * The probe calls document.addUndoPosition() with those texts right before it runs these three, so that the undo
 * histories it prints are those of OpenRocket with QtRocket's fix (an EDIT of the pins, as the rocket-panel family's;
 * README.md). So is QtRocket's fix of bug 6 (MotorConfigurationSet keeps a mount's default ignition when the rocket is
 * copied): OpenRocket's undo history holds copies of the rocket whose mounts have lost their default ignition (a new
 * one, AUTOMATIC without delay and not overridden), so an undo or a redo gives every mount that default. The probe
 * keeps beside each state of the history the default ignitions the rocket had when the state was stored (at the
 * start of the step that stored it: no step changes a default before it stores a state), and gives the mounts those
 * defaults back after an undo or a redo, as QtRocket's history has them. Every other step is OpenRocket's as it is,
 * its bugs included: the test applies QtRocket's documented fixes to the steps where they show.
 * <p>
 * Every design's UUIDs are seeded (DeterministicUuids, reseeded from the scenario's name), so the new configurations'
 * ids are the same on every run; the transcript names configurations by their row (rocket.getIds()) only.
 * <p>
 * Output (one line each, the fields separated by tabs; doubles as Java 17's Double.toString; null as &lt;null&gt;):
 * <ul>
 * <li>D scenario source (source: example:FILE or new)</li>
 * <li>STEP n kind args... (the step, as the test replays it: SELECT tab row0 col0 row1 col1, TAB tab, the operations
 * with their answers, UNDO, REDO, MOUNT row value, EDIT what)</li>
 * <li>CELLS tab rows cols (the cells an operation of a sub-panel acts on: the rows of getSelectedConfigurationIds()
 * and the columns of getSelectedComponents(), comma-separated)</li>
 * <li>Q kind ... (a dialog as it opened: IGNITION overrideText selectedRadio event delay; DEPLOYMENT overrideText
 * selectedRadio event altitude delay altitudeEnabled items; SEPARATION the same; RENAME text; MOTOR row col (the
 * cell the chooser is opened for); MESSAGE title text)</li>
 * <li>A kind ... (the dialog's answer, read back from it: IGNITION ok|cancel scope event delay; DEPLOYMENT and
 * SEPARATION ok|cancel scope event altitude delay; MOTOR ok manufacturer designation digest delay nozzle |
 * cancel)</li>
 * <li>EVENT type sourceClass sourceName origin (a change event of the rocket during the step; origin: panel, one of
 * the three that FlightConfigurationPanel.configurationChanged() fires, one per panel; op, a sub-panel's
 * fireTableDataChanged(); core, any other)</li>
 * <li>SEL row (the rocket's selected configuration, -1 for the default)</li>
 * <li>CONF row raw name overridden</li>
 * <li>SIM name row</li>
 * <li>MOUNT name acting defaultEvent defaultDelay defaultOverride; ME row motorName digest ejectionDelay nozzle event
 * delay override (each motor configuration of the mount)</li>
 * <li>DEV name event altitude delay; DE row event altitude delay (each override)</li>
 * <li>STG name event altitude delay; SE row event altitude delay</li>
 * <li>MH|RH|TH col header; MT row col motorText ignitionText ignitionItalic; RT|TT row col text (the tables, as the
 * panels' renderers draw them)</li>
 * <li>UNDO position available description next redoAvailable redoDescription descriptions...</li>
 * <li>END</li>
 * </ul>
 * usage: FlightConfigurationOperationsProbe EXAMPLEDIR
 */
public class FlightConfigurationOperationsProbe {

	static final int MOTORS = 0;
	static final int RECOVERY = 1;
	static final int STAGES = 2;

	static Unsafe unsafe;
	static OpenRocketDocument doc;
	static Rocket rocket;
	static FlightConfigurationPanel panel;
	static JFrame window;
	static final List<String> events = new ArrayList<>();
	static int step;
	/** Bug 6: the default ignitions of the mounts (by component id) the rocket had when a state of the undo history was stored. */
	static final Map<Rocket, Map<String, Object[]>> historyDefaults = new IdentityHashMap<>();
	/** The default ignitions at the start of the step. */
	static Map<String, Object[]> stepDefaults = new HashMap<>();

	public static class FakeFrame extends BasicFrame {
		public FakeFrame() {
			super(null);
		}

		@Override
		public String toString() {
			return "FakeFrame";
		}
	}

	public static void main(String[] args) throws Exception {
		Field f = Unsafe.class.getDeclaredField("theUnsafe");
		f.setAccessible(true);
		unsafe = (Unsafe) f.get(null);
		bootstrap();
		SwingUtilities.invokeAndWait(() -> UITheme.Themes.LIGHT.applyTheme());
		File dir = new File(args[0]);
		int status = 0;
		try {
			simpleMotors(dir);
			simpleConfigurations(dir);
			simpleRecovery(dir);
			simpleMount(dir);
			twoStage(dir);
			separationCancel(dir);
			newDesign();
			overrideWithoutConfiguration(dir);
			overrideCellsWithoutConfiguration(dir);
			saveAsDefault(dir);
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	// ------------------------------------------------------------------ the scenarios

	static void simpleMotors(File dir) throws Exception {
		open(dir, "simple-motors", "A simple model rocket.ork");
		select(MOTORS, 1, 1, 1, 1);
		ignition(false, null, null, null);
		ignition(true, "THIS", IgnitionEvent.LAUNCH, 2.0);
		select(MOTORS, 2, 1, 2, 1);
		ignition(true, "ALL", IgnitionEvent.BURNOUT, null);
		operation("RESET_IGNITION", MOTORS, "resetIgnition");
		operation("DELETE_MOTOR", MOTORS, "deleteMotor");
		undo();
		select(MOTORS, 0, 1, 1, 1);
		operation("DELETE_MOTOR", MOTORS, "deleteMotor");
		undo();
		select(MOTORS, 0, 1, 0, 1);
		selectMotor(null, 0);
		selectMotor("C6", 7.0);
		select(MOTORS, 1, 1, 3, 1);
		ignition(true, "THIS", IgnitionEvent.EJECTION_CHARGE, 1.0);
		operation("RESET_IGNITION", MOTORS, "resetIgnition");
		selectMotor("A8", 3.0);
		undo();
		redo();
		close();
	}

	static void simpleConfigurations(File dir) throws Exception {
		open(dir, "simple-configurations", "A simple model rocket.ork");
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, null, 0));
		rename(d -> renameOk(d, "My {motors} {cases}"), "ok", "My {motors} {cases}");
		rename(d -> click(d, "Reset to default"), "reset");
		rename(d -> click(d, "Cancel"), "cancel");
		duplicate();
		select(MOTORS, 0, 1, 2, 1);
		rename(d -> renameOk(d, "Flight {motors}"), "ok", "Flight {motors}");
		select(MOTORS, 0, 1, 2, 1);
		duplicate();
		select(MOTORS, 5, 1, 5, 1);
		deleteConfiguration();
		undo();
		redo();
		select(MOTORS, 6, 1, 8, 1);
		deleteConfiguration();
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, "C6", 5.0));
		close();
	}

	static void simpleRecovery(File dir) throws Exception {
		open(dir, "simple-recovery", "A simple model rocket.ork");
		tab(RECOVERY);
		select(RECOVERY, 1, 1, 1, 1);
		deployment(true, "ALL", DeployEvent.APOGEE, null, null);
		select(RECOVERY, 2, 1, 2, 1);
		deployment(true, "THIS", DeployEvent.ALTITUDE, 150.0, 1.5);
		operation("RESET_DEPLOYMENT", RECOVERY, "resetDeployment");
		select(RECOVERY, 0, 1, 2, 1);
		deployment(true, "THIS", DeployEvent.ALTITUDE, 200.0, null);
		deployment(false, null, DeployEvent.NEVER, null, 3.0);
		operation("RESET_DEPLOYMENT", RECOVERY, "resetDeployment");
		newConfiguration("SELECT_DEPLOYMENT", d -> answerDeployment(d, true, "THIS", DeployEvent.NEVER, null, null));
		close();
	}

	static void simpleMount(File dir) throws Exception {
		open(dir, "simple-mount", "A simple model rocket.ork");
		mount(0, true);
		mount(0, false);
		undo();
		undo();
		redo();
		close();
	}

	static void twoStage(File dir) throws Exception {
		open(dir, "two-stage", "Two stage high power rocket.ork");
		tab(STAGES);
		select(STAGES, 0, 1, 0, 1);
		separation(true, "ALL", SeparationEvent.BURNOUT, null, 1.0);
		operation("RESET_SEPARATION", STAGES, "resetSeparation");
		select(STAGES, 0, 1, 1, 1);
		separation(true, "THIS", SeparationEvent.NEVER, null, null);
		select(STAGES, 1, 1, 1, 1);
		separation(true, "THIS", SeparationEvent.ALTITUDE_DESCENDING, 250.0, null);
		operation("RESET_SEPARATION", STAGES, "resetSeparation");
		tab(MOTORS);
		select(MOTORS, 0, 1, 1, 2);
		ignition(true, "THIS", IgnitionEvent.EJECTION_CHARGE, 1.0);
		operation("RESET_IGNITION", MOTORS, "resetIgnition");
		operation("DELETE_MOTOR", MOTORS, "deleteMotor");
		undo();
		tab(RECOVERY);
		select(RECOVERY, 0, 1, 1, 3);
		deployment(true, "ALL", DeployEvent.EJECTION, null, 2.0);
		operation("RESET_DEPLOYMENT", RECOVERY, "resetDeployment");
		close();
	}

	static void separationCancel(File dir) throws Exception {
		open(dir, "separation-cancel", "Two stage high power rocket.ork");
		tab(STAGES);
		select(STAGES, 0, 1, 0, 1);
		// OpenRocket's dialog edits the override in place, and Cancel keeps the edits (bug 3): the scenario's last
		// step.
		separation(false, null, SeparationEvent.ALTITUDE_ASCENDING, 300.0, 0.5);
		close();
	}

	static void newDesign() throws Exception {
		DeterministicUuids.reseed("new-design");
		open(OpenRocketDocumentFactory.createNewRocket(), "new-design", "new");
		edit("ADD_NOSE_AND_BODY", () -> {
			rocket.getChild(0).addChild(new NoseCone());
			rocket.getChild(0).addChild(new BodyTube());
		});
		edit("BODY_MOUNT", () -> ((BodyTube) rocket.getChild(0).getChild(1)).setMotorMount(true));
		edit("ADD_PARACHUTE", () -> rocket.getChild(0).getChild(1).addChild(new Parachute()));
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, "D12", 5.0));
		edit("ADD_STAGE", () -> {
			AxialStage stage = new AxialStage();
			rocket.addChild(stage);
			stage.addChild(new BodyTube());
		});
		tab(STAGES);
		edit("REMOVE_STAGE", () -> rocket.removeChild(1));
		select(MOTORS, 0, 1, 0, 1);
		// OpenRocket's Cancel marks the ignition overridden (bug 3): the scenario's last step.
		ignition(false, null, IgnitionEvent.NEVER, 4.0);
		close();
	}

	static void overrideWithoutConfiguration(File dir) throws Exception {
		open(dir, "override-without-configuration", "A simple model rocket.ork");
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, null, 0));
		select(MOTORS, 5, 1, 5, 1);
		// OpenRocket edits the mount's default configuration (bug 4): the scenario's last step.
		ignition(true, "THIS", IgnitionEvent.BURNOUT, 0.5);
		close();
	}

	static void overrideCellsWithoutConfiguration(File dir) throws Exception {
		open(dir, "override-cells-without-configuration", "A simple model rocket.ork");
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, null, 0));
		select(MOTORS, 4, 1, 5, 1);
		// OpenRocket writes the other cell's values into the mount's default (bug 4): the scenario's last step.
		ignition(true, "THIS", IgnitionEvent.EJECTION_CHARGE, 1.0);
		close();
	}

	static void saveAsDefault(File dir) throws Exception {
		open(dir, "save-as-default", "A simple model rocket.ork");
		select(MOTORS, 0, 1, 0, 1);
		rename(d -> {
			text(d, "Test {motors}");
			dialog(m -> {
				dumpMessage(m);
				click(m, "OK");
			}, () -> click(d, "Save as default"));
			click(d, "Cancel");
		}, "savedefault", "Test {motors}", "cancel");
		newConfiguration("SELECT_MOTOR", d -> chooseMotor(d, null, 0));
		undo();
		close();
		// The default name back for anything after this scenario.
		Application.getPreferences().setDefaultFlightConfigName(null);
	}

	// ------------------------------------------------------------------ the steps

	static void begin(Object... fields) {
		step++;
		events.clear();
		try {
			onEdt(() -> stepDefaults = defaults());
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
		Object[] all = new Object[fields.length + 2];
		all[0] = "STEP";
		all[1] = step;
		System.arraycopy(fields, 0, all, 2, fields.length);
		line(all);
	}

	static void select(int tab, int row0, int col0, int row1, int col1) throws Exception {
		begin("SELECT", tab, row0, col0, row1, col1);
		onEdt(() -> {
			JTable table = table(subPanel(tab));
			table.changeSelection(row0, col0, false, false);
			if (row1 != row0 || col1 != col0) {
				table.changeSelection(row1, col1, false, true);
			}
		});
		end();
	}

	static void tab(int index) throws Exception {
		begin("TAB", index);
		onEdt(() -> tabs().setSelectedIndex(index));
		end();
	}

	static void undo() throws Exception {
		begin("UNDO");
		onEdt(() -> {
			doc.undo();
			restoreDefaults();
		});
		end();
	}

	static void redo() throws Exception {
		begin("REDO");
		onEdt(() -> {
			doc.redo();
			restoreDefaults();
		});
		end();
	}

	/** The default ignition (event, delay, override) of every motor mount, by the mount's id. */
	static Map<String, Object[]> defaults() {
		Map<String, Object[]> map = new HashMap<>();
		if (rocket != null) {
			for (RocketComponent c : rocket) {
				if (c instanceof MotorMount m) {
					MotorConfiguration d = m.getDefaultMotorConfig();
					map.put(c.getID().toString(),
							new Object[] {d.getIgnitionEvent(), d.getIgnitionDelay(), d.hasIgnitionOverride()});
				}
			}
		}
		return map;
	}

	/** Gives the states the undo history stored during the step the defaults of the step's start (bug 6). */
	@SuppressWarnings("unchecked")
	static void recordHistoryDefaults() {
		for (Rocket state : (List<Rocket>) get(OpenRocketDocument.class, doc, "undoHistory")) {
			historyDefaults.putIfAbsent(state, stepDefaults);
		}
	}

	/** After an undo or a redo: the mounts' default ignitions of the state loaded, as QtRocket keeps them (bug 6). */
	@SuppressWarnings("unchecked")
	static void restoreDefaults() {
		recordHistoryDefaults();
		List<Rocket> history = (List<Rocket>) get(OpenRocketDocument.class, doc, "undoHistory");
		Map<String, Object[]> stored = historyDefaults.get(history.get((Integer) get(OpenRocketDocument.class, doc,
				"undoPosition")));
		for (RocketComponent c : rocket) {
			if (c instanceof MotorMount m && stored.containsKey(c.getID().toString())) {
				Object[] values = stored.get(c.getID().toString());
				MotorConfiguration d = m.getDefaultMotorConfig();
				set(MotorConfiguration.class, d, "ignitionEvent", values[0]);
				set(MotorConfiguration.class, d, "ignitionDelay", values[1]);
				set(MotorConfiguration.class, d, "ignitionOveride", values[2]);
			}
		}
	}

	static void edit(String what, Runnable change) throws Exception {
		begin("EDIT", what);
		onEdt(change);
		end();
	}

	/** A sub-panel's operation without a dialog (Reset ..., Delete motor). */
	static void operation(String name, int tab, String method) throws Exception {
		begin(name);
		onEdt(() -> {
			cells(tab);
			invoke(subPanel(tab), method);
		});
		end();
	}

	/** The motor mount check box of row @p row (MotorMountTableModel.setValueAt()), with bug 5's undo position. */
	static void mount(int row, boolean value) throws Exception {
		begin("MOUNT", row, value);
		onEdt(() -> {
			doc.addUndoPosition("Modify motor mount");
			JTable mounts = null;
			for (Component c : all(motorPanel())) {
				if (c instanceof JTable t && t != table(motorPanel())) {
					mounts = t;
				}
			}
			mounts.getModel().setValueAt(value, row, 0);
		});
		end();
	}

	static void ignition(boolean ok, String scope, IgnitionEvent event, Double delay) throws Exception {
		begin("SELECT_IGNITION", ok ? "ok" : "cancel");
		onEdt(() -> cells(MOTORS));
		dialog(d -> {
			dumpIgnition(d);
			if (scope != null) {
				radio(d, scope.equals("ALL") ? 0 : 1);
			}
			if (event != null) {
				combo(d, 0, event);
			}
			if (delay != null) {
				spinner(d, 0, delay);
			}
			if (ok) {
				click(d, "OK");
				MotorConfiguration c = (MotorConfiguration) get(d.getClass(), d, "curMotorInstance");
				line("A", "IGNITION", "ok", ((Boolean) invoke(d, "isOverrideDefault")) ? "THIS" : "ALL",
						c.getIgnitionEvent().name(), c.getIgnitionDelay());
			} else {
				click(d, "Cancel");
				line("A", "IGNITION", "cancel");
			}
		}, () -> invoke(motorPanel(), "selectIgnition"));
		end();
	}

	static void deployment(boolean ok, String scope, DeployEvent event, Double altitude, Double delay)
			throws Exception {
		begin("SELECT_DEPLOYMENT", ok ? "ok" : "cancel");
		onEdt(() -> cells(RECOVERY));
		dialog(d -> answerDeployment(d, ok, scope, event, altitude, delay),
				() -> invoke(subPanel(RECOVERY), "selectDeployment"));
		end();
	}

	static void answerDeployment(JDialog d, boolean ok, String scope, DeployEvent event, Double altitude,
			Double delay) {
		dumpValueDialog("DEPLOYMENT", d);
		if (scope != null) {
			radio(d, scope.equals("ALL") ? 0 : 1);
		}
		if (event != null) {
			combo(d, 0, event);
		}
		if (altitude != null) {
			spinner(d, 0, altitude);
		}
		if (delay != null) {
			spinner(d, 1, delay);
		}
		if (ok) {
			String radio = radioButtons(d).get(0).isSelected() ? "ALL" : "THIS";
			click(d, "OK");
			DeploymentConfiguration c = (DeploymentConfiguration) get(d.getClass(), d, "newConfiguration");
			line("A", "DEPLOYMENT", "ok", radio, c.getDeployEvent().name(), c.getDeployAltitude(), c.getDeployDelay());
		} else {
			click(d, "Cancel");
			line("A", "DEPLOYMENT", "cancel");
		}
	}

	static void separation(boolean ok, String scope, SeparationEvent event, Double altitude, Double delay)
			throws Exception {
		begin("SELECT_SEPARATION", ok ? "ok" : "cancel");
		onEdt(() -> cells(STAGES));
		dialog(d -> {
			dumpValueDialog("SEPARATION", d);
			if (scope != null) {
				radio(d, scope.equals("ALL") ? 0 : 1);
			}
			if (event != null) {
				combo(d, 0, event);
			}
			if (altitude != null) {
				spinner(d, 0, altitude);
			}
			if (delay != null) {
				spinner(d, 1, delay);
			}
			if (ok) {
				String radio = radioButtons(d).get(0).isSelected() ? "ALL" : "THIS";
				click(d, "OK");
				StageSeparationConfiguration c = (StageSeparationConfiguration) get(d.getClass(), d,
						"newConfiguration");
				line("A", "SEPARATION", "ok", radio, c.getSeparationEvent().name(), c.getSeparationAltitude(),
						c.getSeparationDelay());
			} else {
				click(d, "Cancel");
				line("A", "SEPARATION", "cancel");
			}
		}, () -> invoke(subPanel(STAGES), "selectSeparation"));
		end();
	}

	/** Select motor of the motor panel; @p commonName null cancels the chooser. */
	static void selectMotor(String commonName, double delay) throws Exception {
		begin("SELECT_MOTOR", commonName == null ? "cancel" : "ok");
		onEdt(() -> cells(MOTORS));
		dialog(d -> chooseMotor(d, commonName, delay), () -> motorPanel().selectMotor());
		end();
	}

	/** The motor chooser's answer: Estes' motor of @p commonName with @p delay (null: Cancel). */
	static void chooseMotor(JDialog d, String commonName, double delay) {
		dumpChooser();
		if (commonName == null) {
			invoke(d, "close", new Class<?>[] {boolean.class}, false);
			line("A", "MOTOR", "cancel");
			return;
		}
		ThrustCurveMotorSelectionPanel selection = (ThrustCurveMotorSelectionPanel) get(MotorChooserDialog.class, d,
				"selectionPanel");
		ThrustCurveMotor found = null;
		for (ThrustCurveMotorSet set : ((ThrustCurveMotorSetDatabase) Application.getMotorSetDatabase())
				.getMotorSets()) {
			for (ThrustCurveMotor m : set.getMotors()) {
				if (found == null && m.getCommonName().equals(commonName)
						&& m.getManufacturer().getSimpleName().equals("Estes")) {
					found = m;
				}
			}
		}
		invoke(selection, "select", new Class<?>[] {ThrustCurveMotor.class}, found);
		set(ThrustCurveMotorSelectionPanel.class, selection, "selectedDelay", delay);
		invoke(d, "close", new Class<?>[] {boolean.class}, true);
		MotorChooserDialog chooser = (MotorChooserDialog) d;
		ThrustCurveMotor motor = (ThrustCurveMotor) chooser.getSelectedMotor();
		line("A", "MOTOR", "ok", motor.getManufacturer().getSimpleName(), motor.getDesignation(), motor.getDigest(),
				chooser.getSelectedDelay(), chooser.getSelectedNozzleExitDiameter());
	}

	/** New Configuration, with bug 5's undo position, and its follow-up dialog answered by @p followUp. */
	static void newConfiguration(String followUp, Consumer<JDialog> answer) throws Exception {
		begin("NEW", followUp);
		dialog(d -> {
			cells(tabs().getSelectedIndex());
			answer.accept(d);
		}, () -> {
			doc.addUndoPosition("New Configuration");
			((AbstractAction) panel.getNewConfigAction()).actionPerformed(null);
		});
		end();
	}

	static void duplicate() throws Exception {
		begin("DUPLICATE");
		onEdt(() -> {
			configurationCells();
			doc.addUndoPosition("Duplicate Configuration");
			((AbstractAction) panel.getDuplicateConfigAction()).actionPerformed(null);
		});
		end();
	}

	static void deleteConfiguration() throws Exception {
		begin("DELETE_CONFIG");
		onEdt(() -> {
			configurationCells();
			((AbstractAction) panel.getDeleteConfigAction()).actionPerformed(null);
		});
		end();
	}

	static void rename(Consumer<JDialog> answer, String... what) throws Exception {
		Object[] fields = new Object[what.length + 1];
		fields[0] = "RENAME";
		System.arraycopy(what, 0, fields, 1, what.length);
		begin(fields);
		onEdt(FlightConfigurationOperationsProbe::configurationCells);
		dialog(d -> {
			line("Q", "RENAME", of(d, JTextField.class).get(0).getText());
			answer.accept(d);
		}, () -> ((AbstractAction) panel.getRenameConfigAction()).actionPerformed(null));
		end();
	}

	static void renameOk(JDialog d, String name) {
		text(d, name);
		click(d, "OK");
	}

	/** The step's events and the state after it. */
	static void end() throws Exception {
		onEdt(() -> {
			recordHistoryDefaults();
			for (String event : events) {
				System.out.println(event);
			}
			events.clear();
			dumpState();
			line("END");
		});
	}

	// ------------------------------------------------------------------ set-up

	static void open(File dir, String scenario, String file) throws Exception {
		DeterministicUuids.reseed(scenario);
		open(new GeneralRocketLoader(new File(dir, file)).load(), scenario, "example:" + file);
	}

	static void open(OpenRocketDocument d, String scenario, String source) throws Exception {
		line("D", scenario, source);
		step = 0;
		doc = d;
		rocket = d.getRocket();
		events.clear();
		historyDefaults.clear();
		onEdt(() -> {
			try {
				FakeFrame frame = (FakeFrame) unsafe.allocateInstance(FakeFrame.class);
				set(BasicFrame.class, frame, "document", doc);
				set(BasicFrame.class, frame, "rocket", rocket);
				DocumentSelectionModel selection = new DocumentSelectionModel(doc);
				selection.attachComponentTreeSelectionModel(new DefaultTreeSelectionModel());
				selection.attachSimulationListSelectionModel(new DefaultListSelectionModel());
				set(BasicFrame.class, frame, "selectionModel", selection);
				Field framesField = BasicFrame.class.getDeclaredField("frames");
				framesField.setAccessible(true);
				@SuppressWarnings("unchecked")
				List<BasicFrame> frames = (List<BasicFrame>) framesField.get(null);
				frames.clear();
				frames.add(frame);
				panel = new FlightConfigurationPanel(frame, doc);
				window = new JFrame("probe");
				window.add(panel);
				window.setSize(1100, 600);
				window.setVisible(true);
				rocket.addComponentChangeListener(FlightConfigurationOperationsProbe::recordEvent);
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		});
		// The state the scenario starts from.
		begin("OPEN");
		end();
	}

	static void recordEvent(ComponentChangeEvent e) {
		String origin = "core";
		for (StackTraceElement frame : Thread.currentThread().getStackTrace()) {
			if (frame.getClassName().equals(FlightConfigurationPanel.class.getName())
					&& frame.getMethodName().equals("configurationChanged")) {
				origin = "panel";
				break;
			}
			if (frame.getClassName().equals(FlightConfigurablePanel.class.getName())
					&& frame.getMethodName().equals("fireTableDataChanged")) {
				origin = "op";
			}
		}
		Object source = e.getSource();
		String name = source instanceof RocketComponent c ? c.getName() : String.valueOf(source);
		events.add(join("EVENT", e.getType(), source.getClass().getSimpleName(), name, origin));
	}

	static void close() throws Exception {
		onEdt(() -> window.dispose());
	}

	// ------------------------------------------------------------------ the state

	static void dumpState() {
		List<FlightConfigurationId> ids = rocket.getIds();
		line("SEL", ids.indexOf(rocket.getSelectedConfiguration().getId()));
		for (int row = 0; row < ids.size(); row++) {
			FlightConfiguration fc = rocket.getFlightConfiguration(ids.get(row));
			line("CONF", row, fc.getNameRaw(), fc.getName(), fc.isNameOverridden());
		}
		for (Simulation s : doc.getSimulations()) {
			line("SIM", s.getName(), ids.indexOf(s.getFlightConfigurationId()));
		}
		for (RocketComponent c : rocket) {
			if (c instanceof MotorMount m) {
				MotorConfiguration def = m.getDefaultMotorConfig();
				line("MOUNT", c.getName(), m.isMotorMount(), def.getIgnitionEvent().name(), def.getIgnitionDelay(),
						def.hasIgnitionOverride());
				for (FlightConfigurationId id : m.getMotorConfigurationSet().getIds()) {
					MotorConfiguration mc = m.getMotorConfigurationSet().get(id);
					Motor motor = mc.getMotor();
					line("ME", ids.indexOf(id), mc.toMotorName(), motor == null ? "-" : motor.getDigest(),
							mc.getEjectionDelay(), mc.getNozzleExitDiameter(), mc.getIgnitionEvent().name(),
							mc.getIgnitionDelay(), mc.hasIgnitionOverride());
				}
			}
			if (c instanceof RecoveryDevice r) {
				DeploymentConfiguration def = r.getDeploymentConfigurations().getDefault();
				line("DEV", c.getName(), def.getDeployEvent().name(), def.getDeployAltitude(), def.getDeployDelay());
				for (FlightConfigurationId id : r.getDeploymentConfigurations().getIds()) {
					DeploymentConfiguration dc = r.getDeploymentConfigurations().get(id);
					line("DE", ids.indexOf(id), dc.getDeployEvent().name(), dc.getDeployAltitude(),
							dc.getDeployDelay());
				}
			}
			if (c instanceof AxialStage s && s.getStageNumber() > 0) {
				StageSeparationConfiguration def = s.getSeparationConfigurations().getDefault();
				line("STG", c.getName(), def.getSeparationEvent().name(), def.getSeparationAltitude(),
						def.getSeparationDelay());
				for (FlightConfigurationId id : s.getSeparationConfigurations().getIds()) {
					StageSeparationConfiguration sc = s.getSeparationConfigurations().get(id);
					line("SE", ids.indexOf(id), sc.getSeparationEvent().name(), sc.getSeparationAltitude(),
							sc.getSeparationDelay());
				}
			}
		}
		dumpTable("M", motorPanel());
		dumpTable("R", subPanel(RECOVERY));
		dumpTable("T", subPanel(STAGES));
		@SuppressWarnings("unchecked")
		List<String> descriptions = (List<String>) get(OpenRocketDocument.class, doc, "undoDescription");
		List<Object> undo = new ArrayList<>(List.of("UNDO", get(OpenRocketDocument.class, doc, "undoPosition"),
				doc.isUndoAvailable(), text(doc.getUndoDescription()),
				text((String) get(OpenRocketDocument.class, doc, "nextDescription")), doc.isRedoAvailable(),
				text(doc.getRedoDescription())));
		for (String d : descriptions) {
			undo.add(text(d));
		}
		line(undo.toArray());
	}

	static void dumpTable(String tag, FlightConfigurablePanel<?> p) {
		JTable t = table(p);
		for (int c = 0; c < t.getColumnCount(); c++) {
			line(tag + "H", c, t.getColumnName(c));
		}
		for (int r = 0; r < t.getRowCount(); r++) {
			for (int c = 1; c < t.getColumnCount(); c++) {
				JLabel label = (JLabel) t.getCellRenderer(r, c).getTableCellRendererComponent(t, t.getValueAt(r, c),
						false, false, r, c);
				if (tag.equals("M")) {
					List<JLabel> parts = new ArrayList<>();
					for (Component k : label.getComponents()) {
						if (k instanceof JLabel l) {
							parts.add(l);
						}
					}
					Font font = parts.get(1).getFont();
					line("MT", r, c, parts.get(0).getText(), parts.get(1).getText(), font != null && font.isItalic());
				} else {
					line(tag + "T", r, c, label.getText());
				}
			}
		}
	}

	/** The cells the next operation of the sub-panel @p tab acts on. */
	static void cells(int tab) {
		FlightConfigurablePanel<?> p = subPanel(tab);
		@SuppressWarnings("unchecked")
		List<FlightConfigurationId> ids = (List<FlightConfigurationId>) invoke(p, "getSelectedConfigurationIds");
		@SuppressWarnings("unchecked")
		List<Object> components = (List<Object>) invoke(p, "getSelectedComponents");
		line("CELLS", tab, rows(ids), columns(p, components));
	}

	/** The configurations the next operation of FlightConfigurationPanel acts on (its current sub-panel's rows). */
	static void configurationCells() {
		FlightConfigurablePanel<?> p = subPanel(tabs().getSelectedIndex());
		@SuppressWarnings("unchecked")
		List<FlightConfigurationId> ids = (List<FlightConfigurationId>) invoke(p, "getSelectedConfigurationIds");
		line("CELLS", tabs().getSelectedIndex(), rows(ids), "");
	}

	static String rows(List<FlightConfigurationId> ids) {
		List<String> rows = new ArrayList<>();
		if (ids != null) {
			for (FlightConfigurationId id : ids) {
				rows.add(String.valueOf(rocket.getIds().indexOf(id)));
			}
		}
		return String.join(",", rows);
	}

	static String columns(FlightConfigurablePanel<?> p, List<Object> components) {
		List<String> columns = new ArrayList<>();
		JTable t = table(p);
		if (components != null && t.getRowCount() > 0) {
			for (Object component : components) {
				for (int c = 1; c < t.getColumnCount(); c++) {
					if (((Pair<?, ?>) t.getModel().getValueAt(0, c)).getV() == component) {
						columns.add(String.valueOf(c));
					}
				}
			}
		}
		return String.join(",", columns);
	}

	// ------------------------------------------------------------------ the dialogs

	/**
	 * Runs @p action (on the event dispatch thread, or directly when called on it); the first new modal dialog it
	 * shows (or the motor chooser, which its panel keeps) is given to @p handler on the event dispatch thread, from
	 * within the dialog's modal loop.
	 */
	static void dialog(Consumer<JDialog> handler, Runnable action) {
		Set<Window> before = new HashSet<>(Arrays.asList(Window.getWindows()));
		Throwable[] failure = new Throwable[1];
		boolean[] handled = new boolean[1];
		Thread watcher = new Thread(() -> {
			long end = System.currentTimeMillis() + 60000;
			while (System.currentTimeMillis() < end) {
				for (Window w : Window.getWindows()) {
					if (w instanceof JDialog d && d.isShowing()
							&& (!before.contains(w) || d instanceof MotorChooserDialog)) {
						try {
							Thread.sleep(200);
							SwingUtilities.invokeAndWait(() -> handler.accept(d));
							handled[0] = true;
						} catch (Throwable e) {
							// Closed, so that the operation (and the probe) does not wait for it for ever.
							e.printStackTrace();
							failure[0] = e;
							SwingUtilities.invokeLater(() -> d.setVisible(false));
						}
						return;
					}
				}
				try {
					Thread.sleep(50);
				} catch (InterruptedException e) {
					return;
				}
			}
		});
		watcher.start();
		try {
			if (SwingUtilities.isEventDispatchThread()) {
				action.run();
			} else {
				onEdt(action);
			}
			watcher.join();
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
		if (failure[0] != null) {
			throw new RuntimeException("the dialog's handler failed", failure[0]);
		}
		if (!handled[0]) {
			throw new IllegalStateException("no dialog in step " + step);
		}
	}

	static void dumpIgnition(JDialog d) {
		List<JRadioButton> radios = radioButtons(d);
		JComboBox<?> combo = of(d, JComboBox.class).get(0);
		JSpinner delay = of(d, JSpinner.class).get(0);
		line("Q", "IGNITION", radios.get(1).getText(), radios.get(0).isSelected() ? 0 : 1,
				((Enum<?>) combo.getSelectedItem()).name(), spinnerValue(delay));
	}

	static void dumpValueDialog(String kind, JDialog d) {
		List<JRadioButton> radios = radioButtons(d);
		JComboBox<?> combo = of(d, JComboBox.class).get(0);
		List<JSpinner> spinners = of(d, JSpinner.class);
		List<String> items = new ArrayList<>();
		for (int i = 0; i < combo.getItemCount(); i++) {
			items.add(((Enum<?>) combo.getItemAt(i)).name());
		}
		line("Q", kind, radios.get(1).getText(), radios.get(0).isSelected() ? 0 : 1,
				((Enum<?>) combo.getSelectedItem()).name(), spinnerValue(spinners.get(0)), spinnerValue(spinners.get(1)),
				spinners.get(0).isEnabled(), String.join(",", items));
	}

	static void dumpChooser() {
		@SuppressWarnings("unchecked")
		List<FlightConfigurationId> ids = (List<FlightConfigurationId>) invoke(motorPanel(),
				"getSelectedConfigurationIds");
		@SuppressWarnings("unchecked")
		List<Object> components = (List<Object>) invoke(motorPanel(), "getSelectedComponents");
		String row = rows(ids.subList(0, 1));
		String col = columns(motorPanel(), components.subList(0, 1));
		line("Q", "MOTOR", row, col);
	}

	static void dumpMessage(JDialog d) {
		JOptionPane pane = of(d, JOptionPane.class).get(0);
		line("Q", "MESSAGE", d.getTitle(), String.valueOf(pane.getMessage()));
	}

	static List<Component> all(Container c) {
		List<Component> out = new ArrayList<>();
		for (Component k : c.getComponents()) {
			out.add(k);
			if (k instanceof Container kk && !(k instanceof JSpinner) && !(k instanceof JComboBox)) {
				out.addAll(all(kk));
			}
		}
		return out;
	}

	static <T extends Component> List<T> of(JDialog d, Class<T> type) {
		List<T> out = new ArrayList<>();
		for (Component c : all(d.getContentPane())) {
			if (type.isInstance(c)) {
				out.add(type.cast(c));
			}
		}
		return out;
	}

	static List<JRadioButton> radioButtons(JDialog d) {
		return of(d, JRadioButton.class);
	}

	static void click(JDialog d, String text) {
		for (JButton b : of(d, JButton.class)) {
			if (text.equals(b.getText())) {
				b.doClick();
				return;
			}
		}
		throw new IllegalStateException("no button " + text);
	}

	static void radio(JDialog d, int i) {
		radioButtons(d).get(i).doClick();
	}

	static void combo(JDialog d, int i, Object item) {
		of(d, JComboBox.class).get(i).setSelectedItem(item);
	}

	/** Sets a spinner's value (in SI units) when it differs (a spinner the user leaves alone sets nothing). */
	static void spinner(JDialog d, int i, double value) {
		JSpinner s = of(d, JSpinner.class).get(i);
		if (spinnerValue(s) != value) {
			s.setValue(value);
		}
	}

	/** A spinner's value in SI units: its DoubleModel's (the spinner model, an inner class, gives a text). */
	static double spinnerValue(JSpinner s) {
		Object model = s.getModel();
		return (Double) invoke(get(model.getClass(), model, "this$0"), "getValue");
	}

	static void text(JDialog d, String value) {
		of(d, JTextField.class).stream().filter(t -> !(t.getParent() instanceof JSpinner.DefaultEditor)).toList()
				.get(0).setText(value);
	}

	// ------------------------------------------------------------------ the panels

	static JTabbedPane tabs() {
		return (JTabbedPane) get(FlightConfigurationPanel.class, panel, "tabs");
	}

	static MotorConfigurationPanel motorPanel() {
		return (MotorConfigurationPanel) get(FlightConfigurationPanel.class, panel, "motorConfigurationPanel");
	}

	static FlightConfigurablePanel<?> subPanel(int tab) {
		String name = switch (tab) {
			case MOTORS -> "motorConfigurationPanel";
			case RECOVERY -> "recoveryConfigurationPanel";
			default -> "separationConfigurationPanel";
		};
		return (FlightConfigurablePanel<?>) get(FlightConfigurationPanel.class, panel, name);
	}

	static JTable table(FlightConfigurablePanel<?> p) {
		return (JTable) get(FlightConfigurablePanel.class, p, "table");
	}

	// ------------------------------------------------------------------ helpers

	static String text(String s) {
		return s == null ? "<null>" : s;
	}

	static String join(Object... fields) {
		StringBuilder text = new StringBuilder();
		for (Object field : fields) {
			String value = String.valueOf(field);
			if (value.indexOf('\t') >= 0 || value.indexOf('\n') >= 0) {
				throw new IllegalStateException("a tab or a newline in " + value);
			}
			if (text.length() > 0) {
				text.append('\t');
			}
			text.append(value);
		}
		return text.toString();
	}

	static void line(Object... fields) {
		System.out.println(join(fields));
	}

	static void onEdt(Runnable r) throws Exception {
		SwingUtilities.invokeAndWait(r);
	}

	static void set(Class<?> c, Object o, String name, Object v) {
		try {
			Field f = c.getDeclaredField(name);
			f.setAccessible(true);
			f.set(o, v);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	static Object get(Class<?> c, Object o, String name) {
		try {
			Field f = c.getDeclaredField(name);
			f.setAccessible(true);
			return f.get(o);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	static Object invoke(Object o, String name) {
		return invoke(o, name, new Class<?>[0]);
	}

	static Object invoke(Object o, String name, Class<?>[] types, Object... args) {
		try {
			Class<?> c = o.getClass();
			while (c != null) {
				try {
					Method m = c.getDeclaredMethod(name, types);
					m.setAccessible(true);
					return m.invoke(o, args);
				} catch (NoSuchMethodException e) {
					c = c.getSuperclass();
				}
			}
			throw new NoSuchMethodException(name);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	static void bootstrap() throws Exception {
		// Before anything makes a UUID: each scenario's design gets the same UUIDs on every run.
		DeterministicUuids.install();
		DeterministicUuids.reseed("flight-config-operations");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(
				Modules.override(new info.openrocket.swing.ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
