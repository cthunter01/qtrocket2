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
import info.openrocket.core.logging.Warning;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.masscalc.MassCalculator;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.ComponentChangeEvent;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.MathUtil;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.figureelements.RocketInfo;
import info.openrocket.swing.gui.figureelements.RocketInfoContextHelper;
import info.openrocket.swing.gui.scalefigure.RocketFigure;
import info.openrocket.swing.gui.scalefigure.RocketPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Color;
import java.awt.Font;
import java.awt.Graphics2D;
import java.awt.Rectangle;
import java.awt.Shape;
import java.awt.font.FontRenderContext;
import java.awt.font.GlyphVector;
import java.awt.geom.Rectangle2D;
import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.IdentityHashMap;
import java.util.List;

/**
 * What QtRocket's part G3 (DesignPhysics, RocketInfo) must reproduce: RocketPanel.updateExtras() (the M2 part,
 * no background simulation) through RocketInfoContextHelper.computePhysics(), and RocketInfo painted into a
 * Graphics2D that records every drawGlyphVector (with the text, which a recording Font subclass remembers per
 * GlyphVector) and every caret fill, with OpenRocket's SwingPreferences and FlatLaf Light.
 * usage: RocketInfoProbe EXAMPLEDIR
 *
 * Output (doubles as Double.toHexString):
 *   S name
 *   N length diameter cpx cpy cpz cpw cgx cgy cgz cgw massWith massWithout hasMotors     (computePhysics)
 *   V cg=... cp=... length=... diameter=...                                                (RocketInfo's fields)
 *   X stability|cg|cp texts (getStabilityCombined(), getCg(), getCp())
 *   W warning text                                                                         (each, in order)
 *   K view rotation rot(true) cgx cgy cpx cpy                                              (caret positions)
 *   P showWarnings line x1 x2 y1 y2                                                         (a paint)
 *   T font colour x y visualWidth | text                                                    (each text drawn)
 *   F colour centreX centreY width height                                                   (each fill: carets)
 *   E
 */
public class RocketInfoProbe {
	static SwingPreferences prefs;
	static final IdentityHashMap<GlyphVector, String> TEXTS = new IdentityHashMap<>();

	/** A Font that remembers the text of every GlyphVector it makes. */
	static final class RecordingFont extends Font {
		final String role;

		RecordingFont(Font font, String role) {
			super(font);
			this.role = role;
		}

		@Override
		public GlyphVector createGlyphVector(FontRenderContext frc, String str) {
			GlyphVector vector = super.createGlyphVector(frc, str);
			TEXTS.put(vector, role + "\t" + str);
			return vector;
		}
	}

	static final class Recorder extends DelegatingGraphics2D {
		final List<String> lines = new ArrayList<>();

		Recorder(Graphics2D g) {
			super(g);
		}

		@Override
		public void drawGlyphVector(GlyphVector vector, float x, float y) {
			String text = TEXTS.get(vector);
			String[] parts = text == null ? new String[] {"?", "?"} : text.split("\t", 2);
			Rectangle2D bounds = vector.getVisualBounds();
			lines.add("T " + parts[0] + " " + color(g.getColor()) + " " + hex(x) + " " + hex(y) + " "
					+ hex(bounds.getWidth()) + " | " + parts[1]);
			g.drawGlyphVector(vector, x, y);
		}

		@Override
		public void fill(Shape s) {
			Rectangle2D d = g.getTransform().createTransformedShape(s).getBounds2D();
			lines.add("F " + color(g.getColor()) + " " + hex(d.getCenterX()) + " " + hex(d.getCenterY()) + " "
					+ hex(d.getWidth()) + " " + hex(d.getHeight()));
			g.fill(s);
		}

		@Override
		public void draw(Shape s) {
			lines.add("D " + color(g.getColor()));
			g.draw(s);
		}
	}

	public static void main(String[] args) throws Exception {
		bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		prefs = (SwingPreferences) Application.getPreferences();
		File dir = new File(args[0]);
		System.out.println("FONTSIZE " + prefs.getRocketInfoFontSize() + " ui " + prefs.getUIFontSize() + " mach "
				+ prefs.getDefaultMach() + " secondary " + prefs.isDisplaySecondaryStability());

		String[] examples = new File(dir, ".").list((d, n) -> n.endsWith(".ork"));
		java.util.Arrays.sort(examples);
		for (String name : examples) {
			scenario("example:" + name, load(dir, name), Double.NaN, Double.NaN, Double.NaN, Double.NaN);
		}
		String[] makers = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue", "makeIsoHaisu",
				"makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket", "makeEstesAlphaIIIWithPods",
				"makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor", "makeEstesAlphaIIIwithInlinePod",
				"makeClusterPods"};
		for (String maker : makers) {
			Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
			settle(rocket);
			scenario("testrocket:" + maker, rocket, Double.NaN, Double.NaN, Double.NaN, Double.NaN);
		}

		// RocketInfoContextHelperTest.displayPhysicsIncludesGeometryWarnings's design.
		Rocket alpha = TestRockets.makeEstesAlphaIII();
		NoseCone nose = (NoseCone) alpha.getChild(0).getChild(0);
		BodyTube body = (BodyTube) alpha.getChild(0).getChild(1);
		nose.setAftRadius(body.getOuterRadius() * 1.25);
		settle(alpha);
		scenario("special:alpha-discontinuity", alpha, Double.NaN, Double.NaN, Double.NaN, Double.NaN);

		// RocketInfoContextHelperTest.bodyTubeOnlyUsesSameFiniteCpAndStabilityInBothViews's design, the rocket's
		// own selected configuration.
		Rocket tube = new Rocket();
		AxialStage stage = new AxialStage();
		tube.addChild(stage);
		stage.addChild(new BodyTube(1.0, 0.125));
		tube.enableEvents();
		scenario("special:body-tube-only", tube, Double.NaN, Double.NaN, Double.NaN, Double.NaN);

		scenario("special:new-design", OpenRocketDocumentFactory.createNewRocket().getRocket(), Double.NaN,
				Double.NaN, Double.NaN, Double.NaN);

		// A stage below two inactive ones (QtRocket's TestBoostersOnBoostersRocket, setOnlyStage(3)).
		Rocket nested = TestRockets.makeFalcon9Heavy();
		BodyTube boosterBody = (BodyTube) nested.getChild(1).getChild(0).getChild(0).getChild(1);
		ParallelStage inner = new ParallelStage();
		inner.setName("Inner Boosters");
		inner.setInstanceCount(2);
		boosterBody.addChild(inner);
		inner.addChild(new NoseCone(Transition.Shape.OGIVE, 0.05, 0.015));
		BodyTube innerBody = new BodyTube(0.2, 0.015, 0.001);
		inner.addChild(innerBody);
		innerBody.addChild(new TrapezoidFinSet(3, 0.04, 0.02, 0.01, 0.03));
		settle(nested);
		nested.getSelectedConfiguration().setOnlyStage(3);
		scenario("special:boosters-on-boosters-only-inner", nested, Double.NaN, Double.NaN, Double.NaN, Double.NaN);
		// The same selection made by StageSelector.StageAction: the stage toggled, then the rocket's event.
		nested.getSelectedConfiguration().setAllStages();
		nested.getSelectedConfiguration().setOnlyStage(3);
		nested.fireComponentChangeEvent(ComponentChangeEvent.AEROMASS_CHANGE | ComponentChangeEvent.MOTOR_CHANGE
				| ComponentChangeEvent.TREE_CHANGE_CHILDREN, nested.getSelectedConfiguration().getFlightConfigurationID());
		scenario("special:boosters-on-boosters-toggled", nested, Double.NaN, Double.NaN, Double.NaN, Double.NaN);

		// The CP parameters of the component analysis: Mach, AOA, theta (getCP) and roll.
		scenario("special:simple-cp-parameters", load(dir, "A simple model rocket.ork"), 0.8, Math.toRadians(5),
				Math.toRadians(30), 2.0);
		scenario("special:simple-mach-only", load(dir, "A simple model rocket.ork"), 0.5, Double.NaN, Double.NaN,
				Double.NaN);

		// Imperial units.
		UnitGroup.setDefaultImperialUnits();
		scenario("special:simple-imperial", load(dir, "A simple model rocket.ork"), Double.NaN, Double.NaN,
				Double.NaN, Double.NaN);
		UnitGroup.setDefaultMetricUnits();

		// The stability in percent: the secondary unit (%) equals the primary one.
		UnitGroup.UNITS_STABILITY.setDefaultUnit("%");
		scenario("special:simple-stability-percent", load(dir, "A simple model rocket.ork"), Double.NaN, Double.NaN,
				Double.NaN, Double.NaN);
		UnitGroup.UNITS_STABILITY.setDefaultUnit("cal");

		// The secondary stability switched off.
		prefs.setDisplaySecondaryStability(false);
		scenario("special:simple-no-secondary", load(dir, "A simple model rocket.ork"), Double.NaN, Double.NaN,
				Double.NaN, Double.NaN);
		prefs.setDisplaySecondaryStability(true);

		// The secondary in calibers, the primary in metres (the stability unit group has m, cm, ...?).
		UnitGroup.UNITS_SECONDARY_STABILITY.setDefaultUnit("cal");
		UnitGroup.UNITS_STABILITY.setDefaultUnit("in");
		scenario("special:simple-stability-in-and-cal", load(dir, "A simple model rocket.ork"), Double.NaN,
				Double.NaN, Double.NaN, Double.NaN);
		UnitGroup.UNITS_STABILITY.setDefaultUnit("cal");
		UnitGroup.UNITS_SECONDARY_STABILITY.setDefaultUnit("%");
	}

	static Rocket load(File dir, String name) throws Exception {
		OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
		Rocket rocket = doc.getRocket();
		settle(rocket);
		return rocket;
	}

	static String hex(double v) {
		return Double.toHexString(v);
	}

	static String color(Color c) {
		return c.getRed() + "," + c.getGreen() + "," + c.getBlue() + "," + c.getAlpha();
	}

	static Object field(Object o, String name) throws Exception {
		Field f = o.getClass().getDeclaredField(name);
		f.setAccessible(true);
		return f.get(o);
	}

	static void setField(Object o, String name, Object value) throws Exception {
		Field f = o.getClass().getDeclaredField(name);
		f.setAccessible(true);
		f.set(o, value);
	}

	static String coordinate(CoordinateIF c) {
		return hex(c.getX()) + " " + hex(c.getY()) + " " + hex(c.getZ()) + " " + hex(c.getWeight());
	}

	/**
	 * RocketPanel.updateExtras() for a figure of @p view at @p rotation: the caret positions.
	 */
	static void carets(Rocket rocket, RocketPanel.VIEW_TYPE view, double rotationIn,
			RocketInfoContextHelper.RocketPhysics physics) {
		RocketFigure figure = new RocketFigure(rocket);
		figure.setType(view);
		figure.setRotation(rotationIn);
		final double rotation = figure.getRotation(true);
		CoordinateIF cp = physics.cp();
		CoordinateIF cg = physics.cg();
		double cgx = Double.NaN, cgy = Double.NaN, cpx = Double.NaN, cpy = Double.NaN;
		if (cp.getWeight() > MathUtil.EPSILON) {
			cpx = cp.getX();
			cpy = cp.getY() * Math.cos(rotation) + cp.getZ() * Math.sin(rotation);
		}
		if (cg.getWeight() > MassCalculator.MIN_MASS) {
			cgx = cg.getX();
			cgy = cg.getY() * Math.cos(rotation) + cg.getZ() * Math.sin(rotation);
		}
		if (figure.getCurrentViewType() == RocketPanel.VIEW_TYPE.TopView) {
			cgy = -cgy;
		}
		double length = physics.length();
		if (!(length > 0 && (view == RocketPanel.VIEW_TYPE.TopView || view == RocketPanel.VIEW_TYPE.SideView))) {
			cgx = cgy = cpx = cpy = Double.NaN;
		}
		System.out.println("K " + view + " " + hex(rotationIn) + " " + hex(rotation) + " " + hex(cgx) + " "
				+ hex(cgy) + " " + hex(cpx) + " " + hex(cpy));
	}

	static void scenario(String name, Rocket rocket, double cpMach, double cpAOA, double cpTheta, double cpRoll)
			throws Exception {
		FlightConfiguration config = rocket.getSelectedConfiguration();
		RocketInfo info = new RocketInfo(config);
		WarningSet warnings = new WarningSet();
		BarrowmanCalculator calculator = new BarrowmanCalculator();

		// RocketPanel.updateExtras()
		FlightConditions conditions = new FlightConditions(config);
		info.setCurrentConfig(config);
		if (!Double.isNaN(cpMach)) {
			conditions.setMach(cpMach);
			info.setMach(cpMach);
		} else {
			conditions.setMach(Application.getPreferences().getDefaultMach());
			info.setMach(Application.getPreferences().getDefaultMach());
		}
		if (!Double.isNaN(cpAOA)) {
			conditions.setAOA(cpAOA);
		} else {
			conditions.setAOA(0);
		}
		info.setAOA(cpAOA);
		if (!Double.isNaN(cpRoll)) {
			conditions.setRollRate(cpRoll);
		} else {
			conditions.setRollRate(0);
		}
		if (!Double.isNaN(cpTheta)) {
			conditions.setTheta(cpTheta);
		}
		info.setTheta(cpTheta);
		RocketInfoContextHelper.RocketPhysics physics = RocketInfoContextHelper.computePhysics(config, conditions,
				warnings, calculator, Double.isNaN(cpTheta), info);

		System.out.println("S " + name);
		System.out.println("N " + hex(physics.length()) + " " + hex((Double) field(info, "diameter")) + " "
				+ coordinate(physics.cp()) + " " + coordinate(physics.cg()) + " "
				+ hex((Double) field(info, "massWithMotors")) + " " + hex((Double) field(info, "massWithoutMotors"))
				+ " " + config.hasMotors());
		System.out.println("V cg=" + hex((Double) field(info, "cg")) + " cp=" + hex((Double) field(info, "cp"))
				+ " length=" + hex((Double) field(info, "length")) + " mach=" + hex((Double) field(info, "mach"))
				+ " aoa=" + hex((Double) field(info, "aoa")) + " theta=" + hex((Double) field(info, "theta")));
		System.out.println("X " + info.getStabilityCombined() + "\t" + info.getCg() + "\t" + info.getCp());
		for (Warning w : warnings) {
			System.out.println("W " + w.toString());
		}
		carets(rocket, RocketPanel.VIEW_TYPE.SideView, 0.0, physics);
		carets(rocket, RocketPanel.VIEW_TYPE.SideView, Math.PI / 6, physics);
		carets(rocket, RocketPanel.VIEW_TYPE.TopView, 0.0, physics);
		carets(rocket, RocketPanel.VIEW_TYPE.TopView, Math.PI / 6, physics);
		carets(rocket, RocketPanel.VIEW_TYPE.BackView, 0.0, physics);

		paint(info, true);
		if (!warnings.isEmpty()) {
			paint(info, false);
		}
		System.out.println("E");
	}

	static void paint(RocketInfo info, boolean showWarnings) throws Exception {
		info.setShowWarnings(showWarnings);
		float size = prefs.getRocketInfoFontSize();
		setField(info, "font",
				new RecordingFont(GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_REGULAR, size, 0.0f), "regular"));
		setField(info, "smallFont",
				new RecordingFont(GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_REGULAR, (float) (size - 1.5), 0.0f),
						"small"));
		setField(info, "boldFont",
				new RecordingFont(GUIUtil.createUIFont(GUIUtil.UI_FONT_STYLE_BOLD, (float) (size + 1.5), 0.0f),
						"bold"));
		BufferedImage image = new BufferedImage(1000, 700, BufferedImage.TYPE_INT_ARGB);
		Graphics2D g2 = image.createGraphics();
		Recorder recorder = new Recorder(g2);
		try {
			info.paint(recorder, 1.0, new Rectangle(0, 0, 1000, 700));
		} finally {
			g2.dispose();
		}
		System.out.println("P " + showWarnings + " " + hex((Float) field(info, "line")) + " "
				+ hex((Float) field(info, "x1")) + " " + hex((Float) field(info, "x2")) + " "
				+ hex((Float) field(info, "y1")) + " " + hex((Float) field(info, "y2")));
		for (String line : recorder.lines) {
			System.out.println(line);
		}
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
