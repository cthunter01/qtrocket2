package probe;

import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.TreeMap;

import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;
import javax.swing.table.TableModel;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.MotorFilterPanel;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorSelectionPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.gui.widgets.MultiSlider;

/**
 * The motor chooser opened for every motor mount of OpenRocket's 16 example designs (the family
 * "motor-chooser" of tools/openrocket-gui-pins, M3 run 2 part M4): what the chooser of a design
 * window shows when MotorConfigurationPanel.selectMotor() opens it for each acting motor mount in
 * each flight configuration, with "Limit motor diameter to mount diameter" and "Limit motor length
 * to mount length" on (the mount's limits applied), and what its OK answers.
 * <p>
 * usage: MotorChooserMountsProbe EXAMPLES_DIRECTORY (the motor database: -Dprobe.motors)
 * <p>
 * Each design is loaded without component presets (GeneralRocketLoader) and settled as the golden
 * harness settles it (tools/openrocket-goldens' AutomaticDimensions), with the preferences at their
 * defaults but for the two limit boxes ("motorFilterLimitDiameter", "motorFilterLimitLength" true)
 * and the default units. One chooser serves the design (a design window has one, its
 * MotorConfigurationPanel's MotorChooserDialog): OpenRocket's real ThrustCurveMotorSelectionPanel,
 * headless (the dialog is a JDialog, which a headless AWT refuses; it adds open(), which calls
 * selectMotorFromTable() before it shows the panel, and close(true), which this probe does as it
 * does). It is opened for every flight configuration of the design (Rocket.getIds()) and, in each,
 * for every component that is an acting motor mount (MotorMount.isMotorMount()), in the order of the
 * component tree: setMotorMountAndConfig(fcid, mount), then selectMotorFromTable(), then the state is
 * printed; at a mount's first open the four searches are typed into the search field and it is
 * emptied again; then OK (close(true): selectedMotor() with the selected motor) and its answer. The
 * answer is not written into the design (that is the Motors tab's part). The rows of the table:
 * the model rows in the view's order and the MD5 of the rows' "model row, tab, the six column texts,
 * line feed" (UTF-8), as tools/openrocket-motor-chooser's chooser family hashes them.
 * <p>
 * The designs are run twice. The second run ("FIXED" lines) does the same with one change made from
 * outside to OpenRocket's real panel, the fix QtRocket makes (forgetTableSelection()): opened for a
 * configuration with a motor, the chooser forgets the table row an earlier open selected.
 * OpenRocket keeps that row selected while setMotorMountAndConfig() sorts the table for the new
 * mount; when the mount's limits hide the configured motor's set, the row stays selected and its
 * selection listener selects its set instead of the configured motor (an OK then puts that other
 * motor into the mount). Doubles are printed exactly, so the generator keeps of the second run
 * what differs from the first.
 * <p>
 * Each line is a kind and tab-separated fields (gen_motor_chooser_pins.py reads them; the second
 * run's kinds have the prefix FIXED):
 * <ul>
 * <li>DESIGN file configurations mounts</li>
 * <li>MOUNT file mount name diameter length: the acting motor mounts in tree order</li>
 * <li>OPEN file configuration mount configured-set configured-curve configured-delay
 * configured-delay-text configured-nozzle limit-diameter limit-length diameter-thumbs length-thumbs
 * min-diameter max-diameter min-length max-length mount-label motors-label rows md5 selected-row
 * selected-set selected-curve curves-enabled curve-set curves curve-index delay-items delay-text
 * delay nozzle nozzle-maximum</li>
 * <li>ROWS md5 rows model-rows: each table of an OPEN or SEARCH line, once</li>
 * <li>SEARCH file configuration mount text rows md5</li>
 * <li>ANSWER file configuration mount set curve delay nozzle: the chooser's OK (set and curve -1
 * without a motor)</li>
 * <li>PREFERRED file digest value: the node of the preferred curves after the design's opens, its
 * keys sorted</li>
 * </ul>
 * Doubles are Double.toHexString (exact), null a limit that is none; a set is its index in the
 * database's sets, a curve its index in its set; lists are joined by ',' (indices) or '|' (texts).
 * The UUIDs are seeded (DeterministicUuids, reseeded per design), as every probe of Milestone 3
 * does.
 */
public final class MotorChooserMountsProbe {

	/** The searches typed at a mount's first open (the tests-exit scout's E3). */
	private static final String[] SEARCHES = { "C6", "estes c6", "H128", "aerotech 38/240" };

	/** The delay box's text for a plugged delay (TCMotorSelPan.delayBox.Plugged). */
	private static final String PLUGGED = "Plugged";

	private final ThrustCurveMotorSetDatabase db;
	private final PrintStream out;
	private final Set<String> printedRows = new HashSet<>();

	private ThrustCurveMotorSelectionPanel panel;
	private MotorFilterPanel filterPanel;
	/** Whether this is the second run (QtRocket's fix made from outside), and its lines' prefix. */
	private boolean fixed;
	private String prefix = "";

	private MotorChooserMountsProbe(ThrustCurveMotorSetDatabase db, PrintStream out) {
		this.db = db;
		this.out = out;
	}

	public static void main(String[] args) throws Exception {
		DeterministicUuids.install();
		System.setProperty("openrocket.3d.disable", "true");
		ThrustCurveMotorSetDatabase motors = bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		File dir = new File(args[0]);
		String[] examples = dir.list((d, n) -> n.endsWith(".ork"));
		java.util.Arrays.sort(examples);
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		MotorChooserMountsProbe probe = new MotorChooserMountsProbe(motors, out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				for (boolean fixed : new boolean[] { false, true }) {
					probe.fixed = fixed;
					probe.prefix = fixed ? "FIXED" : "";
					for (String name : examples) {
						probe.design(dir, name);
					}
				}
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

	/** The Guice injector OpenRocket's tests use, with SwingPreferences and the motor database. */
	private static ThrustCurveMotorSetDatabase bootstrap() throws Exception {
		Path bundled = Path.of(System.getProperty("probe.motors"));
		Path copy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundled, copy, StandardCopyOption.REPLACE_EXISTING);
		copy.toFile().deleteOnExit();
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor motor : ThrustCurveMotorSQLiteDatabase.readDatabase(copy.toFile())) {
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
		return motors;
	}

	// ---- helpers

	private static Object get(Object o, String name) throws Exception {
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

	/** Calls the method @p name without arguments, also a package-private one (MotorHolder's, MotorRowFilter's). */
	private static Object call(Object o, String name) throws Exception {
		Class<?> c = o.getClass();
		while (c != null) {
			try {
				Method method = c.getDeclaredMethod(name);
				method.setAccessible(true);
				return method.invoke(o);
			} catch (NoSuchMethodException e) {
				c = c.getSuperclass();
			}
		}
		throw new NoSuchMethodException(name);
	}

	private static String hex(double value) {
		return Double.toHexString(value);
	}

	private static String hex(Double value) {
		return value == null ? "null" : Double.toHexString(value);
	}

	private int setIndex(ThrustCurveMotorSet set) {
		return set == null ? -1 : db.getMotorSets().indexOf(set);
	}

	private ThrustCurveMotorSet setOf(Motor motor) {
		for (ThrustCurveMotorSet set : db.getMotorSets()) {
			if (set.getMotors().contains(motor)) {
				return set;
			}
		}
		return null;
	}

	private int curveIndex(Motor motor) {
		ThrustCurveMotorSet set = setOf(motor);
		return set == null ? -1 : set.getMotors().indexOf(motor);
	}

	private JTable table() throws Exception {
		return (JTable) get(panel, "table");
	}

	/** The table's rows: the model rows in the view's order and the MD5 of their texts. */
	private String[] rows() throws Exception {
		JTable table = table();
		TableModel model = table.getModel();
		StringBuilder all = new StringBuilder();
		StringBuilder modelRows = new StringBuilder();
		for (int r = 0; r < table.getRowCount(); r++) {
			int m = table.convertRowIndexToModel(r);
			all.append(m);
			for (int c = 0; c < model.getColumnCount(); c++) {
				all.append('\t').append(model.getValueAt(m, c));
			}
			all.append('\n');
			if (r > 0) {
				modelRows.append(',');
			}
			modelRows.append(m);
		}
		MessageDigest md = MessageDigest.getInstance("MD5");
		StringBuilder digest = new StringBuilder();
		for (byte b : md.digest(all.toString().getBytes(StandardCharsets.UTF_8))) {
			digest.append(String.format("%02x", b));
		}
		String md5 = digest.toString();
		if (printedRows.add(md5)) {
			out.println(prefix + "ROWS\t" + md5 + "\t" + table.getRowCount() + "\t" + modelRows);
		}
		return new String[] { Integer.toString(table.getRowCount()), md5 };
	}

	private String thumbs(String slider) throws Exception {
		MultiSlider s = (MultiSlider) get(filterPanel, slider);
		return s.getValueAt(0) + "," + s.getValueAt(1);
	}

	/** The curve list: the set of its curves and their indices in it, in its order. */
	private String[] curves() throws Exception {
		JComboBox<?> box = (JComboBox<?>) get(panel, "curveSelectionBox");
		ThrustCurveMotorSet set = null;
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < box.getItemCount(); i++) {
			Object holder = box.getItemAt(i);
			if ((Integer) call(holder, "getIndex") != i) {
				throw new IllegalStateException("a curve item's index is not its position");
			}
			ThrustCurveMotor motor = (ThrustCurveMotor) call(holder, "getMotor");
			ThrustCurveMotorSet itemSet = setOf(motor);
			if (set == null) {
				set = itemSet;
			} else if (set != itemSet) {
				throw new IllegalStateException("the curve list holds the curves of two sets");
			}
			if (i > 0) {
				sb.append(',');
			}
			sb.append(itemSet.getMotors().indexOf(motor));
		}
		return new String[] { Boolean.toString(box.isEnabled()), Integer.toString(setIndex(set)), sb.toString(),
				Integer.toString(box.getSelectedIndex()) };
	}

	private String[] delays() throws Exception {
		@SuppressWarnings("unchecked")
		JComboBox<String> box = (JComboBox<String>) get(panel, "delayBox");
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < box.getItemCount(); i++) {
			if (i > 0) {
				sb.append('|');
			}
			sb.append(box.getItemAt(i));
		}
		return new String[] { sb.toString(), String.valueOf(box.getSelectedItem()) };
	}

	// ---- the design

	private void freshPreferences() throws Exception {
		// The node itself stays (FlightConfiguration keeps a SwingPreferences of its own, whose node
		// would be gone after clearPreferences(): tools/openrocket-motor-chooser's ChooserScenarios).
		java.util.prefs.Preferences node = ((SwingPreferences) Application.getPreferences()).getPreferences();
		node.clear();
		for (String child : node.childrenNames()) {
			node.node(child).removeNode();
		}
		UnitGroup.resetDefaultUnits();
		Application.getPreferences().putBoolean("motorFilterLimitDiameter", true);
		Application.getPreferences().putBoolean("motorFilterLimitLength", true);
	}

	private void design(File dir, String name) throws Exception {
		DeterministicUuids.reseed("motor-chooser " + name);
		freshPreferences();
		OpenRocketDocument document = new GeneralRocketLoader(new File(dir, name)).load();
		Rocket rocket = document.getRocket();
		settle(rocket);
		List<MotorMount> mounts = new ArrayList<>();
		for (RocketComponent component : rocket) {
			if (component instanceof MotorMount mount && mount.isMotorMount()) {
				mounts.add(mount);
			}
		}
		List<FlightConfigurationId> ids = rocket.getIds();
		out.println(prefix + "DESIGN\t" + name + "\t" + ids.size() + "\t" + mounts.size());
		for (int m = 0; m < mounts.size(); m++) {
			MotorMount mount = mounts.get(m);
			out.println(prefix + "MOUNT\t" + name + "\t" + m + "\t" + ((RocketComponent) mount).getName() + "\t"
					+ hex(mount.getMotorMountDiameter()) + "\t" + hex(((RocketComponent) mount).getLength()));
		}

		// The window's chooser, made at its first open (MotorConfigurationPanel's constructor makes it).
		panel = new ThrustCurveMotorSelectionPanel();
		filterPanel = (MotorFilterPanel) get(panel, "motorFilterPanel");
		Set<Integer> searched = new HashSet<>();
		for (int c = 0; c < ids.size(); c++) {
			for (int m = 0; m < mounts.size(); m++) {
				open(name, c, ids.get(c), m, mounts.get(m), searched.add(m));
			}
		}
		java.util.prefs.Preferences preferred = ((SwingPreferences) Application.getPreferences())
				.getNode(ApplicationPreferences.PREFERRED_THRUST_CURVE_MOTOR_NODE);
		TreeMap<String, String> sorted = new TreeMap<>();
		for (String key : preferred.keys()) {
			sorted.put(key, preferred.get(key, null));
		}
		for (var entry : sorted.entrySet()) {
			out.println(prefix + "PREFERRED\t" + name + "\t" + entry.getKey() + "\t" + entry.getValue());
		}
	}

	/**
	 * QtRocket's fix, made from outside before setMotorMountAndConfig() for a configuration with a
	 * motor: the table's selection is cleared, with the selection it shows (which
	 * setMotorMountAndConfig() resets first thing), so that no row an earlier open selected
	 * survives. OpenRocket keeps that row selected while the table is sorted for the new mount, and
	 * when the mount's limits hide the configured motor's set, the row stays and its selection
	 * listener (selectMotorFromTable()) selects its set instead of the configured motor.
	 */
	private void forgetTableSelection() throws Exception {
		Field motor = ThrustCurveMotorSelectionPanel.class.getDeclaredField("selectedMotor");
		motor.setAccessible(true);
		motor.set(panel, null);
		Field set = ThrustCurveMotorSelectionPanel.class.getDeclaredField("selectedMotorSet");
		set.setAccessible(true);
		set.set(panel, null);
		table().clearSelection();
	}

	private void open(String name, int c, FlightConfigurationId fcid, int m, MotorMount mount, boolean search)
			throws Exception {
		MotorConfiguration configured = mount.getMotorConfig(fcid);
		Motor configuredMotor = configured.hasMotor() ? configured.getMotor() : null;

		if (fixed && configuredMotor != null) {
			forgetTableSelection();
		}
		panel.setMotorMountAndConfig(fcid, mount);
		// MotorChooserDialog.open().
		panel.selectMotorFromTable();

		Object filter = get(panel, "rowFilter");
		String[] rows = rows();
		JTable table = table();
		ThrustCurveMotor selected = (ThrustCurveMotor) panel.getSelectedMotor();
		ThrustCurveMotorSet selectedSet = (ThrustCurveMotorSet) get(panel, "selectedMotorSet");
		String[] curves = curves();
		String[] delays = delays();
		double maximum = (Double) call(get(panel, "maximumNozzleExitDiameterModel"), "getValue");
		out.println(String.join("\t", prefix + "OPEN", name, Integer.toString(c), Integer.toString(m),
				Integer.toString(configuredMotor == null ? -1 : setIndex(setOf(configuredMotor))),
				Integer.toString(configuredMotor == null ? -1 : curveIndex(configuredMotor)),
				hex(configured.getEjectionDelay()),
				ThrustCurveMotor.getDelayString(configured.getEjectionDelay(), PLUGGED),
				hex(configured.getNozzleExitDiameter()),
				Boolean.toString(((JCheckBox) get(filterPanel, "limitDiameterCheckBox")).isSelected()),
				Boolean.toString(((JCheckBox) get(filterPanel, "limitByLengthCheckBox")).isSelected()),
				thumbs("diameterSlider"), thumbs("lengthSlider"),
				hex((Double) call(filter, "getMinimumDiameter")),
				hex((Double) call(filter, "getMaximumDiameter")),
				hex((Double) call(filter, "getMinimumLength")),
				hex((Double) call(filter, "getMaximumLength")),
				((JLabel) get(filterPanel, "motorMountDimension")).getText(),
				((JLabel) get(panel, "nrOfMotorsLabel")).getText(), rows[0], rows[1],
				Integer.toString(table.getSelectedRow()), Integer.toString(setIndex(selectedSet)),
				Integer.toString(selected == null ? -1 : curveIndex(selected)), curves[0], curves[1], curves[2],
				curves[3], delays[0], delays[1], hex(panel.getSelectedDelay()),
				hex(panel.getSelectedNozzleExitDiameter()), hex(maximum)));

		if (search) {
			JTextField field = (JTextField) get(panel, "searchField");
			for (String text : SEARCHES) {
				field.setText(text);
				String[] found = rows();
				out.println(String.join("\t", prefix + "SEARCH", name, Integer.toString(c), Integer.toString(m), text,
						found[0], found[1]));
			}
			field.setText("");
		}

		// MotorChooserDialog.close(true).
		Motor chosen = panel.getSelectedMotor();
		if (chosen != null) {
			panel.selectedMotor(chosen);
		}
		out.println(String.join("\t", prefix + "ANSWER", name, Integer.toString(c), Integer.toString(m),
				Integer.toString(chosen == null ? -1 : setIndex(setOf(chosen))),
				Integer.toString(chosen == null ? -1 : curveIndex(chosen)), hex(panel.getSelectedDelay()),
				hex(panel.getSelectedNozzleExitDiameter())));
	}

	// ---- settling (tools/openrocket-goldens' AutomaticDimensions, as the other families settle)

	private static void settle(Rocket rocket) {
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

	private static List<Object> snapshot(List<RocketComponent> comps) {
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
}
