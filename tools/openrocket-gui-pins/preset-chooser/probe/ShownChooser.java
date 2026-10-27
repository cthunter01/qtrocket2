package probe;

import java.awt.Component;
import java.awt.KeyboardFocusManager;
import java.awt.Robot;
import java.awt.event.InputEvent;
import java.awt.event.KeyEvent;
import java.lang.reflect.Field;

import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JTable;
import javax.swing.SwingUtilities;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.adaptors.PresetModel;
import info.openrocket.swing.gui.dialogs.preset.ComponentPresetChooserDialog;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * What the probes of OpenRocket's parts library shown on xvfb's screen share (DialogKeysProbe,
 * SpaceRobotProbe, PresetTableInputProbe): FlatLaf Light, a main window of 1000 x 800 pixels at
 * (50, 50) owning a configuration dialog of 400 x 300 pixels, both shown, and over them
 * ComponentPresetChooserDialog as RocketComponentConfig.selectPreset() makes it, shown from another
 * thread (its setVisible() blocks while the modal dialog is open); a java.awt.Robot presses the
 * keys, each followed by the toolkit's idle wait and 150 ms. Each probe starts from the preferences
 * of a fresh store (resetPreferences()), as when it runs alone.
 */
final class ShownChooser {

	static Robot robot;
	static JFrame frame;
	static JDialog config;

	private ShownChooser() {
	}

	/** The theme, the robot and the two windows, shown. */
	static void setUp() throws Exception {
		SwingUtilities.invokeAndWait(() -> GUIUtil.setUITheme(UITheme.Themes.LIGHT));
		robot = new Robot();
		SwingUtilities.invokeAndWait(() -> {
			frame = new JFrame("main");
			frame.setSize(1000, 800);
			frame.setLocation(50, 50);
			frame.setVisible(true);
			config = new JDialog(frame, "config");
			config.setSize(400, 300);
			config.setVisible(true);
		});
		robot.waitForIdle();
		Thread.sleep(500);
	}

	/**
	 * No favourite ticked, and nothing stored of the tables' layouts and the windows' places and sizes
	 * (the dialog's Close stores both): what a probe that runs after another finds as a fresh store.
	 */
	static void resetPreferences() throws Exception {
		SwingPreferences preferences = (SwingPreferences) Application.getPreferences();
		for (ComponentPreset preset : PresetModelProbe.presets) {
			preferences.setComponentFavorite(preset, preset.getType(), false);
		}
		preferences.getTablePreferences().clear();
		preferences.getWindowsPreferences().clear();
	}

	/** Disposes of the two windows. */
	static void tearDown() throws Exception {
		SwingUtilities.invokeAndWait(() -> {
			config.dispose();
			frame.dispose();
		});
	}

	/** The field @p name of the dialog @p dialog. */
	static Object field(Object dialog, String name) throws Exception {
		Field f = ComponentPresetChooserDialog.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(dialog);
	}

	/** The dialog's table (its field componentSelectionTable). */
	static JTable table(ComponentPresetChooserDialog dialog) throws Exception {
		return (JTable) field(dialog, "componentSelectionTable");
	}

	/** The focus owner's class, with a button's text. */
	static String focus() {
		Component c = KeyboardFocusManager.getCurrentKeyboardFocusManager().getFocusOwner();
		if (c == null) {
			return "null";
		}
		String text = c instanceof javax.swing.AbstractButton b ? " '" + b.getText() + "'" : "";
		return c.getClass().getSimpleName() + text;
	}

	/** Presses and releases the key @p code, with Shift when @p modifiers say so. */
	static void key(int code, int modifiers) throws Exception {
		if ((modifiers & InputEvent.SHIFT_DOWN_MASK) != 0) {
			robot.keyPress(KeyEvent.VK_SHIFT);
		}
		robot.keyPress(code);
		robot.keyRelease(code);
		if ((modifiers & InputEvent.SHIFT_DOWN_MASK) != 0) {
			robot.keyRelease(KeyEvent.VK_SHIFT);
		}
		idle();
	}

	/** The toolkit's idle wait, 150 ms, and another idle wait. */
	static void idle() throws Exception {
		robot.waitForIdle();
		Thread.sleep(150);
		robot.waitForIdle();
	}

	/** The dialog's and the table's state, and @p component's preset. */
	static String state(JDialog d, JTable t, RocketComponent component) throws Exception {
		String[] out = new String[1];
		SwingUtilities.invokeAndWait(() -> out[0] = "visible " + d.isVisible() + " row " + t.getSelectedRow() + " col "
				+ t.getSelectedColumn() + " lead " + t.getSelectionModel().getLeadSelectionIndex() + " focus " + focus()
				+ " preset " + component.getPresetComponent());
		return out[0];
	}

	/** The parts library of @p component, owned by the configuration dialog, shown. */
	static ComponentPresetChooserDialog open(RocketComponent component, OpenRocketDocument doc) throws Exception {
		ComponentPresetChooserDialog[] d = new ComponentPresetChooserDialog[1];
		SwingUtilities.invokeAndWait(() -> d[0] = new ComponentPresetChooserDialog(config, component,
				new PresetModel(null, doc, component)));
		new Thread(() -> d[0].setVisible(true)).start();
		for (int i = 0; i < 100 && !d[0].isShowing(); i++) {
			Thread.sleep(50);
		}
		robot.waitForIdle();
		Thread.sleep(500);
		robot.waitForIdle();
		return d[0];
	}
}
