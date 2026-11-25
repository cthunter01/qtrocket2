package probe;

import java.awt.Color;
import java.awt.Component;
import java.awt.Container;
import java.awt.Rectangle;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.prefs.Preferences;

import javax.swing.AbstractButton;
import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JRadioButton;
import javax.swing.JScrollPane;
import javax.swing.JSeparator;
import javax.swing.JTable;
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
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.document.PlotAppearance;
import info.openrocket.core.document.Simulation;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightData;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.SimulationOptions;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.util.LineStyle;
import info.openrocket.swing.gui.components.ColorChooserButton;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.plot.PlotPanel;
import info.openrocket.swing.gui.plot.PlotTypeSelector;
import info.openrocket.swing.gui.plot.SimulationPlotConfiguration;
import info.openrocket.swing.gui.simulation.SimulationPlotPanel;
import info.openrocket.swing.gui.simulation.SimulationPlotTypeSelector;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;
import net.miginfocom.swing.MigLayout;

/**
 * What OpenRocket's plot tab (the "Plot data" tab of the simulation dialog: swing gui/plot/PlotPanel.java,
 * PlotTypeSelector.java, gui/simulation/SimulationPlotPanel.java and SimulationPlotTypeSelector.java) is and
 * does, for Milestone 4's part G5 (dialogs/plot/PlotPanel, PlotTypeSelector, SimulationPlotPanel,
 * SimulationPlotTypeSelector and FlightEventTableModel): SimulationPlotPanel.create() of stored simulations of
 * the examples (data/examples: "A simple model rocket" #1, "Three stage low power rocket" #1) and of the
 * flight of SimulationChartTest's colour test, under FlatLaf Light with SwingPreferences at their defaults, in
 * a JFrame that is packed and never shown (the probe runs under xvfb-run with its plot-dialog family). For each
 * panel: its components in order with their MigLayout constraints, the rows (type, unit, axis, colour, line
 * style, the appearance buttons' states), the events table and the marker style; their bounds after pack()
 * (the tests pin only what does not depend on the fonts: the fixed widths and the gaps); then what the preset
 * box, the X axis, the rows' type, unit, axis, colour and line style, New Y axis plot type, the remove buttons,
 * All and None, a click on an event's name, Set as default, Reset to factory default and doPlot() do. The
 * user's choices are made as Swing makes them (setSelectedItem(), setSelectedIndex(), doClick()): the same
 * listeners run. The whole transcript is pinned by hand (hand-pinned.txt, after PlotDialogProbe's lines).
 */
public class PlotTabProbe {

	public static void main(String[] args) throws Exception {
		bootstrap();
		File examples = new File(args[0]);
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> GUIUtil.setUITheme(UITheme.Themes.LIGHT));
			OpenRocketDocument simple = load(new File(examples, "A simple model rocket.ork"));
			OpenRocketDocument three = load(new File(examples, "Three stage low power rocket.ork"));
			Simulation simpleSim = simple.getSimulations().get(0);
			Simulation threeSim = three.getSimulations().get(0);
			SwingUtilities.invokeAndWait(() -> {
				try {
					panel("simple", simpleSim);
					presets(simpleSim);
					edits(simpleSim);
					appearances(simpleSim);
					events(simpleSim);
					plotted(simpleSim, threeSim);
					panel("three-stage", threeSim);
					colourTest();
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

	static OpenRocketDocument load(File file) throws Exception {
		DeterministicUuids.reseed(file.getName());
		return new GeneralRocketLoader(file).load();
	}

	//// Reflection on the panels' fields and methods

	static Object field(Object object, Class<?> declaring, String name) throws Exception {
		Field f = declaring.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	static void setStatic(Class<?> declaring, String name, Object value) throws Exception {
		Field f = declaring.getDeclaredField(name);
		f.setAccessible(true);
		f.set(null, value);
	}

	static Object staticField(Class<?> declaring, String name) throws Exception {
		Field f = declaring.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(null);
	}

	static JComboBox<?> presetBox(SimulationPlotPanel panel) throws Exception {
		return (JComboBox<?>) field(panel, PlotPanel.class, "configurationSelector");
	}

	static JComboBox<?> domainBox(SimulationPlotPanel panel) throws Exception {
		return (JComboBox<?>) field(panel, PlotPanel.class, "domainTypeSelector");
	}

	static UnitSelector domainUnit(SimulationPlotPanel panel) throws Exception {
		return (UnitSelector) field(panel, PlotPanel.class, "domainUnitSelector");
	}

	static JComboBox<?> typeBox(PlotTypeSelector<?, ?> row) throws Exception {
		return (JComboBox<?>) field(row, PlotTypeSelector.class, "typeSelector");
	}

	static UnitSelector unitOf(PlotTypeSelector<?, ?> row) throws Exception {
		return (UnitSelector) field(row, PlotTypeSelector.class, "unitSelector");
	}

	static JComboBox<?> axisBox(PlotTypeSelector<?, ?> row) throws Exception {
		return (JComboBox<?>) field(row, PlotTypeSelector.class, "axisSelector");
	}

	static JButton removeButton(PlotTypeSelector<?, ?> row) throws Exception {
		return (JButton) field(row, PlotTypeSelector.class, "removeButton");
	}

	static ColorChooserButton colorButton(SimulationPlotTypeSelector row) throws Exception {
		return (ColorChooserButton) field(row, SimulationPlotTypeSelector.class, "colorButton");
	}

	static JComboBox<?> lineStyleBox(SimulationPlotTypeSelector row) throws Exception {
		return (JComboBox<?>) field(row, SimulationPlotTypeSelector.class, "lineStyleSelector");
	}

	static JButton setDefaultButton(SimulationPlotTypeSelector row) throws Exception {
		return (JButton) field(row, SimulationPlotTypeSelector.class, "setDefaultButton");
	}

	static JButton resetButton(SimulationPlotTypeSelector row) throws Exception {
		return (JButton) field(row, SimulationPlotTypeSelector.class, "resetToFactoryButton");
	}

	static TableModel eventModel(SimulationPlotPanel panel) throws Exception {
		return (TableModel) field(panel, SimulationPlotPanel.class, "eventTableModel");
	}

	static void invoke(Object object, Class<?> declaring, String name, Class<?>[] types, Object... arguments)
			throws Exception {
		Method m = declaring.getDeclaredMethod(name, types);
		m.setAccessible(true);
		try {
			m.invoke(object, arguments);
		} catch (InvocationTargetException e) {
			if (e.getCause() instanceof Exception cause) {
				throw cause;
			}
			throw e;
		}
	}

	static void setConfiguration(SimulationPlotPanel panel, SimulationPlotConfiguration configuration)
			throws Exception {
		invoke(panel, PlotPanel.class, "setConfiguration",
				new Class<?>[] { Class.forName("info.openrocket.swing.gui.plot.PlotConfiguration") },
				configuration);
	}

	static void updatePlots(SimulationPlotPanel panel) throws Exception {
		invoke(panel, SimulationPlotPanel.class, "updatePlots", new Class<?>[0]);
	}

	//// Finding components

	static <T extends Component> List<T> find(Container container, Class<T> type) {
		List<T> found = new ArrayList<>();
		for (Component c : container.getComponents()) {
			if (type.isInstance(c)) {
				found.add(type.cast(c));
			}
			if (c instanceof Container child) {
				found.addAll(find(child, type));
			}
		}
		return found;
	}

	static List<SimulationPlotTypeSelector> rows(SimulationPlotPanel panel) {
		List<SimulationPlotTypeSelector> rows = find(panel, SimulationPlotTypeSelector.class);
		rows.sort(Comparator.comparingInt(PlotTypeSelector::getIndex));
		return rows;
	}

	//// Printing

	static String rgb(Color color) {
		return color == null ? "none"
				: String.format("#%02x%02x%02x%s", color.getRed(), color.getGreen(), color.getBlue(),
						color.getAlpha() == 255 ? "" : String.format("/%d", color.getAlpha()));
	}

	static String kind(Component c) {
		String name = c.getClass().getSimpleName();
		if (name.isEmpty()) {
			name = c.getClass().getSuperclass().getSimpleName();
		}
		return name;
	}

	static String describe(Component c) {
		StringBuilder b = new StringBuilder(kind(c));
		if (c instanceof JLabel label) {
			b.append(" text='").append(label.getText()).append("'");
		} else if (c instanceof AbstractButton button) {
			b.append(" text='").append(button.getText()).append("'");
			if (button.getIcon() != null) {
				b.append(" icon");
			}
		}
		if (c instanceof JComponent j && j.getToolTipText() != null) {
			b.append(" tooltip='").append(j.getToolTipText()).append("'");
		}
		b.append(" visible=").append(c.isVisible()).append(" enabled=").append(c.isEnabled());
		return b.toString();
	}

	static void components(String what, Container container) {
		if (!(container.getLayout() instanceof MigLayout layout)) {
			System.out.println(what + " layout=" + container.getLayout().getClass().getSimpleName());
			return;
		}
		System.out.println(what + " layout='" + layout.getLayoutConstraints() + "' columns='"
				+ layout.getColumnConstraints() + "' rows='" + layout.getRowConstraints() + "'");
		int i = 0;
		for (Component c : container.getComponents()) {
			System.out.println("  " + i + ": " + describe(c) + " constraints='" + layout.getComponentConstraints(c)
					+ "'");
			i++;
		}
	}

	static String config(SimulationPlotConfiguration c) {
		StringBuilder b = new StringBuilder("name='" + c.getName() + "' X=" + c.getDomainAxisType().getName() + " ["
				+ c.getDomainAxisUnit().getUnit() + "] rows=" + c.getDataCount() + ":");
		for (int i = 0; i < c.getDataCount(); i++) {
			b.append(" ").append(c.getType(i).getName()).append(" [").append(c.getUnit(i).getUnit()).append("] @")
					.append(c.getAxis(i)).append(" color=").append(rgb(c.getPlotDataColor(i))).append(" ")
					.append(c.getPlotDataLineStyle(i));
			b.append(i + 1 < c.getDataCount() ? ";" : "");
		}
		b.append(" events=").append(c.getActiveEvents());
		return b.toString();
	}

	static String rowsOf(SimulationPlotPanel panel) throws Exception {
		StringBuilder b = new StringBuilder();
		for (SimulationPlotTypeSelector row : rows(panel)) {
			b.append(b.length() == 0 ? "" : " | ");
			b.append(row.getIndex()).append(" ").append(row.getSelectedType().getName()).append(" [")
					.append(row.getSelectedUnit().getUnit()).append("] ")
					.append(axisBox(row).getSelectedItem()).append(" ").append(rgb(row.getSelectedColor())).append(" ")
					.append(row.getSelectedLineStyle()).append(" setDefault=")
					.append(setDefaultButton(row).isEnabled() ? "on" : "off").append(" reset=")
					.append(resetButton(row).isEnabled() ? "on" : "off");
		}
		return b.toString();
	}

	static String eventsOf(SimulationPlotPanel panel) throws Exception {
		TableModel model = eventModel(panel);
		StringBuilder b = new StringBuilder(model.getRowCount() + ":");
		for (int r = 0; r < model.getRowCount(); r++) {
			b.append(" ").append(model.getValueAt(r, 1)).append("=").append(model.getValueAt(r, 0));
			b.append(r + 1 < model.getRowCount() ? ";" : "");
		}
		return b.toString();
	}

	static String extras(SimulationPlotPanel panel) {
		StringBuilder b = new StringBuilder();
		for (Component c : find(panel, JComponent.class)) {
			if (kind(c).equals("DescriptionArea")) {
				b.append("description visible=").append(c.isVisible());
			}
		}
		for (JLabel label : find(panel, JLabel.class)) {
			if ("Marker style:".equals(label.getText())) {
				b.append(" marker style enabled=").append(label.isEnabled()).append(" tooltip='")
						.append(label.getToolTipText()).append("'");
			}
		}
		for (JRadioButton radio : find(panel, JRadioButton.class)) {
			b.append(" ").append(radio.getText()).append(" enabled=").append(radio.isEnabled()).append(" selected=")
					.append(radio.isSelected()).append(" tooltip=")
					.append(radio.getToolTipText() == null ? "none" : "'" + radio.getToolTipText() + "'");
		}
		return b.toString();
	}

	static void state(String what, SimulationPlotPanel panel) throws Exception {
		JComboBox<?> presets = presetBox(panel);
		System.out.println(what + ": preset box=" + presets.getSelectedIndex() + " '" + presets.getSelectedItem()
				+ "' X box='" + ((FlightDataType) domainBox(panel).getSelectedItem()).getName() + "' X unit="
				+ domainUnit(panel).getSelectedUnit().getUnit());
		System.out.println("  configuration: " + config(panel.getConfiguration()));
		System.out.println("  rows: " + rowsOf(panel));
		System.out.println("  " + extras(panel));
	}

	static String bounds(Component c) {
		Rectangle r = c.getBounds();
		return r.x + "," + r.y + " " + r.width + "x" + r.height;
	}

	/** The horizontal gap from the right edge of {@code left} to the left edge of {@code right}, one parent. */
	static int gap(Component left, Component right) {
		return right.getX() - (left.getX() + left.getWidth());
	}

	//// The panel as made

	static SimulationPlotPanel make(Simulation sim) {
		SimulationPlotPanel panel = SimulationPlotPanel.create(sim);
		JFrame frame = new JFrame();
		frame.add(panel);
		frame.pack();
		return panel;
	}

	static void dispose(SimulationPlotPanel panel) {
		SwingUtilities.getWindowAncestor(panel).dispose();
	}

	static void panel(String name, Simulation sim) throws Exception {
		System.out.println("== plot tab " + name + ": '" + sim.getName() + "' branches "
				+ sim.getSimulatedData().getBranchCount());
		FlightDataType[] types = sim.getSimulatedData().getBranch(0).getTypes();
		StringBuilder b = new StringBuilder("types " + types.length + ":");
		for (FlightDataType t : types) {
			b.append(" ").append(t.getName()).append(t == types[types.length - 1] ? "" : " |");
		}
		System.out.println(b);
		SimulationPlotPanel panel = make(sim);
		components("panel", panel);
		JComboBox<?> presets = presetBox(panel);
		StringBuilder items = new StringBuilder("preset box items " + presets.getItemCount() + ":");
		for (int i = 0; i < presets.getItemCount(); i++) {
			items.append(" '").append(presets.getItemAt(i)).append("'");
		}
		System.out.println(items);
		// The Y axis panel and the extra Y panel: the panel's components 8 and 9.
		Container yPanel = (Container) panel.getComponent(8);
		components("y panel", yPanel);
		JScrollPane scroll = (JScrollPane) yPanel.getComponent(1);
		Container rowsPanel = (Container) scroll.getViewport().getView();
		components("rows panel", rowsPanel);
		Container extraY = (Container) panel.getComponent(9);
		components("extra y panel", extraY);
		components("events panel", (Container) extraY.getComponent(1));
		SimulationPlotTypeSelector row0 = rows(panel).get(0);
		components("row 0", row0);
		components("row 0 appearance", (Container) row0.getComponent(row0.getComponentCount() - 1));
		JTable table = find(panel, JTable.class).get(0);
		System.out.println("events table: header=" + (table.getTableHeader() != null) + " showVerticalLines="
				+ table.getShowVerticalLines() + " showHorizontalLines=" + table.getShowHorizontalLines()
				+ " rowSelectionAllowed=" + table.getRowSelectionAllowed() + " columnSelectionAllowed="
				+ table.getColumnSelectionAllowed() + " column 0 width = row height + 2: "
				+ (table.getColumnModel().getColumn(0).getWidth() == table.getRowHeight() + 2)
				+ " scroll preferred height=" + table.getParent().getParent().getPreferredSize().height
				+ " column 1 class=" + table.getModel().getColumnClass(1).getSimpleName() + " editable 0/1="
				+ table.getModel().isCellEditable(0, 0) + "/" + table.getModel().isCellEditable(0, 1));
		state("initial", panel);
		System.out.println("  events: " + eventsOf(panel));
		geometry(panel, row0);
		dispose(panel);
	}

	static void geometry(SimulationPlotPanel panel, SimulationPlotTypeSelector row) throws Exception {
		JComboBox<?> type = typeBox(row);
		Component unitLabel = row.getComponent(1);
		UnitSelector unit = unitOf(row);
		Component axisLabel = row.getComponent(3);
		JComboBox<?> axis = axisBox(row);
		JButton remove = removeButton(row);
		System.out.println("row 0 geometry: type width=" + type.getWidth() + " gap type-'Unit:'=" + gap(type, unitLabel)
				+ " gap 'Unit:'-unit=" + gap(unitLabel, unit) + " unit width=" + unit.getWidth()
				+ " gap unit-'Axis:'=" + gap(unit, axisLabel) + " gap 'Axis:'-axis=" + gap(axisLabel, axis)
				+ " gap axis-remove=" + gap(axis, remove) + " type y=" + type.getY());
		Container appearance = (Container) row.getComponent(row.getComponentCount() - 1);
		System.out.println("row 0 appearance: x=" + appearance.getX() + " (the type box's x=" + type.getX()
				+ ") gaps: " + gap(appearance.getComponent(0), appearance.getComponent(1)) + " "
				+ gap(appearance.getComponent(1), appearance.getComponent(2)) + " "
				+ gap(appearance.getComponent(2), appearance.getComponent(3)) + " "
				+ gap(appearance.getComponent(3), appearance.getComponent(4)) + " "
				+ gap(appearance.getComponent(4), appearance.getComponent(5)) + " vertical gap to the first line="
				+ (appearance.getY() - (type.getY() + type.getHeight())));
		JComboBox<?> domain = domainBox(panel);
		Component domainUnitLabel = panel.getComponent(5);
		UnitSelector domainUnit = domainUnit(panel);
		System.out.println("x axis geometry: type width=" + domain.getWidth() + " gap type-'Unit:'="
				+ gap(domain, domainUnitLabel) + " gap 'Unit:'-unit=" + gap(domainUnitLabel, domainUnit)
				+ " unit width=" + domainUnit.getWidth() + " gap unit-description="
				+ gap(domainUnit, panel.getComponent(7)));
		JComboBox<?> presets = presetBox(panel);
		Component separator = panel.getComponent(2);
		System.out.println("panel geometry: gap preset box-separator="
				+ (separator.getY() - (presets.getY() + presets.getHeight())) + " gap separator-X row="
				+ (domain.getY() - (separator.getY() + separator.getHeight())) + " y panel-extra y gap="
				+ gap(panel.getComponent(8), panel.getComponent(9)) + " new Y button x="
				+ panel.getComponent(10).getX() + " panel insets left=" + panel.getComponent(0).getX());
		JButton all = null;
		JButton none = null;
		for (JButton b : find(panel, JButton.class)) {
			if ("All".equals(b.getText())) {
				all = b;
			} else if ("None".equals(b.getText())) {
				none = b;
			}
		}
		JScrollPane tableScroll = find(panel, JScrollPane.class).get(1);
		System.out.println("events geometry: All and None equally wide=" + (all.getWidth() == none.getWidth())
				+ " All x - table x=" + (all.getX() - tableScroll.getX()) + " gap All-None=" + gap(all, none));
	}

	//// The preset box

	static void presets(Simulation sim) throws Exception {
		System.out.println("== presets of 'A simple model rocket' #1");
		SimulationPlotPanel panel = make(sim);
		JComboBox<?> presets = presetBox(panel);
		for (int k = 0; k < 9; k++) {
			presets.setSelectedIndex(k);
			state("preset " + k, panel);
		}
		presets.setSelectedIndex(9);
		state("Custom chosen (the configuration keeps its name)", panel);
		presets.setSelectedIndex(2);
		state("preset 2 again: the X type is the position, the description and the marker style do not follow",
				panel);
		dispose(panel);
	}

	//// The X axis, the rows, New and remove

	static void edits(Simulation sim) throws Exception {
		System.out.println("== edits of 'A simple model rocket' #1");
		SimulationPlotPanel panel = make(sim);
		JComboBox<?> presets = presetBox(panel);
		presets.setSelectedIndex(0);
		domainBox(panel).setSelectedItem(FlightDataType.TYPE_MACH_NUMBER);
		state("X axis -> Mach number", panel);
		presets.setSelectedIndex(0);
		state("preset 0 after it: the description and the marker style stay", panel);
		domainBox(panel).setSelectedItem(FlightDataType.TYPE_TIME);
		state("X axis -> Time", panel);
		presets.setSelectedIndex(0);
		domainUnit(panel).setSelectedUnit(FlightDataType.TYPE_TIME.getUnitGroup().getUnit("min"));
		state("X unit -> min (no Custom)", panel);
		presets.setSelectedIndex(1);
		presets.setSelectedIndex(0);
		unitOf(rows(panel).get(0)).setSelectedUnit(FlightDataType.TYPE_ALTITUDE.getUnitGroup().getUnit("ft"));
		state("row 0 unit -> ft (no Custom)", panel);
		axisBox(rows(panel).get(1)).setSelectedIndex(2);
		state("row 1 axis -> Right (no Custom; the colours are computed before the axis changes)", panel);
		updatePlots(panel);
		state("updatePlots", panel);
		typeBox(rows(panel).get(2)).setSelectedItem(FlightDataType.TYPE_MASS);
		state("row 2 type -> Mass (Custom; the colours are computed before the type changes)", panel);
		for (int n = 0; n < 3; n++) {
			newY(panel);
			state("New Y axis plot type", panel);
		}
		removeButton(rows(panel).get(1)).doClick();
		state("row 1 removed", panel);
		presets.setSelectedIndex(8);
		state("preset 8", panel);
		removeButton(rows(panel).get(1)).doClick();
		state("row 1 removed", panel);
		try {
			removeButton(rows(panel).get(0)).doClick();
			state("the last row removed", panel);
		} catch (Exception e) {
			System.out.println("the last row removed: " + e.getClass().getSimpleName() + ": " + e.getMessage());
		}
		dispose(panel);
		panel = make(sim);
		presets = presetBox(panel);
		for (int n = 0; n < 12; n++) {
			newY(panel);
		}
		state("12 times New Y axis plot type (the next would show 'A maximum of 15 plots is allowed.')", panel);
		dispose(panel);
	}

	static void newY(SimulationPlotPanel panel) {
		for (JButton b : find(panel, JButton.class)) {
			if ("New Y axis plot type".equals(b.getText())) {
				b.doClick();
				return;
			}
		}
		throw new IllegalStateException("no New Y axis plot type");
	}

	//// Appearances

	static void appearances(Simulation sim) throws Exception {
		System.out.println("== appearances of 'A simple model rocket' #1");
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		SimulationPlotPanel panel = make(sim);
		presetBox(panel).setSelectedIndex(0);
		SimulationPlotTypeSelector row = rows(panel).get(0);
		colorButton(row).setSelectedColor(new Color(200, 100, 50));
		state("row 0 colour -> #c86432", panel);
		System.out.println("  simulation's Altitude appearance: " + appearance(sim.getPlotAppearance(FlightDataType.TYPE_ALTITUDE)));
		lineStyleBox(row).setSelectedItem(LineStyle.DASHED);
		state("row 0 line style -> DASHED", panel);
		System.out.println("  simulation's Altitude appearance: " + appearance(sim.getPlotAppearance(FlightDataType.TYPE_ALTITUDE)));
		setDefaultButton(row).doClick();
		state("row 0 Set as default", panel);
		System.out.println("  default of Altitude: " + appearance(prefs.getDefaultPlotAppearance(FlightDataType.TYPE_ALTITUDE))
				+ " stored keys: " + storedKeys());
		resetButton(row).doClick();
		state("row 0 Reset to factory default", panel);
		System.out.println("  simulation's Altitude appearance: " + appearance(sim.getPlotAppearance(FlightDataType.TYPE_ALTITUDE)));
		typeBox(rows(panel).get(1)).setSelectedItem(FlightDataType.TYPE_ALTITUDE);
		state("row 1 type -> Altitude (its default appearance)", panel);
		lineStyleBox(rows(panel).get(2)).setSelectedItem(LineStyle.DOTTED);
		state("row 2 line style -> DOTTED", panel);
		dispose(panel);
		panel = make(sim);
		state("a new panel: the simulation's appearance of Vertical acceleration and the default of Altitude", panel);
		dispose(panel);
		prefs.setDefaultPlotAppearance(FlightDataType.TYPE_ALTITUDE, null);
		sim.setPlotAppearance(FlightDataType.TYPE_ALTITUDE, null);
		sim.setPlotAppearance(FlightDataType.TYPE_VELOCITY_Z, null);
		sim.setPlotAppearance(FlightDataType.TYPE_ACCELERATION_Z, null);
		System.out.println("  stored keys after clearing: " + storedKeys());
	}

	static String appearance(PlotAppearance a) {
		if (a == null) {
			return "none";
		}
		return "color=" + (a.getColor() == null ? "none" : a.getColor().getRed() + "," + a.getColor().getGreen() + ","
				+ a.getColor().getBlue() + "," + a.getColor().getAlpha()) + " lineStyle="
				+ (a.getLineStyle() == null ? "none" : a.getLineStyle().name());
	}

	static String storedKeys() throws Exception {
		Preferences node = ((SwingPreferences) Application.getPreferences()).getNode("PlotDefaultAppearances");
		List<String> keys = new ArrayList<>();
		for (String key : node.keys()) {
			keys.add(key + "=" + node.get(key, null));
		}
		Collections.sort(keys);
		return keys.toString();
	}

	//// Events and the marker style

	static void events(Simulation sim) throws Exception {
		System.out.println("== events of 'A simple model rocket' #1");
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		SimulationPlotPanel panel = make(sim);
		presetBox(panel).setSelectedIndex(0);
		JButton all = null;
		JButton none = null;
		for (JButton b : find(panel, JButton.class)) {
			if ("All".equals(b.getText())) {
				all = b;
			} else if ("None".equals(b.getText())) {
				none = b;
			}
		}
		none.doClick();
		System.out.println("None: " + eventsOf(panel) + " active=" + panel.getConfiguration().getActiveEvents());
		all.doClick();
		System.out.println("All: " + eventsOf(panel) + " active=" + panel.getConfiguration().getActiveEvents());
		TableModel model = eventModel(panel);
		model.setValueAt(Boolean.FALSE, 1, 0);
		System.out.println("row 1 unchecked: " + eventsOf(panel));
		JTable table = find(panel, JTable.class).get(0);
		Rectangle cell = table.getCellRect(0, 1, true);
		java.awt.event.MouseEvent click = new java.awt.event.MouseEvent(table, java.awt.event.MouseEvent.MOUSE_CLICKED,
				0, 0, cell.x + 2, cell.y + 2, 1, false, java.awt.event.MouseEvent.BUTTON1);
		for (java.awt.event.MouseListener l : table.getMouseListeners()) {
			l.mouseClicked(click);
		}
		System.out.println("a click on row 0's name: " + eventsOf(panel));
		for (JRadioButton radio : find(panel, JRadioButton.class)) {
			if ("Icon".equals(radio.getText())) {
				radio.doClick();
			}
		}
		System.out.println("Icon clicked: pref MarkerStyleIcon=" + prefs.getBoolean(ApplicationPreferences.MARKER_STYLE_ICON, false)
				+ " " + extras(panel));
		for (JRadioButton radio : find(panel, JRadioButton.class)) {
			if ("Vertical line".equals(radio.getText())) {
				radio.doClick();
			}
		}
		System.out.println("Vertical line clicked: pref MarkerStyleIcon="
				+ prefs.getBoolean(ApplicationPreferences.MARKER_STYLE_ICON, false) + " " + extras(panel));
		dispose(panel);
		prefs.putBoolean(ApplicationPreferences.MARKER_STYLE_ICON, true);
		panel = make(sim);
		System.out.println("a new panel with MarkerStyleIcon=true: " + extras(panel));
		dispose(panel);
		prefs.putBoolean(ApplicationPreferences.MARKER_STYLE_ICON, false);
	}

	//// doPlot() and the configuration the next panel starts from

	static void plotted(Simulation simple, Simulation three) throws Exception {
		System.out.println("== doPlot");
		Object initial = staticField(SimulationPlotPanel.class, "DEFAULT_CONFIGURATION");
		SimulationPlotPanel panel = make(simple);
		JComboBox<?> presets = presetBox(panel);
		presets.setSelectedIndex(3);
		colorButton(rows(panel).get(0)).setSelectedColor(new Color(10, 20, 30));
		domainUnit(panel).setSelectedUnit(FlightDataType.TYPE_POSITION_X.getUnitGroup().getUnit("ft"));
		newY(panel);
		JDialog dialog = panel.doPlot(null);
		System.out.println("doPlot: a dialog=" + (dialog != null) + " the panel's: " + config(panel.getConfiguration()));
		System.out.println("  DEFAULT_CONFIGURATION: "
				+ config((SimulationPlotConfiguration) staticField(SimulationPlotPanel.class, "DEFAULT_CONFIGURATION")));
		dialog.dispose();
		dispose(panel);
		panel = make(three);
		state("the next panel (Three stage low power rocket #1)", panel);
		dispose(panel);
		panel = make(simple);
		state("and of 'A simple model rocket' #1 again", panel);
		dispose(panel);
		simple.setPlotAppearance(FlightDataType.TYPE_POSITION_Y, null);
		setStatic(SimulationPlotPanel.class, "DEFAULT_CONFIGURATION", initial);
	}

	//// SimulationChartTest.automaticColorsRefreshWhenLaterVariableChangesAxisAssignment, step by step

	static void colourTest() throws Exception {
		System.out.println("== SimulationChartTest's colour test");
		FlightDataBranch branch = new FlightDataBranch("Sustainer", FlightDataType.TYPE_TIME,
				FlightDataType.TYPE_ALTITUDE, FlightDataType.TYPE_VELOCITY_Z, FlightDataType.TYPE_ACCELERATION_Z,
				FlightDataType.TYPE_POSITION_X, FlightDataType.TYPE_POSITION_Y);
		for (int i = 0; i < 3; i++) {
			branch.addPoint();
			branch.setValue(FlightDataType.TYPE_TIME, i);
			branch.setValue(FlightDataType.TYPE_ALTITUDE, i * 100.0);
			branch.setValue(FlightDataType.TYPE_VELOCITY_Z, i * 20.0);
			branch.setValue(FlightDataType.TYPE_ACCELERATION_Z, i * 1000.0);
			branch.setValue(FlightDataType.TYPE_POSITION_X, i * 10.0);
			branch.setValue(FlightDataType.TYPE_POSITION_Y, i * 10.0);
		}
		OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
		Simulation simulation = new Simulation(document, document.getRocket(), Simulation.Status.UPTODATE, "Test",
				new SimulationOptions(), Collections.emptyList(), new FlightData(branch));
		SimulationPlotPanel panel = make(simulation);
		state("made", panel);
		SimulationPlotConfiguration configuration = new SimulationPlotConfiguration("Test", FlightDataType.TYPE_TIME);
		configuration.addPlotDataType(FlightDataType.TYPE_ALTITUDE, 1);
		configuration.addPlotDataType(FlightDataType.TYPE_VELOCITY_Z);
		configuration.addPlotDataType(FlightDataType.TYPE_ACCELERATION_Z);
		setConfiguration(panel, configuration);
		updatePlots(panel);
		state("setConfiguration, updatePlots", panel);
		typeBox(rows(panel).get(1)).setSelectedItem(FlightDataType.TYPE_POSITION_X);
		state("row 1 type -> Position East of launch", panel);
		typeBox(rows(panel).get(2)).setSelectedItem(FlightDataType.TYPE_POSITION_Y);
		state("row 2 type -> Position North of launch", panel);
		updatePlots(panel);
		state("updatePlots (the colours of the configuration as it is)", panel);
		// An axis: the colours are computed before it changes, too.
		setConfiguration(panel, configuration);
		updatePlots(panel);
		axisBox(rows(panel).get(1)).setSelectedIndex(1);
		state("again; row 1 axis -> Left", panel);
		updatePlots(panel);
		state("updatePlots (the colours of the configuration as it is)", panel);
		dispose(panel);
	}

	static void bootstrap() throws Exception {
		// Before anything makes a UUID (the pins' rule: every probe is seeded).
		DeterministicUuids.install();
		DeterministicUuids.reseed("plot-tab");
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
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
