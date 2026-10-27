package probe;

import java.awt.Component;
import java.awt.Dimension;
import java.lang.reflect.Constructor;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.TreeMap;
import java.util.prefs.Preferences;

import javax.swing.JLabel;
import javax.swing.JTable;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableCellRenderer;
import javax.swing.table.TableColumn;

import info.openrocket.swing.gui.dialogs.preset.XTableColumnModel;
import info.openrocket.swing.gui.util.TableUIPreferences;

/**
 * What OpenRocket's TableUIPreferences stores and restores (the column widths, order and
 * visibility of the parts library's table, ComponentPresetChooserDialog), on a JTable with the
 * parts library's XTableColumnModel, for QtRocket's TableUiPreferencesTests.cpp (values pinned by
 * hand: PresetGuiProbe prints this transcript first, and hand-pinned.txt keeps it).
 *
 * The table: eight columns A to H, each a TableColumn whose identifier is its header text, in an
 * XTableColumnModel (its constructor is package-private: made by reflection), over a
 * DefaultTableModel of three rows. Every width is made independent of fonts: the header renderer's
 * preferred width is 10 pixels per character of the header plus 5, a cell renderer's 10 per
 * character of the cell (QtRocket's test gives its model the same sizes as Qt::SizeHintRole). The
 * table is never laid out, so a column's width stays Swing's default of 75 until setWidth().
 * Printed after each step: the shown columns in view order, every column of the column model in
 * its order with the hidden ones in brackets, each shown column's preferred width, and after a
 * store the keys of the node, sorted.
 */
public class TableUiProbe {

	static final String TABLE = "CmpPrst.Probe";
	static final String[] HEADERS = {"A", "Bb", "Ccc", "D", "Eeee", "F", "G", "H"};
	static final String[][] ROWS = {
			{"x", "y", "zz", "", "wwwwwwww", "v", "u", "t"},
			{"xx", "", "z", "dddddddd", "w", "", "u", "t"},
			{"x", "yyyyy", "", "d", "w", "v", "", ""},
	};

	static Preferences node;

	/** A renderer whose preferred width is perCharacter pixels per character, plus extra. */
	static TableCellRenderer renderer(int perCharacter, int extra) {
		return (table, value, isSelected, hasFocus, row, column) -> {
			String text = value == null ? "" : value.toString();
			JLabel label = new JLabel(text);
			label.setPreferredSize(new Dimension(perCharacter * text.length() + extra, 16));
			return label;
		};
	}

	static final class Table {
		final JTable table;
		final XTableColumnModel model;
		final List<TableColumn> columns = new ArrayList<>();

		Table(String[][] rows) throws Exception {
			Constructor<XTableColumnModel> constructor = XTableColumnModel.class.getDeclaredConstructor();
			constructor.setAccessible(true);
			model = constructor.newInstance();
			DefaultTableModel data = new DefaultTableModel(rows, HEADERS);
			for (int i = 0; i < HEADERS.length; i++) {
				TableColumn column = new TableColumn(i);
				column.setHeaderValue(HEADERS[i]);
				model.addColumn(column);
				columns.add(column);
			}
			table = new JTable(data, model);
			table.setAutoCreateColumnsFromModel(false);
			table.setDefaultRenderer(Object.class, renderer(10, 0));
			table.getTableHeader().setDefaultRenderer(renderer(10, 5));
		}

		TableColumn column(String header) {
			return columns.get(java.util.Arrays.asList(HEADERS).indexOf(header));
		}

		void print(String title) {
			StringBuilder shown = new StringBuilder();
			StringBuilder widths = new StringBuilder();
			for (int i = 0; i < table.getColumnCount(); i++) {
				TableColumn c = table.getColumnModel().getColumn(i);
				shown.append(' ').append(c.getIdentifier());
				widths.append(' ').append(c.getIdentifier()).append('=').append(c.getPreferredWidth());
			}
			StringBuilder all = new StringBuilder();
			Enumeration<TableColumn> e = model.getAllColumns();
			while (e.hasMoreElements()) {
				TableColumn c = e.nextElement();
				all.append(' ').append(model.isColumnVisible(c) ? c.getIdentifier() : "[" + c.getIdentifier() + "]");
			}
			System.out.println(title);
			System.out.println("  shown:" + shown);
			System.out.println("  all:" + all);
			System.out.println("  preferred:" + widths);
		}
	}

	static void keys(String title) throws Exception {
		TreeMap<String, String> sorted = new TreeMap<>();
		for (String key : node.keys()) {
			sorted.put(key, node.get(key, null));
		}
		System.out.println(title + ": " + sorted.size() + " keys");
		for (var entry : sorted.entrySet()) {
			System.out.println("  " + entry.getKey() + " = " + entry.getValue());
		}
	}

	static void put(String kind, String column, String value) {
		node.put(TABLE + "." + kind + "." + column, value);
	}

	static void load(Table t, String title) {
		try {
			TableUIPreferences.loadTableUIPreferences(t.table, TABLE, node);
		} catch (RuntimeException e) {
			System.out.println(title + " threw " + e.getClass().getName() + ": " + e.getMessage());
		}
		t.print(title);
	}

	public static void main(String[] args) throws Exception {
		run();
	}

	/** Prints the transcript (PresetGuiProbe runs it first). */
	static void run() throws Exception {
		node = Preferences.userRoot().node("qtrocket-probe").node("tables");

		// 1. Nothing stored: every column shown in model order, the optimal widths (the widest of
		// the header and the cells, the header 10 per character + 5).
		node.clear();
		Table fresh = new Table(ROWS);
		load(fresh, "== fresh");
		// Store after the user's layout: two columns moved, one hidden, explicit widths.
		fresh.model.moveColumn(0, 3);
		fresh.model.moveColumn(5, 1);
		fresh.model.setColumnVisible(fresh.column("G"), false);
		int width = 30;
		for (int i = 0; i < fresh.table.getColumnCount(); i++) {
			fresh.table.getColumnModel().getColumn(i).setWidth(width);
			width += 7;
		}
		fresh.print("== fresh, moved, G hidden, widths set");
		TableUIPreferences.storeTableUIPreferences(fresh.table, TABLE, node);
		keys("== store");
		// Loaded into a new table: the stored layout comes back.
		Table again = new Table(ROWS);
		load(again, "== load what was stored");

		// 2. A full permutation of the stored view indices.
		String[][] permutations = {
				{"5", "0", "1", "2", "3", "4", "6", "7"},
				{"1", "2", "3", "4", "5", "6", "7", "0"},
				{"7", "6", "5", "4", "3", "2", "1", "0"},
				{"2", "0", "1", "5", "3", "4", "7", "6"},
				{"3", "7", "0", "6", "1", "5", "2", "4"},
		};
		for (String[] order : permutations) {
			node.clear();
			for (int i = 0; i < HEADERS.length; i++) {
				put("co", HEADERS[i], order[i]);
			}
			load(new Table(ROWS), "== order " + String.join(",", order));
		}

		// 3. Inconsistent indices: two columns at one index, one past the end, one absent, one not
		// an integer, one at its own index.
		node.clear();
		put("co", "A", "5");
		put("co", "Bb", "5");
		put("co", "Ccc", "9");
		put("co", "Eeee", "x");
		put("co", "F", "0");
		put("co", "G", "6");
		put("co", "H", "8");
		load(new Table(ROWS), "== order with duplicates and out of range");

		// 4. Hidden columns among moved ones, then shown again: where they come back.
		node.clear();
		put("cv", "Bb", "false");
		put("cv", "D", "FALSE");
		put("cv", "F", "yes");
		put("co", "A", "2");
		put("co", "Ccc", "0");
		put("co", "Eeee", "5");
		put("co", "G", "1");
		Table hidden = new Table(ROWS);
		load(hidden, "== hidden Bb and D, moved");
		hidden.model.setColumnVisible(hidden.column("Bb"), true);
		hidden.print("== Bb shown again");
		hidden.model.setColumnVisible(hidden.column("D"), true);
		hidden.print("== D shown again");
		hidden.model.setColumnVisible(hidden.column("A"), false);
		hidden.model.moveColumn(0, 4);
		hidden.print("== A hidden, then the first column moved to 4");
		hidden.model.setColumnVisible(hidden.column("A"), true);
		hidden.print("== A shown again");

		// 5. Widths: some stored, some missing (so all missing ones get the optimal width), one not
		// an integer, one below Swing's minimum of 15, one very wide; a cell wider than 2000 pixels.
		node.clear();
		put("cw", "A", "40");
		put("cw", "Bb", "abc");
		put("cw", "Ccc", "5");
		put("cw", "D", "99999");
		put("cw", "F", "-3");
		String[][] wide = {ROWS[0].clone(), ROWS[1], ROWS[2]};
		wide[0][6] = "u".repeat(250);
		Table widths = new Table(wide);
		TableColumn fixed = widths.column("H");
		fixed.setPreferredWidth(24);
		fixed.setMinWidth(24);
		fixed.setMaxWidth(24);
		put("cw", "H", "60");
		load(widths, "== widths, some missing");

		// 6. Every width stored: no optimal widths; one not an integer keeps the current width (75).
		node.clear();
		for (String header : HEADERS) {
			put("cw", header, String.valueOf(20 + header.length()));
		}
		put("cw", "Eeee", "wide");
		load(new Table(ROWS), "== widths, all stored");

		// 7. A hidden column's width is not needed: the optimal widths are computed only for a shown
		// column without one.
		node.clear();
		for (String header : HEADERS) {
			if (!header.equals("Ccc")) {
				put("cw", header, String.valueOf(30 + header.length()));
			}
		}
		put("cv", "Ccc", "false");
		load(new Table(ROWS), "== widths of the shown columns stored, Ccc hidden");

		// 8. A negative view index: Swing's moveColumn() throws.
		node.clear();
		put("co", "Ccc", "-1");
		load(new Table(ROWS), "== order -1");

		// 9. A removed column (the parts library removes the columns its type does not display):
		// not shown, not stored.
		node.clear();
		put("cv", "D", "true");
		put("co", "D", "0");
		Table removed = new Table(ROWS);
		removed.model.removeColumn(removed.column("D"));
		load(removed, "== D removed");
		TableUIPreferences.storeTableUIPreferences(removed.table, TABLE, node);
		keys("== store with D removed");

		node.removeNode();
	}
}
