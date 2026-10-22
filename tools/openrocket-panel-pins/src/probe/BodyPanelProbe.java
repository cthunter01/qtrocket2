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
import javax.swing.JComponent;
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
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.adaptors.BooleanModel;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.configdialog.BodyTubeConfig;
import info.openrocket.swing.gui.configdialog.MaterialPanel;
import info.openrocket.swing.gui.configdialog.NoseConeConfig;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;
import info.openrocket.swing.gui.configdialog.TransitionConfig;
import info.openrocket.swing.gui.theme.UITheme;

/**
 * What QtRocket's BodyTubeConfig, NoseConeConfig and TransitionConfig are compared with (BodyPanelPins.h): the
 * General tab of OpenRocket's BodyTubeConfig, NoseConeConfig and
 * TransitionConfig opened on every body tube, nose cone and transition of the 16 examples and the 13
 * test rockets (the labels, spinner texts, unit selectors, slider positions, check boxes with their
 * enabled states and tooltips, the shape combo box), the automatic radii after the panel was made (the
 * T3 switch-off), "Flip to tail cone" clicked twice on every nose cone, and the switch-off reaching a
 * second selected component (a config listener).
 *
 * Each design is settled (the golden harness's AutomaticDimensions) before every panel, so that the
 * lazily stored automatic radii are those of the settled state whatever ran before.
 * <p>
 * Arguments: the directory of the 16 examples, the transcript file (OpenRocket's log goes to standard output, its
 * warnings and errors only), the motor database (initial_motors.db). Run it through generate.sh.
 */
public class BodyPanelProbe {
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
		constructed();
	}

	static boolean isBodyKind(RocketComponent c) {
		return c.getClass() == BodyTube.class || c.getClass() == NoseCone.class || c.getClass() == Transition.class;
	}

	static void design(String source, String name, OpenRocketDocument doc) throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		List<RocketComponent> comps = preorder(rocket);
		// Part A: every panel.
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			if (!isBodyKind(c)) {
				continue;
			}
			settle(rocket);
			out.println("== panel | " + source + " | " + name + " | " + i + " | " + c.getClass().getSimpleName());
			JDialog dialog = new JDialog();
			RocketComponentConfig panel = makePanel(doc, c, dialog);
			List<String> lines = new ArrayList<>();
			general(panel, lines);
			state(c, "", lines);
			Collections.sort(lines);
			for (String line : lines) {
				out.println(line);
			}
			panel.invalidateMe();
			dialog.dispose();
		}
		// Part B: "Flip to tail cone" clicked on and off.
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			if (c.getClass() != NoseCone.class) {
				continue;
			}
			settle(rocket);
			JDialog dialog = new JDialog();
			RocketComponentConfig panel = makePanel(doc, c, dialog);
			JCheckBox flip = checkBox(panel, "Flipped");
			for (String step : new String[] {"flip", "unflip"}) {
				flip.doClick();
				out.println("== " + step + " | " + source + " | " + name + " | " + i + " | NoseCone");
				List<String> lines = new ArrayList<>();
				checkLine(checkBox(panel, "BaseRadiusAutomatic"), lines);
				checkLine(flip, lines);
				state(c, "", lines);
				Collections.sort(lines);
				for (String line : lines) {
					out.println(line);
				}
			}
			panel.invalidateMe();
			dialog.dispose();
		}
		// Part C: a panel that disables one of its "Automatic" check boxes, with the first other component of its
		// class whose automatic radius is on as its config listener.
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			if (!isBodyKind(c)) {
				continue;
			}
			settle(rocket);
			int other = -1;
			for (int j = 0; j < comps.size(); j++) {
				if (j != i && comps.get(j).getClass() == c.getClass() && anyAutomatic(comps.get(j))) {
					other = j;
					break;
				}
			}
			if (other < 0) {
				continue;
			}
			// Only a panel that disables one of its "Automatic" check boxes.
			JDialog probeDialog = new JDialog();
			RocketComponentConfig probePanel = makePanel(doc, c, probeDialog);
			boolean disables = false;
			for (String property : new String[] {"OuterRadiusAutomatic", "BaseRadiusAutomatic", "ForeRadiusAutomatic",
					"AftRadiusAutomatic"}) {
				JCheckBox box = checkBox(probePanel, property);
				disables |= box != null && !box.isEnabled();
			}
			probePanel.invalidateMe();
			probeDialog.dispose();
			if (!disables) {
				continue;
			}
			settle(rocket);
			multi(source + " | " + name + " | " + i + " | " + other, doc, c, comps.get(other));
		}
	}

	/** The panel of main with listener as its config listener: the states of both before and after. */
	static void multi(String title, OpenRocketDocument doc, RocketComponent main, RocketComponent listener)
			throws Exception {
		out.println("== multi | " + title + " | " + main.getClass().getSimpleName());
		List<String> lines = new ArrayList<>();
		state(main, "main ", lines);
		state(listener, "other ", lines);
		main.addConfigListener(listener);
		JDialog dialog = new JDialog();
		RocketComponentConfig panel = makePanel(doc, main, dialog);
		state(main, "main after ", lines);
		state(listener, "other after ", lines);
		Collections.sort(lines);
		for (String line : lines) {
			out.println(line);
		}
		panel.invalidateMe();
		dialog.dispose();
		main.clearConfigListeners();
	}

	/**
	 * Designs built here: Rocket > Stage > [nose cone m, body tube t, nose cone n] (t's automatic radius off, m's
	 * automatic base on, m flipped or not), the panel of n (no next component) with m as listener; Rocket > Stage >
	 * [transition t1, transition t2] (t2's automatic fore radius on), the panel of t1 with t2 as listener.
	 */
	static void constructed() throws Exception {
		for (boolean flipped : new boolean[] {false, true}) {
			Rocket rocket = new Rocket();
			AxialStage stage = new AxialStage();
			rocket.addChild(stage);
			NoseCone m = new NoseCone();
			BodyTube t = new BodyTube();
			NoseCone n = new NoseCone();
			stage.addChild(m);
			stage.addChild(t);
			stage.addChild(n);
			t.setOuterRadiusAutomatic(false);
			if (flipped) {
				m.setFlipped(true);
			}
			m.setBaseRadiusAutomatic(true);
			rocket.enableEvents();
			OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
			settle(rocket);
			multi("built | nose cones" + (flipped ? " flipped" : "") + " | 4 | 2", doc, n, m);
		}
		Rocket rocket = new Rocket();
		AxialStage stage = new AxialStage();
		rocket.addChild(stage);
		Transition t1 = new Transition();
		Transition t2 = new Transition();
		stage.addChild(t1);
		stage.addChild(t2);
		t2.setForeRadiusAutomatic(true);
		rocket.enableEvents();
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
		settle(rocket);
		multi("built | transitions | 2 | 3", doc, t1, t2);
	}

	static boolean anyAutomatic(RocketComponent c) {
		if (c instanceof NoseCone n) {
			return n.isBaseRadiusAutomatic();
		}
		if (c instanceof BodyTube t) {
			return t.isOuterRadiusAutomatic();
		}
		if (c instanceof Transition t) {
			return t.isForeRadiusAutomatic() || t.isAftRadiusAutomatic();
		}
		return false;
	}

	static RocketComponentConfig makePanel(OpenRocketDocument doc, RocketComponent c, JDialog dialog) {
		if (c.getClass() == BodyTube.class) {
			return new BodyTubeConfig(doc, c, dialog);
		}
		if (c.getClass() == NoseCone.class) {
			return new NoseConeConfig(doc, c, dialog);
		}
		return new TransitionConfig(doc, c, dialog);
	}

	static void state(RocketComponent c, String prefix, List<String> lines) {
		if (c instanceof NoseCone n) {
			lines.add("state " + prefix + "BaseRadiusAutomatic=" + n.isBaseRadiusAutomatic());
			lines.add("state " + prefix + "Flipped=" + n.isFlipped());
			lines.add("state " + prefix + "ForeRadiusAutomatic=" + n.isForeRadiusAutomatic());
			lines.add("state " + prefix + "AftRadiusAutomatic=" + n.isAftRadiusAutomatic());
		} else if (c instanceof BodyTube t) {
			lines.add("state " + prefix + "OuterRadiusAutomatic=" + t.isOuterRadiusAutomatic());
		} else if (c instanceof Transition t) {
			lines.add("state " + prefix + "ForeRadiusAutomatic=" + t.isForeRadiusAutomatic());
			lines.add("state " + prefix + "AftRadiusAutomatic=" + t.isAftRadiusAutomatic());
		}
	}

	// ----------------------------------------------------------------------------- the General tab

	static void general(RocketComponentConfig panel, List<String> lines) throws Exception {
		JTabbedPane tabs = tabs(panel);
		lines.add("tab0 " + q(tabs.getTitleAt(0)) + " tip=" + q(tabs.getToolTipTextAt(0)));
		lines.add("tab1 " + q(tabs.getTitleAt(1)) + " tip=" + q(tabs.getToolTipTextAt(1)));
		walk((Container) tabs.getComponentAt(0), lines);
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

	static void walk(Container root, List<String> lines) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof MaterialPanel) {
				continue;
			}
			if (c instanceof JSpinner s) {
				DoubleModel m = (DoubleModel) outer(s.getModel());
				String text = ((JSpinner.DefaultEditor) s.getEditor()).getTextField().getText();
				lines.add("spinner " + valueName(m) + " " + q(text) + " enabled=" + s.isEnabled());
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
				if (l.getText() != null && !l.getText().isEmpty()) {
					lines.add("label " + q(l.getText()) + " enabled=" + l.isEnabled());
				}
				continue;
			}
			if (c instanceof Container k) {
				walk(k, lines);
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
