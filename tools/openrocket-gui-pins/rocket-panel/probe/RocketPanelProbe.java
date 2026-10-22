package probe;

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
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.configdialog.ComponentConfigDialog;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import info.openrocket.swing.gui.scalefigure.RocketFigure;
import info.openrocket.swing.gui.scalefigure.RocketPanel;
import info.openrocket.swing.gui.scalefigure.ScaleScrollPane;
import info.openrocket.swing.gui.scalefigure.ViewRotationControl;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

import javax.swing.SwingUtilities;
import javax.swing.tree.DefaultTreeSelectionModel;
import javax.swing.tree.TreePath;
import java.awt.Component;
import java.awt.Container;
import java.awt.Graphics2D;
import java.awt.Point;
import java.awt.event.InputEvent;
import java.awt.event.MouseEvent;
import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;

/**
 * What QtRocket's part P1 (RocketPanel, ViewRotationControl) must reproduce, on OpenRocket's own
 * classes, headless (SwingPreferences, FlatLaf Light, 3D off, no background simulation):
 * <ul>
 * <li>SNAP in out: ViewRotationControl.snapRotation(in) for a list of angles (radians, hex).</li>
 * <li>DRAG orig dy height shift out: RocketPanel.handleMouseDragged() (the left drag rotating the
 * figure) for a press at y0 and a drag to y0 + dy with the scroll pane of the given height.</li>
 * <li>HIT x y | names: the components RocketFigure.getComponentsByPoint() gives at the model point
 * (x, y) in metres of the side view of "A simple model rocket" (settled), rotation 0.</li>
 * <li>STEP label | selection | dialog: a click sequence through RocketPanel.handleMouseClick() (the
 * mouse clicks of its scroll pane, with the click count of its CustomClickCountListener), the
 * selection of the tree selection model after it, and the configuration dialog asked for (main
 * and its config listeners, the others of a multi-edit; "-" for none).</li>
 * </ul>
 * usage: RocketPanelProbe EXAMPLEDIR
 */
public class RocketPanelProbe {

	static RocketPanel panel;
	static RocketFigure figure;
	static ScaleScrollPane scrollPane;
	static DefaultTreeSelectionModel selection;
	static Rocket rocket;

	public static void main(String[] args) throws Exception {
		System.setProperty("openrocket.3d.disable", "true");
		bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		prefs.setComputeFlightInBackground(false);
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					run(new File(args[0]));
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

	static void run(File dir) throws Exception {
		snaps();

		OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, "A simple model rocket.ork")).load();
		rocket = doc.getRocket();
		settle(rocket);
		panel = new RocketPanel(doc);
		selection = new DefaultTreeSelectionModel();
		panel.setSelectionModel(selection);
		figure = panel.getFigure();
		Field f = RocketPanel.class.getDeclaredField("scrollPane");
		f.setAccessible(true);
		scrollPane = (ScaleScrollPane) f.get(panel);
		panel.setSize(1000, 600);
		layoutTree(panel);
		scrollPane.componentResized(null);
		layoutTree(panel);
		paintFigure();
		System.out.println("SIZE panel=" + panel.getWidth() + "x" + panel.getHeight() + " scroll="
				+ scrollPane.getWidth() + "x" + scrollPane.getHeight() + " figure=" + figure.getWidth() + "x"
				+ figure.getHeight() + " origin=" + figure.getSubjectOrigin() + " scale=" + figure.getAbsoluteScale()
				+ " view=" + scrollPane.getViewport().getViewPosition());

		drags();
		hits();
		clicks();
	}

	// ----------------------------------------------------------------------------- snapping

	static void snaps() {
		List<Double> angles = new ArrayList<>();
		double[] degrees = {0, 1, 14.9, 15, 15.1, 20, 22.4, 22.5, 22.6, 26.2, 26.3, 30, 37.4, 37.5, 37.6, 40,
				44, 45, 52.4, 52.5, 52.6, 60, 67.4, 67.5, 67.6, 75, 82.4, 82.5, 90, 97.5, 105, 112.5, 120, 127.5,
				135, 150, 165, 172.4, 172.5, 180, 187.5, 195, 225, 247.5, 270, 300, 315, 330, 337.4, 337.5, 337.6,
				344.9, 345, 345.1, 352.4, 352.5, 352.6, 359, 359.9, 360, 361, 375, -1, -15, -22.5, -30, 720, 1e-9};
		for (double d : degrees) {
			angles.add(Math.toRadians(d));
		}
		double[] radians = {0.1, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, Math.PI, 3.5, 4.0, 5.0, 6.0, 2 * Math.PI, 6.5,
				Math.PI / 4, Math.PI / 6, Math.PI / 12, 7 * Math.PI / 24, Double.NaN, Double.POSITIVE_INFINITY};
		for (double r : radians) {
			angles.add(r);
		}
		for (double a : angles) {
			System.out.println("SNAP " + Double.toHexString(a) + " " + Double.toHexString(ViewRotationControl.snapRotation(a)));
		}
	}

	// ----------------------------------------------------------------------------- dragging

	static void drags() throws Exception {
		Method drag = RocketPanel.class.getDeclaredMethod("handleMouseDragged", MouseEvent.class, Point.class,
				double.class);
		drag.setAccessible(true);
		Component source = scrollPane.getViewport();
		double[] origins = {0, 1, Math.PI, 6};
		int[] dys = {0, 1, -1, 37, -37, 100, -100, 250, -250, 600, -600, 1200, -1300};
		int[] heights = {400, 517};
		int savedHeight = scrollPane.getHeight();
		for (int height : heights) {
			scrollPane.setSize(scrollPane.getWidth(), height);
			for (double origin : origins) {
				for (int dy : dys) {
					for (boolean shift : new boolean[] {false, true}) {
						figure.setRotation(origin);
						Point pressed = new Point(100, 200);
						int modifiers = InputEvent.BUTTON1_DOWN_MASK | (shift ? InputEvent.SHIFT_DOWN_MASK : 0);
						MouseEvent event = new MouseEvent(source, MouseEvent.MOUSE_DRAGGED, 0, modifiers, 100, 200 + dy,
								0, false, MouseEvent.NOBUTTON);
						drag.invoke(panel, event, pressed, origin);
						System.out.println("DRAG " + Double.toHexString(origin) + " " + dy + " " + height + " "
								+ (shift ? 1 : 0) + " " + Double.toHexString(figure.getRotation()));
					}
				}
			}
		}
		scrollPane.setSize(scrollPane.getWidth(), savedHeight);
		figure.setRotation(0);
		layoutTree(panel);
		paintFigure();
	}

	// ----------------------------------------------------------------------------- hit tests

	static final double[][] POINTS = {
			// x, y in metres (side view, rotation 0)
			{0.05, 0.0}, {0.10, 0.0}, {0.12, 0.0}, {0.15, 0.0}, {0.20, 0.0}, {0.25, 0.0}, {0.28, 0.0},
			{0.30, 0.0}, {0.32, 0.0}, {0.34, 0.0}, {0.36, 0.0}, {0.38, 0.0}, {0.40, 0.0}, {0.30, 0.008},
			{0.36, 0.008}, {0.37, 0.011}, {0.385, 0.011}, {0.385, 0.03}, {0.38, -0.03}, {0.39, -0.03},
			{0.30, 0.013}, {0.20, 0.009}, {0.15, 0.009}, {0.5, 0.0}, {0.0, 0.1}, {0.2, 0.1}, {-0.05, 0.0},
			{0.36, 0.0115}, {0.31, 0.0115}, {0.33, -0.012}, {0.3345, 0.006}, {0.3345, 0.0}, {0.354, 0.011},
			{0.15, 0.005}, {0.16, 0.0}, {0.195, 0.0}, {0.25, 0.0135}, {0.399, 0.011}, {0.38, 0.02},
	};

	static void hits() {
		for (double[] p : POINTS) {
			System.out.println("HIT " + p[0] + " " + p[1] + " | " + names(componentsAt(p[0], p[1])));
		}
	}

	static RocketComponent[] componentsAt(double x, double y) {
		Point canvas = canvasPoint(x, y);
		return figure.getComponentsByPoint(canvas.x, canvas.y);
	}

	static Point canvasPoint(double x, double y) {
		Point origin = figure.getSubjectOrigin();
		double scale = figure.getAbsoluteScale();
		return new Point((int) Math.round(origin.x + x * scale), (int) Math.round(origin.y - y * scale));
	}

	// ----------------------------------------------------------------------------- clicks

	/** A click of the button at the model point; count = the click count it is given. */
	static void click(String label, double[] point, int button, int modifiers, int count) throws Exception {
		for (RocketComponent c : rocket) {
			c.clearConfigListeners();
		}
		closeDialog();
		Point canvas = canvasPoint(point[0], point[1]);
		Point view = scrollPane.getViewport().getViewPosition();
		MouseEvent event = new MouseEvent(scrollPane.getViewport(), MouseEvent.MOUSE_CLICKED, 0, modifiers,
				canvas.x - view.x, canvas.y - view.y, count, false, button);
		Method m = RocketPanel.class.getDeclaredMethod("handleMouseClick", MouseEvent.class, int.class);
		m.setAccessible(true);
		String popup = "";
		String dialog = "-";
		try {
			m.invoke(panel, event, count);
		} catch (InvocationTargetException e) {
			Throwable cause = e.getCause();
			if (cause instanceof NullPointerException && cause.getMessage().contains("doComponentTreePopup")) {
				popup = " popup";
			} else if (fromShowDialog(cause)) {
				// The dialog cannot be made here (the appearance panel's injection): it was asked for the
				// component with the config listeners (a multi-edit), else the last selected one (the
				// selection is that component alone, or the multi-edit's main without others).
				RocketComponent[] selectedNow = selected();
				RocketComponent main = selectedNow.length > 0 ? selectedNow[selectedNow.length - 1] : null;
				String others = "[]";
				for (RocketComponent c : rocket) {
					if (!c.getConfigListeners().isEmpty()) {
						main = c;
						others = names(c.getConfigListeners().toArray(new RocketComponent[0]));
					}
				}
				dialog = "dialog " + (main == null ? "?" : name(main)) + " others=" + others;
			} else {
				throw e;
			}
		}
		String buttonName = button == MouseEvent.BUTTON1 ? "left" : button == MouseEvent.BUTTON2 ? "middle" : "right";
		String modifierNames = ((modifiers & InputEvent.SHIFT_DOWN_MASK) != 0 ? "shift" : "")
				+ ((modifiers & InputEvent.CTRL_DOWN_MASK) != 0 ? "ctrl" : "");
		System.out.println("CLICK " + label + " | " + pointName(point) + " " + buttonName + " "
				+ (modifierNames.isEmpty() ? "-" : modifierNames) + " " + count + " | " + names(selected()) + " | "
				+ dialog + " |" + popup);
		closeDialog();
	}

	static boolean fromShowDialog(Throwable t) {
		for (Throwable c = t; c != null; c = c.getCause()) {
			for (StackTraceElement e : c.getStackTrace()) {
				if (e.getClassName().endsWith("ComponentConfigDialog") && e.getMethodName().equals("showDialog")) {
					return true;
				}
			}
		}
		return false;
	}

	static void closeDialog() throws Exception {
		ComponentConfigDialog shown = ComponentConfigDialog.getDialog();
		if (shown != null) {
			shown.dispose();
			Field f = ComponentConfigDialog.class.getDeclaredField("dialog");
			f.setAccessible(true);
			f.set(null, null);
		}
	}

	static RocketComponent[] selected() {
		TreePath[] paths = selection.getSelectionPaths();
		if (paths == null) {
			return new RocketComponent[0];
		}
		RocketComponent[] result = new RocketComponent[paths.length];
		for (int i = 0; i < paths.length; i++) {
			result[i] = (RocketComponent) paths[i].getLastPathComponent();
		}
		return result;
	}

	static void select(String label, String... names) {
		List<TreePath> paths = new ArrayList<>();
		for (String name : names) {
			for (RocketComponent c : rocket) {
				if (name(c).equals(name)) {
					paths.add(ComponentTreeModel.makeTreePath(c));
				}
			}
		}
		selection.setSelectionPaths(paths.toArray(new TreePath[0]));
		System.out.println("SELECT " + label + " | " + String.join(", ", names) + " | " + names(selected()));
	}

	static final double[] STACK = {0.3345, 0.006};
	static final double[] TUBE = {0.25, 0.0};
	static final double[] EMPTY = {0.2, 0.1};

	static String pointName(double[] point) {
		return point == STACK ? "stack" : point == TUBE ? "tube" : point == EMPTY ? "empty" : "?";
	}

	static void clicks() throws Exception {
		final int L = MouseEvent.BUTTON1, M = MouseEvent.BUTTON2, R = MouseEvent.BUTTON3;
		final int SHIFT = InputEvent.SHIFT_DOWN_MASK, CTRL = InputEvent.CTRL_DOWN_MASK;
		// [Engine block, Inner Tube, Body tube], [Body tube], nothing.
		double[] stack = STACK;
		double[] tube = TUBE;
		double[] empty = EMPTY;
		for (double[] point : new double[][] {STACK, TUBE, EMPTY}) {
			System.out.println("POINT " + pointName(point) + " " + Double.toHexString(point[0]) + " "
					+ Double.toHexString(point[1]) + " | " + names(componentsAt(point[0], point[1])));
		}
		final String fins = "Trapezoidal fin set";

		// A: plain clicks.
		select("A start");
		click("A click stack", stack, L, 0, 1);
		click("A click stack again", stack, L, 0, 1);
		select("A select tube", "Body tube");
		click("A click stack", stack, L, 0, 1);
		select("A select inner tube and fins", "Inner Tube", fins);
		click("A click stack", stack, L, 0, 1);
		select("A select engine block and fins", "Engine block", fins);
		click("A click stack", stack, L, 0, 1);
		click("A click tube", tube, L, 0, 1);

		// B: Shift and Ctrl clicks.
		select("B start");
		click("B shift-click stack", stack, L, SHIFT, 1);
		click("B shift-click stack", stack, L, SHIFT, 1);
		click("B shift-click stack", stack, L, SHIFT, 1);
		click("B shift-click stack", stack, L, SHIFT, 1);
		click("B shift-click stack", stack, L, SHIFT, 1);
		click("B ctrl-click tube", tube, L, CTRL, 1);
		click("B ctrl-click tube", tube, L, CTRL, 1);
		click("B ctrl-shift-click tube", tube, L, CTRL | SHIFT, 1);

		// C: clicks on nothing.
		select("C select tube", "Body tube");
		click("C click empty", empty, L, 0, 1);
		select("C select tube", "Body tube");
		click("C middle-click empty", empty, M, 0, 1);
		select("C select tube", "Body tube");
		click("C right-click empty", empty, R, 0, 1);
		select("C select tube", "Body tube");
		click("C shift-click empty", empty, L, SHIFT, 1);
		select("C select tube", "Body tube");
		click("C double-click empty", empty, L, 0, 2);

		// D: the middle button on components.
		select("D select fins", fins);
		click("D middle-click stack", stack, M, 0, 1);
		click("D middle-double-click stack", stack, M, 0, 2);

		// E: the right button (Java selects the LAST clicked component when none of them is selected).
		select("E start");
		click("E right-click stack", stack, R, 0, 1);
		select("E select fins", fins);
		click("E right-click stack", stack, R, 0, 1);
		select("E select inner tube", "Inner Tube");
		click("E right-click stack", stack, R, 0, 1);
		select("E select fins and body tube", fins, "Body tube");
		click("E right-click stack", stack, R, 0, 1);
		select("E start");
		click("E shift-right-click stack", stack, R, SHIFT, 1);
		select("E start");
		click("E right-click tube", tube, R, 0, 1);

		// F: double-clicks (the first click with the count 1, the second with 2).
		select("F start");
		click("F double stack 1", stack, L, 0, 1);
		click("F double stack 2", stack, L, 0, 2);
		select("F select engine block and fins", "Engine block", fins);
		click("F double stack 1", stack, L, 0, 1);
		click("F double stack 2", stack, L, 0, 2);
		select("F start");
		click("F second click alone", stack, L, 0, 2);
		click("F third click", stack, L, 0, 3);

		// G: Shift and Ctrl double-clicks (multi-edit).
		select("G select fins", fins);
		click("G shift-double stack 1", stack, L, SHIFT, 1);
		click("G shift-double stack 2", stack, L, SHIFT, 2);
		select("G select fins and tube", fins, "Body tube");
		click("G ctrl-double tube 1", tube, L, CTRL, 1);
		click("G ctrl-double tube 2", tube, L, CTRL, 2);
		select("G start");
		click("G shift-double tube 1", tube, L, SHIFT, 1);
		click("G shift-double tube 2", tube, L, SHIFT, 2);
		select("G start");
		click("G shift second click alone", tube, L, SHIFT, 2);
		select("G select fins and engine block", fins, "Engine block");
		click("G ctrl second click alone", stack, L, CTRL, 2);
	}

	/** The component's name, with its number among the components of that name when there are several. */
	static String name(RocketComponent component) {
		int count = 0;
		int index = 0;
		for (RocketComponent c : rocket) {
			if (c.getName().equals(component.getName())) {
				count++;
				if (c == component) {
					index = count;
				}
			}
		}
		return count > 1 ? component.getName() + " " + index : component.getName();
	}

	static String names(RocketComponent[] components) {
		StringBuilder sb = new StringBuilder("[");
		for (int i = 0; i < components.length; i++) {
			sb.append(i > 0 ? ", " : "").append(name(components[i]));
		}
		return sb.append("]").toString();
	}

	// ----------------------------------------------------------------------------- support

	static void paintFigure() {
		BufferedImage image = new BufferedImage(Math.max(1, figure.getWidth()), Math.max(1, figure.getHeight()),
				BufferedImage.TYPE_INT_ARGB);
		Graphics2D g = image.createGraphics();
		figure.paint(g);
		g.dispose();
	}

	static void layoutTree(Component c) {
		if (c instanceof Container container) {
			container.doLayout();
			for (Component child : container.getComponents()) {
				layoutTree(child);
			}
		}
	}

	static void bootstrap() throws Exception {
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

	static void settle(Rocket rocket) {
		List<RocketComponent> comps = new ArrayList<>();
		for (RocketComponent c : rocket) {
			comps.add(c);
		}
		List<Object> previous = snapshot(comps);
		for (int pass = 0; pass < 20; pass++) {
			List<Object> current = snapshot(comps);
			if (current.equals(previous)) {
				return;
			}
			previous = current;
		}
		throw new IllegalStateException("did not settle: " + rocket.getName());
	}

	static List<Object> snapshot(List<RocketComponent> comps) {
		List<Object> values = new ArrayList<>();
		for (RocketComponent c : comps) {
			if (c instanceof BodyTube tube) {
				values.add(tube.getOuterRadius());
				values.add(tube.getInnerRadius());
			}
			if (c instanceof SymmetricComponent s) {
				values.add(s.getForeRadius());
				values.add(s.getAftRadius());
				values.add(s.usesPreviousCompAutomatic());
				values.add(s.usesNextCompAutomatic());
			}
			if (c instanceof RingComponent ring) {
				values.add(ring.getOuterRadius());
				values.add(ring.getInnerRadius());
			}
			if (c instanceof MassObject m) {
				values.add(m.getRadius());
				values.add(m.getLength());
			}
		}
		return values;
	}
}
