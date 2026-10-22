package probe;

import java.awt.Component;
import java.awt.Container;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.swing.Action;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JSlider;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.SwingUtilities;

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
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.adaptors.BooleanModel;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.adaptors.IntegerModel;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.configdialog.LaunchLugConfig;
import info.openrocket.swing.gui.configdialog.MaterialPanel;
import info.openrocket.swing.gui.configdialog.RailButtonConfig;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;
import info.openrocket.swing.gui.configdialog.TubeFinSetConfig;
import info.openrocket.swing.gui.theme.UITheme;

/**
 * What QtRocket's TubeFinSetConfig, LaunchLugConfig and RailButtonConfig are compared with (LugPanelPins.h): the
 * General tab of OpenRocket's TubeFinSetConfig, LaunchLugConfig and
 * RailButtonConfig opened on every tube fin set, launch lug and rail button of the 16 examples and the 13
 * test rockets (the labels with their tooltips, the spinner texts, unit selectors, slider positions, check
 * boxes, the placement combo box; the material panel is left out), then edits through the panels' own models
 * on designs built here (the text a spinner commits, a count, a check box clicked), each followed by the
 * panel's widgets and the component's values, and edits of a panel with a second selected component (a
 * config listener), followed by the values of both.
 *
 * Each design is settled (the golden harness's AutomaticDimensions) before every panel.
 * <p>
 * Arguments: the directory of the 16 examples, the transcript file (OpenRocket's log goes to standard output, its
 * warnings and errors only), the motor database (initial_motors.db). Run it through generate.sh.
 */
public class LugPanelProbe {
	static final String[] MAKERS = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods"};

	/** The transcript (args[1]). */
	static PrintStream out;

	public static void main(String[] args) throws Exception {
		out = new PrintStream(new FileOutputStream(args[1]), true, StandardCharsets.UTF_8);
		((ch.qos.logback.classic.Logger) org.slf4j.LoggerFactory.getLogger(org.slf4j.Logger.ROOT_LOGGER_NAME))
				.setLevel(ch.qos.logback.classic.Level.WARN);
		bootstrap(args[2]);
		SwingUtilities.invokeAndWait(() -> {
			try {
				UITheme.Themes.LIGHT.applyTheme();
				run(new File(args[0]));
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		});
		out.flush();
		out.close();
		System.exit(0);
	}

	static void run(File dir) throws Exception {
		String[] examples = dir.list((d, n) -> n.endsWith(".ork"));
		java.util.Arrays.sort(examples);
		for (String name : examples) {
			OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
			design("example", name, doc);
		}
		for (String maker : MAKERS) {
			Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
			design("testrocket", maker, OpenRocketDocumentFactory.createDocumentFromRocket(rocket));
		}
		built();
	}

	static boolean isMyKind(RocketComponent c) {
		return c.getClass() == TubeFinSet.class || c.getClass() == LaunchLug.class
				|| c.getClass() == RailButton.class;
	}

	/** Part A: the panel of every tube fin set, launch lug and rail button. */
	static void design(String source, String name, OpenRocketDocument doc) throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		List<RocketComponent> comps = preorder(rocket);
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			if (!isMyKind(c)) {
				continue;
			}
			settle(rocket);
			out.println("== panel | " + source + " | " + name + " | " + i + " | " + c.getClass().getSimpleName());
			JDialog dialog = new JDialog();
			RocketComponentConfig panel = makePanel(doc, c, dialog);
			List<String> lines = new ArrayList<>();
			general(panel, lines, true);
			state(c, "", lines);
			print(lines);
			panel.invalidateMe();
			dialog.dispose();
		}
	}

	// ----------------------------------------------------------------------------- the built designs

	/** An edit: a property and the text a spinner commits, "int:<n>" for a count, "click" for a check box. */
	record Edit(String property, String value) {
	}

	static Edit e(String property, String value) {
		return new Edit(property, value);
	}

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

	static void built() throws Exception {
		// Part B: edits of one component.
		edits("tube fins", onBodyTube(new TubeFinSet()), 3, List.of(e("FinCount", "int:2"), e("FinCount", "int:9"),
				e("OuterRadius", "3"), e("OuterRadiusAutomatic", "click"), e("OuterRadius", "4"),
				e("Thickness", "10"), e("InnerRadius", "1"), e("BaseRotation", "200"), e("BaseRotation", "45"),
				e("Length", "5"), e("FinCount", "int:0")));
		edits("launch lug", onBodyTube(new LaunchLug()), 3, List.of(e("Length", "4"), e("OuterRadius", "1.5"),
				e("InnerRadius", "1"), e("Thickness", "1"), e("AngleOffset", "200"), e("AngleOffset", "-45"),
				e("InstanceCount", "int:3"), e("InstanceSeparation", "5"), e("InstanceCount", "int:0")));
		edits("rail button", onBodyTube(new RailButton()), 3, List.of(e("OuterDiameter", "0.5"),
				e("InnerDiameter", "2"), e("OuterDiameter", "1.2"), e("FlangeHeight", "1"), e("TotalHeight", "0.1"),
				e("BaseHeight", "0.1"), e("TotalHeight", "1.5"), e("ScrewHeight", "-1"), e("ScrewHeight", "0.3"),
				e("AngleOffset", "200"), e("AngleOffset", "-20000"), e("InstanceCount", "int:2"),
				e("InstanceSeparation", "10")));

		// Part C: edits of a panel with a second component selected (Java: its config listener).
		LaunchLug lugA = new LaunchLug();
		LaunchLug lugB = new LaunchLug();
		lugB.setOuterRadius(0.004);
		lugB.setThickness(0.0005);
		multi("launch lugs", onBodyTube(lugA, lugB), 3, 4, List.of(e("InnerRadius", "0.6"), e("Length", "6"),
				e("AngleOffset", "90"), e("Thickness", "0.05")));
		TubeFinSet finsA = new TubeFinSet();
		TubeFinSet finsB = new TubeFinSet();
		OpenRocketDocument finsDoc = onBodyTube(finsA, finsB);
		finsA.setOuterRadius(0.01);
		finsA.setThickness(0.001);
		finsB.setOuterRadius(0.008);
		finsB.setThickness(0.002);
		multi("tube fin sets", finsDoc, 3, 4, List.of(e("InnerRadius", "1"), e("FinCount", "int:3"),
				e("BaseRotation", "30"), e("OuterRadiusAutomatic", "click")));
		RailButton buttonA = new RailButton();
		RailButton buttonB = new RailButton();
		buttonB.setInnerDiameter(0.006);
		multi("rail buttons", onBodyTube(buttonA, buttonB), 3, 4, List.of(e("OuterDiameter", "0.9"),
				e("FlangeHeight", "0.3"), e("AngleOffset", "-90")));
	}

	static void edits(String design, OpenRocketDocument doc, int index, List<Edit> edits) throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		RocketComponent c = preorder(rocket).get(index);
		out.println("== panel | built | " + design + " | " + index + " | " + c.getClass().getSimpleName());
		JDialog dialog = new JDialog();
		RocketComponentConfig panel = makePanel(doc, c, dialog);
		List<String> lines = new ArrayList<>();
		general(panel, lines, true);
		state(c, "", lines);
		print(lines);
		for (Edit edit : edits) {
			apply(panel, edit);
			out.println("== edit | built | " + design + " | " + index + " | " + edit.property() + " "
					+ edit.value());
			lines = new ArrayList<>();
			general(panel, lines, false);
			state(c, "", lines);
			print(lines);
		}
		panel.invalidateMe();
		dialog.dispose();
	}

	static void multi(String design, OpenRocketDocument doc, int mainIndex, int otherIndex, List<Edit> edits)
			throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		List<RocketComponent> comps = preorder(rocket);
		RocketComponent main = comps.get(mainIndex);
		RocketComponent other = comps.get(otherIndex);
		out.println("== multi | built | " + design + " | " + mainIndex + " | " + otherIndex + " | "
				+ main.getClass().getSimpleName());
		List<String> lines = new ArrayList<>();
		state(main, "main ", lines);
		state(other, "other ", lines);
		print(lines);
		main.addConfigListener(other);
		JDialog dialog = new JDialog();
		RocketComponentConfig panel = makePanel(doc, main, dialog);
		for (Edit edit : edits) {
			apply(panel, edit);
			out.println("== multi edit | built | " + design + " | " + edit.property() + " " + edit.value());
			lines = new ArrayList<>();
			state(main, "main ", lines);
			state(other, "other ", lines);
			print(lines);
		}
		panel.invalidateMe();
		dialog.dispose();
		main.clearConfigListeners();
	}

	static void apply(RocketComponentConfig panel, Edit edit) throws Exception {
		if (edit.value().equals("click")) {
			JCheckBox box = checkBox(panel, edit.property());
			if (box == null) {
				throw new IllegalStateException("no check box " + edit.property());
			}
			box.doClick();
			return;
		}
		JSpinner spinner = spinner(panel, edit.property());
		if (spinner == null) {
			throw new IllegalStateException("no spinner " + edit.property());
		}
		if (edit.value().startsWith("int:")) {
			spinner.getModel().setValue(Integer.valueOf(edit.value().substring(4)));
		} else {
			spinner.getModel().setValue(edit.value());
		}
	}

	static RocketComponentConfig makePanel(OpenRocketDocument doc, RocketComponent c, JDialog dialog) {
		if (c.getClass() == TubeFinSet.class) {
			return new TubeFinSetConfig(doc, c, dialog);
		}
		if (c.getClass() == LaunchLug.class) {
			return new LaunchLugConfig(doc, c, dialog);
		}
		return new RailButtonConfig(doc, c, dialog);
	}

	static String d(double value) {
		return Double.toString(value);
	}

	/** The component's values: booleans and counts always, lengths and angles where no sine enters. */
	static void state(RocketComponent c, String prefix, List<String> lines) {
		if (c instanceof TubeFinSet t) {
			lines.add("state " + prefix + "FinCount=" + t.getFinCount());
			lines.add("state " + prefix + "OuterRadiusAutomatic=" + t.isOuterRadiusAutomatic());
			lines.add("state " + prefix + "Length=" + d(t.getLength()));
			lines.add("state " + prefix + "BaseRotation=" + d(t.getBaseRotation()));
			if (!t.isOuterRadiusAutomatic()) {
				lines.add("state " + prefix + "OuterRadius=" + d(t.getOuterRadius()));
				lines.add("state " + prefix + "Thickness=" + d(t.getThickness()));
			}
		} else if (c instanceof LaunchLug l) {
			lines.add("state " + prefix + "Length=" + d(l.getLength()));
			lines.add("state " + prefix + "OuterRadius=" + d(l.getOuterRadius()));
			lines.add("state " + prefix + "Thickness=" + d(l.getThickness()));
			lines.add("state " + prefix + "AngleOffset=" + d(l.getAngleOffset()));
			lines.add("state " + prefix + "InstanceCount=" + l.getInstanceCount());
			lines.add("state " + prefix + "InstanceSeparation=" + d(l.getInstanceSeparation()));
		} else if (c instanceof RailButton b) {
			lines.add("state " + prefix + "OuterDiameter=" + d(b.getOuterDiameter()));
			lines.add("state " + prefix + "InnerDiameter=" + d(b.getInnerDiameter()));
			lines.add("state " + prefix + "BaseHeight=" + d(b.getBaseHeight()));
			lines.add("state " + prefix + "FlangeHeight=" + d(b.getFlangeHeight()));
			lines.add("state " + prefix + "TotalHeight=" + d(b.getTotalHeight()));
			lines.add("state " + prefix + "ScrewHeight=" + d(b.getScrewHeight()));
			lines.add("state " + prefix + "AngleOffset=" + d(b.getAngleOffset()));
			lines.add("state " + prefix + "InstanceCount=" + b.getInstanceCount());
			lines.add("state " + prefix + "InstanceSeparation=" + d(b.getInstanceSeparation()));
		}
	}

	static void print(List<String> lines) {
		Collections.sort(lines);
		for (String line : lines) {
			out.println(line);
		}
	}

	// ----------------------------------------------------------------------------- the General tab

	static void general(RocketComponentConfig panel, List<String> lines, boolean withLabels) throws Exception {
		JTabbedPane tabs = tabs(panel);
		if (withLabels) {
			lines.add("tab0 " + q(tabs.getTitleAt(0)) + " tip=" + q(tabs.getToolTipTextAt(0)));
		}
		walk((Container) tabs.getComponentAt(0), lines, withLabels);
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
			if (c instanceof MaterialPanel) {
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
				checkLine(b, lines);
				continue;
			}
			if (c instanceof JComboBox<?> box) {
				lines.add("combo " + q(String.valueOf(box.getSelectedItem())) + " enabled=" + box.isEnabled());
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

	static void checkLine(JCheckBox b, List<String> lines) throws Exception {
		lines.add("check " + property(b) + " " + q(b.getText()) + " checked=" + b.isSelected() + " enabled="
				+ b.isEnabled() + " tip=" + q(b.getToolTipText()));
	}

	static JCheckBox checkBox(Container root, String property) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof JCheckBox b && property.equals(property(b))) {
				return b;
			}
			if (c instanceof Container k) {
				JCheckBox found = checkBox(k, property);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	static JSpinner spinner(Container root, String property) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof JSpinner s && property.equals(modelName(outer(s.getModel())))) {
				return s;
			}
			if (c instanceof Container k) {
				JSpinner found = spinner(k, property);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	/** The property of a check box: its BooleanModel's name, or its DoubleModel's name + "Automatic". */
	static String property(JCheckBox b) throws Exception {
		Action a = b.getAction();
		if (a instanceof BooleanModel bm) {
			return (String) get(bm, BooleanModel.class, "valueName");
		}
		if (a != null && a.getClass().getName().endsWith("AutomaticActionModel")) {
			return valueName((DoubleModel) outer(a)) + "Automatic";
		}
		return "?";
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
		Module swingPrefs = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ApplicationPreferences.class).to(info.openrocket.swing.gui.util.SwingPreferences.class);
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
