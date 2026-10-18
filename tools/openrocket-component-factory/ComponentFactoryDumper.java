import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

import javax.swing.tree.DefaultTreeSelectionModel;
import javax.swing.tree.TreeSelectionModel;

import com.google.inject.Guice;
import com.google.inject.Injector;

import info.openrocket.core.database.Databases;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.file.GeneralRocketSaver;
import info.openrocket.core.material.Material;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Bulkhead;
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.ClusterConfiguration;
import info.openrocket.core.rocketcomponent.EllipticalFinSet;
import info.openrocket.core.rocketcomponent.EngineBlock;
import info.openrocket.core.rocketcomponent.ExternalComponent;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.PodSet;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.ShockCord;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.TubeCoupler;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.main.ComponentAddButtons;
import info.openrocket.swing.gui.main.ComponentButton;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * Writes tests/data/component-factory/: what OpenRocket makes when the user adds a component
 * through one of the add buttons of its design window (ComponentAddButtons, ComponentButton),
 * for the tests of QtRocket's rocket/ComponentFactory.
 *
 * In the Guice set-up of swing's ServicesForTesting (its in-memory SwingPreferences, its English
 * translator) and PluginModule, it prints to standard output:
 * <ol>
 * <li>"defaults": every value Java's constructors give a new component of each of the 21 classes
 * an add button makes, read through the getters (and two private fields, which no getter
 * reads), after SwingPreferences.loadDefaultComponentMaterials(), which OpenRocket's start
 * (SwingStartup) runs;</li>
 * <li>"materials": the materials of a new component of each class with three sets of preferences:
 * none stored, loadDefaultComponentMaterials(), and a set of defaults stored for classes along
 * the class chains;</li>
 * <li>"clicks": 25 clicks on the add buttons (the scenario of the M2 scout's AddProbe), each with
 * its undo text and where the new component went, and the tree they build.</li>
 * </ol>
 * The design of the clicks is saved with GeneralRocketSaver; its rocket.ork (the plain XML of the
 * archive) is written, byte for byte, to {@code <output directory>/add-buttons-rocket.ork.xml}.
 *
 * Usage: java ComponentFactoryDumper <output directory>
 *
 * Run it through generate.sh, which compiles it with swing's ServicesForTesting against
 * OpenRocket's compiled core and swing classes.
 */
public final class ComponentFactoryDumper {
	private ComponentFactoryDumper() {
	}

	/** The classes of the add buttons, in the order of ComponentAddButtons. */
	static final List<Class<? extends RocketComponent>> BUTTON_CLASSES = List.of(AxialStage.class,
			ParallelStage.class, PodSet.class, NoseCone.class, BodyTube.class, Transition.class,
			TrapezoidFinSet.class, EllipticalFinSet.class, FreeformFinSet.class, TubeFinSet.class,
			RailButton.class, LaunchLug.class, InnerTube.class, TubeCoupler.class, CenteringRing.class,
			Bulkhead.class, EngineBlock.class, Parachute.class, Streamer.class, ShockCord.class,
			MassComponent.class);

	/**
	 * What every component is asked. Not getOverrideMass() and getOverrideCGX(): while nothing is
	 * overridden they give the component's own mass and CG, computed, not a value a constructor sets.
	 */
	static final List<String> COMMON = List.of("getName", "getComponentName", "getLength", "getAxialMethod",
			"getAxialOffset", "getInstanceCount", "getChildCount", "isMassOverridden", "isCGOverridden",
			"isCDOverridden", "getOverrideCD",
			"isSubcomponentsOverriddenMass", "isSubcomponentsOverriddenCG", "isSubcomponentsOverriddenCD",
			"getComment", "isVisible", "getColor", "getLineStyle", "getAppearance", "getPresetComponent",
			"isMotorMount", "getDisplayOrder_side", "getDisplayOrder_back");

	static final List<String> EXTERNAL = List.of("getMaterial", "getFinish");

	static final List<String> SYMMETRIC = List.of("getThickness", "isFilled");

	static final List<String> TRANSITION = List.of("getShapeType", "getShapeParameter", "isClipped",
			"getForeRadius", "isForeRadiusAutomatic", "getAftRadius", "isAftRadiusAutomatic",
			"getForeShoulderRadius", "getForeShoulderLength", "getForeShoulderThickness", "isForeShoulderCapped",
			"getAftShoulderRadius", "getAftShoulderLength", "getAftShoulderThickness", "isAftShoulderCapped");

	static final List<String> MOUNT = List.of("getMotorOverhang", "getDefaultMotorConfig.getIgnitionEvent",
			"getDefaultMotorConfig.getIgnitionDelay", "getDefaultMotorConfig.getEjectionDelay",
			"getMotorConfigurationSet.size");

	static final List<String> FIN_SET = List.of("getFinCount", "getBaseRotation", "getAngleOffset",
			"getAngleMethod", "getRadiusMethod", "getRadiusOffset", "getCantAngle", "getThickness",
			"getCrossSection", "getTabHeight", "getTabLength", "getTabOffset", "getTabOffsetMethod",
			"getFilletRadius", "getFilletMaterial");

	static final List<String> ASSEMBLY = List.of("getAngleMethod", "getAngleOffset", "getRadiusMethod",
			"getRadiusOffset");

	static final List<String> STAGE = List.of("getStageNumber",
			"getSeparationConfigurations.getDefault.getSeparationEvent",
			"getSeparationConfigurations.getDefault.getSeparationDelay",
			"getSeparationConfigurations.getDefault.getSeparationAltitude", "getSeparationConfigurations.size");

	static final List<String> RING = List.of("getOuterRadius", "isOuterRadiusAutomatic", "getInnerRadius",
			"isInnerRadiusAutomatic", "getThickness", "getRadialPosition", "getRadialDirection", "getMaterial");

	static final List<String> MASS_OBJECT = List.of("getRadius", "isRadiusAutomatic", "getRadialPosition",
			"getRadialDirection");

	static final List<String> RECOVERY = List.of("getCD", "isCDAutomatic", "isDrogue", "getMaterial",
			"field:defaultMaterial", "getDeploymentConfigurations.getDefault.getDeployEvent",
			"getDeploymentConfigurations.getDefault.getDeployAltitude",
			"getDeploymentConfigurations.getDefault.getDeployDelay", "getDeploymentConfigurations.size");

	/** What the components of a class are asked besides COMMON. */
	static Map<Class<?>, List<List<String>>> asked() {
		Map<Class<?>, List<List<String>>> m = new LinkedHashMap<>();
		m.put(AxialStage.class, List.of(STAGE));
		m.put(ParallelStage.class, List.of(STAGE, ASSEMBLY));
		m.put(PodSet.class, List.of(ASSEMBLY));
		m.put(NoseCone.class, List.of(EXTERNAL, SYMMETRIC, TRANSITION, List.of("isFlipped")));
		m.put(BodyTube.class, List.of(EXTERNAL, SYMMETRIC, List.of("getOuterRadius", "isOuterRadiusAutomatic",
				"getForeRadius", "isForeRadiusAutomatic", "getAftRadius", "isAftRadiusAutomatic"), MOUNT));
		m.put(Transition.class, List.of(EXTERNAL, SYMMETRIC, TRANSITION));
		m.put(TrapezoidFinSet.class, List.of(EXTERNAL, FIN_SET, List.of("getRootChord", "getTipChord", "getHeight",
				"getSweep", "getFinPoints")));
		// The points of an elliptical fin are cosines and sines: not asked.
		m.put(EllipticalFinSet.class, List.of(EXTERNAL, FIN_SET, List.of("getHeight")));
		m.put(FreeformFinSet.class, List.of(EXTERNAL, FIN_SET, List.of("getFinPoints")));
		m.put(TubeFinSet.class, List.of(EXTERNAL, List.of("getFinCount", "getOuterRadius", "isOuterRadiusAutomatic",
				"getThickness", "getBaseRotation", "getAngleOffset", "getAngleMethod", "getRadiusMethod",
				"getRadiusOffset")));
		m.put(RailButton.class, List.of(EXTERNAL, List.of("getOuterDiameter", "getInnerDiameter", "getTotalHeight",
				"getFlangeHeight", "getBaseHeight", "getScrewHeight", "getInstanceSeparation", "getAngleOffset",
				"getAngleMethod", "getRadiusMethod", "getRadiusOffset")));
		m.put(LaunchLug.class, List.of(EXTERNAL, List.of("getOuterRadius", "getInnerRadius", "getThickness",
				"getInstanceSeparation", "getAngleOffset", "getAngleMethod", "getRadiusMethod", "getRadiusOffset")));
		m.put(InnerTube.class, List.of(RING, List.of("getClusterConfiguration", "getClusterScale",
				"getClusterRotation"), MOUNT));
		m.put(TubeCoupler.class, List.of(RING));
		m.put(CenteringRing.class, List.of(RING, List.of("getInstanceSeparation")));
		m.put(Bulkhead.class, List.of(RING, List.of("getInstanceSeparation")));
		m.put(EngineBlock.class, List.of(RING));
		m.put(Parachute.class, List.of(MASS_OBJECT, RECOVERY, List.of("getDiameter", "getLineCount",
				"getLineLength", "isLineLengthAutomatic", "getLineMaterial", "field:DEFAULT_LINE_MATERIAL")));
		m.put(Streamer.class, List.of(MASS_OBJECT, RECOVERY, List.of("getStripLength", "getStripWidth")));
		m.put(ShockCord.class, List.of(MASS_OBJECT, List.of("getCordLength", "isCordLengthAutomatic",
				"getMaterial")));
		m.put(MassComponent.class, List.of(MASS_OBJECT, List.of("getComponentMass", "getMassComponentType")));
		return m;
	}

	/** The value of @p v as the report writes it. */
	static String show(Object v) {
		if (v == null) {
			return "null";
		}
		if (v instanceof Material m) {
			return m.toStorableString();
		}
		if (v instanceof String s) {
			return "\"" + s + "\"";
		}
		if (v instanceof Enum<?> e) {
			return e.name();
		}
		if (v instanceof ClusterConfiguration c) {
			return c.getXMLName();
		}
		if (v instanceof CoordinateIF[] points) {
			StringBuilder sb = new StringBuilder();
			for (CoordinateIF p : points) {
				if (sb.length() > 0) {
					sb.append(' ');
				}
				sb.append('(').append(p.getX()).append(',').append(p.getY()).append(',').append(p.getZ()).append(')');
			}
			return sb.toString();
		}
		return v.toString();
	}

	/** Follows @p path ("getA.getB", or "field:name" for a private field) from @p o. */
	static Object ask(Object o, String path) throws ReflectiveOperationException {
		if (path.startsWith("field:")) {
			String name = path.substring("field:".length());
			for (Class<?> c = o.getClass(); c != null; c = c.getSuperclass()) {
				try {
					Field f = c.getDeclaredField(name);
					f.setAccessible(true);
					return f.get(o);
				} catch (NoSuchFieldException ignore) {
					// further up
				}
			}
			throw new NoSuchFieldException(name);
		}
		Object v = o;
		for (String step : path.split("\\.")) {
			Method m = v.getClass().getMethod(step);
			m.setAccessible(true);
			v = m.invoke(v);
		}
		return v;
	}

	static void printDefaults(RocketComponent c, Map<Class<?>, List<List<String>>> asked)
			throws ReflectiveOperationException {
		System.out.println("-- " + c.getClass().getSimpleName());
		for (String path : COMMON) {
			System.out.println(path + " = " + show(ask(c, path)));
		}
		for (List<String> group : asked.get(c.getClass())) {
			for (String path : group) {
				System.out.println(path + " = " + show(ask(c, path)));
			}
		}
	}

	static void printMaterials(String title) throws ReflectiveOperationException {
		System.out.println("-- " + title);
		for (Class<? extends RocketComponent> cls : BUTTON_CLASSES) {
			RocketComponent c = cls.getConstructor().newInstance();
			StringBuilder sb = new StringBuilder(cls.getSimpleName() + ":");
			for (String path : List.of("getMaterial", "getFilletMaterial", "field:defaultMaterial", "getLineMaterial",
					"field:DEFAULT_LINE_MATERIAL")) {
				try {
					Object v = ask(c, path);
					sb.append(' ').append(path).append('=').append(show(v));
				} catch (NoSuchMethodException | NoSuchFieldException ignore) {
					// the class has none
				}
			}
			System.out.println(sb);
		}
	}

	static Material find(Material.Type type, String name) {
		Material m = Databases.findMaterial(type, name);
		if (m == null || !m.getName().equals(name)) {
			throw new IllegalStateException("no built-in material " + name);
		}
		return m;
	}

	/** Defaults stored along the class chains (see the README). */
	static void storeChainDefaults(ApplicationPreferences prefs) {
		Map<Class<? extends RocketComponent>, Material> stored = new LinkedHashMap<>();
		stored.put(ExternalComponent.class, find(Material.Type.BULK, "Aluminum"));
		stored.put(Transition.class, find(Material.Type.BULK, "Fiberglass"));
		stored.put(RingComponent.class, find(Material.Type.BULK, "Kraft phenolic"));
		stored.put(CenteringRing.class, find(Material.Type.BULK, "Plywood (birch)"));
		stored.put(RecoveryDevice.class, find(Material.Type.SURFACE, "Silk"));
		stored.put(Parachute.class, find(Material.Type.SURFACE, "Mylar"));
		stored.put(MassObject.class, find(Material.Type.LINE, "Tubular nylon (11 mm, 7/16 in)"));
		System.out.println("-- stored along the class chains");
		for (Map.Entry<Class<? extends RocketComponent>, Material> e : stored.entrySet()) {
			prefs.setDefaultComponentMaterial(e.getKey(), e.getValue());
			System.out.println("stored " + e.getKey().getSimpleName() + " = " + e.getValue().toStorableString());
		}
	}

	// ---------------------------------------------------------------- the clicks (after AddProbe)

	static OpenRocketDocument doc;
	static DefaultTreeSelectionModel tsm;
	static Map<String, ComponentButton> buttons = new LinkedHashMap<>();

	static void build() throws ReflectiveOperationException {
		doc = OpenRocketDocumentFactory.createNewRocket();
		tsm = new DefaultTreeSelectionModel();
		tsm.setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
		ComponentAddButtons panel = new ComponentAddButtons(doc, tsm, null);
		Field f = ComponentAddButtons.class.getDeclaredField("buttons");
		f.setAccessible(true);
		ComponentButton[][] rows = (ComponentButton[][]) f.get(panel);
		Field cls = ComponentButton.class.getDeclaredField("componentClass");
		cls.setAccessible(true);
		buttons.clear();
		for (ComponentButton[] row : rows) {
			if (row == null) {
				continue;
			}
			for (ComponentButton b : row) {
				buttons.put(((Class<?>) cls.get(b)).getSimpleName(), b);
			}
		}
	}

	static String tree(RocketComponent c) {
		StringBuilder sb = new StringBuilder(c.getName());
		if (c.getChildCount() > 0) {
			sb.append('[');
			for (int i = 0; i < c.getChildCount(); i++) {
				if (i > 0) {
					sb.append(", ");
				}
				sb.append(tree(c.getChild(i)));
			}
			sb.append(']');
		}
		return sb.toString();
	}

	/** Clicks the button of @p button with @p selected selected. */
	static void click(String button, RocketComponent selected) {
		tsm.setSelectionPath(ComponentTreeModel.makeTreePath(selected));
		ComponentButton b = buttons.get(button);
		if (!b.isEnabled()) {
			throw new IllegalStateException(button + " is disabled on " + selected.getName());
		}
		try {
			b.doClick(0);
		} catch (java.awt.HeadlessException e) {
			// ComponentConfigDialog.showDialog(), after the component was added and selected
		}
		RocketComponent sel = (RocketComponent) tsm.getSelectionPath().getLastPathComponent();
		System.out.println(button + " on " + selected.getName() + ": undo='" + doc.getUndoDescription() + "' selected="
				+ sel.getName() + " parent=" + sel.getParent().getName() + "@" + sel.getParent().getChildPosition(sel));
	}

	static void clicks(Path out) throws Exception {
		ApplicationPreferences prefs = Application.getPreferences();
		prefs.putInt(ApplicationPreferences.BODY_COMPONENT_INSERT_POSITION_KEY, 2);
		prefs.putInt(ApplicationPreferences.STAGE_INSERT_POSITION_KEY, 2);
		build();
		Rocket rocket = doc.getRocket();
		click("NoseCone", rocket.getChild(0));
		click("BodyTube", rocket.getChild(0).getChild(0));
		RocketComponent bt = rocket.getChild(0).getChild(1);
		click("Transition", bt);
		click("BodyTube", rocket.getChild(0).getChild(2));
		for (String b : new String[] { "TrapezoidFinSet", "EllipticalFinSet", "FreeformFinSet", "TubeFinSet",
				"RailButton", "LaunchLug", "InnerTube", "TubeCoupler", "CenteringRing", "Bulkhead", "EngineBlock",
				"Parachute", "Streamer", "ShockCord", "MassComponent", "PodSet", "ParallelStage" }) {
			click(b, bt);
		}
		click("AxialStage", rocket);
		click("BodyTube", rocket.getChild(1));
		RocketComponent pods = null;
		RocketComponent boosters = null;
		for (RocketComponent c : bt.getChildren()) {
			if (c instanceof PodSet) {
				pods = c;
			}
			if (c instanceof ParallelStage) {
				boosters = c;
			}
		}
		click("BodyTube", pods);
		click("NoseCone", boosters);
		System.out.println("tree=" + tree(rocket));

		File archive = File.createTempFile("add-buttons", ".ork");
		try {
			new GeneralRocketSaver().save(archive, doc);
			try (ZipFile zip = new ZipFile(archive)) {
				ZipEntry entry = zip.getEntry("rocket.ork");
				try (InputStream in = zip.getInputStream(entry)) {
					Files.write(out.resolve("add-buttons-rocket.ork.xml"), in.readAllBytes());
				}
			}
		} finally {
			Files.delete(archive.toPath());
		}
	}

	public static void main(String[] args) throws Exception {
		if (args.length != 1) {
			System.err.println("usage: java ComponentFactoryDumper <output directory>");
			System.exit(2);
		}
		Path out = Path.of(args[0]);
		Files.createDirectories(out);
		Injector injector = Guice.createInjector(new ServicesForTesting(), new PluginModule());
		Application.setInjector(injector);
		SwingPreferences prefs = (SwingPreferences) Application.getPreferences();
		java.util.prefs.Preferences materialNode = prefs.getNode("componentMaterials");
		materialNode.clear();

		System.out.println("== materials");
		printMaterials("preferences: none stored");
		prefs.loadDefaultComponentMaterials();
		printMaterials("preferences: loadDefaultComponentMaterials()");
		materialNode.clear();
		storeChainDefaults(prefs);
		printMaterials("preferences: defaults along the class chains");
		materialNode.clear();

		prefs.loadDefaultComponentMaterials();
		System.out.println("== defaults (after loadDefaultComponentMaterials())");
		Map<Class<?>, List<List<String>>> asked = asked();
		for (Class<? extends RocketComponent> cls : BUTTON_CLASSES) {
			printDefaults(cls.getConstructor().newInstance(), asked);
		}

		System.out.println("== clicks (both insert preferences 'add to the end')");
		clicks(out);
	}
}
