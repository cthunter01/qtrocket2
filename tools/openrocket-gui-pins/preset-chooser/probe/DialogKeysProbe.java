package probe;

import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;

import javax.swing.JTable;
import javax.swing.JTextField;
import javax.swing.SwingUtilities;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;

import static probe.ShownChooser.config;
import static probe.ShownChooser.field;
import static probe.ShownChooser.focus;
import static probe.ShownChooser.frame;
import static probe.ShownChooser.key;
import static probe.ShownChooser.open;
import static probe.ShownChooser.robot;
import static probe.ShownChooser.state;
import static probe.ShownChooser.table;

/**
 * What the keys do in OpenRocket's parts library shown on xvfb's screen (ShownChooser: FlatLaf
 * Light, pressed with a robot), for QtRocket's tests/gui/dialogs/preset/ComponentPresetTableTests.cpp
 * and ComponentPresetChooserDialogTests.cpp (values pinned by hand: PresetShownProbe runs it, and
 * hand-pinned.txt keeps the transcript): the focus order from the filter field (Tab nine times; in
 * the table Tab moves from row to row, TableRowTraversalPolicy), the table's Enter and Shift+Enter
 * (selectNextRowCell and selectPreviousRowCell: down the column, from the last row to the next
 * column's first, from the last cell to the first; without a lead cell to the first cell, Shift+Enter
 * to the last; nothing without rows) none of which presses the dialog's default button Close,
 * Space in the table, Escape in the table (the dialog closes without a preset), Ctrl+Tab out of the
 * table, the focus order of a body tube between two tubes (both diameter boxes), Enter in the
 * filter field (Close: the selected row's preset applied) and Escape. A body tube of a new design
 * over PresetModelProbe's database (the small one of QtRocket's tests, 12 body tubes); each line
 * the dialog's visibility, the table's selected row, column and lead row, the focus owner and the
 * tube's preset.
 */
public class DialogKeysProbe {

	/** Runs it alone (PresetShownProbe runs it with the others). */
	public static void main(String[] args) {
		try {
			PresetModelProbe.bootstrap(args);
			ShownChooser.setUp();
			run();
			ShownChooser.tearDown();
		} catch (Throwable e) {
			e.printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}

	/** Prints the transcript (PresetShownProbe runs it). */
	static void run() throws Exception {
		ShownChooser.resetPreferences();
		System.out.println("== the parts library shown: its keys");
		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		BodyTube tube = new BodyTube();
		doc.getRocket().getStage(0).addChild(tube);

		ComponentPresetChooserDialog d = open(tube, doc);
		JTable t = table(d);
		System.out.println("frame " + frame.getSize() + " config " + config.getSize() + " dialog " + d.getSize()
				+ " at " + d.getLocation() + " frame at " + frame.getLocation() + " rows " + t.getRowCount());
		System.out.println("initial focus " + focus());
		StringBuilder tabs = new StringBuilder();
		for (int i = 0; i < 9; i++) {
			key(KeyEvent.VK_TAB, 0);
			tabs.append(" -> ").append(focus());
		}
		System.out.println("Tab:" + tabs);
		// the table: Enter, Shift+Enter, Escape
		SwingUtilities.invokeAndWait(() -> {
			t.requestFocusInWindow();
		});
		Thread.sleep(300);
		System.out.println("table focused, nothing selected: " + state(d, t, tube));
		key(KeyEvent.VK_ENTER, 0);
		System.out.println("Enter, nothing selected: " + state(d, t, tube));
		SwingUtilities.invokeAndWait(() -> {
			t.setRowSelectionInterval(2, 2);
			t.setColumnSelectionInterval(3, 3);
		});
		System.out.println("row 2 col 3 selected: " + state(d, t, tube));
		key(KeyEvent.VK_ENTER, 0);
		System.out.println("Enter: " + state(d, t, tube));
		key(KeyEvent.VK_ENTER, InputEvent.SHIFT_DOWN_MASK);
		System.out.println("Shift+Enter: " + state(d, t, tube));
		int last = t.getRowCount() - 1;
		SwingUtilities.invokeAndWait(() -> {
			t.setRowSelectionInterval(last, last);
			t.setColumnSelectionInterval(3, 3);
		});
		key(KeyEvent.VK_ENTER, 0);
		System.out.println("Enter on the last row: " + state(d, t, tube));
		SwingUtilities.invokeAndWait(() -> {
			t.setRowSelectionInterval(0, 0);
			t.setColumnSelectionInterval(3, 3);
		});
		key(KeyEvent.VK_ENTER, InputEvent.SHIFT_DOWN_MASK);
		System.out.println("Shift+Enter on row 0: " + state(d, t, tube));
		int lastColumn = t.getColumnCount() - 1;
		SwingUtilities.invokeAndWait(() -> {
			t.setRowSelectionInterval(last, last);
			t.setColumnSelectionInterval(lastColumn, lastColumn);
		});
		key(KeyEvent.VK_ENTER, 0);
		System.out.println("Enter on the last cell: " + state(d, t, tube));
		key(KeyEvent.VK_SPACE, 0);
		System.out.println("Space: " + state(d, t, tube) + " favourite of row " + t.getSelectedRow() + " "
				+ t.getValueAt(t.getSelectedRow(), 0));
		key(KeyEvent.VK_ESCAPE, 0);
		System.out.println("Escape in the table: " + state(d, t, tube));

		// A fresh dialog: Enter in the table before anything was selected; Ctrl+Tab out of it.
		{
			ComponentPresetChooserDialog f = open(tube, doc);
			JTable ft = table(f);
			SwingUtilities.invokeAndWait(() -> ft.requestFocusInWindow());
			Thread.sleep(300);
			System.out.println("fresh, table focused: " + state(f, ft, tube));
			key(KeyEvent.VK_ENTER, 0);
			System.out.println("fresh, Enter: " + state(f, ft, tube));
			robot.keyPress(KeyEvent.VK_CONTROL);
			key(KeyEvent.VK_TAB, 0);
			robot.keyRelease(KeyEvent.VK_CONTROL);
			robot.waitForIdle();
			System.out.println("Ctrl+Tab: " + focus());
			key(KeyEvent.VK_TAB, 0);
			System.out.println("Tab: " + focus());
			key(KeyEvent.VK_TAB, 0);
			System.out.println("Tab: " + focus());
			key(KeyEvent.VK_ESCAPE, 0);
		}
		{
			ComponentPresetChooserDialog f = open(tube, doc);
			JTable ft = table(f);
			SwingUtilities.invokeAndWait(() -> ft.requestFocusInWindow());
			Thread.sleep(300);
			key(KeyEvent.VK_ENTER, InputEvent.SHIFT_DOWN_MASK);
			System.out.println("fresh, Shift+Enter: " + state(f, ft, tube) + " lead col "
					+ ft.getColumnModel().getSelectionModel().getLeadSelectionIndex() + " of " + ft.getColumnCount());
			key(KeyEvent.VK_ESCAPE, 0);
			f = open(tube, doc);
			JTable et = table(f);
			JTextField ftext = (JTextField) field(f, "filterText");
			SwingUtilities.invokeAndWait(() -> {
				ftext.setText("nothing matches this");
				et.requestFocusInWindow();
			});
			Thread.sleep(300);
			System.out.println("empty table: " + state(f, et, tube) + " rows " + et.getRowCount());
			key(KeyEvent.VK_ENTER, 0);
			System.out.println("empty table, Enter: " + state(f, et, tube));
			key(KeyEvent.VK_ESCAPE, 0);
			// one row
			f = open(tube, doc);
			JTable ot = table(f);
			JTextField otext = (JTextField) field(f, "filterText");
			SwingUtilities.invokeAndWait(() -> {
				otext.setText("31360");
				ot.requestFocusInWindow();
			});
			Thread.sleep(300);
			System.out.println("one row: " + state(f, ot, tube) + " rows " + ot.getRowCount());
			key(KeyEvent.VK_ENTER, 0);
			System.out.println("one row, Enter: " + state(f, ot, tube));
			key(KeyEvent.VK_ENTER, 0);
			System.out.println("one row, Enter: " + state(f, ot, tube));
			key(KeyEvent.VK_ESCAPE, 0);
		}
		// A tube between two tubes: the boxes in the focus order.
		{
			BodyTube before = new BodyTube();
			BodyTube after = new BodyTube();
			doc.getRocket().getStage(0).addChild(before, 0);
			doc.getRocket().getStage(0).addChild(after);
			ComponentPresetChooserDialog f = open(tube, doc);
			StringBuilder order = new StringBuilder(focus());
			for (int i = 0; i < 4; i++) {
				key(KeyEvent.VK_TAB, 0);
				order.append(" -> ").append(focus());
			}
			System.out.println("between tubes, Tab: " + order);
			key(KeyEvent.VK_ESCAPE, 0);
			doc.getRocket().getStage(0).removeChild(before);
			doc.getRocket().getStage(0).removeChild(after);
		}
		// Enter in the filter field presses Close
		d = open(tube, doc);
		JTable t2 = table(d);
		JTextField text = (JTextField) field(d, "filterText");
		SwingUtilities.invokeAndWait(() -> {
			t2.setRowSelectionInterval(1, 1);
			text.requestFocusInWindow();
		});
		Thread.sleep(300);
		System.out.println("second dialog " + d.getSize() + " at " + d.getLocation() + ", text focused: " + state(d, t2, tube));
		key(KeyEvent.VK_ENTER, 0);
		System.out.println("Enter in the filter field: " + state(d, t2, tube));

		// Escape in the field
		d = open(tube, doc);
		JTable t3 = table(d);
		SwingUtilities.invokeAndWait(() -> t3.setRowSelectionInterval(3, 3));
		System.out.println("third dialog " + d.getSize() + " at " + d.getLocation() + " focus " + focus());
		key(KeyEvent.VK_ESCAPE, 0);
		System.out.println("Escape: " + state(d, t3, tube));
	}
}
