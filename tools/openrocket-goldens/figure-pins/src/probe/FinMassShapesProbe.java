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
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.EllipticalFinSet;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.InstanceContext;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.PodSet;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.ShockCord;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.position.AxialMethod;
import info.openrocket.core.rocketcomponent.position.RadiusMethod;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.BoundingBox;
import info.openrocket.core.util.Coordinate;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.ORColor;
import info.openrocket.core.util.TestRockets;
import info.openrocket.core.util.Transformation;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapeProvider;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapes;

import java.awt.Shape;
import java.awt.geom.Arc2D;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Line2D;
import java.awt.geom.Path2D;
import java.awt.geom.PathIterator;
import java.awt.geom.Rectangle2D;
import java.awt.geom.RoundRectangle2D;
import java.io.File;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Dumps what OpenRocket's shape providers of QtRocket's part F3 (FinSetShapes, MassComponentShapes,
 * ParachuteShapes, StreamerShapes, ShockCordShapes, RailButtonShapes, ComponentAssemblyShapes for pod sets,
 * booster sets and the rocket) return, and what RocketFigure assembles for whole designs (its updateShapes,
 * addThisShape, drawMotors and updateSubjectDimensions, replicated line by line, since RocketFigure itself
 * needs SwingPreferences), with every double as Double.toHexString (exact):
 * <ul>
 * <li>"I" records: for every component of those kinds of the 16 examples (loaded with no presets) and the
 * 13 test rockets, both settled as the golden harness settles them, every active instance in five views
 * (side 0, side 30 deg, top 0, back 0, back 30 deg): the transformation RocketFigure passes and the shapes;</li>
 * <li>"G" records: the figure of each design in six views (side, top and back, each at 0 and 30 deg), with
 * the markers preference off, and for the designs with pod sets or booster sets also on (with no selection
 * and with the first such assembly selected): the subject bounds, every shape with its component, instance
 * and bounds, and the motors;</li>
 * <li>"H" records: hand-built components under transformations of their own.</li>
 * </ul>
 * usage: FinMassShapesProbe EXAMPLEDIR OUTFILE MOTORDB (MOTORDB: OpenRocket's initial_motors.db)
 */
public class FinMassShapesProbe {
	static final String[] TEST_ROCKETS = { "makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods" };
	static final String[] VIEW_NAMES = { "side", "side", "top", "back", "back" };
	static final double[] VIEW_ROTATIONS = { 0.0, Math.PI / 6, 0.0, 0.0, Math.PI / 6 };
	static final String[] GEOMETRY_VIEWS = { "side", "side", "top", "top", "back", "back" };
	static final double[] GEOMETRY_ROTATIONS = { 0.0, Math.PI / 6, 0.0, Math.PI / 6, 0.0, Math.PI / 6 };

	static PrintStream out;

	public static void main(String[] args) throws Exception {
		bootstrap(Path.of(args[2]));
		out = new PrintStream(new File(args[1]), "UTF-8");
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
		// A new design (OpenRocketDocumentFactory.createNewRocket): one stage without children.
		OpenRocketDocument fresh = OpenRocketDocumentFactory.createNewRocket();
		out.println("D new design");
		dumpGeometries(fresh.getRocket(), treeOrder(fresh.getRocket()));
		dumpDesign("hand-built empty booster sets", emptyBoosterRocket());
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
		return c instanceof FinSet || c instanceof MassObject || c instanceof RailButton || c instanceof PodSet
				|| c instanceof ParallelStage;
	}

	/** RocketFigure.getFigureRotation(). */
	static Transformation figureRotation(String view, double rotation) {
		Transformation axial = Transformation.rotate_x(rotation);
		if (view.equals("top")) {
			return axial.applyTransformation(Transformation.rotate_x(-Math.PI / 2));
		}
		return axial;
	}

	static List<RocketComponent> treeOrder(Rocket rocket) {
		List<RocketComponent> order = new ArrayList<>();
		for (RocketComponent c : rocket) {
			order.add(c);
		}
		return order;
	}

	static void dumpDesign(String design, Rocket rocket) throws Exception {
		out.println("D " + design);
		FlightConfiguration config = rocket.getSelectedConfiguration();
		List<RocketComponent> order = treeOrder(rocket);
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
					if (shapes == null) {
						out.println("S null");
						continue;
					}
					for (RocketComponentShapes s : shapes) {
						out.println("S " + visibility(s) + " " + describe(s.shape));
					}
				}
			}
		}
		dumpGeometries(rocket, order);
	}

	static String describeComponent(RocketComponent c) {
		if (c instanceof FinSet f) {
			return "cant " + f.getCantAngle() + " thickness " + f.getThickness() + " tab " + f.getTabHeight() + " "
					+ f.getTabLength();
		}
		if (c instanceof MassComponent m) {
			return "type " + m.getMassComponentType().name() + " length " + m.getLength() + " radius " + m.getRadius()
					+ " radial " + m.getRadialPosition() + " " + m.getRadialDirection();
		}
		if (c instanceof MassObject m) {
			return "length " + m.getLength() + " radius " + m.getRadius() + " radial " + m.getRadialPosition() + " "
					+ m.getRadialDirection();
		}
		if (c instanceof RailButton b) {
			return "base " + b.getBaseHeight() + " inner " + b.getInnerHeight() + " flange " + b.getFlangeHeight()
					+ " screw " + b.getScrewHeight() + " angle " + b.getAngleOffset();
		}
		if (c instanceof PodSet || c instanceof ParallelStage) {
			return "axial " + c.getAxialMethod() + " radius " + c.getRadiusMethod();
		}
		return "-";
	}

	// ------------------------------------------------------------ RocketFigure, replicated

	/** The designs' figures in the six views, with the markers preference off and on. */
	static void dumpGeometries(Rocket rocket, List<RocketComponent> order) {
		RocketComponent firstAssembly = null;
		for (RocketComponent c : order) {
			if (c instanceof PodSet || c instanceof ParallelStage) {
				firstAssembly = c;
				break;
			}
		}
		for (int v = 0; v < GEOMETRY_VIEWS.length; v++) {
			dumpGeometry(rocket, order, GEOMETRY_VIEWS[v], GEOMETRY_ROTATIONS[v], false, Collections.emptySet());
			if (firstAssembly != null) {
				dumpGeometry(rocket, order, GEOMETRY_VIEWS[v], GEOMETRY_ROTATIONS[v], true, Collections.emptySet());
				dumpGeometry(rocket, order, GEOMETRY_VIEWS[v], GEOMETRY_ROTATIONS[v], true, Set.of(firstAssembly));
			}
		}
	}

	static void dumpGeometry(Rocket rocket, List<RocketComponent> order, String view, double rotation,
			boolean showMarkers, Set<RocketComponent> selection) {
		FlightConfiguration config = rocket.getSelectedConfiguration();
		StringBuilder sel = new StringBuilder();
		for (RocketComponent c : selection) {
			sel.append(' ').append(order.indexOf(c));
		}
		out.println("G " + view + " " + h(rotation) + " markers " + showMarkers + " selection" + sel);

		// updateSubjectDimensions
		final BoundingBox bounds = config.getBoundingBox();
		final double maxR = Math.max(Math.hypot(bounds.min.getY(), bounds.min.getZ()),
				Math.hypot(bounds.max.getY(), bounds.max.getZ()));
		Rectangle2D subject;
		if (view.equals("back")) {
			subject = new Rectangle2D.Double(-maxR, -maxR, 2 * maxR, 2 * maxR);
		} else {
			subject = new Rectangle2D.Double(bounds.min.getX(), -maxR, bounds.span().getX(), 2 * maxR);
		}
		out.println("B " + h(subject.getX()) + " " + h(subject.getY()) + " " + h(subject.getWidth()) + " "
				+ h(subject.getHeight()));

		// updateShapes: the active instances, then the extra render instances
		Transformation figRot = figureRotation(view, rotation);
		dumpInstanceEntries(config.getActiveInstances().entrySet(), 0, order, view, figRot, showMarkers, selection);
		dumpInstanceEntries(config.getExtraRenderInstances().entrySet(), 1, order, view, figRot, showMarkers,
				selection);

		// drawMotors, with the motors sorted by their mount's tree index (getActiveMotors iterates a HashMap
		// keyed by random ids)
		List<MotorConfiguration> activeMotors = new ArrayList<>(config.getActiveMotors());
		activeMotors.sort((a, b) -> Integer.compare(order.indexOf((RocketComponent) a.getMount()),
				order.indexOf((RocketComponent) b.getMount())));
		for (MotorConfiguration curInstance : activeMotors) {
			MotorMount mount = curInstance.getMount();
			if (!((RocketComponent) mount).isVisible()) {
				continue;
			}
			Motor motor = curInstance.getMotor();
			double motorLength = motor.getLength();
			double motorRadius = motor.getDiameter() / 2;
			CoordinateIF[] mountLocations = mount.getComponentLocations();
			CoordinateIF motorPosition = mount.getMotorPosition(config.getId());
			for (int k = 0; k < mountLocations.length; k++) {
				CoordinateIF curMotorLocation = mountLocations[k].add(motorPosition);
				curMotorLocation = figRot.transform(curMotorLocation);
				Shape s;
				if (!view.equals("back")) {
					s = new Rectangle2D.Double(curMotorLocation.getX(), (curMotorLocation.getY() - motorRadius),
							motorLength, 2 * motorRadius);
				} else {
					s = new Ellipse2D.Double((curMotorLocation.getZ() - motorRadius),
							(curMotorLocation.getY() - motorRadius), 2 * motorRadius, 2 * motorRadius);
				}
				out.println("M " + order.indexOf((RocketComponent) mount) + " " + k + " " + describe(s));
			}
		}
	}

	static void dumpInstanceEntries(Set<Map.Entry<RocketComponent, ArrayList<InstanceContext>>> entries, int extra,
			List<RocketComponent> order, String view, Transformation figRot, boolean showMarkers,
			Set<RocketComponent> selection) {
		// Sorted by tree order: InstanceMap is a ConcurrentHashMap.
		List<Map.Entry<RocketComponent, ArrayList<InstanceContext>>> sorted = new ArrayList<>(entries);
		sorted.sort((a, b) -> Integer.compare(order.indexOf(a.getKey()), order.indexOf(b.getKey())));
		for (Map.Entry<RocketComponent, ArrayList<InstanceContext>> entry : sorted) {
			final RocketComponent comp = entry.getKey();
			// Only draw pod sets and boosters when they are selected
			if (showMarkers && (comp instanceof PodSet || comp instanceof ParallelStage)) {
				if (!selection.contains(comp)) {
					continue;
				}
			}
			final List<InstanceContext> contextList = entry.getValue();
			for (int j = 0; j < contextList.size(); j++) {
				InstanceContext context = contextList.get(j);
				final Transformation currentTransform = figRot.applyTransformation(context.transform);
				// addThisShape
				if ((comp instanceof Rocket) || (comp instanceof AxialStage && !(comp instanceof ParallelStage))) {
					continue;
				}
				RocketComponentShapes[] shapes = view.equals("back")
						? RocketComponentShapeProvider.getShapesBack(comp, currentTransform)
						: RocketComponentShapeProvider.getShapesSide(comp, currentTransform);
				int displayOrder = view.equals("back") ? comp.getDisplayOrder_back() : comp.getDisplayOrder_side();
				for (int k = 0; k < shapes.length; k++) {
					RocketComponentShapes s = shapes[k];
					Rectangle2D b = s.shape.getBounds2D();
					out.println("E " + order.indexOf(comp) + " " + extra + " " + j + " " + context.instanceNumber + " "
							+ displayOrder + " " + k + " " + visibility(s) + " " + shapeLetter(s.shape) + " "
							+ h(b.getX()) + " " + h(b.getY()) + " " + h(b.getWidth()) + " " + h(b.getHeight()));
				}
			}
		}
	}

	// ------------------------------------------------------------ hand-built components

	static final Transformation T_AXIAL = Transformation.getTranslationTransform(0.25, 0, 0);
	static final Transformation T_OFF = Transformation.rotate_x(0.7)
			.applyTransformation(Transformation.getTranslationTransform(0.1, 0.02, -0.03));

	static void hand(String name, RocketComponent c, Transformation t, String view) throws Exception {
		RocketComponentShapes[] shapes = view.equals("back") ? RocketComponentShapeProvider.getShapesBack(c, t)
				: RocketComponentShapeProvider.getShapesSide(c, t);
		out.println("H " + name + " " + view + " T " + describe(t));
		if (shapes == null) {
			out.println("S null");
			return;
		}
		for (RocketComponentShapes s : shapes) {
			out.println("S " + visibility(s) + " " + describe(s.shape));
		}
	}

	/** Side and back with both transformations. */
	static void handAll(String name, RocketComponent c) throws Exception {
		hand(name + "/axial", c, T_AXIAL, "side");
		hand(name + "/axial", c, T_AXIAL, "back");
		hand(name + "/off", c, T_OFF, "side");
		hand(name + "/off", c, T_OFF, "back");
	}

	/** A rocket with a stage and a body tube (length 0.3, radius 0.02) to hang fins and assemblies on. */
	static BodyTube bodyOfNewRocket() {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		BodyTube body = new BodyTube();
		body.setLength(0.3);
		body.setOuterRadius(0.02);
		stage.addChild(body);
		return body;
	}

	/** A rocket with a stage and a conical transition (length 0.1, radii 0.02 and 0.03). */
	static Transition transitionOfNewRocket() {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		Transition transition = new Transition();
		transition.setShapeType(Transition.Shape.CONICAL);
		transition.setLength(0.1);
		transition.setForeRadius(0.02);
		transition.setAftRadius(0.03);
		stage.addChild(transition);
		return transition;
	}

	/**
	 * A rocket with a booster set and a pod set without children (the booster set is an extra render instance:
	 * inactive, since it has no children, but its stage is flagged active) beside a booster set with a body.
	 */
	static Rocket emptyBoosterRocket() {
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		NoseCone nose = new NoseCone(Transition.Shape.OGIVE, 0.1, 0.02);
		stage.addChild(nose);
		BodyTube body = new BodyTube();
		body.setLength(0.3);
		body.setOuterRadius(0.02);
		stage.addChild(body);
		ParallelStage empty = new ParallelStage();
		empty.setInstanceCount(3);
		body.addChild(empty);
		ParallelStage boosters = new ParallelStage();
		boosters.setInstanceCount(2);
		body.addChild(boosters);
		BodyTube boosterBody = new BodyTube();
		boosterBody.setLength(0.15);
		boosterBody.setOuterRadius(0.01);
		boosters.addChild(boosterBody);
		PodSet emptyPods = new PodSet();
		emptyPods.setInstanceCount(2);
		body.addChild(emptyPods);
		rocket.enableEvents();
		rocket.getSelectedConfiguration().setAllStages();
		return rocket;
	}

	static TrapezoidFinSet trapezoid() {
		TrapezoidFinSet fins = new TrapezoidFinSet();
		fins.setFinCount(3);
		fins.setRootChord(0.08);
		fins.setTipChord(0.04);
		fins.setSweep(0.03);
		fins.setHeight(0.05);
		fins.setThickness(0.003);
		return fins;
	}

	static MassComponent mass(MassComponent.MassComponentType type, double length, double radius) {
		MassComponent m = new MassComponent();
		m.setMassComponentType(type);
		m.setLength(length);
		m.setRadius(radius);
		return m;
	}

	static void handBuilt() throws Exception {
		out.println("D hand-built");

		// ---- fin sets
		TrapezoidFinSet plain = trapezoid();
		bodyOfNewRocket().addChild(plain);
		plain.getRocket().enableEvents();
		handAll("fin-trapezoid", plain);

		TrapezoidFinSet tabbed = trapezoid();
		bodyOfNewRocket().addChild(tabbed);
		tabbed.setTabHeight(0.01);
		tabbed.setTabLength(0.04);
		tabbed.setTabOffset(0.01);
		tabbed.getRocket().enableEvents();
		handAll("fin-trapezoid-tab", tabbed);

		TrapezoidFinSet canted = trapezoid();
		bodyOfNewRocket().addChild(canted);
		canted.setCantAngle(0.1);
		canted.setTabHeight(0.01);
		canted.setTabLength(0.04);
		canted.setTabOffset(0.01);
		canted.getRocket().enableEvents();
		handAll("fin-canted-tab", canted);

		TrapezoidFinSet negative = trapezoid();
		bodyOfNewRocket().addChild(negative);
		negative.setCantAngle(-0.05);
		negative.getRocket().enableEvents();
		handAll("fin-canted-negative", negative);

		TrapezoidFinSet thin = trapezoid();
		bodyOfNewRocket().addChild(thin);
		thin.setThickness(0.0004);
		thin.setCantAngle(0.05);
		thin.setTabHeight(0.008);
		thin.setTabLength(0.03);
		thin.getRocket().enableEvents();
		handAll("fin-canted-thin", thin);

		EllipticalFinSet elliptical = new EllipticalFinSet();
		bodyOfNewRocket().addChild(elliptical);
		elliptical.setLength(0.07);
		elliptical.setHeight(0.04);
		elliptical.setThickness(0.002);
		elliptical.getRocket().enableEvents();
		handAll("fin-elliptical", elliptical);

		FreeformFinSet freeform = new FreeformFinSet();
		transitionOfNewRocket().addChild(freeform);
		freeform.setPoints(new Coordinate[] { new Coordinate(0, 0), new Coordinate(0.02, 0.05),
				new Coordinate(0.06, 0.05), new Coordinate(0.08, 0) });
		freeform.setThickness(0.003);
		freeform.getRocket().enableEvents();
		handAll("fin-freeform-on-transition", freeform);

		FreeformFinSet onTransition = new FreeformFinSet();
		transitionOfNewRocket().addChild(onTransition);
		onTransition.setPoints(new Coordinate[] { new Coordinate(0, 0), new Coordinate(0.03, 0.04),
				new Coordinate(0.07, 0.04), new Coordinate(0.09, 0) });
		onTransition.setThickness(0.003);
		onTransition.setTabHeight(0.01);
		onTransition.setTabLength(0.04);
		onTransition.setTabOffset(0.01);
		onTransition.getRocket().enableEvents();
		handAll("fin-freeform-tab-on-transition", onTransition);

		FreeformFinSet cantedOnTransition = new FreeformFinSet();
		transitionOfNewRocket().addChild(cantedOnTransition);
		cantedOnTransition.setPoints(new Coordinate[] { new Coordinate(0, 0), new Coordinate(0.03, 0.04),
				new Coordinate(0.07, 0.04), new Coordinate(0.09, 0) });
		cantedOnTransition.setThickness(0.003);
		cantedOnTransition.setCantAngle(0.08);
		cantedOnTransition.setTabHeight(0.01);
		cantedOnTransition.setTabLength(0.04);
		cantedOnTransition.setTabOffset(0.01);
		cantedOnTransition.getRocket().enableEvents();
		handAll("fin-canted-tab-on-transition", cantedOnTransition);

		// ---- mass components, every type
		for (MassComponent.MassComponentType type : MassComponent.MassComponentType.values()) {
			handAll("mass-" + type.name(), mass(type, 0.06, 0.012));
		}
		// short and wide: the arc is 0.7 length; the symbols' other branches
		for (MassComponent.MassComponentType type : new MassComponent.MassComponentType[] {
				MassComponent.MassComponentType.ALTIMETER, MassComponent.MassComponentType.FLIGHTCOMPUTER,
				MassComponent.MassComponentType.TRACKER, MassComponent.MassComponentType.RECOVERYHARDWARE,
				MassComponent.MassComponentType.BATTERY, MassComponent.MassComponentType.PAYLOAD }) {
			handAll("mass-short-" + type.name(), mass(type, 0.01, 0.02));
		}
		// long and thin
		handAll("mass-long-TRACKER", mass(MassComponent.MassComponentType.TRACKER, 0.1, 0.005));
		handAll("mass-long-BATTERY", mass(MassComponent.MassComponentType.BATTERY, 0.1, 0.005));
		// off the axis
		MassComponent offAxis = mass(MassComponent.MassComponentType.PAYLOAD, 0.03, 0.008);
		offAxis.setRadialPosition(0.01);
		offAxis.setRadialDirection(1.0);
		handAll("mass-off-axis", offAxis);

		// ---- recovery devices and the shock cord
		Parachute chute = new Parachute();
		chute.setLength(0.04);
		chute.setRadius(0.015);
		handAll("parachute", chute);
		Parachute wideChute = new Parachute();
		wideChute.setLength(0.01);
		wideChute.setRadius(0.02);
		wideChute.setRadialPosition(0.004);
		wideChute.setRadialDirection(-0.5);
		handAll("parachute-wide", wideChute);
		Streamer streamer = new Streamer();
		streamer.setLength(0.05);
		streamer.setRadius(0.01);
		handAll("streamer", streamer);
		ShockCord cord = new ShockCord();
		cord.setLength(0.03);
		cord.setRadius(0.008);
		cord.setRadialPosition(0.005);
		cord.setRadialDirection(2.0);
		handAll("shock-cord", cord);

		// ---- rail buttons
		RailButton button = new RailButton();
		handAll("rail-default", button);
		RailButton screw = new RailButton();
		screw.setScrewHeight(0.002);
		handAll("rail-screw", screw);
		RailButton noBase = new RailButton();
		noBase.setBaseHeight(0);
		noBase.setScrewHeight(0.0015);
		handAll("rail-no-base", noBase);
		RailButton bare = new RailButton();
		bare.setFlangeHeight(0);
		bare.setScrewHeight(0);
		handAll("rail-no-flange", bare);
		RailButton angled = new RailButton();
		angled.setAngleOffset(0.3);
		angled.setScrewHeight(0.002);
		handAll("rail-angle", angled);
		// rotated about the axis: the sine and cosine of the view angle change sign; at some angles the screw
		// hides behind the flange
		double[] rotations = { 0.5, 1.6, 2.5, Math.PI, 3.9, 3 * Math.PI / 2, 5.5 };
		for (int i = 0; i < rotations.length; i++) {
			Transformation t = Transformation.rotate_x(rotations[i]).applyTransformation(T_AXIAL);
			hand("rail-screw/rot" + i, screw, t, "side");
			hand("rail-screw/rot" + i, screw, t, "back");
		}

		// ---- pod sets and booster sets: the markers
		AxialMethod[] axials = { AxialMethod.TOP, AxialMethod.BOTTOM, AxialMethod.MIDDLE, AxialMethod.ABSOLUTE };
		for (AxialMethod axial : axials) {
			BodyTube body = bodyOfNewRocket();
			PodSet pods = new PodSet();
			pods.setInstanceCount(2);
			body.addChild(pods);
			BodyTube podBody = new BodyTube();
			podBody.setLength(0.1);
			podBody.setOuterRadius(0.01);
			pods.addChild(podBody);
			pods.setAxialMethod(axial);
			pods.setRadiusMethod(RadiusMethod.RELATIVE);
			pods.setRadiusOffset(0.005);
			pods.getRocket().enableEvents();
			handAll("pods-" + axial.name(), pods);
		}
		{
			BodyTube body = bodyOfNewRocket();
			PodSet pods = new PodSet();
			body.addChild(pods);
			BodyTube podBody = new BodyTube();
			podBody.setLength(0.08);
			podBody.setOuterRadius(0.012);
			pods.addChild(podBody);
			pods.setRadiusMethod(RadiusMethod.FREE);
			pods.setRadiusOffset(0.05);
			pods.getRocket().enableEvents();
			handAll("pods-FREE", pods);
			PodSet empty = new PodSet();
			body.addChild(empty);
			empty.getRocket().enableEvents();
			handAll("pods-empty", empty);
		}
		for (AxialMethod axial : axials) {
			BodyTube body = bodyOfNewRocket();
			ParallelStage boosters = new ParallelStage();
			boosters.setInstanceCount(3);
			body.addChild(boosters);
			BodyTube boosterBody = new BodyTube();
			boosterBody.setLength(0.15);
			boosterBody.setOuterRadius(0.015);
			boosters.addChild(boosterBody);
			boosters.setAxialMethod(axial);
			boosters.setRadiusMethod(RadiusMethod.SURFACE);
			boosters.getRocket().enableEvents();
			handAll("boosters-" + axial.name(), boosters);
		}
		{
			BodyTube body = bodyOfNewRocket();
			ParallelStage boosters = new ParallelStage();
			body.addChild(boosters);
			BodyTube boosterBody = new BodyTube();
			boosterBody.setLength(0.15);
			boosterBody.setOuterRadius(0.015);
			boosters.addChild(boosterBody);
			boosters.setRadiusMethod(RadiusMethod.RELATIVE);
			boosters.setRadiusOffset(0.002);
			boosters.getRocket().enableEvents();
			handAll("boosters-RELATIVE", boosters);
			// the rocket itself: ComponentAssemblyShapes draws a marker for it (RocketFigure never asks)
			Rocket rocket = body.getRocket();
			handAll("rocket", rocket);
			handAll("stage", rocket.getChild(0));
		}

		// A boundary case: a canted fin set with a tab shorter than the root's divisions (2.5 mm), so that no
		// root point lies on the tab. The back view's search for the tab's lowest point then passes over
		// neighbours of equal height (the root corners, then the two bottom corners): it moves on while the
		// point before is not strictly higher.
		TrapezoidFinSet shortTab = trapezoid();
		bodyOfNewRocket().addChild(shortTab);
		shortTab.setCantAngle(0.1);
		shortTab.setTabHeight(0.01);
		shortTab.setTabLength(0.002);
		shortTab.setTabOffset(0);
		shortTab.getRocket().enableEvents();
		handAll("fin-canted-short-tab", shortTab);
	}

	// ------------------------------------------------------------ printing

	static String h(double v) {
		return Double.toHexString(v);
	}

	static String visibility(RocketComponentShapes s) {
		return s.color == ORColor.INVISIBLE ? "i" : "v";
	}

	static String shapeLetter(Shape s) {
		if (s instanceof RoundRectangle2D) {
			return "RR";
		} else if (s instanceof Rectangle2D) {
			return "R";
		} else if (s instanceof Ellipse2D) {
			return "E";
		} else if (s instanceof Arc2D a) {
			return "A" + a.getArcType();
		} else if (s instanceof Line2D) {
			return "N";
		} else if (s instanceof Path2D.Float) {
			return "PF";
		} else if (s instanceof Path2D) {
			return "PD";
		}
		return "?";
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
		if (s instanceof RoundRectangle2D.Double rr) {
			b.append("RR ").append(h(rr.x)).append(' ').append(h(rr.y)).append(' ').append(h(rr.width)).append(' ')
					.append(h(rr.height)).append(' ').append(h(rr.arcwidth)).append(' ').append(h(rr.archeight));
		} else if (s instanceof Rectangle2D.Double re) {
			b.append("R ").append(h(re.x)).append(' ').append(h(re.y)).append(' ').append(h(re.width)).append(' ')
					.append(h(re.height));
		} else if (s instanceof Ellipse2D.Double e) {
			b.append("E ").append(h(e.x)).append(' ').append(h(e.y)).append(' ').append(h(e.width)).append(' ')
					.append(h(e.height));
		} else if (s instanceof Arc2D.Double a) {
			b.append("A ").append(h(a.x)).append(' ').append(h(a.y)).append(' ').append(h(a.width)).append(' ')
					.append(h(a.height)).append(' ').append(h(a.start)).append(' ').append(h(a.extent)).append(' ')
					.append(a.getArcType());
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
