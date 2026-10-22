package probe;

import java.awt.Component;
import java.awt.Container;
import java.awt.Point;
import java.awt.event.MouseEvent;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.swing.AbstractAction;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JComponent;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JMenuItem;
import javax.swing.JPopupMenu;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTable;
import javax.swing.JViewport;
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
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.position.AxialMethod;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.adaptors.IntegerModel;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.configdialog.FreeformFinSetConfig;
import info.openrocket.swing.gui.configdialog.MaterialPanel;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;
import info.openrocket.swing.gui.scalefigure.AbstractScaleFigure;
import info.openrocket.swing.gui.scalefigure.FinPointFigure;
import info.openrocket.swing.gui.scalefigure.ScaleScrollPane;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;
import info.openrocket.swing.gui.widgets.GroupableAndSearchableComboBox;

/**
 * What QtRocket's freeform fin editor is compared with (FreeformPanelPins.h): OpenRocket's FreeformFinSetConfig (its
 * General, Shape and Fin tabs tabs) with FinPointFigure and its FinPointScrollPane, under Xvfb, the panel in a JDialog that is never shown (so
 * that nothing lays it out or moves the focus).
 * <ul>
 * <li>Part A, "panel": the panel of every freeform fin set of the 16 examples (loaded without presets) and the 13
 * test rockets: the tabs' titles and tooltips, the General and Fin tabs' widgets (as FinPanelProbe), the Shape tab's
 * point table (headers, cells, which are editable), labels, buttons, check box and "Point Actions" menu, and the
 * figure fitted into a scroll pane of 600 x 400 pixels at 96 dpi: its scale, the x of its origin and its
 * subject and content bounds.</li>
 * <li>Part B, "edit": steps on designs built here, each followed by the fin points, the table, the figure's
 * selection and highlight, the edit history and the undo/redo buttons: table edits (the text the cell editor
 * commits), the table's Insert point and Delete point, mouse presses, drags, releases and clicks on the scroll
 * pane (at model coordinates turned into the figure's pixels by its projection, plus a pixel offset), the undo and
 * redo buttons, the live update preference, hit tests and the CSV export.</li>
 * <li>Part C, "multi": the same steps with a second freeform fin set selected (Java: its config listener), after
 * each the points of both.</li>
 * </ul>
 * Arguments: the directory of the 16 examples, the transcript file (OpenRocket's log goes to standard output, its
 * warnings and errors only), the motor database (initial_motors.db). Run it through generate.sh.
 */
public class FreeformPanelProbe {
	static final String[] MAKERS = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods"};

	static final int PANE_WIDTH = 600;
	static final int PANE_HEIGHT = 400;

	static PrintStream out;

	public static void main(String[] args) throws Exception {
		out = new PrintStream(new FileOutputStream(args[1]), true, StandardCharsets.UTF_8);
		((ch.qos.logback.classic.Logger) org.slf4j.LoggerFactory.getLogger(org.slf4j.Logger.ROOT_LOGGER_NAME))
				.setLevel(ch.qos.logback.classic.Level.WARN);
		bootstrap(args[2]);
		Thread.setDefaultUncaughtExceptionHandler((t, e) -> out.println("uncaught " + e.getClass().getName()));
		Application.getPreferences().putInt("DPI", 960);
		prefs().setUpdateRocketWhileDraggingPoint(false);
		edt(() -> {
			UITheme.Themes.LIGHT.applyTheme();
			Thread.currentThread().setUncaughtExceptionHandler(
					(t, e) -> out.println("uncaught " + e.getClass().getName()));
		});
		File dir = new File(args[0]);
		String[] examples = dir.list((d, n) -> n.endsWith(".ork"));
		java.util.Arrays.sort(examples);
		for (String name : examples) {
			OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
			edt(() -> design("example", name, doc));
		}
		for (String maker : MAKERS) {
			Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
			OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
			edt(() -> design("testrocket", maker, doc));
		}
		built();
		out.flush();
		out.close();
		System.exit(0);
	}

	static SwingPreferences prefs() {
		return (SwingPreferences) Application.getPreferences();
	}

	// ----------------------------------------------------------------------------- Part A

	static void design(String source, String name, OpenRocketDocument doc) throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		List<RocketComponent> comps = preorder(rocket);
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			if (c.getClass() != FreeformFinSet.class) {
				continue;
			}
			settle(rocket);
			out.println("== panel | " + source + " | " + name + " | " + i + " | " + c.getClass().getSimpleName());
			JDialog dialog = new JDialog();
			FreeformFinSetConfig panel = new FreeformFinSetConfig(doc, c, dialog);
			fit(panel);
			List<String> lines = new ArrayList<>();
			tabsLines(panel, lines);
			shapeLines(panel, lines);
			figureLines(panel, lines);
			print(lines);
			panel.invalidateMe();
			dialog.dispose();
		}
	}

	/** The scroll pane 600 x 400 pixels, fitted, and the figure painted once (its hit tests need that). */
	static void fit(FreeformFinSetConfig panel) throws Exception {
		ScaleScrollPane pane = pane(panel);
		pane.setSize(PANE_WIDTH, PANE_HEIGHT);
		pane.setFitting(true);
		FinPointFigure figure = figure(panel);
		BufferedImage image = new BufferedImage(PANE_WIDTH, PANE_HEIGHT, BufferedImage.TYPE_INT_ARGB);
		java.awt.Graphics2D g = image.createGraphics();
		g.setClip(0, 0, PANE_WIDTH, PANE_HEIGHT);
		figure.paint(g);
		g.dispose();
	}

	static void tabsLines(FreeformFinSetConfig panel, List<String> lines) throws Exception {
		JTabbedPane tabs = tabs(panel);
		for (int t = 0; t < tabs.getTabCount(); t++) {
			lines.add("tab" + t + " " + q(tabs.getTitleAt(t)) + " tip=" + q(tabs.getToolTipTextAt(t)));
		}
		List<String> general = new ArrayList<>();
		walk((Container) tabs.getComponentAt(0), general, true);
		for (String line : general) {
			lines.add("general " + line);
		}
		List<String> finTabs = new ArrayList<>();
		walk((Container) tabs.getComponentAt(2), finTabs, true);
		for (String line : finTabs) {
			lines.add("fintabs " + line);
		}
		for (String text : new String[] {"Split fins", "Convert to freeform"}) {
			JButton b = button(panel, text);
			lines.add("row " + (b == null ? "no button " + q(text) : buttonLine(b)));
		}
	}

	/** The Shape tab: the table, the labels, the buttons, the check box and the Point Actions menu. */
	static void shapeLines(FreeformFinSetConfig panel, List<String> lines) throws Exception {
		JTabbedPane tabs = tabs(panel);
		Container shape = (Container) tabs.getComponentAt(1);
		JTable table = table(panel);
		lines.add("shape table header " + q(table.getColumnName(0)) + " " + q(table.getColumnName(1)));
		for (int row = 0; row < table.getRowCount(); row++) {
			lines.add(String.format("shape table row %02d %s %s editable=%b,%b", row, q(String.valueOf(table.getValueAt(row, 0))),
					q(String.valueOf(table.getValueAt(row, 1))), table.isCellEditable(row, 0),
					table.isCellEditable(row, 1)));
		}
		List<String> walked = new ArrayList<>();
		shapeWalk(shape, walked);
		for (String line : walked) {
			lines.add("shape " + line);
		}
	}

	static void shapeWalk(Container root, List<String> lines) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof JTable || c instanceof FinPointFigure || c instanceof ScaleScrollPane) {
				continue;
			}
			if (c instanceof JComboBox<?> box) {
				lines.add("combo " + q(String.valueOf(box.getSelectedItem())) + " editable=" + box.isEditable());
				continue;
			}
			if (c instanceof JCheckBox b) {
				lines.add("check " + q(b.getText()) + " checked=" + b.isSelected() + " tip=" + q(b.getToolTipText()));
				continue;
			}
			if (c instanceof JButton b) {
				lines.add("button " + q(b.getText()) + " enabled=" + b.isEnabled() + " focusable=" + b.isFocusable()
						+ " tip=" + q(b.getToolTipText()));
				if (b instanceof info.openrocket.swing.gui.widgets.DropdownButton d) {
					JPopupMenu menu = d.getPopupMenu();
					for (Component item : menu.getComponents()) {
						if (item instanceof JMenuItem m) {
							lines.add("menu " + q(m.getText()) + " enabled=" + m.isEnabled());
						}
					}
				}
				continue;
			}
			if (c instanceof JLabel l) {
				if (l.getText() != null && !l.getText().isEmpty()) {
					lines.add("label " + q(l.getText()) + " size=" + l.getFont().getSize2D() + " base="
							+ new JLabel().getFont().getSize2D());
				}
				continue;
			}
			if (c instanceof Container k) {
				shapeWalk(k, lines);
			}
		}
	}

	static void figureLines(FreeformFinSetConfig panel, List<String> lines) throws Exception {
		FinPointFigure figure = figure(panel);
		ScaleScrollPane pane = pane(panel);
		lines.add("figure fitting=" + pane.isFitting() + " userScale=" + d(figure.getUserScale()) + " scale="
				+ d(figure.getAbsoluteScale()));
		Point origin = (Point) get(figure, AbstractScaleFigure.class, "originLocation_px");
		lines.add("figure originX=" + origin.x);
		lines.add("figure subject=" + rect((Rectangle2D) get(figure, AbstractScaleFigure.class, "subjectBounds_m")));
		lines.add("figure content=" + rect((Rectangle2D) get(figure, AbstractScaleFigure.class, "contentBounds_m")));
		lines.add("figure autoZoomX=" + figure.getAutoZoomPoint().x);
	}

	static String rect(Rectangle2D r) {
		return "(" + d(r.getX()) + "," + d(r.getY()) + "," + d(r.getWidth()) + "," + d(r.getHeight()) + ")";
	}

	// ----------------------------------------------------------------------------- Part B and C

	/** Rocket > Stage > body tube (30 cm long, 2.1 cm radius, not automatic) > the component(s). */
	static OpenRocketDocument onBodyTube(RocketComponent... children) {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		body.setOuterRadiusAutomatic(false);
		body.setOuterRadius(0.021);
		body.setLength(0.3);
		stage.addChild(body);
		for (RocketComponent child : children) {
			body.addChild(child);
		}
		rocket.enableEvents();
		return OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
	}

	/** Rocket > Stage > a conical transition (10 cm, radii 2 and 3 cm) > the component(s). */
	static OpenRocketDocument onTransition(RocketComponent... children) {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		Transition transition = new Transition();
		transition.setShapeType(Transition.Shape.CONICAL);
		transition.setForeRadiusAutomatic(false);
		transition.setAftRadiusAutomatic(false);
		transition.setForeRadius(0.02);
		transition.setAftRadius(0.03);
		transition.setLength(0.1);
		stage.addChild(transition);
		for (RocketComponent child : children) {
			transition.addChild(child);
		}
		rocket.enableEvents();
		return OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
	}

	/** Rocket > Stage > an ogive nose cone (15 cm, base radius 2.5 cm) > the component(s). */
	static OpenRocketDocument onNoseCone(RocketComponent... children) {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		NoseCone nose = new NoseCone();
		nose.setShapeType(Transition.Shape.OGIVE);
		nose.setBaseRadiusAutomatic(false);
		nose.setBaseRadius(0.025);
		nose.setLength(0.15);
		stage.addChild(nose);
		for (RocketComponent child : children) {
			nose.addChild(child);
		}
		rocket.enableEvents();
		return OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
	}

	static FreeformFinSet freeform(double... xy) {
		FreeformFinSet fins = new FreeformFinSet();
		if (xy.length > 0) {
			CoordinateIF[] points = new CoordinateIF[xy.length / 2];
			for (int i = 0; i < points.length; i++) {
				points[i] = new Coordinate(xy[2 * i], xy[2 * i + 1]);
			}
			fins.setPoints(points);
		}
		return fins;
	}

	static List<String> steps(String... steps) {
		return List.of(steps);
	}

	static void built() throws Exception {
		// The default outline (0,0) (2.5,5) (7.5,5) (5,0) cm on a body tube.
		edits("body tube hits", onBodyTube(freeform()), 3, steps("hit 1 0 0", "hit 1 14 0", "hit 1 16 0",
				"hit 1 10 10", "hit 1 11 11", "hit 0 0 -14", "hit 3 -14 0", "hitAt 0.0125 0.025",
				"hitAt 0.05 0.05", "hitAt 0.0625 0.025", "hitAt 0.03 0", "hitAt 0.2 0.2", "hitAt 0.0125 0.03",
				"hitAt 0.0 0.05", "hitAt -0.01 -0.01", "hitAt 0.0625 0.0275", "hitAt 0.025 0.05"));
		edits("body tube table", onBodyTube(freeform()), 3, steps("table 1 0 3", "table 1 1 45 mm",
				"table 1 0 abc", "table 1 1 ", "table 0 0 1", "table 3 0 6", "table 2 1 -1", "table 2 0 0.5",
				"table 1 0 9", "table 3 1 2", "table 0 1 1", "table 9 0 1", "table 1 1 1 in", "table 2 1 300"));
		edits("body tube insert and delete", onBodyTube(freeform()), 3, steps("select -1", "select 0",
				"insert 0", "insert 1", "select 5", "insert 5", "delete 5", "delete 0", "delete 3", "delete 1",
				"select 2"));
		edits("body tube drags", onBodyTube(freeform()), 3, steps("press 1", "drag 0.03 0.06", "drag 0.035 0.065",
				"release", "press 2", "drag 0.08 0.052 shift", "drag 0.09 0.09 shift", "drag 0.0355 0.09 shift",
				"drag 0.06 0.075 shift", "release", "press 1", "drag 0.04 0.07 ctrl shift",
				"drag 0.06 0.068 ctrl shift", "release", "press 0", "drag 0.01 0.01 shift", "drag -0.02 0.0",
				"release", "press 3", "drag 0.09 0.02 shift", "drag 0.1 0.0", "release"));
		edits("body tube segments", onBodyTube(freeform()), 3, steps("pressAt 0.05 0.05", "drag 0.05 0.07",
				"release", "pressAt 0.0125 0.025", "release", "pressAt 0.2 0.2", "drag 0.21 0.21", "release",
				"pressAt 0.0125 0.03", "drag 0.0 0.06", "release"));
		edits("body tube clicks", onBodyTube(freeform()), 3, steps("click 2 ctrl", "click 0 ctrl", "click 2 ctrl",
				"click 1 ctrl", "click 1 button3", "click 0 button3", "click 1", "click 1 shift"));
		edits("body tube refusals", onBodyTube(freeform()), 3, steps("press 1", "drag 0.1 0.01", "drag 0.09 -0.03",
				"release", "press 2", "drag 0.0 0.0", "release", "press 1", "drag 3.0 3.0", "release", "press 2",
				"drag -3.0 0.05", "release", "table 1 0 400", "table 2 1 400"));
		edits("body tube history", onBodyTube(freeform()), 3, steps("undo", "table 1 0 3", "table 1 1 6",
				"press 2", "drag 0.08 0.07", "release", "undo", "undo", "redo", "undo", "undo", "undo", "redo",
				"table 2 0 8", "redo", "press 1", "release", "undo", "click 1 ctrl", "undo", "redo", "insert 1",
				"undo", "press 1", "undo", "release"));
		edits("body tube live update", onBodyTube(freeform()), 3, steps("live true", "press 1", "drag 0.03 0.06",
				"release", "live false", "press 1", "drag 0.035 0.065", "release", "csv"));
		edits("body tube csv", onBodyTube(freeform(0, 0, 0.0123456, 0.031, 0.08, 0.031, 0.06, 0)), 3,
				steps("csv", "units mm", "csv", "units in", "csv", "units cm"));
		List<String> many = new ArrayList<>();
		for (int i = 0; i < 55; i++) {
			many.add("table 1 1 " + (4.0 + 0.05 * i));
		}
		for (int i = 0; i < 52; i++) {
			many.add("undo");
		}
		many.add("redo");
		many.add("redo");
		edits("body tube fifty", onBodyTube(freeform()), 3, many);

		FreeformFinSet middle = freeform();
		middle.setAxialMethod(AxialMethod.MIDDLE);
		middle.setAxialOffset(0.02);
		edits("body tube middle", onBodyTube(middle), 3, steps("table 0 0 1", "press 0", "drag 0.02 0.0",
				"release", "press 3", "drag 0.07 0.0", "release", "csv"));
		FreeformFinSet bottom = freeform(0, 0, 0.02, 0.04, 0.05, 0.03, 0.04, 0);
		bottom.setAxialMethod(AxialMethod.BOTTOM);
		bottom.setAxialOffset(0);
		edits("transition", onTransition(bottom), 3, steps("hitAt 0.01 0.02", "table 1 1 1", "table 3 0 4",
				"press 2", "drag 0.05 0.0", "release", "press 1", "drag 0.01 -0.03", "release", "csv"));
		FreeformFinSet onNose = freeform(0, 0, 0.01, 0.03, 0.04, 0.03, 0.05, 0);
		onNose.setAxialMethod(AxialMethod.BOTTOM);
		onNose.setAxialOffset(0);
		edits("nose cone", onNoseCone(onNose), 3, steps("table 0 0 -1", "table 1 1 0", "press 1",
				"drag 0.02 0.04 shift", "release", "csv"));

		// Part C: a second freeform fin set selected (Java: its config listener).
		multi("four and five", onBodyTube(freeform(), freeform(0, 0, 0.115, 0.072, 0.255, 0.072, 0.255, 0.037,
				0.150, 0)), 3, 4, steps("table 1 0 3", "table 3 0 6", "table 2 1 4", "select 2", "insert 2",
				"select 1", "delete 1", "press 1", "drag 0.03 0.06", "release", "pressAt 0.05 0.05", "release",
				"click 2 ctrl", "undo", "undo", "redo", "table 0 0 1"));
		multi("five and four", onBodyTube(freeform(0, 0, 0.02, 0.05, 0.06, 0.05, 0.07, 0.02, 0.05, 0), freeform()),
				3, 4, steps("table 3 0 8", "table 3 1 3", "table 4 0 6", "select 3", "insert 3", "select 3",
						"delete 3", "click 4 ctrl", "press 3", "drag 0.07 0.03", "release", "undo"));
	}

	static FreeformFinSetConfig current;
	static OpenRocketDocument currentDoc;

	static void edits(String design, OpenRocketDocument doc, int index, List<String> steps) throws Exception {
		edt(() -> {
			Rocket rocket = doc.getRocket();
			settle(rocket);
			RocketComponent c = preorder(rocket).get(index);
			out.println("== panel | built | " + design + " | " + index + " | " + c.getClass().getSimpleName());
			JDialog dialog = new JDialog();
			current = new FreeformFinSetConfig(doc, c, dialog);
			currentDoc = doc;
			fit(current);
			List<String> lines = new ArrayList<>();
			shapeLines(current, lines);
			figureLines(current, lines);
			print(lines);
			stateLines(c, null);
		});
		for (String step : steps) {
			edt(() -> {
				out.println("== edit | built | " + design + " | " + step);
				apply(current, step);
			});
			flush();
			edt(() -> stateLines(preorder(doc.getRocket()).get(index), null));
		}
		edt(() -> {
			prefs().setUpdateRocketWhileDraggingPoint(false);
			current.invalidateMe();
		});
	}

	static void multi(String design, OpenRocketDocument doc, int mainIndex, int otherIndex, List<String> steps)
			throws Exception {
		edt(() -> {
			Rocket rocket = doc.getRocket();
			settle(rocket);
			List<RocketComponent> comps = preorder(rocket);
			RocketComponent main = comps.get(mainIndex);
			RocketComponent other = comps.get(otherIndex);
			out.println("== multi | built | " + design + " | " + mainIndex + " | " + otherIndex + " | "
					+ main.getClass().getSimpleName());
			main.addConfigListener(other);
			JDialog dialog = new JDialog();
			current = new FreeformFinSetConfig(doc, main, dialog);
			currentDoc = doc;
			fit(current);
			stateLines(main, other);
		});
		for (String step : steps) {
			edt(() -> {
				out.println("== multi edit | built | " + design + " | " + step);
				apply(current, step);
			});
			flush();
			edt(() -> {
				List<RocketComponent> comps = preorder(doc.getRocket());
				stateLines(comps.get(mainIndex), comps.get(otherIndex));
			});
		}
		edt(() -> {
			current.invalidateMe();
			preorder(doc.getRocket()).get(mainIndex).clearConfigListeners();
		});
	}

	/** One step (see the class comment). */
	static void apply(FreeformFinSetConfig panel, String step) throws Exception {
		String[] w = step.split(" ", -1);
		FreeformFinSet fins = (FreeformFinSet) get(panel, RocketComponentConfig.class, "component");
		ScaleScrollPane pane = pane(panel);
		JTable table = table(panel);
		switch (w[0]) {
			case "hit", "hitAt" -> {
				Point p = w[0].equals("hit") ? pointPixel(panel, fins, Integer.parseInt(w[1]), Integer.parseInt(w[2]),
						Integer.parseInt(w[3])) : modelPixel(panel, Double.parseDouble(w[1]), Double.parseDouble(w[2]));
				FinPointFigure figure = figure(panel);
				out.println("hit index=" + figure.getIndexByPoint(p.x, p.y) + " segment="
						+ figure.getSegmentByPoint(p.x, p.y));
			}
			case "table" -> {
				TableModel model = (TableModel) get(panel, FreeformFinSetConfig.class, "tableModel");
				String text = step.substring(("table " + w[1] + " " + w[2] + " ").length());
				model.setValueAt(text, Integer.parseInt(w[1]), Integer.parseInt(w[2]));
			}
			case "select" -> {
				int row = Integer.parseInt(w[1]);
				if (row < 0) {
					table.clearSelection();
				} else {
					table.setRowSelectionInterval(row, row);
				}
			}
			case "insert", "delete" -> {
				int row = Integer.parseInt(w[1]);
				table.setRowSelectionInterval(row, row);
				AbstractAction action = (AbstractAction) get(panel, FreeformFinSetConfig.class,
						w[0].equals("insert") ? "insertFinPointAction" : "deleteFinPointAction");
				out.println("action " + w[0] + " enabled=" + action.isEnabled());
				if (action.isEnabled()) {
					action.actionPerformed(null);
				}
			}
			case "press", "pressAt" -> {
				int mods = MouseEvent.BUTTON1_DOWN_MASK;
				Point p = w[0].equals("press")
						? pointPixel(panel, fins, Integer.parseInt(w[1]), w.length > 3 ? Integer.parseInt(w[2]) : 0,
								w.length > 3 ? Integer.parseInt(w[3]) : 0)
						: modelPixel(panel, Double.parseDouble(w[1]), Double.parseDouble(w[2]));
				pane.mousePressed(event(pane, MouseEvent.MOUSE_PRESSED, p, mods, MouseEvent.BUTTON1, 1));
			}
			case "drag" -> {
				int mods = MouseEvent.BUTTON1_DOWN_MASK | modifiers(w, 3);
				Point p = modelPixel(panel, Double.parseDouble(w[1]), Double.parseDouble(w[2]));
				pane.mouseDragged(event(pane, MouseEvent.MOUSE_DRAGGED, p, mods, MouseEvent.NOBUTTON, 0));
			}
			case "release" -> {
				Point last = lastPixel == null ? new Point(0, 0) : lastPixel;
				pane.mouseReleased(event(pane, MouseEvent.MOUSE_RELEASED, last, 0, MouseEvent.BUTTON1, 1));
			}
			case "click" -> {
				int mods = modifiers(w, 2);
				int button = step.contains("button3") ? MouseEvent.BUTTON3 : MouseEvent.BUTTON1;
				int downMask = button == MouseEvent.BUTTON3 ? MouseEvent.BUTTON3_DOWN_MASK
						: MouseEvent.BUTTON1_DOWN_MASK;
				Point p = pointPixel(panel, fins, Integer.parseInt(w[1]), 0, 0);
				pane.mousePressed(event(pane, MouseEvent.MOUSE_PRESSED, p, mods | downMask, button, 1));
				pane.mouseReleased(event(pane, MouseEvent.MOUSE_RELEASED, p, mods, button, 1));
				try {
					pane.mouseClicked(event(pane, MouseEvent.MOUSE_CLICKED, p, mods, button, 1));
				} catch (java.awt.IllegalComponentStateException e) {
					out.println("popup " + e.getClass().getSimpleName());
				}
			}
			case "undo", "redo" -> {
				JButton b = (JButton) get(panel, FreeformFinSetConfig.class,
						w[0].equals("undo") ? "finPointUndoButton" : "finPointRedoButton");
				b.doClick();
			}
			case "live" -> prefs().setUpdateRocketWhileDraggingPoint(Boolean.parseBoolean(w[1]));
			case "units" -> info.openrocket.core.unit.UnitGroup.UNITS_LENGTH.setDefaultUnit(w[1]);
			case "csv" -> {
				Method write = FreeformFinSetConfig.class.getDeclaredMethod("writeCSVFile", JTable.class, File.class);
				write.setAccessible(true);
				File file = File.createTempFile("probe-fin", ".csv");
				write.invoke(null, table, file);
				String text = Files.readString(file.toPath(), StandardCharsets.UTF_8);
				file.delete();
				out.println("csv " + q(text.replace("\\", "\\\\").replace("\r", "\\r").replace("\n", "\\n")));
			}
			default -> throw new IllegalArgumentException(step);
		}
	}

	static int modifiers(String[] w, int from) {
		int mods = 0;
		for (int i = from; i < w.length; i++) {
			if (w[i].equals("shift")) {
				mods |= MouseEvent.SHIFT_DOWN_MASK;
			} else if (w[i].equals("ctrl")) {
				mods |= MouseEvent.CTRL_DOWN_MASK;
			}
		}
		return mods;
	}

	static Point lastPixel;

	/** A mouse event of the viewport at the figure's pixel p. */
	static MouseEvent event(ScaleScrollPane pane, int id, Point p, int mods, int button, int clicks) {
		JViewport viewport = pane.getViewport();
		Point view = viewport.getViewPosition();
		lastPixel = p;
		return new MouseEvent(viewport, id, 0, mods, p.x - view.x, p.y - view.y, clicks, false, button);
	}

	/** The figure's pixel of the model point (x, y): origin + Math.round(x * scale), origin - Math.round(y * scale). */
	static Point modelPixel(FreeformFinSetConfig panel, double x, double y) throws Exception {
		FinPointFigure figure = figure(panel);
		Point origin = (Point) get(figure, AbstractScaleFigure.class, "originLocation_px");
		double scale = figure.getAbsoluteScale();
		return new Point(origin.x + (int) Math.round(x * scale), origin.y - (int) Math.round(y * scale));
	}

	/** The figure's pixel of the fin point i (getFinPoints()) plus (dx, dy) pixels. */
	static Point pointPixel(FreeformFinSetConfig panel, FreeformFinSet fins, int i, int dx, int dy) throws Exception {
		CoordinateIF c = fins.getFinPoints()[i];
		Point p = modelPixel(panel, c.getX(), c.getY());
		return new Point(p.x + dx, p.y + dy);
	}

	static void stateLines(RocketComponent main, RocketComponent other) {
		try {
			FreeformFinSetConfig panel = current;
			out.println("points " + points((FreeformFinSet) main));
			if (other != null) {
				out.println("points other " + points((FreeformFinSet) other));
			}
			JTable table = table(panel);
			StringBuilder cells = new StringBuilder();
			for (int row = 0; row < table.getRowCount(); row++) {
				cells.append(" ").append(q(String.valueOf(table.getValueAt(row, 0)))).append(" ")
						.append(q(String.valueOf(table.getValueAt(row, 1))));
			}
			out.println("table rows=" + table.getRowCount() + " selected=" + table.getSelectedRow() + " cells"
					+ cells);
			FinPointFigure figure = figure(panel);
			out.println("figure selected=" + get(figure, FinPointFigure.class, "selectedIndex") + " highlight="
					+ get(figure, FinPointFigure.class, "highlightIndex") + " drag="
					+ get(panel, FreeformFinSetConfig.class, "dragIndex") + " scale=" + d(figure.getAbsoluteScale())
					+ " originX=" + ((Point) get(figure, AbstractScaleFigure.class, "originLocation_px")).x);
			Object history = get(figure, FinPointFigure.class, "editHistory");
			Class<?> hc = history.getClass();
			List<?> list = (List<?>) get(history, hc, "history");
			out.println("history size=" + list.size() + " position=" + get(history, hc, "position") + " editing="
					+ get(history, hc, "editInProgress") + " canUndo=" + figure.canUndo() + " canRedo="
					+ figure.canRedo());
			JButton undo = (JButton) get(panel, FreeformFinSetConfig.class, "finPointUndoButton");
			JButton redo = (JButton) get(panel, FreeformFinSetConfig.class, "finPointRedoButton");
			AbstractAction insert = (AbstractAction) get(panel, FreeformFinSetConfig.class, "insertFinPointAction");
			AbstractAction delete = (AbstractAction) get(panel, FreeformFinSetConfig.class, "deleteFinPointAction");
			out.println("buttons undo=" + undo.isEnabled() + " redo=" + redo.isEnabled() + " insert="
					+ insert.isEnabled() + " delete=" + delete.isEnabled());
			Rocket rocket = main.getRocket();
			out.println("rocket frozen=" + (get(rocket, Rocket.class, "freezeList") != null) + " offset="
					+ d(main.getAxialOffset()) + " length=" + d(main.getLength()));
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	static String points(FreeformFinSet fins) {
		StringBuilder s = new StringBuilder();
		for (CoordinateIF c : fins.getFinPoints()) {
			if (s.length() > 0) {
				s.append(' ');
			}
			s.append('(').append(d(c.getX())).append(',').append(d(c.getY())).append(')');
		}
		return s.toString();
	}

	// ----------------------------------------------------------------------------- reflection

	static ScaleScrollPane pane(FreeformFinSetConfig panel) throws Exception {
		return (ScaleScrollPane) get(panel, FreeformFinSetConfig.class, "figurePane");
	}

	static FinPointFigure figure(FreeformFinSetConfig panel) throws Exception {
		return (FinPointFigure) get(panel, FreeformFinSetConfig.class, "figure");
	}

	static JTable table(FreeformFinSetConfig panel) throws Exception {
		return (JTable) get(panel, FreeformFinSetConfig.class, "table");
	}

	// ----------------------------------------------------------------------------- FinPanelProbe's walk

	static String buttonLine(JButton b) {
		return "button " + q(b.getText()) + " enabled=" + b.isEnabled() + " tip=" + q(b.getToolTipText());
	}

	static JTabbedPane tabs(Container root) {
		for (Component c : root.getComponents()) {
			if (c instanceof JTabbedPane t) {
				return t;
			}
			if (c instanceof Container k) {
				JTabbedPane t = tabs(k);
				if (t != null) {
					return t;
				}
			}
		}
		return null;
	}

	static void walk(Container root, List<String> lines, boolean withLabels) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof MaterialPanel || c instanceof GroupableAndSearchableComboBox) {
				continue;
			}
			if (c instanceof JSpinner s) {
				String text = ((JSpinner.DefaultEditor) s.getEditor()).getTextField().getText();
				lines.add("spinner " + modelName(outer(s.getModel())) + " " + q(text) + " enabled=" + s.isEnabled());
				continue;
			}
			if (c instanceof UnitSelector u) {
				DoubleModel m = (DoubleModel) get(u, UnitSelector.class, "model");
				lines.add("unit " + valueName(m) + " " + q(u.getText()));
				continue;
			}
			if (c instanceof JSlider s) {
				DoubleModel m = (DoubleModel) outer(s.getModel());
				lines.add("slider " + valueName(m) + " " + s.getValue() + " enabled=" + s.isEnabled());
				continue;
			}
			if (c instanceof JCheckBox b) {
				lines.add("check " + q(b.getText()) + " checked=" + b.isSelected());
				continue;
			}
			if (c instanceof JComboBox<?> box) {
				lines.add("combo " + q(String.valueOf(box.getSelectedItem())) + " enabled=" + box.isEnabled());
				continue;
			}
			if (c instanceof JButton b) {
				lines.add(buttonLine(b));
				continue;
			}
			if (c instanceof JLabel l) {
				if (withLabels && l.getText() != null && !l.getText().isEmpty()) {
					lines.add("label " + q(l.getText()) + " enabled=" + l.isEnabled() + " tip=" + q(l.getToolTipText()));
				}
				continue;
			}
			if (c instanceof Container k) {
				walk(k, lines, withLabels);
			}
		}
	}

	static JButton button(Container root, String text) {
		for (Component c : root.getComponents()) {
			if (c instanceof JButton b && text.equals(b.getText())) {
				return b;
			}
			if (c instanceof Container k) {
				JButton found = button(k, text);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	static String modelName(Object model) throws Exception {
		if (model instanceof IntegerModel im) {
			return (String) get(im, IntegerModel.class, "valueName");
		}
		return valueName((DoubleModel) model);
	}

	static String valueName(DoubleModel m) throws Exception {
		return (String) get(m, DoubleModel.class, "valueName");
	}

	static Object outer(Object inner) throws Exception {
		Field f = inner.getClass().getDeclaredField("this$0");
		f.setAccessible(true);
		return f.get(inner);
	}

	static Object get(Object o, Class<?> c, String name) throws Exception {
		Field f = c.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(o);
	}

	static String q(String s) {
		return s == null ? "null" : "\"" + s + "\"";
	}

	static String d(double value) {
		return Double.toString(value);
	}

	static void print(List<String> lines) {
		Collections.sort(lines);
		for (String line : lines) {
			out.println(line);
		}
	}

	// ----------------------------------------------------------------------------- the EDT

	static void edt(ThrowingRunnable r) throws Exception {
		SwingUtilities.invokeAndWait(() -> {
			try {
				r.run();
			} catch (Exception ex) {
				throw new RuntimeException(ex);
			}
		});
	}

	interface ThrowingRunnable {
		void run() throws Exception;
	}

	/** Lets the EDT process what is queued (the component and focus events). */
	static void flush() throws Exception {
		for (int i = 0; i < 3; i++) {
			edt(() -> {
			});
		}
	}

	// ----------------------------------------------------------------------------- the design

	static List<RocketComponent> preorder(Rocket rocket) {
		List<RocketComponent> comps = new ArrayList<>();
		for (RocketComponent c : rocket) {
			comps.add(c);
		}
		return comps;
	}

	/** The golden harness's AutomaticDimensions.settle() (tools/openrocket-goldens). */
	static void settle(Rocket rocket) {
		List<RocketComponent> comps = preorder(rocket);
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

	static void bootstrap(String motorsDatabase) throws Exception {
		Path bundledDb = Path.of(motorsDatabase);
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
				bind(ApplicationPreferences.class).to(info.openrocket.swing.gui.util.SwingPreferences.class);
				bind(ComponentPresetDao.class).toInstance(presets);
				bind(info.openrocket.swing.gui.watcher.WatchService.class)
						.to(info.openrocket.swing.gui.watcher.WatchServiceImpl.class);
				bind(ThrustCurveMotorSetDatabase.class).toInstance(motors);
				bind(MotorDatabase.class).to(ThrustCurveMotorSetDatabase.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(dbOverrides),
				new PluginModule()));
	}
}
