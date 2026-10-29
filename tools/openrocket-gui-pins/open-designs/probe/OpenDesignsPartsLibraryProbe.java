package probe;

import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.adaptors.PresetModel;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetRowFilter;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetTable;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Window;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.table.TableColumn;
import javax.swing.table.TableModel;

/**
 * OpenRocket's parts library (ComponentPresetChooserDialog, swing gui/dialogs/preset/ComponentPresetChooserDialog.java)
 * as it opens from the configuration dialog of each component of the sixteen example designs that takes presets
 * (getPresetType() != null: RocketComponentConfig's "Parts Library" button, selectPreset()), over the bundled presets:
 * what QtRocket's exit suite OpenDesignsExitGolden (tests/gui/exit/open_designs_exit_tests.cpp, plan section 10.5;
 * the tests-exit scout's E4) compares the parts library of each component's dialog with, through
 * gen_open_designs_parts_library_pins.py into tests/gui/exit/OpenDesignsPartsLibraryPins.h.
 * <p>
 * The designs are the lines of kind "example" of designs.py's list (the first argument), each loaded with the motors
 * of the bundled database (-Dprobe.motors) and the bundled presets (every .orc file below the directory of the second
 * argument, depth first, each directory in name order, as QtRocket's DirectoryIterator walks it, each file's presets
 * added with Database.addAll(): the preset-chooser family's walk), bound as the application's ComponentPresetDao, so
 * that the parts library lists them and the designs' six presets are found among them (as QtRocket's
 * DesignFileEnvironment::Presets::BUNDLED loads them). The preferences are SwingPreferences in generate.sh's fresh
 * store, at their defaults (the "default filters": MatchForeDiameter and MatchAftDiameter true, "Show Legacy Database"
 * unticked, no text, no stored table layout), put back before each dialog; FlatLaf Light. Each design's UUIDs are
 * seeded (DeterministicUuids, reseeded from the design's name).
 * <p>
 * Each design is settled as the golden harness settles it (tools/openrocket-goldens' AutomaticDimensions, as the
 * motor-chooser family does) before its first dialog. OpenRocket computes automatic dimensions lazily and keeps
 * them, so that right after a load a body tube's automatic radius can still be the value the file stores ("Dual
 * parachute deployment": the first body tube's "auto 0.025", where the settled design has the nose cone's
 * 0.028321, which the "Match aft diameter" filter of the nose cone's parts library compares with). No user opens
 * a parts library on a design in that state: OpenRocket's window paints the design's figure and its overlay as it
 * opens, before any configuration dialog, which refreshes them (a scratch probe of part Y2: right after the load the
 * nose cone's filter would compare with 0.05 m, after a RocketFigure painted and RocketInfoContextHelper's physics
 * with 0.056642 m, the settled value), and QtRocket's suite compares its windows' dialogs and overlay with the
 * goldens, which describe the settled design.
 * <p>
 * Each dialog is made as RocketComponentConfig.selectPreset() makes it, for the component, its owner a configuration
 * dialog whose owner is a main window, and never shown (a JDialog, which a headless AWT refuses to make, so the probe
 * runs under xvfb-run). Its PresetModel records what the dialog would apply instead of loading it (nothing is
 * applied: the probe closes no dialog with Close). For a flipped nose cone whose previous component's fore and aft
 * radii differ, the aft filter is replaced (before the first view, by reflection) by QtRocket's fixed one, the previous
 * component's aft diameter (bug 16 of QtRocket's plan, section 10.3), so that the views are QtRocket's, as the
 * preset-chooser family's PresetChooserProbe does.
 * <p>
 * Output: one line each, its fields separated by tabs (every double Double.toHexString):
 * <ul>
 * <li>DATABASE files read kept (the bundled presets)</li>
 * <li>D name (an example design of designs.py, in its order)</li>
 * <li>C index name componentName presetType preset: a component that takes presets, in the rocket's iteration order
 * (index: its place in that order, the rocket 0), its name, getComponentName(), getPresetType() and what its
 * configuration dialog's preset combo box shows when closed (PresetModel.getSelectedItem(): the component's preset by
 * its toString(), the part number; empty for "Custom")</li>
 * <li>L text selected filter (the other default filters: the "Show Legacy Database" box, its text and whether it is
 * ticked, and the text of the "Filter by text" field)</li>
 * <li>B fore|aft text tooltip checked (a diameter box the dialog shows), or "B fore|aft none" (none, or one it makes
 * and never adds)</li>
 * <li>F fore|aft value epsilon (the box's ComponentPresetRowFilter: its dvalue and epsilon), or "F fore|aft none"</li>
 * <li>X aft openRocket fixed fixedEpsilon (bug 16: OpenRocket's value, the previous component's fore diameter, and the
 * fixed one, which the F line then holds)</li>
 * <li>A text selected (the "Always open this dialog when creating a new ..." box)</li>
 * <li>V open view (the table as the dialog opens: "rows &lt;n&gt; md5 &lt;hex&gt;", its rows and the MD5 of their
 * presets' keys in view order, each followed by '\n', a key the preset's digest but for the six nose cones whose
 * digest is not the same on every platform, viewKey()); then, when the dialog shows a ticked diameter box, "V
 * unmatched view" with every shown diameter box unticked (the boxes are ticked again afterwards)</li>
 * <li>R row text... (the first FIRST_ROWS rows as the dialog opens: the texts of their shown cells in view order,
 * but the favourite's check box, as the cells' toString(), which the table shows)</li>
 * </ul>
 * usage: OpenDesignsPartsLibraryProbe DESIGNS COMPONENTS
 */
public class OpenDesignsPartsLibraryProbe {

	/** The rows of a table the probe prints. */
	static final int FIRST_ROWS = 5;

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

	public static void main(String[] args) throws Exception {
		int status = 0;
		try {
			bootstrap(new File(args[1]));
			for (String design : Files.readAllLines(Path.of(args[0]), StandardCharsets.UTF_8)) {
				String[] fields = design.split("\t");
				if (fields.length != 3) {
					throw new IllegalStateException("not a design: " + design);
				}
				if (fields[1].equals("example")) {
					probe(fields[0], new File(fields[2]));
				}
			}
			SwingUtilities.invokeAndWait(() -> {
				for (Window window : Window.getWindows()) {
					window.dispose();
				}
			});
		} catch (Throwable e) {
			e.printStackTrace();
			status = 1;
		}
		System.out.flush();
		System.exit(status);
	}

	static void bootstrap(File components) throws Exception {
		// Before anything makes a UUID: each design gets the same UUIDs on every run.
		DeterministicUuids.install();
		DeterministicUuids.reseed("open-designs-parts-library");
		// The motors as OpenDesignsTabProbe reads them (from a copy of the bundled database), and the bundled
		// presets, which the .orc reader reads with the application's injector (its materials).
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile())) {
			motors.addMotor(motor);
		}
		database = new ComponentPresetDatabase();
		Application.setInjector(OpenDesignsTabProbe.injector(motors, database));
		List<File> files = new ArrayList<>();
		OpenDesignsTabProbe.collectOrcFiles(components, files);
		int read = 0;
		for (File file : files) {
			try (InputStream in = new FileInputStream(file)) {
				Collection<ComponentPreset> loaded = new OpenRocketComponentLoader().load(in, file.getName());
				read += loaded.size();
				database.addAll(loaded);
			}
		}
		line("DATABASE", files.size(), read, database.size());
		SwingUtilities.invokeAndWait(() -> {
			GUIUtil.setUITheme(UITheme.Themes.LIGHT);
			frame = new JFrame();
			frame.setSize(1000, 800);
			config = new JDialog(frame);
		});
	}

	/** Every preference the dialog reads or writes back at its default. */
	static void resetPreferences(RocketComponent component) {
		SwingPreferences p = (SwingPreferences) Application.getPreferences();
		p.setMatchForeDiameter(true);
		p.setMatchAftDiameter(true);
		p.putString(component.getComponentName() + "AlwaysOpenPreset", null);
		try {
			p.getTablePreferences().clear();
			p.getWindowsPreferences().clear();
		} catch (java.util.prefs.BackingStoreException e) {
			throw new IllegalStateException(e);
		}
	}

	static void probe(String name, File file) throws Exception {
		DeterministicUuids.reseed(name);
		OpenRocketDocument document = new GeneralRocketLoader(file).load();
		settle(document.getRocket());
		line("D", name);
		int index = 0;
		for (RocketComponent component : document.getRocket()) {
			if (component.getPresetType() != null) {
				int at = index;
				SwingUtilities.invokeAndWait(() -> {
					try {
						dialog(at, document, component);
					} catch (Exception e) {
						throw new RuntimeException(e);
					}
				});
			}
			++index;
		}
	}

	static Object field(Object object, String name) throws Exception {
		Field f = ComponentPresetChooserDialog.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	static Object filterField(Object filter, String name) throws Exception {
		Field f = ComponentPresetRowFilter.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(filter);
	}

	static String hex(double value) {
		return Double.toHexString(value);
	}

	/** Whether the dialog shows @p box (made and added). */
	static boolean shown(JCheckBox box) {
		return box != null && box.getParent() != null;
	}

	static void box(String name, JCheckBox box) {
		if (!shown(box)) {
			line("B", name, "none");
		} else {
			line("B", name, box.getText(), String.valueOf(box.getToolTipText()), box.isSelected());
		}
	}

	static void filter(String name, Object filter, JCheckBox box) throws Exception {
		if (!shown(box) || filter == null) {
			line("F", name, "none");
		} else {
			line("F", name, hex((Double) filterField(filter, "dvalue")), hex((Double) filterField(filter, "epsilon")));
		}
	}

	/** The line digestOfView() hashes for a preset (ComponentPresetTableProbe.viewKey(), QtRocket's presetViewKey()). */
	static String viewKey(ComponentPreset preset) {
		if (preset.getType() == ComponentPreset.Type.NOSE_CONE && preset.has(ComponentPreset.SHAPE)
				&& preset.has(ComponentPreset.MASS)) {
			Transition.Shape shape = preset.get(ComponentPreset.SHAPE);
			if (shape == Transition.Shape.HAACK || shape == Transition.Shape.POWER) {
				return preset.getType().name() + "|" + preset.getManufacturer().getSimpleName() + "|"
						+ preset.getPartNo();
			}
		}
		return preset.getDigest();
	}

	/**
	 * "rows &lt;n&gt; md5 &lt;hex&gt;": the rows and the MD5 of their keys (viewKey()) in view order
	 * (ComponentPresetTableProbe.digestOfView(), QtRocket's presetDigestOfView()).
	 */
	static String view(ComponentPresetTable table, List<ComponentPreset> presets) throws Exception {
		StringBuilder text = new StringBuilder();
		for (int i = 0; i < table.getRowCount(); i++) {
			text.append(viewKey(presets.get(table.convertRowIndexToModel(i)))).append('\n');
		}
		MessageDigest md5 = MessageDigest.getInstance("MD5");
		StringBuilder hex = new StringBuilder();
		for (byte b : md5.digest(text.toString().getBytes(StandardCharsets.UTF_8))) {
			hex.append(String.format("%02x", b));
		}
		return "rows " + table.getRowCount() + " md5 " + hex;
	}

	/** The first rows: the texts of their shown cells in view order, but the favourite's (model column 0). */
	static void rows(ComponentPresetTable table) {
		TableModel model = table.getModel();
		for (int row = 0; row < Math.min(FIRST_ROWS, table.getRowCount()); row++) {
			int modelRow = table.convertRowIndexToModel(row);
			List<Object> fields = new ArrayList<>();
			fields.add("R");
			fields.add(row);
			for (int c = 0; c < table.getColumnCount(); c++) {
				TableColumn column = table.getColumnModel().getColumn(c);
				if (column.getModelIndex() == 0) {
					continue;
				}
				Object value = model.getValueAt(modelRow, column.getModelIndex());
				fields.add(value == null ? "" : value.toString());
			}
			line(fields.toArray());
		}
	}

	/** The "Always open this dialog" box: the last check box of the dialog's panel. */
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

	static void dialog(int index, OpenRocketDocument document, RocketComponent component) throws Exception {
		resetPreferences(component);
		RecordingModel model = new RecordingModel(document, component);
		Object shownPreset = model.getSelectedItem();
		line("C", index, component.getName(), component.getComponentName(), component.getPresetType(),
				shownPreset instanceof ComponentPreset preset ? preset.toString() : "");
		ComponentPresetChooserDialog dialog = new ComponentPresetChooserDialog(config, component, model);
		ComponentPresetTable table = (ComponentPresetTable) field(dialog, "componentSelectionTable");
		@SuppressWarnings("unchecked")
		List<ComponentPreset> presets = (List<ComponentPreset>) field(dialog, "presets");
		JCheckBox fore = (JCheckBox) field(dialog, "foreDiameterFilterCheckBox");
		JCheckBox aft = (JCheckBox) field(dialog, "aftDiameterFilterCheckBox");
		JCheckBox legacy = (JCheckBox) field(dialog, "showLegacyCheckBox");
		line("L", legacy.getText(), legacy.isSelected(), ((JTextField) field(dialog, "filterText")).getText());
		box("fore", fore);
		box("aft", aft);
		// Bug 16: a flipped nose cone's base meets the previous component's aft end.
		Object aftFilter = field(dialog, "aftDiameterFilter");
		String fixedLine = null;
		if (component instanceof NoseCone nose && nose.isFlipped() && shown(aft) && aftFilter != null) {
			SymmetricComponent previous = nose.getPreviousSymmetricComponent();
			double openRocket = (Double) filterField(aftFilter, "dvalue");
			double fixed = previous.getAftRadius() * 2.0;
			if (fixed != openRocket) {
				Constructor<ComponentPresetRowFilter> c = ComponentPresetRowFilter.class.getDeclaredConstructor(
						double.class, int.class);
				c.setAccessible(true);
				ComponentPresetRowFilter fixedFilter = c.newInstance(fixed, (Integer) field(dialog,
						"aftDiameterColumnIndex"));
				fixedLine = hex(openRocket) + "\t" + hex(fixed) + "\t" + hex((Double) filterField(fixedFilter, "epsilon"));
				Field f = ComponentPresetChooserDialog.class.getDeclaredField("aftDiameterFilter");
				f.setAccessible(true);
				f.set(dialog, fixedFilter);
				Method update = ComponentPresetChooserDialog.class.getDeclaredMethod("updateFilters");
				update.setAccessible(true);
				update.invoke(dialog);
				aftFilter = fixedFilter;
			}
		}
		filter("fore", field(dialog, "foreDiameterFilter"), fore);
		filter("aft", aftFilter, aft);
		if (fixedLine != null) {
			System.out.println("X\taft\t" + fixedLine);
		}
		JCheckBox always = alwaysOpen(dialog);
		line("A", always.getText(), always.isSelected());
		line("V", "open", view(table, presets));
		rows(table);
		boolean foreTicked = shown(fore) && fore.isSelected();
		boolean aftTicked = shown(aft) && aft.isSelected();
		if (foreTicked || aftTicked) {
			if (foreTicked) {
				fore.setSelected(false);
			}
			if (aftTicked) {
				aft.setSelected(false);
			}
			line("V", "unmatched", view(table, presets));
			if (foreTicked) {
				fore.setSelected(true);
			}
			if (aftTicked) {
				aft.setSelected(true);
			}
		}
		if (model.applied != null) {
			throw new IllegalStateException("the dialog applied a preset");
		}
		dialog.dispose();
	}

	// ---- settling (tools/openrocket-goldens' AutomaticDimensions, as the other families settle)

	static void settle(Rocket rocket) {
		List<RocketComponent> comps = new ArrayList<>();
		for (RocketComponent c : rocket) {
			comps.add(c);
		}
		List<Object> previous = snapshot(comps);
		for (int pass = 0; pass < 20; pass++) {
			List<Object> current = snapshot(comps);
			if (current.equals(previous)) {
				return;
			}
			previous = current;
		}
		throw new IllegalStateException("did not settle: " + rocket.getName());
	}

	static List<Object> snapshot(List<RocketComponent> comps) {
		List<Object> values = new ArrayList<>();
		for (RocketComponent c : comps) {
			if (c instanceof BodyTube tube) {
				values.add(tube.getOuterRadius());
				values.add(tube.getInnerRadius());
			}
			if (c instanceof SymmetricComponent s) {
				values.add(s.getForeRadius());
				values.add(s.getAftRadius());
				values.add(s.usesPreviousCompAutomatic());
				values.add(s.usesNextCompAutomatic());
			}
			if (c instanceof RingComponent ring) {
				values.add(ring.getOuterRadius());
				values.add(ring.getInnerRadius());
			}
			if (c instanceof MassObject mass) {
				values.add(mass.getRadius());
				values.add(mass.getLength());
			}
		}
		return values;
	}

	/** Prints @p fields separated by tabs (texts hold " | " and spaces, never a tab). */
	static void line(Object... fields) {
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
		System.out.println(text);
	}
}
