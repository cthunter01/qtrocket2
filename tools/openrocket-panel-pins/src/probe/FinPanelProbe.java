package probe;

import java.awt.Component;
import java.awt.Container;
import java.io.File;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JFrame;
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
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.EllipticalFinSet;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.MassObject;
import info.openrocket.core.rocketcomponent.RingComponent;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.SymmetricComponent;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.position.AxialMethod;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.adaptors.EnumModel;
import info.openrocket.swing.gui.adaptors.IntegerModel;
import info.openrocket.swing.gui.components.UnitSelector;
import info.openrocket.swing.gui.configdialog.ComponentConfigDialog;
import info.openrocket.swing.gui.configdialog.EllipticalFinSetConfig;
import info.openrocket.swing.gui.configdialog.FinSetConfig;
import info.openrocket.swing.gui.configdialog.MaterialPanel;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;
import info.openrocket.swing.gui.configdialog.TrapezoidFinSetConfig;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.widgets.GroupableAndSearchableComboBox;

/**
 * What QtRocket's TrapezoidFinSetConfig and EllipticalFinSetConfig (with their base FinSetConfig) are compared with
 * (FinPanelPins.h): OpenRocket's TrapezoidFinSetConfig and EllipticalFinSetConfig (with their base FinSetConfig)
 * under Xvfb.
 * <ul>
 * <li>Part A, "panel": the panel of every trapezoidal and elliptical fin set of the 16 examples (loaded without
 * presets) and the 13 test rockets: the titles of the tabs General and Fin tabs, their widgets (labels with their
 * tooltips, spinner texts, unit selectors, slider positions, combo boxes and the button "Calculate automatically";
 * the material panel and the fillet material combo box left out), the buttons "Split fins" and "Convert to
 * freeform", and the component's values.</li>
 * <li>Part B, "edit": edits through the panel's own models on designs built here (the text a spinner commits, a
 * count, a combo box item, a button clicked), after each the widgets but the labels, the buttons and the values.</li>
 * <li>Part C, "multi": edits of a panel with a second fin set selected (Java: its config listener), after each the
 * values of both.</li>
 * <li>Part D, "compute": FinSetConfig.computeFinTabLength() on FinSetConfigTest's nine inputs: the length it
 * returns and the tab offset it gives the model.</li>
 * <li>Part E, "dialog": "Convert to freeform", "Split fins" and "Calculate automatically" through
 * ComponentConfigDialog itself, with the events processed: the dialog then open, the rocket's tree and the undo
 * history.</li>
 * </ul>
 * Each design is settled (the golden harness's AutomaticDimensions) before every panel.
 * <p>
 * Arguments: the directory of the 16 examples, the transcript file (OpenRocket's log goes to standard output, its
 * warnings and errors only), the motor database (initial_motors.db). Run it through generate.sh.
 */
public class FinPanelProbe {
	static final String[] MAKERS = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods"};

	static PrintStream out;

	public static void main(String[] args) throws Exception {
		out = new PrintStream(new FileOutputStream(args[1]), true, StandardCharsets.UTF_8);
		((ch.qos.logback.classic.Logger) org.slf4j.LoggerFactory.getLogger(org.slf4j.Logger.ROOT_LOGGER_NAME))
				.setLevel(ch.qos.logback.classic.Level.WARN);
		bootstrap(args[2]);
		Thread.setDefaultUncaughtExceptionHandler(
				(t, e) -> out.println("uncaught " + e.getClass().getName()));
		SwingUtilities.invokeAndWait(() -> {
			try {
				UITheme.Themes.LIGHT.applyTheme();
				Thread.currentThread().setUncaughtExceptionHandler(
						(t, e) -> out.println("uncaught " + e.getClass().getName()));
				run(new File(args[0]));
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
		});
		dialogs();
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
		compute();
	}

	static boolean isMyKind(RocketComponent c) {
		return c.getClass() == TrapezoidFinSet.class || c.getClass() == EllipticalFinSet.class;
	}

	/** Part A: the panel of every trapezoidal and elliptical fin set. */
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
			tabsLines(panel, lines, true);
			state(c, "", lines, true);
			print(lines);
			panel.invalidateMe();
			dialog.dispose();
		}
	}

	// ----------------------------------------------------------------------------- the built designs

	/**
	 * An edit: a property and the text a spinner commits, "int:<n>" for a count, "select:<NAME>" for a combo box
	 * item, "button" for the button "Calculate automatically".
	 */
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

	/** A motor mount: an inner tube of the outer radius r, the length len, at the bottom of its parent. */
	static InnerTube mount(double r, double len, double bottomOffset) {
		InnerTube tube = new InnerTube();
		tube.setOuterRadius(r);
		tube.setThickness(0.0005);
		tube.setLength(len);
		tube.setAxialMethod(AxialMethod.BOTTOM);
		tube.setAxialOffset(bottomOffset);
		return tube;
	}

	/** A centering ring of the length len at the offset from the top of its parent; automatic radii. */
	static CenteringRing ring(double top, double len) {
		CenteringRing ring = new CenteringRing();
		ring.setLength(len);
		ring.setAxialMethod(AxialMethod.TOP);
		ring.setAxialOffset(top);
		return ring;
	}

	/** A trapezoidal fin set with the root chord root at the bottom of its parent. */
	static TrapezoidFinSet trapezoid(double root, double tip, double height, double sweep) {
		TrapezoidFinSet fins = new TrapezoidFinSet();
		fins.setRootChord(root);
		fins.setTipChord(tip);
		fins.setHeight(height);
		fins.setSweep(sweep);
		return fins;
	}

	static void built() throws Exception {
		// Part B: edits of one fin set.
		edits("trapezoid", onBodyTube(new TrapezoidFinSet()), 3, List.of(e("FinCount", "int:4"),
				e("FinCount", "int:1"), e("FinCount", "int:9"), e("CantAngle", "5"), e("CantAngle", "20"),
				e("RootChord", "8"), e("TipChord", "3"), e("Height", "4"), e("Sweep", "2"), e("SweepAngle", "30"),
				e("CrossSection", "select:AIRFOIL"), e("Thickness", "0.4"), e("BaseRotation", "200"),
				e("TabLength", "3"), e("TabHeight", "1"), e("TabHeight", "5"), e("TabOffset", "1"),
				e("TabOffsetMethod", "select:TOP"), e("TabOffset", "-2"), e("TabOffsetMethod", "select:AFTER"),
				e("FilletRadius", "0.5"), e("FilletRadius", "-1"), e("RootChord", "2"), e("Height", "0")));
		edits("elliptical", onBodyTube(new EllipticalFinSet()), 3, List.of(e("FinCount", "int:2"),
				e("CantAngle", "-3"), e("Length", "7"), e("Height", "3"), e("CrossSection", "select:ROUNDED"),
				e("Thickness", "0.2"), e("BaseRotation", "-90"), e("TabLength", "4"), e("TabHeight", "0.5"),
				e("TabOffsetMethod", "select:BOTTOM"), e("TabOffset", "0.5"), e("FilletRadius", "0.3"),
				e("Length", "3")));

		// "Calculate automatically" on designs with motor mounts and centering rings.
		edits("autotab plain", onBodyTube(trapezoid(0.08, 0.04, 0.05, 0.03)), 3,
				List.of(e("AutoCalc", "button")));
		edits("autotab mount", onBodyTube(mount(0.0125, 0.1, 0), ring(0.21, 0.005), ring(0.27, 0.005),
				trapezoid(0.08, 0.04, 0.05, 0.03)), 6, List.of(e("AutoCalc", "button"), e("TabHeight", "0.3"),
				e("TabOffsetMethod", "select:MIDDLE"), e("AutoCalc", "button")));
		edits("autotab rings ahead", onBodyTube(mount(0.0125, 0.15, 0), ring(0.16, 0.005), ring(0.18, 0.005),
				trapezoid(0.08, 0.04, 0.05, 0.03)), 6, List.of(e("AutoCalc", "button")));
		edits("autotab one ring within", onBodyTube(mount(0.0125, 0.15, 0), ring(0.17, 0.005),
				ring(0.25, 0.005), trapezoid(0.08, 0.04, 0.05, 0.03)), 6, List.of(e("AutoCalc", "button")));
		edits("autotab ring behind", onBodyTube(mount(0.0125, 0.15, 0), ring(0.24, 0.005), ring(0.295, 0.005),
				trapezoid(0.08, 0.04, 0.05, 0.03)), 6, List.of(e("AutoCalc", "button")));
		edits("autotab adjacent rings", onBodyTube(mount(0.0125, 0.15, 0), ring(0.22, 0.005), ring(0.225, 0.005),
				ring(0.28, 0.005), trapezoid(0.08, 0.04, 0.05, 0.03)), 7, List.of(e("AutoCalc", "button")));
		InnerTube shortMount = mount(0.0125, 0.05, 0.15);
		edits("autotab tube ahead", onBodyTube(shortMount, ring(0.12, 0.005), trapezoid(0.08, 0.04, 0.05, 0.03)),
				5, List.of(e("AutoCalc", "button")));
		CenteringRing small1 = ring(0.23, 0.005);
		small1.setOuterRadiusAutomatic(false);
		small1.setOuterRadius(0.01);
		CenteringRing small2 = ring(0.27, 0.005);
		small2.setOuterRadiusAutomatic(false);
		small2.setOuterRadius(0.0125);
		edits("autotab small rings", onBodyTube(mount(0.0125, 0.1, 0), small1, small2,
				trapezoid(0.08, 0.04, 0.05, 0.03)), 6, List.of(e("AutoCalc", "button")));
		TrapezoidFinSet middle = trapezoid(0.08, 0.04, 0.05, 0.03);
		middle.setAxialMethod(AxialMethod.MIDDLE);
		middle.setAxialOffset(0.01);
		edits("autotab middle fins", onBodyTube(mount(0.0125, 0.2, 0), ring(0.1, 0.004), ring(0.13, 0.004),
				ring(0.2, 0.004), middle), 7, List.of(e("TabOffsetMethod", "select:BOTTOM"),
				e("AutoCalc", "button")));
		EllipticalFinSet ellipse = new EllipticalFinSet();
		ellipse.setLength(0.07);
		edits("autotab elliptical", onBodyTube(mount(0.012, 0.1, 0), ring(0.235, 0.005), ring(0.29, 0.005),
				ellipse), 6, List.of(e("AutoCalc", "button")));

		// Part C: edits of a panel with a second fin set selected (Java: its config listener).
		TrapezoidFinSet finsB = trapezoid(0.06, 0.02, 0.03, 0.01);
		finsB.setFinCount(4);
		multi("trapezoid pair", onBodyTube(mount(0.0125, 0.1, 0), ring(0.21, 0.005), ring(0.27, 0.005),
				trapezoid(0.08, 0.04, 0.05, 0.03), finsB), 6, 7,
				List.of(e("SweepAngle", "45"), e("Height", "4"), e("TabLength", "2"), e("RootChord", "7"),
						e("CrossSection", "select:AIRFOIL"), e("FilletRadius", "0.5"),
						e("TabOffsetMethod", "select:MIDDLE"), e("AutoCalc", "button"), e("FinCount", "int:2"),
						e("CantAngle", "3"), e("TabHeight", "2")));
		EllipticalFinSet ellipseB = new EllipticalFinSet();
		ellipseB.setLength(0.04);
		ellipseB.setHeight(0.02);
		multi("elliptical pair", onBodyTube(new EllipticalFinSet(), ellipseB), 3, 4,
				List.of(e("Length", "6"), e("Height", "4"), e("TabHeight", "1"), e("Thickness", "0.5"),
						e("TabOffset", "0.2")));
	}

	static void edits(String design, OpenRocketDocument doc, int index, List<Edit> edits) throws Exception {
		Rocket rocket = doc.getRocket();
		settle(rocket);
		RocketComponent c = preorder(rocket).get(index);
		out.println("== panel | built | " + design + " | " + index + " | " + c.getClass().getSimpleName());
		JDialog dialog = new JDialog();
		RocketComponentConfig panel = makePanel(doc, c, dialog);
		List<String> lines = new ArrayList<>();
		tabsLines(panel, lines, true);
		state(c, "", lines, false);
		print(lines);
		for (Edit edit : edits) {
			apply(panel, edit);
			out.println("== edit | built | " + design + " | " + index + " | " + edit.property() + " "
					+ edit.value());
			lines = new ArrayList<>();
			tabsLines(panel, lines, false);
			state(c, "", lines, false);
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
		state(main, "main ", lines, false);
		state(other, "other ", lines, false);
		print(lines);
		main.addConfigListener(other);
		JDialog dialog = new JDialog();
		RocketComponentConfig panel = makePanel(doc, main, dialog);
		for (Edit edit : edits) {
			apply(panel, edit);
			out.println("== multi edit | built | " + design + " | " + edit.property() + " " + edit.value());
			lines = new ArrayList<>();
			state(main, "main ", lines, false);
			state(other, "other ", lines, false);
			print(lines);
		}
		panel.invalidateMe();
		dialog.dispose();
		main.clearConfigListeners();
	}

	static void apply(RocketComponentConfig panel, Edit edit) throws Exception {
		if (edit.value().equals("button")) {
			JButton button = button(panel, "Calculate automatically");
			if (button == null) {
				throw new IllegalStateException("no button");
			}
			button.doClick();
			return;
		}
		if (edit.value().startsWith("select:")) {
			JComboBox<?> box = combo(panel, edit.property());
			if (box == null) {
				throw new IllegalStateException("no combo " + edit.property());
			}
			String name = edit.value().substring(7);
			for (int i = 0; i < box.getItemCount(); i++) {
				Object item = box.getItemAt(i);
				if (item instanceof Enum<?> en && en.name().equals(name)) {
					box.setSelectedItem(item);
					return;
				}
			}
			throw new IllegalStateException("no item " + name);
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
		if (!isMyKind(c)) {
			throw new IllegalStateException("not a fin set: " + c);
		}
		if (c.getClass() == TrapezoidFinSet.class) {
			return new TrapezoidFinSetConfig(doc, c, dialog);
		}
		return new EllipticalFinSetConfig(doc, c, dialog);
	}

	static String d(double value) {
		return Double.toString(value);
	}

	/**
	 * The component's values: counts, enums and lengths, the maximum tab height only on a body tube; the sweep
	 * exactly when it was read from a design (exact), in micrometres (rounded) after edits, where an edit of the
	 * sweep angle makes it with a tangent.
	 */
	static void state(RocketComponent c, String prefix, List<String> lines, boolean exactSweep) {
		if (!(c instanceof FinSet f)) {
			return;
		}
		lines.add("state " + prefix + "FinCount=" + f.getFinCount());
		lines.add("state " + prefix + "CantAngle=" + d(f.getCantAngle()));
		lines.add("state " + prefix + "Thickness=" + d(f.getThickness()));
		lines.add("state " + prefix + "CrossSection=" + f.getCrossSection().name());
		lines.add("state " + prefix + "BaseRotation=" + d(f.getBaseRotation()));
		lines.add("state " + prefix + "TabHeight=" + d(f.getTabHeight()));
		lines.add("state " + prefix + "TabLength=" + d(f.getTabLength()));
		lines.add("state " + prefix + "TabOffset=" + d(f.getTabOffset()));
		lines.add("state " + prefix + "TabOffsetMethod=" + f.getTabOffsetMethod().name());
		lines.add("state " + prefix + "TabFrontEdge=" + d(f.getTabFrontEdge()));
		lines.add("state " + prefix + "FilletRadius=" + d(f.getFilletRadius()));
		lines.add("state " + prefix + "FilletMaterial=" + f.getFilletMaterial().getName());
		if (f.getParent() instanceof BodyTube) {
			lines.add("state " + prefix + "MaxTabHeight=" + d(f.getMaxTabHeight()));
		}
		if (c instanceof TrapezoidFinSet t) {
			lines.add("state " + prefix + "RootChord=" + d(t.getRootChord()));
			lines.add("state " + prefix + "TipChord=" + d(t.getTipChord()));
			lines.add("state " + prefix + "Height=" + d(t.getHeight()));
			if (exactSweep) {
				lines.add("state " + prefix + "Sweep=" + d(t.getSweep()));
			} else {
				lines.add("state " + prefix + "SweepUm=" + Math.round(t.getSweep() * 1e6));
			}
		} else if (c instanceof EllipticalFinSet el) {
			lines.add("state " + prefix + "Length=" + d(el.getLength()));
			lines.add("state " + prefix + "Height=" + d(el.getHeight()));
		}
	}

	static void print(List<String> lines) {
		Collections.sort(lines);
		for (String line : lines) {
			out.println(line);
		}
	}

	// ----------------------------------------------------------------------------- the tabs

	/** The tabs General and Fin tabs (their titles with the labels), and the buttons of the button row. */
	static void tabsLines(RocketComponentConfig panel, List<String> lines, boolean withLabels) throws Exception {
		JTabbedPane tabs = tabs(panel);
		if (withLabels) {
			lines.add("tab0 " + q(tabs.getTitleAt(0)) + " tip=" + q(tabs.getToolTipTextAt(0)));
			lines.add("tab1 " + q(tabs.getTitleAt(1)) + " tip=" + q(tabs.getToolTipTextAt(1)));
		}
		List<String> general = new ArrayList<>();
		walk((Container) tabs.getComponentAt(0), general, withLabels);
		for (String line : general) {
			lines.add("general " + line);
		}
		List<String> finTabs = new ArrayList<>();
		walk((Container) tabs.getComponentAt(1), finTabs, withLabels);
		for (String line : finTabs) {
			lines.add("fintabs " + line);
		}
		for (String text : new String[] {"Split fins", "Convert to freeform"}) {
			JButton b = button(panel, text);
			if (b != null) {
				lines.add("row " + buttonLine(b));
			}
		}
	}

	static String buttonLine(JButton b) {
		return "button " + q(b.getText()) + " enabled=" + b.isEnabled() + " tip=" + q(b.getToolTipText());
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
			if (c instanceof MaterialPanel || c instanceof GroupableAndSearchableComboBox) {
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
				lines.add("check " + q(b.getText()) + " checked=" + b.isSelected());
				continue;
			}
			if (c instanceof JComboBox<?> box) {
				lines.add("combo " + q(String.valueOf(box.getSelectedItem())) + " enabled=" + box.isEnabled());
				continue;
			}
			if (c instanceof JButton b) {
				lines.add(buttonLine(b));
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

	static JButton button(Container root, String text) {
		for (Component c : root.getComponents()) {
			if (c instanceof JButton b && text.equals(b.getText())) {
				return b;
			}
			if (c instanceof Container k) {
				JButton found = button(k, text);
				if (found != null) {
					return found;
				}
			}
		}
		return null;
	}

	static JComboBox<?> combo(Container root, String property) throws Exception {
		for (Component c : root.getComponents()) {
			if (c instanceof JComboBox<?> box && box.getModel() instanceof EnumModel<?> em
					&& property.equals(get(em, EnumModel.class, "valueName"))) {
				return box;
			}
			if (c instanceof Container k) {
				JComboBox<?> found = combo(k, property);
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

	// ----------------------------------------------------------------------------- Part D

	/** FinSetConfigTest's nine inputs, each with the length returned and the tab offset given to the model. */
	static void compute() throws Exception {
		Method method = FinSetConfig.class.getDeclaredMethod("computeFinTabLength", List.class, Double.class,
				Double.class, DoubleModel.class, RocketComponent.class);
		method.setAccessible(true);

		// testComputeFinTabLength
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			RocketComponent parent = new BodyTube();
			Double result = (Double) method.invoke(null, rings, 10.0d, 11.0d, dm, parent);
			computeLine("testComputeFinTabLength empty", result, dm);
			dm = new DoubleModel(1.0d);
			result = (Double) method.invoke(null, null, 10.0d, 11.0d, dm, parent);
			computeLine("testComputeFinTabLength null", result, dm);
		}
		// testCompute2LeadingRings
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			RocketComponent parent = new BodyTube();
			CenteringRing ring1 = topRing(0.004, 0.43);
			CenteringRing ring2 = topRing(0.004, 0.45);
			rings.add(ring1);
			rings.add(ring2);
			parent.addChild(ring1);
			parent.addChild(ring2);
			computeLine("testCompute2LeadingRings", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent), dm);
		}
		// testCompute1Ring
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = topRing(0.004, 0.43);
			rings.add(ring1);
			RocketComponent parent = new BodyTube();
			parent.addChild(ring1);
			computeLine("testCompute1Ring", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent), dm);
		}
		// testComputeOneLeadingOneRingWithinRoot
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = topRing(0.004, 0.43);
			CenteringRing ring2 = topRing(0.004, 0.45);
			rings.add(ring1);
			rings.add(ring2);
			RocketComponent parent = new BodyTube(1.0d, 0.01);
			parent.addChild(ring1);
			parent.addChild(ring2);
			computeLine("testComputeOneLeadingOneRingWithinRoot",
					(Double) method.invoke(null, rings, 0.45d, 0.01, dm, parent), dm);
		}
		// testComputeOneLeadingOneTrailingRing (the rings are in no parent)
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = topRing(0.004, 0.43);
			CenteringRing ring2 = topRing(0.004, 0.48);
			rings.add(ring1);
			rings.add(ring2);
			computeLine("testComputeOneLeadingOneTrailingRing",
					(Double) method.invoke(null, rings, 0.47d, 0.01, dm, ring1), dm);
		}
		// testComputeOneWithinRootOneTrailingRing
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = topRing(0.004, 0.4701);
			CenteringRing ring2 = topRing(0.004, 0.48);
			rings.add(ring1);
			rings.add(ring2);
			RocketComponent parent = new BodyTube(1.0d, 0.1d);
			parent.addChild(ring1);
			parent.addChild(ring2);
			computeLine("testComputeOneWithinRootOneTrailingRing",
					(Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent), dm);
		}
		// testBothRingsWithinRootChord
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			RocketComponent parent = new BodyTube(1.0000d, 0.1d);
			CenteringRing ring1 = topRing(0.004, 0.4701);
			parent.addChild(ring1);
			CenteringRing ring2 = topRing(0.004, 0.4750);
			parent.addChild(ring2);
			rings.add(ring1);
			rings.add(ring2);
			computeLine("testBothRingsWithinRootChord", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent),
					dm);
		}
		// testBothRingsBeyondRootChord
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = topRing(0.004, 0.48);
			CenteringRing ring2 = topRing(0.004, 0.49);
			rings.add(ring1);
			rings.add(ring2);
			RocketComponent parent = new BodyTube(1.0d, 0.1d);
			parent.addChild(ring1);
			parent.addChild(ring2);
			computeLine("testBothRingsBeyondRootChord", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent),
					dm);
		}
		// test3RingsWithinRootChord
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			CenteringRing ring1 = absoluteRing(0.004, 0.47);
			CenteringRing ring2 = absoluteRing(0.004, 0.4702);
			CenteringRing ring3 = absoluteRing(0.004, 0.4770);
			rings.add(ring1);
			rings.add(ring2);
			rings.add(ring3);
			BodyTube parent = new BodyTube(1.0d, 0.1d);
			parent.setAxialOffset(0);
			parent.addChild(ring1);
			parent.addChild(ring2);
			parent.addChild(ring3);
			computeLine("test3RingsWithinRootChord", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent),
					dm);
		}
		// Unsorted rings within 1 mm of each other (the comparator truncates to millimetres).
		{
			DoubleModel dm = new DoubleModel(1.0d);
			List<CenteringRing> rings = new ArrayList<>();
			BodyTube parent = new BodyTube(1.0d, 0.1d);
			double[] tops = {0.4705, 0.4740, 0.4712, 0.4699, 0.4768, 0.4751};
			for (double top : tops) {
				CenteringRing ring = topRing(0.0006, top);
				parent.addChild(ring);
				rings.add(ring);
			}
			computeLine("unsorted rings", (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent), dm);
		}
		// Rings within 1 mm, the later one higher: the comparator keeps their order, so they merge.
		computeTops("rings within a millimetre", new double[] {0.4705, 0.4700}, 0.0002);
		computeTops("rings within a millimetre reversed", new double[] {0.4700, 0.4705}, 0.0002);
		computeTops("descending rings", new double[] {0.4760, 0.4740, 0.4720, 0.4705}, 0.0002);
		computeTops("mixed rings", new double[] {0.4712, 0.4705, 0.4718, 0.4699, 0.4790, 0.4771}, 0.0002);
	}

	/** computeFinTabLength() of rings of the length len at the tops, in that order, for the fin at 0.47, 1 cm. */
	static void computeTops(String name, double[] tops, double len) throws Exception {
		Method method = FinSetConfig.class.getDeclaredMethod("computeFinTabLength", List.class, Double.class,
				Double.class, DoubleModel.class, RocketComponent.class);
		method.setAccessible(true);
		DoubleModel dm = new DoubleModel(1.0d);
		List<CenteringRing> rings = new ArrayList<>();
		BodyTube parent = new BodyTube(1.0d, 0.1d);
		for (double top : tops) {
			CenteringRing ring = topRing(len, top);
			parent.addChild(ring);
			rings.add(ring);
		}
		computeLine(name, (Double) method.invoke(null, rings, 0.47d, 0.01, dm, parent), dm);
	}

	static CenteringRing topRing(double length, double offset) {
		CenteringRing ring = new CenteringRing();
		ring.setLength(length);
		ring.setAxialMethod(AxialMethod.TOP);
		ring.setAxialOffset(offset);
		return ring;
	}

	static CenteringRing absoluteRing(double length, double offset) {
		CenteringRing ring = new CenteringRing();
		ring.setAxialMethod(AxialMethod.ABSOLUTE);
		ring.setLength(length);
		ring.setAxialOffset(offset);
		return ring;
	}

	static void computeLine(String name, Double result, DoubleModel dm) {
		out.println("== compute | " + name);
		out.println("result=" + d(result) + " offset=" + d(dm.getValue()));
	}

	// ----------------------------------------------------------------------------- Part E

	static void edt(ThrowingRunnable r) throws Exception {
		SwingUtilities.invokeAndWait(() -> {
			try {
				r.run();
			} catch (Exception ex) {
				throw new RuntimeException(ex);
			}
		});
	}

	interface ThrowingRunnable {
		void run() throws Exception;
	}

	/** Lets the EDT process what is queued (the invokeLater of the buttons, the window events). */
	static void flush() throws Exception {
		for (int i = 0; i < 3; i++) {
			edt(() -> {
			});
		}
	}

	static JFrame frame;

	static void dialogs() throws Exception {
		edt(() -> {
			frame = new JFrame();
		});
		convert("convert one", 0, false);
		convert("convert one from fin tabs", 0, true);
		convert("convert two", 1, false);
		convert("convert three", 2, false);
		convert("convert four", 3, false);
		split("split one", 0, 3);
		split("split two", 1, 3);
		split("split three", 2, 3);
		split("split single fin", 0, 1);
		autoCalcDialog();
		edt(() -> frame.dispose());
	}

	/** Fin sets on a body tube: the main one at index 3, others after it; each with a name of its own. */
	static OpenRocketDocument finsDocument(int others, int finCount) {
		List<RocketComponent> sets = new ArrayList<>();
		for (int i = 0; i <= others; i++) {
			TrapezoidFinSet fins = trapezoid(0.05 + 0.01 * i, 0.03, 0.04, 0.02);
			fins.setFinCount(finCount);
			if (i > 0) {
				fins.setName("Trapezoidal fin set " + (i + 1));
			}
			sets.add(fins);
		}
		return onBodyTube(sets.toArray(new RocketComponent[0]));
	}

	static OpenRocketDocument current;

	static void showFor(OpenRocketDocument doc, int others) throws Exception {
		current = doc;
		edt(() -> {
			List<RocketComponent> comps = preorder(doc.getRocket());
			RocketComponent main = comps.get(3);
			for (int i = 1; i <= others; i++) {
				main.addConfigListener(comps.get(3 + i));
			}
			ComponentConfigDialog.showDialog(frame, doc, main);
		});
		flush();
	}

	static RocketComponentConfig configurator() throws Exception {
		ComponentConfigDialog dialog = ComponentConfigDialog.getDialog();
		Field f = ComponentConfigDialog.class.getDeclaredField("configurator");
		f.setAccessible(true);
		return (RocketComponentConfig) f.get(dialog);
	}

	static void dialogState(OpenRocketDocument doc) throws Exception {
		edt(() -> {
			ComponentConfigDialog dialog = ComponentConfigDialog.getDialog();
			if (dialog == null || !dialog.isVisible()) {
				out.println("dialog none");
			} else {
				RocketComponent c = dialog.getComponent();
				StringBuilder others = new StringBuilder();
				for (RocketComponent listener : c.getConfigListeners()) {
					others.append(" ").append(q(listener.getName()));
				}
				out.println("dialog " + c.getClass().getSimpleName() + " " + q(c.getName()) + " title="
						+ q(dialog.getTitle()) + " tab=" + q(dialog.getSelectedTabName()) + " others=["
						+ others.toString().trim() + "]");
			}
			treeLines(doc);
			out.println("undo=" + q(doc.getUndoDescription()) + " redo=" + q(doc.getRedoDescription()));
		});
	}

	static void treeLines(OpenRocketDocument doc) {
		List<RocketComponent> comps = preorder(doc.getRocket());
		for (int i = 0; i < comps.size(); i++) {
			RocketComponent c = comps.get(i);
			String line = "tree " + i + " " + c.getClass().getSimpleName() + " " + q(c.getName());
			if (c instanceof FinSet f) {
				line += " fins=" + f.getFinCount() + " rotation=" + Math.round(Math.toDegrees(f.getBaseRotation()))
						+ " bypass=" + c.isBypassComponentChangeEvent() + " listeners=" + c.getConfigListeners().size();
			}
			out.println(line);
		}
	}

	static void closeAndUndo(OpenRocketDocument doc) throws Exception {
		edt(() -> ComponentConfigDialog.disposeDialog());
		flush();
		out.println("-- after closing");
		dialogState(doc);
		edt(() -> {
			if (doc.isUndoAvailable()) {
				doc.undo();
			} else {
				out.println("undo not available");
			}
		});
		flush();
		out.println("-- after undo");
		dialogState(doc);
	}

	static void convert(String name, int others, boolean finTabsTab) throws Exception {
		out.println("== dialog | " + name);
		OpenRocketDocument doc = finsDocument(others, 3);
		showFor(doc, others);
		if (finTabsTab) {
			edt(() -> configurator().setSelectedTab("Fin tabs"));
		}
		out.println("-- shown");
		dialogState(doc);
		edt(() -> button(configurator(), "Convert to freeform").doClick());
		flush();
		out.println("-- after convert");
		dialogState(doc);
		closeAndUndo(doc);
	}

	static void split(String name, int others, int finCount) throws Exception {
		out.println("== dialog | " + name);
		OpenRocketDocument doc = finsDocument(others, finCount);
		showFor(doc, others);
		out.println("-- shown");
		dialogState(doc);
		edt(() -> {
			JButton b = button(configurator(), "Split fins");
			out.println("split enabled=" + b.isEnabled());
			if (b.isEnabled()) {
				b.doClick();
			}
		});
		flush();
		out.println("-- after split");
		dialogState(doc);
		closeAndUndo(doc);
	}

	static void autoCalcDialog() throws Exception {
		out.println("== dialog | autocalc");
		OpenRocketDocument doc = onBodyTube(mount(0.0125, 0.1, 0), ring(0.21, 0.005), ring(0.27, 0.005),
				trapezoid(0.08, 0.04, 0.05, 0.03));
		current = doc;
		edt(() -> {
			settle(doc.getRocket());
			ComponentConfigDialog.showDialog(frame, doc, preorder(doc.getRocket()).get(6));
		});
		flush();
		out.println("-- shown");
		dialogState(doc);
		edt(() -> spinner(configurator(), "Thickness").getModel().setValue("0.5"));
		flush();
		out.println("-- after an edit");
		dialogState(doc);
		edt(() -> button(configurator(), "Calculate automatically").doClick());
		flush();
		out.println("-- after calculate");
		dialogState(doc);
		edt(() -> {
			List<String> lines = new ArrayList<>();
			state(preorder(doc.getRocket()).get(6), "", lines, false);
			print(lines);
		});
		edt(() -> ComponentConfigDialog.disposeDialog());
		flush();
		out.println("-- after closing");
		dialogState(doc);
		edt(() -> {
			if (doc.isUndoAvailable()) {
				doc.undo();
			} else {
				out.println("undo not available");
			}
		});
		flush();
		out.println("-- after undo");
		dialogState(doc);
		edt(() -> {
			List<String> lines = new ArrayList<>();
			state(preorder(doc.getRocket()).get(6), "", lines, false);
			print(lines);
		});
		edt(() -> {
			if (doc.isUndoAvailable()) {
				doc.undo();
			} else {
				out.println("undo not available");
			}
		});
		flush();
		out.println("-- after a second undo");
		dialogState(doc);
		edt(() -> {
			List<String> lines = new ArrayList<>();
			state(preorder(doc.getRocket()).get(6), "", lines, false);
			print(lines);
		});
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
