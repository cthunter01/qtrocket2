package probe;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import javax.swing.AbstractButton;
import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JEditorPane;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JRadioButton;
import javax.swing.JScrollBar;
import javax.swing.JScrollPane;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.ListModel;
import javax.swing.SwingUtilities;
import javax.swing.border.TitledBorder;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.Scopes;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.components.CollapsiblePanel;
import info.openrocket.swing.gui.simulation.SimulationWarningsPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Milestone 4 run 2, part S5: OpenRocket's Warnings tab of the simulation settings dialog (swing
 * gui/simulation/SimulationWarningsPanel.java, its public constructor) over simulations of the examples
 * (data/examples), headless under FlatLaf Light with SwingPreferences of a fresh store (one instance, as
 * OpenRocket's GuiModule binds it). For each panel it prints:
 * - WALK: the panel in the form of the M4 sim-config scout's dialog walk (TABS and TAB for a tabbed pane, BORDER
 *   for a titled panel with its children indented; LABEL (a UnitSelector is a label), SPINNER, BUTTON, ...,
 *   each with " disabled" and " tip=..." as they apply; hidden components and scroll bars left out), with
 *   LIST for a JList: its rows joined by " | " and the icon its renderer gives them (WARNING_HIGH,
 *   WARNING_NORMAL or WARNING_LOW of Icons);
 * - STYLE: the font colour of every label of the Messages tab as #rrggbb, its style and its size in points
 *   above the default label font's (to one decimal);
 * - TABS: the scroll panes of the tabs (their borders, unit increments and preferred heights).
 * The cases: the first simulation of "A simple model rocket" (no warnings) for one simulation and for a
 * multi-simulation edit, its third (one warning), the first of "Two stage high power rocket" (an informational
 * one), the third of "Deployable payload" (two), and the first of "A simple model rocket" with four warnings of
 * every priority added to its stored ones. Then the steps of "Reset to default" and "Save as default" (STEP):
 * the thresholds typed on the spinner models, the preferences changed, the buttons clicked with doClick(), with
 * the options' and the preferences' thresholds.
 */
public final class SimulationWarningsProbe {
	private final PrintStream out;

	private SimulationWarningsProbe(PrintStream out) {
		this.out = out;
	}

	private static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n").replace("\t", "\\t");
	}

	private static String state(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	private static String iconName(Icon icon) {
		if (icon == null) {
			return "none";
		}
		if (icon == Icons.WARNING_HIGH) {
			return "WARNING_HIGH";
		}
		if (icon == Icons.WARNING_NORMAL) {
			return "WARNING_NORMAL";
		}
		if (icon == Icons.WARNING_LOW) {
			return "WARNING_LOW";
		}
		return "other";
	}

	@SuppressWarnings({ "unchecked", "rawtypes" })
	private static String describe(Component c) {
		if (c instanceof JList list) {
			ListModel model = list.getModel();
			StringBuilder rows = new StringBuilder();
			String icons = "";
			for (int i = 0; i < model.getSize(); i++) {
				Object value = model.getElementAt(i);
				rows.append(i > 0 ? " | " : "").append(clean(String.valueOf(value)));
				Component renderer = list.getCellRenderer().getListCellRendererComponent(list, value, i, false, false);
				String icon = renderer instanceof JLabel l ? iconName(l.getIcon()) : "none";
				icons = i == 0 ? icon : (icons.equals(icon) ? icons : icons + "," + icon);
			}
			return "LIST " + rows + " icon=" + icons;
		}
		if (c instanceof JSpinner sp) {
			return "SPINNER " + clean(((JSpinner.DefaultEditor) sp.getEditor()).getTextField().getText());
		}
		if (c instanceof JComboBox<?> cb) {
			return "COMBO selected=" + clean(String.valueOf(cb.getSelectedItem()));
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

	private void walk(Component c, String indent) {
		if (!c.isVisible() || c instanceof JScrollBar || c instanceof JEditorPane) {
			return;
		}
		String line = describe(c);
		if (line != null) {
			out.println("WALK " + indent + line + (line.equals("SEPARATOR") ? "" : state(c)));
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
						walk(child, indent + "  ");
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
				walk(child, inner);
			}
		}
	}

	private static <T extends Component> List<T> find(Component root, Class<T> type) {
		List<T> found = new ArrayList<>();
		if (type.isInstance(root)) {
			found.add(type.cast(root));
		}
		if (root instanceof Container container) {
			for (Component child : container.getComponents()) {
				found.addAll(find(child, type));
			}
		}
		return found;
	}

	private static String hex(Color color) {
		return String.format("#%02x%02x%02x", color.getRed(), color.getGreen(), color.getBlue());
	}

	private void styles(SimulationWarningsPanel panel) {
		JTabbedPane tabs = find(panel, JTabbedPane.class).get(0);
		float base = new JLabel().getFont().getSize2D();
		for (JLabel label : find(tabs.getComponentAt(0), JLabel.class)) {
			if (find(label, JList.class).isEmpty() && SwingUtilities.getAncestorOfClass(JList.class, label) == null) {
				int style = label.getFont().getStyle();
				String styleName = style == 0 ? "PLAIN" : style == 1 ? "BOLD" : style == 2 ? "ITALIC" : "BOLD_ITALIC";
				out.println("STYLE " + clean(label.getText()) + " color=" + hex(label.getForeground()) + " style="
						+ styleName + " size=+" + String.format(java.util.Locale.ROOT, "%.1f",
								label.getFont().getSize2D() - base));
			}
		}
		for (int i = 0; i < tabs.getTabCount(); i++) {
			JScrollPane pane = (JScrollPane) tabs.getComponentAt(i);
			out.println("TABS " + tabs.getTitleAt(i) + " scrollPane=" + (pane.getBorder() == null ? "no border" : "border")
					+ " unitIncrement=" + pane.getVerticalScrollBar().getUnitIncrement() + " preferredHeight="
					+ pane.getPreferredSize().height);
		}
	}

	private void panel(String title, Simulation simulation, boolean showSimulationWarnings) {
		out.println("== " + title + " showSimulationWarnings=" + showSimulationWarnings);
		SimulationWarningsPanel panel = new SimulationWarningsPanel(simulation, showSimulationWarnings);
		walk(panel, "");
		styles(panel);
	}

	private static OpenRocketDocument load(File repo, String example) throws Exception {
		return new GeneralRocketLoader(new File(repo, "data/examples/" + example + ".ork")).load();
	}

	private static void typeSpinner(JSpinner spinner, String typed) {
		spinner.getModel().setValue(typed);
	}

	private static String thresholds(SimulationOptions options) {
		return "options " + options.getRecoverySpeedWarning() + " " + options.getRecoveryDrogueMainLowSpeedWarning() + " "
				+ options.getRecoveryDrogueMainHighSpeedWarning();
	}

	private static String thresholds(ApplicationPreferences preferences) {
		return "preferences " + preferences.getRecoverySpeedWarning() + " "
				+ preferences.getRecoveryDrogueMainLowSpeedWarning() + " "
				+ preferences.getRecoveryDrogueMainHighSpeedWarning();
	}

	private static JButton button(Component root, String text) {
		for (JButton b : find(root, JButton.class)) {
			if (text.equals(b.getText())) {
				return b;
			}
		}
		throw new IllegalStateException("no button " + text);
	}

	private void run(File repo) throws Exception {
		OpenRocketDocument simple = load(repo, "A simple model rocket");
		panel("A simple model rocket.ork, " + simple.getSimulation(0).getName(), simple.getSimulation(0), true);
		panel("A simple model rocket.ork, " + simple.getSimulation(0).getName(), simple.getSimulation(0), false);
		panel("A simple model rocket.ork, " + simple.getSimulation(2).getName(), simple.getSimulation(2), true);
		OpenRocketDocument twoStage = load(repo, "Two stage high power rocket");
		panel("Two stage high power rocket.ork, " + twoStage.getSimulation(0).getName(), twoStage.getSimulation(0), true);
		OpenRocketDocument payload = load(repo, "Deployable payload");
		panel("Deployable payload.ork, " + payload.getSimulation(2).getName(), payload.getSimulation(2), true);

		OpenRocketDocument withWarnings = load(repo, "A simple model rocket");
		Simulation simulation = withWarnings.getSimulation(0);
		// The loaded flight data's warning set is immutable: the data gets a new one (its final field set by
		// reflection) with the warnings it had and four more.
		WarningSet warnings = new WarningSet();
		warnings.addAll(simulation.getSimulatedWarnings());
		warnings.add(Warning.NO_RECOVERY_DEVICE);
		warnings.add(Warning.SUPERSONIC);
		warnings.add(Warning.TUMBLE_UNDER_THRUST);
		warnings.add(Warning.DIAMETER_DISCONTINUITY);
		Field field = FlightData.class.getDeclaredField("warnings");
		field.setAccessible(true);
		field.set(simulation.getSimulatedData(), warnings);
		panel("A simple model rocket.ork, " + simulation.getName() + " with NO_RECOVERY_DEVICE, SUPERSONIC, "
				+ "TUMBLE_UNDER_THRUST and DIAMETER_DISCONTINUITY added", simulation, true);

		out.println("== the default buttons");
		ApplicationPreferences preferences = Application.getPreferences();
		OpenRocketDocument document = load(repo, "A simple model rocket");
		Simulation edited = document.getSimulation(0);
		SimulationOptions options = edited.getOptions();
		SimulationWarningsPanel panel = new SimulationWarningsPanel(edited, true);
		find(panel, CollapsiblePanel.class).get(1).setExpanded(true);
		List<JSpinner> spinners = find(panel, JSpinner.class);
		out.println("STEP made | spinners " + spinners.size() + " | " + thresholds(options) + " | "
				+ thresholds(preferences));
		typeSpinner(spinners.get(0), "25");
		typeSpinner(spinners.get(1), "10");
		typeSpinner(spinners.get(2), "40");
		out.println("STEP typed 25 10 40 | " + thresholds(options) + " | " + thresholds(preferences));
		button(panel, "Reset to default").doClick();
		out.println("STEP reset, fresh preferences | " + thresholds(options) + " | " + thresholds(preferences)
				+ " | spinners [" + text(spinners.get(0)) + "] [" + text(spinners.get(1)) + "] [" + text(spinners.get(2))
				+ "]");
		preferences.setRecoverySpeedWarning(33);
		preferences.setRecoveryDrogueMainLowSpeedWarning(11);
		preferences.setRecoveryDrogueMainHighSpeedWarning(44);
		button(panel, "Reset to default").doClick();
		out.println("STEP reset, stored preferences | " + thresholds(options) + " | " + thresholds(preferences)
				+ " | spinners [" + text(spinners.get(0)) + "] [" + text(spinners.get(1)) + "] [" + text(spinners.get(2))
				+ "]");
		typeSpinner(spinners.get(0), "26");
		typeSpinner(spinners.get(1), "12.5");
		typeSpinner(spinners.get(2), "41");
		button(panel, "Save as default").doClick();
		out.println("STEP save 26 12.5 41 | " + thresholds(options) + " | " + thresholds(preferences));
	}

	private static String text(JSpinner spinner) {
		return ((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText();
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
				// One SwingPreferences, as OpenRocket's GuiModule binds it (Scopes.SINGLETON): each instance
				// keeps its own cache of the stored keys.
				bind(ApplicationPreferences.class).to(SwingPreferences.class).in(Scopes.SINGLETON);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		UITheme.Themes.LIGHT.applyTheme();
		SimulationWarningsProbe probe = new SimulationWarningsProbe(out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				probe.run(repo);
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
