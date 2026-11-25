package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.Module;
import com.google.inject.Scopes;
import com.google.inject.util.Modules;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel.LevelWindModel;
import info.openrocket.core.models.wind.WindModelType;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.StateChangeListener;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.simulation.MultiLevelWindEditDialog;
import info.openrocket.swing.gui.simulation.MultiLevelWindTable;
import info.openrocket.swing.gui.simulation.SimulationConfigDialog;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Window;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.TreeSet;
import javax.swing.AbstractButton;
import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JEditorPane;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JScrollBar;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.ListModel;
import javax.swing.SwingUtilities;
import javax.swing.border.TitledBorder;

/**
 * OpenRocket's simulation settings dialog (swing gui/simulation/SimulationConfigDialog.java) over every simulation
 * of the designs it is given, for the pins of QtRocket's simulation-dialog family (Milestone 4, the exit check E4:
 * OpenDesignsExitGolden.ShowsEverySimulationInItsDialogAndCancels).
 * <p>
 * The designs come from a list in designs.py's format (name, kind, file; tools/openrocket-gui-pins/open-designs):
 * generate.sh passes the 66 designs of QtRocket's exit suite OpenDesignsExitGolden. Each design is loaded as that suite
 * loads it (and left as OpenFileWorker leaves a document File > Open loads: its file set, saved), as the
 * simulation-table family's SimulationTableProbe loads it (DesignFileEnvironment::Presets::EXAMPLES): the bundled
 * motor database (-Dprobe.motors), but for the test rockets' re-saves, whose motors are TestRockets' seven synthetic
 * ones, and the six component presets of the example designs (from the bundled .orc files below the directory of the
 * second argument). The preferences are one SwingPreferences (Scopes.SINGLETON, as OpenRocket's GuiModule binds it) in
 * generate.sh's fresh store with MotorNameColumn false, the value of the suite's environment that the configuration
 * selector shows; FlatLaf Light. Every design's UUIDs are seeded from its name (DeterministicUuids).
 * <p>
 * For each simulation of a design, in the document's order, on the one document (as the suite edits them one after
 * the other in one window): the dialog the tab's Edit opens (SimulationPanel.openDialog(false, false, sim): a new
 * SimulationConfigDialog over the simulation, switched to the settings tab; never shown), its walk, the wind editor's
 * table of a multi-level wind (the dialog that "Edit levels" opens, MultiLevelWindEditDialog, made, never shown),
 * then its Cancel button (doClick()), whose closing (an invokeLater() of the dialog) is done before the state after it
 * is printed. A dialog that would ask its discard question (a modified simulation) fails the probe, as the question
 * would block.
 * <p>
 * Output: one line each, its fields separated by tabs; a text field has its backslashes, newlines and tabs escaped as
 * \\, \n and \t; a missing value (Java's null) is &lt;null&gt;.
 * <ul>
 * <li>COLORS text warning error (the theme's colours of a simulation's status, UITheme.getStatusColor(): TEXT,
 * WARNING and ERROR, #rrggbb)</li>
 * <li>D name kind file simulationCount (a design; then its simulations, up to END)</li>
 * <li>S index name (a simulation; then its lines)</li>
 * <li>UNDO before|after position descriptions next stored clean saved (the document's undo state before the dialog
 * is made and after its Cancel: the undo position, the description of each state of the history joined by "|" (each
 * escaped, &lt;null&gt; for none), the next and the stored description, isCleanState() and isSaved())</li>
 * <li>STATUS before|after statuses (the name of getStatus() of every simulation of the document, space separated)</li>
 * <li>TITLE title (the dialog's title after it was made)</li>
 * <li>W line (the walk: the top rows, the line of the tabs, each tab's title with, for the Launch conditions,
 * Simulation options and Warnings tabs, the walk of its page indented by two spaces, then the buttons; the format of
 * the probes tools/openrocket-probes/simulation-options and simulation-warnings: TABS and TAB for a tabbed pane,
 * BORDER for a titled panel with its children indented, LABEL (a UnitSelector is a label), SPINNER, SLIDER, CHECK,
 * RADIO, COMBO with its items, FIELD, TEXTAREA, BUTTON, LIST with its rows and icons, SEPARATOR, each with " disabled"
 * and " tip=..." as they apply; hidden components, scroll bars and editor panes left out)</li>
 * <li>TAB index title enabled tip (each tab of the dialog: getTitleAt(), isEnabledAt(), getToolTipTextAt())</li>
 * <li>EXT buttons (each extension's panel in the Simulation options tab, in order: the icons of its buttons,
 * CONFIGURE, HELP and DELETE, space separated)</li>
 * <li>WIND line (a multi-level wind's levels in the wind editor's table: the wind-editor probe's HEADER, ROW and MODEL
 * lines)</li>
 * <li>MODIFIED modified title (the dialog's isModified after the walk and the wind editor, and its title, right before
 * Cancel)</li>
 * <li>CHANGES counts (the change events every simulation of the document fired from before the dialog was made until
 * after its Cancel, space separated)</li>
 * <li>END</li>
 * </ul>
 * usage: SimulationDialogProbe DESIGNS COMPONENTS
 */
public class SimulationDialogProbe {

	/** The digests of the six component presets of the example designs (tests/core/rocket/preset/ExamplePresets.h). */
	static final Set<String> EXAMPLE_PRESET_DIGESTS = Set.of("e9c3e97d32e96762f65e051d74cb9fd7",
			"b595c8a31baaf325291d5c957f3940f8", "9a96b8806749e790db29bacfbc70eb1e", "18363dec5642206a109389ef48efcb32",
			"a59dec8e4034a2fee5955dbf4ff07f1c", "6f564a198c80de3af437608827fa1bc1");

	/** The generators of the motors of TestRockets, in the order of QtRocket's testRocketMotors(). */
	static final List<String> TEST_ROCKET_MOTORS = List.of("generateMotor_A8_18mm", "generateMotor_A10_13mm",
			"generateMotor_B4_18mm", "generateMotor_C6_18mm", "generateMotor_D21_18mm", "generateMotor_M1350_75mm",
			"generateMotor_G77_29mm");

	/** The tabs whose pages the walk prints: Launch conditions, Simulation options and Warnings. */
	static final int WALKED_TABS = 3;

	static Injector designInjector;
	static Injector testRocketInjector;

	public static void main(String[] args) throws Exception {
		int status = 0;
		try {
			bootstrap(new File(args[1]));
			List<String> designs = Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8);
			SwingUtilities.invokeAndWait(() -> {
				GUIUtil.setUITheme(UITheme.Themes.LIGHT);
				line("COLORS", rgb(UITheme.getColor(UITheme.Keys.TEXT)), rgb(UITheme.getColor(UITheme.Keys.WARNING)),
						rgb(UITheme.getColor(UITheme.Keys.ERROR)));
			});
			for (String design : designs) {
				String[] fields = design.split("\t");
				if (fields.length != 3) {
					throw new IllegalStateException("not a design: " + design);
				}
				design(fields[0], fields[1], new File(fields[2]));
			}
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		} finally {
			SwingUtilities.invokeAndWait(() -> {
				for (Window window : Window.getWindows()) {
					window.dispose();
				}
			});
		}
		System.out.flush();
		System.exit(status);
	}

	/** Runs @p body on the event dispatch thread, rethrowing what it throws. */
	interface Body {
		void run() throws Exception;
	}

	static void onEdt(Body body) throws Exception {
		Exception[] thrown = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				body.run();
			} catch (Exception e) {
				thrown[0] = e;
			}
		});
		if (thrown[0] != null) {
			throw thrown[0];
		}
	}

	// ------------------------------------------------------------------------------------ output

	static void line(Object... fields) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < fields.length; i++) {
			if (i > 0) {
				sb.append('\t');
			}
			sb.append(fields[i]);
		}
		System.out.println(sb);
	}

	static String esc(String s) {
		if (s == null) {
			return "<null>";
		}
		return s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t");
	}

	static String rgb(Color color) {
		return String.format(Locale.ROOT, "#%02x%02x%02x", color.getRed(), color.getGreen(), color.getBlue());
	}

	static Object field(Object o, String name) throws Exception {
		Class<?> c = o.getClass();
		while (c != null) {
			try {
				Field f = c.getDeclaredField(name);
				f.setAccessible(true);
				return f.get(o);
			} catch (NoSuchFieldException e) {
				c = c.getSuperclass();
			}
		}
		throw new NoSuchFieldException(name);
	}

	// ------------------------------------------------------------------------------------ the designs

	static void design(String name, String kind, File file) throws Exception {
		OpenRocketDocument[] doc = new OpenRocketDocument[1];
		onEdt(() -> {
			Application.setInjector(kind.equals("testrocket") ? testRocketInjector : designInjector);
			DeterministicUuids.reseed(name);
			doc[0] = new GeneralRocketLoader(file).load();
			// As OpenFileWorker leaves a document File > Open loads.
			doc[0].setFile(file);
			doc[0].setSaved(true);
			line("D", name, kind, esc(file.getName()), doc[0].getSimulationCount());
		});
		for (int i = 0; i < doc[0].getSimulationCount(); i++) {
			simulation(doc[0], i);
		}
		onEdt(() -> line("END"));
	}

	/** The dialog of the simulation @p index of @p doc, walked, then cancelled. */
	static void simulation(OpenRocketDocument doc, int index) throws Exception {
		int[] changes = new int[doc.getSimulationCount()];
		List<StateChangeListener> listeners = new ArrayList<>();
		onEdt(() -> {
			Simulation sim = doc.getSimulation(index);
			line("S", index, esc(sim.getName()));
			undo("before", doc);
			statuses("before", doc);
			for (int i = 0; i < doc.getSimulationCount(); i++) {
				final int counted = i;
				StateChangeListener listener = e -> ++changes[counted];
				doc.getSimulation(i).addChangeListener(listener);
				listeners.add(listener);
			}
			// SimulationPanel.openDialog(plotMode false, isNewSimulation false, sim), not shown.
			SimulationConfigDialog dialog = new SimulationConfigDialog(null, doc, false, sim);
			dialog.switchToSettingsTab();
			line("TITLE", esc(dialog.getTitle()));
			walkDialog(dialog);
			if (sim.getOptions().getWindModelType() == WindModelType.MULTI_LEVEL) {
				wind(dialog, sim.getOptions().getMultiLevelWindModel());
			}
			boolean modified = (Boolean) field(dialog, "isModified");
			line("MODIFIED", modified, esc(dialog.getTitle()));
			if (modified) {
				throw new IllegalStateException(sim.getName() + ": the dialog would ask its discard question");
			}
			((JButton) field(dialog, "cancelButton")).doClick();
		});
		// The dialog's closing (closeDialog(): an invokeLater()) is done by now.
		onEdt(() -> {
			StringBuilder counts = new StringBuilder();
			for (int i = 0; i < changes.length; i++) {
				counts.append(i > 0 ? " " : "").append(changes[i]);
				doc.getSimulation(i).removeChangeListener(listeners.get(i));
			}
			line("CHANGES", counts);
			undo("after", doc);
			statuses("after", doc);
		});
	}

	@SuppressWarnings("unchecked")
	static void undo(String when, OpenRocketDocument doc) throws Exception {
		StringBuilder descriptions = new StringBuilder();
		List<String> list = (List<String>) field(doc, "undoDescription");
		for (int i = 0; i < list.size(); i++) {
			descriptions.append(i > 0 ? "|" : "").append(esc(list.get(i)));
		}
		Method clean = OpenRocketDocument.class.getDeclaredMethod("isCleanState");
		clean.setAccessible(true);
		line("UNDO", when, field(doc, "undoPosition"), descriptions, esc((String) field(doc, "nextDescription")),
				esc((String) field(doc, "storedDescription")), clean.invoke(doc), doc.isSaved());
	}

	static void statuses(String when, OpenRocketDocument doc) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < doc.getSimulationCount(); i++) {
			sb.append(i > 0 ? " " : "").append(doc.getSimulation(i).getStatus().name());
		}
		line("STATUS", when, sb);
	}

	// ------------------------------------------------------------------------------------ the walk

	static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n").replace("\t", "\\t");
	}

	static String state(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	static String iconName(Icon icon) {
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
		if (icon == Icons.CONFIGURE) {
			return "CONFIGURE";
		}
		if (icon == Icons.HELP) {
			return "HELP";
		}
		if (icon == Icons.EDIT_DELETE) {
			return "DELETE";
		}
		return "other";
	}

	/** The line of @p c, or null for a container the walk enters. */
	@SuppressWarnings({ "unchecked", "rawtypes" })
	static String describe(Component c) {
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

	static void walk(Component c, String indent) {
		if (!c.isVisible() || c instanceof JScrollBar || c instanceof JEditorPane) {
			return;
		}
		String line = describe(c);
		if (line != null) {
			line("W", esc(indent + line + (line.equals("SEPARATOR") ? "" : state(c))));
			return;
		}
		if (c instanceof JTabbedPane tabs) {
			StringBuilder titles = new StringBuilder();
			for (int i = 0; i < tabs.getTabCount(); i++) {
				titles.append(" [").append(tabs.getTitleAt(i)).append(']');
			}
			line("W", esc(indent + "TABS" + titles + " selected=" + tabs.getSelectedIndex()));
			for (int i = 0; i < tabs.getTabCount(); i++) {
				line("W", esc(indent + "TAB " + tabs.getTitleAt(i)));
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
				line("W", esc(indent + "BORDER " + tb.getTitle()));
				inner = indent + "  ";
			}
			for (Component child : k.getComponents()) {
				walk(child, inner);
			}
		}
	}

	/**
	 * The dialog's walk: its top rows, the line of the tabs, each tab's title (with the page of the first three), the
	 * buttons; then each tab's state and the extensions' buttons.
	 */
	static void walkDialog(SimulationConfigDialog dialog) throws Exception {
		JTabbedPane tabs = (JTabbedPane) field(dialog, "tabbedPane");
		// The content panel holds the top rows and the tabs; the bottom panel is the dialog's second child.
		Container content = tabs.getParent();
		Component top = content.getComponent(0);
		walk(top, "");
		StringBuilder titles = new StringBuilder();
		for (int i = 0; i < tabs.getTabCount(); i++) {
			titles.append(" [").append(tabs.getTitleAt(i)).append(']');
		}
		line("W", esc("TABS" + titles + " selected=" + tabs.getSelectedIndex()));
		for (int i = 0; i < tabs.getTabCount(); i++) {
			line("W", esc("TAB " + tabs.getTitleAt(i)));
			if (i < WALKED_TABS && tabs.getComponentAt(i) instanceof Container page) {
				for (Component child : page.getComponents()) {
					walk(child, "  ");
				}
			}
		}
		Container pane = dialog.getContentPane();
		Component bottom = null;
		for (Component child : pane.getComponents()) {
			if (child != content) {
				bottom = child;
			}
		}
		walk(bottom, "");
		for (int i = 0; i < tabs.getTabCount(); i++) {
			line("TAB", i, esc(tabs.getTitleAt(i)), tabs.isEnabledAt(i), esc(tabs.getToolTipTextAt(i)));
		}
		Object optionsPanel = field(dialog, "simulationOptionsTab");
		JPanel current = (JPanel) field(optionsPanel, "currentExtensions");
		for (Component c : current.getComponents()) {
			if (c instanceof JPanel p) {
				StringBuilder buttons = new StringBuilder();
				for (Component child : p.getComponents()) {
					if (child instanceof JButton b) {
						buttons.append(buttons.length() > 0 ? " " : "").append(iconName(b.getIcon()));
					}
				}
				line("EXT", buttons);
			}
		}
	}

	// ------------------------------------------------------------------------------------ the wind levels

	static <T> List<T> find(Component c, Class<T> type) {
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

	/** The wind editor's table of @p model (the dialog "Edit levels" opens over the settings dialog). */
	@SuppressWarnings("unchecked")
	static void wind(SimulationConfigDialog owner, MultiLevelPinkNoiseWindModel model) throws Exception {
		MultiLevelWindEditDialog editor = new MultiLevelWindEditDialog(owner, model);
		MultiLevelWindTable table = (MultiLevelWindTable) field(editor, "windTable");
		List<JLabel> labels = new ArrayList<>();
		for (JLabel l : find(table.getHeaderPanel(), JLabel.class)) {
			if (!(l instanceof UnitSelector)) {
				labels.add(l);
			}
		}
		List<UnitSelector> selectors = find(table.getHeaderPanel(),
				UnitSelector.class);
		for (int i = 0; i < labels.size(); i++) {
			JLabel l = labels.get(i);
			String unit = i < selectors.size() ? selectors.get(i).getSelectedUnit().getUnit() : "none";
			line("WIND", esc("HEADER " + i + " [" + clean(l.getText()) + "]"
					+ (l.getToolTipText() != null ? " tip=" + clean(l.getToolTipText()) : "") + " unit=[" + unit + "]"));
		}
		List<JComponent> rows = (List<JComponent>) field(table, "rows");
		for (int i = 0; i < rows.size(); i++) {
			JComponent row = rows.get(i);
			StringBuilder sb = new StringBuilder("ROW " + i);
			for (JSpinner spinner : find(row, JSpinner.class)) {
				sb.append(" [").append(((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText()).append(']');
			}
			JLabel intensity = (JLabel) field(row, "intensityLabel");
			JButton delete = (JButton) field(row, "deleteButton");
			sb.append(" intensity=[").append(intensity.getText()).append("] delete=")
					.append(delete.isEnabled() ? "enabled" : "disabled");
			line("WIND", esc(sb.toString()));
		}
		StringBuilder sb = new StringBuilder("MODEL");
		for (LevelWindModel level : model.getLevels()) {
			sb.append(' ').append(level.getAltitude());
		}
		line("WIND", esc(sb.toString()));
		editor.dispose();
	}

	// ------------------------------------------------------------------------------------ set-up

	/** The .orc files below @p directory, depth first, each directory in name order (DirectoryIterator's walk). */
	static void collectOrcFiles(File directory, List<File> files) {
		File[] children = directory.listFiles();
		if (children == null) {
			throw new IllegalStateException("not a directory: " + directory);
		}
		Arrays.sort(children, (a, b) -> a.getName().compareTo(b.getName()));
		for (File child : children) {
			if (child.getName().startsWith(".")) {
				continue;
			}
			if (child.isDirectory()) {
				collectOrcFiles(child, files);
			} else if (child.getName().toLowerCase(Locale.ROOT).endsWith(".orc")) {
				files.add(child);
			}
		}
	}

	/** Adds to @p presets the six presets of the example designs, from the bundled .orc files below @p components. */
	static void addExamplePresets(ComponentPresetDatabase presets, File components) throws Exception {
		List<File> files = new ArrayList<>();
		collectOrcFiles(components, files);
		Set<String> found = new TreeSet<>();
		int count = 0;
		for (File file : files) {
			try (InputStream in = new FileInputStream(file)) {
				for (ComponentPreset preset : new OpenRocketComponentLoader().load(in, file.getName())) {
					if (EXAMPLE_PRESET_DIGESTS.contains(preset.getDigest())) {
						presets.add(preset);
						found.add(preset.getDigest());
						++count;
					}
				}
			}
		}
		if (count != EXAMPLE_PRESET_DIGESTS.size() || !found.equals(EXAMPLE_PRESET_DIGESTS)) {
			throw new IllegalStateException("the bundled presets hold " + count
					+ " presets of the example designs' digests, not each of the six once: " + found);
		}
	}

	static Injector injector(ThrustCurveMotorSetDatabase motors, ComponentPresetDatabase presets,
			SwingPreferences preferences) {
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				// One instance, as OpenRocket's GuiModule binds it (Scopes.SINGLETON): every class that reads the
				// preferences when it is initialised and every getter share its cache of the stored keys. The test
				// rockets' injector is given the instance of the designs' one.
				if (preferences == null) {
					bind(ApplicationPreferences.class).to(SwingPreferences.class).in(Scopes.SINGLETON);
				} else {
					bind(ApplicationPreferences.class).toInstance(preferences);
				}
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		return Guice.createInjector(Modules.override(new info.openrocket.swing.ServicesForTesting()).with(overrides),
				new PluginModule());
	}

	static void bootstrap(File components) throws Exception {
		// Before anything makes a UUID: each design gets the same UUIDs on every run.
		DeterministicUuids.install();
		DeterministicUuids.reseed("simulation-dialog");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		ThrustCurveMotorSetDatabase bundled = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile())) {
			bundled.addMotor(motor);
		}
		ThrustCurveMotorSetDatabase synthetic = new ThrustCurveMotorSetDatabase();
		for (String generator : TEST_ROCKET_MOTORS) {
			Method method = TestRockets.class.getDeclaredMethod(generator);
			method.setAccessible(true);
			synthetic.addMotor((ThrustCurveMotor) method.invoke(null));
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		designInjector = injector(bundled, presets, null);
		Application.setInjector(designInjector);
		testRocketInjector = injector(synthetic, presets, (SwingPreferences) Application.getPreferences());
		addExamplePresets(presets, components);
		// The suite's preference store shows a motor by its common name (DesignFileEnvironment), in the
		// configurations' names (Motor.getMotorName()).
		Application.getPreferences().putBoolean(ApplicationPreferences.MOTOR_NAME_COLUMN, false);
		if (Application.getPreferences().getMotorNameColumn()) {
			throw new IllegalStateException("MotorNameColumn is not false");
		}
	}
}
