package probe;

import java.awt.event.ActionEvent;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Enumeration;
import java.util.List;

import javax.swing.Action;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.KeyStroke;
import javax.swing.RowFilter;
import javax.swing.SwingUtilities;
import javax.swing.table.TableColumn;
import javax.swing.table.TableModel;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Bulkhead;
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.EngineBlock;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TubeCoupler;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.adaptors.PresetModel;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetRowFilter;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetTable;
import info.openrocket.swing.gui.dialogs.preset.XTableColumnModel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * What OpenRocket's parts library dialog (ComponentPresetChooserDialog, swing
 * gui/dialogs/preset/ComponentPresetChooserDialog.java) shows and does for the components of two
 * designs, over the bundled presets, for QtRocket's tests/gui/dialogs/preset/PresetChooserPins.h
 * (gen_preset_chooser_pins.py; preset_chooser_golden_tests.cpp compares them).
 *
 * The presets are the bundled ones (the components directory given as the argument: every .orc
 * file below it, depth first, each directory in name order, as QtRocket's DirectoryIterator walks
 * it, each file's presets added with Database.addAll(), ComponentPresetTableProbe's walk), bound as
 * the application's ComponentPresetDao, with SwingPreferences in the fresh java.util.prefs store
 * generate.sh gives every probe and FlatLaf Light. Each dialog is made as RocketComponentConfig's
 * selectPreset() makes it, its owner a configuration dialog whose owner is a main window, and
 * never shown: a JDialog, which a headless AWT refuses to make, so the probe runs under xvfb-run.
 * The dialog's PresetModel records the preset the dialog applies instead of loading it (the
 * dialog calls nothing else of it).
 *
 * Before every scenario the preferences the dialog reads and writes are put back to their defaults
 * (MatchForeDiameter and MatchAftDiameter true, no "<name>AlwaysOpenPreset", an empty "tables"
 * node, no window size or position), then the scenario's own are set, and OpenRocket's UUIDs are
 * reseeded from the scenario's name (DeterministicUuids; the designs are built before the first
 * scenario, after a reseed of their own).
 *
 * The lines, one per fact, fields separated by " | " (every double Double.toHexString):
 * <ul>
 * <li>DATABASE files | read | kept; TEXT key | text (the dialog's texts, from the first dialog)</li>
 * <li>SCENARIO index | name | component name | preset type | MatchForeDiameter | MatchAftDiameter
 * | AlwaysOpenPreset (true, false or "default": not stored)</li>
 * <li>RADII the scenario's design: each symmetric component's fore and aft radius, in order</li>
 * <li>COLUMN header | model index | shown (the table's columns in XTableColumnModel's order)</li>
 * <li>INDEX legacyColumnIndex | foreDiameterColumnIndex | aftDiameterColumnIndex</li>
 * <li>BOX legacy|fore|aft | text | tooltip ("null" for none) | shown | selected, or "BOX fore |
 * none" when the dialog has no such box (a component that is not symmetric)</li>
 * <li>FILTER fore|aft | value | epsilon (ComponentPresetRowFilter's), "none" without one</li>
 * <li>FIXED aft | OpenRocket's value | QtRocket's | QtRocket's epsilon: for a flipped nose cone whose previous
 * component's fore and aft radii differ (bug 16 of QtRocket's plan, section 10.3: OpenRocket takes
 * the fore radius, QtRocket the aft one); the dialog's aft filter is then replaced by the fixed one
 * (by reflection, before the first view), so that the views are QtRocket's</li>
 * <li>ALWAYS text | selected (the "Always open this dialog" box)</li>
 * <li>VIEW step | argument | rows n md5 hex: the rows after each step (the MD5 of the view rows'
 * digests, each followed by '\n', ComponentPresetTableProbe.digestOfView()); the steps: open, the
 * fore and the aft box unticked (when shown and ticked), the legacy box ticked, the texts, the text
 * cleared, the legacy box unticked, the fore and the aft box ticked again</li>
 * <li>PREFS MatchForeDiameter | MatchAftDiameter (after the steps)</li>
 * <li>ACTION scenario | what | the applied preset's digest ("none") | the "tables" node's keys
 * after it (key=value separated by ";", a width as "cw"), or for the "Always open this dialog"
 * box its preference and the box's state</li>
 * <li>DIALOG, FOCUS and KEY: the dialog as a window and the table's Enter keys (window())</li>
 * </ul>
 */
public class PresetChooserProbe {

	/** The text filters of every scenario (none of them can match a Boolean column's text). */
	static final String[] TEXTS = { "estes", "BT-20", "Cardboard", "ogive", "2.48 cm" };

	static ComponentPresetDatabase database;
	static JFrame frame;
	static JDialog config;

	/** A PresetModel that records what the dialog applies (setSelectedItem()) instead of loading it. */
	static final class RecordingModel extends PresetModel {
		private static final long serialVersionUID = 1L;
		Object applied;

		RecordingModel(OpenRocketDocument document, RocketComponent component) {
			super(null, document, component);
		}

		@Override
		public void setSelectedItem(Object item) {
			applied = item;
		}
	}

	static Object field(Object object, String name) throws Exception {
		Field f = ComponentPresetChooserDialog.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	static void setField(Object object, String name, Object value) throws Exception {
		Field f = ComponentPresetChooserDialog.class.getDeclaredField(name);
		f.setAccessible(true);
		f.set(object, value);
	}

	static Object filterField(Object filter, String name) throws Exception {
		Field f = ComponentPresetRowFilter.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(filter);
	}

	static void updateFilters(ComponentPresetChooserDialog dialog) throws Exception {
		Method m = ComponentPresetChooserDialog.class.getDeclaredMethod("updateFilters");
		m.setAccessible(true);
		m.invoke(dialog);
	}

	static String hex(double value) {
		return Double.toHexString(value);
	}

	static SwingPreferences preferences() {
		return (SwingPreferences) Application.getPreferences();
	}

	static void bootstrap(String components) throws Exception {
		DeterministicUuids.install();
		DeterministicUuids.reseed("preset-chooser");
		database = new ComponentPresetDatabase();
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(database);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		List<File> files = new ArrayList<>();
		ComponentPresetTableProbe.collect(new File(components), files);
		int read = 0;
		for (File file : files) {
			try (InputStream in = new FileInputStream(file)) {
				Collection<ComponentPreset> loaded = new OpenRocketComponentLoader().load(in, file.getName());
				read += loaded.size();
				database.addAll(loaded);
			}
		}
		System.out.println("DATABASE " + files.size() + " | " + read + " | " + database.size());
		SwingUtilities.invokeAndWait(() -> {
			GUIUtil.setUITheme(UITheme.Themes.LIGHT);
			frame = new JFrame();
			frame.setSize(1000, 800);
			config = new JDialog(frame);
		});
	}

	/** Every preference the dialog reads or writes back at its default. */
	static void resetPreferences(RocketComponent component) throws Exception {
		SwingPreferences p = preferences();
		p.setMatchForeDiameter(true);
		p.setMatchAftDiameter(true);
		p.putString(component.getComponentName() + "AlwaysOpenPreset", null);
		p.getTablePreferences().clear();
		p.getWindowsPreferences().clear();
	}

	/** A scenario's preferences: MatchForeDiameter, MatchAftDiameter and AlwaysOpenPreset (null: not stored). */
	record Setup(boolean matchFore, boolean matchAft, Boolean alwaysOpen) {
		static final Setup DEFAULT = new Setup(true, true, null);
		/** The actions' dialogs: the match boxes off, so that the body tube's table has rows. */
		static final Setup ACTIONS = new Setup(false, false, null);
	}

	static String box(String name, JCheckBox box) {
		if (box == null) {
			return "BOX " + name + " | none";
		}
		return "BOX " + name + " | " + box.getText() + " | " + box.getToolTipText() + " | " + (box.getParent() != null)
				+ " | " + box.isSelected();
	}

	static String filter(String name, Object filter) throws Exception {
		if (filter == null) {
			return "FILTER " + name + " | none";
		}
		return "FILTER " + name + " | " + hex((Double) filterField(filter, "dvalue")) + " | "
				+ hex((Double) filterField(filter, "epsilon"));
	}

	static String radii(RocketComponent component) {
		StringBuilder line = new StringBuilder("RADII");
		for (RocketComponent c : component.getRocket()) {
			if (c instanceof SymmetricComponent s && s.getParent() instanceof AxialStage) {
				line.append(line.length() == 5 ? " " : " | ").append(c.getComponentName()).append(' ')
						.append(hex(s.getForeRadius())).append(' ').append(hex(s.getAftRadius()));
			}
		}
		return line.toString();
	}

	static ComponentPresetChooserDialog open(String name, OpenRocketDocument document, RocketComponent component,
			Setup setup, RecordingModel[] model) throws Exception {
		resetPreferences(component);
		SwingPreferences p = preferences();
		p.setMatchForeDiameter(setup.matchFore());
		p.setMatchAftDiameter(setup.matchAft());
		if (setup.alwaysOpen() != null) {
			p.putBoolean(component.getComponentName() + "AlwaysOpenPreset", setup.alwaysOpen());
		}
		DeterministicUuids.reseed(name);
		ComponentPresetChooserDialog[] dialog = new ComponentPresetChooserDialog[1];
		SwingUtilities.invokeAndWait(() -> {
			model[0] = new RecordingModel(document, component);
			dialog[0] = new ComponentPresetChooserDialog(config, component, model[0]);
		});
		return dialog[0];
	}

	static String view(ComponentPresetChooserDialog dialog) throws Exception {
		ComponentPresetTable table = (ComponentPresetTable) field(dialog, "componentSelectionTable");
		@SuppressWarnings("unchecked")
		List<ComponentPreset> presets = (List<ComponentPreset>) field(dialog, "presets");
		return ComponentPresetTableProbe.digestOfView(table, presets);
	}

	static void step(ComponentPresetChooserDialog dialog, String step, String argument) throws Exception {
		System.out.println("VIEW " + step + " | " + argument + " | " + view(dialog));
	}

	static void texts(ComponentPresetChooserDialog dialog) throws Exception {
		System.out.println("TEXT title | " + dialog.getTitle());
		JComponent panel = (JComponent) dialog.getContentPane().getComponent(0);
		JComponent sub = (JComponent) panel.getComponent(0);
		System.out.println("TEXT filterLabel | " + ((JLabel) sub.getComponent(0)).getText());
		for (java.awt.Component c : panel.getComponents()) {
			if (c instanceof JLabel label) {
				System.out.println("TEXT favoritesLabel | " + label.getText());
			} else if (c instanceof JButton button) {
				System.out.println("TEXT closeButton | " + button.getText());
			}
		}
	}

	static JCheckBox alwaysOpen(ComponentPresetChooserDialog dialog) {
		JComponent panel = (JComponent) dialog.getContentPane().getComponent(0);
		JCheckBox found = null;
		for (java.awt.Component c : panel.getComponents()) {
			if (c instanceof JCheckBox box) {
				found = box;
			}
		}
		return found;
	}

	static JButton closeButton(ComponentPresetChooserDialog dialog) {
		JComponent panel = (JComponent) dialog.getContentPane().getComponent(0);
		for (java.awt.Component c : panel.getComponents()) {
			if (c instanceof JButton button) {
				return button;
			}
		}
		throw new IllegalStateException("no close button");
	}

	static void scenario(int index, String name, OpenRocketDocument document, RocketComponent component, Setup setup)
			throws Exception {
		RecordingModel[] model = new RecordingModel[1];
		ComponentPresetChooserDialog dialog = open(name, document, component, setup, model);
		if (index == 0) {
			texts(dialog);
		}
		System.out.println("SCENARIO " + index + " | " + name + " | " + component.getComponentName() + " | "
				+ component.getPresetType() + " | " + setup.matchFore() + " | " + setup.matchAft() + " | "
				+ (setup.alwaysOpen() == null ? "default" : setup.alwaysOpen()));
		System.out.println(radii(component));
		ComponentPresetTable table = (ComponentPresetTable) field(dialog, "componentSelectionTable");
		XTableColumnModel columns = table.getXColumnModel();
		Enumeration<TableColumn> all = columns.getAllColumns();
		while (all.hasMoreElements()) {
			TableColumn column = all.nextElement();
			System.out.println("COLUMN " + column.getHeaderValue() + " | " + column.getModelIndex() + " | "
					+ columns.isColumnVisible(column));
		}
		System.out.println("INDEX " + field(dialog, "legacyColumnIndex") + " | " + field(dialog, "foreDiameterColumnIndex")
				+ " | " + field(dialog, "aftDiameterColumnIndex"));
		JCheckBox legacy = (JCheckBox) field(dialog, "showLegacyCheckBox");
		JCheckBox fore = (JCheckBox) field(dialog, "foreDiameterFilterCheckBox");
		JCheckBox aft = (JCheckBox) field(dialog, "aftDiameterFilterCheckBox");
		System.out.println(box("legacy", legacy));
		System.out.println(box("fore", fore));
		System.out.println(box("aft", aft));
		System.out.println(filter("fore", field(dialog, "foreDiameterFilter")));
		Object aftFilter = field(dialog, "aftDiameterFilter");
		System.out.println(filter("aft", aftFilter));
		if (component instanceof NoseCone nose && nose.isFlipped() && aftFilter != null) {
			SymmetricComponent previous = nose.getPreviousSymmetricComponent();
			double openRocket = (Double) filterField(aftFilter, "dvalue");
			double fixed = previous.getAftRadius() * 2.0;
			if (fixed != openRocket) {
				Constructor<ComponentPresetRowFilter> c = ComponentPresetRowFilter.class.getDeclaredConstructor(
						double.class, int.class);
				c.setAccessible(true);
				ComponentPresetRowFilter fixedFilter = c.newInstance(fixed, (Integer) field(dialog, "aftDiameterColumnIndex"));
				System.out.println("FIXED aft | " + hex(openRocket) + " | " + hex(fixed) + " | "
						+ hex((Double) filterField(fixedFilter, "epsilon")));
				setField(dialog, "aftDiameterFilter", fixedFilter);
				updateFilters(dialog);
			}
		}
		JCheckBox always = alwaysOpen(dialog);
		System.out.println("ALWAYS " + always.getText() + " | " + always.isSelected());

		step(dialog, "open", "");
		boolean foreShown = fore != null && fore.getParent() != null;
		boolean aftShown = aft != null && aft.getParent() != null;
		if (foreShown && fore.isSelected()) {
			fore.setSelected(false);
			step(dialog, "fore off", "");
		}
		if (aftShown && aft.isSelected()) {
			aft.setSelected(false);
			step(dialog, "aft off", "");
		}
		legacy.setSelected(true);
		step(dialog, "legacy on", "");
		JTextField text = (JTextField) field(dialog, "filterText");
		for (String t : TEXTS) {
			text.setText(t);
			step(dialog, "text", t);
		}
		text.setText("");
		step(dialog, "text", "");
		legacy.setSelected(false);
		step(dialog, "legacy off", "");
		if (foreShown) {
			fore.setSelected(true);
			step(dialog, "fore on", "");
		}
		if (aftShown) {
			aft.setSelected(true);
			step(dialog, "aft on", "");
		}
		System.out.println("PREFS " + preferences().isMatchForeDiameter() + " | " + preferences().isMatchAftDiameter());
		SwingUtilities.invokeAndWait(dialog::dispose);
	}

	/** The "tables" node's keys and values, a width as "cw". */
	static String tables() throws Exception {
		java.util.prefs.Preferences node = preferences().getTablePreferences();
		String[] keys = node.keys();
		Arrays.sort(keys);
		StringBuilder line = new StringBuilder();
		for (String key : keys) {
			line.append(line.length() == 0 ? "" : ";").append(key).append('=')
					.append(key.contains(".cw.") ? "cw" : node.get(key, null));
		}
		return line.toString();
	}

	static String digest(Object applied) {
		return applied == null ? "none" : ((ComponentPreset) applied).getDigest();
	}

	/**
	 * Close, Close without a selection, double clicks, "Always open this dialog" clicked, on a dialog
	 * whose match boxes start unticked (ACTIONS_SETUP).
	 */
	static void actions(int index, String name, OpenRocketDocument document, RocketComponent component)
			throws Exception {
		RecordingModel[] model = new RecordingModel[1];

		ComponentPresetChooserDialog dialog = open(name + ", close", document, component, Setup.ACTIONS, model);
		ComponentPresetTable table = (ComponentPresetTable) field(dialog, "componentSelectionTable");
		table.setRowSelectionInterval(3, 3);
		closeButton(dialog).doClick();
		System.out.println("ACTION " + index + " | close with view row 3 selected | " + digest(model[0].applied) + " | "
				+ tables());
		SwingUtilities.invokeAndWait(dialog::dispose);

		dialog = open(name + ", close none", document, component, Setup.ACTIONS, model);
		closeButton(dialog).doClick();
		System.out.println("ACTION " + index + " | close without a selection | " + digest(model[0].applied) + " | "
				+ tables());
		SwingUtilities.invokeAndWait(dialog::dispose);

		for (int column : new int[] { 0, 2 }) {
			dialog = open(name + ", double click " + column, document, component, Setup.ACTIONS, model);
			ComponentPresetTable t = (ComponentPresetTable) field(dialog, "componentSelectionTable");
			t.setRowSelectionInterval(5, 5);
			t.setColumnSelectionInterval(column, column);
			java.awt.Rectangle cell = t.getCellRect(5, column, false);
			t.dispatchEvent(new java.awt.event.MouseEvent(t, java.awt.event.MouseEvent.MOUSE_CLICKED,
					System.currentTimeMillis(), 0, cell.x + cell.width / 2, cell.y + cell.height / 2, 2, false,
					java.awt.event.MouseEvent.BUTTON1));
			System.out.println("ACTION " + index + " | double click on view row 5, view column " + column + " | "
					+ digest(model[0].applied) + " | " + tables());
			SwingUtilities.invokeAndWait(dialog::dispose);
		}

		dialog = open(name + ", always open", document, component, Setup.ACTIONS, model);
		JCheckBox always = alwaysOpen(dialog);
		always.doClick();
		String key = component.getComponentName() + "AlwaysOpenPreset";
		System.out.println("ACTION " + index + " | always open clicked | " + digest(model[0].applied) + " | " + key + "="
				+ preferences().getPreferences().get(key, "unset") + " (box " + always.isSelected() + ")");
		always.doClick();
		System.out.println("ACTION " + index + " | always open clicked again | " + digest(model[0].applied) + " | " + key
				+ "=" + preferences().getPreferences().get(key, "unset") + " (box " + always.isSelected() + ")");
		SwingUtilities.invokeAndWait(dialog::dispose);
	}

	/** A component's descriptor in the focus order: its class and its text. */
	static String descriptor(java.awt.Component c) {
		if (c instanceof javax.swing.AbstractButton b) {
			return c.getClass().getSimpleName() + " " + b.getText();
		}
		return c.getClass().getSimpleName();
	}

	/** The table's state: the selected row and column and the leads. */
	static String cell(JTable table) {
		return table.getSelectedRow() + " | " + table.getSelectedColumn() + " | "
				+ table.getSelectionModel().getLeadSelectionIndex() + " | "
				+ table.getColumnModel().getSelectionModel().getLeadSelectionIndex();
	}

	/** Runs the table's binding of @p stroke (WHEN_ANCESTOR_OF_FOCUSED_COMPONENT) as the key would. */
	static void press(JTable table, KeyStroke stroke) {
		Object name = table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT).get(stroke);
		Action action = table.getActionMap().get(name);
		action.actionPerformed(new ActionEvent(table, ActionEvent.ACTION_PERFORMED, String.valueOf(name)));
	}

	/**
	 * The dialog as a window (its size for an owner whose owner is 1000 x 800, its default button,
	 * Escape, the focus order of its LayoutFocusTraversalPolicy once it is shown) and the table's Enter and Shift+Enter
	 * (JTable's selectNextRowCell and selectPreviousRowCell, run through its input and action maps).
	 * Printed: DIALOG key | value; FOCUS index | descriptor (the first is the default component, which
	 * has the focus when the dialog opens); KEY start row | start view column | key | selected row |
	 * selected column | row lead | column lead (a start of -1 | -1: a new dialog, nothing selected;
	 * "empty": a table without rows).
	 */
	static void window(int index, String name, OpenRocketDocument document, RocketComponent component)
			throws Exception {
		RecordingModel[] model = new RecordingModel[1];
		ComponentPresetChooserDialog dialog = open(name + ", window", document, component, Setup.ACTIONS, model);
		System.out.println("DIALOG scenario | " + index);
		System.out.println("DIALOG owner | " + config.getOwner().getWidth() + "x" + config.getOwner().getHeight());
		System.out.println("DIALOG size | " + dialog.getWidth() + "x" + dialog.getHeight());
		System.out.println("DIALOG defaultButton | " + dialog.getRootPane().getDefaultButton().getText());
		Object escape = dialog.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
				.get(KeyStroke.getKeyStroke(KeyEvent.VK_ESCAPE, 0));
		System.out.println("DIALOG escapeCloses | " + (escape != null));
		// The focus order is the layout's, which a window only has once it is shown: shown here (on
		// xvfb's virtual screen) without its modality, so that setVisible() returns.
		ComponentPresetChooserDialog shown = dialog;
		SwingUtilities.invokeAndWait(() -> {
			shown.setModalityType(java.awt.Dialog.ModalityType.MODELESS);
			shown.setVisible(true);
		});
		java.awt.FocusTraversalPolicy policy = dialog.getFocusTraversalPolicy();
		java.awt.Component first = policy.getDefaultComponent(dialog);
		java.awt.Component c = first;
		int i = 0;
		do {
			System.out.println("FOCUS " + i++ + " | " + descriptor(c));
			c = policy.getComponentAfter(dialog, c);
		} while (c != null && c != first && i < 20);
		JTable table = (JTable) field(dialog, "componentSelectionTable");
		KeyStroke enter = KeyStroke.getKeyStroke(KeyEvent.VK_ENTER, 0);
		KeyStroke shiftEnter = KeyStroke.getKeyStroke(KeyEvent.VK_ENTER, InputEvent.SHIFT_DOWN_MASK);
		int last = table.getRowCount() - 1;
		int lastColumn = table.getColumnCount() - 1;
		System.out.println("DIALOG table | " + table.getRowCount() + "x" + table.getColumnCount());
		press(table, enter);
		System.out.println("KEY -1 | -1 | enter | " + cell(table));
		SwingUtilities.invokeAndWait(dialog::dispose);
		dialog = open(name + ", window shift enter", document, component, Setup.ACTIONS, model);
		table = (JTable) field(dialog, "componentSelectionTable");
		press(table, shiftEnter);
		System.out.println("KEY -1 | -1 | shift enter | " + cell(table));
		int[][] starts = { { 2, 3, 0 }, { 2, 3, 1 }, { last, 3, 0 }, { 0, 3, 1 }, { last, lastColumn, 0 },
				{ 0, 0, 1 }, { 5, 0, 0 } };
		for (int[] start : starts) {
			table.changeSelection(start[0], start[1], false, false);
			press(table, start[2] == 0 ? enter : shiftEnter);
			System.out.println("KEY " + start[0] + " | " + start[1] + " | " + (start[2] == 0 ? "enter" : "shift enter")
					+ " | " + cell(table));
		}
		// A table without rows: the filter text matches nothing.
		String nothing = "no preset has this text";
		((JTextField) field(dialog, "filterText")).setText(nothing);
		System.out.println("DIALOG emptyText | " + nothing);
		System.out.println("DIALOG emptyRows | " + table.getRowCount());
		press(table, enter);
		System.out.println("KEY empty | -1 | enter | " + cell(table));
		SwingUtilities.invokeAndWait(dialog::dispose);
	}

	public static void main(String[] args) throws Exception {
		bootstrap(args[0]);

		// The scouting's design: every kind that takes presets.
		DeterministicUuids.reseed("preset-chooser design");
		OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
		AxialStage stage = document.getRocket().getStage(0);
		NoseCone nose = new NoseCone();
		stage.addChild(nose);
		BodyTube front = new BodyTube();
		front.setOuterRadiusAutomatic(false);
		front.setOuterRadius(0.0124);
		front.setLength(0.3);
		stage.addChild(front);
		Transition transition = new Transition();
		stage.addChild(transition);
		BodyTube back = new BodyTube();
		back.setOuterRadiusAutomatic(false);
		back.setOuterRadius(0.0208);
		back.setLength(0.3);
		stage.addChild(back);
		NoseCone tail = new NoseCone();
		stage.addChild(tail);
		tail.setFlipped(true);
		InnerTube innerTube = new InnerTube();
		front.addChild(innerTube);
		CenteringRing ring = new CenteringRing();
		front.addChild(ring);
		Bulkhead bulkhead = new Bulkhead();
		front.addChild(bulkhead);
		EngineBlock block = new EngineBlock();
		front.addChild(block);
		TubeCoupler coupler = new TubeCoupler();
		front.addChild(coupler);
		Parachute parachute = new Parachute();
		front.addChild(parachute);
		Streamer streamer = new Streamer();
		front.addChild(streamer);
		LaunchLug lug = new LaunchLug();
		front.addChild(lug);
		RailButton button = new RailButton();
		front.addChild(button);
		TubeFinSet tubeFins = new TubeFinSet();
		back.addChild(tubeFins);

		// A flipped nose cone right behind a transition of other radii (bug 16).
		DeterministicUuids.reseed("preset-chooser bug 16 design");
		OpenRocketDocument second = OpenRocketDocumentFactory.createNewRocket();
		AxialStage secondStage = second.getRocket().getStage(0);
		NoseCone secondNose = new NoseCone();
		secondStage.addChild(secondNose);
		BodyTube secondTube = new BodyTube();
		secondTube.setOuterRadiusAutomatic(false);
		secondTube.setOuterRadius(0.0124);
		secondTube.setLength(0.3);
		secondStage.addChild(secondTube);
		Transition secondTransition = new Transition();
		secondStage.addChild(secondTransition);
		secondTransition.setAftRadiusAutomatic(false);
		secondTransition.setAftRadius(0.0208);
		NoseCone secondTail = new NoseCone();
		secondStage.addChild(secondTail);
		secondTail.setFlipped(true);

		Object[][] scenarios = { { "nose cone before a 24.8 mm tube", document, nose, Setup.DEFAULT },
				{ "body tube 24.8 mm after the nose", document, front, Setup.DEFAULT },
				{ "transition 24.8 -> 41.6 mm", document, transition, Setup.DEFAULT },
				{ "body tube 41.6 mm after the transition", document, back, Setup.DEFAULT },
				{ "flipped nose cone at the aft end", document, tail, Setup.DEFAULT },
				{ "inner tube", document, innerTube, Setup.DEFAULT },
				{ "centering ring", document, ring, Setup.DEFAULT },
				{ "bulkhead", document, bulkhead, Setup.DEFAULT },
				{ "engine block", document, block, Setup.DEFAULT },
				{ "tube coupler", document, coupler, Setup.DEFAULT },
				{ "launch lug", document, lug, Setup.DEFAULT },
				{ "rail button", document, button, Setup.DEFAULT },
				{ "streamer", document, streamer, Setup.DEFAULT },
				{ "parachute", document, parachute, Setup.DEFAULT },
				{ "tube fin set", document, tubeFins, Setup.DEFAULT },
				{ "flipped nose cone behind a transition", second, secondTail, Setup.DEFAULT },
				{ "body tube 24.8 mm, the match boxes and 'always open' off", document, front,
						new Setup(false, false, Boolean.FALSE) },
				{ "transition, only the aft box on, 'always open' on", document, transition,
						new Setup(false, true, Boolean.TRUE) } };
		for (int i = 0; i < scenarios.length; i++) {
			Object[] s = scenarios[i];
			scenario(i, (String) s[0], (OpenRocketDocument) s[1], (RocketComponent) s[2], (Setup) s[3]);
		}
		actions(1, "body tube 24.8 mm after the nose", document, front);
		window(1, "body tube 24.8 mm after the nose", document, front);
		SwingUtilities.invokeAndWait(() -> {
			config.dispose();
			frame.dispose();
		});
		System.exit(0);
	}
}
