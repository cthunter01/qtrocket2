package probe;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;
import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.aerodynamics.BarrowmanCalculator;
import info.openrocket.core.aerodynamics.FlightConditions;
import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.database.motor.MotorDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSQLiteDatabase;
import info.openrocket.core.database.motor.ThrustCurveMotorSetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.masscalc.MassCalculator;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.LineStyle;
import info.openrocket.core.util.MathUtil;
import info.openrocket.core.util.ORColor;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.figureelements.CGCaret;
import info.openrocket.swing.gui.figureelements.CPCaret;
import info.openrocket.swing.gui.figureelements.Caret;
import info.openrocket.swing.gui.figureelements.RocketInfo;
import info.openrocket.swing.gui.figureelements.RocketInfoContextHelper;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapes;
import info.openrocket.swing.gui.scalefigure.RocketFigure;
import info.openrocket.swing.gui.scalefigure.RocketPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Graphics2D;
import java.awt.Shape;
import java.awt.Stroke;
import java.awt.geom.AffineTransform;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.List;
import java.util.PriorityQueue;

/**
 * What QtRocket's part G2 (RocketFigure, RocketFigurePainter, the carets) must reproduce: OpenRocket's
 * RocketFigure, painted headless (SwingPreferences, FlatLaf Light) as RocketPanel.create2DPreviewFigure does,
 * into a Graphics2D that records every draw(Shape) and fill(Shape) with its colour, stroke and the device
 * bounds of the shape; the carets placed as RocketPanel.updateExtras places them; and
 * getComponentsByPoint() at points of the painted figure.
 * usage: FigurePaintProbe EXAMPLEDIR
 */
public class FigurePaintProbe {
	/** One recorded draw or fill. */
	static final class Op {
		String kind;
		Color color;
		Stroke stroke;
		AffineTransform transform;
		Shape shape;
		Rectangle2D device;
	}

	static final class Recorder extends DelegatingGraphics2D {
		final List<Op> ops = new ArrayList<>();

		Recorder(Graphics2D g) {
			super(g);
		}

		void record(String kind, Shape s) {
			Op op = new Op();
			op.kind = kind;
			op.color = g.getColor();
			op.stroke = g.getStroke();
			op.transform = g.getTransform();
			op.shape = s;
			op.device = op.transform.createTransformedShape(s).getBounds2D();
			ops.add(op);
		}

		@Override
		public void draw(Shape s) {
			record("draw", s);
			g.draw(s);
		}

		@Override
		public void fill(Shape s) {
			record("fill", s);
			g.fill(s);
		}
	}

	static SwingPreferences prefs;

	public static void main(String[] args) throws Exception {
		bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		prefs = (SwingPreferences) Application.getPreferences();
		File dir = new File(args[0]);

		scenario("simple-side", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("simple-side-selected", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[] {3}, false, null);
		scenario("simple-back", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.BackView, 0.0, new int[0], false, null);
		scenario("simple-top30", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.TopView, Math.PI / 6, new int[0], false, null);
		scenario("simple-side30", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.SideView, Math.PI / 6, new int[0], false, null);
		scenario("simple-styled", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, "styled");
		scenario("simple-hidden-body", load(dir, "A simple model rocket.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, "hide3");
		scenario("pods-side", load(dir, "Pods--airframes and winglets.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("pods-back", load(dir, "Pods--airframes and winglets.ork"), RocketPanel.VIEW_TYPE.BackView, 0.0, new int[0], false, null);
		scenario("pods-side-markers", load(dir, "Pods--airframes and winglets.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], true, null);
		scenario("cluster-back", load(dir, "Clustered motors.ork"), RocketPanel.VIEW_TYPE.BackView, 0.0, new int[0], false, null);
		scenario("cluster-side", load(dir, "Clustered motors.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("boosters-side", load(dir, "Parallel booster staging.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("boosters-back30", load(dir, "Parallel booster staging.ork"), RocketPanel.VIEW_TYPE.BackView, Math.PI / 6, new int[0], false, null);
		scenario("dual-side", load(dir, "Dual parachute deployment.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("twostage-side", load(dir, "Two stage high power rocket.ork"), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("tubefin-back", load(dir, "Tube fin rocket.ork"), RocketPanel.VIEW_TYPE.BackView, 0.0, new int[0], false, null);
		Rocket beta = TestRockets.makeBeta();
		settle(beta);
		scenario("beta-side", beta, RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("new-side", OpenRocketDocumentFactory.createNewRocket().getRocket(), RocketPanel.VIEW_TYPE.SideView, 0.0, new int[0], false, null);
		scenario("new-back", OpenRocketDocumentFactory.createNewRocket().getRocket(), RocketPanel.VIEW_TYPE.BackView, 0.0, new int[0], false, null);
	}

	static Rocket load(File dir, String name) throws Exception {
		OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
		Rocket rocket = doc.getRocket();
		settle(rocket);
		return rocket;
	}

	static List<RocketComponent> treeOrder(Rocket rocket) {
		List<RocketComponent> order = new ArrayList<>();
		for (RocketComponent c : rocket) {
			order.add(c);
		}
		return order;
	}

	static String hex(double v) {
		return Double.toHexString(v);
	}

	static String color(Color c) {
		return c.getRed() + "," + c.getGreen() + "," + c.getBlue() + "," + c.getAlpha();
	}

	static String rect(Rectangle2D r) {
		return hex(r.getX()) + " " + hex(r.getY()) + " " + hex(r.getWidth()) + " " + hex(r.getHeight());
	}

	/** The stroke in device pixels: width and dashes times the transform's scale. */
	static String stroke(Stroke s, AffineTransform t) {
		if (!(s instanceof BasicStroke b)) {
			return "stroke?";
		}
		double scale = Math.sqrt(Math.abs(t.getDeterminant()));
		StringBuilder sb = new StringBuilder();
		sb.append("width=").append(String.format("%.6f", b.getLineWidth() * scale));
		sb.append(" cap=").append(b.getEndCap()).append(" join=").append(b.getLineJoin());
		sb.append(" dashes=");
		float[] d = b.getDashArray();
		if (d == null) {
			sb.append("none");
		} else {
			for (int i = 0; i < d.length; i++) {
				if (i > 0) {
					sb.append(",");
				}
				sb.append(String.format("%.6f", d[i] * scale));
			}
		}
		return sb.toString();
	}

	@SuppressWarnings("unchecked")
	static PriorityQueue<RocketComponentShapes> queue(RocketFigure fig, RocketPanel.VIEW_TYPE view) throws Exception {
		Field f = RocketFigure.class.getDeclaredField(
				view == RocketPanel.VIEW_TYPE.BackView ? "figureShapes_back" : "figureShapes_side");
		f.setAccessible(true);
		return (PriorityQueue<RocketComponentShapes>) f.get(fig);
	}

	static void scenario(String name, Rocket rocket, RocketPanel.VIEW_TYPE view, double rotation, int[] selected,
			boolean showMarkers, String modification) throws Exception {
		prefs.setShowMarkers(showMarkers);
		List<RocketComponent> order = treeOrder(rocket);
		if ("styled".equals(modification)) {
			// The body tube in a colour and dotted, the fins dash-dotted, the nose cone invisible (alpha 0).
			order.get(3).setColor(new ORColor(10, 20, 30, 255));
			order.get(3).setLineStyle(LineStyle.DOTTED);
			order.get(4).setLineStyle(LineStyle.DASHDOT);
			order.get(2).setColor(new ORColor(200, 100, 50, 0));
		}
		if ("hide3".equals(modification)) {
			order.get(3).setVisible(false);
		}
		RocketComponent[] selection = new RocketComponent[selected.length];
		for (int i = 0; i < selected.length; i++) {
			selection[i] = order.get(selected[i]);
		}

		RocketFigure fig = new RocketFigure(rocket);
		fig.setType(view);
		fig.setRotation(rotation);
		fig.setSelection(selection);
		fig.setDrawCarets(true);
		Caret cg = new CGCaret(0, 0);
		Caret cp = new CPCaret(0, 0);
		FlightConfiguration config = rocket.getSelectedConfiguration();
		RocketInfo info = new RocketInfo(config);
		FlightConditions conditions = new FlightConditions(config);
		double mach = Application.getPreferences().getDefaultMach();
		conditions.setMach(mach);
		info.setMach(mach);
		conditions.setAOA(0);
		info.setAOA(Double.NaN);
		conditions.setRollRate(0);
		info.setTheta(Double.NaN);
		RocketInfoContextHelper.RocketPhysics physics = RocketInfoContextHelper.computePhysics(config, conditions,
				new WarningSet(), new BarrowmanCalculator(), true, info);
		double rot = fig.getRotation(true);
		CoordinateIF cpc = physics.cp();
		CoordinateIF cgc = physics.cg();
		double cpx = Double.NaN, cpy = Double.NaN, cgx = Double.NaN, cgy = Double.NaN;
		if (cpc.getWeight() > MathUtil.EPSILON) {
			cpx = cpc.getX();
			cpy = cpc.getY() * Math.cos(rot) + cpc.getZ() * Math.sin(rot);
		}
		if (cgc.getWeight() > MassCalculator.MIN_MASS) {
			cgx = cgc.getX();
			cgy = cgc.getY() * Math.cos(rot) + cgc.getZ() * Math.sin(rot);
		}
		if (view == RocketPanel.VIEW_TYPE.TopView) {
			cgy = -cgy;
		}
		if (physics.length() > 0 && view != RocketPanel.VIEW_TYPE.BackView) {
			cp.setPosition(cpx, cpy);
			cg.setPosition(cgx, cgy);
		} else {
			cp.setPosition(Double.NaN, Double.NaN);
			cg.setPosition(Double.NaN, Double.NaN);
		}
		fig.addRelativeExtra(cp);
		fig.addRelativeExtra(cg);

		Dimension renderBounds = new Dimension(1000, 700);
		fig.scaleTo(renderBounds);
		fig.updateFigure();
		Dimension canvasSize = fig.getPreferredSize();
		if (canvasSize.width <= 0 || canvasSize.height <= 0) {
			canvasSize = renderBounds;
		}
		fig.setSize(canvasSize);
		fig.doLayout();

		BufferedImage raw = new BufferedImage(canvasSize.width, canvasSize.height, BufferedImage.TYPE_INT_ARGB);
		Graphics2D g2 = raw.createGraphics();
		Recorder rec = new Recorder(g2);
		try {
			fig.paintComponent(rec);
		} finally {
			g2.dispose();
		}

		System.out.println("S " + name + " view=" + view + " rotation=" + hex(rotation) + " markers=" + showMarkers
				+ " modification=" + modification);
		StringBuilder sel = new StringBuilder();
		for (int s : selected) {
			sel.append(" ").append(s);
		}
		System.out.println("Z selection" + sel);
		for (int i = 0; i < order.size(); i++) {
			RocketComponent c = order.get(i);
			System.out.println("C " + i + " " + c.getClass().getSimpleName() + " [" + c.getName() + "] visible="
					+ c.isVisible());
		}
		System.out.println("F user=" + hex(fig.getUserScale()) + " scale=" + hex(fig.getAbsoluteScale()) + " origin="
				+ fig.getSubjectOrigin().x + "," + fig.getSubjectOrigin().y + " size=" + canvasSize.width + ","
				+ canvasSize.height);
		System.out.println("K cg=" + hex(cg.getX()) + "," + hex(cg.getY()) + " cp=" + hex(cp.getX()) + ","
				+ hex(cp.getY()));

		// Pair the shape draws with the queue's poll order (the paint loop polls a copy of the same queue).
		PriorityQueue<RocketComponentShapes> copy = new PriorityQueue<>(queue(fig, view));
		List<RocketComponentShapes> drawnShapes = new ArrayList<>();
		List<RocketComponentShapes> allShapes = new ArrayList<>();
		while (!copy.isEmpty()) {
			RocketComponentShapes rcs = copy.poll();
			allShapes.add(rcs);
			if (rcs.getComponent().isVisible()) {
				drawnShapes.add(rcs);
			}
		}
		int opIndex = 0;
		for (RocketComponentShapes rcs : drawnShapes) {
			Op op = rec.ops.get(opIndex++);
			if (!op.kind.equals("draw") || op.shape != rcs.shape) {
				throw new IllegalStateException("draw " + opIndex + " is not the queue's shape");
			}
			RocketComponent c = rcs.getComponent();
			int displayOrder = view == RocketPanel.VIEW_TYPE.BackView ? c.getDisplayOrder_back() : c.getDisplayOrder_side();
			System.out.println("P shape comp=" + treeIndex(order(rocket), c) + " order=" + displayOrder + " color="
					+ color(op.color) + " " + stroke(op.stroke, op.transform) + " device=" + rect(op.device));
		}
		for (; opIndex < rec.ops.size(); opIndex++) {
			Op op = rec.ops.get(opIndex);
			String what = op.kind.equals("fill") && op.color.getAlpha() == 100 ? "motor-fill"
					: op.kind.equals("draw") ? "motor-draw" : "caret";
			System.out.println("P " + what + " color=" + color(op.color) + " " + stroke(op.stroke, op.transform)
					+ " device=" + rect(op.device));
			if (what.equals("caret")) {
				// The caret's area in device pixels, sampled about its centre.
				Shape device = op.transform.createTransformedShape(op.shape);
				double cx = op.device.getCenterX();
				double cy = op.device.getCenterY();
				StringBuilder samples = new StringBuilder();
				double[][] offsets = {{3, 3}, {-3, -3}, {3, -3}, {-3, 3}, {6.6, 0}, {0, 6.6}, {7.5, 0}, {5.8, 0},
						{0, 0.5}, {4.5, 0}};
				for (double[] o : offsets) {
					samples.append(device.contains(cx + o[0], cy + o[1]) ? "1" : "0");
				}
				System.out.println("Q centre=" + hex(cx) + "," + hex(cy) + " samples=" + samples);
			}
		}

		// Hit tests at the centre of every drawn and undrawn shape's device bounds, and two empty points.
		List<int[]> points = new ArrayList<>();
		points.add(new int[] {2, 2});
		points.add(new int[] {canvasSize.width - 3, canvasSize.height - 3});
		for (RocketComponentShapes rcs : allShapes) {
			AffineTransform t = new AffineTransform();
			Rectangle2D d = projectionOf(fig).createTransformedShape(rcs.shape).getBounds2D();
			int[] p = {(int) Math.floor(d.getCenterX()), (int) Math.floor(d.getCenterY())};
			boolean known = false;
			for (int[] q : points) {
				if (q[0] == p[0] && q[1] == p[1]) {
					known = true;
				}
			}
			if (!known) {
				points.add(p);
			}
		}
		for (int[] p : points) {
			String result = hits(fig, rocket, view, p[0], p[1]);
			// Robust: the same components a hundredth of a pixel away in each diagonal direction (no shape
			// boundary passes through the point, so the last bits of sin and cos cannot change the result).
			boolean robust = true;
			double[][] offsets = {{0.01, 0.01}, {-0.01, 0.01}, {0.01, -0.01}, {-0.01, -0.01}};
			for (double[] o : offsets) {
				if (!sortedHits(fig, rocket, p[0] + o[0], p[1] + o[1]).equals(sortedHits(fig, rocket, p[0], p[1]))) {
					robust = false;
				}
			}
			System.out.println("H " + p[0] + "," + p[1] + " robust=" + robust + " ->" + result);
		}
		System.out.println("E");
	}

	static String hits(RocketFigure fig, Rocket rocket, RocketPanel.VIEW_TYPE view, double x, double y) {
		RocketComponent[] hit = fig.getComponentsByPoint(x, y);
		StringBuilder sb = new StringBuilder();
		for (RocketComponent c : hit) {
			int displayOrder = view == RocketPanel.VIEW_TYPE.BackView ? c.getDisplayOrder_back()
					: c.getDisplayOrder_side();
			sb.append(" ").append(treeIndex(order(rocket), c)).append(":").append(displayOrder);
		}
		return sb.toString();
	}

	static String sortedHits(RocketFigure fig, Rocket rocket, double x, double y) {
		List<Integer> indices = new ArrayList<>();
		for (RocketComponent c : fig.getComponentsByPoint(x, y)) {
			indices.add(treeIndex(order(rocket), c));
		}
		java.util.Collections.sort(indices);
		return indices.toString();
	}

	static List<RocketComponent> order(Rocket rocket) {
		return treeOrder(rocket);
	}

	static int treeIndex(List<RocketComponent> order, RocketComponent c) {
		for (int i = 0; i < order.size(); i++) {
			if (order.get(i) == c) {
				return i;
			}
		}
		return -1;
	}

	static AffineTransform projectionOf(RocketFigure fig) throws Exception {
		Field f = info.openrocket.swing.gui.scalefigure.AbstractScaleFigure.class.getDeclaredField("projection");
		f.setAccessible(true);
		return (AffineTransform) f.get(fig);
	}

	static void bootstrap() throws Exception {
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(
				Modules.override(new ServicesForTesting()).with(swingPrefs), new PluginModule()));
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

	// ------------------------------------------------------------ settling (tools/openrocket-goldens)

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
