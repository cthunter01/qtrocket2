package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
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
import info.openrocket.swing.gui.dialogs.preferences.DesignPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.GeneralPreferencesPanel;
import info.openrocket.swing.gui.dialogs.preferences.PreferencesDialog;
import info.openrocket.swing.gui.dialogs.preferences.UnitsPreferencesPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JSeparator;
import javax.swing.JSpinner;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.text.JTextComponent;
import java.awt.Component;
import java.awt.Container;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
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

	// --------------------------------------------------------------------------- set-up

	static void bootstrap() throws Exception {
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
