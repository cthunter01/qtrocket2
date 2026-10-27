package probe;

import java.awt.BasicStroke;
import java.awt.Component;
import java.awt.Container;
import java.awt.Rectangle;
import java.awt.event.ActionEvent;
import java.awt.event.FocusEvent;
import java.awt.event.FocusListener;
import java.awt.event.InputEvent;
import java.awt.event.MouseEvent;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.time.Instant;
import java.time.ZoneOffset;
import java.util.ArrayList;
import java.util.List;

import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JRadioButton;
import javax.swing.JSpinner;
import javax.swing.JTable;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;

import org.jfree.chart.JFreeChart;
import org.jfree.chart.plot.XYPlot;
import org.jfree.data.xy.XYSeriesCollection;

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
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorDatabaseModel;
import info.openrocket.swing.gui.dialogs.motor.thrustcurve.ThrustCurveMotorSelectionPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Milestone 3 run 2, part M3: OpenRocket's ThrustCurveMotorSelectionPanel (swing gui/dialogs/motor/thrustcurve/
 * ThrustCurveMotorSelectionPanel.java) and its MotorInformationPanel over the bundled motor database, headless,
 * driven as a user would; the panels' members are read by reflection.
 * <ul>
 * <li>COUNT, VERSION: the "Number of motors" and "Motor DB version" labels (text and tooltip) after a search.</li>
 * <li>ENABLED: which of the left column's controls are enabled, with no motor, a set of several curves and a
 * set of one.</li>
 * <li>DELAY: the ejection delay box of Estes C6 after a text is typed into its editor and Enter pressed (the
 * editor's postActionEvent(), which the text field's Enter key fires), after the editor loses the focus (its
 * focus listeners called with a permanent FOCUS_LOST, as the focus manager calls them), and after an item is
 * chosen (setSelectedIndex(), what a click in the popup list does): the panel's delay, the editor's text,
 * the selected item and index, and how many action events the box fired.</li>
 * <li>INFO: the "Show Details" tab for every curve of the sets of QtRocket's smallMotorDatabase() (chosen in
 * the curve box with "Hide very similar thrust curves" off, so that every curve is listed), and for two of
 * them in imperial units: every value label with the tooltips of the impulse labels, the description and
 * whether it is italic, the plot's series names and line widths.</li>
 * <li>UPDATED, SOURCE: MotorInformationPanel's formatUpdatedOn() and formatSource() (private, by reflection)
 * for edge texts; ZONE: what java.time gives for the same instants in fixed-offset zones (the time zone is
 * UTC in run.sh).</li>
 * <li>NAMECOLUMN: the selection when the name column is switched while the table is sorted by its Name
 * column (M3's bug 10).</li>
 * <li>STALE: the panel opened for a mount's configuration with Estes C6, then for an empty configuration
 * of the mount, then selectMotorFromTable() (MotorChooserDialog.open()).</li>
 * <li>TABLEKEYS: the table's key bindings (Enter, Shift+Enter, Ctrl+Enter, a typed letter) and whether its
 * columns can be moved; then the selected row and the column lead when the table gains the focus (its
 * focus listeners called), after Enter and Shift+Enter (the bound actions run), at the first and the last row,
 * without a lead, after clearSelection() and in an empty table; and after a press of the right and the middle
 * button on another row.</li>
 * <li>RESELECT: the panel opened for an empty configuration of an 18 mm mount, AeroTech E15W (24 mm) selected
 * in the table, "Limit motor diameter to mount diameter" turned on (it hides E15W), OK (selectedMotor());
 * then opened for an empty configuration of a 29 mm mount (which shows E15W again), then for the 18 mm mount
 * again; each opening as MotorChooserDialog does it (setMotorMountAndConfig(), then open()'s
 * selectMotorFromTable()).</li>
 * </ul>
 */
public final class MotorSelectionPanelProbe {
	private final PrintStream out;
	private final ThrustCurveMotorSelectionPanel panel = new ThrustCurveMotorSelectionPanel();
	private final JTable table;
	private final ThrustCurveMotorDatabaseModel model;
	private final List<String> actions = new ArrayList<>();

	private MotorSelectionPanelProbe(PrintStream out) throws Exception {
		this.out = out;
		table = (JTable) get(panel, ThrustCurveMotorSelectionPanel.class, "table");
		model = (ThrustCurveMotorDatabaseModel) get(panel, ThrustCurveMotorSelectionPanel.class, "model");
		JComboBox<String> box = delayBox();
		box.addActionListener(e -> actions.add(e.getActionCommand() + " " + box.getSelectedItem()));
	}

	private static Object get(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private Object member(String name) throws Exception {
		return get(panel, ThrustCurveMotorSelectionPanel.class, name);
	}

	private Object info(String name) throws Exception {
		Object information = member("motorInformationPanel");
		return get(information, information.getClass(), name);
	}

	@SuppressWarnings("unchecked")
	private JComboBox<String> delayBox() throws Exception {
		return (JComboBox<String>) member("delayBox");
	}

	@SuppressWarnings("unchecked")
	private JComboBox<Object> curveBox() throws Exception {
		return (JComboBox<Object>) member("curveSelectionBox");
	}

	private JTextField delayEditor() throws Exception {
		return (JTextField) delayBox().getEditor().getEditorComponent();
	}

	private static String hex(double value) {
		return value + " (" + Double.toHexString(value) + ")";
	}

	/** The MD5 of @p text's UTF-8 bytes, in hex. */
	private static String md5(String text) throws Exception {
		byte[] digest = java.security.MessageDigest.getInstance("MD5").digest(text.getBytes(StandardCharsets.UTF_8));
		StringBuilder sb = new StringBuilder();
		for (byte b : digest) {
			sb.append(String.format("%02x", b & 0xff));
		}
		return sb.toString();
	}

	/** @p text with its control and non-ASCII characters as <U+XXXX>. */
	private static String escaped(String text) {
		if (text == null) {
			return "<null>";
		}
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < text.length(); i++) {
			char c = text.charAt(i);
			if (c < 0x20 || c > 0x7e) {
				sb.append(String.format("<U+%04X>", (int) c));
			} else {
				sb.append(c);
			}
		}
		return sb.toString();
	}

	private static ThrustCurveMotorSet findSet(String manufacturer, String designation) {
		for (ThrustCurveMotorSet set : Application.getThrustCurveMotorSetDatabase().getMotorSets()) {
			if (set.getManufacturer().matches(manufacturer) && set.getDesignation().equals(designation)) {
				return set;
			}
		}
		throw new IllegalStateException("no set " + manufacturer + " " + designation);
	}

	private void selectThroughTable(ThrustCurveMotorSet set) {
		int row = table.convertRowIndexToView(model.getIndex(set));
		table.setRowSelectionInterval(row, row);
	}

	private ThrustCurveMotorSet selectedSet() throws Exception {
		return (ThrustCurveMotorSet) member("selectedMotorSet");
	}

	private static String setName(ThrustCurveMotorSet set) {
		return set == null ? "none" : set.getManufacturer().getDisplayName() + " " + set.getDesignation();
	}

	// ---------------------------------------------------------------- the labels and enabled states

	private void count(String step) throws Exception {
		JLabel count = (JLabel) member("nrOfMotorsLabel");
		out.println("COUNT\t" + step + "\t[" + count.getText() + "]\t[" + count.getToolTipText() + "]\trows="
				+ table.getRowCount());
	}

	private boolean enabled(String name) throws Exception {
		return ((Component) member(name)).isEnabled();
	}

	private void enabledState(String step) throws Exception {
		out.println("ENABLED\t" + step + "\tcurveLabel=" + enabled("curveSelectionLabel") + "\tcurveBox="
				+ enabled("curveSelectionBox") + "\tdelayLabel=" + enabled("ejectionChargeDelayLabel")
				+ "\tdelayBox=" + enabled("delayBox") + "\tnozzleLabel=" + enabled("nozzleExitDiameterLabel")
				+ "\tnozzleSpinner=" + enabled("nozzleExitDiameterSpinner") + "\tnozzleUnit="
				+ enabled("nozzleExitDiameterUnitSelector") + "\tcurves=" + curveBox().getItemCount() + "\tdelays="
				+ delayBox().getItemCount());
	}

	private static void labels(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		JLabel version = (JLabel) p.member("motorDbVersionLabel");
		out.println("VERSION\t[" + version.getText() + "]\t[" + version.getToolTipText() + "]");
		p.count("initial");
		JTextField search = (JTextField) p.member("searchField");
		search.setText("zzz");
		p.count("search zzz");
		search.setText("estes c6");
		p.count("search estes c6");
		search.setText("");
		p.count("search cleared");
		p.enabledState("no motor");
		p.selectThroughTable(findSet("AeroTech", "E15W"));
		p.enabledState("AeroTech E15W");
		p.selectThroughTable(findSet("AeroTech", "E16W"));
		p.enabledState("AeroTech E16W");
	}

	// ------------------------------------------------------------------------------- the delay box

	private void delayState(String step) throws Exception {
		JComboBox<String> box = delayBox();
		out.println("DELAY\t" + step + "\tdelay=" + hex(panel.getSelectedDelay()) + "\ttext=[" + delayEditor().getText()
				+ "]\tselected=[" + box.getSelectedItem() + "]\tindex=" + box.getSelectedIndex() + "\tactions="
				+ actions);
		actions.clear();
	}

	private void typeEnter(String text) throws Exception {
		delayEditor().setText(text);
		delayEditor().postActionEvent();
	}

	private void focusLost() throws Exception {
		JTextField editor = delayEditor();
		FocusEvent event = new FocusEvent(editor, FocusEvent.FOCUS_LOST, false, null);
		for (FocusListener listener : editor.getFocusListeners()) {
			listener.focusLost(event);
		}
	}

	private void typeFocusLost(String text) throws Exception {
		delayEditor().setText(text);
		focusLost();
	}

	private static void delays(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		p.selectThroughTable(findSet("Estes", "C6"));
		p.delayState("Estes C6");
		p.typeEnter("5.04");
		p.delayState("type 5.04, Enter");
		p.delayEditor().postActionEvent();
		p.delayState("Enter on the text shown");
		p.typeFocusLost("6");
		p.delayState("type 6, focus lost");
		p.typeEnter("5.04");
		p.delayState("type 5.04, Enter again");
		p.focusLost();
		p.delayState("focus lost, text unchanged");
		p.typeFocusLost("5");
		p.delayState("type 5 (the text shown), focus lost");
		p.typeFocusLost("4.99");
		p.delayState("type 4.99, focus lost");
		p.focusLost();
		p.delayState("focus lost again");
		p.delayBox().setSelectedIndex(1);
		p.delayState("choose item 1");
		p.delayBox().setSelectedIndex(1);
		p.delayState("choose item 1 again");
		p.delayBox().setSelectedIndex(p.delayBox().getItemCount() - 1);
		p.delayState("choose the last item");
		p.typeEnter("abc");
		p.delayState("type abc, Enter");
		p.typeFocusLost("abc");
		p.delayState("type abc, focus lost");
		p.typeFocusLost("7");
		p.delayState("type 7, focus lost");
		p.typeFocusLost("3");
		p.delayState("type 3, focus lost");
		p.typeEnter("0x1p3");
		p.delayState("type 0x1p3, Enter");
		p.typeEnter("none");
		p.delayState("type none, Enter");
		p.typeEnter(" 5 ");
		p.delayState("type ' 5 ', Enter");
		p.typeEnter("NaN");
		p.delayState("type NaN, Enter");
		p.typeEnter("2");
		p.delayState("type 2, Enter");
		p.selectThroughTable(findSet("Estes", "1/2A3"));
		p.delayState("then Estes 1/2A3");
	}

	// ----------------------------------------------------------------------- the information panel

	private String label(String name) throws Exception {
		return "[" + escaped(((JLabel) info(name)).getText()) + "]";
	}

	private String tip(String name) throws Exception {
		return "[" + escaped(((JLabel) info(name)).getToolTipText()) + "]";
	}

	private void information(String step) throws Exception {
		JTextArea comment = (JTextArea) info("comment");
		XYPlot plot = ((JFreeChart) info("chart")).getXYPlot();
		XYSeriesCollection dataset = (XYSeriesCollection) plot.getDataset();
		StringBuilder series = new StringBuilder();
		StringBuilder strokes = new StringBuilder();
		for (int i = 0; i < dataset.getSeriesCount(); i++) {
			series.append(i == 0 ? "" : "|").append(dataset.getSeries(i).getKey());
			strokes.append(i == 0 ? "" : "|")
					.append(((BasicStroke) plot.getRenderer().getSeriesStroke(i)).getLineWidth());
		}
		out.println("INFO\t" + step + "\tmanufacturer=" + label("manufacturerLabel") + "\tdesignation="
				+ label("designationLabel") + "\tcommon=" + label("commonNameLabel") + "\timpulse="
				+ label("totalImpulseLabel") + "\tclass=" + label("classificationLabel") + "\timpulseTip="
				+ tip("totalImpulseLabel") + "\tclassTip=" + tip("classificationLabel") + "\tavg="
				+ label("avgThrustLabel") + "\tmax=" + label("maxThrustLabel") + "\tburn=" + label("burnTimeLabel")
				+ "\tlaunch=" + label("launchMassLabel") + "\tempty=" + label("emptyMassLabel") + "\ttype="
				+ label("motorTypeLabel") + "\tcase=" + label("caseInfoLabel") + "\tprop=" + label("propInfoLabel")
				+ "\tcompatible=" + label("compatibleCasesLabel") + "\tpoints=" + label("dataPointsLabel")
				+ "\tupdated=" + label("lastUpdatedOnLabel") + "\tsource=" + label("sourceLabel") + "\tcomment=["
				+ escaped(comment.getText()) + "]\tcommentMd5=" + md5(comment.getText()) + "\titalic="
				+ comment.getFont().isItalic() + "\tseries=["
				+ series + "]\tstrokes=[" + strokes + "]");
	}

	private static final String[][] SMALL_SETS = { { "Klima", "C6" }, { "AeroTech", "E15W" },
			{ "AeroTech", "E16W" }, { "AeroTech", "G12ST" }, { "AeroTech", "J145H_2JET_STD" },
			{ "AeroTech", "M1850W" }, { "Cesaroni", "117G69-14A" }, { "Estes", "1/2A3" }, { "Estes", "A8" },
			{ "Estes", "C6" }, { "Quest", "B6W" } };

	private void everyCurve(ThrustCurveMotorSet set, String units) throws Exception {
		selectThroughTable(set);
		JComboBox<Object> curves = curveBox();
		for (int i = 0; i < curves.getItemCount(); i++) {
			curves.setSelectedIndex(i);
			ThrustCurveMotor motor = (ThrustCurveMotor) panel.getSelectedMotor();
			information(units + setName(set) + " item " + i + " curve " + set.getMotors().indexOf(motor));
		}
	}

	private static void informations(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		p.information("no motor");
		// "Hide very similar thrust curves" off: every curve in the curve box.
		((javax.swing.JCheckBox) p.member("hideSimilarBox")).doClick();
		for (String[] set : SMALL_SETS) {
			p.everyCurve(findSet(set[0], set[1]), "");
		}
		UnitGroup.setDefaultImperialUnits();
		p.everyCurve(findSet("Estes", "C6"), "imperial ");
		p.everyCurve(findSet("AeroTech", "M1850W"), "imperial ");
		UnitGroup.setDefaultMetricUnits();
		((javax.swing.JCheckBox) p.member("hideSimilarBox")).doClick();
	}

	private static final String[] UPDATED = { null, "", "   ", "2024-01-02", " 2024-01-02 ", "2024-01-02T03:04:05Z",
			" 2024-01-02T03:04:05Z\t", "2024-01-02T23:30:00Z", "2024-01-02T03:04Z", "2024-01-02T03:04:05.123456789Z",
			"2024-01-02T03:04:05.1234567891Z", "2024-01-02T03:04:05.Z", "2024-01-02T03:04:05,5Z",
			"2024-01-02T03:04:05+02:00", "2024-01-02T01:04:05+02:00", "2024-01-02T22:04:05-02:00",
			"2024-01-02T03:04:05+0200", "2024-01-02T03:04:05+02", "2024-01-02T03:04:05+02:00:30",
			"2024-01-02T03:04:05-00:00", "2024-01-02T03:04:05+18:00", "2024-01-02T03:04:05+19:00",
			"2024-01-02t03:04:05z", "2024-01-02T03:04:05z", "2024-01-02T24:00:00Z", "2024-01-02T23:59:60Z",
			"2016-12-31T23:59:60Z", "2024-01-02T23:60:00Z", "2024-02-29T00:00:00Z", "2023-02-29T00:00:00Z",
			"2024-02-30T00:00:00Z", "2024-04-31T00:00:00Z", "2024-01-02 03:04:05Z", "2024-1-2T03:04:05Z",
			"20240102T030405Z", "2024-01-02T3:04:05Z", "2024-01-02T03:04:05", "+2024-01-02T03:04:05Z",
			"+12024-01-02T03:04:05Z", "12024-01-02T03:04:05Z", "0000-01-01T00:00:00Z", "-0001-12-31T00:00:00Z",
			"0999-01-01T00:00:00Z", "999-01-01T00:00:00Z", "2024-01-02T03:04:05Z[UTC]", "2024-01-02T03:04:05.5Z",
			"2024-01-02T12:00:60Z", "2024-01-02T23:58:60Z", "2024-01-02T23:59:60.5Z", "2024-01-02T24:00:01Z",
			"2024-01-02T24:01:00Z", "2024-01-02T24:00:00.5Z", "2024-12-31T24:00:00Z", "2024-01-02T25:00:00Z",
			"2024-13-02T03:04:05Z", "2024-00-02T03:04:05Z", "2024-01-00T03:04:05Z", "2024-01-32T03:04:05Z",
			"2024-01-02T03:04:05.000000000Z", "2024-01-02T03:04:05-18:00", "2024-01-02T03:04:05-18:01",
			"2024-01-02T03:04:05+2:00", "2024-01-02T03:04:05+02:0", "2024-01-02T03:04:05+02:00:3",
			"2024-01-02T03:04:05+02:00:60", "2024-01-02T03:04:05+02:60", "2024-01-02T03:04:05+24:00",
			"\uff12\uff10\uff12\uff14-01-02T03:04:05Z", "2024-01-02T03:04:05\u0660Z", "+0999-01-01T00:00:00Z",
			"+10000-01-01T00:00:00Z", "-10000-01-01T00:00:00Z", "-2024-01-02T03:04:05Z", "+999999999-12-31T23:59:59Z",
			"+1000000000-12-31T23:59:59Z", "+1000000001-01-01T00:00:00Z", "-1000000000-01-01T00:00:00Z",
			"-1000000001-01-01T00:00:00Z", "2024-01-02T03:04:05Z ", "2024-01-02T03:04:05+00:00",
			"2024-01-02T03:04:05.123+05:30", "1970-01-01T00:00:00Z", "1969-12-31T23:59:59.999999999Z",
			"2024-01-02T03:04:05ZZ", "2024-01-02T03:04:05..5Z", "2024-01-02T03:04:5Z", "-0000-01-01T00:00:00Z",
			"+00000-01-01T00:00:00Z", "+01000-01-01T00:00:00Z", "2024-01-02T23:59:60+02:00", "2024-01-02T21:59:60Z",
			"2024-01-02T24:00:00.0Z", "2024-01-02T24:00:00.Z", "2024-01-02T03:04:05+18:00:01",
			"2024-01-02T03:04:05+00:00:00", "2024-01-02T03:04:05-00:00:01", "2024-01-02T03:04:05.Z" };

	private static final String[] SOURCES = { null, "", "  ", "thrustcurve.org", " thrustcurve.org ",
			"THRUSTCURVE.ORG", "ThrustCurve.org", "thrustcurve.orgx", "www.thrustcurve.org", "Manufacturer",
			" cert ", "thrustcurve\u00a0org" };

	private static void formats(PrintStream out) throws Exception {
		Class<?> type = Class.forName("info.openrocket.swing.gui.dialogs.motor.thrustcurve.MotorInformationPanel");
		Method updated = type.getDeclaredMethod("formatUpdatedOn", String.class);
		updated.setAccessible(true);
		Method source = type.getDeclaredMethod("formatSource", String.class);
		source.setAccessible(true);
		for (String text : UPDATED) {
			String shown;
			try {
				shown = "[" + escaped((String) updated.invoke(null, text)) + "]";
			} catch (InvocationTargetException e) {
				shown = "throws " + e.getCause().getClass().getName();
			}
			out.println("UPDATED\t[" + escaped(text) + "]\t" + shown);
		}
		ZoneOffset[] zones = { ZoneOffset.UTC, ZoneOffset.ofHours(-6), ZoneOffset.ofHoursMinutes(5, 30),
				ZoneOffset.ofHours(14), ZoneOffset.ofHours(-12) };
		for (String text : UPDATED) {
			if (text == null) {
				continue;
			}
			Instant instant;
			try {
				instant = Instant.parse(text.trim());
			} catch (Exception e) {
				continue;
			}
			StringBuilder line = new StringBuilder("ZONE\t[" + escaped(text) + "]");
			for (ZoneOffset zone : zones) {
				line.append("\t").append(zone).append("=");
				try {
					line.append(instant.atZone(zone).toLocalDate());
				} catch (Exception e) {
					line.append("throws ").append(e.getClass().getName());
				}
			}
			out.println(line);
		}
		for (String text : SOURCES) {
			out.println("SOURCE\t[" + escaped(text) + "]\t[" + escaped((String) source.invoke(null, text)) + "]");
		}
	}

	// ------------------------------------------------------------------------- the name column

	private static List<JRadioButton> radios(Container container) {
		List<JRadioButton> found = new ArrayList<>();
		for (Component child : container.getComponents()) {
			if (child instanceof JRadioButton) {
				found.add((JRadioButton) child);
			}
			if (child instanceof Container) {
				found.addAll(radios((Container) child));
			}
		}
		return found;
	}

	private JRadioButton radio(String text) {
		for (JRadioButton button : radios(panel)) {
			if (button.getText().equals(text)) {
				return button;
			}
		}
		throw new IllegalStateException("no radio button " + text);
	}

	private void nameState(String step) throws Exception {
		int row = table.getSelectedRow();
		String rowSet = row < 0 ? "none" : setName(model.getMotorSet(table.convertRowIndexToModel(row)));
		out.println("NAMECOLUMN\t" + step + "\tselected=" + setName(selectedSet()) + "\trow=" + row + "\trowSet="
				+ rowSet + "\tcurveItem=[" + curveBox().getSelectedItem() + "]");
	}

	private static void nameColumn(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		p.table.getRowSorter().toggleSortOrder(1);
		p.table.setRowSelectionInterval(10, 10);
		p.nameState("sorted by Name, row 10");
		p.radio("Use common name").doClick();
		p.nameState("common name");
		p.radio("Use manufacturer's designation").doClick();
		p.nameState("designation");
		p.selectThroughTable(findSet("Quest", "B6W"));
		p.nameState("Quest B6W");
		p.radio("Use common name").doClick();
		p.nameState("common name again");
		p.radio("Use manufacturer's designation").doClick();
		p.nameState("designation again");
	}

	// ---------------------------------------------------------- an empty configuration after a motor

	private void staleState(String step) throws Exception {
		panelState("STALE", step);
	}

	private void panelState(String tag, String step) throws Exception {
		int row = table.getSelectedRow();
		String rowSet = row < 0 ? "none" : setName(model.getMotorSet(table.convertRowIndexToModel(row)));
		StringBuilder curves = new StringBuilder();
		for (int i = 0; i < curveBox().getItemCount(); i++) {
			curves.append(i == 0 ? "" : "|").append(curveBox().getItemAt(i));
		}
		StringBuilder delays = new StringBuilder();
		for (int i = 0; i < delayBox().getItemCount(); i++) {
			delays.append(i == 0 ? "" : "|").append(delayBox().getItemAt(i));
		}
		Motor motor = panel.getSelectedMotor();
		out.println(tag + "\t" + step + "\tmotor=" + (motor == null ? "none" : motor.getDesignation()) + "\tset="
				+ setName(selectedSet()) + "\tdelay=" + hex(panel.getSelectedDelay()) + "\tnozzle="
				+ hex(panel.getSelectedNozzleExitDiameter()) + "\trowSet=" + rowSet + "\tcurves=[" + curves
				+ "]\tcurveBox=" + enabled("curveSelectionBox") + "\tdelays=[" + delays + "]\tdelayText=["
				+ delayEditor().getText() + "]\tdelayBox=" + enabled("delayBox") + "\tnozzleText=["
				+ ((JSpinner.DefaultEditor) ((JSpinner) member("nozzleExitDiameterSpinner")).getEditor())
						.getTextField().getText()
				+ "]\tinfo=" + label("designationLabel") + "\tseries="
				+ ((XYSeriesCollection) ((JFreeChart) info("chart")).getXYPlot().getDataset()).getSeriesCount());
	}

	private static void staleConfiguration(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
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
		FlightConfigurationId withMotor = rocket.createFlightConfiguration(null).getId();
		FlightConfigurationId empty = rocket.createFlightConfiguration(null).getId();
		MotorConfiguration configuration = new MotorConfiguration(mount, withMotor);
		configuration.setMotor(findSet("Estes", "C6").getMotors().get(0));
		configuration.setEjectionDelay(5);
		configuration.setNozzleExitDiameter(0.004);
		mount.setMotorConfig(configuration, withMotor);
		p.staleState("fresh panel");
		p.panel.setMotorMountAndConfig(empty, mount);
		p.staleState("fresh panel, empty configuration");
		p.panel.selectMotorFromTable();
		p.staleState("fresh panel, empty configuration, open()");
		p.panel.setMotorMountAndConfig(withMotor, mount);
		p.staleState("Estes C6 configuration");
		p.panel.setMotorMountAndConfig(empty, mount);
		p.staleState("then the empty configuration");
		p.panel.selectMotorFromTable();
		p.staleState("then open()");
	}

	// ------------------------------------------------------------------------------- the table's keys

	private void tableKeyState(String step) {
		int row = table.getSelectedRow();
		String rowSet = row < 0 ? "none" : setName(model.getMotorSet(table.convertRowIndexToModel(row)));
		out.println("TABLEKEYS\t" + step + "\trow=" + row + "\tlast=" + (table.getRowCount() - 1) + "\tlead="
				+ table.getSelectionModel().getLeadSelectionIndex() + "\tcolumn="
				+ table.getColumnModel().getSelectionModel().getLeadSelectionIndex() + "\trowSet=" + rowSet);
	}

	/** Runs the action bound to @p stroke in the table (when it has the focus or is its ancestor). */
	private void tableKey(String stroke) {
		Object name = table.getInputMap(JComponent.WHEN_FOCUSED).get(KeyStroke.getKeyStroke(stroke));
		if (name == null) {
			name = table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT).get(KeyStroke.getKeyStroke(stroke));
		}
		table.getActionMap().get(name).actionPerformed(new ActionEvent(table, ActionEvent.ACTION_PERFORMED, ""));
	}

	/** A press, release and click of @p button (2 or 3) on the middle of @p row's second cell. */
	private void click(int button, int row) {
		Rectangle cell = table.getCellRect(row, 1, true);
		int x = cell.x + cell.width / 2;
		int y = cell.y + cell.height / 2;
		int mask = button == MouseEvent.BUTTON2 ? InputEvent.BUTTON2_DOWN_MASK : InputEvent.BUTTON3_DOWN_MASK;
		long when = 0;
		table.dispatchEvent(new MouseEvent(table, MouseEvent.MOUSE_PRESSED, when, mask, x, y, x, y, 1,
				button == MouseEvent.BUTTON3, button));
		table.dispatchEvent(new MouseEvent(table, MouseEvent.MOUSE_RELEASED, when, 0, x, y, x, y, 1, false, button));
		table.dispatchEvent(new MouseEvent(table, MouseEvent.MOUSE_CLICKED, when, 0, x, y, x, y, 1, false, button));
	}

	private static void tableKeys(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		for (String stroke : new String[] { "ENTER", "shift ENTER", "ctrl ENTER", "typed a" }) {
			Object focused = p.table.getInputMap(JComponent.WHEN_FOCUSED).get(KeyStroke.getKeyStroke(stroke));
			Object ancestor = p.table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT)
					.get(KeyStroke.getKeyStroke(stroke));
			out.println("TABLEKEYS\tbinding [" + stroke + "]\tfocused=" + focused + "\tancestor=" + ancestor);
		}
		out.println("TABLEKEYS\treorderingAllowed=" + p.table.getTableHeader().getReorderingAllowed());
		p.tableKeyState("fresh panel");
		FocusEvent gained = new FocusEvent(p.table, FocusEvent.FOCUS_GAINED, false, null);
		for (FocusListener listener : p.table.getFocusListeners()) {
			listener.focusGained(gained);
		}
		p.tableKeyState("focus gained");
		p.tableKey("ENTER");
		p.tableKeyState("Enter");
		p.tableKey("ENTER");
		p.tableKeyState("Enter");
		p.tableKey("shift ENTER");
		p.tableKeyState("Shift+Enter");
		p.tableKey("shift ENTER");
		p.tableKeyState("Shift+Enter on the first row");
		p.tableKey("ENTER");
		p.tableKeyState("Enter on the last row");
		p.table.clearSelection();
		p.tableKeyState("clearSelection()");
		p.tableKey("ENTER");
		p.tableKeyState("Enter");
		int last = p.table.getRowCount() - 1;
		p.table.setRowSelectionInterval(last, last);
		p.tableKeyState("the last row selected");
		p.tableKey("ENTER");
		p.tableKeyState("Enter on the last row");
		JTextField search = (JTextField) p.member("searchField");
		search.setText("zzz");
		p.tableKeyState("search zzz");
		p.tableKey("ENTER");
		p.tableKeyState("Enter in the empty table");
		search.setText("");
		p.tableKeyState("search cleared");
		p.table.setRowSelectionInterval(3, 3);
		p.tableKeyState("row 3 selected");
		p.table.setSize(600, 4000);
		p.table.doLayout();
		p.click(MouseEvent.BUTTON3, 5);
		p.tableKeyState("right button on row 5");
		p.click(MouseEvent.BUTTON2, 6);
		p.tableKeyState("middle button on row 6");
	}

	// ------------------------------------------- an empty configuration after a set a filter hid

	private static InnerTube innerTube(BodyTube body, double innerRadius, double length) {
		InnerTube tube = new InnerTube();
		body.addChild(tube);
		tube.setOuterRadius(innerRadius + 0.0005);
		tube.setInnerRadius(innerRadius);
		tube.setLength(length);
		tube.setMotorMount(true);
		return tube;
	}

	/** MotorChooserDialog's opening: setMotorMountAndConfig(), then open()'s selectMotorFromTable(). */
	private void open(String step, FlightConfigurationId fcid, InnerTube mount) throws Exception {
		panel.setMotorMountAndConfig(fcid, mount);
		panelState("RESELECT", step + ": setMotorMountAndConfig");
		panel.selectMotorFromTable();
		panelState("RESELECT", step + ": open()");
	}

	/** MotorChooserDialog.close(true). */
	private void ok() {
		Motor selected = panel.getSelectedMotor();
		if (selected != null) {
			panel.selectedMotor(selected);
		}
	}

	private static void reselect(PrintStream out) throws Exception {
		MotorSelectionPanelProbe p = new MotorSelectionPanelProbe(out);
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		stage.addChild(body);
		InnerTube small = innerTube(body, 0.009, 0.075);
		InnerTube large = innerTube(body, 0.0145, 0.15);
		FlightConfigurationId empty = rocket.createFlightConfiguration(null).getId();
		p.open("18 mm mount", empty, small);
		p.selectThroughTable(findSet("AeroTech", "E15W"));
		p.panelState("RESELECT", "AeroTech E15W selected");
		JCheckBox limit = (JCheckBox) get(p.member("motorFilterPanel"),
				p.member("motorFilterPanel").getClass().getSuperclass(), "limitDiameterCheckBox");
		limit.doClick(0);
		p.panelState("RESELECT", "Limit motor diameter on");
		p.ok();
		p.open("29 mm mount", empty, large);
		p.ok();
		p.open("18 mm mount again", empty, small);
		limit.doClick(0);
	}

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		File database = new File(System.getProperty("qtrocket.repo"), "data/motors/initial_motors.db");
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		List<ThrustCurveMotor> read = ThrustCurveMotorSQLiteDatabase.readDatabase(database);
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
		UITheme.Themes.LIGHT.applyTheme();
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				labels(out);
				delays(out);
				informations(out);
				formats(out);
				nameColumn(out);
				staleConfiguration(out);
				tableKeys(out);
				reselect(out);
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
