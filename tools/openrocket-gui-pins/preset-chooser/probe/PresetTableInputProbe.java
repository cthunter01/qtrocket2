package probe;

import java.awt.Rectangle;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.awt.event.MouseEvent;
import java.util.ArrayList;
import java.util.List;

import javax.swing.JTable;
import javax.swing.SwingUtilities;
import javax.swing.table.TableColumn;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.swing.gui.adaptors.PresetModel;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetTable;
import info.openrocket.swing.gui.util.GUIUtil;

import static probe.ShownChooser.config;
import static probe.ShownChooser.idle;
import static probe.ShownChooser.key;
import static probe.ShownChooser.open;
import static probe.ShownChooser.state;
import static probe.ShownChooser.table;

/**
 * What typed text and the mouse do in the table of OpenRocket's parts library shown on xvfb's
 * screen (ShownChooser: FlatLaf Light), and which rows the widths of a first open are measured
 * over, for QtRocket's tests/gui/dialogs/preset/ComponentPresetTableTests.cpp (values pinned by
 * hand: PresetShownProbe runs it, and hand-pinned.txt keeps the transcript). A body tube of a new
 * design over PresetModelProbe's database (the small one of QtRocket's tests).
 *
 * "== typed text": with the lead cell in the Part Number column (view column 2) of view row 0, a
 * robot types 'B' (Shift+B) and '8', which no keyboard search follows (BT-20 and 85865 are rows of
 * the table); then 'x' with the lead cell in the favourite column, whose check box editor JTable
 * starts (autoStartsEdit) and which ignores the letter. Each line the state ShownChooser prints,
 * whether a cell is being edited and the view rows' favourites ("x" ticked).
 *
 * "== clicks in the favourite column": with view row 3 selected (its Part Number cell), a click
 * (press, release and click, as AWT delivers them: to the table, which passes them to the cell's
 * editor), a second click and a double click (the second press with a click count of 2) in the
 * favourite cell of view row 1, near its left edge, in its middle and near its right edge: the
 * favourite, the selected row, the lead column and the tube's preset (a double click in the first
 * column applies nothing).
 *
 * "== the widths of a first open": the dialog of a streamer (the small database's two streamers,
 * one of them legacy, whose description is the longer), made and not shown, with nothing stored:
 * whether each shown column's preferred width, which TableUIPreferences.loadTableUIPreferences()
 * set from GUIUtil.computeOptimalColumnWidths(), is that function's width over every row (no row
 * filter, as when the dialog's constructor loads the layout, before its first updateFilters()) and
 * whether it is its width over the rows the dialog shows (the legacy row filtered out). No width
 * is printed: they depend on the fonts.
 */
public class PresetTableInputProbe {

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
		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		BodyTube tube = new BodyTube();
		doc.getRocket().getStage(0).addChild(tube);
		typedText(doc, tube);
		favouriteClicks(doc, tube);
		firstOpenWidths();
	}

	/** The favourites of the view rows, "x" for a ticked one. */
	static String favourites(JTable t) throws Exception {
		String[] out = new String[1];
		SwingUtilities.invokeAndWait(() -> {
			StringBuilder line = new StringBuilder();
			for (int r = 0; r < t.getRowCount(); r++) {
				line.append(Boolean.TRUE.equals(t.getValueAt(r, 0)) ? "x" : ".");
			}
			out[0] = line.toString();
		});
		return out[0];
	}

	static void close(ComponentPresetChooserDialog d) throws Exception {
		SwingUtilities.invokeAndWait(() -> {
			JTable t = (JTable) fieldOrNull(d);
			if (t != null && t.isEditing()) {
				t.getCellEditor().cancelCellEditing();
			}
			d.setVisible(false);
			d.dispose();
		});
		idle();
	}

	static Object fieldOrNull(ComponentPresetChooserDialog d) {
		try {
			return table(d);
		} catch (Exception e) {
			return null;
		}
	}

	static void typedText(OpenRocketDocument doc, BodyTube tube) throws Exception {
		System.out.println("== the parts library shown: typed text in its table");
		ComponentPresetChooserDialog d = open(tube, doc);
		JTable t = table(d);
		SwingUtilities.invokeAndWait(() -> {
			t.requestFocusInWindow();
			t.changeSelection(0, 2, false, false);
		});
		Thread.sleep(300);
		System.out.println("  lead (0, 2) '" + t.getValueAt(0, 2) + "': " + state(d, t, tube) + " editing " + t.isEditing()
				+ " favourites " + favourites(t));
		key(KeyEvent.VK_B, InputEvent.SHIFT_DOWN_MASK);
		System.out.println("  'B': " + state(d, t, tube) + " editing " + t.isEditing() + " favourites " + favourites(t));
		key(KeyEvent.VK_8, 0);
		System.out.println("  '8': " + state(d, t, tube) + " editing " + t.isEditing() + " favourites " + favourites(t));
		SwingUtilities.invokeAndWait(() -> {
			if (t.isEditing()) {
				t.getCellEditor().cancelCellEditing();
			}
			t.requestFocusInWindow();
			t.changeSelection(0, 0, false, false);
		});
		Thread.sleep(300);
		key(KeyEvent.VK_X, 0);
		System.out.println("  'x', the lead cell (0, 0): " + state(d, t, tube) + " editing " + t.isEditing()
				+ " favourites " + favourites(t));
		close(d);
	}

	static long time = 1_000_000L;

	/** Sends the table a press, a release and a click of the left button at (@p x, @p y). */
	static void click(JTable t, int x, int y, int count) {
		time += 10;
		t.dispatchEvent(new MouseEvent(t, MouseEvent.MOUSE_PRESSED, time, InputEvent.BUTTON1_DOWN_MASK, x, y, count,
				false, MouseEvent.BUTTON1));
		t.dispatchEvent(new MouseEvent(t, MouseEvent.MOUSE_RELEASED, time, 0, x, y, count, false, MouseEvent.BUTTON1));
		t.dispatchEvent(new MouseEvent(t, MouseEvent.MOUSE_CLICKED, time, 0, x, y, count, false, MouseEvent.BUTTON1));
	}

	static String clickState(ComponentPresetChooserDialog d, JTable t, BodyTube tube) {
		return "favourite " + t.getValueAt(1, 0) + ", selected row " + t.getSelectedRow() + ", lead column "
				+ t.getColumnModel().getSelectionModel().getLeadSelectionIndex() + ", editing " + t.isEditing()
				+ ", visible " + d.isVisible() + ", preset " + tube.getPresetComponent();
	}

	static void favouriteClicks(OpenRocketDocument doc, BodyTube tube) throws Exception {
		ShownChooser.resetPreferences();
		System.out.println("== the parts library shown: clicks in its favourite column");
		ComponentPresetChooserDialog d = open(tube, doc);
		JTable t = table(d);
		Rectangle[] cell = new Rectangle[1];
		SwingUtilities.invokeAndWait(() -> cell[0] = t.getCellRect(1, 0, false));
		int y = cell[0].y + cell[0].height / 2;
		for (int x : new int[] { 2, cell[0].width / 2, cell[0].width - 3 }) {
			List<String> lines = new ArrayList<>();
			SwingUtilities.invokeAndWait(() -> {
				t.changeSelection(3, 2, false, false);
				lines.add("x " + x + " of " + cell[0].width + ", view row 3 selected");
				click(t, cell[0].x + x, y, 1);
				lines.add("a click: " + clickState(d, t, tube));
				click(t, cell[0].x + x, y, 1);
				lines.add("a second click: " + clickState(d, t, tube));
				t.changeSelection(3, 2, false, false);
				click(t, cell[0].x + x, y, 1);
				click(t, cell[0].x + x, y, 2);
				lines.add("view row 3 selected, a double click: " + clickState(d, t, tube));
			});
			System.out.println("  " + String.join("; ", lines));
		}
		close(d);
	}

	static void firstOpenWidths() throws Exception {
		ShownChooser.resetPreferences();
		System.out.println("== the parts library: the widths of a first open (streamers)");
		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		BodyTube body = new BodyTube();
		doc.getRocket().getStage(0).addChild(body);
		Streamer streamer = new Streamer();
		body.addChild(streamer);
		List<String> lines = new ArrayList<>();
		SwingUtilities.invokeAndWait(() -> {
			try {
				ComponentPresetChooserDialog d = new ComponentPresetChooserDialog(config, streamer,
						new PresetModel(null, doc, streamer));
				ComponentPresetTable t = (ComponentPresetTable) table(d);
				int shown = t.getRowCount();
				int[] open = GUIUtil.computeOptimalColumnWidths(t, 20, true);
				t.setRowFilter(null);
				int all = t.getRowCount();
				int[] every = GUIUtil.computeOptimalColumnWidths(t, 20, true);
				lines.add("rows shown " + shown + ", every row " + all);
				for (int i = 0; i < t.getColumnCount(); i++) {
					TableColumn column = t.getColumnModel().getColumn(i);
					int preferred = column.getPreferredWidth();
					lines.add(column.getIdentifier() + ": over every row " + (preferred == every[i])
							+ ", over the rows shown " + (preferred == open[i]) + ", the two differ "
							+ (every[i] != open[i]));
				}
				d.dispose();
			} catch (Exception e) {
				throw new IllegalStateException(e);
			}
		});
		for (String line : lines) {
			System.out.println("  " + line);
		}
	}
}
