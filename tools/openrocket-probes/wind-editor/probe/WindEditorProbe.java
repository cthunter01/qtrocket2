package probe;

import java.awt.Component;
import java.awt.Container;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JLabel;
import javax.swing.JSpinner;
import javax.swing.SwingUtilities;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel;
import info.openrocket.core.models.wind.MultiLevelPinkNoiseWindModel.LevelWindModel;
import info.openrocket.core.models.wind.WindModel;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.simulation.MultiLevelWindTable;
import info.openrocket.swing.gui.simulation.WindProfilePanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Milestone 4 run 2, part S2: OpenRocket's wind editor (swing gui/simulation/ MultiLevelWindEditDialog.java's
 * table, MultiLevelWindTable.java, and its visualization, WindProfilePanel.java), headless under FlatLaf Light with
 * SwingPreferences of a fresh store, over MultiLevelPinkNoiseWindModels made here. The dialog itself (a JDialog) is
 * not made: headless Swing refuses windows, and the scout's dialog walk (handoff/m4-scout, sim-config's
 * java/dialog-walk.txt, "=== wind editor") has it. It prints:
 * - HEADER: the table's header cells (label, tool tip, the unit selector's unit);
 * - ROW: the table's rows as they are laid out (the private list rows, which the panel shows in order): each
 *   spinner's text (altitude, speed, direction, deviation, turbulence), the intensity label and whether the row's
 *   delete button is enabled; MODEL: the altitudes of the model's levels in the model's order (getLevels());
 * - STEP: what a user does (texts set on the spinner models, the header's units set, the buttons' actions called:
 *   addRow(), the row's delete button with doClick(), resetLevels()), each followed by ROW and MODEL lines:
 *   the bounds of the cells, the coupling of speed, deviation and turbulence, an altitude edit that reorders the
 *   rows but not the model (M4's bug 32), two levels at one altitude and the delete that removes both (bug 31);
 * - TIP: the visualization's tool tip of a level (WindProfileVisualization's private formatTooltipText(), by
 *   reflection), in the table's units, for MSL and AGL;
 * - WALK: the visualization's check box.
 */
public final class WindEditorProbe {
	private final PrintStream out;

	private WindEditorProbe(PrintStream out) {
		this.out = out;
	}

	private static String clean(String s) {
		return s == null ? "" : s.replace("\n", "\\n");
	}

	private static Object field(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	private static <T> List<T> find(Component c, Class<T> type) {
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

	private static String text(JSpinner spinner) {
		return ((JSpinner.DefaultEditor) spinner.getEditor()).getTextField().getText();
	}

	// ------------------------------------------------------------------------------- the table

	private void header(MultiLevelWindTable table) {
		List<JLabel> labels = new ArrayList<>();
		for (JLabel l : find(table.getHeaderPanel(), JLabel.class)) {
			if (!(l instanceof UnitSelector)) {
				labels.add(l);
			}
		}
		List<UnitSelector> selectors = find(table.getHeaderPanel(), UnitSelector.class);
		for (int i = 0; i < labels.size(); i++) {
			JLabel l = labels.get(i);
			String unit = i < selectors.size() ? selectors.get(i).getSelectedUnit().getUnit() : "none";
			out.println("HEADER " + i + " [" + clean(l.getText()) + "]"
					+ (l.getToolTipText() != null ? " tip=" + clean(l.getToolTipText()) : "") + " unit=[" + unit + "]");
		}
	}

	@SuppressWarnings("unchecked")
	private static List<JComponent> rowsOf(MultiLevelWindTable table) throws Exception {
		return (List<JComponent>) field(table, MultiLevelWindTable.class, "rows");
	}

	private void rows(MultiLevelWindTable table, MultiLevelPinkNoiseWindModel model) throws Exception {
		List<JComponent> rows = rowsOf(table);
		for (int i = 0; i < rows.size(); i++) {
			JComponent row = rows.get(i);
			StringBuilder sb = new StringBuilder("ROW " + i);
			for (JSpinner spinner : find(row, JSpinner.class)) {
				sb.append(" [").append(text(spinner)).append(']');
			}
			JLabel intensity = (JLabel) field(row, row.getClass(), "intensityLabel");
			JButton delete = (JButton) field(row, row.getClass(), "deleteButton");
			sb.append(" intensity=[").append(intensity.getText()).append("] delete=")
					.append(delete.isEnabled() ? "enabled" : "disabled");
			out.println(sb);
		}
		StringBuilder sb = new StringBuilder("MODEL");
		for (LevelWindModel level : model.getLevels()) {
			sb.append(' ').append(level.getAltitude());
		}
		out.println(sb);
	}

	private JSpinner spinner(MultiLevelWindTable table, int row, int column) throws Exception {
		return find(rowsOf(table).get(row), JSpinner.class).get(column);
	}

	private void type(MultiLevelWindTable table, MultiLevelPinkNoiseWindModel model, int row, int column,
			String typed) throws Exception {
		String failure = "";
		try {
			spinner(table, row, column).getModel().setValue(typed);
		} catch (RuntimeException e) {
			failure = " EXCEPTION " + e.getClass().getName() + ": " + clean(e.getMessage());
		}
		out.println("STEP row " + row + " column " + column + " typed=[" + typed + "]" + failure);
		rows(table, model);
	}

	private void clickDelete(MultiLevelWindTable table, MultiLevelPinkNoiseWindModel model, int row)
			throws Exception {
		JComponent r = rowsOf(table).get(row);
		((JButton) field(r, r.getClass(), "deleteButton")).doClick();
		out.println("STEP delete button of row " + row);
		rows(table, model);
	}

	private void units(MultiLevelWindTable table, MultiLevelPinkNoiseWindModel model, String altitude, String speed,
			String direction, String deviation, String turbulence) throws Exception {
		table.setAltitudeUnit(UnitGroup.UNITS_DISTANCE.getUnit(altitude));
		table.setSpeedUnit(UnitGroup.UNITS_WINDSPEED.getUnit(speed));
		table.setDirectionUnit(UnitGroup.UNITS_ANGLE.getUnit(direction));
		table.setStdDeviationUnit(UnitGroup.UNITS_WINDSPEED.getUnit(deviation));
		UnitSelector turbulenceSelector = (UnitSelector) field(table, MultiLevelWindTable.class,
				"turbulenceUnitSelector");
		Unit turbulenceUnit = null;
		for (Unit u : UnitGroup.UNITS_RELATIVE.getUnits()) {
			if (u.getUnit().equals(turbulence)) {
				turbulenceUnit = u;
			}
		}
		turbulenceSelector.setSelectedUnit(turbulenceUnit);
		out.println("STEP units altitude=[" + altitude + "] speed=[" + speed + "] direction=[" + direction
				+ "] deviation=[" + deviation + "] turbulence=[" + turbulence + "]");
		header(table);
		rows(table, model);
	}

	private void tips(WindProfilePanel panel, MultiLevelPinkNoiseWindModel model, String title) throws Exception {
		Object visualization = field(panel, WindProfilePanel.class, "visualization");
		Method format = visualization.getClass().getDeclaredMethod("formatTooltipText", LevelWindModel.class);
		format.setAccessible(true);
		for (LevelWindModel level : model.getLevels()) {
			out.println("TIP " + title + " " + level.getAltitude() + " [" + clean((String) format.invoke(visualization,
					level)) + "]");
		}
	}

	private static MultiLevelPinkNoiseWindModel threeLevels() {
		MultiLevelPinkNoiseWindModel model = new MultiLevelPinkNoiseWindModel();
		model.clearLevels();
		model.addWindLevel(0, 3, Math.PI / 2, 0.3);
		model.addWindLevel(500, 7.5, Math.PI, 1.0);
		model.addWindLevel(1250.5, 12.25, 0.0, 3.0);
		return model;
	}

	private MultiLevelPinkNoiseWindModel model;
	private MultiLevelWindTable table;

	/** The steps up to the reordering altitude edit, whose deferred flash (invokeLater) runs before run2(). */
	private void run1() throws Exception {
		out.println("== the default model (one level from the preferences' average wind)");
		MultiLevelPinkNoiseWindModel initial = new MultiLevelPinkNoiseWindModel();
		MultiLevelWindTable initialTable = new MultiLevelWindTable(initial);
		header(initialTable);
		rows(initialTable, initial);
		ApplicationPreferences preferences = Application.getPreferences();
		out.println("PREFERENCES average=" + preferences.getAverageWindModel().getAverage() + " direction="
				+ preferences.getAverageWindModel().getDirection() + " deviation="
				+ preferences.getAverageWindModel().getStandardDeviation());

		out.println("== three levels");
		model = threeLevels();
		table = new MultiLevelWindTable(model);
		WindProfilePanel panel = new WindProfilePanel(model, table);
		rows(table, model);
		tips(panel, model, "MSL m");
		for (JCheckBox box : find(panel, JCheckBox.class)) {
			out.println("WALK CHECK " + box.getText() + " selected=" + box.isSelected());
		}
		table.updateAltitudeHeader(WindModel.AltitudeReference.AGL);
		model.setAltitudeReference(WindModel.AltitudeReference.AGL);
		out.println("STEP AGL");
		header(table);
		tips(panel, model, "AGL m");
		table.updateAltitudeHeader(WindModel.AltitudeReference.MSL);
		model.setAltitudeReference(WindModel.AltitudeReference.MSL);
		out.println("STEP MSL");
		header(table);

		out.println("== the units");
		units(table, model, "ft", "mph", "rad", "km/h", "\u2030");
		tips(panel, model, "MSL ft");
		units(table, model, "m", "m/s", "\u00b0", "m/s", "%");

		out.println("== the cells' bounds and couplings");
		type(table, model, 0, 0, "-200");
		type(table, model, 0, 0, "0");
		type(table, model, 0, 1, "-5");
		type(table, model, 0, 1, "6");
		type(table, model, 0, 2, "400");
		type(table, model, 0, 2, "-30");
		type(table, model, 0, 2, "90");
		type(table, model, 0, 3, "10");
		type(table, model, 0, 3, "1.2");
		type(table, model, 0, 4, "150");
		type(table, model, 0, 4, "4");
		type(table, model, 0, 4, "12");
		type(table, model, 0, 4, "22");
		type(table, model, 0, 1, "0");
		type(table, model, 0, 1, "3");
		type(table, model, 1, 0, "2*300");
		type(table, model, 1, 0, "abc");

		out.println("== add a level");
		table.addRow();
		out.println("STEP addRow");
		rows(table, model);

		out.println("== an altitude edit that reorders the rows (M4's bug 32)");
		type(table, model, 0, 0, "800");
		out.println("SORTED " + isSorted(model));
	}

	/** The rest. */
	private void run2() throws Exception {
		out.println("== the delete buttons");
		clickDelete(table, model, 1);

		out.println("== two levels at one altitude, then a delete (M4's bug 31)");
		MultiLevelPinkNoiseWindModel duplicates = threeLevels();
		MultiLevelWindTable duplicateTable = new MultiLevelWindTable(duplicates);
		rows(duplicateTable, duplicates);
		type(duplicateTable, duplicates, 1, 0, "0");
		clickDelete(duplicateTable, duplicates, 0);

		out.println("== delete down to one level");
		MultiLevelPinkNoiseWindModel two = new MultiLevelPinkNoiseWindModel();
		two.clearLevels();
		two.addWindLevel(0, 2, 0, 0.1);
		two.addWindLevel(100, 4, 1, 0.2);
		MultiLevelWindTable twoTable = new MultiLevelWindTable(two);
		rows(twoTable, two);
		clickDelete(twoTable, two, 0);
		clickDelete(twoTable, two, 0);

		out.println("== add to a table of one level");
		MultiLevelPinkNoiseWindModel one = new MultiLevelPinkNoiseWindModel();
		one.clearLevels();
		one.addWindLevel(-50, 5.5, 2, 0.5);
		MultiLevelWindTable oneTable = new MultiLevelWindTable(one);
		rows(oneTable, one);
		oneTable.addRow();
		out.println("STEP addRow");
		rows(oneTable, one);

		out.println("== reset levels");
		table.resetLevels();
		out.println("STEP resetLevels");
		rows(table, model);
	}

	private static boolean isSorted(MultiLevelPinkNoiseWindModel model) {
		List<LevelWindModel> levels = model.getLevels();
		for (int i = 1; i < levels.size(); i++) {
			if (levels.get(i - 1).getAltitude() > levels.get(i).getAltitude()) {
				return false;
			}
		}
		return true;
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
		WindEditorProbe probe = new WindEditorProbe(out);
		final Exception[] failure = new Exception[1];
		SwingUtilities.invokeAndWait(() -> {
			try {
				probe.run1();
			} catch (Exception e) {
				failure[0] = e;
			}
		});
		SwingUtilities.invokeAndWait(() -> {
			try {
				if (failure[0] == null) {
					probe.run2();
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
}
