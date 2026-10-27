package probe;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Enumeration;
import java.util.List;
import java.util.Set;

import javax.swing.JCheckBoxMenuItem;
import javax.swing.JMenu;
import javax.swing.JMenuItem;
import javax.swing.JPopupMenu;
import javax.swing.JRadioButtonMenuItem;
import javax.swing.JSeparator;
import javax.swing.RowFilter;
import javax.swing.RowSorter;
import javax.swing.SortOrder;
import javax.swing.table.TableColumn;
import javax.swing.table.TableModel;

import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.TypedKey;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.unit.Unit;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetRowFilter;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetTable;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetTableColumn;
import info.openrocket.swing.gui.dialogs.preset.XTableColumnModel;

/**
 * What OpenRocket's parts library table does, for QtRocket's tests/gui/dialogs/preset/ (values
 * pinned by hand: PresetGuiProbe prints this transcript last, and hand-pinned.txt keeps it): the
 * table classes of swing gui/dialogs/preset (ComponentPresetTable with its model, sorter and header
 * menu, XTableColumnModel, ComponentPresetTableColumn and ComponentPresetRowFilter) with the filters
 * ComponentPresetChooserDialog gives the table (its updateFilters(): RowFilter.regexFilter("(?iu)" +
 * text), the two diameter filters and the legacy filter, ANDed). The dialog itself is not made here.
 *
 * Two databases: the small one of QtRocket's tests (the preset files given as arguments, which
 * PresetModelProbe has loaded, in its order), whose cells, sorts, menus and filters are printed in
 * full, and the bundled one (the components directory given as an argument: every .orc file below
 * it, depth first, each directory in name order, as QtRocket's DirectoryIterator walks it, each
 * file's presets added with Database.addAll()), whose view orders are printed as the MD5 of their
 * rows' digests, one per line ("<digest>\n" for each view row, in view order).
 *
 * Where QtRocket fixes an OpenRocket bug the transcript prints both: what OpenRocket does, and what
 * the fixed behaviour gives, computed with OpenRocket's own classes ("fixed:").
 * - Bug 13 (plan 10.3): the header menu's Sort items sort by the clicked column's view index, taken
 *   as a model index. "fixed:" is the sort on the clicked column's model index.
 * - Bug 17: the text filter also matches the true/false text of the Boolean columns (Favorite,
 *   Legacy, Filled), so "e" matches every row. "fixed:" is RowFilter.regexFilter() over the other
 *   columns only (its indices).
 */
public class ComponentPresetTableProbe {

	/** The model columns of the Boolean values: the favourite and the Boolean keys. */
	static final List<Integer> BOOLEAN_COLUMNS = new ArrayList<>();
	/** The other model columns: the text filter's columns once bug 17 is fixed. */
	static final List<Integer> TEXT_COLUMNS = new ArrayList<>();

	static {
		BOOLEAN_COLUMNS.add(0);
		for (int i = 0; i < ComponentPreset.ORDERED_KEY_LIST.size(); i++) {
			if (ComponentPreset.ORDERED_KEY_LIST.get(i).getType() == Boolean.class) {
				BOOLEAN_COLUMNS.add(i + 1);
			} else {
				TEXT_COLUMNS.add(i + 1);
			}
		}
	}

	// ------------------------------------------------------------------------------- helpers

	static ComponentPresetTable table(ComponentPreset.Type type, List<ComponentPreset> presets) {
		return new ComponentPresetTable(type, presets, Arrays.asList(type.getDisplayedColumns()));
	}

	/** ComponentPresetRowFilter(double, column), whose constructor is package-private. */
	static RowFilter<TableModel, Object> diameterFilter(double value, int column) throws Exception {
		Constructor<ComponentPresetRowFilter> c = ComponentPresetRowFilter.class.getDeclaredConstructor(double.class,
				int.class);
		c.setAccessible(true);
		return c.newInstance(value, column);
	}

	/** ComponentPresetRowFilter(Boolean false, column): the dialog's legacy filter. */
	static RowFilter<TableModel, Object> legacyFilter(int column) throws Exception {
		Constructor<ComponentPresetRowFilter> c = ComponentPresetRowFilter.class.getDeclaredConstructor(Boolean.class,
				int.class);
		c.setAccessible(true);
		return c.newInstance(Boolean.FALSE, column);
	}

	static Object field(Object object, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	/** TableRowSorter's string value of a cell: toString(), "" for null. */
	static String text(TableModel model, int row, int column) {
		Object value = model.getValueAt(row, column);
		return value == null ? "" : value.toString();
	}

	static String header(ComponentPresetTable table, int modelColumn) {
		XTableColumnModel columns = table.getXColumnModel();
		TableColumn column = columns.getColumnByModelIndex(modelColumn);
		if (column != null) {
			return String.valueOf(column.getHeaderValue());
		}
		return "(removed " + modelColumn + ")";
	}

	/** The shown columns in view order, as "<header> [<model index>]". */
	static String viewColumns(ComponentPresetTable table) {
		StringBuilder line = new StringBuilder();
		for (int i = 0; i < table.getColumnCount(); i++) {
			TableColumn c = table.getColumnModel().getColumn(i);
			line.append(i == 0 ? "" : " | ").append(c.getHeaderValue()).append(" [").append(c.getModelIndex())
					.append(']');
		}
		return line.toString();
	}

	/** The view's rows as their model indices. */
	static String viewRows(ComponentPresetTable table) {
		StringBuilder line = new StringBuilder();
		for (int i = 0; i < table.getRowCount(); i++) {
			line.append(i == 0 ? "" : " ").append(table.convertRowIndexToModel(i));
		}
		return line.toString();
	}

	static String sortKeys(ComponentPresetTable table) {
		StringBuilder line = new StringBuilder();
		for (RowSorter.SortKey key : table.getRowSorter().getSortKeys()) {
			line.append(line.length() == 0 ? "" : ", ").append(key.getColumn()).append(' ')
					.append(key.getSortOrder() == SortOrder.ASCENDING ? "asc" : "desc");
		}
		return "[" + line + "]";
	}

	/** The rows and the MD5 of their digests in view order. */
	static String digestOfView(ComponentPresetTable table, List<ComponentPreset> presets) throws Exception {
		StringBuilder text = new StringBuilder();
		for (int i = 0; i < table.getRowCount(); i++) {
			text.append(presets.get(table.convertRowIndexToModel(i)).getDigest()).append('\n');
		}
		MessageDigest md5 = MessageDigest.getInstance("MD5");
		StringBuilder hex = new StringBuilder();
		for (byte b : md5.digest(text.toString().getBytes(StandardCharsets.UTF_8))) {
			hex.append(String.format("%02x", b));
		}
		return "rows " + table.getRowCount() + " md5 " + hex;
	}

	static void sortBy(ComponentPresetTable table, int column, SortOrder order) {
		table.getRowSorter().setSortKeys(Collections.singletonList(new RowSorter.SortKey(column, order)));
	}

	static int modelColumn(TypedKey<?> key) {
		return ComponentPreset.ORDERED_KEY_LIST.indexOf(key) + 1;
	}

	/** The text filter of the dialog over every column (OpenRocket) or the TEXT_COLUMNS (fixed). */
	static RowFilter<TableModel, Object> textFilter(String text, boolean fixed) {
		int[] indices = fixed ? TEXT_COLUMNS.stream().mapToInt(Integer::intValue).toArray() : new int[0];
		return RowFilter.regexFilter("(?iu)" + text, indices);
	}

	// ------------------------------------------------------------------------------- the small database

	static void columns() {
		System.out.println("== columns");
		ComponentPresetTable table = table(ComponentPreset.Type.BODY_TUBE, new ArrayList<>());
		TableModel model = table.getModel();
		StringBuilder line = new StringBuilder();
		for (int c = 0; c < model.getColumnCount(); c++) {
			line.append(c == 0 ? "" : " | ").append(c).append(' ').append(model.getColumnClass(c).getSimpleName());
		}
		System.out.println("  model " + model.getColumnCount() + " columns: " + line);
		StringBuilder boolText = new StringBuilder();
		for (int c : BOOLEAN_COLUMNS) {
			boolText.append(boolText.length() == 0 ? "" : " ").append(c);
		}
		System.out.println("  Boolean columns " + boolText);
		for (ComponentPreset.Type type : ComponentPreset.Type.values()) {
			ComponentPresetTable t = table(type, new ArrayList<>());
			StringBuilder all = new StringBuilder();
			Enumeration<TableColumn> e = t.getXColumnModel().getAllColumns();
			while (e.hasMoreElements()) {
				TableColumn c = e.nextElement();
				all.append(all.length() == 0 ? "" : " | ").append(c.getHeaderValue()).append(" [")
						.append(c.getModelIndex()).append(']');
			}
			System.out.println("  " + type + ": " + all);
			System.out.println("    sort keys " + sortKeys(t) + ", view = all: "
					+ (t.getColumnCount() == t.getXColumnModel().getColumnCount(false)));
		}
	}

	static void cells(List<ComponentPreset> all) {
		System.out.println("== cells (every model column, '' for null)");
		for (ComponentPreset.Type type : ComponentPreset.Type.values()) {
			List<ComponentPreset> presets = PresetModelProbe.presets.listForType(type);
			ComponentPresetTable t = table(type, presets);
			TableModel model = t.getModel();
			System.out.println("  " + type + ": " + presets.size() + " rows, view " + viewRows(t));
			for (int r = 0; r < presets.size(); r++) {
				StringBuilder line = new StringBuilder();
				for (int c = 0; c < model.getColumnCount(); c++) {
					String s = text(model, r, c);
					if (!s.isEmpty()) {
						line.append(line.length() == 0 ? "" : " | ").append(c).append('=').append(s);
					}
				}
				System.out.println("    " + r + ": " + line);
			}
		}
	}

	/**
	 * Each model column sorted ascending and descending (setSortKeys() of one key): the columns
	 * whose two orders differ from the model order, then those whose two orders are the model order.
	 */
	static void sortEachColumn(String title, ComponentPresetTable table) {
		String modelOrder = null;
		StringBuilder unsorted = new StringBuilder();
		for (int c = 0; c < table.getModel().getColumnCount(); c++) {
			sortBy(table, c, SortOrder.ASCENDING);
			String asc = viewRows(table);
			sortBy(table, c, SortOrder.DESCENDING);
			String desc = viewRows(table);
			if (modelOrder == null) {
				table.getRowSorter().setSortKeys(null);
				modelOrder = viewRows(table);
			}
			if (asc.equals(modelOrder) && desc.equals(modelOrder)) {
				unsorted.append(' ').append(c);
			} else {
				System.out.println("  " + title + " column " + c + " asc: " + asc + "; desc: " + desc);
			}
		}
		System.out.println("  " + title + " model order both ways:" + unsorted);
	}

	static void sorting() throws Exception {
		System.out.println("== sorting (the view's model rows)");
		List<ComponentPreset> presets = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable t = table(ComponentPreset.Type.BODY_TUBE, presets);
		System.out.println("  BODY_TUBE at open " + sortKeys(t) + ": " + viewRows(t));
		sortEachColumn("BODY_TUBE", t);
		t.getRowSorter().setSortKeys(null);
		System.out.println("  BODY_TUBE no keys: " + viewRows(t));
		// Header clicks (toggleSortOrder): Length, Length, Inner Diameter, Outer Diameter, Length,
		// Description.
		int[] clicks = {9, 9, 8, 5, 9, 4};
		for (int c : clicks) {
			t.getRowSorter().toggleSortOrder(c);
			System.out.println("  toggle " + c + ": " + sortKeys(t) + ": " + viewRows(t));
		}
		t.getRowSorter().setSortKeys(Arrays.asList(new RowSorter.SortKey(5, SortOrder.DESCENDING),
				new RowSorter.SortKey(8, SortOrder.ASCENDING), new RowSorter.SortKey(9, SortOrder.DESCENDING)));
		System.out.println("  keys " + sortKeys(t) + ": " + viewRows(t));
		for (ComponentPreset.Type type : new ComponentPreset.Type[] { ComponentPreset.Type.NOSE_CONE,
				ComponentPreset.Type.CENTERING_RING, ComponentPreset.Type.STREAMER, ComponentPreset.Type.PARACHUTE }) {
			sortEachColumn(type.toString(), table(type, PresetModelProbe.presets.listForType(type)));
		}
	}

	static String menuItems(JPopupMenu menu) {
		StringBuilder line = new StringBuilder();
		for (java.awt.Component item : menu.getComponents()) {
			if (line.length() > 0) {
				line.append(' ');
			}
			if (item instanceof JSeparator) {
				line.append('|');
			} else if (item instanceof JMenu sub) {
				StringBuilder units = new StringBuilder();
				for (java.awt.Component u : sub.getMenuComponents()) {
					JRadioButtonMenuItem radio = (JRadioButtonMenuItem) u;
					units.append(' ').append(radio.isSelected() ? "*" : "").append(radio.getText());
				}
				line.append('[').append(sub.getText()).append(':').append(units).append(']');
			} else if (item instanceof JCheckBoxMenuItem check) {
				line.append(check.isSelected() ? "[x]" : "[ ]").append(check.getText());
			} else {
				line.append(((JMenuItem) item).getText());
			}
		}
		return line.toString();
	}

	static JPopupMenu popup(ComponentPresetTable table, int viewColumn) throws Exception {
		Class<?> menuClass = null;
		for (Class<?> k : ComponentPresetTable.class.getDeclaredClasses()) {
			if (k.getSimpleName().equals("ColumnPopupMenu")) {
				menuClass = k;
			}
		}
		Constructor<?> ctor = menuClass.getDeclaredConstructors()[0];
		ctor.setAccessible(true);
		ComponentPresetTableColumn clicked = viewColumn >= 0
				? (ComponentPresetTableColumn) table.getColumnModel().getColumn(viewColumn) : null;
		return (JPopupMenu) ctor.newInstance(table, clicked, viewColumn);
	}

	static JMenuItem item(JPopupMenu menu, String text) {
		for (java.awt.Component c : menu.getComponents()) {
			if (c instanceof JMenuItem m && m.getText().equals(text)) {
				return m;
			}
		}
		throw new IllegalArgumentException(text);
	}

	static void headerMenu() throws Exception {
		System.out.println("== header menu (body tubes, Legacy hidden as the dialog hides it)");
		List<ComponentPreset> presets = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable t = table(ComponentPreset.Type.BODY_TUBE, presets);
		XTableColumnModel columns = t.getXColumnModel();
		columns.setColumnVisible(columns.getColumnByModelIndex(modelColumn(ComponentPreset.LEGACY)), false);
		System.out.println("  view " + viewColumns(t));
		System.out.println("  outside the columns: " + menuItems(popup(t, -1)));
		for (int v = 0; v < t.getColumnCount(); v++) {
			int model = t.getColumnModel().getColumn(v).getModelIndex();
			JPopupMenu menu = popup(t, v);
			System.out.println("  menu of view " + v + " (model " + model + "): " + menuItems(menu));
			item(menu, "Sort Ascending").doClick();
			String asc = sortKeys(t) + " " + viewRows(t);
			item(popup(t, v), "Sort Descending").doClick();
			String desc = sortKeys(t) + " " + viewRows(t);
			System.out.println("    Sort Ascending: " + asc + "; Sort Descending: " + desc);
			sortBy(t, model, SortOrder.ASCENDING);
			asc = sortKeys(t) + " " + viewRows(t);
			sortBy(t, model, SortOrder.DESCENDING);
			System.out.println("    fixed: " + asc + "; " + sortKeys(t) + " " + viewRows(t));
		}
		ComponentPresetTable rail = table(ComponentPreset.Type.RAIL_BUTTON,
				PresetModelProbe.presets.listForType(ComponentPreset.Type.RAIL_BUTTON));
		int mass = rail.convertColumnIndexToView(modelColumn(ComponentPreset.MASS));
		System.out.println("  rail button, menu of Mass (view " + mass + "): " + menuItems(popup(rail, mass)));
		ComponentPresetTable chute = table(ComponentPreset.Type.PARACHUTE,
				PresetModelProbe.presets.listForType(ComponentPreset.Type.PARACHUTE));
		int sides = chute.convertColumnIndexToView(modelColumn(ComponentPreset.SIDES));
		System.out.println("  parachute, menu of Sides (view " + sides + "): " + menuItems(popup(chute, sides)));

		System.out.println("== header menu: units (body tubes, Outer Diameter)");
		ComponentPresetTable u = table(ComponentPreset.Type.BODY_TUBE, presets);
		int outer = modelColumn(ComponentPreset.OUTER_DIAMETER);
		int view = u.convertColumnIndexToView(outer);
		u.setRowSelectionInterval(3, 3);
		System.out.println("  selected row " + u.getSelectedRow() + " (model " + u.convertRowIndexToModel(3) + ")");
		JPopupMenu menu = popup(u, view);
		JMenu units = null;
		for (java.awt.Component c : menu.getComponents()) {
			if (c instanceof JMenu m) {
				units = m;
			}
		}
		for (java.awt.Component c : units.getMenuComponents()) {
			JRadioButtonMenuItem radio = (JRadioButtonMenuItem) c;
			if (radio.getText().equals("mm")) {
				radio.doClick();
			}
		}
		TableModel model = u.getModel();
		StringBuilder line = new StringBuilder();
		for (int r = 0; r < model.getRowCount(); r++) {
			line.append(r == 0 ? "" : " | ").append(text(model, r, outer));
		}
		System.out.println("  mm chosen: " + line);
		System.out.println("  selected row after: " + u.getSelectedRow());
		System.out.println("  menu again: " + menuItems(popup(u, view)));
		ComponentPresetTable fresh = table(ComponentPreset.Type.BODY_TUBE, presets);
		System.out.println("  a new table: " + text(fresh.getModel(), 0, outer));
		// The selected unit of every unit column of a table: the group's default.
		ComponentPresetTableColumn.DoubleWithUnit length = (ComponentPresetTableColumn.DoubleWithUnit) u
				.getXColumnModel().getColumnByModelIndex(modelColumn(ComponentPreset.LENGTH));
		System.out.println("  Length's unit " + ((Unit) field(length, ComponentPresetTableColumn.DoubleWithUnit.class,
				"selectedUnit")).getUnit());

		System.out.println("== header menu: columns (body tubes)");
		ComponentPresetTable h = table(ComponentPreset.Type.BODY_TUBE, presets);
		XTableColumnModel x = h.getXColumnModel();
		item(popup(h, 1), "Manufacturer").doClick();
		System.out.println("  Manufacturer unticked: " + viewColumns(h));
		h.moveColumn(h.convertColumnIndexToView(modelColumn(ComponentPreset.LENGTH)), 0);
		System.out.println("  Length moved to 0: " + viewColumns(h));
		item(popup(h, 0), "Manufacturer").doClick();
		System.out.println("  Manufacturer ticked: " + viewColumns(h));
		x.setColumnVisible(x.getColumnByModelIndex(modelColumn(ComponentPreset.LEGACY)), false);
		System.out.println("  Legacy hidden: " + viewColumns(h));
		System.out.println("  menu: " + menuItems(popup(h, 2)));
	}

	static void favourites() throws Exception {
		System.out.println("== favourites (body tubes)");
		List<ComponentPreset> presets = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable t = table(ComponentPreset.Type.BODY_TUBE, presets);
		TableModel model = t.getModel();
		System.out.println("  editable: " + model.isCellEditable(0, 0) + " " + model.isCellEditable(0, 1));
		int view = 5;
		int row = t.convertRowIndexToModel(view);
		t.setRowSelectionInterval(view, view);
		model.setValueAt(Boolean.TRUE, row, 0);
		StringBuilder ticked = new StringBuilder();
		for (int r = 0; r < model.getRowCount(); r++) {
			if ((Boolean) model.getValueAt(r, 0)) {
				ticked.append(' ').append(r).append(' ').append(presets.get(r).preferenceKey());
			}
		}
		System.out.println("  model row " + row + " ticked: favourites" + ticked + "; selected view row "
				+ t.getSelectedRow());
		sortBy(t, 0, SortOrder.DESCENDING);
		System.out.println("  favourite desc: " + viewRows(t) + "; selected view row " + t.getSelectedRow() + " (model "
				+ t.convertRowIndexToModel(t.getSelectedRow()) + ")");
		model.setValueAt(Boolean.FALSE, row, 0);
		System.out.println("  unticked: favourites "
				+ info.openrocket.core.startup.Application.getPreferences().getComponentFavorites(
						ComponentPreset.Type.BODY_TUBE)
				+ "; view " + viewRows(t) + "; selected view row " + t.getSelectedRow());
	}

	static String selected(ComponentPresetTable table) {
		int row = table.getSelectedRow();
		return row < 0 ? "-1" : row + " (model " + table.convertRowIndexToModel(row) + ")";
	}

	/**
	 * JTable keeps its selection as model rows through the sorter's changes: a row the filter
	 * hides is no longer selected, and selected again when the filter shows it, unless another row
	 * was selected meanwhile.
	 */
	static void selectionThroughFilters() throws Exception {
		System.out.println("== the selection through the filters (body tubes, the view's selected row)");
		List<ComponentPreset> presets = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable t = table(ComponentPreset.Type.BODY_TUBE, presets);
		t.setRowSelectionInterval(2, 2);
		StringBuilder line = new StringBuilder();
		for (String text : new String[] { "B", "BT", "BT-20", "BT", "B", "" }) {
			t.setRowFilter(textFilter(text, false));
			line.append(" '").append(text).append("' ").append(selected(t)).append(';');
		}
		System.out.println("  view row 2 selected, then the texts:" + line);
		t.setRowFilter(textFilter("BT-20", false));
		t.clearSelection();
		t.setRowFilter(null);
		System.out.println("  hidden, the selection cleared, shown: " + selected(t));
		t.setRowFilter(textFilter("BT-20", false));
		t.getRowSorter().toggleSortOrder(modelColumn(ComponentPreset.LENGTH));
		t.setRowFilter(null);
		System.out.println("  hidden, sorted by Length, shown: " + selected(t));
		t.setRowFilter(textFilter("BT-20", false));
		t.setRowSelectionInterval(0, 0);
		t.setRowFilter(null);
		System.out.println("  hidden, the shown row selected, the filter gone: " + selected(t));
		t.clearSelection();
		t.setRowFilter(textFilter("BT-20", false));
		t.setRowFilter(null);
		System.out.println("  that row unselected, hidden, shown: " + selected(t));
	}

	static void filters(List<ComponentPreset> unused) throws Exception {
		System.out.println("== text filter (rows: OpenRocket, fixed)");
		String[] patterns = { "", "estes", "ESTES", "bt-20", "e", "true", "false", "Kraft", "cm", "2.5", "^B", "[",
				"(", "0.5 mil", "Ogive", "\\d{5}" };
		for (ComponentPreset.Type type : new ComponentPreset.Type[] { ComponentPreset.Type.BODY_TUBE,
				ComponentPreset.Type.NOSE_CONE, ComponentPreset.Type.STREAMER, ComponentPreset.Type.PARACHUTE }) {
			List<ComponentPreset> presets = PresetModelProbe.presets.listForType(type);
			ComponentPresetTable t = table(type, presets);
			StringBuilder line = new StringBuilder();
			for (String p : patterns) {
				String or;
				String fixed;
				try {
					t.setRowFilter(textFilter(p, false));
					or = viewRows(t);
					t.setRowFilter(textFilter(p, true));
					fixed = viewRows(t);
				} catch (java.util.regex.PatternSyntaxException e) {
					or = "invalid";
					fixed = "invalid";
				}
				line.append("\n    '").append(p).append("': ").append(or).append(" / fixed: ").append(fixed);
			}
			System.out.println("  " + type + line);
		}

		System.out.println("== diameter filters (the view's model rows)");
		List<ComponentPreset> tubes = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable t = table(ComponentPreset.Type.BODY_TUBE, tubes);
		int outer = modelColumn(ComponentPreset.OUTER_DIAMETER);
		StringBuilder ods = new StringBuilder();
		for (int r = 0; r < tubes.size(); r++) {
			ods.append(r == 0 ? "" : " ").append(tubes.get(r).get(ComponentPreset.OUTER_DIAMETER));
		}
		System.out.println("  outer diameters " + ods);
		double[] values = { 0.0248, 0.0187, 0.0337, 0.0414, 0.0, 0.02, 0.019, 0.0182, Double.NaN };
		for (double v : values) {
			t.setRowFilter(diameterFilter(v, outer));
			System.out.println("  " + v + ": " + viewRows(t));
		}
		List<ComponentPreset> transitions = PresetModelProbe.presets.listForType(ComponentPreset.Type.TRANSITION);
		ComponentPresetTable tr = table(ComponentPreset.Type.TRANSITION, transitions);
		StringBuilder diameters = new StringBuilder();
		for (ComponentPreset p : transitions) {
			diameters.append(' ').append(p.get(ComponentPreset.FORE_OUTER_DIAMETER)).append('>')
					.append(p.get(ComponentPreset.AFT_OUTER_DIAMETER));
		}
		System.out.println("  transitions" + diameters);
		tr.setRowFilter(diameterFilter(transitions.get(0).get(ComponentPreset.FORE_OUTER_DIAMETER),
				modelColumn(ComponentPreset.FORE_OUTER_DIAMETER)));
		System.out.println("  fore of the first: " + viewRows(tr));
		tr.setRowFilter(diameterFilter(transitions.get(1).get(ComponentPreset.AFT_OUTER_DIAMETER),
				modelColumn(ComponentPreset.AFT_OUTER_DIAMETER)));
		System.out.println("  aft of the second: " + viewRows(tr));

		System.out.println("== legacy filter (the view's model rows)");
		for (ComponentPreset.Type type : new ComponentPreset.Type[] { ComponentPreset.Type.BULK_HEAD,
				ComponentPreset.Type.STREAMER, ComponentPreset.Type.BODY_TUBE }) {
			List<ComponentPreset> presets = PresetModelProbe.presets.listForType(type);
			ComponentPresetTable table = table(type, presets);
			String all = viewRows(table);
			table.setRowFilter(legacyFilter(modelColumn(ComponentPreset.LEGACY)));
			System.out.println("  " + type + ": all " + all + "; legacy hidden " + viewRows(table));
		}
	}

	// ------------------------------------------------------------------------------- the bundled database

	static void collect(File directory, List<File> files) {
		File[] children = directory.listFiles();
		Arrays.sort(children, (a, b) -> a.getName().compareTo(b.getName()));
		for (File child : children) {
			if (child.getName().startsWith(".")) {
				continue;
			}
			if (child.isDirectory()) {
				collect(child, files);
			} else if (child.getName().toLowerCase(java.util.Locale.ROOT).endsWith(".orc")) {
				files.add(child);
			}
		}
	}

	static ComponentPresetDatabase bundled(String directory) throws Exception {
		List<File> files = new ArrayList<>();
		collect(new File(directory), files);
		ComponentPresetDatabase database = new ComponentPresetDatabase();
		int read = 0;
		for (File file : files) {
			try (InputStream in = new FileInputStream(file)) {
				java.util.Collection<ComponentPreset> presets = new OpenRocketComponentLoader().load(in, file.getName());
				read += presets.size();
				database.addAll(presets);
			}
		}
		System.out.println("== the bundled database: " + files.size() + " files, " + read + " presets read, "
				+ database.size() + " kept");
		return database;
	}

	static void bundledViews(ComponentPresetDatabase database) throws Exception {
		System.out.println("== bundled views (the dialog's filters; md5 of the digests in view order)");
		for (ComponentPreset.Type type : ComponentPreset.Type.values()) {
			List<ComponentPreset> presets = database.listForType(type);
			ComponentPresetTable t = table(type, presets);
			String all = digestOfView(t, presets);
			t.setRowFilter(legacyFilter(modelColumn(ComponentPreset.LEGACY)));
			System.out.println("  " + type + ": all " + all + "; legacy hidden " + digestOfView(t, presets));
		}

		System.out.println("== bundled favourites (body tubes: the first preference key of several presets)");
		List<ComponentPreset> tubes = database.listForType(ComponentPreset.Type.BODY_TUBE);
		ComponentPresetTable favourites = table(ComponentPreset.Type.BODY_TUBE, tubes);
		for (int r = 0; r < tubes.size(); r++) {
			List<Integer> same = new ArrayList<>();
			for (int o = 0; o < tubes.size(); o++) {
				if (tubes.get(o).preferenceKey().equals(tubes.get(r).preferenceKey())) {
					same.add(o);
				}
			}
			if (same.size() > 1) {
				favourites.getModel().setValueAt(Boolean.TRUE, r, 0);
				List<Integer> ticked = new ArrayList<>();
				for (int o = 0; o < tubes.size(); o++) {
					if ((Boolean) favourites.getModel().getValueAt(o, 0)) {
						ticked.add(o);
					}
				}
				System.out.println("  '" + tubes.get(r).preferenceKey() + "' of model rows " + same + ": row " + r
						+ " ticked, ticked rows " + ticked);
				favourites.getModel().setValueAt(Boolean.FALSE, same.get(1), 0);
				System.out.println("  row " + same.get(1) + " unticked: favourites "
						+ info.openrocket.core.startup.Application.getPreferences().getComponentFavorites(
								ComponentPreset.Type.BODY_TUBE));
				break;
			}
		}

		System.out.println("== bundled sorts (body tubes, nose cones, parachutes; legacy shown, no filter)");
		for (ComponentPreset.Type type : new ComponentPreset.Type[] { ComponentPreset.Type.BODY_TUBE,
				ComponentPreset.Type.NOSE_CONE, ComponentPreset.Type.PARACHUTE }) {
			List<ComponentPreset> presets = database.listForType(type);
			ComponentPresetTable t = table(type, presets);
			for (TypedKey<?> key : type.getDisplayedColumns()) {
				int c = modelColumn(key);
				if (c == 0) {
					continue;
				}
				sortBy(t, c, SortOrder.ASCENDING);
				String asc = digestOfView(t, presets);
				sortBy(t, c, SortOrder.DESCENDING);
				System.out.println("  " + type + " " + key.getName() + " (" + c + ") asc " + asc + "; desc "
						+ digestOfView(t, presets));
			}
			// Material and Finish, hidden columns of the nose cones: the tertiary collator.
			if (type == ComponentPreset.Type.NOSE_CONE) {
				for (TypedKey<?> key : new TypedKey<?>[] { ComponentPreset.MATERIAL, ComponentPreset.FINISH }) {
					int c = modelColumn(key);
					sortBy(t, c, SortOrder.ASCENDING);
					System.out.println("  " + type + " " + key.getName() + " (" + c + ") asc "
							+ digestOfView(t, presets));
				}
			}
			t.getRowSorter().setSortKeys(Arrays.asList(new RowSorter.SortKey(modelColumn(ComponentPreset.DESCRIPTION),
					SortOrder.DESCENDING), new RowSorter.SortKey(modelColumn(ComponentPreset.LENGTH), SortOrder.ASCENDING),
					new RowSorter.SortKey(modelColumn(ComponentPreset.MANUFACTURER), SortOrder.ASCENDING)));
			System.out.println("  " + type + " " + sortKeys(t) + " " + digestOfView(t, presets));
		}

		System.out.println("== bundled text filters (legacy shown; rows: OpenRocket, fixed)");
		String[] patterns = { "estes", "BT-20", "e", "true", "Cardboard", "cm", "ogive", "[" };
		for (ComponentPreset.Type type : new ComponentPreset.Type[] { ComponentPreset.Type.BODY_TUBE,
				ComponentPreset.Type.NOSE_CONE, ComponentPreset.Type.CENTERING_RING,
				ComponentPreset.Type.PARACHUTE }) {
			List<ComponentPreset> presets = database.listForType(type);
			ComponentPresetTable t = table(type, presets);
			StringBuilder line = new StringBuilder();
			for (String p : patterns) {
				String or;
				String fixed;
				try {
					t.setRowFilter(textFilter(p, false));
					or = String.valueOf(t.getRowCount());
					t.setRowFilter(textFilter(p, true));
					fixed = String.valueOf(t.getRowCount());
				} catch (java.util.regex.PatternSyntaxException e) {
					or = "invalid";
					fixed = "invalid";
				}
				line.append(" '").append(p).append("' ").append(or).append('/').append(fixed);
			}
			System.out.println("  " + type + ":" + line);
		}

		System.out.println("== bundled diameter filters (the scouting's rocket; legacy hidden)");
		// {type, fore diameter or NaN, aft diameter or NaN}
		Object[][] scenarios = { { ComponentPreset.Type.NOSE_CONE, Double.NaN, 0.0248 },
				{ ComponentPreset.Type.BODY_TUBE, 0.05, 0.0248 }, { ComponentPreset.Type.BODY_TUBE, Double.NaN, 0.0248 },
				{ ComponentPreset.Type.TRANSITION, 0.0248, 0.0416 }, { ComponentPreset.Type.TRANSITION, Double.NaN, 0.0416 },
				{ ComponentPreset.Type.BODY_TUBE, 0.0416, Double.NaN }, { ComponentPreset.Type.NOSE_CONE, Double.NaN, 0.0416 } };
		for (Object[] s : scenarios) {
			ComponentPreset.Type type = (ComponentPreset.Type) s[0];
			double fore = (Double) s[1];
			double aft = (Double) s[2];
			List<ComponentPreset> presets = database.listForType(type);
			ComponentPresetTable t = table(type, presets);
			List<RowFilter<TableModel, Object>> filters = new ArrayList<>();
			filters.add(textFilter("", false));
			int foreColumn = -1;
			int aftColumn = -1;
			int i = 0;
			List<TypedKey<?>> displayed = Arrays.asList(type.getDisplayedColumns());
			for (TypedKey<?> key : ComponentPreset.ORDERED_KEY_LIST) {
				i++;
				if (!displayed.contains(key)) {
					continue;
				}
				if (key == ComponentPreset.OUTER_DIAMETER || key == ComponentPreset.AFT_OUTER_DIAMETER) {
					aftColumn = i;
				}
				if (key == ComponentPreset.OUTER_DIAMETER || key == ComponentPreset.FORE_OUTER_DIAMETER) {
					foreColumn = i;
				}
			}
			if (!Double.isNaN(aft)) {
				filters.add(diameterFilter(aft, aftColumn));
			}
			if (!Double.isNaN(fore)) {
				filters.add(diameterFilter(fore, foreColumn));
			}
			filters.add(legacyFilter(modelColumn(ComponentPreset.LEGACY)));
			t.setRowFilter(RowFilter.andFilter(filters));
			System.out.println("  " + type + " fore " + fore + " (column " + foreColumn + ") aft " + aft + " (column "
					+ aftColumn + "): " + digestOfView(t, presets));
		}
	}

	/** Prints the transcript (PresetGuiProbe runs it last, once PresetModelProbe has loaded the files). */
	static void run(String components) throws Exception {
		System.out.println("== the parts library's table");
		// No favourites: the probes before this one ticked some.
		for (ComponentPreset p : PresetModelProbe.presets) {
			info.openrocket.core.startup.Application.getPreferences().setComponentFavorite(p, p.getType(), false);
		}
		columns();
		cells(null);
		sorting();
		headerMenu();
		favourites();
		selectionThroughFilters();
		filters(null);
		ComponentPresetDatabase database = bundled(components);
		Set<String> none = info.openrocket.core.startup.Application.getPreferences()
				.getComponentFavorites(ComponentPreset.Type.BODY_TUBE);
		System.out.println("  body tube favourites now: " + none);
		bundledViews(database);
	}
}
