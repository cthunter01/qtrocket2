package probe;

import java.awt.event.KeyEvent;

import javax.swing.JTable;
import javax.swing.SwingUtilities;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;

import static probe.ShownChooser.key;
import static probe.ShownChooser.open;
import static probe.ShownChooser.state;
import static probe.ShownChooser.table;

/**
 * What Space does in the table of OpenRocket's parts library shown on xvfb's screen (ShownChooser:
 * FlatLaf Light, pressed with a robot), for QtRocket's
 * tests/gui/dialogs/preset/ComponentPresetTableTests.cpp (values pinned by hand: PresetShownProbe
 * runs it, and hand-pinned.txt keeps the transcript): with the lead cell in the favourite column
 * Space ticks the row's favourite and a second Space unticks it, although JTable binds Space to
 * addToSelection (the favourite's check box editor takes the key); in another column it changes
 * nothing. Each line the favourites of the view rows before and after ("x" ticked), the state
 * ShownChooser prints and whether a cell is being edited.
 */
public class SpaceRobotProbe {

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
		System.out.println("== the parts library shown: Space in its table");
		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		BodyTube tube = new BodyTube();
		doc.getRocket().getStage(0).addChild(tube);

		ComponentPresetChooserDialog d = open(tube, doc);
		JTable t = table(d);
		int[][] cells = { { 2, 3 }, { 4, 0 }, { 4, 0 }, { 6, 1 } };
		for (int[] c : cells) {
			SwingUtilities.invokeAndWait(() -> {
				t.requestFocusInWindow();
				t.changeSelection(c[0], c[1], false, false);
			});
			Thread.sleep(200);
			StringBuilder before = new StringBuilder();
			for (int r = 0; r < t.getRowCount(); r++) before.append(t.getValueAt(r, 0).equals(Boolean.TRUE) ? "x" : ".");
			key(KeyEvent.VK_SPACE, 0);
			StringBuilder after = new StringBuilder();
			for (int r = 0; r < t.getRowCount(); r++) after.append(t.getValueAt(r, 0).equals(Boolean.TRUE) ? "x" : ".");
			System.out.println("Space at (" + c[0] + "," + c[1] + "): favourites " + before + " -> " + after + "; " + state(d, t, tube) + " editing " + t.isEditing());
		}
		key(KeyEvent.VK_ESCAPE, 0);
	}
}
