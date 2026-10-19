package info.qtrocket.goldens;

import java.awt.Shape;
import java.awt.geom.Arc2D;
import java.awt.geom.Ellipse2D;
import java.awt.geom.Line2D;
import java.awt.geom.Path2D;
import java.awt.geom.PathIterator;
import java.awt.geom.Rectangle2D;
import java.awt.geom.RoundRectangle2D;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import info.openrocket.core.motor.Motor;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.InstanceContext;
import info.openrocket.core.rocketcomponent.InstanceMap;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.util.BoundingBox;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.LineStyle;
import info.openrocket.core.util.ORColor;
import info.openrocket.core.util.Transformation;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapeProvider;
import info.openrocket.swing.gui.rocketfigure.RocketComponentShapes;

/**
 * figure.json: what OpenRocket's 2D rocket figure (the Swing class
 * {@code info.openrocket.swing.gui.scalefigure.RocketFigure}) draws of the rocket's selected flight
 * configuration in the {@link #VIEWS}: the subject bounds, the shapes of every instance and the motors,
 * all in the figure's coordinates (metres, y up, before the projection to pixels).
 * <p>
 * RocketFigure itself needs Swing's preferences and a component, so its geometry half is replicated here
 * line by line: {@code updateSubjectDimensions()}, {@code updateShapes()} with
 * {@code addShapesFromInstanceEntries()} and {@code addThisShape()}, and the shapes of
 * {@code drawMotors()}. The shapes themselves come from OpenRocket's own shape providers
 * ({@code info.openrocket.swing.gui.rocketfigure}, compiled read-only from the checkout's Swing sources
 * by build.gradle), through {@link RocketComponentShapeProvider} as RocketFigure calls it. The
 * preference "only show pod set/booster markers when selected" ({@code isShowMarkers()}) is off, as in
 * a new installation, and nothing is selected, so every pod set and booster set draws its marker.
 * <p>
 * Two orders of RocketFigure are not reproducible and are replaced by stable ones: its shapes sit in a
 * PriorityQueue ordered by display order alone, fed from InstanceMaps, which are hash maps (keyed by
 * component ids); and {@code getActiveMotors()} iterates a hash map too. Instances are therefore written
 * in tree order of their components (the active instances, then the extra render instances), a
 * component's instances in the order of its InstanceContexts, an instance's shapes in the provider's
 * order, each instance with the display order RocketFigure sorts it by; motors by the tree order of
 * their mounts and then by the mount's location. See README.md for the format.
 */
final class FigureDumper {

	/** A view of the figure: the view type and the rotation about the rocket's axis, in radians. */
	record View(String name, double rotation) {
	}

	/**
	 * The views dumped: side, top and back at rotation 0 (RocketFigure's three 2D view types), and the
	 * side view at 30 degrees (the rotation the snapping of the rotation control gives first) and at 225
	 * degrees (the third quadrant: sine and cosine both negative, the figure seen from below and behind).
	 */
	static final List<View> VIEWS = List.of(new View("side", 0.0), new View("top", 0.0), new View("back", 0.0),
			new View("side", Math.PI / 6), new View("side", 5 * Math.PI / 4));

	/**
	 * RocketFigure's preference "only show pod set/booster markers when selected"
	 * ({@code ApplicationPreferences.isShowMarkers()}): off, its default.
	 */
	static final boolean SHOW_MARKERS = false;

	private FigureDumper() {
	}

	static Map<String, Object> dump(String inputName, Rocket rocket, ComponentIndex index) {
		Map<String, Object> root = Json.object();
		root.put("schema", "figure");
		root.put("schemaVersion", GoldenDumper.SCHEMA_VERSION);
		root.put("input", inputName);

		FlightConfiguration config = rocket.getSelectedConfiguration();
		Map<String, Object> configuration = Json.object();
		GeometryDumper.putConfigurationHeader(configuration, configurationIndex(rocket, config), config);
		root.put("configuration", configuration);
		root.put("showMarkers", SHOW_MARKERS);
		root.put("selection", Json.array());

		Map<RocketComponent, Integer> treeIndex = new HashMap<>();
		for (RocketComponent c : index.components()) {
			treeIndex.put(c, treeIndex.size());
		}
		List<Object> views = Json.array();
		for (View view : VIEWS) {
			views.add(view(view, config, index, treeIndex));
		}
		root.put("views", views);
		return root;
	}

	/** The index of {@code config} among the rocket's flight configurations, the default first. */
	private static int configurationIndex(Rocket rocket, FlightConfiguration config) {
		int i = 0;
		for (FlightConfiguration c : rocket.getFlightConfigurations()) {
			if (c == config) {
				return i;
			}
			i++;
		}
		throw new IllegalStateException("The selected configuration is none of the rocket's");
	}

	private static boolean isBack(View view) {
		return view.name().equals("back");
	}

	/** RocketFigure.getFigureRotation(). */
	private static Transformation figureRotation(View view) {
		Transformation axialRotation = Transformation.rotate_x(view.rotation());
		if (view.name().equals("top")) {
			return axialRotation.applyTransformation(Transformation.rotate_x(-Math.PI / 2));
		}
		return axialRotation;
	}

	private static Map<String, Object> view(View view, FlightConfiguration config, ComponentIndex index,
			Map<RocketComponent, Integer> treeIndex) {
		Map<String, Object> o = Json.object();
		o.put("view", view.name());
		o.put("rotation", view.rotation());
		// The rotation from the rocket's frame into the figure's, which every instance's transformation
		// and the motors go through: it shows whether a port's sine and cosine of the rotation are Java's.
		Transformation rotation = figureRotation(view);
		o.put("figureRotation", Values.transformation(rotation));

		// updateSubjectDimensions()
		final BoundingBox bounds = config.getBoundingBox();
		final double maxR = Math.max(Math.hypot(bounds.min.getY(), bounds.min.getZ()),
				Math.hypot(bounds.max.getY(), bounds.max.getZ()));
		Rectangle2D subject;
		if (isBack(view)) {
			subject = new Rectangle2D.Double(-maxR, -maxR, 2 * maxR, 2 * maxR);
		} else {
			subject = new Rectangle2D.Double(bounds.min.getX(), -maxR, bounds.span().getX(), 2 * maxR);
		}
		o.put("subjectBounds", Json.numbers(subject.getX(), subject.getY(), subject.getWidth(), subject.getHeight()));

		// updateShapes(): the active instances, then the extra render instances
		List<Object> instances = Json.array();
		addShapesFromInstanceEntries(instances, config.getActiveInstances(), false, view, rotation, index);
		addShapesFromInstanceEntries(instances, config.getExtraRenderInstances(), true, view, rotation, index);
		o.put("instances", instances);

		o.put("motors", motors(config, view, rotation, index, treeIndex));
		return o;
	}

	/**
	 * RocketFigure.addShapesFromInstanceEntries() and addThisShape() for the instances of {@code map}, in
	 * tree order (see the class comment), with the preference isShowMarkers() off.
	 */
	private static void addShapesFromInstanceEntries(List<Object> instances, InstanceMap map, boolean extraRender,
			View view, Transformation figureRotation, ComponentIndex index) {
		int listed = 0;
		for (RocketComponent comp : index.components()) {
			List<InstanceContext> contextList = map.get(comp);
			if (contextList == null) {
				continue;
			}
			listed++;
			if (SHOW_MARKERS) {
				throw new IllegalStateException("isShowMarkers() on is not dumped");
			}
			for (int j = 0; j < contextList.size(); j++) {
				InstanceContext context = contextList.get(j);
				final Transformation currentTransform = figureRotation.applyTransformation(context.transform);
				// addThisShape(): no shapes for the rocket and the plain stages
				if ((comp instanceof Rocket) || (comp instanceof AxialStage && !(comp instanceof ParallelStage))) {
					continue;
				}
				RocketComponentShapes[] shapes = isBack(view)
						? RocketComponentShapeProvider.getShapesBack(comp, currentTransform)
						: RocketComponentShapeProvider.getShapesSide(comp, currentTransform);
				instances.add(instance(comp, extraRender, j, context, view, shapes, index));
			}
		}
		if (listed != map.size()) {
			throw new IllegalStateException("An instance map holds a component that is not in the rocket's tree");
		}
	}

	/**
	 * One instance: its component, which map and context it is, the display order RocketFigure's
	 * PriorityQueue sorts it by (the side order in the side and top views, the back order in the back
	 * view), the component's colour and line style (which every shape of the component carries, but for
	 * the invisible ones, whose colour is ORColor.INVISIBLE) and the shapes.
	 */
	private static Map<String, Object> instance(RocketComponent comp, boolean extraRender, int contextIndex,
			InstanceContext context, View view, RocketComponentShapes[] shapes, ComponentIndex index) {
		Map<String, Object> o = Json.object();
		o.put("path", index.path(comp));
		o.put("extraRender", extraRender);
		o.put("context", contextIndex);
		o.put("instanceNumber", context.instanceNumber);
		o.put("displayOrder", isBack(view) ? comp.getDisplayOrder_back() : comp.getDisplayOrder_side());
		ORColor color = comp.getColor();
		LineStyle lineStyle = comp.getLineStyle();
		o.put("color", color == null ? null
				: List.<Object>of(color.getRed(), color.getGreen(), color.getBlue(), color.getAlpha()));
		o.put("lineStyle", lineStyle == null ? null : lineStyle.name());
		List<Object> list = Json.array();
		for (RocketComponentShapes s : shapes) {
			boolean visible = s.color != ORColor.INVISIBLE;
			if (s.component != comp || s.lineStyle != lineStyle || (visible && s.color != color)) {
				throw new IllegalStateException("A shape of " + index.path(comp)
						+ " does not carry its component, colour and line style");
			}
			list.add(shape(s.shape, visible));
		}
		o.put("shapes", list);
		return o;
	}

	/**
	 * The shapes of RocketFigure.drawMotors(): for every active motor whose mount is visible, at each of
	 * the mount's component locations, a Rectangle2D (side and top views) or an Ellipse2D (back view);
	 * sorted by the tree order of the mounts.
	 */
	private static List<Object> motors(FlightConfiguration config, View view, Transformation figureRotation,
			ComponentIndex index, Map<RocketComponent, Integer> treeIndex) {
		List<MotorConfiguration> active = new ArrayList<>(config.getActiveMotors());
		active.sort((a, b) -> Integer.compare(treeIndex.get((RocketComponent) a.getMount()),
				treeIndex.get((RocketComponent) b.getMount())));
		List<Object> list = Json.array();
		for (MotorConfiguration curInstance : active) {
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
				curMotorLocation = figureRotation.transform(curMotorLocation);
				Shape s;
				if (!isBack(view)) {
					s = new Rectangle2D.Double(curMotorLocation.getX(), (curMotorLocation.getY() - motorRadius),
							motorLength, 2 * motorRadius);
				} else {
					s = new Ellipse2D.Double((curMotorLocation.getZ() - motorRadius),
							(curMotorLocation.getY() - motorRadius), 2 * motorRadius, 2 * motorRadius);
				}
				Map<String, Object> o = Json.object();
				o.put("mount", index.path((RocketComponent) mount));
				o.put("location", k);
				o.put("shape", shape(s, true));
				list.add(o);
			}
		}
		return list;
	}

	/**
	 * A shape by its primitive's parameters, or a path by its segments (see README.md). OpenRocket's
	 * providers make only these classes (a rotated rail button is AffineTransform.createTransformedShape's
	 * Path2D.Double); anything else fails the dump.
	 */
	static Map<String, Object> shape(Shape shape, boolean visible) {
		Map<String, Object> o = Json.object();
		if (shape instanceof RoundRectangle2D.Double r) {
			o.put("type", "roundRect");
			o.put("frame", Json.numbers(r.x, r.y, r.width, r.height));
			o.put("arc", Json.numbers(r.arcwidth, r.archeight));
		} else if (shape instanceof Rectangle2D.Double r) {
			o.put("type", "rect");
			o.put("frame", Json.numbers(r.x, r.y, r.width, r.height));
		} else if (shape instanceof Ellipse2D.Double e) {
			o.put("type", "ellipse");
			o.put("frame", Json.numbers(e.x, e.y, e.width, e.height));
		} else if (shape instanceof Arc2D.Double a) {
			o.put("type", "arc");
			o.put("frame", Json.numbers(a.x, a.y, a.width, a.height));
			o.put("angles", Json.numbers(a.start, a.extent));
			o.put("arcType", arcType(a.getArcType()));
		} else if (shape instanceof Line2D.Double l) {
			o.put("type", "line");
			o.put("points", Json.numbers(l.x1, l.y1, l.x2, l.y2));
		} else if (shape instanceof Path2D p) {
			o.put("type", "path");
			o.put("precision", p instanceof Path2D.Float ? "float" : "double");
			o.put("winding", switch (p.getWindingRule()) {
				case Path2D.WIND_NON_ZERO -> "nonZero";
				case Path2D.WIND_EVEN_ODD -> "evenOdd";
				default -> throw new IllegalStateException("Unknown winding rule " + p.getWindingRule());
			});
			pathSegments(p, o);
		} else {
			throw new IllegalStateException("A shape of an unexpected class: " + shape.getClass().getName());
		}
		o.put("visible", visible);
		return o;
	}

	private static String arcType(int type) {
		return switch (type) {
			case Arc2D.OPEN -> "open";
			case Arc2D.CHORD -> "chord";
			case Arc2D.PIE -> "pie";
			default -> throw new IllegalStateException("Unknown arc type " + type);
		};
	}

	/**
	 * The segments of {@code path} (its PathIterator without a transformation): {@code segments} is the
	 * sequence of the segment letters M (moveTo), L (lineTo), Q (quadTo), C (cubicTo) and Z (closePath),
	 * a run of one letter written as the letter and its length when longer than one ("ML182Z": a move, 182
	 * lines and a close); {@code coordinates} the x and y of every point of every segment in their order
	 * (one point for M and L, two for Q, three for C, none for Z). A Path2D.Float's coordinates are its
	 * floats, written as the doubles they are.
	 */
	private static void pathSegments(Path2D path, Map<String, Object> o) {
		StringBuilder letters = new StringBuilder();
		List<Object> coordinates = Json.array();
		char run = 0;
		int runLength = 0;
		double[] c = new double[6];
		for (PathIterator it = path.getPathIterator(null); !it.isDone(); it.next()) {
			int type = it.currentSegment(c);
			char letter;
			int points;
			switch (type) {
				case PathIterator.SEG_MOVETO -> {
					letter = 'M';
					points = 1;
				}
				case PathIterator.SEG_LINETO -> {
					letter = 'L';
					points = 1;
				}
				case PathIterator.SEG_QUADTO -> {
					letter = 'Q';
					points = 2;
				}
				case PathIterator.SEG_CUBICTO -> {
					letter = 'C';
					points = 3;
				}
				case PathIterator.SEG_CLOSE -> {
					letter = 'Z';
					points = 0;
				}
				default -> throw new IllegalStateException("Unknown path segment " + type);
			}
			if (letter == run) {
				runLength++;
			} else {
				appendRun(letters, run, runLength);
				run = letter;
				runLength = 1;
			}
			for (int i = 0; i < 2 * points; i++) {
				coordinates.add(c[i]);
			}
		}
		appendRun(letters, run, runLength);
		o.put("segments", letters.toString());
		o.put("coordinates", coordinates);
	}

	private static void appendRun(StringBuilder letters, char letter, int length) {
		if (length == 0) {
			return;
		}
		letters.append(letter);
		if (length > 1) {
			letters.append(length);
		}
	}
}
