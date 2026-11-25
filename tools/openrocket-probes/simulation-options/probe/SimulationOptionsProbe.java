package probe;

import java.awt.Component;
import java.awt.Container;
import java.awt.Dialog;
import java.awt.Rectangle;
import java.awt.Window;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

import javax.swing.AbstractButton;
import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JEditorPane;
import javax.swing.JFileChooser;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JRootPane;
import javax.swing.JScrollBar;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.KeyStroke;
import javax.swing.SpinnerModel;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.border.TitledBorder;
import javax.swing.filechooser.FileFilter;
import javax.swing.filechooser.FileNameExtensionFilter;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.aerodynamics.lookup.CsvMachAoALookup;
import info.openrocket.core.aerodynamics.lookup.MachAoALookup;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.models.gravity.GravityModelType;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.simulation.SimulationStepperMethod;
import info.openrocket.core.simulation.extension.SimulationExtension;
import info.openrocket.core.startup.Application;
import info.openrocket.core.startup.ExceptionHandler;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.GeodeticComputationStrategy;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.components.FieldSeparatorComboBox;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.FlatLafOutlines;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Milestone 4 run 2, part S4: OpenRocket's simulation options tab (swing gui/simulation/SimulationOptionsPanel.java,
 * a package-private class made by reflection), its extension list and the aerodynamic lookup dialog
 * (AerodynamicLookupDialog.java), under FlatLaf Light with SwingPreferences of a fresh store. The dialogs are
 * JDialogs, so the probe needs a display (xvfb-run: the file needs-display beside probe/); the lookup dialog is
 * made and packed, never shown; the extension's help dialog, the file chooser and the message dialogs are shown
 * modally and answered by a timer from inside their event loops. It prints:
 * - WALK: the options tab of the first simulation of the example "A simple model rocket" (data/examples) in the
 *   order of its containers, in the form of the M4 sim-config scout's dialog walk (BORDER for a titled panel, its
 *   children indented; LABEL (a UnitSelector is a label), SPINNER, SLIDER, CHECK, COMBO, FIELD, BUTTON, each with
 *   " disabled" and " tip=..." as they apply; hidden components, the description area and the scroll bars are
 *   left out); MODEL: every spinner's DoubleModel; BOX: the bounds of what the walk prints, after a layout at the
 *   preferred size;
 * - STEP: what a user does on the tab (combo box items selected, texts set on the spinner models, the seed field's
 *   text set, the seed check box and the default buttons clicked with doClick()), each followed by the state it
 *   concerns: the options' values, the combo boxes' tooltips, the spinners' texts, the seed field's text, enabled
 *   state, outline and tooltip, and the preferences; random seeds drawn by Java are never printed (only whether the
 *   field shows the options' seed); the lookup summary's tooltip (the private updateLookupSummary(), called by
 *   reflection, as the options' listener calls it later) for tables set on the options;
 * - EXT: the extension list of every simulation of the examples "Simulation extensions" and "Simulation
 *   scripting": each extension's name and buttons (Configure, Help, Delete by their icons), its walk, the help
 *   dialog (HELP: title, modality, text, buttons, the Escape key) and the list after deletions;
 * - the lookup dialog (LOOKUP): its title, modality, default button and Escape key, its walk (TABS and TAB for the
 *   tabbed pane), the summaries; then what a user does (separators chosen, the text edited, Clear, Refresh, OK by
 *   the private applyAndClose(), Load from file with the file chooser answered: its title, filters, directory and
 *   selected file), each followed by the sections' state (summary, its tooltip, the text area's title, text,
 *   editability and colour, the buttons' enabled states), the options' tables and the messages shown (MESSAGE:
 *   type, title and text); the static formatLookupSummary() for tables of the probe's own.
 * The stability table's three columns, which Java joins in an order that changes from run to run, are printed as
 * "{CN, CM, CP}". Paths below java.io.tmpdir are printed from "<tmp>", the home directory as "<home>" and the working directory (in which
 * a relative path is made absolute) as "<cwd>".
 */
public final class SimulationOptionsProbe {
	private static final String PANEL = "info.openrocket.swing.gui.simulation.SimulationOptionsPanel";
	private static final String LOOKUP = "info.openrocket.swing.gui.simulation.AerodynamicLookupDialog";

	/**
	 * The stability table's columns in any order: MachAoALookup keeps them in a Set.copyOf(), whose iteration order
	 * changes from one run of the JVM to the next (ImmutableCollections' SALT), and formatLookupSummary() joins
	 * them in that order; the probe prints them as "{CN, CM, CP}".
	 */
	private static final String STABILITY_COLUMNS = "CN, CM, CP|CN, CP, CM|CM, CN, CP|CM, CP, CN|CP, CN, CM|CP, CM, CN";

	private final PrintStream out;
	private final String tmp;
	private final String home;
	private final String cwd;

	private SimulationOptionsProbe(PrintStream out) throws Exception {
		this.out = out;
		this.tmp = new File(System.getProperty("java.io.tmpdir")).getCanonicalPath();
		this.home = new File(System.getProperty("user.home")).getCanonicalPath();
		this.cwd = new File(System.getProperty("user.dir")).getAbsolutePath();
	}

	// ------------------------------------------------------------------------------------- texts

	private String clean(String s) {
		if (s == null) {
			return "";
		}
		return s.replace(tmp, "<tmp>").replace(home, "<home>").replace(cwd, "<cwd>").replace("\n", "\\n")
				.replace("\t", "\\t").replaceAll(STABILITY_COLUMNS, "{CN, CM, CP}");
	}

	private String path(Object p) {
		return p == null ? "null" : clean(p.toString());
	}

	// -------------------------------------------------------------------------------- reflection

	private static Object field(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private static void setField(Object object, Class<?> type, String name, Object value) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		f.set(object, value);
	}

	private static Object call(Object object, Class<?> type, String name) throws Exception {
		Method m = type.getDeclaredMethod(name);
		m.setAccessible(true);
		try {
			return m.invoke(object);
		} catch (InvocationTargetException e) {
			throw (Exception) e.getCause();
		}
	}

	// ------------------------------------------------------------------------------------- the walk

	private String state(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	/** The line of @p c, or null for a container the walk enters. */
	private String describe(Component c) {
		if (c instanceof JSpinner sp) {
			return "SPINNER " + clean(((JSpinner.DefaultEditor) sp.getEditor()).getTextField().getText());
		}
		if (c instanceof JComboBox<?> cb) {
			StringBuilder items = new StringBuilder();
			for (int i = 0; i < cb.getItemCount(); i++) {
				items.append(i > 0 ? "|" : "").append(cb.getItemAt(i));
			}
			return "COMBO selected=" + clean(String.valueOf(cb.getSelectedItem())) + " items=" + clean(items.toString())
					+ (cb.isEditable() ? " editable" : "");
		}
		if (c instanceof JRadioButton rb) {
			return "RADIO " + rb.getText() + " selected=" + rb.isSelected();
		}
		if (c instanceof JCheckBox cb) {
			return "CHECK " + cb.getText() + " selected=" + cb.isSelected();
		}
		if (c instanceof AbstractButton b) {
			return "BUTTON " + clean(b.getText()) + (b.getIcon() != null ? " icon" : "");
		}
		if (c instanceof JSlider s) {
			return "SLIDER " + s.getMinimum() + ".." + s.getMaximum() + " at " + s.getValue();
		}
		if (c instanceof JSeparator) {
			return "SEPARATOR";
		}
		if (c instanceof JTextArea ta) {
			return "TEXTAREA " + clean(ta.getText());
		}
		if (c instanceof JTextField tf) {
			return "FIELD " + clean(tf.getText());
		}
		if (c instanceof JLabel l) {
			return "LABEL " + clean(l.getText()) + (l.getIcon() != null ? " icon" : "");
		}
		return null;
	}

	/** Whether the walk leaves @p c out: the scroll bars and the description areas' editor panes. */
	private static boolean skipped(Component c) {
		return c instanceof JScrollBar || c instanceof JEditorPane;
	}

	private void walk(Component c, String indent, List<Component> order) {
		if (!c.isVisible() || skipped(c)) {
			return;
		}
		String line = describe(c);
		if (line != null) {
			out.println("WALK " + indent + line + (line.equals("SEPARATOR") ? "" : state(c)));
			order.add(c);
			return;
		}
		if (c instanceof JTabbedPane tabs) {
			StringBuilder titles = new StringBuilder();
			for (int i = 0; i < tabs.getTabCount(); i++) {
				titles.append(" [").append(tabs.getTitleAt(i)).append(']');
			}
			out.println("WALK " + indent + "TABS" + titles + " selected=" + tabs.getSelectedIndex());
			for (int i = 0; i < tabs.getTabCount(); i++) {
				out.println("WALK " + indent + "TAB " + tabs.getTitleAt(i));
				if (tabs.getComponentAt(i) instanceof Container page) {
					for (Component child : page.getComponents()) {
						walk(child, indent + "  ", order);
					}
				}
			}
			return;
		}
		if (c instanceof Container k) {
			String inner = indent;
			if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
				out.println("WALK " + indent + "BORDER " + tb.getTitle());
				inner = indent + "  ";
			}
			for (Component child : k.getComponents()) {
				walk(child, inner, order);
			}
		}
	}

	private List<Component> walk(String title, Component panel) {
		out.println("== " + title);
		List<Component> order = new ArrayList<>();
		walk(panel, "", order);
		return order;
	}

	// -------------------------------------------------------------------------------- the models

	private static DoubleModel modelOf(JSpinner spinner) throws Exception {
		SpinnerModel model = spinner.getModel();
		return (DoubleModel) field(model, model.getClass(), "this$0");
	}

	private static String unitGroupName(UnitGroup group) throws Exception {
		for (Field f : UnitGroup.class.getDeclaredFields()) {
			if (Modifier.isStatic(f.getModifiers()) && UnitGroup.class.isAssignableFrom(f.getType())
					&& f.get(null) == group) {
				return f.getName();
			}
		}
		return "?";
	}

	private static String bound(DoubleModel model, String valueField, String modelField) throws Exception {
		DoubleModel boundModel = (DoubleModel) field(model, DoubleModel.class, modelField);
		if (boundModel != null) {
			return "model:" + field(boundModel, DoubleModel.class, "valueName");
		}
		double value = (Double) field(model, DoubleModel.class, valueField);
		return Double.toString(value);
	}

	private static String slider(JSlider slider) throws Exception {
		if (slider == null) {
			return "none";
		}
		Object model = slider.getModel();
		Class<?> type = model.getClass();
		StringBuilder sb = new StringBuilder((Boolean) field(model, type, "islinear") ? "linear" : "quadratic");
		for (String name : new String[] { "min", "mid", "max", "linearPosition" }) {
			Object value = field(model, type, name);
			if (value instanceof DoubleModel dm) {
				Object source = field(dm, DoubleModel.class, "source");
				sb.append(' ').append(name).append('=').append(source == null ? Double.toString(dm.getValue())
						: "model:" + field(dm, DoubleModel.class, "valueName"));
			} else {
				sb.append(' ').append(name).append('=').append(value);
			}
		}
		return sb.toString();
	}

	private void model(JSpinner spinner, JSlider slider) throws Exception {
		DoubleModel model = modelOf(spinner);
		out.println("MODEL " + field(model, DoubleModel.class, "valueName") + " source="
				+ field(model, DoubleModel.class, "source").getClass().getSimpleName() + " units="
				+ unitGroupName((UnitGroup) field(model, DoubleModel.class, "units")) + " min="
				+ bound(model, "minValue", "minModel") + " max=" + bound(model, "maxValue", "maxModel")
				+ " multiplier=" + field(model, DoubleModel.class, "multiplier") + " slider=" + slider(slider));
	}

	private void models(List<Component> order) throws Exception {
		for (int i = 0; i < order.size(); i++) {
			if (!(order.get(i) instanceof JSpinner spinner)) {
				continue;
			}
			JSlider found = null;
			for (int j = i + 1; j < order.size() && !(order.get(j) instanceof JSpinner); j++) {
				if (order.get(j) instanceof JSlider s) {
					found = s;
					break;
				}
			}
			model(spinner, found);
		}
	}

	// ---------------------------------------------------------------------------------- the boxes

	private static void layout(Component c) {
		if (c instanceof Container k) {
			k.doLayout();
			for (Component child : k.getComponents()) {
				layout(child);
			}
		}
	}

	private static Rectangle boundsIn(Component c, Component ancestor) {
		Rectangle r = new Rectangle(c.getBounds());
		for (Component p = c.getParent(); p != null && p != ancestor; p = p.getParent()) {
			r.translate(p.getX(), p.getY());
		}
		return r;
	}

	private void boxes(Container panel) {
		panel.setSize(panel.getPreferredSize());
		layout(panel);
		out.println("BOX panel w=" + panel.getWidth() + " h=" + panel.getHeight());
		boxes(panel, panel);
	}

	private void boxes(Container panel, Component c) {
		if (!c.isVisible() || skipped(c)) {
			return;
		}
		String name = null;
		if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
			name = "BORDER " + tb.getTitle();
		} else if (describe(c) != null) {
			String line = describe(c);
			name = line.length() > 47 ? line.substring(0, 47) : line;
		}
		if (name != null) {
			Rectangle r = boundsIn(c, panel);
			out.println("BOX " + name + " x=" + r.x + " y=" + r.y + " w=" + r.width + " h=" + r.height);
		}
		if (c instanceof Container k && !(c instanceof JSpinner) && !(c instanceof JComboBox)) {
			for (Component child : k.getComponents()) {
				boxes(panel, child);
			}
		}
	}

	// ---------------------------------------------------------------------------- modal answers

	/**
	 * Runs @p action, which shows modal dialogs one after the other, and answers them in turn with @p answers from
	 * a timer inside the dialogs' event loops.
	 */
	@SafeVarargs
	private void whileModal(String what, ThrowingRunnable action, Consumer<Window>... answers) throws Exception {
		int[] next = { 0 };
		java.util.Set<Window> answered = java.util.Collections.newSetFromMap(new java.util.IdentityHashMap<>());
		Timer timer = new Timer(50, null);
		timer.addActionListener(e -> {
			if (next[0] >= answers.length) {
				return;
			}
			for (Window w : Window.getWindows()) {
				if (!answered.contains(w) && w.isShowing() && w instanceof Dialog d && d.isModal()) {
					answered.add(w);
					answers[next[0]++].accept(w);
					return;
				}
			}
		});
		timer.start();
		try {
			action.run();
		} finally {
			timer.stop();
		}
		if (next[0] < answers.length) {
			out.println("  (" + what + ": " + next[0] + " of " + answers.length + " modal dialogs)");
		}
	}

	@FunctionalInterface
	private interface ThrowingRunnable {
		void run() throws Exception;
	}

	private static <T extends Component> T findIn(Component c, Class<T> type) {
		if (type.isInstance(c)) {
			return type.cast(c);
		}
		if (c instanceof Container k) {
			for (Component child : k.getComponents()) {
				T found = findIn(child, type);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	private static JButton buttonIn(Component c, String text) {
		if (c instanceof JButton b && text.equals(b.getText())) {
			return b;
		}
		if (c instanceof Container k) {
			for (Component child : k.getComponents()) {
				JButton found = buttonIn(child, text);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	/** Prints a JOptionPane's message dialog and closes it. */
	private void message(Window w) {
		JOptionPane pane = findIn(w, JOptionPane.class);
		String type = switch (pane.getMessageType()) {
			case JOptionPane.ERROR_MESSAGE -> "ERROR";
			case JOptionPane.WARNING_MESSAGE -> "WARNING";
			case JOptionPane.INFORMATION_MESSAGE -> "INFORMATION";
			default -> "OTHER";
		};
		out.println("  MESSAGE " + type + " title=[" + clean(((Dialog) w).getTitle()) + "] text=["
				+ clean(String.valueOf(pane.getMessage())) + "]");
		w.dispose();
	}

	// ------------------------------------------------------------------------------- the options tab

	private static JPanel newPanel(OpenRocketDocument document, Simulation simulation) throws Exception {
		Constructor<?> constructor = Class.forName(PANEL).getDeclaredConstructor(OpenRocketDocument.class,
				Simulation.class);
		constructor.setAccessible(true);
		return (JPanel) constructor.newInstance(document, simulation);
	}

	private static String text(JSpinner spinner) {
		return ((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText();
	}

	private String spinner(String name, JSpinner spinner, JSlider slider) {
		return name + "=[" + text(spinner) + "]" + (slider != null ? " at " + slider.getValue() : "")
				+ (spinner.isVisible() ? "" : " hidden") + (spinner.isEnabled() ? "" : " disabled");
	}

	private String combo(String name, JComboBox<?> combo) {
		return name + "=[" + combo.getSelectedItem() + "] tip=" + clean(combo.getToolTipText());
	}

	private String seed(Object panel, SimulationOptions options) throws Exception {
		Class<?> type = Class.forName(PANEL);
		JCheckBox check = (JCheckBox) field(panel, type, "fixedRandomSeedCheckBox");
		JTextField seedField = (JTextField) field(panel, type, "randomSeedField");
		FlatLafOutlines.Outline outline = FlatLafOutlines.getOutline(seedField);
		String shown = seedField.getText();
		String seedText;
		if (shown.isEmpty()) {
			seedText = "[]";
		} else {
			// A seed Java drew is never printed: only whether the field shows the options' seed.
			seedText = shown.equals(Integer.toString(options.getRandomSeed())) ? "[the options' seed]" : "[" + shown + "]";
		}
		return "check=" + check.isSelected() + " field=" + seedText + (seedField.isEnabled() ? "" : " disabled")
				+ (outline != FlatLafOutlines.Outline.NONE ? " outline=" + outline : "")
				+ " tip=" + clean(seedField.getToolTipText()) + " | options fixed=" + options.isRandomSeedFixed();
	}

	private String seedValue(SimulationOptions options, int expected) {
		return " seed==" + expected + ":" + (options.getRandomSeed() == expected);
	}

	private void typeSpinner(JSpinner spinner, String typed) {
		try {
			spinner.getModel().setValue(typed);
		} catch (RuntimeException e) {
			out.println("  EXCEPTION " + e.getClass().getName() + ": " + clean(e.getMessage()));
		}
	}

	private static MachAoALookup table(List<String> rows, List<String> columns) {
		return CsvMachAoALookup.parse(rows, columns, ',');
	}

	private void optionsTab(File repo) throws Exception {
		Class<?> type = Class.forName(PANEL);
		OpenRocketDocument document = new GeneralRocketLoader(new File(repo, "data/examples/A simple model rocket.ork"))
				.load();
		Simulation simulation = document.getSimulation(0);
		SimulationOptions options = simulation.getOptions();
		ApplicationPreferences prefs = Application.getPreferences();
		JPanel panel = newPanel(document, simulation);

		List<Component> order = walk("tab: A simple model rocket.ork, " + simulation.getName(), panel);
		models(order);
		boxes(panel);

		List<JComboBox<?>> combos = new ArrayList<>();
		List<JSpinner> spinners = new ArrayList<>();
		List<JSlider> sliders = new ArrayList<>();
		for (Component c : order) {
			if (c instanceof JComboBox<?> cb) {
				combos.add(cb);
			} else if (c instanceof JSpinner s) {
				spinners.add(s);
			} else if (c instanceof JSlider s) {
				sliders.add(s);
			}
		}
		@SuppressWarnings("unchecked")
		JComboBox<Object> stepper = (JComboBox<Object>) combos.get(0);
		@SuppressWarnings("unchecked")
		JComboBox<Object> geodetic = (JComboBox<Object>) combos.get(1);
		@SuppressWarnings("unchecked")
		JComboBox<Object> gravity = (JComboBox<Object>) combos.get(2);
		JSpinner timeStep = spinners.get(0);
		JSlider timeStepSlider = sliders.get(0);
		JSpinner maxTime = spinners.get(1);
		JSpinner gravitySpinner = (JSpinner) field(panel, type, "gravitySpinner");
		JSlider gravitySlider = (JSlider) field(panel, type, "gravitySlider");
		JLabel gravityLabel = (JLabel) field(panel, type, "gravityLabel");
		JLabel summaryIcon = (JLabel) field(panel, type, "aerodynamicLookupSummaryIconLabel");

		out.println("== the stepper");
		for (SimulationStepperMethod method : new SimulationStepperMethod[] { SimulationStepperMethod.RK6,
				SimulationStepperMethod.RK4 }) {
			stepper.setSelectedItem(method);
			out.println("STEP stepper " + method.name() + " | " + combo("stepper", stepper) + " | options="
					+ options.getSimulationStepperMethodChoice().name() + " preference="
					+ prefs.getString(ApplicationPreferences.SIMULATION_STEPPER_METHOD, "(none)"));
		}

		out.println("== the geodetic computation");
		for (GeodeticComputationStrategy gcs : new GeodeticComputationStrategy[] { GeodeticComputationStrategy.SPHERICAL,
				GeodeticComputationStrategy.WGS84, GeodeticComputationStrategy.FLAT }) {
			geodetic.setSelectedItem(gcs);
			out.println("STEP geodetic " + gcs.name() + " | " + combo("geodetic", geodetic) + " | options="
					+ options.getGeodeticComputation().name());
		}

		out.println("== the gravity model");
		gravity.setSelectedItem(GravityModelType.CONSTANT);
		out.println("STEP gravity CONSTANT | " + combo("gravity", gravity) + " | options="
				+ options.getGravityModelType().name() + " | label visible=" + gravityLabel.isVisible() + " "
				+ spinner("gravity", gravitySpinner, gravitySlider));
		model(gravitySpinner, gravitySlider);
		walk("tab with the constant gravity", panel);
		for (String typed : new String[] { "-1", "0", "3.72", "30", "150" }) {
			typeSpinner(gravitySpinner, typed);
			out.println("STEP gravity typed=[" + typed + "] | " + spinner("gravity", gravitySpinner, gravitySlider)
					+ " | options=" + options.getConstantGravity());
		}
		gravity.setSelectedItem(GravityModelType.WGS);
		out.println("STEP gravity WGS | " + combo("gravity", gravity) + " | options="
				+ options.getGravityModelType().name() + " | label visible=" + gravityLabel.isVisible() + " "
				+ spinner("gravity", gravitySpinner, gravitySlider));

		out.println("== the time step and the maximum time");
		for (String typed : new String[] { "0.001", "0.01", "0.02", "0.2", "5", "0.05" }) {
			typeSpinner(timeStep, typed);
			out.println("STEP time step typed=[" + typed + "] | " + spinner("time step", timeStep, timeStepSlider)
					+ " | options=" + options.getTimeStep());
		}
		for (String typed : new String[] { "0.5", "1", "5000000", "1200" }) {
			typeSpinner(maxTime, typed);
			out.println("STEP maximum time typed=[" + typed + "] | " + spinner("maximum time", maxTime, null)
					+ " | options=" + options.getMaxSimulationTime());
		}

		out.println("== the lookup summary");
		List<String> dragRows = List.of("Mach,Cd", "0,0.5", "0.3333333,0.55", "2.5,0.6");
		options.setDragLookup(Path.of("/data/aero/drag & <lift>.csv"), table(dragRows, List.of("cd")), dragRows);
		call(panel, type, "updateLookupSummary");
		out.println("STEP drag table | tip=" + clean(summaryIcon.getToolTipText()));
		List<String> stabilityRows = List.of("Mach,AoA,Cn,Cm,Cp", "0.1,-0.0004,1,1,1", "1.25,10,2,2,2",
				"1234567.891,10,3,3,3", "0.1,10,1,1,1", "1.25,-0.0004,2,2,2", "1234567.891,-0.0004,3,3,3");
		options.setStabilityLookup(Path.of("stability.csv"), table(stabilityRows, List.of("cn", "cm", "cp")),
				stabilityRows);
		call(panel, type, "updateLookupSummary");
		out.println("STEP stability table | tip=" + clean(summaryIcon.getToolTipText()));
		options.clearDragLookup();
		options.clearStabilityLookup();
		call(panel, type, "updateLookupSummary");
		out.println("STEP no tables | tip=" + clean(summaryIcon.getToolTipText()));

		out.println("== the random seed");
		JCheckBox check = (JCheckBox) field(panel, type, "fixedRandomSeedCheckBox");
		JTextField seedField = (JTextField) field(panel, type, "randomSeedField");
		out.println("STEP initial | " + seed(panel, options));
		check.doClick();
		out.println("STEP checked | " + seed(panel, options));
		for (String typed : new String[] { "12345", " -2147483648 ", "2147483648", "abc", "" }) {
			seedField.setText(typed);
			out.println("STEP typed=[" + typed + "] | " + seed(panel, options) + seedValue(options, 12345)
					+ seedValue(options, Integer.MIN_VALUE));
		}
		seedField.setText("abc");
		call(panel, type, "replaceInvalidRandomSeed");
		out.println("STEP focus lost with [abc] | " + seed(panel, options));
		for (String typed : new String[] { "+77", "\u0661\u0662\u0663", "0x10", "1.0", "-0" }) {
			seedField.setText(typed);
			out.println("STEP typed=[" + typed + "] | " + seed(panel, options) + seedValue(options, 77)
					+ seedValue(options, 123) + seedValue(options, 0));
		}
		seedField.setText("4711");
		call(panel, type, "replaceInvalidRandomSeed");
		out.println("STEP focus lost with [4711] | " + seed(panel, options) + seedValue(options, 4711));
		seedField.setText("  ");
		call(panel, type, "prepareForSimulation");
		out.println("STEP prepareForSimulation with [  ] | " + seed(panel, options));
		check.doClick();
		seedField.setText(" 99 ");
		call(panel, type, "prepareForSimulation");
		out.println("STEP prepareForSimulation with [ 99 ] | " + seed(panel, options) + seedValue(options, 99));
		check.doClick();
		out.println("STEP unchecked | " + seed(panel, options) + seedValue(options, 99));

		out.println("== reset to default");
		JButton reset = buttonIn(panel, "Reset to default");
		JButton save = buttonIn(panel, "Save as default");
		stepper.setSelectedItem(SimulationStepperMethod.RK6);
		gravity.setSelectedItem(GravityModelType.CONSTANT);
		typeSpinner(gravitySpinner, "5");
		typeSpinner(timeStep, "0.04");
		typeSpinner(maxTime, "300");
		reset.doClick();
		out.println("STEP reset, fresh preferences | time step=" + options.getTimeStep() + " max time="
				+ options.getMaxSimulationTime() + " geodetic=" + options.getGeodeticComputation().name() + " stepper="
				+ options.getSimulationStepperMethodChoice().name() + " gravity=" + options.getGravityModelType().name()
				+ " " + options.getConstantGravity() + " | " + spinner("time step", timeStep, timeStepSlider) + " "
				+ spinner("maximum time", maxTime, null) + " | " + combo("geodetic", geodetic) + " | "
				+ seed(panel, options));
		prefs.setTimeStep(0.02);
		prefs.setMaxSimulationTime(600);
		prefs.setGeodeticComputation(GeodeticComputationStrategy.WGS84);
		prefs.setRandomSeed(777);
		prefs.setRandomSeedFixed(true);
		reset.doClick();
		out.println("STEP reset, stored preferences | time step=" + options.getTimeStep() + " max time="
				+ options.getMaxSimulationTime() + " geodetic=" + options.getGeodeticComputation().name() + " | "
				+ spinner("time step", timeStep, timeStepSlider) + " " + spinner("maximum time", maxTime, null) + " | "
				+ combo("geodetic", geodetic) + " | " + seed(panel, options) + seedValue(options, 777));
		prefs.setRandomSeedFixed(false);
		reset.doClick();
		out.println("STEP reset, preferences without a fixed seed | " + seed(panel, options));

		out.println("== save as default");
		typeSpinner(timeStep, "0.03");
		typeSpinner(maxTime, "900");
		geodetic.setSelectedItem(GeodeticComputationStrategy.FLAT);
		check.doClick();
		seedField.setText("4242");
		save.doClick();
		out.println("STEP save with [4242] | preferences time step=" + prefs.getTimeStep() + " max time="
				+ prefs.getMaxSimulationTime() + " geodetic=" + prefs.getGeodeticComputation().name() + " fixed="
				+ prefs.isRandomSeedFixed() + " seed=" + prefs.getRandomSeed() + " | " + seed(panel, options));
		check.doClick();
		check.doClick();
		seedField.setText("x");
		save.doClick();
		out.println("STEP save with [x] | preferences fixed=" + prefs.isRandomSeedFixed() + " seed="
				+ prefs.getRandomSeed() + " | " + seed(panel, options));
		gravity.setSelectedItem(GravityModelType.WGS);
	}

	// ---------------------------------------------------------------------------------- extensions

	private static String iconName(Icon icon) {
		if (icon == Icons.CONFIGURE) {
			return "CONFIGURE";
		}
		if (icon == Icons.HELP) {
			return "HELP";
		}
		if (icon == Icons.EDIT_DELETE) {
			return "DELETE";
		}
		return icon == null ? "none" : "other";
	}

	private void extensionPanels(JPanel current) {
		for (Component c : current.getComponents()) {
			if (!(c instanceof JPanel p)) {
				out.println("EXT " + describe(c));
				continue;
			}
			StringBuilder sb = new StringBuilder("EXT panel");
			for (Component child : p.getComponents()) {
				if (child instanceof JLabel l && !(child instanceof AbstractButton)) {
					sb.append(" name=[").append(clean(l.getText())).append(']');
				} else if (child instanceof JButton b) {
					sb.append(" button=").append(iconName(b.getIcon()))
							.append(b.getToolTipText() != null ? "(tip " + clean(b.getToolTipText()) + ")" : "");
				}
			}
			out.println(sb);
		}
	}

	private void help(JButton button) throws Exception {
		whileModal("help", button::doClick, w -> {
			JDialog d = (JDialog) w;
			JEditorPane pane = findIn(d, JEditorPane.class);
			String plain;
			try {
				plain = pane.getDocument().getText(0, pane.getDocument().getLength());
			} catch (Exception e) {
				plain = e.toString();
			}
			JRootPane root = d.getRootPane();
			Object escape = root.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
					.get(KeyStroke.getKeyStroke("ESCAPE"));
			JButton close = buttonIn(d.getContentPane(), "Close");
			out.println("  HELP title=[" + clean(d.getTitle()) + "] modality=" + d.getModalityType() + " text=["
					+ clean(plain.strip()) + "] close=" + (close != null) + " default="
					+ (root.getDefaultButton() != null ? root.getDefaultButton().getText() : "none") + " escape="
					+ (escape != null));
			close.doClick();
			out.println("  HELP closed: visible=" + d.isVisible());
		});
	}

	private void extensions(File repo, String example) throws Exception {
		Class<?> type = Class.forName(PANEL);
		OpenRocketDocument document = new GeneralRocketLoader(new File(repo, "data/examples/" + example + ".ork"))
				.load();
		for (Simulation simulation : document.getSimulations()) {
			JPanel panel = newPanel(document, simulation);
			JPanel current = (JPanel) field(panel, type, "currentExtensions");
			walk("extensions: " + example + ", " + simulation.getName(), current);
			for (SimulationExtension extension : simulation.getSimulationExtensions()) {
				out.println("EXT extension name=[" + clean(extension.getName()) + "] id=" + extension.getId()
						+ " description=[" + clean(extension.getDescription()) + "]");
			}
			extensionPanels(current);
		}
		// The help dialog and the deletions, on the last simulation.
		Simulation last = document.getSimulation(document.getSimulationCount() - 1);
		JPanel panel = newPanel(document, last);
		JPanel current = (JPanel) field(panel, type, "currentExtensions");
		for (Component c : current.getComponents()) {
			if (c instanceof JPanel p) {
				for (Component child : p.getComponents()) {
					if (child instanceof JButton b && b.getIcon() == Icons.HELP) {
						help(b);
					}
				}
			}
		}
		while (!last.getSimulationExtensions().isEmpty()) {
			JPanel first = (JPanel) current.getComponent(0);
			for (Component child : first.getComponents()) {
				if (child instanceof JButton b && b.getIcon() == Icons.EDIT_DELETE) {
					b.doClick();
					break;
				}
			}
			StringBuilder names = new StringBuilder();
			for (SimulationExtension extension : last.getSimulationExtensions()) {
				names.append(" [").append(extension.getName()).append(']');
			}
			out.println("STEP deleted the first | extensions" + names);
			walk("extensions after the deletion", current);
		}
	}

	// --------------------------------------------------------------------------------- the lookup

	private Object newLookup(SimulationOptions options) throws Exception {
		Constructor<?> constructor = Class.forName(LOOKUP).getDeclaredConstructor(Window.class,
				SimulationOptions.class);
		constructor.setAccessible(true);
		return constructor.newInstance(null, options);
	}

	private void section(Object dialog, boolean drag) throws Exception {
		Class<?> type = Class.forName(LOOKUP);
		String p = drag ? "drag" : "stability";
		JLabel summary = (JLabel) field(dialog, type, p + "SummaryLabel");
		JButton clear = (JButton) field(dialog, type, drag ? "clearDragButton" : "clearStabilityButton");
		JButton refresh = (JButton) field(dialog, type, p + "RefreshButton");
		FieldSeparatorComboBox combo = (FieldSeparatorComboBox) field(dialog, type, p + "SeparatorCombo");
		JTextArea area = (JTextArea) field(dialog, type, p + "ExampleArea");
		TitledBorder border = (TitledBorder) field(dialog, type, p + "ExampleBorder");
		boolean modified = (Boolean) field(dialog, type, p + "Modified");
		Object csv = field(dialog, type, drag ? "dragCsv" : "stabilityCsv");
		Object table = field(dialog, type, p + "Table");
		java.awt.Color fg = area.getForeground();
		out.println("  SECTION " + p + " summary=[" + clean(summary.getText()) + "] tip=" + clean(summary.getToolTipText())
				+ " | refresh" + (refresh.isEnabled() ? "" : " disabled") + " clear" + (clear.isEnabled() ? "" : " disabled")
				+ " | separator=[" + clean(String.valueOf(combo.getSelectedItem())) + "] | " + border.getTitle()
				+ " text=[" + clean(area.getText()) + "]" + (area.isEditable() ? " editable" : "") + " colour="
				+ fg.getRed() + "," + fg.getGreen() + "," + fg.getBlue() + " | modified=" + modified + " csv=" + path(csv)
				+ " table=" + (table != null));
	}

	private void options(SimulationOptions options) {
		MachAoALookup drag = options.getDragLookupTable();
		MachAoALookup stability = options.getStabilityLookupTable();
		out.println("  OPTIONS drag=" + path(options.getDragLookupCsvPath()) + " "
				+ (drag == null ? "none" : "cd(0,0)=" + drag.interpolate(0, 0, "cd") + " maxMach=" + drag.getMaxMach())
				+ " rows=" + clean(String.valueOf(options.getDragLookupCsvRows())) + " | stability="
				+ path(options.getStabilityLookupCsvPath()) + " "
				+ (stability == null ? "none" : "cp(0,0)=" + stability.interpolate(0, 0, "cp")) + " rows="
				+ clean(String.valueOf(options.getStabilityLookupCsvRows())));
	}

	private void lookupState(String step, Object dialog, SimulationOptions options) throws Exception {
		out.println("STEP " + step);
		section(dialog, true);
		section(dialog, false);
		options(options);
	}

	private void apply(String step, Object dialog, SimulationOptions options) throws Exception {
		Class<?> type = Class.forName(LOOKUP);
		whileModal("apply", () -> call(dialog, type, "applyAndClose"), this::message);
		out.println("STEP " + step + ": displayable=" + ((Window) dialog).isDisplayable());
		section(dialog, true);
		section(dialog, false);
		options(options);
	}

	/** The JUnit tests' options: both tables from rows, with a path to a file that does not exist. */
	private static SimulationOptions junitOptions() {
		SimulationOptions options = new SimulationOptions();
		List<String> drag = List.of("Mach,Cd", "0,1");
		List<String> stability = List.of("Mach,Cn,Cm,Cp", "0,1,1,1");
		Path csv = Path.of("missing-beta-test-lookup.csv");
		options.setDragLookup(csv, CsvMachAoALookup.parse(drag, List.of("cd"), ','), drag);
		options.setStabilityLookup(csv, CsvMachAoALookup.parse(stability, List.of("cn", "cm", "cp"), ','),
				stability);
		return options;
	}

	private void chooser(Window w, File answer) {
		JFileChooser chooser = findIn(w, JFileChooser.class);
		StringBuilder filters = new StringBuilder();
		for (FileFilter filter : chooser.getChoosableFileFilters()) {
			filters.append(" [").append(filter.getDescription());
			if (filter instanceof FileNameExtensionFilter f) {
				filters.append(" ").append(String.join(",", f.getExtensions()));
			}
			filters.append(']');
		}
		out.println("  CHOOSER title=[" + chooser.getDialogTitle() + "] filters" + filters + " selected filter=["
				+ chooser.getFileFilter().getDescription() + "] directory=" + path(chooser.getCurrentDirectory())
				+ " selected file=" + path(chooser.getSelectedFile()) + " mode=" + chooser.getFileSelectionMode()
				+ " multi=" + chooser.isMultiSelectionEnabled());
		if (answer == null) {
			chooser.cancelSelection();
		} else {
			chooser.setSelectedFile(answer);
			chooser.approveSelection();
		}
	}

	private void summaries() {
		Translator trans = Application.getTranslator();
		try {
			Method format = Class.forName(LOOKUP).getDeclaredMethod("formatLookupSummary", Translator.class,
					MachAoALookup.class);
			format.setAccessible(true);
			Object[][] cases = {
					{ "no AoA", List.of("Mach,Cd", "0.3,0.35", "2,0.5") },
					{ "with AoA", List.of("Mach,AoA,Cd", "0.30,0,0.35", "0.30,5,0.42") },
					{ "negative zero", List.of("Mach,AoA,Cd", "0,-0.0004,0.3", "0.99999,-0.0004,0.3", "0,15.12345,1",
							"0.99999,15.12345,1") },
					{ "angleofattack and extra columns", List.of("mach, angleofattack, cd, cl", "0.5,2,0.3,0.1",
							"1.5,2,0.4,0.2", "0.5,4,0.3,0.1", "1.5,4,0.4,0.2") },
					{ "large", List.of("Mach,Cd", "0.0005,1", "12345.6785,2") },
			};
			for (Object[] c : cases) {
				@SuppressWarnings("unchecked")
				List<String> rows = (List<String>) c[1];
				MachAoALookup table = CsvMachAoALookup.parse(rows, List.of("cd"), ',');
				out.println("FORMAT " + c[0] + " [" + clean((String) format.invoke(null, trans, table)) + "]");
			}
			List<String> stability = List.of("Mach,AoA,Cn,Cm,Cp", "0.30,0,0.12,-0.01,1.45", "0.30,5,0.15,-0.03,1.40");
			MachAoALookup table = CsvMachAoALookup.parse(stability, List.of("cn", "cm", "cp"), ',');
			out.println("FORMAT stability [" + clean((String) format.invoke(null, trans, table)) + "]");
		} catch (Exception e) {
			out.println("FORMAT failed " + e);
		}
	}

	private void lookup() throws Exception {
		Class<?> type = Class.forName(LOOKUP);
		ApplicationPreferences prefs = Application.getPreferences();
		summaries();

		SimulationOptions options = new SimulationOptions();
		Object dialog = newLookup(options);
		JDialog d = (JDialog) dialog;
		JRootPane root = d.getRootPane();
		out.println("LOOKUP title=[" + d.getTitle() + "] modality=" + d.getModalityType() + " default="
				+ (root.getDefaultButton() != null ? root.getDefaultButton().getText() : "none") + " escape="
				+ (root.getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW).get(KeyStroke.getKeyStroke("ESCAPE")) != null)
				+ " close operation=" + d.getDefaultCloseOperation() + " resizable=" + d.isResizable());
		walk("lookup dialog without tables", d.getContentPane());
		lookupState("without tables", dialog, options);

		out.println("== the separator");
		FieldSeparatorComboBox dragCombo = (FieldSeparatorComboBox) field(dialog, type, "dragSeparatorCombo");
		FieldSeparatorComboBox stabilityCombo = (FieldSeparatorComboBox) field(dialog, type, "stabilitySeparatorCombo");
		for (String separator : new String[] { ";", "SPACE", "TAB", "|", "" }) {
			dragCombo.setSelectedItem(separator);
			out.println("STEP drag separator [" + clean(separator) + "] char=[" + clean(String.valueOf(dragCombo.getSeparatorChar()))
					+ "]");
			section(dialog, true);
		}
		dragCombo.setSelectedItem(",");
		stabilityCombo.setSelectedItem(";");
		lookupState("stability separator [;]", dialog, options);
		apply("OK without tables", dialog, options);
		d.dispose();

		out.println("== the JUnit tests' tables");
		options = junitOptions();
		dialog = newLookup(options);
		d = (JDialog) dialog;
		walk("lookup dialog with the JUnit tests' tables", d.getContentPane());
		lookupState("with stored rows", dialog, options);
		JTextArea dragArea = (JTextArea) field(dialog, type, "dragExampleArea");
		JTextArea stabilityArea = (JTextArea) field(dialog, type, "stabilityExampleArea");
		dragArea.setText("Mach,Cd\n0,2");
		lookupState("drag text set", dialog, options);
		dragCombo = (FieldSeparatorComboBox) field(dialog, type, "dragSeparatorCombo");
		dragCombo.setSelectedItem(";");
		lookupState("drag separator [;] after the edit", dialog, options);
		dragCombo.setSelectedItem(",");
		dragArea.setText("Mach,Cd\n0,2");
		stabilityArea.setText("invalid");
		apply("OK with an invalid stability text", dialog, options);
		stabilityArea.setText("Mach,Cn,Cm,Cp\n0,1,1,3");
		apply("OK again", dialog, options);
		d.dispose();

		options = junitOptions();
		dialog = newLookup(options);
		d = (JDialog) dialog;
		dragArea = (JTextArea) field(dialog, type, "dragExampleArea");
		dragArea.setText("  # a comment  \n\nMach , Cd\n 0, 3 \n\n");
		apply("OK with comments, blank lines and spaces", dialog, options);
		d.dispose();

		options = junitOptions();
		dialog = newLookup(options);
		d = (JDialog) dialog;
		dragArea = (JTextArea) field(dialog, type, "dragExampleArea");
		dragArea.setText("Mach,Cl\n0,2");
		apply("OK with a drag text without Cd", dialog, options);
		dragArea.setText("Mach,Cd");
		apply("OK with a drag text without rows", dialog, options);
		dragArea.setText("Mach,Cd\n0,x");
		apply("OK with a drag text that is no number", dialog, options);
		dragArea.setText("Mach;Cd\n0;4");
		apply("OK with semicolons and the separator [,]", dialog, options);
		d.dispose();

		out.println("== Clear and Refresh");
		options = junitOptions();
		dialog = newLookup(options);
		d = (JDialog) dialog;
		((JButton) field(dialog, type, "clearDragButton")).doClick();
		lookupState("drag cleared", dialog, options);
		JButton stabilityRefresh = (JButton) field(dialog, type, "stabilityRefreshButton");
		whileModal("refresh", stabilityRefresh::doClick, this::message);
		lookupState("stability refreshed (the file does not exist)", dialog, options);
		apply("OK after Clear", dialog, options);
		d.dispose();

		Path dir = Path.of(System.getProperty("java.io.tmpdir"), "lookup");
		Files.createDirectories(dir);
		Path dragFile = dir.resolve("drag.csv");
		Files.write(dragFile, List.of("# drag of the probe", "Mach,AoA,Cd", "0.1,0,0.4", "0.1,4,0.45", "0.9,0,0.5",
				"0.9,4,0.55"), StandardCharsets.UTF_8);
		Path semicolons = dir.resolve("semicolons.txt");
		Files.write(semicolons, List.of("Mach;Cd", "0.2;0.3", "1.2;0.6"), StandardCharsets.UTF_8);
		Path noCd = dir.resolve("no-cd.csv");
		Files.write(noCd, List.of("Mach,Cl", "0.2,0.3"), StandardCharsets.UTF_8);
		Path stabilityFile = dir.resolve("stability.dat");
		Files.write(stabilityFile, List.of("Mach,Cn,Cm,Cp", "0.2,1,2,0.5", "1.4,1.5,2.5,0.6"), StandardCharsets.UTF_8);

		out.println("== Load from file");
		options = new SimulationOptions();
		Object loading = newLookup(options);
		d = (JDialog) loading;
		whileModal("load", () -> call(loading, type, "chooseDragLookup"), w -> chooser(w, dragFile.toFile()));
		lookupState("drag loaded from drag.csv", loading, options);
		out.println("  default directory=" + path(prefs.getDefaultDirectory()));
		whileModal("load again", () -> call(loading, type, "chooseDragLookup"), w -> chooser(w, null));
		lookupState("drag load cancelled", loading, options);
		FieldSeparatorComboBox loadingDragCombo = (FieldSeparatorComboBox) field(loading, type, "dragSeparatorCombo");
		loadingDragCombo.setSelectedItem(";");
		whileModal("load semicolons", () -> call(loading, type, "chooseDragLookup"),
				w -> chooser(w, semicolons.toFile()));
		lookupState("drag loaded from semicolons.txt with [;]", loading, options);
		loadingDragCombo.setSelectedItem(",");
		whileModal("load no cd", () -> call(loading, type, "chooseDragLookup"), w -> chooser(w, noCd.toFile()),
				this::message);
		lookupState("drag load of no-cd.csv (its error above, if any)", loading, options);
		whileModal("load stability", () -> call(loading, type, "chooseStabilityLookup"),
				w -> chooser(w, stabilityFile.toFile()));
		lookupState("stability loaded from stability.dat", loading, options);
		JTextArea loadingDragArea = (JTextArea) field(loading, type, "dragExampleArea");
		loadingDragArea.append("\n# edited");
		lookupState("drag text appended", loading, options);
		Files.write(dragFile, List.of("Mach,Cd", "0,0.7", "3,0.9"), StandardCharsets.UTF_8);
		((JButton) field(loading, type, "dragRefreshButton")).doClick();
		lookupState("drag refreshed after the file changed", loading, options);
		Files.delete(stabilityFile);
		whileModal("refresh a deleted file", () -> ((JButton) field(loading, type, "stabilityRefreshButton")).doClick(),
				this::message);
		lookupState("stability refreshed after its file was deleted", loading, options);
		apply("OK after the loads", loading, options);
		d.dispose();

		out.println("== a stored table whose file is missing and has no rows");
		options = new SimulationOptions();
		options.setDragLookup(dir.resolve("gone.csv"), CsvMachAoALookup.parse(List.of("Mach,Cd", "0,1"), List.of("cd"), ','),
				null);
		dialog = newLookup(options);
		lookupState("drag table without rows, file missing", dialog, options);
		((JDialog) dialog).dispose();

		out.println("== the error of a failed load");
		options = new SimulationOptions();
		Object failing = newLookup(options);
		whileModal("load no cd", () -> call(failing, type, "chooseDragLookup"), w -> chooser(w, noCd.toFile()),
				this::message);
		lookupState("after the chooser and the message", failing, options);
		((JDialog) failing).dispose();
	}

	private void failedLoadMessage() throws Exception {
		Class<?> type = Class.forName(LOOKUP);
		Path dir = Path.of(System.getProperty("java.io.tmpdir"), "lookup");
		Path noCd = dir.resolve("no-cd.csv");
		SimulationOptions options = new SimulationOptions();
		Object dialog = newLookup(options);
		setField(dialog, type, "dragCsv", noCd);
		setField(dialog, type, "dragTable", CsvMachAoALookup.parse(List.of("Mach,Cd", "0,1"), List.of("cd"), ','));
		whileModal("reload no cd", () -> call(dialog, type, "reloadDragLookup"), this::message);
		setField(dialog, type, "dragCsv", dir.resolve("nothing-here.csv"));
		whileModal("reload a missing file", () -> call(dialog, type, "reloadDragLookup"), this::message);
		setField(dialog, type, "dragCsv", dir);
		whileModal("reload a directory", () -> call(dialog, type, "reloadDragLookup"), this::message);
		((JDialog) dialog).dispose();
	}

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		File repo = new File(System.getProperty("qtrocket.repo"));
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		List<ThrustCurveMotor> read = ThrustCurveMotorSQLiteDatabase
				.readDatabase(new File(repo, "data/motors/initial_motors.db"));
		for (ThrustCurveMotor motor : read) {
			motors.addMotor(motor);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		Application.setExceptionHandler(new ExceptionHandler() {
			@Override
			public void handleErrorCondition(String message) {
				out.println("HANDLER " + message);
			}

			@Override
			public void handleErrorCondition(String message, Throwable exception) {
				out.println("HANDLER " + message + " (" + exception.getClass().getName() + ")");
			}

			@Override
			public void handleErrorCondition(Throwable exception) {
				out.println("HANDLER " + exception.getClass().getName());
			}

			@Override
			public void uncaughtException(Thread thread, Throwable throwable) {
				out.println("HANDLER uncaught " + throwable.getClass().getName());
			}
		});
		UITheme.Themes.LIGHT.applyTheme();
		SimulationOptionsProbe probe = new SimulationOptionsProbe(out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				probe.optionsTab(repo);
				probe.extensions(repo, "Simulation extensions");
				probe.extensions(repo, "Simulation scripting");
				probe.lookup();
				probe.failedLoadMessage();
			} catch (Exception e) {
				failure[0] = e;
			}
		});
		out.flush();
		if (failure[0] != null) {
			failure[0].printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}
}
