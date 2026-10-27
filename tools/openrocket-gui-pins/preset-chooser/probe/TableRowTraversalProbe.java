package probe;

import java.awt.AWTKeyStroke;
import java.awt.KeyboardFocusManager;
import java.awt.event.ActionEvent;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.util.Set;

import javax.swing.Action;
import javax.swing.InputMap;
import javax.swing.JComponent;
import javax.swing.JScrollPane;
import javax.swing.JTable;
import javax.swing.KeyStroke;
import javax.swing.ListSelectionModel;
import javax.swing.RowFilter;
import javax.swing.table.DefaultTableModel;
import javax.swing.table.TableModel;
import javax.swing.table.TableRowSorter;

import info.openrocket.swing.utils.TableRowTraversalPolicy;

/**
 * What OpenRocket's TableRowTraversalPolicy (the parts library's and the simulations table's
 * Tab, Shift+Tab, Right and Left) does beyond its JUnit test (TableRowTraversalPolicyTest), for
 * QtRocket's TableRowTraversalTests.cpp (values pinned by hand: PresetGuiProbe prints this
 * transcript last, and hand-pinned.txt keeps it): its four actions, run through the action map as
 * the JUnit test runs them, on JTables without a selection, with several rows selected, without
 * rows, of one row and of row selection (the parts library's), whether the table scrolls to the
 * row an action selects (and to the row JTable's own Down selects), an empty filtered table, and the key bindings and focus traversal keys of
 * a table with the policy.
 *
 * Printed after each action: JTable.getSelectedRow(), the row selection model's interval, lead
 * and anchor, getSelectedColumn() and the column selection model's interval and lead, and
 * getSelectedRowCount(). Headless: the scrolling table is laid out by hand in a JScrollPane of
 * 300 x 200 pixels and never shown.
 */
public class TableRowTraversalProbe {

	static final String[] ACTIONS = {"Action.NextRow", "Action.NextRowCycle", "Action.PreviousRow",
			"Action.PreviousRowCycle"};

	/** The JUnit test's table (createTableWithPolicy()): cell selection, rows x 2. */
	static JTable cellTable(int rows) {
		JTable table = new JTable(rows, 2);
		table.setCellSelectionEnabled(true);
		table.setColumnSelectionAllowed(true);
		table.setRowSelectionAllowed(true);
		TableRowTraversalPolicy.setTableRowTraversalPolicy(table);
		return table;
	}

	/** A table that selects single rows, as the parts library's. */
	static JTable rowTable(int rows, int columns) {
		JTable table = new JTable(new DefaultTableModel(rows, columns));
		table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
		TableRowTraversalPolicy.setTableRowTraversalPolicy(table);
		return table;
	}

	/** The JUnit test's invokeAction(). */
	static void act(JTable table, String key) {
		Action action = table.getActionMap().get(key);
		action.actionPerformed(new ActionEvent(table, ActionEvent.ACTION_PERFORMED, key));
	}

	static String state(JTable table) {
		ListSelectionModel rows = table.getSelectionModel();
		ListSelectionModel columns = table.getColumnModel().getSelectionModel();
		return "row " + table.getSelectedRow() + " rows [" + rows.getMinSelectionIndex() + "," +
				rows.getMaxSelectionIndex() + "] lead " + rows.getLeadSelectionIndex() + " anchor " +
				rows.getAnchorSelectionIndex() + "; col " + table.getSelectedColumn() + " cols [" +
				columns.getMinSelectionIndex() + "," + columns.getMaxSelectionIndex() + "] lead " +
				columns.getLeadSelectionIndex() + " count " + table.getSelectedRowCount();
	}

	public static void main(String[] args) {
		run();
	}

	/** Prints the transcript (PresetGuiProbe runs it last). */
	static void run() {
		System.out.println("== no selection (cell table, 3 rows)");
		for (String key : ACTIONS) {
			JTable table = cellTable(3);
			act(table, key);
			System.out.println("  " + key + ": " + state(table));
		}
		System.out.println("== rows 1..2 selected (cell table, 4 rows), column 1");
		for (String key : ACTIONS) {
			JTable table = cellTable(4);
			table.setRowSelectionInterval(1, 2);
			table.setColumnSelectionInterval(1, 1);
			act(table, key);
			System.out.println("  " + key + ": " + state(table));
		}
		System.out.println("== empty table");
		for (String key : ACTIONS) {
			JTable table = cellTable(0);
			act(table, key);
			System.out.println("  " + key + ": " + state(table));
		}
		System.out.println("== one row, row 0 selected");
		for (String key : ACTIONS) {
			JTable table = cellTable(1);
			table.setRowSelectionInterval(0, 0);
			act(table, key);
			System.out.println("  " + key + ": " + state(table));
		}
		System.out.println("== row table (single selection), row 1 col lead 2, 3 rows 3 columns");
		for (String key : ACTIONS) {
			JTable table = rowTable(3, 3);
			table.changeSelection(1, 2, false, false);
			System.out.println("  before: " + state(table));
			act(table, key);
			System.out.println("  " + key + ": " + state(table));
		}
		System.out.println("== scrolling: 100 rows, viewport 200 px high");
		{
			JTable table = rowTable(100, 2);
			JScrollPane pane = new JScrollPane(table);
			pane.setSize(300, 200);
			pane.doLayout();
			pane.getViewport().doLayout();
			table.setSize(table.getPreferredSize());
			System.out.println("  row height " + table.getRowHeight() + " extent " +
					pane.getViewport().getExtentSize());
			table.setRowSelectionInterval(5, 5);
			System.out.println("  at 5: view " + pane.getViewport().getViewPosition());
			for (int i = 0; i < 20; i++) {
				act(table, "Action.NextRow");
			}
			System.out.println("  after 20 NextRow: " + state(table) + " view " +
					pane.getViewport().getViewPosition());
			table.setRowSelectionInterval(0, 0);
			act(table, "Action.PreviousRowCycle");
			System.out.println("  PreviousRowCycle from 0: " + state(table) + " view " +
					pane.getViewport().getViewPosition());
			// JTable's own Down (its ancestor input map's selectNextRow) from row 5 at the top.
			table.setRowSelectionInterval(5, 5);
			pane.getViewport().setViewPosition(new java.awt.Point(0, 0));
			Object down = table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT)
					.get(KeyStroke.getKeyStroke(KeyEvent.VK_DOWN, 0));
			for (int i = 0; i < 20; i++) {
				table.getActionMap().get(down).actionPerformed(
						new ActionEvent(table, ActionEvent.ACTION_PERFORMED, String.valueOf(down)));
			}
			System.out.println("  after 20 " + down + " (Down): " + state(table) + " view " +
					pane.getViewport().getViewPosition());
		}
		System.out.println("== an empty filtered table (the parts library's sorter and filter)");
		{
			DefaultTableModel model = new DefaultTableModel(3, 2);
			JTable table = new JTable(model);
			table.setSelectionMode(ListSelectionModel.SINGLE_SELECTION);
			TableRowSorter<TableModel> sorter = new TableRowSorter<>(model);
			table.setRowSorter(sorter);
			TableRowTraversalPolicy.setTableRowTraversalPolicy(table);
			sorter.setRowFilter(new RowFilter<TableModel, Integer>() {
				@Override
				public boolean include(Entry<? extends TableModel, ? extends Integer> entry) {
					return false;
				}
			});
			System.out.println("  rows " + table.getRowCount() + ": " + state(table));
			act(table, "Action.NextRowCycle");
			System.out.println("  Action.NextRowCycle: " + state(table));
			try {
				System.out.println("  model row " + table.convertRowIndexToModel(table.getSelectedRow()));
			} catch (RuntimeException e) {
				// ComponentPresetChooserDialog.getSelectedComponentPreset() on Close.
				System.out.println("  convertRowIndexToModel threw " + e);
			}
		}
		System.out.println("== key bindings");
		{
			JTable table = cellTable(3);
			InputMap focused = table.getInputMap(JComponent.WHEN_FOCUSED);
			InputMap ancestor = table.getInputMap(JComponent.WHEN_ANCESTOR_OF_FOCUSED_COMPONENT);
			int[][] strokes = {{KeyEvent.VK_TAB, 0}, {KeyEvent.VK_TAB, InputEvent.SHIFT_DOWN_MASK},
					{KeyEvent.VK_TAB, InputEvent.CTRL_DOWN_MASK},
					{KeyEvent.VK_TAB, InputEvent.CTRL_DOWN_MASK | InputEvent.SHIFT_DOWN_MASK},
					{KeyEvent.VK_RIGHT, 0}, {KeyEvent.VK_LEFT, 0}, {KeyEvent.VK_KP_RIGHT, 0},
					{KeyEvent.VK_KP_LEFT, 0}, {KeyEvent.VK_RIGHT, InputEvent.SHIFT_DOWN_MASK},
					{KeyEvent.VK_RIGHT, InputEvent.CTRL_DOWN_MASK}, {KeyEvent.VK_LEFT, InputEvent.SHIFT_DOWN_MASK},
					{KeyEvent.VK_LEFT, InputEvent.CTRL_DOWN_MASK}, {KeyEvent.VK_DOWN, 0}, {KeyEvent.VK_UP, 0},
					{KeyEvent.VK_ENTER, 0}};
			for (int[] s : strokes) {
				KeyStroke stroke = KeyStroke.getKeyStroke(s[0], s[1]);
				System.out.println("  " + stroke + ": focused " + focused.get(stroke) + ", ancestor " +
						ancestor.get(stroke));
			}
			Set<AWTKeyStroke> forward = table.getFocusTraversalKeys(KeyboardFocusManager.FORWARD_TRAVERSAL_KEYS);
			Set<AWTKeyStroke> backward = table.getFocusTraversalKeys(KeyboardFocusManager.BACKWARD_TRAVERSAL_KEYS);
			System.out.println("  focus forward " + forward + " backward " + backward);
		}
	}
}
