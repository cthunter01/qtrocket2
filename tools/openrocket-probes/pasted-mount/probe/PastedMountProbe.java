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
import info.openrocket.core.document.StorageOptions;
import info.openrocket.core.file.GeneralRocketLoader;
import info.openrocket.core.file.GeneralRocketSaver;
import info.openrocket.core.formatting.RocketDescriptor;
import info.openrocket.core.l10n.Translator;
import info.openrocket.core.motor.MotorConfiguration;
import info.openrocket.core.motor.ThrustCurveMotor;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfiguration;
import info.openrocket.core.rocketcomponent.FlightConfigurationId;
import info.openrocket.core.rocketcomponent.MotorMount;
import info.openrocket.core.rocketcomponent.RecoveryDevice;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurablePanel;
import info.openrocket.swing.gui.main.flightconfigpanel.FlightConfigurableTableModel;
import info.openrocket.swing.gui.main.flightconfigpanel.MotorConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.RecoveryConfigurationPanel;
import info.openrocket.swing.gui.main.flightconfigpanel.SeparationConfigurationPanel;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.GUIUtil;
import info.openrocket.swing.gui.util.SwingPreferences;

import java.awt.Component;
import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.JTable;
import javax.swing.SwingUtilities;
import javax.swing.table.AbstractTableModel;
import javax.swing.table.TableCellRenderer;

/**
 * Milestone 3 run 3, part X4 (the M3 tests-exit scout's open point C2): what OpenRocket does with a motor mount,
 * a recovery device and a stage pasted into a design whose flight configurations have other ids than the design
 * they were copied from. Their parameter sets keep the source's configuration ids (RocketComponent.copy() keeps
 * every MotorConfigurationSet and FlightConfigurableParameterSet with its ids), which the target's rocket does not
 * have.
 * <p>
 * The source is the example "Two stage high power rocket" (two configurations, here S1 and S2), the target the
 * example "A simple model rocket" (five, T1 to T5), both loaded from data/examples of the repository with the
 * bundled motor database (data/motors/initial_motors.db) and no component preset. The copy and paste are those of
 * RocketActions: CopyAction's copyComponentsMaintainParent() (copy()), OpenRocketClipboard.setClipboard() (copy()
 * again), PasteAction's copy(), getPastePosition() (the selected component when it is compatible, else its parent
 * after it), document.addUndoPosition("Paste " + name) and parent.addChild(child, index). Three cases:
 * <ul>
 * <li>mount: the booster's motor mount (Booster motor mount: H148R-0 for S1, I357T-14 for S2) pasted with the
 * target's Body tube selected;</li>
 * <li>stage: the booster stage (Booster: its separation overrides for S1 and S2, Booster Chute's deployment
 * overrides for S1 and S2, the motor mount) pasted into another load of the target with its Sustainer
 * selected;</li>
 * <li>back: the mount pasted into the target (the first case) copied from there and pasted into the source again,
 * with its Booster fin section selected.</li>
 * </ul>
 * For the first two: the parameter sets of the pasted components, what the target's configurations hold of them,
 * the three tables of the Motors &amp; Configuration tab (OpenRocket's own table models and cell renderers, as
 * tools/openrocket-gui-pins/flight-config/probe/FlightConfigurationTablesProbe.java renders them), the entries of
 * the save (GeneralRocketSaver's .ork archive, as Save as writes it, without simulation data) and the lines of its
 * document that name a configuration, the difference between the pasted component's element in that document and
 * the copied component's element in the source's save (what the save of the pasted copy writes otherwise), and the
 * same after a reload of that save, with whether the reload's save is the save; for the last, the parameter sets
 * and the source's tables.
 * <p>
 * Configuration ids are printed by name (S1, S2, T1 to T5 by their rows; "default" for the default id), components
 * by path from their stage. tests/gui/exit/pasted_motor_mount_tests.cpp pins what this prints by hand (expected.txt;
 * see ../README.md).
 */
public class PastedMountProbe {

	static final String SOURCE = "Two stage high power rocket.ork";
	static final String TARGET = "A simple model rocket.ork";

	static final Pattern UUID = Pattern
			.compile("[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}");

	/** The names of the configuration ids, by their key. */
	static final Map<String, String> names = new LinkedHashMap<>();

	static Path examples;
	static RocketDescriptor descriptor;
	/** The source's save as it was loaded (the elements the pasted copies are compared with). */
	static File sourceSave;

	public static void main(String[] args) throws Exception {
		bootstrap();
		int status = 0;
		try {
			SwingUtilities.invokeAndWait(() -> {
				try {
					run();
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

	static void run() throws Exception {
		GUIUtil.setUITheme(UITheme.Themes.LIGHT);
		FlightConfigurablePanel.updateColors();
		descriptor = Application.getInjector().getInstance(RocketDescriptor.class);
		examples = Path.of(System.getProperty("qtrocket.repo")).resolve("data").resolve("examples");

		OpenRocketDocument source = load(SOURCE);
		name(source.getRocket(), "S");
		sourceSave = saveArchive(source);
		OpenRocketDocument target = load(TARGET);
		name(target.getRocket(), "T");
		System.out.println("source " + SOURCE + ": " + ids(source.getRocket().getIds()));
		System.out.println("target " + TARGET + ": " + ids(target.getRocket().getIds()));

		System.out.println();
		System.out.println("== mount");
		RocketComponent mount = paste(source, "Booster motor mount", target, "Body tube");
		report(target, mount);

		System.out.println();
		System.out.println("== stage");
		OpenRocketDocument stageTarget = load(TARGET);
		RocketComponent stage = paste(source, "Booster", stageTarget, "Sustainer");
		report(stageTarget, stage);

		System.out.println();
		System.out.println("== back");
		RocketComponent back = paste(target, path(mount), source, "Booster fin section");
		System.out.println("pasted " + path(back) + " into the source at "
				+ back.getParent().getChildPosition(back));
		sets(back);
		tables(source.getRocket());
	}

	/** The parameter sets of the pasted tree, the target's configurations, tables, save and reload. */
	static void report(OpenRocketDocument target, RocketComponent pasted) throws Exception {
		System.out.println("pasted " + path(pasted) + " into " + path(pasted.getParent()) + " at "
				+ pasted.getParent().getChildPosition(pasted));
		sets(pasted);
		configurations(target.getRocket());
		tables(target.getRocket());
		File file = saveArchive(target);
		System.out.println("archive: " + entries(file).keySet());
		List<String> save = lines(file);
		System.out.println("save:");
		for (String line : save) {
			System.out.println("  " + line);
		}
		System.out.println("element " + pasted.getName() + " (- the source's save, + the target's):");
		for (String line : difference(element(sourceSave, pasted.getName()), element(file, pasted.getName()))) {
			System.out.println("  " + line);
		}
		GeneralRocketLoader loader = new GeneralRocketLoader(file);
		OpenRocketDocument reloaded = loader.load();
		System.out.println("reload: warnings " + loader.getWarnings().size() + ", "
				+ ids(reloaded.getRocket().getIds()));
		RocketComponent again = find(reloaded.getRocket(), path(pasted));
		sets(again);
		tables(reloaded.getRocket());
		File resave = saveArchive(reloaded);
		System.out.println("resave " + (entries(resave).keySet().equals(entries(file).keySet())
				&& lines(resave).equals(save) ? "same as the save" : "DIFFERENT"));
	}

	/** The path of @p component from its stage, "Booster/Booster fin section/Booster motor mount". */
	static String path(RocketComponent component) {
		StringBuilder text = new StringBuilder(component.getName());
		for (RocketComponent c = component.getParent(); c != null && !(c instanceof Rocket); c = c.getParent()) {
			text.insert(0, c.getName() + "/");
		}
		return text.toString();
	}

	/** The first component of @p rocket whose path or name is @p name. */
	static RocketComponent find(Rocket rocket, String name) {
		Iterator<RocketComponent> it = rocket.iterator(true);
		while (it.hasNext()) {
			RocketComponent c = it.next();
			if (path(c).equals(name) || c.getName().equals(name)) {
				return c;
			}
		}
		throw new IllegalStateException("no component " + name);
	}

	/** RocketActions' copy of @p name in @p from, and its paste with @p selected selected in @p to. */
	static RocketComponent paste(OpenRocketDocument from, String name, OpenRocketDocument to, String selected) {
		RocketComponent original = find(from.getRocket(), name);
		// CopyAction: copyComponentsMaintainParent(); OpenRocketClipboard.setClipboard() copies again.
		RocketComponent clipboard = original.copy().copy();
		// PasteAction: a copy of the clipboard's, at getPastePosition() of the selected component.
		RocketComponent pasted = clipboard.copy();
		RocketComponent destination = find(to.getRocket(), selected);
		RocketComponent parent;
		int index;
		if (destination.isCompatible(pasted)) {
			parent = destination;
			index = destination.getChildCount();
		} else {
			parent = destination.getParent();
			index = parent.getChildPosition(destination) + 1;
			if (!parent.isCompatible(pasted)) {
				throw new IllegalStateException(name + " fits neither into nor beside " + selected);
			}
		}
		to.addUndoPosition("Paste " + pasted.getComponentName());
		parent.addChild(pasted, index);
		return pasted;
	}

	static OpenRocketDocument load(String file) throws Exception {
		GeneralRocketLoader loader = new GeneralRocketLoader(examples.resolve(file).toFile());
		OpenRocketDocument document = loader.load();
		if (!loader.getWarnings().isEmpty()) {
			throw new IllegalStateException(file + ": " + loader.getWarnings());
		}
		return document;
	}

	/** Names the configuration ids of @p rocket @p prefix + row + 1. */
	static void name(Rocket rocket, String prefix) {
		List<FlightConfigurationId> ids = rocket.getIds();
		for (int i = 0; i < ids.size(); i++) {
			names.put(ids.get(i).key.toString(), prefix + (i + 1));
		}
	}

	static String id(FlightConfigurationId id) {
		if (id == FlightConfigurationId.DEFAULT_VALUE_FCID || id.equals(FlightConfigurationId.DEFAULT_VALUE_FCID)) {
			return "default";
		}
		String name = names.get(id.key.toString());
		if (name == null) {
			throw new IllegalStateException("an id of neither design: " + id.key);
		}
		return name;
	}

	static String ids(List<FlightConfigurationId> ids) {
		List<String> texts = new ArrayList<>();
		for (FlightConfigurationId id : ids) {
			texts.add(id(id));
		}
		return "ids " + texts;
	}

	/** @p text with every configuration id replaced by its name (a component id stays). */
	static String named(String text) {
		Matcher matcher = UUID.matcher(text);
		StringBuilder sb = new StringBuilder();
		while (matcher.find()) {
			String name = names.get(matcher.group().toLowerCase());
			matcher.appendReplacement(sb, Matcher.quoteReplacement(name != null ? name : matcher.group()));
		}
		matcher.appendTail(sb);
		return sb.toString();
	}

	/** The parameter sets of the motor mounts, recovery devices and stages of the tree of @p root. */
	static void sets(RocketComponent root) {
		Iterator<RocketComponent> it = root.iterator(true);
		while (it.hasNext()) {
			RocketComponent c = it.next();
			if (c instanceof MotorMount mount && mount.isMotorMount()) {
				List<String> entries = new ArrayList<>();
				for (FlightConfigurationId id : mount.getMotorConfigurationSet().getIds()) {
					entries.add(id(id) + " " + motor(mount.getMotorConfig(id)));
				}
				System.out.println("  motors " + path(c) + ": default " + ignition(mount.getDefaultMotorConfig())
						+ ", " + entries);
			}
			if (c instanceof RecoveryDevice device) {
				List<String> entries = new ArrayList<>();
				for (FlightConfigurationId id : device.getDeploymentConfigurations().getIds()) {
					entries.add(id(id) + " " + deployment(device.getDeploymentConfigurations().get(id)));
				}
				System.out.println("  deployments " + path(c) + ": default "
						+ deployment(device.getDeploymentConfigurations().getDefault()) + ", " + entries);
			}
			if (c instanceof AxialStage stage) {
				List<String> entries = new ArrayList<>();
				for (FlightConfigurationId id : stage.getSeparationConfigurations().getIds()) {
					entries.add(id(id) + " " + separation(stage.getSeparationConfigurations().get(id)));
				}
				System.out.println("  separations " + path(c) + " (stage " + stage.getStageNumber() + "): default "
						+ separation(stage.getSeparationConfigurations().getDefault()) + ", " + entries);
			}
		}
	}

	static String motor(MotorConfiguration config) {
		String motor = config.getMotor() == null ? "no motor"
				: config.getMotor().getDesignation(config.getEjectionDelay());
		return motor + " " + ignition(config) + (config.hasIgnitionOverride() ? " (overridden)" : "");
	}

	static String ignition(MotorConfiguration config) {
		return config.getIgnitionEvent().name() + "+" + config.getIgnitionDelay();
	}

	static String deployment(DeploymentConfiguration config) {
		return config.getDeployEvent().name() + "/" + config.getDeployAltitude() + "/" + config.getDeployDelay();
	}

	static String separation(StageSeparationConfiguration config) {
		return config.getSeparationEvent().name() + "/" + config.getSeparationAltitude() + "/"
				+ config.getSeparationDelay();
	}

	/** What each configuration of @p rocket holds: its motors by mount and its active stages. */
	static void configurations(Rocket rocket) {
		for (FlightConfigurationId id : rocket.getIds()) {
			FlightConfiguration config = rocket.getFlightConfiguration(id);
			List<String> motors = new ArrayList<>();
			for (MotorConfiguration motor : config.getAllMotors()) {
				motors.add(path((RocketComponent) motor.getMount()) + " "
						+ motor.getMotor().getDesignation(motor.getEjectionDelay()));
			}
			List<Integer> stages = new ArrayList<>();
			for (AxialStage stage : config.getActiveStages()) {
				stages.add(stage.getStageNumber());
			}
			System.out.println("  configuration " + id(id) + " '" + config.getName() + "': motors " + motors
					+ ", active stages " + stages);
		}
	}

	// ---------------------------------------------------------------------------------------------
	// The tables (FlightConfigurationTablesProbe's technique)

	static void tables(Rocket rocket) throws Exception {
		Table motors = table(MotorConfigurationPanel.class, "MotorTableCellRenderer", MotorMount.class, rocket);
		Table recovery = table(RecoveryConfigurationPanel.class, "RecoveryTableCellRenderer", RecoveryDevice.class,
				rocket);
		Table stages = table(SeparationConfigurationPanel.class, "SeparationTableCellRenderer", AxialStage.class,
				rocket);
		print("motors", motors, true);
		print("recovery", recovery, false);
		print("stages", stages, false);
	}

	/**
	 * The table @p name: its header, then a row per configuration, "row: cell | cell" (column 0 first; a motor
	 * cell "motor / ignition", the ignition after "~" when it is drawn as the default's).
	 */
	static void print(String name, Table table, boolean motorCells) {
		List<String> header = new ArrayList<>();
		for (int col = 0; col < table.model.getColumnCount(); col++) {
			header.add(table.model.getColumnName(col));
		}
		System.out.println("  " + name + ": " + String.join(" | ", header));
		for (int row = 0; row < table.model.getRowCount(); row++) {
			List<String> cells = new ArrayList<>();
			cells.add(table.render(row, 0).getText());
			for (int col = 1; col < table.model.getColumnCount(); col++) {
				JLabel label = table.render(row, col);
				if (!motorCells) {
					cells.add(label.getText());
					continue;
				}
				List<JLabel> parts = new ArrayList<>();
				for (Component c : label.getComponents()) {
					if (c instanceof JLabel l) {
						parts.add(l);
					}
				}
				if (parts.size() != 2) {
					throw new IllegalStateException("a motor cell of " + parts.size() + " labels");
				}
				cells.add(parts.get(0).getText() + " / " + (parts.get(1).getFont().isItalic() ? "~" : "")
						+ parts.get(1).getText());
			}
			System.out.println("    " + row + ": " + String.join(" | ", cells));
		}
	}

	static final class Table {
		final AbstractTableModel model;
		final JTable table;
		final TableCellRenderer renderer;

		Table(AbstractTableModel model, TableCellRenderer renderer) {
			this.model = model;
			this.table = new JTable(model);
			this.renderer = renderer;
		}

		JLabel render(int row, int col) {
			return (JLabel) renderer.getTableCellRendererComponent(table, table.getValueAt(row, col), false, false,
					row, col);
		}
	}

	static Table table(Class<? extends FlightConfigurablePanel<?>> panelClass, String rendererName,
			Class<?> componentClass, Rocket rocket) throws Exception {
		FlightConfigurablePanel<?> panel = allocate(panelClass);
		setField(FlightConfigurablePanel.class, panel, "rocket", rocket);
		for (Class<?> c = panelClass; c != JPanel.class; c = c.getSuperclass()) {
			for (Field field : c.getDeclaredFields()) {
				if (Modifier.isStatic(field.getModifiers())) {
					continue;
				}
				if (field.getType() == Translator.class) {
					setField(c, panel, field.getName(), Application.getTranslator());
				} else if (field.getType() == RocketDescriptor.class) {
					setField(c, panel, field.getName(), descriptor);
				}
			}
		}
		AbstractTableModel model = panelModel(panelClass, panel, componentClass, rocket);
		Class<?> rendererClass = Class.forName(panelClass.getName() + "$" + rendererName);
		Constructor<?> constructor = rendererClass.getDeclaredConstructor(panelClass);
		constructor.setAccessible(true);
		return new Table(model, (TableCellRenderer) constructor.newInstance(panel));
	}

	static AbstractTableModel panelModel(Class<?> panelClass, Object panel, Class<?> componentClass, Rocket rocket)
			throws Exception {
		Class<?> found = null;
		for (int n = 1; n <= 20; n++) {
			Class<?> candidate;
			try {
				candidate = Class.forName(panelClass.getName() + "$" + n);
			} catch (ClassNotFoundException e) {
				break;
			}
			if (candidate.getSuperclass() == FlightConfigurableTableModel.class) {
				if (found != null) {
					throw new IllegalStateException("two table models in " + panelClass.getName());
				}
				found = candidate;
			}
		}
		if (found == null) {
			return new FlightConfigurableTableModel<>(cast(componentClass), rocket);
		}
		Constructor<?>[] constructors = found.getDeclaredConstructors();
		if (constructors.length != 1 || constructors[0].getParameterCount() != 3) {
			throw new IllegalStateException("unexpected constructors of " + found.getName());
		}
		constructors[0].setAccessible(true);
		return (AbstractTableModel) constructors[0].newInstance(panel, componentClass, rocket);
	}

	@SuppressWarnings({"unchecked", "rawtypes"})
	static Class cast(Class<?> componentClass) {
		return componentClass;
	}

	@SuppressWarnings("unchecked")
	static <T> T allocate(Class<T> type) throws Exception {
		Field field = Class.forName("sun.misc.Unsafe").getDeclaredField("theUnsafe");
		field.setAccessible(true);
		Object unsafe = field.get(null);
		return (T) unsafe.getClass().getMethod("allocateInstance", Class.class).invoke(unsafe, type);
	}

	static void setField(Class<?> owner, Object target, String name, Object value) throws Exception {
		Field field = owner.getDeclaredField(name);
		field.setAccessible(true);
		field.set(target, value);
	}

	// ---------------------------------------------------------------------------------------------
	// The save

	/**
	 * The .ork archive of @p document (GeneralRocketSaver, as Save as writes it, without the simulation data) in a
	 * temporary file.
	 */
	static File saveArchive(OpenRocketDocument document) throws Exception {
		StorageOptions options = new StorageOptions();
		options.setSaveSimulationData(false);
		File file = Files.createTempFile("pasted-mount", ".ork").toFile();
		file.deleteOnExit();
		GeneralRocketSaver saver = new GeneralRocketSaver();
		saver.save(file, document, options);
		if (!saver.getWarnings().isEmpty() || !saver.getErrors().isEmpty()) {
			throw new IllegalStateException("the save: " + saver.getWarnings() + " " + saver.getErrors());
		}
		return file;
	}

	/** The entries of the archive @p file, by name in their order. */
	static Map<String, byte[]> entries(File file) throws Exception {
		Map<String, byte[]> entries = new LinkedHashMap<>();
		try (ZipInputStream in = new ZipInputStream(Files.newInputStream(file.toPath()))) {
			ZipEntry entry;
			while ((entry = in.getNextEntry()) != null) {
				entries.put(entry.getName(), in.readAllBytes());
			}
		}
		return entries;
	}

	/**
	 * The lines of the design document (rocket.ork) of the archive @p file that name a configuration id or a
	 * stage of a configuration, stripped, the ids by name, each after the name of the element it is in (the last
	 * &lt;name&gt; before it; the rocket's for a configuration without a name of its own).
	 */
	static List<String> lines(File file) throws Exception {
		List<String> lines = new ArrayList<>();
		String context = "";
		for (String raw : new String(entries(file).get("rocket.ork"), StandardCharsets.UTF_8).split("\n", -1)) {
			String line = raw.strip();
			if (line.startsWith("<name>")) {
				context = line.substring("<name>".length(), line.length() - "</name>".length());
				continue;
			}
			Matcher matcher = UUID.matcher(line);
			boolean namesConfiguration = false;
			while (matcher.find()) {
				namesConfiguration |= names.containsKey(matcher.group().toLowerCase());
			}
			if (namesConfiguration || line.startsWith("<stage number=")) {
				lines.add(context + ": " + named(line));
			}
		}
		return lines;
	}

	/**
	 * The lines of the element of the component named @p name in the design document of the archive @p file: from
	 * the line before its &lt;name&gt; (its opening tag) to its closing tag (the next line of the opening tag's
	 * indentation), stripped, the configuration ids by name, every component id (&lt;id&gt;, new in a pasted copy:
	 * RocketComponent.copy()) as "&lt;id&gt;(component)&lt;/id&gt;". The name must be the component's alone in
	 * the document.
	 */
	static List<String> element(File file, String name) throws Exception {
		String[] raw = new String(entries(file).get("rocket.ork"), StandardCharsets.UTF_8).split("\n", -1);
		int start = -1;
		for (int i = 1; i < raw.length; i++) {
			if (raw[i].strip().equals("<name>" + name + "</name>") && !raw[i - 1].strip().startsWith("</")) {
				if (start >= 0) {
					throw new IllegalStateException("two elements named " + name);
				}
				start = i - 1;
			}
		}
		if (start < 0) {
			throw new IllegalStateException("no element named " + name);
		}
		String indentation = raw[start].substring(0, raw[start].length() - raw[start].stripLeading().length());
		List<String> lines = new ArrayList<>();
		for (int i = start; i < raw.length; i++) {
			String line = raw[i].strip();
			lines.add(line.startsWith("<id>") ? "<id>(component)</id>" : named(line));
			if (i > start && raw[i].startsWith(indentation + "</")) {
				return lines;
			}
		}
		throw new IllegalStateException("the element named " + name + " does not end");
	}

	/**
	 * The lines in which @p before and @p after differ, "- " a line of @p before only, "+ " one of @p after only,
	 * in their order: the longest common subsequence's complement (a table of the common lengths of the suffixes;
	 * at a choice the line of @p before goes first), as tests/gui/exit/pasted_motor_mount_tests.cpp computes it.
	 */
	static List<String> difference(List<String> before, List<String> after) {
		int[][] common = new int[before.size() + 1][after.size() + 1];
		for (int i = before.size() - 1; i >= 0; i--) {
			for (int j = after.size() - 1; j >= 0; j--) {
				common[i][j] = before.get(i).equals(after.get(j)) ? common[i + 1][j + 1] + 1
						: Math.max(common[i + 1][j], common[i][j + 1]);
			}
		}
		List<String> lines = new ArrayList<>();
		int i = 0;
		int j = 0;
		while (i < before.size() || j < after.size()) {
			if (i < before.size() && j < after.size() && before.get(i).equals(after.get(j))) {
				i++;
				j++;
			} else if (j == after.size() || (i < before.size() && common[i + 1][j] >= common[i][j + 1])) {
				lines.add("- " + before.get(i++));
			} else {
				lines.add("+ " + after.get(j++));
			}
		}
		return lines;
	}

	// ---------------------------------------------------------------------------------------------
	// Start-up

	static void bootstrap() throws Exception {
		Path bundledDb = Path.of(System.getProperty("qtrocket.repo")).resolve("data").resolve("motors")
				.resolve("initial_motors.db");
		Path dbCopy = Files.createTempFile("probe-motors", ".db");
		Files.copy(bundledDb, dbCopy, StandardCopyOption.REPLACE_EXISTING);
		dbCopy.toFile().deleteOnExit();
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(swingPrefs),
				new PluginModule()));
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
}
