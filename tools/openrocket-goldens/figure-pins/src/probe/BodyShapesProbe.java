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
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Bulkhead;
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.EngineBlock;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.InstanceContext;
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TubeCoupler;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.TestRockets;
import info.openrocket.core.util.Transformation;
import info.openrocket.swing.gui.print.PrintUnit;
import info.openrocket.swing.gui.rocketfigure.EmptyShapes;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapeProvider;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapes;
import info.openrocket.swing.gui.rocketfigure.TransitionShapes;

import java.awt.Shape;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Line2D;
import java.awt.geom.Path2D;
import java.awt.geom.PathIterator;
import java.awt.geom.Rectangle2D;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Dumps what OpenRocket's shape providers of QtRocket's part F2 (BodyTubeShapes, TransitionShapes with
 * SymmetricComponentShapes and TubeShapes, RingComponentShapes, LaunchLugShapes, TubeFinSetShapes and
 * EmptyShapes) return, with every double as Double.toHexString (exact):
 * <ul>
 * <li>for every component of those kinds of the 16 examples (loaded with no presets) and the 13 test
 * rockets, both settled as the golden harness settles them, every active instance in five views (side 0,
 * side 30 deg, top 0, back 0, back 30 deg): the transformation RocketFigure passes (figure rotation applied
 * after the instance's) and the shapes;</li>
 * <li>hand-built components and EmptyShapes under given transformations.</li>
 * </ul>
 * usage: BodyShapesProbe EXAMPLEDIR OUTFILE MOTORDB (MOTORDB: OpenRocket's initial_motors.db)
 */
public class BodyShapesProbe {
	static final String[] TEST_ROCKETS = { "makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods" };
	static final String[] VIEW_NAMES = { "side", "side", "top", "back", "back" };
	static final double[] VIEW_ROTATIONS = { 0.0, Math.PI / 6, 0.0, 0.0, Math.PI / 6 };

	static PrintStream out;

	public static void main(String[] args) throws Exception {
		bootstrap(Path.of(args[2]));
		out = new PrintStream(new File(args[1]), "UTF-8");
		Field acceptable = info.openrocket.swing.gui.rocketfigure.SymmetricComponentShapes.class
				.getDeclaredField("ACCEPTABLE_ANGLE");
		acceptable.setAccessible(true);
		out.println("K ACCEPTABLE_ANGLE " + h(acceptable.getDouble(null)) + " " + acceptable.getDouble(null) + " "
				+ h(Math.cos(7.0 * Math.PI / 180.0)) + " " + h(StrictMath.cos(7.0 * Math.PI / 180.0)));
		File[] files = new File(args[0]).listFiles((d, n) -> n.endsWith(".ork"));
		Arrays.sort(files);
		for (File f : files) {
			OpenRocketDocument doc = new GeneralRocketLoader(f).load();
			Rocket rocket = doc.getRocket();
			settle(rocket);
			dumpDesign("example " + f.getName(), rocket);
		}
		for (String name : TEST_ROCKETS) {
			Rocket rocket = (Rocket) TestRockets.class.getMethod(name).invoke(null);
			settle(rocket);
			dumpDesign("testrocket " + name, rocket);
		}
		handBuilt();
		out.close();
	}

	static void bootstrap(Path bundledDb) throws Exception {
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule())));
		List<ThrustCurveMotor> motorList = ThrustCurveMotorSQLiteDatabase.readDatabase(dbCopy.toFile());
		ThrustCurveMotorSetDatabase motors = new ThrustCurveMotorSetDatabase();
		for (ThrustCurveMotor m : motorList) {
			motors.addMotor(m);
		}
		ComponentPresetDatabase presets = new ComponentPresetDatabase();
		Module dbOverrides = new AbstractModule() {
			@Override
			protected void configure() {
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

	// ------------------------------------------------------------ designs

	static boolean isMine(RocketComponent c) {
		return c instanceof BodyTube || c instanceof Transition || c instanceof RingComponent
				|| c instanceof LaunchLug || c instanceof TubeFinSet;
	}

	static Transformation figureRotation(String view, double rotation) {
		Transformation axial = Transformation.rotate_x(rotation);
		if (view.equals("top")) {
			return axial.applyTransformation(Transformation.rotate_x(-Math.PI / 2));
		}
		return axial;
	}

	static void dumpDesign(String design, Rocket rocket) throws Exception {
		out.println("D " + design);
		FlightConfiguration config = rocket.getSelectedConfiguration();
		List<RocketComponent> order = new ArrayList<>();
		for (RocketComponent c : rocket) {
			order.add(c);
		}
		for (int index = 0; index < order.size(); index++) {
			RocketComponent comp = order.get(index);
			if (!isMine(comp)) {
				continue;
			}
			out.println("C " + index + " " + comp.getClass().getSimpleName() + " " + describeComponent(comp)
					+ " name " + comp.getName());
			List<InstanceContext> contexts = config.getActiveInstances().getInstanceContexts(comp);
			if (contexts == null) {
				out.println("X inactive");
				continue;
			}
			for (int j = 0; j < contexts.size(); j++) {
				InstanceContext context = contexts.get(j);
				for (int v = 0; v < VIEW_NAMES.length; v++) {
					String view = VIEW_NAMES[v];
					Transformation t = figureRotation(view, VIEW_ROTATIONS[v]).applyTransformation(context.transform);
					RocketComponentShapes[] shapes = view.equals("back")
							? RocketComponentShapeProvider.getShapesBack(comp, t)
							: RocketComponentShapeProvider.getShapesSide(comp, t);
					out.println("I " + index + " " + j + " " + context.instanceNumber + " " + view + " "
							+ h(VIEW_ROTATIONS[v]) + " T " + describe(t));
					for (RocketComponentShapes s : shapes) {
						out.println("S " + describe(s.shape));
					}
				}
			}
		}
	}

	static String describeComponent(RocketComponent c) {
		if (c instanceof NoseCone n) {
			return "shape " + n.getShapeType().name() + " param " + n.getShapeParameter() + " clipped " + n.isClipped()
					+ " flipped " + n.isFlipped() + " fore " + n.getForeShoulderLength() + " aft "
					+ n.getAftShoulderLength();
		}
		if (c instanceof Transition t) {
			return "shape " + t.getShapeType().name() + " param " + t.getShapeParameter() + " clipped " + t.isClipped()
					+ " fore " + t.getForeShoulderLength() + " aft " + t.getAftShoulderLength();
		}
		if (c instanceof RingComponent r) {
			return "outer " + r.getOuterRadius() + " inner " + r.getInnerRadius();
		}
		return "-";
	}

	// ------------------------------------------------------------ hand-built components

	static final Transformation T_AXIAL = Transformation.getTranslationTransform(0.25, 0, 0);
	static final Transformation T_OFF = Transformation.rotate_x(0.7)
			.applyTransformation(Transformation.getTranslationTransform(0.1, 0.02, -0.03));

	static void hand(String name, RocketComponent c, Transformation t, String view) throws Exception {
		RocketComponentShapes[] shapes = view.equals("back") ? RocketComponentShapeProvider.getShapesBack(c, t)
				: RocketComponentShapeProvider.getShapesSide(c, t);
		out.println("H " + name + " " + view + " T " + describe(t));
		for (RocketComponentShapes s : shapes) {
			out.println("S " + describe(s.shape));
		}
	}

	/** Side and back with both transformations. */
	static void handAll(String name, RocketComponent c) throws Exception {
		hand(name + "/axial", c, T_AXIAL, "side");
		hand(name + "/axial", c, T_AXIAL, "back");
		hand(name + "/off", c, T_OFF, "side");
		hand(name + "/off", c, T_OFF, "back");
	}

	static InnerTube innerTube(double length, double outer, double inner) {
		InnerTube t = new InnerTube();
		t.setLength(length);
		t.setOuterRadius(outer);
		t.setInnerRadius(inner);
		return t;
	}

	static Transition transition(Transition.Shape shape, double param, double length, double fore, double aft) {
		Transition t = new Transition();
		t.setShapeType(shape);
		t.setShapeParameter(param);
		t.setLength(length);
		t.setForeRadius(fore);
		t.setAftRadius(aft);
		return t;
	}

	static void handBuilt() throws Exception {
		out.println("D hand-built");
		// Ring components: the inner outline needs outer - inner >= 0.0012 and inner > 0.
		handAll("ring-inner-tube-0.0113", innerTube(0.075, 0.0125, 0.0113));
		handAll("ring-inner-tube-0.0112", innerTube(0.075, 0.0125, 0.0112));
		handAll("ring-inner-tube-0.0114", innerTube(0.075, 0.0125, 0.0114));
		handAll("ring-inner-tube-0.0188", innerTube(0.075, 0.02, 0.0188));
		InnerTube thick = new InnerTube();
		thick.setLength(0.06);
		thick.setOuterRadius(0.012);
		thick.setThickness(0.0012);
		handAll("ring-inner-tube-thickness", thick);
		Bulkhead bulkhead = new Bulkhead();
		bulkhead.setLength(0.002);
		bulkhead.setOuterRadius(0.02);
		handAll("ring-bulkhead", bulkhead);
		CenteringRing ring = new CenteringRing();
		ring.setLength(0.002);
		ring.setOuterRadius(0.03);
		ring.setInnerRadius(0.01);
		handAll("ring-centering-ring", ring);
		CenteringRing closed = new CenteringRing();
		closed.setLength(0.003);
		closed.setOuterRadius(0.01);
		closed.setInnerRadius(0.01);
		handAll("ring-centering-ring-closed", closed);
		TubeCoupler coupler = new TubeCoupler();
		coupler.setLength(0.04);
		coupler.setOuterRadius(0.015);
		coupler.setInnerRadius(0.0145);
		handAll("ring-tube-coupler", coupler);
		EngineBlock block = new EngineBlock();
		block.setLength(0.005);
		block.setOuterRadius(0.009);
		block.setThickness(0.003);
		handAll("ring-engine-block", block);

		BodyTube body = new BodyTube();
		body.setLength(0.3);
		body.setOuterRadius(0.0125);
		handAll("body-tube", body);

		LaunchLug lug = new LaunchLug();
		lug.setLength(0.035);
		lug.setOuterRadius(0.0035);
		handAll("launch-lug", lug);

		TubeFinSet fins = new TubeFinSet();
		fins.setLength(0.1);
		fins.setOuterRadius(0.01);
		handAll("tube-fin-set", fins);

		// Conical transitions: a closed float path; shoulders longer than 0.0005 m.
		Transition conical = transition(Transition.Shape.CONICAL, 0, 0.05, 0.01, 0.02);
		conical.setForeShoulderLength(0.02);
		conical.setForeShoulderRadius(0.009);
		conical.setAftShoulderLength(0.0005);
		conical.setAftShoulderRadius(0.019);
		handAll("transition-conical-fore-shoulder", conical);
		Transition conical2 = transition(Transition.Shape.CONICAL, 0, 0.05, 0.02, 0.01);
		conical2.setForeShoulderLength(0.0005);
		conical2.setForeShoulderRadius(0.019);
		conical2.setAftShoulderLength(0.0006);
		conical2.setAftShoulderRadius(0.009);
		handAll("transition-conical-aft-shoulder", conical2);

		// The sampled shapes, each type, side view without a transformation (the points are the profile).
		Object[][] sampled = { { "ogive", Transition.Shape.OGIVE, 0.6 }, { "ellipsoid", Transition.Shape.ELLIPSOID, 0.0 },
				{ "power", Transition.Shape.POWER, 0.5 }, { "parabolic", Transition.Shape.PARABOLIC, 0.5 },
				{ "haack", Transition.Shape.HAACK, 0.0 }, { "haack-third", Transition.Shape.HAACK, 1.0 / 3 } };
		for (Object[] s : sampled) {
			Transition t = transition((Transition.Shape) s[1], (Double) s[2], 0.08, 0.005, 0.02);
			hand("transition-" + s[0], t, Transformation.IDENTITY, "side");
		}
		Transition unclipped = transition(Transition.Shape.ELLIPSOID, 0, 0.08, 0.005, 0.02);
		unclipped.setClipped(false);
		hand("transition-ellipsoid-unclipped", unclipped, Transformation.IDENTITY, "side");
		Transition boattail = transition(Transition.Shape.OGIVE, 1.0, 0.05, 0.02, 0.012);
		boattail.setForeShoulderLength(0.01);
		boattail.setForeShoulderRadius(0.019);
		boattail.setAftShoulderLength(0.01);
		boattail.setAftShoulderRadius(0.011);
		handAll("transition-boattail", boattail);
		Transition shortOne = transition(Transition.Shape.OGIVE, 1.0, 0.0005, 0.01, 0.02);
		hand("transition-short", shortOne, T_AXIAL, "side");
		Transition flat = transition(Transition.Shape.ELLIPSOID, 0, 0, 0.01, 0.02);
		hand("transition-zero-length", flat, T_AXIAL, "side");
		hand("transition-zero-length", flat, T_AXIAL, "back");

		// Nose cones.
		NoseCone ogive = new NoseCone(Transition.Shape.OGIVE, 0.1, 0.0125);
		ogive.setShoulderLength(0.02);
		ogive.setShoulderRadius(0.0115);
		hand("nose-ogive", ogive, T_AXIAL, "side");
		hand("nose-ogive", ogive, T_AXIAL, "back");
		NoseCone tail = new NoseCone(Transition.Shape.ELLIPSOID, 0.1, 0.025);
		tail.setShoulderLength(0.03);
		tail.setShoulderRadius(0.024);
		tail.setFlipped(true);
		hand("nose-ellipsoid-flipped", tail, T_AXIAL, "side");
		hand("nose-ellipsoid-flipped", tail, T_AXIAL, "back");
		NoseCone power = new NoseCone(Transition.Shape.POWER, 0.12, 0.02);
		power.setShapeParameter(0.3);
		hand("nose-power", power, Transformation.IDENTITY, "side");
		NoseCone haack = new NoseCone(Transition.Shape.HAACK, 0.12, 0.02);
		hand("nose-haack", haack, Transformation.IDENTITY, "side");
		NoseCone conicalNose = new NoseCone(Transition.Shape.CONICAL, 0.07, 0.01);
		hand("nose-conical", conicalNose, T_OFF, "side");
		hand("nose-conical", conicalNose, T_OFF, "back");
		NoseCone offOgive = new NoseCone(Transition.Shape.OGIVE, 0.07, 0.01);
		hand("nose-ogive-off", offOgive, T_OFF, "side");

		// Outlines that the refinement changes much: steep tips and a fat ogive.
		hand("refine-ellipsoid-nose", new NoseCone(Transition.Shape.ELLIPSOID, 0.5, 0.1), Transformation.IDENTITY,
				"side");
		NoseCone steepPower = new NoseCone(Transition.Shape.POWER, 0.3, 0.05);
		steepPower.setShapeParameter(0.1);
		hand("refine-power-nose", steepPower, Transformation.IDENTITY, "side");
		hand("refine-fat-ogive", new NoseCone(Transition.Shape.OGIVE, 0.1, 0.2), Transformation.IDENTITY, "side");
		hand("refine-haack-transition", transition(Transition.Shape.HAACK, 0, 0.4, 0.02, 0.1),
				Transformation.IDENTITY, "side");
		hand("refine-fat-ellipsoid", new NoseCone(Transition.Shape.ELLIPSOID, 0.1, 0.3), Transformation.IDENTITY,
				"side");
		hand("refine-fat-haack", new NoseCone(Transition.Shape.HAACK, 0.12, 0.2), Transformation.IDENTITY, "side");
		hand("refine-parabolic-nose", new NoseCone(Transition.Shape.PARABOLIC, 0.2, 0.1), Transformation.IDENTITY,
				"side");

		// PrintableNoseCone's call: no transformation, scaled to points.
		double scale = PrintUnit.METERS.toPoints(1);
		RocketComponentShapes[] printed = TransitionShapes.getShapesSide(ogive, Transformation.IDENTITY, scale);
		out.println("H nose-ogive-printed side scale " + h(scale) + " T " + describe(Transformation.IDENTITY));
		for (RocketComponentShapes s : printed) {
			out.println("S " + describe(s.shape));
		}

		// EmptyShapes, ComponentAssemblyShapes' marker.
		for (Transformation t : new Transformation[] { T_AXIAL, T_OFF }) {
			String suffix = t == T_AXIAL ? "axial" : "off";
			emptyShapes("empty/" + suffix, EmptyShapes.getShapesSide(t, 0.0042), t, "side");
			emptyShapes("empty-selection/" + suffix, EmptyShapes.getShapesSideWithSelectionSquare(t, 0.0042), t,
					"side");
			emptyShapes("empty/" + suffix, EmptyShapes.getShapesBack(t, 0.0042), t, "back");
			emptyShapes("empty-selection/" + suffix, EmptyShapes.getShapesBackWithSelectionSquare(t, 0.0042), t,
					"back");
		}
	}

	static void emptyShapes(String name, Shape[] shapes, Transformation t, String view) throws Exception {
		out.println("H " + name + " " + view + " T " + describe(t));
		for (Shape s : shapes) {
			out.println("S " + describe(s));
		}
	}

	// ------------------------------------------------------------ printing

	static String h(double v) {
		return Double.toHexString(v);
	}

	static String describe(Transformation t) throws Exception {
		Field rotationField = Transformation.class.getDeclaredField("rotation");
		rotationField.setAccessible(true);
		Field translateField = Transformation.class.getDeclaredField("translate");
		translateField.setAccessible(true);
		double[][] r = (double[][]) rotationField.get(t);
		CoordinateIF c = (CoordinateIF) translateField.get(t);
		StringBuilder b = new StringBuilder();
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				b.append(h(r[i][j])).append(' ');
			}
		}
		b.append(h(c.getX())).append(' ').append(h(c.getY())).append(' ').append(h(c.getZ())).append(' ')
				.append(h(c.getWeight()));
		return b.toString();
	}

	static String describe(Shape s) {
		StringBuilder b = new StringBuilder();
		if (s instanceof Rectangle2D.Double re) {
			b.append("R ").append(h(re.x)).append(' ').append(h(re.y)).append(' ').append(h(re.width)).append(' ')
					.append(h(re.height));
		} else if (s instanceof Ellipse2D.Double e) {
			b.append("E ").append(h(e.x)).append(' ').append(h(e.y)).append(' ').append(h(e.width)).append(' ')
					.append(h(e.height));
		} else if (s instanceof Line2D.Double l) {
			b.append("N ").append(h(l.x1)).append(' ').append(h(l.y1)).append(' ').append(h(l.x2)).append(' ')
					.append(h(l.y2));
		} else if (s instanceof Path2D p) {
			b.append("P ").append(p instanceof Path2D.Float ? "F" : "D").append(' ').append(p.getWindingRule());
			PathIterator it = s.getPathIterator(null);
			double[] c = new double[6];
			while (!it.isDone()) {
				int type = it.currentSegment(c);
				switch (type) {
					case PathIterator.SEG_MOVETO -> b.append(" M ").append(h(c[0])).append(' ').append(h(c[1]));
					case PathIterator.SEG_LINETO -> b.append(" L ").append(h(c[0])).append(' ').append(h(c[1]));
					case PathIterator.SEG_QUADTO -> b.append(" Q ").append(h(c[0])).append(' ').append(h(c[1]))
							.append(' ').append(h(c[2])).append(' ').append(h(c[3]));
					case PathIterator.SEG_CUBICTO -> b.append(" C ").append(h(c[0])).append(' ').append(h(c[1]))
							.append(' ').append(h(c[2])).append(' ').append(h(c[3])).append(' ').append(h(c[4]))
							.append(' ').append(h(c[5]));
					case PathIterator.SEG_CLOSE -> b.append(" Z");
					default -> b.append(" ?");
				}
				it.next();
			}
		} else {
			b.append("? ").append(s.getClass().getName());
		}
		return b.toString();
	}
}
