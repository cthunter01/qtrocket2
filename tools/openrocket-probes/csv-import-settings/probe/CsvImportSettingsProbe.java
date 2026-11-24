package probe;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Point;
import java.awt.Rectangle;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JRadioButton;
import javax.swing.JScrollBar;
import javax.swing.JSeparator;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.JTextPane;
import javax.swing.KeyStroke;
import javax.swing.SwingUtilities;
import javax.swing.border.TitledBorder;
import javax.swing.text.AttributeSet;
import javax.swing.text.Element;
import javax.swing.text.StyleConstants;
import javax.swing.text.StyledDocument;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.components.FieldSeparatorComboBox;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.simulation.CSVImportSettingsDialog;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.FlatLafOutlines;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Milestone 4 run 2, part S3: OpenRocket's CSV import settings dialog of the wind editor (swing gui/simulation/
 * CSVImportSettingsDialog.java, "Wind Data Import Settings"), under FlatLaf Light with SwingPreferences of a fresh
 * store. The dialog is a JDialog, so the probe needs a display (xvfb-run: the file needs-display beside probe/);
 * it is made and packed, never shown. It prints:
 * - DIALOG: its title, modality, size after pack(), the default button and the button Escape clicks;
 * - WALK: its content in the order of its containers, in the form of the M4 sim-config scout's dialog walk
 *   (BORDER for a titled panel, its children indented; TEXTAREA, CHECK, LABEL (a UnitSelector is a label too),
 *   FIELD, SPINNER, COMBO, SEPARATOR, BUTTON, each with " disabled" and " tip=..." as they apply); a hidden card,
 *   the scroll pane's scroll bars and the preview's text pane are left out;
 * - UNITS: the units each unit selector offers;
 * - PREVIEW: the preview's text pane line by line, each run of one colour as style[text] (normal: black,
 *   optional: gray, index: (0, 100, 0));
 * - BOX: the bounds of the walk's components and of the titled panels, relative to the content pane, after pack();
 * - STEP: what a user does (texts set into the column fields, values set on the index spinners, the header check
 *   box clicked with doClick(), separator items selected or typed, units selected), each followed by STATE (the
 *   column type label, the getters: the column of each row as the import is given it, the names, the indices, the
 *   units and getSeparator(), M4's bug 34), OUTLINE (the error outlines of the fields and spinners), PREVIEW and OK:
 *   what OK would do, its listener's validations (private methods called by reflection, in its order) and the
 *   message it would show, or "approved";
 * - the preferences the dialog starts from (other values stored, a separator "SPACE" (bug 34's store; the
 *   combo box shows its first character, M4's bug 50), a negative index), and OK and Cancel clicked.
 */
public final class CsvImportSettingsProbe {
	private final PrintStream out;

	private CsvImportSettingsProbe(PrintStream out) {
		this.out = out;
	}

	private static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n").replace("\t", "\\t");
	}

	private static Object field(Object object, String name) throws Exception {
		Field f = CSVImportSettingsDialog.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private static Object call(Object object, String name) throws Exception {
		Method m = CSVImportSettingsDialog.class.getDeclaredMethod(name);
		m.setAccessible(true);
		try {
			return m.invoke(object);
		} catch (InvocationTargetException e) {
			throw (Exception) e.getCause();
		}
	}

	// ------------------------------------------------------------------------------------- the walk

	private static String state(Component c) {
		String en = c.isEnabled() ? "" : " disabled";
		String tip = c instanceof JComponent jc && jc.getToolTipText() != null ? " tip=" + clean(jc.getToolTipText()) : "";
		return en + tip;
	}

	private static String describe(Component c) {
		if (c instanceof JSpinner sp) {
			return "SPINNER " + clean(((JSpinner.DefaultEditor) sp.getEditor()).getTextField().getText());
		}
		if (c instanceof JComboBox<?> cb) {
			StringBuilder items = new StringBuilder();
			for (int i = 0; i < cb.getItemCount(); i++) {
				items.append(i > 0 ? "|" : "").append(cb.getItemAt(i));
			}
			return "COMBO selected=" + clean(String.valueOf(cb.getSelectedItem())) + " items=" + items
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

	/** Whether the walk leaves @p c out: the scroll pane's scroll bars, the preview's text pane. */
	private static boolean skipped(Component c) {
		return c instanceof JScrollBar || c instanceof JTextPane;
	}

	private void walk(Component c, String indent) {
		if (!c.isVisible() || skipped(c)) {
			return;
		}
		String line = describe(c);
		if (line != null) {
			out.println("WALK " + indent + line + (line.equals("SEPARATOR") ? "" : state(c)));
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

	private static Rectangle boundsIn(Component c, Container root) {
		Point p = SwingUtilities.convertPoint(c.getParent(), c.getLocation(), root);
		return new Rectangle(p.x, p.y, c.getWidth(), c.getHeight());
	}

	private void boxes(Container root, Component c) {
		if (!c.isVisible() || c instanceof JScrollBar) {
			return;
		}
		String name = null;
		if (c instanceof JComponent jc && jc.getBorder() instanceof TitledBorder tb) {
			name = "BORDER " + tb.getTitle();
		} else if (c instanceof JTextPane) {
			name = "TEXTPANE";
		} else if (describe(c) != null) {
			String line = describe(c);
			name = line.length() > 47 ? line.substring(0, 47) : line;
		}
		if (name != null) {
			Rectangle r = boundsIn(c, root);
			out.println("BOX " + name + " x=" + r.x + " y=" + r.y + " w=" + r.width + " h=" + r.height);
		}
		if (c instanceof Container k && !(c instanceof JSpinner) && !(c instanceof JComboBox)) {
			for (Component child : k.getComponents()) {
				boxes(root, child);
			}
		}
	}

	// ------------------------------------------------------------------------------------- the state

	private static String styleOf(AttributeSet attributes) {
		Color color = StyleConstants.getForeground(attributes);
		if (Color.BLACK.equals(color)) {
			return "normal";
		}
		if (Color.GRAY.equals(color)) {
			return "optional";
		}
		if (new Color(0, 100, 0).equals(color)) {
			return "index";
		}
		return "color(" + color.getRed() + "," + color.getGreen() + "," + color.getBlue() + ")";
	}

	private void preview(CSVImportSettingsDialog dialog) throws Exception {
		JTextPane pane = (JTextPane) field(dialog, "previewTextPane");
		StyledDocument doc = pane.getStyledDocument();
		StringBuilder line = new StringBuilder();
		String runStyle = null;
		StringBuilder run = new StringBuilder();
		Element root = doc.getDefaultRootElement();
		List<Element> leaves = new ArrayList<>();
		for (int p = 0; p < root.getElementCount(); p++) {
			Element paragraph = root.getElement(p);
			for (int i = 0; i < paragraph.getElementCount(); i++) {
				leaves.add(paragraph.getElement(i));
			}
		}
		for (Element leaf : leaves) {
			String style = styleOf(leaf.getAttributes());
			String text = doc.getText(leaf.getStartOffset(), leaf.getEndOffset() - leaf.getStartOffset());
			for (char ch : text.toCharArray()) {
				if (ch == '\n') {
					if (run.length() > 0) {
						line.append(runStyle).append('[').append(run).append(']');
					}
					out.println("PREVIEW " + line);
					line.setLength(0);
					run.setLength(0);
					runStyle = null;
					continue;
				}
				if (runStyle != null && !runStyle.equals(style) && run.length() > 0) {
					line.append(runStyle).append('[').append(run).append(']');
					run.setLength(0);
				}
				runStyle = style;
				run.append(ch);
			}
		}
		if (run.length() > 0) {
			line.append(runStyle).append('[').append(run).append(']');
		}
		if (line.length() > 0) {
			out.println("PREVIEW " + line);
		}
	}

	private static String outlineOf(JComponent c) {
		return FlatLafOutlines.getOutline(c).name();
	}

	private void outlines(CSVImportSettingsDialog dialog) throws Exception {
		StringBuilder sb = new StringBuilder("OUTLINE");
		for (String row : new String[] {"altitude", "speed", "direction", "stdDeviation"}) {
			sb.append(' ').append(row).append("Field=").append(outlineOf((JComponent) field(dialog, row + "ColumnField")));
		}
		for (String row : new String[] {"altitude", "speed", "direction", "stdDeviation"}) {
			sb.append(' ').append(row).append("Spinner=")
					.append(outlineOf((JComponent) field(dialog, row + "ColumnSpinner")));
		}
		out.println(sb);
	}

	private void state(CSVImportSettingsDialog d) throws Exception {
		JLabel columnType = (JLabel) field(d, "columnTypeLabel");
		out.println("STATE columnType=[" + columnType.getText() + "] header=" + d.hasHeader()
				+ " columns=[" + d.getAltitudeColumn() + "|" + d.getSpeedColumn() + "|" + d.getDirectionColumn() + "|"
				+ d.getStdDeviationColumn() + "]"
				+ " names=[" + d.getAltitudeColumnName() + "|" + d.getSpeedColumnName() + "|"
				+ d.getDirectionColumnName() + "|" + d.getStdDeviationColumnName() + "]"
				+ " indices=[" + d.getAltitudeColumnIndex() + "|" + d.getSpeedColumnIndex() + "|"
				+ d.getDirectionColumnIndex() + "|" + d.getStdDeviationColumnIndex() + "]"
				+ " units=[" + d.getAltitudeUnit().getUnit() + "|" + d.getSpeedUnit().getUnit() + "|"
				+ d.getDirectionUnit().getUnit() + "|" + d.getStdDeviationUnit().getUnit() + "]"
				+ " separator=[" + clean(d.getSeparator()) + "]");
		outlines(d);
		preview(d);
		ok(d);
	}

	/** What OK's listener would do: its validations in its order, then the message it shows, or approval. */
	private void ok(CSVImportSettingsDialog d) throws Exception {
		JCheckBox header = (JCheckBox) field(d, "hasHeaderCheckBox");
		call(d, "validateColumnIndices");
		call(d, "validateColumnNames");
		if (header.isSelected() && (Boolean) call(d, "hasInvalidColumnNames")) {
			out.println("OK ERROR [" + Application.getTranslator().get("CSVImportSettingsDialog.validationError") + "] ["
					+ clean((String) call(d, "getColumnNameValidationErrorMessage")) + "]");
			return;
		}
		if (!header.isSelected() && (Boolean) call(d, "hasDuplicateColumnIndices")) {
			out.println("OK ERROR [" + Application.getTranslator().get("CSVImportSettingsDialog.validationError") + "] ["
					+ Application.getTranslator().get("CSVImportSettingsDialog.duplicateColumnError") + "]");
			return;
		}
		out.println("OK approved");
	}

	// ------------------------------------------------------------------------------------- the steps

	private void step(String what, CSVImportSettingsDialog d) throws Exception {
		out.println("STEP " + what);
		state(d);
	}

	private static void text(CSVImportSettingsDialog d, String row, String text) throws Exception {
		((JTextField) field(d, row + "ColumnField")).setText(text);
	}

	private static void index(CSVImportSettingsDialog d, String row, int value) throws Exception {
		((JSpinner) field(d, row + "ColumnSpinner")).setValue(value);
	}

	private static void header(CSVImportSettingsDialog d) throws Exception {
		((JCheckBox) field(d, "hasHeaderCheckBox")).doClick();
	}

	private static void separator(CSVImportSettingsDialog d, String item) throws Exception {
		((FieldSeparatorComboBox) field(d, "separatorComboBox")).setSelectedItem(item);
	}

	private static UnitSelector selector(CSVImportSettingsDialog d, String row) throws Exception {
		return (UnitSelector) field(d, row + "UnitSelector");
	}

	private static void unit(CSVImportSettingsDialog d, String row, String unit) throws Exception {
		UnitSelector s = selector(d, row);
		s.setSelectedUnit(s.getUnitGroup().getUnit(unit));
	}

	private void dialog(CSVImportSettingsDialog d) throws Exception {
		out.println("DIALOG title=[" + d.getTitle() + "] modality=" + d.getModalityType() + " size=" + d.getWidth() + "x"
				+ d.getHeight() + " default=[" + d.getRootPane().getDefaultButton().getText() + "] escape="
				+ (d.getRootPane().getInputMap(JComponent.WHEN_IN_FOCUSED_WINDOW)
						.get(KeyStroke.getKeyStroke("ESCAPE")) != null));
		walk(d.getContentPane(), "");
		for (String row : new String[] {"altitude", "speed", "direction", "stdDeviation"}) {
			UnitGroup group = selector(d, row).getUnitGroup();
			StringBuilder sb = new StringBuilder("UNITS " + row);
			for (int i = 0; i < group.getUnitCount(); i++) {
				sb.append(i == 0 ? " " : "|").append(group.getUnit(i).getUnit());
			}
			out.println(sb);
		}
		state(d);
		boxes(d.getContentPane(), d.getContentPane());
	}

	private void run() throws Exception {
		out.println("== fresh preferences");
		CSVImportSettingsDialog d = new CSVImportSettingsDialog(null);
		dialog(d);

		// The column names (the header mode)
		text(d, "altitude", "");
		step("altitude name empty", d);
		text(d, "altitude", "   ");
		step("altitude name blank", d);
		text(d, "altitude", "altitude");
		text(d, "speed", "altitude");
		step("speed name = altitude", d);
		text(d, "altitude", "");
		step("altitude name empty again (speed still named altitude)", d);
		text(d, "direction", "altitude");
		step("altitude empty, speed and direction named altitude", d);
		text(d, "altitude", " alt ");
		text(d, "speed", "speed");
		text(d, "direction", "direction");
		text(d, "stdDeviation", "alt");
		step("altitude ' alt ', std dev 'alt' (trimmed duplicates)", d);
		text(d, "stdDeviation", "");
		step("std dev name empty", d);
		text(d, "stdDeviation", "  ");
		step("std dev name blank", d);
		text(d, "altitude", "altitude");
		text(d, "stdDeviation", "stddev");
		step("names back to the defaults", d);

		// The separators
		for (String item : new String[] {";", "SPACE", "TAB", "|", "<->", ""}) {
			separator(d, item);
			step("separator item " + clean(item), d);
		}
		separator(d, ",");

		// The units
		for (String row : new String[] {"altitude", "speed", "direction", "stdDeviation"}) {
			UnitSelector s = selector(d, row);
			UnitGroup group = s.getUnitGroup();
			Unit original = s.getSelectedUnit();
			for (int i = 0; i < group.getUnitCount(); i++) {
				s.setSelectedUnit(group.getUnit(i));
				out.println("UNITSTEP " + row + " " + group.getUnit(i).getUnit());
				preview(d);
			}
			s.setSelectedUnit(original);
		}

		// The index mode
		header(d);
		step("header row unchecked", d);
		walk(d.getContentPane(), "");
		index(d, "altitude", 1);
		step("altitude index 1", d);
		index(d, "direction", 1);
		step("direction index 1 too", d);
		index(d, "altitude", 0);
		index(d, "direction", 2);
		index(d, "stdDeviation", 0);
		step("std dev index 0 (its name stddev)", d);
		header(d);
		text(d, "stdDeviation", "");
		header(d);
		step("std dev name emptied in the header mode, back to the index mode", d);
		index(d, "stdDeviation", 3);
		index(d, "speed", 7);
		index(d, "altitude", 12345);
		step("indices 12345, 7, 2, 3", d);
		header(d);
		step("header row checked again (std dev name empty)", d);
		text(d, "speed", "");
		header(d);
		step("speed name empty, then the index mode", d);
		index(d, "speed", 0);
		header(d);
		step("speed index 0, back to the header mode (speed name empty)", d);
		d.dispose();

		out.println("== OK and Cancel");
		d = new CSVImportSettingsDialog(null);
		((JButton) findButton(d.getContentPane(), "OK")).doClick();
		out.println("OK clicked: approved=" + d.isApproved() + " displayable=" + d.isDisplayable());
		d = new CSVImportSettingsDialog(null);
		((JButton) findButton(d.getContentPane(), "Cancel")).doClick();
		out.println("Cancel clicked: approved=" + d.isApproved() + " displayable=" + d.isDisplayable());

		out.println("== stored preferences");
		ApplicationPreferences prefs = Application.getPreferences();
		prefs.setMultiLevelWindCsvImportHeader(false);
		prefs.setMultiLevelWindCsvImportAltitudeColumn("Alt (ft)");
		prefs.setMultiLevelWindCsvImportSpeedColumn("Wind");
		prefs.setMultiLevelWindCsvImportDirectionColumn("Dir");
		prefs.setMultiLevelWindCsvImportStddevColumn("");
		prefs.setMultiLevelWindCsvImportAltitudeColumnIndex(4);
		prefs.setMultiLevelWindCsvImportSpeedColumnIndex(2);
		prefs.setMultiLevelWindCsvImportDirectionColumnIndex(0);
		prefs.setMultiLevelWindCsvImportStddevColumnIndex(2);
		prefs.setMultiLevelWindCsvImportAltitudeUnit(UnitGroup.UNITS_DISTANCE.getUnit("ft"));
		prefs.setMultiLevelWindCsvImportSpeedUnit(UnitGroup.UNITS_WINDSPEED.getUnit("km/h"));
		prefs.setMultiLevelWindCsvImportDirectionUnit(UnitGroup.UNITS_ANGLE.getUnit("rad"));
		prefs.setMultiLevelWindCsvImportStddevUnit(UnitGroup.UNITS_WINDSPEED.getUnit("mph"));
		prefs.putString(ApplicationPreferences.EXPORT_FIELD_SEPARATOR, ";");
		d = new CSVImportSettingsDialog(null);
		dialog(d);
		header(d);
		step("header row checked", d);
		d.dispose();
		prefs.putString(ApplicationPreferences.EXPORT_FIELD_SEPARATOR, "SPACE");
		d = new CSVImportSettingsDialog(null);
		step("separator stored as SPACE", d);
		d.dispose();
		prefs.putString(ApplicationPreferences.EXPORT_FIELD_SEPARATOR, "\t");
		d = new CSVImportSettingsDialog(null);
		step("separator stored as a tab", d);
		d.dispose();
		prefs.putString(ApplicationPreferences.EXPORT_FIELD_SEPARATOR, "");
		d = new CSVImportSettingsDialog(null);
		step("separator stored empty", d);
		d.dispose();
		prefs.setMultiLevelWindCsvImportSpeedColumnIndex(-1);
		try {
			d = new CSVImportSettingsDialog(null);
			out.println("negative index: made");
			d.dispose();
		} catch (RuntimeException e) {
			out.println("negative index: " + e.getClass().getName() + ": " + e.getMessage());
		}
	}

	private static Component findButton(Component c, String text) {
		if (c instanceof JButton b && text.equals(b.getText())) {
			return b;
		}
		if (c instanceof Container k) {
			for (Component child : k.getComponents()) {
				Component found = findButton(child, text);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		PrintStream out = new PrintStream(System.out, false, StandardCharsets.UTF_8);
		UITheme.Themes.LIGHT.applyTheme();
		CsvImportSettingsProbe probe = new CsvImportSettingsProbe(out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				probe.run();
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
