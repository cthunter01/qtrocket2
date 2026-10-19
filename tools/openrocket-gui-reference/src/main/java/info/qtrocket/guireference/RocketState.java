package info.qtrocket.guireference;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.UUID;

import org.xml.sax.InputSource;

import info.openrocket.core.appearance.Appearance;
import info.openrocket.core.appearance.Decal;
import info.openrocket.core.appearance.DecalImage;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.file.DatabaseMotorFinder;
import info.openrocket.core.file.DocumentLoadingContext;
import info.openrocket.core.file.openrocket.importt.OpenRocketHandler;
import info.openrocket.core.file.simplesax.SimpleSAX;
import info.openrocket.core.logging.WarningSet;
import info.openrocket.core.masscalc.MassCalculator;
import info.openrocket.core.masscalc.RigidBody;
import info.openrocket.core.material.Material;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.preferences.DocumentPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration;
import info.openrocket.core.rocketcomponent.EllipticalFinSet;
import info.openrocket.core.rocketcomponent.ExternalComponent;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.InsideColorComponent;
import info.openrocket.core.rocketcomponent.InsideColorComponentHandler;
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.PodSet;
import info.openrocket.core.rocketcomponent.RadiusRingComponent;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.ShockCord;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.core.rocketcomponent.StructuralComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.core.rocketcomponent.position.AnglePositionable;
import info.openrocket.core.rocketcomponent.position.RadiusPositionable;
import info.openrocket.core.util.CoordinateIF;
import info.openrocket.core.util.ModID;
import info.openrocket.core.util.ORColor;

/**
 * The state of a rocket and of a loaded design file in the lines QtRocket's tests print for their
 * own: the Java probes of tiers 9b and 9c, from which tests/core/file/openrocket/
 * ComponentHandlerTestSupport.cpp (describeRocket()) and DesignFileState.cpp (designFileState())
 * were written, copied here line for line:
 * <ul>
 * <li>{@link #dump}: HandlerProbe.dump of tier 9b: every component in tree order with its class,
 * its name and what it holds (two blanks deeper per level), the decal images, the selected and every
 * flight configuration. An id is printed in full when it is among the known ones (or made of a hash),
 * else as "random".</li>
 * <li>{@link #loadedState}: RecordedProbe.run ("full") of tier 9c without the simulations (a document
 * made from scratch has none; the tool checks that) and ExampleProbe.documentPreferenceLines: the
 * version line, the warnings, the document lines, the change events of the load, the dump, the names
 * of the flight configurations, the motors, the decal images, the document preferences and
 * materials.</li>
 * <li>{@link #numbers}: RecordedProbe.numbers, asked of a load on which nothing else was asked.</li>
 * </ul>
 * The order of the getters is part of the state (an automatic dimension stores what it computes), as
 * in the probes.
 */
final class RocketState {

	private RocketState() {
	}

	// ------------------------------------------------------------------ HandlerProbe.dump of tier 9b

	/** The UUIDs a text spells out: every run of hexadecimal digits and dashes that UUID.fromString reads. */
	static Set<UUID> knownIds(String xml) {
		Set<UUID> known = new HashSet<>();
		int i = 0;
		while (i < xml.length()) {
			int j = i;
			while (j < xml.length() && isIdChar(xml.charAt(j))) {
				j++;
			}
			if (j > i) {
				String run = xml.substring(i, j);
				if (run.indexOf('-') >= 0) {
					try {
						known.add(UUID.fromString(run));
					} catch (IllegalArgumentException e) {
						// no id
					}
				}
				i = j;
			} else {
				i++;
			}
		}
		return known;
	}

	private static boolean isIdChar(char c) {
		return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
	}


	/** An id in full when the case's text spells it out or it is made of a hash, else "random". */
	private static String idText(UUID id, Set<UUID> known) {
		if (known.contains(id) || id.getMostSignificantBits() == 0) {
			return id.toString();
		}
		return "random";
	}

	private static String fcidText(FlightConfigurationId id, Set<UUID> known) {
		return idText(id.key, known);
	}

	private static String quote(String text) {
		return "'" + text.replace("\n", "\\n") + "'";
	}

	private static String material(Material m) {
		return m == null ? "null" : "[" + m.toStorableString() + "]";
	}

	private static String ignition(MotorConfiguration config) {
		return (config.getIgnitionEvent() == null ? "null" : config.getIgnitionEvent().name()) + ":"
				+ config.getIgnitionDelay() + ":" + config.hasIgnitionOverride();
	}

	private static String deployment(DeploymentConfiguration config) {
		return config.getDeployEvent().name() + ":" + config.getDeployDelay() + ":" + config.getDeployAltitude();
	}

	private static String separation(StageSeparationConfiguration config) {
		return config.getSeparationEvent().name() + ":" + config.getSeparationDelay() + ":"
				+ config.getSeparationAltitude();
	}

	/** One line for a component: its class and name, and "key=value" for what it holds. */
	private static String describe(RocketComponent c, Set<UUID> known) {
		StringBuilder sb = new StringBuilder();
		sb.append(c.getClass().getSimpleName()).append(' ').append(quote(c.getName()));
		if (known.contains(c.getID())) {
			sb.append(" id=").append(c.getID());
		}
		sb.append(" axial=").append(c.getAxialMethod().name()).append(':').append(c.getAxialOffset());
		sb.append(" x=").append(c.getPosition().getX());
		// Not of the rocket: its length is that of its bounds, and computing them reads every
		// component in the order of a hash map, which changes what automatic values store.
		if (!(c instanceof Rocket)) {
			sb.append(" len=").append(c.getLength());
		}
		if (!c.getComment().isEmpty()) {
			sb.append(" comment=").append(quote(c.getComment()));
		}
		ORColor color = c.getColor();
		if (color != null) {
			sb.append(" color=").append(color.getRed()).append(',').append(color.getGreen()).append(',')
					.append(color.getBlue()).append(',').append(color.getAlpha());
		}
		if (c.getLineStyle() != null) {
			sb.append(" linestyle=").append(c.getLineStyle().name());
		}
		if (c.getPresetComponent() != null) {
			sb.append(" preset=").append(c.getPresetComponent().getPartNo());
		}
		if (c.isMassOverridden()) {
			sb.append(" massovr=").append(c.getOverrideMass());
		}
		if (c.isCGOverridden()) {
			sb.append(" cgovr=").append(c.getOverrideCGX());
		}
		if (c.isCDOverridden()) {
			sb.append(" cdovr=").append(c.getOverrideCD());
		}
		if (c.isSubcomponentsOverriddenMass() || c.isSubcomponentsOverriddenCG() || c.isSubcomponentsOverriddenCD()) {
			sb.append(" subovr=").append(c.isSubcomponentsOverriddenMass()).append(',')
					.append(c.isSubcomponentsOverriddenCG()).append(',').append(c.isSubcomponentsOverriddenCD());
		}

		if (c instanceof Rocket r) {
			if (!r.getDesigner().isEmpty()) {
				sb.append(" designer=").append(quote(r.getDesigner()));
			}
			if (!r.getRevision().isEmpty()) {
				sb.append(" revision=").append(quote(r.getRevision()));
			}
			sb.append(" ref=").append(r.getReferenceType().name());
			sb.append(" customref=").append(r.getCustomReferenceLength());
			sb.append(" design=").append(r.getDesignType().name());
			if (!r.getKitName().isEmpty()) {
				sb.append(" kit=").append(quote(r.getKitName()));
			}
		}
		if (c instanceof AxialStage s) {
			sb.append(" stage=").append(s.getStageNumber());
			sb.append(" sep=").append(separation(s.getSeparationConfigurations().getDefault()));
			for (FlightConfigurationId id : s.getSeparationConfigurations().getIds()) {
				sb.append(" sep[").append(fcidText(id, known)).append("]=")
						.append(separation(s.getSeparationConfigurations().get(id)));
			}
		}
		if (c instanceof ParallelStage || c instanceof PodSet || c instanceof LaunchLug || c instanceof RailButton
				|| c instanceof FinSet || c instanceof TubeFinSet || c instanceof RadiusRingComponent) {
			sb.append(" inst=").append(c.getInstanceCount());
		}
		if (c instanceof RadiusPositionable rp) {
			sb.append(" radius=").append(rp.getRadiusMethod().name()).append(':').append(rp.getRadiusOffset());
		}
		if (c instanceof AnglePositionable ap) {
			sb.append(" angle=").append(ap.getAngleMethod().name()).append(':').append(ap.getAngleOffset());
		}
		if (c instanceof ExternalComponent e) {
			sb.append(" finish=").append(e.getFinish().name());
			sb.append(" mat=").append(material(e.getMaterial()));
		}
		if (c instanceof StructuralComponent s) {
			sb.append(" mat=").append(material(s.getMaterial()));
		}
		if (c instanceof BodyTube t) {
			sb.append(" r=").append(t.getOuterRadius()).append(':').append(t.isOuterRadiusAutomatic());
			sb.append(" thick=").append(t.getThickness()).append(':').append(t.isFilled());
		}
		if (c instanceof Transition t) {
			sb.append(" shape=").append(t.getShapeType().name()).append(':').append(t.getShapeParameter()).append(':')
					.append(t.isClipped());
			sb.append(" fore=").append(t.getForeRadius()).append(':').append(t.isForeRadiusAutomatic());
			sb.append(" aft=").append(t.getAftRadius()).append(':').append(t.isAftRadiusAutomatic());
			sb.append(" thick=").append(t.getThickness()).append(':').append(t.isFilled());
			sb.append(" foresh=").append(t.getForeShoulderRadius()).append(':').append(t.getForeShoulderLength())
					.append(':').append(t.getForeShoulderThickness()).append(':').append(t.isForeShoulderCapped());
			sb.append(" aftsh=").append(t.getAftShoulderRadius()).append(':').append(t.getAftShoulderLength())
					.append(':').append(t.getAftShoulderThickness()).append(':').append(t.isAftShoulderCapped());
		}
		if (c instanceof NoseCone n) {
			sb.append(" flipped=").append(n.isFlipped());
		}
		if (c instanceof FinSet f) {
			sb.append(" fins=").append(f.getFinCount());
			sb.append(" cant=").append(f.getCantAngle());
			sb.append(" thick=").append(f.getThickness());
			sb.append(" cross=").append(f.getCrossSection().name());
			sb.append(" tab=").append(f.getTabHeight()).append(':').append(f.getTabLength()).append(':')
					.append(f.getTabOffsetMethod().name()).append(':').append(f.getTabOffset()).append(':')
					.append(f.getTabFrontEdge());
			sb.append(" fillet=").append(f.getFilletRadius()).append(':').append(material(f.getFilletMaterial()));
		}
		if (c instanceof TrapezoidFinSet f) {
			sb.append(" chord=").append(f.getRootChord()).append(':').append(f.getTipChord());
			sb.append(" sweep=").append(f.getSweep());
			sb.append(" height=").append(f.getHeight());
		}
		if (c instanceof EllipticalFinSet f) {
			sb.append(" height=").append(f.getHeight());
		}
		if (c instanceof FreeformFinSet f) {
			sb.append(" points=");
			boolean first = true;
			for (CoordinateIF p : f.getFinPoints()) {
				sb.append(first ? "" : ";").append(p.getX()).append(',').append(p.getY());
				first = false;
			}
		}
		if (c instanceof TubeFinSet t) {
			sb.append(" fins=").append(t.getFinCount());
			sb.append(" autoradius=").append(t.isOuterRadiusAutomatic());
			sb.append(" thick=").append(t.getThickness());
		}
		if (c instanceof LaunchLug l) {
			sb.append(" r=").append(l.getOuterRadius());
			sb.append(" thick=").append(l.getThickness());
			sb.append(" spacing=").append(l.getInstanceSeparation());
		}
		if (c instanceof RailButton b) {
			sb.append(" diameter=").append(b.getOuterDiameter()).append(':').append(b.getInnerDiameter());
			sb.append(" height=").append(b.getTotalHeight()).append(':').append(b.getBaseHeight()).append(':')
					.append(b.getFlangeHeight()).append(':').append(b.getScrewHeight());
			sb.append(" spacing=").append(b.getInstanceSeparation());
		}
		if (c instanceof RingComponent r) {
			sb.append(" outer=").append(r.getOuterRadius()).append(':').append(r.isOuterRadiusAutomatic());
			sb.append(" inner=").append(r.getInnerRadius()).append(':').append(r.isInnerRadiusAutomatic());
			sb.append(" radial=").append(r.getRadialPosition()).append(':').append(r.getRadialDirection());
		}
		if (c instanceof RadiusRingComponent r) {
			sb.append(" spacing=").append(r.getInstanceSeparation());
		}
		if (c instanceof InnerTube t) {
			sb.append(" cluster=").append(t.getClusterConfiguration().getXMLName()).append(':')
					.append(t.getClusterScale()).append(':').append(t.getClusterRotation());
		}
		if (c instanceof MassObject m) {
			sb.append(" packed=").append(m.getLength()).append(':').append(m.getRadius()).append(':')
					.append(m.isRadiusAutomatic());
			sb.append(" radial=").append(m.getRadialPosition()).append(':').append(m.getRadialDirection());
		}
		if (c instanceof MassComponent m) {
			sb.append(" mass=").append(m.getComponentMass());
			sb.append(" type=").append(m.getMassComponentType().name());
		}
		if (c instanceof ShockCord s) {
			sb.append(" cord=").append(s.getCordLength()).append(':').append(s.isCordLengthAutomatic());
			sb.append(" mat=").append(material(s.getMaterial()));
		}
		if (c instanceof RecoveryDevice d) {
			sb.append(" cd=").append(d.getCD()).append(':').append(d.isCDAutomatic());
			sb.append(" drogue=").append(d.isDrogue());
			sb.append(" mat=").append(material(d.getMaterial()));
			sb.append(" deploy=").append(deployment(d.getDeploymentConfigurations().getDefault()));
			for (FlightConfigurationId id : d.getDeploymentConfigurations().getIds()) {
				sb.append(" deploy[").append(fcidText(id, known)).append("]=")
						.append(deployment(d.getDeploymentConfigurations().get(id)));
			}
		}
		if (c instanceof Parachute p) {
			sb.append(" diameter=").append(p.getDiameter());
			sb.append(" lines=").append(p.getLineCount()).append(':').append(p.getLineLength()).append(':')
					.append(p.isLineLengthAutomatic());
			sb.append(" linemat=").append(material(p.getLineMaterial()));
		}
		if (c instanceof Streamer s) {
			sb.append(" strip=").append(s.getStripLength()).append(':').append(s.getStripWidth());
		}
		if (c instanceof MotorMount m) {
			sb.append(" mount=").append(m.isMotorMount());
			sb.append(" overhang=").append(m.getMotorOverhang());
			sb.append(" ign=").append(ignition(m.getDefaultMotorConfig()));
			for (FlightConfigurationId id : m.getMotorConfigurationSet().getIds()) {
				MotorConfiguration config = m.getMotorConfig(id);
				sb.append(" motor[").append(fcidText(id, known)).append("]=")
						.append(config.getMotor() == null ? "none" : config.getMotor().getDesignation()).append(':')
						.append(config.getEjectionDelay()).append(':').append(config.getNozzleExitDiameter())
						.append(':').append(ignition(config));
			}
		}
		if (c.getAppearance() != null) {
			sb.append(" app=").append(appearance(c.getAppearance()));
		}
		if (c instanceof InsideColorComponent i) {
			InsideColorComponentHandler h = i.getInsideColorComponentHandler();
			if (h.getInsideAppearance() != null) {
				sb.append(" inside=").append(appearance(h.getInsideAppearance()));
			}
			if (h.isEdgesSameAsInside() || h.isSeparateInsideOutside()) {
				sb.append(" insideflags=").append(h.isEdgesSameAsInside()).append(',')
						.append(h.isSeparateInsideOutside());
			}
		}
		return sb.toString();
	}

	/** An appearance: [paint=r,g,b,a shine=s opacity=flag decal='image':rotation:EDGE:center=u,v:offset=u,v:scale=u,v]. */
	private static String appearance(Appearance a) {
		StringBuilder sb = new StringBuilder("[paint=");
		ORColor paint = a.getPaint();
		sb.append(paint.getRed()).append(',').append(paint.getGreen()).append(',').append(paint.getBlue()).append(',')
				.append(paint.getAlpha());
		sb.append(" shine=").append(a.getShine());
		sb.append(" opacity=").append(a.isOpacityAffectsTexture());
		Decal d = a.getTexture();
		if (d != null) {
			sb.append(" decal=").append(quote(d.getImage().getName())).append(':').append(d.getRotation()).append(':')
					.append(d.getEdgeMode().name());
			sb.append(":center=").append(d.getCenter().getX()).append(',').append(d.getCenter().getY());
			sb.append(":offset=").append(d.getOffset().getX()).append(',').append(d.getOffset().getY());
			sb.append(":scale=").append(d.getScale().getX()).append(',').append(d.getScale().getY());
		}
		return sb.append(']').toString();
	}

	private static void dumpTree(RocketComponent c, int depth, Set<UUID> known, List<String> lines) {
		lines.add("  ".repeat(depth) + describe(c, known));
		for (RocketComponent child : c.getChildren()) {
			dumpTree(child, depth + 1, known, lines);
		}
	}

	@SuppressWarnings("unchecked")
	private static String preloaded(FlightConfiguration config) throws Exception {
		Field f = FlightConfiguration.class.getDeclaredField("preloadStageActiveness");
		f.setAccessible(true);
		Map<Integer, Boolean> map = (Map<Integer, Boolean>) f.get(config);
		return map == null ? "null" : new TreeMap<>(map).toString();
	}

	private static String configuration(Rocket rocket, FlightConfiguration config, String label) throws Exception {
		StringBuilder sb = new StringBuilder();
		sb.append("config ").append(label);
		sb.append(" name=").append(quote(config.getNameRaw()));
		sb.append(" preload=").append(preloaded(config));
		sb.append(" active=[");
		for (int i = 0; i < rocket.getStageCount(); i++) {
			sb.append(i == 0 ? "" : ",").append(config.isStageActive(i));
		}
		sb.append("] motors=").append(config.getAllMotors().size());
		// Tier 9b, the fixer of the rocket side: the motors themselves, as the configuration has them.
		if (!config.getAllMotors().isEmpty() || !config.getActiveMotors().isEmpty()) {
			sb.append(" all=").append(motors(rocket, config.getAllMotors()));
			sb.append(" flying=").append(motors(rocket, config.getActiveMotors()));
		}
		return sb.toString();
	}

	/** Counts the components in the order of dumpTree(); true when the wanted one (the same object) is found. */
	private static boolean findInTreeOrder(RocketComponent c, Object wanted, int[] index) {
		if (c == wanted) {
			return true;
		}
		index[0]++;
		for (RocketComponent child : c.getChildren()) {
			if (findInTreeOrder(child, wanted, index)) {
				return true;
			}
		}
		return false;
	}

	/**
	 * The motors of a flight configuration: "[#n:designation:event:delay:override,...]", n being the place of
	 * the motor's mount among the lines of the tree (-1 when it is not in the tree), in the order of those
	 * places (the collection itself is in hash order).
	 */
	private static String motors(Rocket rocket, java.util.Collection<MotorConfiguration> motors) {
		List<Object[]> entries = new ArrayList<>();
		for (MotorConfiguration m : motors) {
			int[] index = new int[1];
			if (!findInTreeOrder(rocket, m.getMount(), index)) {
				index[0] = -1;
			}
			String text = "#" + index[0] + ":" + (m.getMotor() == null ? "none" : m.getMotor().getDesignation())
					+ ":" + ignition(m);
			entries.add(new Object[] { index[0], text });
		}
		entries.sort((a, b) -> {
			int byIndex = Integer.compare((Integer) a[0], (Integer) b[0]);
			return byIndex != 0 ? byIndex : ((String) a[1]).compareTo((String) b[1]);
		});
		StringBuilder sb = new StringBuilder("[");
		for (Object[] entry : entries) {
			sb.append(sb.length() > 1 ? "," : "").append((String) entry[1]);
		}
		return sb.append("]").toString();
	}

	static List<String> dump(OpenRocketDocument doc, Set<UUID> known) throws Exception {
		List<String> lines = new ArrayList<>();
		Rocket rocket = doc.getRocket();
		dumpTree(rocket, 0, known, lines);
		// The images of the document's decal registry, in its order (by name); no line without one.
		if (!doc.getDecalList().isEmpty()) {
			StringBuilder decals = new StringBuilder("decals=");
			boolean first = true;
			for (DecalImage image : doc.getDecalList()) {
				decals.append(first ? "" : ",").append(quote(image.getName()));
				first = false;
			}
			lines.add(decals.toString());
		}
		FlightConfigurationId selected = rocket.getSelectedConfiguration().getFlightConfigurationID();
		lines.add("selected=" + (selected.isDefaultId() ? "default" : fcidText(selected, known)));
		lines.add(configuration(rocket, rocket.getFlightConfigurations().getDefault(), "default"));
		for (FlightConfigurationId id : rocket.getIds()) {
			lines.add(configuration(rocket, rocket.getFlightConfiguration(id), fcidText(id, known)));
		}
		return lines;
	}

	// ------------------------------------------------------------------ RecordedProbe of tier 9c

	static Object field(Object o, String name) throws Exception {
		Field f = o.getClass().getDeclaredField(name);
		f.setAccessible(true);
		return f.get(o);
	}

	static String oneLine(String s) {
		return s == null ? "<null>" : s.replace("\n", "\\n").replace("\r", "\\r");
	}

	/** "version=<n>": the file version OpenRocketHandler leaves in a loading context that reads the document. */
	static List<String> version(byte[] document) {
		List<String> lines = new ArrayList<>();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createEmptyRocket();
		DocumentLoadingContext context = new DocumentLoadingContext();
		context.setOpenRocketDocument(doc);
		context.setMotorFinder(new DatabaseMotorFinder());
		WarningSet warnings = new WarningSet();
		try {
			SimpleSAX.readXML(new InputSource(new ByteArrayInputStream(document)), new OpenRocketHandler(context),
					warnings);
		} catch (Throwable t) {
			lines.add("version THROWN " + t.getClass().getName() + ": " + oneLine(t.getMessage()));
		}
		lines.add("version=" + context.getFileVersion());
		return lines;
	}

	/** TopProbe.load() of part "loader", without the document's own events and the drawn ids. */
	static List<String> documentLines(OpenRocketDocument doc) throws Exception {
		List<String> lines = new ArrayList<>();
		Rocket rocket = doc.getRocket();
		int components = 0;
		for (@SuppressWarnings("unused") RocketComponent c : rocket)
			components++;
		lines.add("rocket '" + oneLine(rocket.getName()) + "' stages=" + rocket.getStageCount()
				+ " components=" + components + " configurations=" + rocket.getFlightConfigurationCount()
				+ " selected=" + (rocket.getSelectedConfiguration() == rocket.getEmptyConfiguration() ? "default"
						: String.valueOf(rocket.getIds().indexOf(rocket.getSelectedConfiguration().getId()))));
		List<FlightConfiguration> configs = new ArrayList<>();
		configs.add(rocket.getEmptyConfiguration());
		for (FlightConfigurationId id : rocket.getIds())
			configs.add(rocket.getFlightConfiguration(id));
		int index = -1;
		for (FlightConfiguration config : configs) {
			StringBuilder active = new StringBuilder();
			for (int stage = 0; stage < rocket.getStageCount(); stage++)
				active.append(config.isStageActive(stage) ? 't' : 'f');
			lines.add("config " + (index < 0 ? "default" : String.valueOf(index)) + " active=" + active);
			index++;
		}
		if (doc.getSimulationCount() != 0) {
			throw new IllegalStateException("the tool describes no simulations; the document has "
					+ doc.getSimulationCount());
		}
		lines.add("storage fileType=" + doc.getDefaultStorageOptions().getFileType() + " saveSimulationData="
				+ doc.getDefaultStorageOptions().getSaveSimulationData() + " explicitlySet="
				+ doc.getDefaultStorageOptions().isExplicitlySet());
		Method clean = OpenRocketDocument.class.getDeclaredMethod("isCleanState");
		clean.setAccessible(true);
		lines.add("saved=" + doc.isSaved() + " undoAvailable=" + doc.isUndoAvailable() + " redoAvailable="
				+ doc.isRedoAvailable() + " undoDescription=" + doc.getUndoDescription() + " history="
				+ ((List<?>) field(doc, "undoHistory")).size() + " position=" + field(doc, "undoPosition") + " clean="
				+ clean.invoke(doc) + " file=" + doc.getFile());
		ModID mod = rocket.getModID();
		Rocket snapshot = (Rocket) ((List<?>) field(doc, "undoHistory")).get(0);
		lines.add("modids mass=" + (rocket.getMassModID() == mod) + " aero=" + (rocket.getAerodynamicModID() == mod)
				+ " tree=" + (rocket.getTreeModID() == mod) + " functional=" + (rocket.getFunctionalModID() == mod)
				+ " snapshot=" + (snapshot.getModID() == mod));
		StringBuilder materials = new StringBuilder();
		for (Material m : doc.getDocumentPreferences().getAllMaterials())
			materials.append(m.getName()).append(':').append(m.getType()).append(':').append(m.getDensity()).append(' ');
		lines.add("materials=[" + materials.toString().trim() + "]");
		Map<String, String> photo = new TreeMap<>(doc.getPhotoSettings());
		lines.add("expressions=" + doc.getCustomExpressions().size() + " photo=" + photo + " decals="
				+ doc.getDecalList());
		return lines;
	}

	/** Counts the components in tree order (the rocket is 0) and collects the motor mounts. */
	static void mounts(RocketComponent c, int[] index, Map<Integer, MotorMount> found) {
		if (c instanceof MotorMount && ((MotorMount) c).isMotorMount()) {
			found.put(index[0], (MotorMount) c);
		}
		index[0]++;
		for (RocketComponent child : c.getChildren()) {
			mounts(child, index, found);
		}
	}

	static List<String> motorLines(Rocket rocket) {
		List<String> lines = new ArrayList<>();
		Map<Integer, MotorMount> found = new LinkedHashMap<>();
		mounts(rocket, new int[1], found);
		int config = 0;
		for (FlightConfigurationId id : rocket.getIds()) {
			for (Map.Entry<Integer, MotorMount> mount : found.entrySet()) {
				MotorConfiguration mc = mount.getValue().getMotorConfig(id);
				if (mc.getMotor() == null) {
					lines.add("motor config=" + config + " mount=#" + mount.getKey() + " none");
					continue;
				}
				ThrustCurveMotor motor = (ThrustCurveMotor) mc.getMotor();
				lines.add("motor config=" + config + " mount=#" + mount.getKey() + " "
						+ motor.getManufacturer().getDisplayName() + "|" + motor.getDesignation() + "|" + motor.getDigest()
						+ "|delay=" + mc.getEjectionDelay());
			}
			config++;
		}
		return lines;
	}

	/** The names of the flight configurations as they are shown (there is no simulation). */
	static List<String> nameLines(OpenRocketDocument doc) {
		List<String> lines = new ArrayList<>();
		Rocket rocket = doc.getRocket();
		lines.add("name default '" + oneLine(rocket.getEmptyConfiguration().getName()) + "'");
		int index = 0;
		for (FlightConfigurationId id : rocket.getIds()) {
			lines.add("name " + index + " '" + oneLine(rocket.getFlightConfiguration(id).getName()) + "'");
			index++;
		}
		return lines;
	}

	static List<String> decalLines(OpenRocketDocument doc) {
		List<String> result = new ArrayList<>();
		Map<String, String> lines = new TreeMap<>();
		for (DecalImage d : doc.getDecalList()) {
			String state;
			try (InputStream is = d.getBytes()) {
				state = "bytes=" + is.readAllBytes().length;
			} catch (Throwable t) {
				state = "unreadable";
			}
			lines.put(d.getName(), state);
		}
		for (Map.Entry<String, String> line : lines.entrySet()) {
			result.add("decal '" + line.getKey() + "' " + line.getValue());
		}
		return result;
	}

	/** ExampleProbe.documentPreferenceLines of tier 9c. */
	static List<String> documentPreferenceLines(OpenRocketDocument doc) {
		List<String> lines = new ArrayList<>();
		DocumentPreferences prefs = doc.getDocumentPreferences();
		lines.add("docprefs=" + prefs.getPreferencesMap().size());
		// OpenRocketSaver.saveDocumentPreferences(): the order of <docmaterials>.
		for (Material m : prefs.getAllMaterials()) {
			lines.add("docmaterial " + oneLine(m.toStorableString()));
		}
		return lines;
	}

	/**
	 * The state of a design file as loaded (RecordedProbe.run, "full", and ExampleProbe's document
	 * preferences): @p document is the file's XML, @p doc its load, @p warnings the load's warnings,
	 * @p events the number of change events the rocket fired during the load.
	 */
	static List<String> loadedState(byte[] document, OpenRocketDocument doc, List<String> warnings, int events)
			throws Exception {
		List<String> lines = new ArrayList<>(version(document));
		for (String w : warnings) {
			lines.add("W " + oneLine(w));
		}
		lines.addAll(documentLines(doc));
		lines.add("events rocket=" + events);
		for (String line : dump(doc, knownIds(new String(document, StandardCharsets.UTF_8)))) {
			lines.add("| " + line);
		}
		lines.addAll(nameLines(doc));
		lines.addAll(motorLines(doc.getRocket()));
		lines.addAll(decalLines(doc));
		lines.addAll(documentPreferenceLines(doc));
		return lines;
	}

	/** RecordedProbe.numbers: masses, centres of mass and the length, as "name=value". */
	static List<String> numbers(OpenRocketDocument doc) {
		List<String> lines = new ArrayList<>();
		Rocket rocket = doc.getRocket();
		FlightConfiguration all = rocket.getEmptyConfiguration().clone();
		all.setAllStages();
		RigidBody structure = MassCalculator.calculateStructure(all);
		lines.add("structure.mass=" + structure.getMass());
		lines.add("structure.cm.x=" + structure.getCM().getX());
		lines.add("structure.cm.y=" + structure.getCM().getY());
		lines.add("structure.cm.z=" + structure.getCM().getZ());
		int index = 0;
		for (FlightConfigurationId id : rocket.getIds()) {
			FlightConfiguration config = rocket.getFlightConfiguration(id).clone();
			config.setAllStages();
			RigidBody launch = MassCalculator.calculateLaunch(config);
			lines.add("launch[" + index + "].mass=" + launch.getMass());
			lines.add("launch[" + index + "].cm.x=" + launch.getCM().getX());
			index++;
		}
		lines.add("length=" + all.getLength());
		return lines;
	}
}
