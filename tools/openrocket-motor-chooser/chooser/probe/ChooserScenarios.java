package info.openrocket.swing.gui.dialogs.motor.thrustcurve;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.RowSorter;
import javax.swing.SortOrder;
import javax.swing.table.TableModel;
import javax.swing.table.TableRowSorter;

import info.openrocket.core.database.motor.ThrustCurveMotorSet;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.file.motor.GeneralMotorLoader;
import info.openrocket.core.motor.Manufacturer;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.swing.gui.util.SwingPreferences;
import probe.DeterministicUuids;

/**
 * The scenarios of MotorChooserProbe, in the package of OpenRocket's chooser so that they reach its
 * package-private members (MotorRowFilter's setters, MotorHolder); private fields are read by
 * reflection. Each line is a kind and tab-separated fields (gen_motor_chooser_pins.py reads them):
 * <ul>
 * <li>DATABASE curves-read sets motors-in-sets</li>
 * <li>MANUFACTURER simple-name display-name set-count: the filter panel's list, in its order</li>
 * <li>LABELS unit label...: the diameter slider's labels</li>
 * <li>SCENARIO name row-count sort-keys md5, then ROW view-row model-row text... for the first
 * rows: the table (MD5 of every row's "model row, tab, the six column texts, line feed", UTF-8)</li>
 * <li>FILTER name min-diameter max-diameter min-length max-length: the filter's limits</li>
 * <li>CURVES set-index selected-curve curves: a set selected through the table, and the curve
 * list (the set indices of its curves, in its order); CURVESALL the same with "Hide very similar
 * thrust curves" off</li>
 * <li>SELECT label set-index curve digest delay curves-enabled curves curve-selection delay-items
 * delay-text nozzle: the panel after a step</li>
 * <li>DELAY typed delay text: the delay box after the text is typed</li>
 * <li>OPEN label digest delay nozzle curves delay-items delay-text model-rows set-row: the panel
 * after setMotorMountAndConfig()</li>
 * <li>PREFERRED digest value: the node of the preferred curves</li>
 * <li>CLASS impulse name description class-description: MotorClass</li>
 * </ul>
 * Doubles are Double.toHexString (exact); lists are joined by ',' (indices) or '|' (texts).
 */
public final class ChooserScenarios {

	/** How many rows of a scenario are printed (all of them go into its MD5). */
	private static final int PRINTED_ROWS = 10;

	private final ThrustCurveMotorSetDatabase db;
	private final PrintStream out;
	private ThrustCurveMotorSelectionPanel panel;
	private JTable table;
	private TableRowSorter<TableModel> sorter;
	private MotorRowFilter filter;

	private ChooserScenarios(ThrustCurveMotorSetDatabase db, PrintStream out) {
		this.db = db;
		this.out = out;
	}

	public static void run(ThrustCurveMotorSetDatabase db, int curvesRead, File customMotor, PrintStream out)
			throws Exception {
		ChooserScenarios scenarios = new ChooserScenarios(db, out);
		int motors = 0;
		for (ThrustCurveMotorSet set : db.getMotorSets()) {
			motors += set.getMotorCount();
		}
		out.println("DATABASE\t" + curvesRead + "\t" + db.getMotorSets().size() + "\t" + motors);
		scenarios.manufacturers();
		scenarios.rows();
		scenarios.customMotor(customMotor);
		scenarios.hideUsed();
		scenarios.curves();
		scenarios.selections();
		scenarios.opens();
		scenarios.preferred();
		scenarios.inches();
		scenarios.classes();
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

	/**
	 * Cleared preferences and default units, then a new panel. The preferences node itself stays:
	 * SwingPreferences.clearPreferences() removes it, and FlightConfiguration keeps a SwingPreferences
	 * of its own (a static field), whose node would then be gone ("Node has been removed").
	 */
	private void freshPanel() throws Exception {
		java.util.prefs.Preferences node = ((SwingPreferences) Application.getPreferences()).getPreferences();
		node.clear();
		for (String child : node.childrenNames()) {
			node.node(child).removeNode();
		}
		UnitGroup.resetDefaultUnits();
		newPanel();
	}

	/** A new panel over the preferences as they are. */
	@SuppressWarnings("unchecked")
	private void newPanel() throws Exception {
		panel = new ThrustCurveMotorSelectionPanel();
		table = (JTable) get(panel, "table");
		sorter = (TableRowSorter<TableModel>) get(panel, "sorter");
		filter = (MotorRowFilter) get(panel, "rowFilter");
	}

	private ThrustCurveMotorDatabaseModel model() throws Exception {
		return (ThrustCurveMotorDatabaseModel) get(panel, "model");
	}

	private int setIndex(ThrustCurveMotorSet set) {
		return db.getMotorSets().indexOf(set);
	}

	private ThrustCurveMotorSet findSet(String key) {
		for (ThrustCurveMotorSet s : db.getMotorSets()) {
			if ((s.getManufacturer().getDisplayName() + " " + s.getDesignation()).equals(key)) {
				return s;
			}
		}
		throw new IllegalArgumentException(key);
	}

	/** @p text with every character below U+0020 or above U+007E written as "<U+XXXX>". */
	private static String printable(String text) {
		StringBuilder sb = new StringBuilder();
		for (char c : text.toCharArray()) {
			if (c < 0x20 || c > 0x7E) {
				sb.append(String.format("<U+%04X>", (int) c));
			} else {
				sb.append(c);
			}
		}
		return sb.toString();
	}

	private static String hex(double value) {
		return Double.toHexString(value);
	}

	private static String hex(Double value) {
		return value == null ? "null" : Double.toHexString(value);
	}

	private static String keys(List<? extends RowSorter.SortKey> sortKeys) {
		StringBuilder sb = new StringBuilder();
		for (RowSorter.SortKey k : sortKeys) {
			if (sb.length() > 0) {
				sb.append(' ');
			}
			sb.append(k.getColumn()).append(k.getSortOrder().name().charAt(0));
		}
		return sb.toString();
	}

	private void scenario(String name) throws Exception {
		ThrustCurveMotorDatabaseModel model = model();
		int rows = table.getRowCount();
		StringBuilder all = new StringBuilder();
		List<String> printed = new ArrayList<>();
		for (int r = 0; r < rows; r++) {
			int m = table.convertRowIndexToModel(r);
			StringBuilder sb = new StringBuilder();
			sb.append(m);
			for (int c = 0; c < model.getColumnCount(); c++) {
				sb.append('\t').append(model.getValueAt(m, c));
			}
			all.append(sb).append('\n');
			if (r < PRINTED_ROWS) {
				printed.add("ROW\t" + r + "\t" + sb);
			}
		}
		MessageDigest md = MessageDigest.getInstance("MD5");
		StringBuilder hexDigest = new StringBuilder();
		for (byte b : md.digest(all.toString().getBytes(StandardCharsets.UTF_8))) {
			hexDigest.append(String.format("%02x", b));
		}
		out.println("SCENARIO\t" + name + "\t" + rows + "\t" + keys(sorter.getSortKeys()) + "\t" + hexDigest);
		for (String line : printed) {
			out.println(line);
		}
	}

	private void filterLine(String name) throws Exception {
		out.println("FILTER\t" + name + "\t" + hex(filter.getMinimumDiameter()) + "\t" + hex(filter.getMaximumDiameter())
				+ "\t" + hex(filter.getMinimumLength()) + "\t" + hex(filter.getMaximumLength()));
	}

	private void search(String text) throws Exception {
		((JTextField) get(panel, "searchField")).setText(text);
	}

	private void selectSet(ThrustCurveMotorSet set) throws Exception {
		int vi = table.convertRowIndexToView(model().getIndex(set));
		if (vi < 0) {
			throw new IllegalStateException("not visible: " + set);
		}
		table.getSelectionModel().setSelectionInterval(vi, vi);
	}

	@SuppressWarnings("unchecked")
	private JComboBox<MotorHolder> curveBox() throws Exception {
		return (JComboBox<MotorHolder>) get(panel, "curveSelectionBox");
	}

	@SuppressWarnings("unchecked")
	private JComboBox<String> delayBox() throws Exception {
		return (JComboBox<String>) get(panel, "delayBox");
	}

	private JCheckBox hideSimilarBox() throws Exception {
		return (JCheckBox) get(panel, "hideSimilarBox");
	}

	/** The set indices of the curves in the curve list, in its order. */
	private String curveList(ThrustCurveMotorSet set) throws Exception {
		JComboBox<MotorHolder> curves = curveBox();
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < curves.getItemCount(); i++) {
			if (i > 0) {
				sb.append(',');
			}
			MotorHolder holder = curves.getItemAt(i);
			if (holder.getIndex() != i) {
				throw new IllegalStateException("a curve item's index is not its position");
			}
			sb.append(set.getMotors().indexOf(holder.getMotor()));
		}
		return sb.toString();
	}

	private String delayItems() throws Exception {
		JComboBox<String> delays = delayBox();
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < delays.getItemCount(); i++) {
			if (i > 0) {
				sb.append('|');
			}
			sb.append(delays.getItemAt(i));
		}
		return sb.toString();
	}

	private ThrustCurveMotorSet selectedSet() throws Exception {
		return (ThrustCurveMotorSet) get(panel, "selectedMotorSet");
	}

	private void selectLine(String label) throws Exception {
		ThrustCurveMotor motor = (ThrustCurveMotor) panel.getSelectedMotor();
		ThrustCurveMotorSet set = selectedSet();
		out.println("SELECT\t" + label + "\t" + setIndex(set) + "\t" + set.getMotors().indexOf(motor) + "\t"
				+ motor.getDigest() + "\t" + hex(panel.getSelectedDelay()) + "\t" + curveBox().isEnabled() + "\t"
				+ curveList(set) + "\t" + curveBox().getSelectedIndex() + "\t" + delayItems() + "\t"
				+ delayBox().getSelectedItem() + "\t" + hex(panel.getSelectedNozzleExitDiameter()));
	}

	private static BodyTube bodyTubeMount(Rocket rocket) {
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		stage.addChild(body);
		body.setMotorMount(true);
		return body;
	}

	private static void configure(MotorMount mount, FlightConfigurationId fcid, Motor motor, double delay,
			double nozzle) {
		MotorConfiguration configuration = new MotorConfiguration(mount, fcid);
		configuration.setMotor(motor);
		configuration.setEjectionDelay(delay);
		configuration.setNozzleExitDiameter(nozzle);
		mount.setMotorConfig(configuration, fcid);
	}

	// ---- the groups

	/** The filter panel's manufacturer list: the database's manufacturers sorted by simple name. */
	private void manufacturers() {
		Set<Manufacturer> all = new HashSet<>();
		for (ThrustCurveMotorSet s : db.getMotorSets()) {
			all.add(s.getManufacturer());
		}
		List<Manufacturer> list = new ArrayList<>(all);
		list.sort((a, b) -> a.getSimpleName().compareTo(b.getSimpleName()));
		for (Manufacturer m : list) {
			int n = 0;
			for (ThrustCurveMotorSet s : db.getMotorSets()) {
				if (s.getManufacturer() == m) {
					n++;
				}
			}
			out.println("MANUFACTURER\t" + m.getSimpleName() + "\t" + m.getDisplayName() + "\t" + n);
		}
	}

	private void labels(String unit) throws Exception {
		Field f = MotorFilterPanel.class.getDeclaredField("diameterLabels");
		f.setAccessible(true);
		@SuppressWarnings("unchecked")
		java.util.Hashtable<Integer, javax.swing.JLabel> labels = (java.util.Hashtable<Integer, javax.swing.JLabel>) f
				.get(null);
		StringBuilder sb = new StringBuilder("LABELS\t" + unit);
		for (int i = 0; i < labels.size(); i++) {
			sb.append('\t').append(labels.get(i).getText());
		}
		out.println(sb);
	}

	/** The table under the filters, the search and the sort keys of the scout's probe. */
	private void rows() throws Exception {
		freshPanel();
		labels("mm");
		scenario("default");
		((JCheckBox) get(panel, "hideUnavailableBox")).doClick();
		scenario("hideUnavailableOff");
		sorter.toggleSortOrder(ThrustCurveMotorColumns.CASEINFO.ordinal());
		scenario("sortedByCase");
		sorter.toggleSortOrder(ThrustCurveMotorColumns.MANUFACTURER.ordinal());
		scenario("sortedByManufacturer");
		sorter.setSortKeys(Arrays.asList(new RowSorter.SortKey(4, SortOrder.ASCENDING),
				new RowSorter.SortKey(2, SortOrder.ASCENDING), new RowSorter.SortKey(0, SortOrder.ASCENDING)));

		List<Manufacturer> excluded = new ArrayList<>();
		for (ThrustCurveMotorSet s : db.getMotorSets()) {
			if (!excluded.contains(s.getManufacturer()) && s.getManufacturer() != Manufacturer.getManufacturer("Estes")) {
				excluded.add(s.getManufacturer());
			}
		}
		filter.setExcludedManufacturers(excluded);
		sorter.sort();
		scenario("onlyEstes");
		filter.setExcludedManufacturers(new ArrayList<>());
		sorter.sort();

		filter.setMinimumImpulse(ImpulseClass.D);
		filter.setMaximumImpulse(ImpulseClass.F);
		sorter.sort();
		scenario("impulseDToF");
		filter.setMinimumImpulse(ImpulseClass.A);
		filter.setMaximumImpulse(ImpulseClass.O);
		sorter.sort();
		scenario("impulseAToO");
		filter.setMinimumImpulse(null);
		filter.setMaximumImpulse(null);
		sorter.sort();

		for (String text : new String[] { "H12", "aerotech h", "29", "38 mm", "estes c6", "Ns", "blue", "0.1", "  ",
				"j", "rms", " \u0001Estes\u0002\tC6 " }) {
			search(text);
			scenario("search [" + printable(text) + "]");
		}
		search("");

		// A 28 mm mount (an inner tube of outer radius 14.5 mm, inner radius 14 mm, 0.1 m long)
		DeterministicUuids.reseed("mount 28 mm");
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		stage.addChild(body);
		InnerTube mount = new InnerTube();
		body.addChild(mount);
		mount.setOuterRadius(0.0145);
		mount.setInnerRadius(0.0145 - 0.0005);
		mount.setLength(0.1);
		mount.setMotorMount(true);
		FlightConfigurationId fcid = rocket.createFlightConfiguration(null).getId();
		out.println("MOUNT\t" + hex(mount.getMotorMountDiameter()) + "\t" + hex(mount.getLength()));
		panel.setMotorMountAndConfig(fcid, mount);
		MotorFilterPanel filterPanel = (MotorFilterPanel) get(panel, "motorFilterPanel");
		scenario("mountNoLimits");
		filterLine("mountNoLimits");
		JCheckBox limitDiameter = (JCheckBox) get(filterPanel, "limitDiameterCheckBox");
		limitDiameter.doClick();
		scenario("mountLimitDiameter");
		filterLine("mountLimitDiameter");
		JCheckBox limitLength = (JCheckBox) get(filterPanel, "limitByLengthCheckBox");
		limitLength.doClick();
		scenario("mountLimitDiameterAndLength");
		filterLine("mountLimitDiameterAndLength");
		limitLength.doClick();
		limitDiameter.doClick();
		scenario("mountLimitsOff");
		filterLine("mountLimitsOff");

		sorter.toggleSortOrder(ThrustCurveMotorColumns.NAME.ordinal());
		scenario("clickName");
		sorter.toggleSortOrder(ThrustCurveMotorColumns.NAME.ordinal());
		scenario("clickNameAgain");
		sorter.toggleSortOrder(ThrustCurveMotorColumns.MANUFACTURER.ordinal());
		scenario("clickManufacturer");
		sorter.toggleSortOrder(ThrustCurveMotorColumns.CASEINFO.ordinal());
		scenario("clickCase");
		((SwingPreferences) Application.getPreferences()).setMotorNameColumn(false);
		model().fireTableDataChanged();
		scenario("commonNameColumn");
	}

	/** A motor that is in no set of the database: the model's list becomes sorted. */
	private void customMotor(File file) throws Exception {
		freshPanel();
		GeneralMotorLoader loader = new GeneralMotorLoader();
		List<ThrustCurveMotor.Builder> builders;
		try (InputStream in = new FileInputStream(file)) {
			builders = loader.load(in, file.getName());
		}
		ThrustCurveMotor custom = builders.get(0).build();
		DeterministicUuids.reseed("custom motor");
		Rocket rocket = new Rocket();
		BodyTube mount = bodyTubeMount(rocket);
		FlightConfigurationId fcid = rocket.createFlightConfiguration(null).getId();
		configure(mount, fcid, custom, 3.3, 0);
		panel.setMotorMountAndConfig(fcid, mount);
		out.println("OPEN\tcustomMotor\t" + custom.getDigest() + "\t" + hex(panel.getSelectedDelay()) + "\t"
				+ hex(panel.getSelectedNozzleExitDiameter()) + "\t" + curveList(selectedSet()) + "\t" + delayItems()
				+ "\t" + delayBox().getSelectedItem() + "\t" + model().getRowCount() + "\t"
				+ model().getIndex(selectedSet()));
		scenario("customMotor");
	}

	/** "Hide motors already used in the mount", for a first mount and then a second one. */
	private void hideUsed() throws Exception {
		freshPanel();
		ThrustCurveMotorSet c6 = findSet("Estes C6");
		ThrustCurveMotorSet d12 = findSet("Estes D12");
		ThrustCurveMotorSet h128 = findSet("AeroTech H128W");
		DeterministicUuids.reseed("hide used, first mount");
		Rocket first = new Rocket();
		BodyTube firstMount = bodyTubeMount(first);
		FlightConfigurationId a = first.createFlightConfiguration(null).getId();
		FlightConfigurationId b = first.createFlightConfiguration(null).getId();
		configure(firstMount, a, c6.getMotors().get(1), 5, 0);
		configure(firstMount, b, d12.getMotors().get(0), Motor.PLUGGED_DELAY, 0);
		DeterministicUuids.reseed("hide used, second mount");
		Rocket second = new Rocket();
		BodyTube secondMount = bodyTubeMount(second);
		FlightConfigurationId c = second.createFlightConfiguration(null).getId();
		configure(secondMount, c, h128.getMotors().get(0), 6, 0);

		panel.setMotorMountAndConfig(a, firstMount);
		JCheckBox hideUsed = (JCheckBox) findHideUsed((MotorFilterPanel) get(panel, "motorFilterPanel"));
		hideUsed.doClick();
		scenario("hideUsed");
		panel.setMotorMountAndConfig(c, secondMount);
		scenario("hideUsedSecondMount");

		freshPanel();
		panel.setMotorMountAndConfig(c, secondMount);
		hideUsed = (JCheckBox) findHideUsed((MotorFilterPanel) get(panel, "motorFilterPanel"));
		hideUsed.doClick();
		scenario("hideUsedOnlySecondMount");
	}

	private static java.awt.Component findHideUsed(java.awt.Container container) {
		for (java.awt.Component component : container.getComponents()) {
			if (component instanceof JCheckBox box && box.getText().startsWith("Hide motors already used")) {
				return box;
			}
			if (component instanceof java.awt.Container inner) {
				java.awt.Component found = findHideUsed(inner);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	/** Every set of more than one curve selected through the table, with and without "Hide similar". */
	private void curves() throws Exception {
		freshPanel();
		for (ThrustCurveMotorSet set : db.getMotorSets()) {
			if (set.getMotorCount() > 1) {
				selectSet(set);
				out.println("CURVES\t" + setIndex(set) + "\t" + set.getMotors().indexOf(panel.getSelectedMotor()) + "\t"
						+ curveList(set));
			}
		}
		hideSimilarBox().doClick();
		for (ThrustCurveMotorSet set : db.getMotorSets()) {
			if (set.getMotorCount() > 1) {
				selectSet(set);
				out.println("CURVESALL\t" + setIndex(set) + "\t" + set.getMotors().indexOf(panel.getSelectedMotor())
						+ "\t" + curveList(set));
			}
		}
	}

	/** The scout's selections and delay transcript, and the curve list's choices. */
	private void selections() throws Exception {
		freshPanel();
		for (String key : new String[] { "AeroTech D24T", "AeroTech E15W", "Estes C6", "AeroTech H128W",
				"Cesaroni Technology Inc. I205", "Estes C6" }) {
			selectSet(findSet(key));
			selectLine("table " + key);
		}
		for (String typed : new String[] { "5", "5.5", "abc", "plugged", "PLUGGED", "None", "-3", "1e1", "5d", "0x1p3",
				"Infinity", "NaN", " 7 ", "5.04", "5.05", "5.06", "5.15", "1000", "", "3", "0.25", "-Infinity",
				"1e10", " 7" }) {
			delayBox().setSelectedItem(typed);
			out.println("DELAY\t" + typed + "\t" + hex(panel.getSelectedDelay()) + "\t" + delayBox().getSelectedItem());
		}
		delayBox().setSelectedItem("4");
		selectSet(findSet("Estes D12"));
		selectLine("delay 4, then Estes D12");
		delayBox().setSelectedItem("Plugged");
		selectSet(findSet("Estes C6"));
		selectLine("plugged, then Estes C6");
		selectSet(findSet("AeroTech H128W"));
		selectLine("then AeroTech H128W");
		delayBox().setSelectedItem("9");
		selectSet(findSet("AeroTech H128W"));
		selectLine("delay 9, the same set");
		delayBox().setSelectedItem("1e10");
		selectSet(findSet("Estes C6"));
		selectLine("delay 1e10, then Estes C6");

		// The curve list: AeroTech E15W, whose middle curve is very similar to the first.
		selectSet(findSet("AeroTech E15W"));
		selectLine("table AeroTech E15W again");
		curveBox().setSelectedIndex(1);
		selectLine("E15W curve item 1");
		hideSimilarBox().doClick();
		selectLine("E15W hide similar off");
		curveBox().setSelectedIndex(1);
		selectLine("E15W hide similar off, curve item 1");
		hideSimilarBox().doClick();
		selectLine("E15W hide similar on");
		curveBox().setSelectedIndex(0);
		selectLine("E15W curve item 0");
	}

	private static ThrustCurveMotor motor(String designation, double[] delays) {
		return new ThrustCurveMotor.Builder().setManufacturer(Manufacturer.getManufacturer("Estes"))
				.setDesignation(designation).setDiameter(0.018).setLength(0.07)
				.setTimePoints(new double[] { 0, 1, 2 }).setThrustPoints(new double[] { 0, 5, 0 })
				.setCGPoints(new CoordinateIF[] { new Coordinate(0.035, 0, 0, 0.02), new Coordinate(0.035, 0, 0, 0.015),
						new Coordinate(0.035, 0, 0, 0.01) })
				.setStandardDelays(delays).setDigest("digest" + designation).setMotorType(Motor.Type.SINGLE).build();
	}

	/** setMotorMountAndConfig() for one configuration on a fresh panel. */
	private void open(String label, Motor motor, double delay, double nozzle, boolean fresh) throws Exception {
		if (fresh) {
			freshPanel();
		}
		DeterministicUuids.reseed("open " + label);
		Rocket rocket = new Rocket();
		BodyTube mount = bodyTubeMount(rocket);
		FlightConfigurationId fcid = rocket.createFlightConfiguration(null).getId();
		configure(mount, fcid, motor, delay, nozzle);
		panel.setMotorMountAndConfig(fcid, mount);
		ThrustCurveMotor selected = (ThrustCurveMotor) panel.getSelectedMotor();
		out.println("OPEN\t" + label + "\t" + selected.getDigest() + "\t" + hex(panel.getSelectedDelay()) + "\t"
				+ hex(panel.getSelectedNozzleExitDiameter()) + "\t" + curveList(selectedSet()) + "\t" + delayItems()
				+ "\t" + delayBox().getSelectedItem() + "\t" + model().getRowCount() + "\t"
				+ model().getIndex(selectedSet()));
	}

	/** The panel opened for configurations (M3's bug 8 and the delay rules). */
	private void opens() throws Exception {
		ThrustCurveMotorSet c6 = findSet("Estes C6");
		ThrustCurveMotorSet h128 = findSet("AeroTech H128W");
		open("C6 curve 0, delay 6, nozzle 0.01", c6.getMotors().get(0), 6, 0.01, true);
		open("C6 curve 0, delay 5.04, nozzle 0.001", c6.getMotors().get(0), 5.04, 0.001, true);
		open("C6 curve 1, delay NaN", c6.getMotors().get(1), Double.NaN, 0, true);
		open("C6 curve 0, plugged", c6.getMotors().get(0), Motor.PLUGGED_DELAY, 0, true);
		open("H128W curve 1, plugged", h128.getMotors().get(1), Motor.PLUGGED_DELAY, 0.0005, true);
		open("C6 curve 0, delay 5", c6.getMotors().get(0), 5, 0, true);
		open("custom delays 2.55 and 4.25, delay 3.3", motor("Z1", new double[] { 2.55, 4.25 }), 3.3, 0, true);
		open("custom without delays, delay 5", motor("Z2", new double[] {}), 5, 0, true);
		open("custom delay 7, delay 3", motor("Z3", new double[] { 7 }), 3, 0, true);
		open("another custom, delay 3", motor("Z4", new double[] { 7 }), 3, 0, false);
		open("the first custom again, delay 7", findCustom("digestZ3"), 7, 0, false);
	}

	private ThrustCurveMotor findCustom(String digest) throws Exception {
		ThrustCurveMotorDatabaseModel model = model();
		for (int row = 0; row < model.getRowCount(); row++) {
			for (ThrustCurveMotor m : model.getMotorSet(row).getMotors()) {
				if (m.getDigest().equals(digest)) {
					return m;
				}
			}
		}
		throw new IllegalStateException(digest);
	}

	/** The preferred curve stored on OK, and chosen by a later chooser. */
	private void preferred() throws Exception {
		freshPanel();
		ThrustCurveMotorSet c6 = findSet("Estes C6");
		panel.selectedMotor(c6.getMotors().get(1));
		java.util.prefs.Preferences node = ((SwingPreferences) Application.getPreferences())
				.getNode(ApplicationPreferences.PREFERRED_THRUST_CURVE_MOTOR_NODE);
		for (String key : node.keys()) {
			out.println("PREFERRED\t" + key + "\t" + node.get(key, null));
		}
		newPanel();
		selectSet(c6);
		selectLine("preferred C6 curve 1");
		selectSet(findSet("Estes D12"));
		selectLine("then Estes D12");
	}

	/** Motor dimensions in inches. */
	private void inches() throws Exception {
		freshPanel();
		UnitGroup.UNITS_MOTOR_DIMENSIONS.setDefaultUnit("in");
		newPanel();
		labels("in");
		scenario("inchNoFilter");
		search("1.14");
		scenario("inchSearch [1.14]");
		UnitGroup.resetDefaultUnits();
	}

	/** MotorClass for some impulses. */
	private void classes() {
		UnitGroup.resetDefaultUnits();
		for (double impulse : new double[] { -1, 0, 0.1, 0.3125, 0.3125001, 0.4, 1.0, 2.5, 2.6, 9.99, 10.0, 99.6, 100.4,
				319.5, 640.1, 640.5, 640.6, 40960, 40960.4, 40960.6, 50000, Double.MAX_VALUE }) {
			MotorClass mc = MotorClass.getMotorClass(impulse);
			out.println("CLASS\t" + hex(impulse) + "\t" + mc.name() + "\t" + mc.getDescription(impulse) + "\t"
					+ mc.getClassDescription());
		}
	}
}
