package probe;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Component;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.Stroke;
import java.awt.event.ItemEvent;
import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.swing.Icon;
import javax.swing.JButton;
import javax.swing.JCheckBoxMenuItem;
import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.JMenu;
import javax.swing.JPanel;
import javax.swing.JPopupMenu;
import javax.swing.ListCellRenderer;
import javax.swing.ListModel;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.simulation.FlightDataBranch;
import info.openrocket.core.simulation.FlightDataType;
import info.openrocket.core.simulation.FlightDataTypeGroup;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.LineStyle;
import info.openrocket.swing.gui.components.ColorChooserButton;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.plot.PlotPanel;
import info.openrocket.swing.gui.plot.PlotTypeSelector;
import info.openrocket.swing.gui.plot.SimulationPlotConfiguration;
import info.openrocket.swing.gui.simulation.SimulationPlotTypeSelector;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.gui.widgets.GroupableAndSearchableComboBox;
import net.miginfocom.swing.MigLayout;

/**
 * What the plot widgets of Milestone 4's part K5 port, as OpenRocket's own Swing classes show them
 * (headless, FlatLaf Light, SwingPreferences at their defaults):
 * - the data type combo box (GroupableAndSearchableComboBox as PlotTypeSelector and PlotPanel make
 *   it over FlightDataTypes): its placeholder, its layout constraints, its tooltip, its groups popup
 *   with the check marks, its filter() searches and its item events;
 * - the unit selector of a bare unit group (UnitSelector(UnitGroup)): its text, its selected unit,
 *   setUnitGroup() and setSelectedUnit() with their item events and refusals;
 * - the line style combo box of SimulationPlotTypeSelector: its items, what its renderer draws
 *   (LineStyleIcon's size and stroke) and its action events;
 * - ColorChooserButton: its ColorIcon (size, what it paints enabled and disabled) and its
 *   property change events.
 * Its whole transcript is pinned by hand (hand-pinned.txt; README.md).
 */
public class PlotWidgetsProbe {
	static Object field(Class<?> type, Object object, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	/** A Graphics2D that prints the calls the icons make (their create() copies print too). */
	static final class Recorder extends DelegatingGraphics2D {
		Recorder(Graphics2D g) {
			super(g);
		}

		@Override
		public Graphics create() {
			System.out.println("    create()");
			return new Recorder((Graphics2D) g.create());
		}

		@Override
		public void setColor(Color c) {
			System.out.println("    setColor(" + rgba(c) + ")");
			g.setColor(c);
		}

		@Override
		public void setStroke(Stroke s) {
			if (s instanceof BasicStroke b) {
				System.out.println("    setStroke(width=" + b.getLineWidth() + " cap=" + b.getEndCap() + " join="
						+ b.getLineJoin() + " miter=" + b.getMiterLimit() + " dashes="
						+ Arrays.toString(b.getDashArray()) + " phase=" + b.getDashPhase() + ")");
			} else {
				System.out.println("    setStroke(" + s + ")");
			}
			g.setStroke(s);
		}

		@Override
		public void drawLine(int x1, int y1, int x2, int y2) {
			System.out.println("    drawLine(" + x1 + ", " + y1 + ", " + x2 + ", " + y2 + ")");
			g.drawLine(x1, y1, x2, y2);
		}

		@Override
		public void fillRect(int x, int y, int w, int h) {
			System.out.println("    fillRect(" + x + ", " + y + ", " + w + ", " + h + ")");
			g.fillRect(x, y, w, h);
		}

		@Override
		public void drawRect(int x, int y, int w, int h) {
			System.out.println("    drawRect(" + x + ", " + y + ", " + w + ", " + h + ")");
			g.drawRect(x, y, w, h);
		}

		@Override
		public void dispose() {
			System.out.println("    dispose()");
			g.dispose();
		}

		/** Graphics.finalize() disposes: whenever the collector runs, which no transcript may show. */
		@Override
		@SuppressWarnings({ "deprecation", "removal" })
		public void finalize() {
			// Nothing: the wrapped Graphics finalizes itself.
		}
	}

	static String rgba(Color c) {
		return c == null ? "null" : c.getRed() + "," + c.getGreen() + "," + c.getBlue() + "," + c.getAlpha();
	}

	/** Paints @p icon at (3, 4) of a component @p c into a Recorder. */
	static void paint(String title, Icon icon, Component c) {
		BufferedImage image = new BufferedImage(80, 30, BufferedImage.TYPE_INT_ARGB);
		Graphics2D graphics = image.createGraphics();
		System.out.println("  paint " + title + " (" + icon.getIconWidth() + "x" + icon.getIconHeight() + ") at 3,4:");
		icon.paintIcon(c, new Recorder(graphics), 3, 4);
		graphics.dispose();
	}

	// ------------------------------------------------------------------ the data type combo box

	@SuppressWarnings("unchecked")
	static void groupsPopup(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> combo)
			throws Exception {
		JPopupMenu popup = (JPopupMenu) field(GroupableAndSearchableComboBox.class, combo, "groupsPopup");
		Method contains = GroupableAndSearchableComboBox.class.getDeclaredMethod("containsSelectedItem",
				info.openrocket.core.util.Group.class, info.openrocket.core.util.Groupable.class);
		contains.setAccessible(true);
		System.out.println("groups popup:");
		for (Component c : popup.getComponents()) {
			if (c instanceof JMenu menu) {
				FlightDataTypeGroup group = null;
				for (FlightDataTypeGroup g : FlightDataTypeGroup.ALL_GROUPS) {
					if (g.toString().equals(menu.getText())) {
						group = g;
					}
				}
				boolean check = group != null && (Boolean) contains.invoke(combo, group, combo.getSelectedItem());
				StringBuilder sb = new StringBuilder();
				for (Component item : menu.getMenuComponents()) {
					if (item instanceof JCheckBoxMenuItem cb) {
						if (sb.length() > 0)
							sb.append(" | ");
						sb.append(cb.isSelected() ? "[x] " : "").append(cb.getText());
					}
				}
				System.out.println("  menu '" + menu.getText() + "'" + (check ? " [checked]" : "") + ": " + sb);
			} else {
				System.out.println("  " + c.getClass().getSimpleName()
						+ (c instanceof javax.swing.JTextField tf ? " text='" + tf.getText() + "'" : ""));
			}
		}
	}

	@SuppressWarnings("unchecked")
	static void search(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> combo, String text)
			throws Exception {
		Method filter = GroupableAndSearchableComboBox.class.getDeclaredMethod("filter", String.class);
		filter.setAccessible(true);
		filter.invoke(combo, text);
		JList<FlightDataType> list = (JList<FlightDataType>) field(GroupableAndSearchableComboBox.class, combo,
				"filteredList");
		ListModel<FlightDataType> model = list.getModel();
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < model.getSize(); i++) {
			FlightDataType t = model.getElementAt(i);
			if (sb.length() > 0)
				sb.append(" | ");
			sb.append(t == combo.getSelectedItem() ? "✓ " : "").append(combo.getDisplayString(t));
		}
		System.out.println("search '" + text + "': " + model.getSize() + ": " + sb);
	}

	/** The combo box's items, in the order of its model. */
	static void model(JComboBox<FlightDataType> combo) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < combo.getItemCount(); i++) {
			sb.append(i == 0 ? "" : " | ").append(combo.getItemAt(i));
		}
		System.out.println("model: " + combo.getItemCount() + ": " + sb);
	}

	static String constraints(JPanel panel, Component component) {
		return String.valueOf(((MigLayout) panel.getLayout()).getComponentConstraints(component));
	}

	static List<String> events = new ArrayList<>();

	@SuppressWarnings("unchecked")
	static void dataTypeCombo() throws Exception {
		FlightDataType longType = FlightDataType.TYPE_DAMPING_MOMENT_COEFF_AERODYNAMIC;
		List<FlightDataType> types = Arrays.asList(FlightDataType.ALL_TYPES);
		System.out.println("== data type combo (PlotTypeSelector over FlightDataType.ALL_TYPES, "
				+ "Damping moment coefficient (aerodynamic) selected)");
		PlotTypeSelector<FlightDataType, FlightDataTypeGroup> row = new PlotTypeSelector<>(0, longType,
				longType.getUnitGroup().getDefaultUnit(), -1, types);
		GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> combo =
				(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType>) field(PlotTypeSelector.class, row,
						"typeSelector");
		System.out.println("placeholder='" + field(GroupableAndSearchableComboBox.class, combo, "placeHolderText") + "'");
		System.out.println("DATA_TYPE_SELECTOR_WIDTH='" + field(PlotTypeSelector.class, null, "DATA_TYPE_SELECTOR_WIDTH")
				+ "'");
		System.out.println("constraints typeSelector='" + constraints(row, combo) + "'");
		for (Component c : row.getComponents()) {
			if (c instanceof UnitSelector) {
				System.out.println("constraints unitSelector='" + constraints(row, c) + "'");
			}
		}
		System.out.println("tooltip='" + combo.getToolTipText() + "'");
		System.out.println("selected display='" + combo.getDisplayString((FlightDataType) combo.getSelectedItem())
				+ "' editable=" + combo.isEditable());
		model(combo);
		groupsPopup(combo);
		for (String text : new String[] { "alt", "ALT", "coef", "(", "moment", "zzz", " ", "Mach", "x",
				"damping moment" }) {
			search(combo, text);
		}

		combo.addItemListener(e -> events.add((e.getStateChange() == ItemEvent.SELECTED ? "SELECTED " : "DESELECTED ")
				+ e.getItem()));
		combo.addActionListener(e -> events.add("action"));
		combo.setSelectedItem(FlightDataType.TYPE_ALTITUDE);
		System.out.println("setSelectedItem(Altitude): tooltip='" + combo.getToolTipText() + "' events=" + events);
		events.clear();
		combo.setSelectedItem(FlightDataType.TYPE_ALTITUDE);
		System.out.println("setSelectedItem(Altitude) again: events=" + events);
		events.clear();
		groupsPopup(combo);
		search(combo, "alt");
		// A type that is not one of the items: ALL_TYPES leaves out TYPE_THRUST_CORRECTION.
		FlightDataType apart = FlightDataType.TYPE_THRUST_CORRECTION;
		combo.setSelectedItem(apart);
		System.out.println("setSelectedItem(Thrust Pressure Correction, not an item): selected='"
				+ combo.getSelectedItem() + "' index=" + combo.getSelectedIndex() + " tooltip='"
				+ combo.getToolTipText() + "' events=" + events);
		events.clear();
		groupsPopup(combo);
		combo.setSelectedItem(null);
		System.out.println("setSelectedItem(null): selected=" + combo.getSelectedItem() + " tooltip='"
				+ combo.getToolTipText() + "' events=" + events);
		events.clear();

		System.out.println("== data type combo (ties: Thrust Pressure Correction, Altitude, Thrust, Time; Time selected)");
		List<FlightDataType> ties = Arrays.asList(FlightDataType.TYPE_THRUST_CORRECTION, FlightDataType.TYPE_ALTITUDE,
				FlightDataType.TYPE_THRUST_FORCE, FlightDataType.TYPE_TIME);
		PlotTypeSelector<FlightDataType, FlightDataTypeGroup> tieRow = new PlotTypeSelector<>(0,
				FlightDataType.TYPE_TIME, null, 0, ties);
		GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> tieCombo =
				(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType>) field(PlotTypeSelector.class, tieRow,
						"typeSelector");
		groupsPopup(tieCombo);
		search(tieCombo, "t");
		search(tieCombo, "THRUST");
		// The same types the other way round: after the constructor's setSelectedItem(), which rebuilds the
		// groups from the model (updateItemsFromModel()), the types that compare equal are in the order of the
		// model, a HashSet's (extractItemsFromMap()), whatever the list's.
		System.out.println("== data type combo (ties reversed: Time, Thrust, Altitude, Thrust Pressure Correction; "
				+ "Time selected)");
		List<FlightDataType> reversed = Arrays.asList(FlightDataType.TYPE_TIME, FlightDataType.TYPE_THRUST_FORCE,
				FlightDataType.TYPE_ALTITUDE, FlightDataType.TYPE_THRUST_CORRECTION);
		PlotTypeSelector<FlightDataType, FlightDataTypeGroup> reversedRow = new PlotTypeSelector<>(0,
				FlightDataType.TYPE_TIME, null, 0, reversed);
		GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> reversedCombo =
				(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType>) field(PlotTypeSelector.class,
						reversedRow, "typeSelector");
		model(reversedCombo);
		groupsPopup(reversedCombo);
		GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> unselected =
				new GroupableAndSearchableComboBox<>(reversed, "placeholder");
		System.out.println("without a selection change (a new combo box: the list's order):");
		model(unselected);
		groupsPopup(unselected);

		// What a fresh flight's branch holds: every type of ALL_TYPES and TYPE_THRUST_CORRECTION.
		System.out.println("== data type combo (FlightDataType.ALL_TYPES and Thrust Pressure Correction; Altitude "
				+ "selected)");
		List<FlightDataType> fresh = new ArrayList<>(types);
		fresh.add(FlightDataType.TYPE_THRUST_CORRECTION);
		PlotTypeSelector<FlightDataType, FlightDataTypeGroup> freshRow = new PlotTypeSelector<>(0,
				FlightDataType.TYPE_ALTITUDE, null, 0, fresh);
		GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType> freshCombo =
				(GroupableAndSearchableComboBox<FlightDataTypeGroup, FlightDataType>) field(PlotTypeSelector.class,
						freshRow, "typeSelector");
		model(freshCombo);
		groupsPopup(freshCombo);

		System.out.println("== domain type selector (PlotPanel of PlotTypeSelectorTest)");
		SimulationPlotConfiguration custom = new SimulationPlotConfiguration("Custom", longType);
		SimulationPlotConfiguration defaults = new SimulationPlotConfiguration("Default", longType);
		FlightDataType[] available = { longType, FlightDataType.TYPE_ALTITUDE };
		PlotPanel<FlightDataType, FlightDataBranch, FlightDataTypeGroup, SimulationPlotConfiguration,
				PlotTypeSelector<FlightDataType, FlightDataTypeGroup>> panel = new PlotPanel<>(available, available,
						custom, new SimulationPlotConfiguration[] { defaults, custom }, defaults, null, null);
		JComboBox<FlightDataType> domain = (JComboBox<FlightDataType>) field(PlotPanel.class, panel,
				"domainTypeSelector");
		System.out.println("constraints domainTypeSelector='" + constraints(panel, domain) + "'");
		System.out.println("tooltip='" + domain.getToolTipText() + "'");
		domain.setSelectedItem(FlightDataType.TYPE_ALTITUDE);
		System.out.println("setSelectedItem(Altitude): tooltip='" + domain.getToolTipText() + "'");
	}

	// ------------------------------------------------------------------ the unit selector

	static String units(UnitGroup group) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < group.getUnitCount(); i++) {
			sb.append(i == 0 ? "" : " ").append(group.getUnit(i).getUnit());
		}
		return sb.toString();
	}

	static void unitSelector() {
		System.out.println("== unit selector (UnitSelector(UnitGroup.UNITS_LENGTH))");
		UnitSelector selector = new UnitSelector(UnitGroup.UNITS_LENGTH);
		selector.addItemListener(e -> events.add((e.getStateChange() == ItemEvent.SELECTED ? "SELECTED " : "DESELECTED ")
				+ ((Unit) e.getItem()).getUnit()));
		System.out.println("units: " + units(UnitGroup.UNITS_LENGTH) + "; default="
				+ UnitGroup.UNITS_LENGTH.getDefaultUnit().getUnit());
		System.out.println("text='" + selector.getText() + "' selected=" + selector.getSelectedUnit().getUnit()
				+ " model=" + selector.getModel() + " group is UNITS_LENGTH="
				+ (selector.getUnitGroup() == UnitGroup.UNITS_LENGTH));
		Unit inch = UnitGroup.UNITS_LENGTH.getUnit("in");
		selector.setSelectedUnit(inch);
		System.out.println("setSelectedUnit(in): text='" + selector.getText() + "' selected="
				+ selector.getSelectedUnit().getUnit() + " events=" + events + " default="
				+ UnitGroup.UNITS_LENGTH.getDefaultUnit().getUnit());
		events.clear();
		selector.setSelectedUnit(inch);
		System.out.println("setSelectedUnit(in) again: events=" + events);
		events.clear();
		selector.setUnitGroup(UnitGroup.UNITS_LENGTH);
		System.out.println("setUnitGroup(UNITS_LENGTH), the same: text='" + selector.getText() + "' selected="
				+ selector.getSelectedUnit().getUnit() + " events=" + events);
		events.clear();
		selector.setUnitGroup(UnitGroup.UNITS_DISTANCE);
		System.out.println("setUnitGroup(UNITS_DISTANCE): units: " + units(UnitGroup.UNITS_DISTANCE) + "; text='"
				+ selector.getText() + "' selected=" + selector.getSelectedUnit().getUnit() + " events=" + events);
		events.clear();
		try {
			selector.setSelectedUnit(inch);
			System.out.println("setSelectedUnit(in) of another group: accepted");
		} catch (IllegalArgumentException e) {
			System.out.println("setSelectedUnit(in) of another group: IllegalArgumentException: " + e.getMessage()
					.replaceAll("@[0-9a-f]+", "@<hash>") + " text='" + selector.getText() + "' events=" + events);
		}
		events.clear();
		UnitSelector none = new UnitSelector(UnitGroup.UNITS_NONE);
		System.out.println("UnitSelector(UNITS_NONE): units=" + UnitGroup.UNITS_NONE.getUnitCount() + " text='"
				+ none.getText() + "'");
		UnitSelector modelBacked = new UnitSelector(new info.openrocket.swing.gui.adaptors.DoubleModel(0.0,
				UnitGroup.UNITS_LENGTH));
		try {
			modelBacked.setUnitGroup(UnitGroup.UNITS_DISTANCE);
			System.out.println("setUnitGroup() of a model's selector: accepted");
		} catch (IllegalStateException e) {
			System.out.println("setUnitGroup() of a model's selector: IllegalStateException: " + e.getMessage());
		}
	}

	// ------------------------------------------------------------------ the line style combo box

	@SuppressWarnings("unchecked")
	static void lineStyles() throws Exception {
		System.out.println("== line style combo (SimulationPlotTypeSelector, initial line style null)");
		List<FlightDataType> types = Arrays.asList(FlightDataType.TYPE_TIME, FlightDataType.TYPE_ALTITUDE);
		SimulationPlotTypeSelector selector = new SimulationPlotTypeSelector(0, FlightDataType.TYPE_ALTITUDE, null, -1,
				types, new Color(1, 2, 3), null);
		JComboBox<LineStyle> combo = (JComboBox<LineStyle>) field(SimulationPlotTypeSelector.class, selector,
				"lineStyleSelector");
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < combo.getItemCount(); i++) {
			sb.append(i == 0 ? "" : " | ").append(combo.getItemAt(i).name()).append("='").append(combo.getItemAt(i))
					.append("'");
		}
		System.out.println("items: " + sb);
		System.out.println("selected=" + ((LineStyle) combo.getSelectedItem()).name() + " editable=" + combo.isEditable());
		for (LineStyle style : LineStyle.values()) {
			System.out.println("dashes " + style.name() + "=" + Arrays.toString(style.getDashes()));
		}
		ListCellRenderer<? super LineStyle> renderer = combo.getRenderer();
		JList<LineStyle> list = new JList<>();
		for (int i = 0; i < combo.getItemCount(); i++) {
			LineStyle style = combo.getItemAt(i);
			for (boolean selected : new boolean[] { false, true }) {
				JLabel label = (JLabel) renderer.getListCellRendererComponent(list, style, i, selected, false);
				System.out.println("renderer " + style.name() + (selected ? " selected" : "") + ": text='"
						+ label.getText() + "' gap=" + label.getIconTextGap() + " foreground="
						+ rgba(label.getForeground()) + " list foreground=" + rgba(list.getForeground())
						+ " selection foreground=" + rgba(list.getSelectionForeground()));
				if (!selected) {
					paint(style.name(), label.getIcon(), label);
				}
			}
		}
		combo.addActionListener(e -> events.add("action"));
		combo.addItemListener(e -> events.add((e.getStateChange() == ItemEvent.SELECTED ? "SELECTED " : "DESELECTED ")
				+ ((LineStyle) e.getItem()).name()));
		selector.setSelectedLineStyle(LineStyle.DASHED);
		System.out.println("setSelectedLineStyle(DASHED): selected=" + selector.getSelectedLineStyle().name()
				+ " events=" + events);
		events.clear();
		selector.setSelectedLineStyle(LineStyle.DASHED);
		System.out.println("setSelectedLineStyle(DASHED) again: events=" + events);
		events.clear();
		SimulationPlotTypeSelector dotted = new SimulationPlotTypeSelector(0, FlightDataType.TYPE_ALTITUDE, null, -1,
				types, new Color(1, 2, 3), LineStyle.DOTTED);
		System.out.println("initial DOTTED: selected=" + dotted.getSelectedLineStyle().name());
	}

	// ------------------------------------------------------------------ the colour button

	static void colorButton() {
		System.out.println("== color chooser button (ColorChooserButton(new Color(1, 2, 3)))");
		ColorChooserButton button = new ColorChooserButton(new Color(1, 2, 3));
		button.addColorPropertyChangeListener((PropertyChangeEvent e) -> events.add(e.getPropertyName() + " "
				+ rgba((Color) e.getOldValue()) + " -> " + rgba((Color) e.getNewValue())));
		Icon icon = button.getIcon();
		System.out.println("COLOR_KEY='" + ColorChooserButton.COLOR_KEY + "' selected=" + rgba(button.getSelectedColor())
				+ " icon=" + icon.getClass().getSimpleName() + " " + icon.getIconWidth() + "x" + icon.getIconHeight()
				+ " text='" + button.getText() + "' tooltip=" + button.getToolTipText());
		paint("enabled", icon, button);
		button.setEnabled(false);
		paint("disabled", icon, button);
		button.setEnabled(true);
		button.setSelectedColor(new Color(1, 2, 3));
		System.out.println("setSelectedColor(1,2,3), equal: events=" + events + " same icon=" + (button.getIcon() == icon));
		events.clear();
		button.setSelectedColor(null);
		System.out.println("setSelectedColor(null): selected=" + rgba(button.getSelectedColor()) + " events=" + events);
		events.clear();
		button.setSelectedColor(new Color(200, 100, 50, 128));
		System.out.println("setSelectedColor(200,100,50,128): selected=" + rgba(button.getSelectedColor()) + " events="
				+ events + " same icon=" + (button.getIcon() == icon));
		events.clear();
		ColorChooserButton empty = new ColorChooserButton(null);
		System.out.println("ColorChooserButton(null): selected=" + rgba(empty.getSelectedColor()) + " icon="
				+ empty.getIcon());
	}

	public static void main(String[] args) throws Exception {
		DeterministicUuids.install();
		DeterministicUuids.reseed("plot-widgets");
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(swingPrefs),
				new PluginModule()));
		UITheme.Themes.LIGHT.applyTheme();

		dataTypeCombo();
		unitSelector();
		lineStyles();
		colorButton();
	}
}
