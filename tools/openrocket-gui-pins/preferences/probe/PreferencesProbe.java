package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.Scopes;
import com.google.inject.util.Modules;
import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.models.gravity.GravityModelType;
import info.openrocket.core.util.GeodeticComputationStrategy;
import info.openrocket.swing.gui.components.CollapsiblePanel;
import info.openrocket.swing.gui.dialogs.preferences.DesignPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.GeneralPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.LaunchPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.PreferencesDialog;
import info.openrocket.swing.gui.dialogs.preferences.SimulationPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.UnitsPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.WarningsPreferencesPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.FlatLafOutlines;
import info.openrocket.swing.gui.util.SwingPreferences;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JEditorPane;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JScrollBar;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.border.TitledBorder;
import javax.swing.text.JTextComponent;
import java.awt.Component;
import java.awt.Container;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.prefs.Preferences;

/**
 * What QtRocket's part P2 (the preferences dialog) must reproduce, on OpenRocket's own pages,
 * headless (SwingPreferences, FlatLaf Light), each line one fact:
 * <ul>
 * <li>PAGE name / ITEM ...: the components of a page in add order (depth first), for a store that
 * was just cleared (clearPreferences(): the units node only).</li>
 * <li>STORE ...: the keys of the store after a step (sorted, with the units node and the other
 * nodes), and VALUE key=value lines.</li>
 * <li>Steps on the pages: the choice combos, the default Mach spinner, the check boxes, the unit
 * combos and the metric/imperial buttons.</li>
 * <li>MERGE: java.util.prefs' importPreferences() (the dialog's Cancel) over a store changed after
 * the export.</li>
 * <li>Milestone 4 (run 2, part S5): the Simulation, Warnings and Launch pages and the Design page's
 * two boxes of the simulations, each over a store just cleared: WALKPAGE name / WALK ...: a page
 * in the form of the M4 sim-config scout's dialog walk (tools/openrocket-probes'
 * simulation-conditions and simulation-options: BORDER for a titled panel with its children
 * indented, LABEL (a UnitSelector is a label), SPINNER, SLIDER, CHECK, RADIO, COMBO, FIELD,
 * BUTTON, each with " disabled" and " tip=..." as they apply; hidden components and scroll bars
 * left out), STEPS name / STEP ...: what a user does on it (check boxes and buttons clicked with
 * doClick(), combo box items selected, texts set on the spinner models and the seed field, the
 * seed field's focus loss and prepareForSave() by reflection) with what it shows and the keys it
 * stores (SwingPreferences' getters store their defaults, which QtRocket's do not: no STORE of
 * these pages is printed). A random seed drawn by Java is never printed (only whether the field
 * shows the stored seed). Stored values are printed as the store holds them (Double.toString for a
 * double).</li>
 * </ul>
 */
public class PreferencesProbe {

	static SwingPreferences prefs;

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		prefs = (SwingPreferences) Application.getPreferences();
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					run();
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			});
		} catch (Exception e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	static void run() throws Exception {
		prefs.clearPreferences();
		store("CLEARED");

		UnitsPreferencesPanel units = new UnitsPreferencesPanel((JDialog) null);
		page("UNITS", units);
		store("AFTER UNITS PAGE");
		DesignPreferencesPanel design = new DesignPreferencesPanel();
		page("DESIGN", design);
		store("AFTER DESIGN PAGE");
		GeneralPreferencesPanel general = new GeneralPreferencesPanel(null);
		page("GENERAL", general);
		store("AFTER GENERAL PAGE");

		designSteps(design);
		unitsSteps(units);
		generalSteps(general);
		choiceOutOfRange();
		storeAndApply();
		merge();
		simulationPages();
	}

	// --------------------------------------------------------------------------- the pages

	static void page(String name, Container page) {
		System.out.println("PAGE " + name);
		walk(page);
		System.out.println("END " + name);
	}

	static void walk(Container container) {
		for (Component c : container.getComponents()) {
			String item = describe(c);
			if (item != null) {
				System.out.println("ITEM " + item);
			} else if (c instanceof Container) {
				walk((Container) c);
			}
		}
	}

	static String tip(Component c) {
		String tip = (c instanceof JComponent) ? ((JComponent) c).getToolTipText() : null;
		return tip == null ? "" : " tip=" + tip;
	}

	static String enabled(Component c) {
		return c.isEnabled() ? "" : " disabled";
	}

	static String describe(Component c) {
		if (c instanceof JCheckBox) {
			JCheckBox b = (JCheckBox) c;
			return "CHECK " + b.getText() + " selected=" + b.isSelected() + enabled(c) + tip(c);
		}
		if (c instanceof JButton) {
			return "BUTTON " + ((JButton) c).getText() + enabled(c) + tip(c);
		}
		if (c instanceof JLabel) {
			return "LABEL " + ((JLabel) c).getText() + enabled(c) + tip(c);
		}
		if (c instanceof JComboBox) {
			JComboBox<?> box = (JComboBox<?>) c;
			StringBuilder items = new StringBuilder();
			for (int i = 0; i < box.getItemCount(); i++) {
				items.append(i == 0 ? "" : "|").append(box.getItemAt(i));
			}
			return "COMBO selected=" + box.getSelectedItem() + " items=" + items + enabled(c) + tip(c);
		}
		if (c instanceof JSpinner) {
			return "SPINNER " + spinnerText((JSpinner) c) + enabled(c) + tip(c);
		}
		if (c instanceof JSeparator) {
			return "SEPARATOR";
		}
		if (c instanceof JTextField) {
			return "FIELD " + ((JTextField) c).getText() + enabled(c) + tip(c);
		}
		if (c instanceof JTextComponent) {
			return "TEXT " + ((JTextComponent) c).getText();
		}
		if (c instanceof JPanel || c instanceof Container && ((Container) c).getComponentCount() > 0) {
			return null;
		}
		return "OTHER " + c.getClass().getName();
	}

	static String spinnerText(JSpinner spinner) {
		JComponent editor = spinner.getEditor();
		if (editor instanceof JSpinner.DefaultEditor) {
			return ((JSpinner.DefaultEditor) editor).getTextField().getText() + " value="
					+ spinner.getValue();
		}
		return "value=" + spinner.getValue();
	}

	static <T extends Component> List<T> find(Container container, Class<T> type) {
		List<T> found = new ArrayList<>();
		for (Component c : container.getComponents()) {
			if (type.isInstance(c)) {
				found.add(type.cast(c));
			} else if (c instanceof Container && !(c instanceof JSpinner) && !(c instanceof JComboBox)) {
				found.addAll(find((Container) c, type));
			}
		}
		return found;
	}

	// --------------------------------------------------------------------------- the store

	static void store(String step) throws Exception {
		System.out.println("STORE " + step);
		dumpNode(prefs.getPreferences(), "");
		System.out.println("END STORE");
	}

	static void dumpNode(Preferences node, String path) throws Exception {
		String[] keys = node.keys();
		java.util.Arrays.sort(keys);
		for (String key : keys) {
			System.out.println("VALUE " + path + key + "=" + node.get(key, null));
		}
		String[] children = node.childrenNames();
		java.util.Arrays.sort(children);
		for (String child : children) {
			dumpNode(node.node(child), path + child + "/");
		}
	}

	static void value(String label, String key) {
		System.out.println("VALUE " + label + " " + key + "=" + prefs.getPreferences().get(key, null));
	}

	// --------------------------------------------------------------------------- the steps

	static void designSteps(DesignPreferencesPanel design) throws Exception {
		List<JComboBox> combos = find(design, JComboBox.class);
		String[] keys = { ApplicationPreferences.BODY_COMPONENT_INSERT_POSITION_KEY,
				ApplicationPreferences.STAGE_INSERT_POSITION_KEY, ApplicationPreferences.ROCKET_INFO_FONT_SIZE };
		for (int c = 0; c < combos.size(); c++) {
			for (int i : new int[] { 2, 0, 1 }) {
				combos.get(c).setSelectedIndex(i);
				value("STEP choice " + c + " index " + i + " selected=" + combos.get(c).getSelectedItem(), keys[c]);
			}
		}

		JSpinner spinner = find(design, JSpinner.class).get(0);
		for (Object v : new Object[] { "0.5", "0.95", "0.05", "0.3", "1/0", "abc", "0.123456", 0.7 }) {
			spinner.getModel().setValue(v);
			System.out.println("STEP mach set " + v + " -> text " + spinnerText(spinner) + " getDefaultMach="
					+ prefs.getDefaultMach() + " stored=" + prefs.getPreferences().get("DefaultMachNumber", null));
		}
		for (int n = 0; n < 3; n++) {
			Object next = spinner.getModel().getNextValue();
			System.out.println("STEP mach next=" + next);
			if (next != null) {
				spinner.getModel().setValue(next);
			}
		}
		System.out.println("STEP mach after next: " + spinnerText(spinner));
		spinner.getModel().setValue("0.1");
		System.out.println("STEP mach previous at 0.1=" + spinner.getModel().getPreviousValue());

		for (JCheckBox box : find(design, JCheckBox.class)) {
			box.doClick();
			System.out.println("STEP click '" + box.getText() + "' selected=" + box.isSelected());
		}
		store("AFTER DESIGN STEPS");
		for (JCheckBox box : find(design, JCheckBox.class)) {
			box.doClick();
		}
		store("AFTER DESIGN CLICKS BACK");
	}

	static void unitsSteps(UnitsPreferencesPanel units) throws Exception {
		List<JComboBox> combos = find(units, JComboBox.class);
		JComboBox length = combos.get(0);
		length.setSelectedIndex(length.getItemCount() - 1);
		System.out.println("STEP length last -> " + length.getSelectedItem() + " default="
				+ UnitGroup.UNITS_LENGTH.getDefaultUnit() + " stored="
				+ prefs.getPreferences().node("units").get("LENGTH", null));
		for (String button : new String[] { "Default imperial", "Default metric" }) {
			for (JButton b : find(units, JButton.class)) {
				if (b.getText().equals(button)) {
					b.doClick();
				}
			}
			StringBuilder line = new StringBuilder("STEP " + button + ":");
			for (JComboBox box : combos) {
				line.append(' ').append(box.getSelectedItem());
			}
			System.out.println(line);
		}
		JCheckBox secondary = find(units, JCheckBox.class).get(0);
		for (int n = 0; n < 2; n++) {
			secondary.doClick();
			System.out.println("STEP secondary selected=" + secondary.isSelected() + " combo enabled="
					+ combos.get(10).isEnabled() + " stored="
					+ prefs.getPreferences().get("DisplaySecondaryStability", null));
		}
		page("UNITS AFTER STEPS", units);
		UnitGroup.resetDefaultUnits();
	}

	static void generalSteps(GeneralPreferencesPanel general) throws Exception {
		for (JCheckBox box : find(general, JCheckBox.class)) {
			box.doClick();
			System.out.println("STEP click '" + box.getText() + "' selected=" + box.isSelected());
		}
		store("AFTER GENERAL STEPS");
	}

	static void choiceOutOfRange() throws Exception {
		for (int stored : new int[] { 3, -1, 7 }) {
			prefs.putInt(ApplicationPreferences.BODY_COMPONENT_INSERT_POSITION_KEY, stored);
			try {
				DesignPreferencesPanel design = new DesignPreferencesPanel();
				JComboBox<?> box = find(design, JComboBox.class).get(0);
				System.out.println("STEP stored choice " + stored + " -> selected=" + box.getSelectedItem());
			} catch (Throwable t) {
				System.out.println("STEP stored choice " + stored + " -> " + t);
			}
		}
		for (String mach : new String[] { "0.95", "0.05", "-0.1" }) {
			prefs.getPreferences().put("DefaultMachNumber", mach);
			DesignPreferencesPanel design = new DesignPreferencesPanel();
			System.out.println("STEP stored mach " + mach + " -> " + spinnerText(find(design, JSpinner.class).get(0)));
		}
		prefs.getPreferences().remove(ApplicationPreferences.BODY_COMPONENT_INSERT_POSITION_KEY);
		prefs.getPreferences().remove("DefaultMachNumber");
	}

	static void storeAndApply() throws Exception {
		prefs.clearPreferences();
		prefs.getPreferences().node("units").removeNode();
		UnitGroup.UNITS_LENGTH.setDefaultUnit(UnitGroup.UNITS_LENGTH.getUnitIndex(UnitGroup.UNITS_LENGTH.getUnit("in")));
		Method m = PreferencesDialog.class.getDeclaredMethod("storeAndApplyPreferences", SwingPreferences.class,
				info.openrocket.swing.gui.main.BasicFrame.class);
		m.setAccessible(true);
		m.invoke(null, prefs, null);
		store("AFTER storeAndApplyPreferences(prefs, null) with LENGTH in");
		UnitGroup.resetDefaultUnits();
	}

	static void merge() throws Exception {
		prefs.clearPreferences();
		Preferences root = prefs.getPreferences();
		root.put("KeptKey", "before");
		ByteArrayOutputStream out = new ByteArrayOutputStream();
		root.exportSubtree(out);
		root.put("KeptKey", "after");
		root.put("AddedKey", "added");
		root.node("addedNode").put("key", "value");
		root.node("units").put("LENGTH", "in");
		Preferences.importPreferences(new ByteArrayInputStream(out.toByteArray()));
		store("MERGE after import of the snapshot");
		prefs.clearPreferences();
	}

	// ------------------------------------------------------- the simulation pages (Milestone 4)

	static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n").replace("\t", "\\t");
	}

	static String walkState(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	/** The walk's line of @p c, or null for a container the walk enters (the dialog walk's describe()). */
	static String walkLine(Component c) {
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

	static void walkInto(Component c, String indent) {
		if (!c.isVisible() || c instanceof JScrollBar || c instanceof JEditorPane) {
			return;
		}
		String line = walkLine(c);
		if (line != null) {
			System.out.println("WALK " + indent + line + (line.equals("SEPARATOR") ? "" : walkState(c)));
			return;
		}
		if (c instanceof JTabbedPane tabs) {
			StringBuilder titles = new StringBuilder();
			for (int i = 0; i < tabs.getTabCount(); i++) {
				titles.append(" [").append(tabs.getTitleAt(i)).append(']');
			}
			System.out.println("WALK " + indent + "TABS" + titles + " selected=" + tabs.getSelectedIndex());
			for (int i = 0; i < tabs.getTabCount(); i++) {
				System.out.println("WALK " + indent + "TAB " + tabs.getTitleAt(i));
				if (tabs.getComponentAt(i) instanceof Container page) {
					for (Component child : page.getComponents()) {
						walkInto(child, indent + "  ");
					}
				}
			}
			return;
		}
		if (c instanceof Container k) {
			String inner = indent;
			if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
				System.out.println("WALK " + indent + "BORDER " + tb.getTitle());
				inner = indent + "  ";
			}
			for (Component child : k.getComponents()) {
				walkInto(child, inner);
			}
		}
	}

	static void walkPage(String name, Container page) {
		System.out.println("WALKPAGE " + name);
		walkInto(page, "");
		System.out.println("END " + name);
	}

	static Object field(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	static void call(Object object, Class<?> type, String name) throws Exception {
		Method m = type.getDeclaredMethod(name);
		m.setAccessible(true);
		try {
			m.invoke(object);
		} catch (InvocationTargetException e) {
			throw (Exception) e.getCause();
		}
	}

	static String stored(String key) {
		return key + "=" + prefs.getPreferences().get(key, null);
	}

	static String spinnerText(JSpinner spinner, JSlider slider) {
		return "[" + ((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText() + "]"
				+ (slider != null ? " at " + slider.getValue() : "") + (spinner.isVisible() ? "" : " hidden")
				+ (spinner.isEnabled() ? "" : " disabled");
	}

	static void typeSpinner(JSpinner spinner, String typed) {
		try {
			spinner.getModel().setValue(typed);
		} catch (RuntimeException e) {
			System.out.println("  EXCEPTION " + e.getClass().getName() + ": " + clean(e.getMessage()));
		}
	}

	static JButton button(Container container, String text) {
		for (JButton b : find(container, JButton.class)) {
			if (text.equals(b.getText())) {
				return b;
			}
		}
		throw new IllegalStateException("no button " + text);
	}

	static JCheckBox checkBox(Container container, String text) {
		for (JCheckBox b : find(container, JCheckBox.class)) {
			if (text.equals(b.getText())) {
				return b;
			}
		}
		throw new IllegalStateException("no check box " + text);
	}

	static String seedState(Object page) throws Exception {
		JCheckBox check = (JCheckBox) field(page, SimulationPreferencesPanel.class, "fixedRandomSeedCheckBox");
		JTextField seedField = (JTextField) field(page, SimulationPreferencesPanel.class, "randomSeedField");
		FlatLafOutlines.Outline outline = FlatLafOutlines.getOutline(seedField);
		String shown = seedField.getText();
		String seedText;
		if (shown.isEmpty()) {
			seedText = "[]";
		} else {
			// A seed Java drew is never printed: only whether the field shows the stored seed.
			seedText = shown.equals(Integer.toString(prefs.getRandomSeed())) ? "[the stored seed]" : "[" + shown + "]";
		}
		return "check=" + check.isSelected() + " field=" + seedText + (seedField.isEnabled() ? "" : " disabled")
				+ (outline != FlatLafOutlines.Outline.NONE ? " outline=" + outline : "") + " | "
				+ stored("SimulationRandomSeedFixed");
	}

	static String seedIs(int expected) {
		return " seed==" + expected + ":" + (prefs.getRandomSeed() == expected);
	}

	static void simulationPages() throws Exception {
		designSimulationBoxes();
		simulationPage();
		warningsPage();
		launchPage();
		prefs.clearPreferences();
	}

	static void designSimulationBoxes() throws Exception {
		prefs.clearPreferences();
		DesignPreferencesPanel design = new DesignPreferencesPanel();
		System.out.println("STEPS DESIGN");
		String[][] boxes = {
				{ "Show confirmation dialog for discarding simulation changes", "IgnoreDiscardSimulationEditingWarning" },
				{ "Update estimated flight parameters in design window", "backgroundFlight" } };
		for (int round = 0; round < 2; round++) {
			for (String[] box : boxes) {
				JCheckBox check = checkBox(design, box[0]);
				check.doClick();
				System.out.println("STEP click '" + box[0] + "' selected=" + check.isSelected() + " | " + stored(box[1]));
			}
		}
		System.out.println("END STEPS");
	}

	static void simulationPage() throws Exception {
		prefs.clearPreferences();
		SimulationPreferencesPanel page = new SimulationPreferencesPanel();
		walkPage("SIMULATION", page);

		List<JComboBox> combos = find(page, JComboBox.class);
		List<JSpinner> spinners = find(page, JSpinner.class);
		List<JSlider> sliders = find(page, JSlider.class);
		@SuppressWarnings("unchecked")
		JComboBox<Object> geodetic = combos.get(0);
		@SuppressWarnings("unchecked")
		JComboBox<Object> gravity = combos.get(1);
		JSpinner gravitySpinner = spinners.get(0);
		JSlider gravitySlider = sliders.get(0);
		JSpinner timeStep = spinners.get(1);
		JSlider timeStepSlider = sliders.get(1);
		JSpinner maxTime = spinners.get(2);

		System.out.println("STEPS SIMULATION");
		for (String text : new String[] { "Confirm deletion of simulations.",
				"Run out-dated simulations when you open the simulation tab." }) {
			JCheckBox check = checkBox(page, text);
			check.doClick();
			System.out.println("STEP click '" + text + "' selected=" + check.isSelected() + " | "
					+ stored("ConfirmDeleteSimulation") + " " + stored("AutoRunSimulations"));
		}
		for (GeodeticComputationStrategy gcs : new GeodeticComputationStrategy[] { GeodeticComputationStrategy.FLAT,
				GeodeticComputationStrategy.WGS84, GeodeticComputationStrategy.SPHERICAL }) {
			geodetic.setSelectedItem(gcs);
			System.out.println("STEP geodetic " + gcs.name() + " | geodetic=[" + geodetic.getSelectedItem() + "] tip="
					+ clean(geodetic.getToolTipText()) + " | " + stored("GeodeticComputationStrategy") + " getter="
					+ prefs.getGeodeticComputation().name());
		}
		gravity.setSelectedItem(GravityModelType.CONSTANT);
		System.out.println("STEP gravity CONSTANT | gravity=[" + gravity.getSelectedItem() + "] tip="
				+ clean(gravity.getToolTipText()) + " | " + stored("GravityModel") + " | gravity value="
				+ spinnerText(gravitySpinner, gravitySlider));
		System.out.println("END STEPS");
		walkPage("SIMULATION CONSTANT GRAVITY", page);
		System.out.println("STEPS SIMULATION 2");
		for (String typed : new String[] { "-1", "0", "3.72", "30", "150", "9.807" }) {
			typeSpinner(gravitySpinner, typed);
			System.out.println("STEP gravity typed=[" + typed + "] | gravity value="
					+ spinnerText(gravitySpinner, gravitySlider) + " | " + stored("ConstantGravityValue"));
		}
		gravity.setSelectedItem(GravityModelType.WGS);
		System.out.println("STEP gravity WGS | gravity=[" + gravity.getSelectedItem() + "] tip="
				+ clean(gravity.getToolTipText()) + " | " + stored("GravityModel") + " | gravity value="
				+ spinnerText(gravitySpinner, gravitySlider));
		for (String typed : new String[] { "0.001", "0.01", "0.02", "0.2", "5", "0.05" }) {
			typeSpinner(timeStep, typed);
			System.out.println("STEP time step typed=[" + typed + "] | time step=" + spinnerText(timeStep, timeStepSlider)
					+ " | " + stored("SimulationTimeStep"));
		}
		for (String typed : new String[] { "0.5", "1", "600", "5000000", "1200" }) {
			typeSpinner(maxTime, typed);
			System.out.println("STEP maximum time typed=[" + typed + "] | maximum time=" + spinnerText(maxTime, null)
					+ " | " + stored("SimulationMaxTime"));
		}

		JCheckBox check = (JCheckBox) field(page, SimulationPreferencesPanel.class, "fixedRandomSeedCheckBox");
		JTextField seedField = (JTextField) field(page, SimulationPreferencesPanel.class, "randomSeedField");
		System.out.println("STEP seed initial | " + seedState(page));
		check.doClick();
		System.out.println("STEP seed checked | " + seedState(page));
		for (String typed : new String[] { "12345", " -2147483648 ", "2147483648", "abc", "" }) {
			seedField.setText(typed);
			System.out.println("STEP seed typed=[" + typed + "] | " + seedState(page) + seedIs(12345)
					+ seedIs(Integer.MIN_VALUE));
		}
		seedField.setText("abc");
		call(page, SimulationPreferencesPanel.class, "replaceInvalidRandomSeed");
		System.out.println("STEP seed focus lost with [abc] | " + seedState(page));
		seedField.setText("4711");
		call(page, SimulationPreferencesPanel.class, "replaceInvalidRandomSeed");
		System.out.println("STEP seed focus lost with [4711] | " + seedState(page) + seedIs(4711));
		seedField.setText("  ");
		call(page, SimulationPreferencesPanel.class, "prepareForSave");
		System.out.println("STEP seed prepareForSave with [  ] | " + seedState(page));
		check.doClick();
		seedField.setText(" 99 ");
		call(page, SimulationPreferencesPanel.class, "prepareForSave");
		System.out.println("STEP seed prepareForSave with [ 99 ] | " + seedState(page) + seedIs(99));
		check.doClick();
		System.out.println("STEP seed unchecked | " + seedState(page) + seedIs(99));

		typeSpinner(timeStep, "0.04");
		typeSpinner(maxTime, "300");
		geodetic.setSelectedItem(GeodeticComputationStrategy.WGS84);
		gravity.setSelectedItem(GravityModelType.CONSTANT);
		typeSpinner(gravitySpinner, "5");
		check.doClick();
		seedField.setText("31");
		button(page, "Reset to default").doClick();
		System.out.println("STEP reset | time step=" + spinnerText(timeStep, timeStepSlider) + " maximum time="
				+ spinnerText(maxTime, null) + " geodetic=[" + geodetic.getSelectedItem() + "] gravity=["
				+ gravity.getSelectedItem() + "] gravity value=" + spinnerText(gravitySpinner, gravitySlider) + " | "
				+ seedState(page) + seedIs(31) + " | " + stored("SimulationTimeStep") + " "
				+ stored("SimulationMaxTime") + " " + stored("GeodeticComputationStrategy") + " "
				+ stored("GravityModel") + " " + stored("ConstantGravityValue") + " "
				+ stored("SimulationRandomSeedFixed"));
		System.out.println("END STEPS");
		walkPage("SIMULATION AFTER RESET", page);
	}

	static void warningsPage() throws Exception {
		prefs.clearPreferences();
		WarningsPreferencesPanel page = new WarningsPreferencesPanel();
		walkPage("WARNINGS", page);
		List<CollapsiblePanel> sections = find(page, CollapsiblePanel.class);
		System.out.println("STEPS WARNINGS");
		System.out.println("STEP sections " + sections.size() + " expanded=" + sections.get(0).isExpanded() + ","
				+ sections.get(1).isExpanded() + " spinners=" + find(page, JSpinner.class).size());
		JSpinner high = find(page, JSpinner.class).get(0);
		for (String typed : new String[] { "25", "-5", "20" , "37" }) {
			typeSpinner(high, typed);
			System.out.println("STEP single high typed=[" + typed + "] | " + spinnerText(high, null) + " | "
					+ stored("RecoverySpeedWarning"));
		}
		sections.get(1).getHeaderButton().doClick();
		List<JSpinner> spinners = find(page, JSpinner.class);
		System.out.println("STEP expand dual | expanded=" + sections.get(1).isExpanded() + " spinners="
				+ spinners.size());
		for (String typed : new String[] { "10", "-1", "12.5" }) {
			typeSpinner(spinners.get(1), typed);
			System.out.println("STEP dual low typed=[" + typed + "] | " + spinnerText(spinners.get(1), null)
					+ " | " + stored("RecoveryDrogueMainLowSpeedWarning"));
		}
		for (String typed : new String[] { "40", "0" , "45" }) {
			typeSpinner(spinners.get(2), typed);
			System.out.println("STEP dual high typed=[" + typed + "] | " + spinnerText(spinners.get(2), null)
					+ " | " + stored("RecoveryDrogueMainHighSpeedWarning"));
		}
		System.out.println("END STEPS");
		walkPage("WARNINGS EXPANDED", page);
		sections.get(0).getHeaderButton().doClick();
		walkPage("WARNINGS SINGLE COLLAPSED", page);
	}

	static void launchPage() throws Exception {
		prefs.clearPreferences();
		LaunchPreferencesPanel page = new LaunchPreferencesPanel();
		walkPage("LAUNCH", page);
		List<JSpinner> spinners = find(page, JSpinner.class);
		System.out.println("STEPS LAUNCH");
		// The page's average wind model was loaded and stored (the first getAverageWindModel()), and the rod
		// direction follows the wind (getLaunchRodDirection() stores it while launching into the wind).
		System.out.println("STEP made | spinners " + spinners.size() + " | " + stored("WindAverage") + " "
				+ stored("WindTurbulence") + " " + stored("WindDirection") + " " + stored("LaunchRodDirection"));
		// The average wind speed, the rod length, the latitude.
		typeSpinner(spinners.get(0), "5");
		System.out.println("STEP average wind typed=[5] | " + spinnerText(spinners.get(0), null) + " | "
				+ stored("WindAverage") + " " + stored("WindTurbulence") + " " + stored("WindDirection"));
		typeSpinner(spinners.get(10), "150");
		System.out.println("STEP rod length typed=[150] | " + spinnerText(spinners.get(10), null) + " | "
				+ stored("LaunchRodLength"));
		typeSpinner(spinners.get(7), "45");
		System.out.println("STEP latitude typed=[45] | " + spinnerText(spinners.get(7), null) + " | "
				+ stored("LaunchLatitude"));
		System.out.println("END STEPS");
	}

	// --------------------------------------------------------------------------- set-up

	static void bootstrap() throws Exception {
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				// One SwingPreferences for the probe and every page, as OpenRocket's GuiModule binds it
				// (Scopes.SINGLETON): each instance keeps its own cache of the stored keys, and a getter
				// of one that missed a key another instance stored writes the default over it.
				bind(ApplicationPreferences.class).to(SwingPreferences.class).in(Scopes.SINGLETON);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
