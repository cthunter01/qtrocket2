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
import info.openrocket.core.l10n.Translator;
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
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.gui.components.ConfigurationComboBox;
import info.openrocket.swing.gui.components.StageSelector;
import info.openrocket.swing.gui.theme.UITheme;
import info.openrocket.swing.gui.util.Icons;
import info.openrocket.swing.gui.util.SwingPreferences;

import javax.swing.ComboBoxModel;
import javax.swing.JToggleButton;
import java.awt.Font;
import java.io.File;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.util.ArrayList;
import java.util.EventObject;
import java.util.List;
import java.util.function.Supplier;

/**
 * What QtRocket's part G4 (StageSelector, ConfigurationComboBox) must reproduce: OpenRocket's own StageSelector
 * and ConfigurationComboBox, made as RocketPanel makes them (the selector added to the rocket's listeners), over
 * the 16 examples, the 13 TestRockets and a few designs of the probe's own, with SwingPreferences and FlatLaf
 * Light. The buttons are clicked with doClick() (the user's click), the combo's items picked with
 * setSelectedItem() (what the popup does).
 * usage: RibbonProbe EXAMPLEDIR
 *
 * Output:
 *   D source:design                        a design
 *   B index selected enabled italic icon tooltip | text      each stage button (icon: none|disabled;
 *                                                            tooltip: toggle|lastActive|noChildren|none)
 *   F flags                                isStageActive(n) of the selected configuration for n = 0 .. stages-1
 *   N                                      a design made anew for the clicks that follow
 *   K index                                the button at index clicked
 *   E type source                          each change event the rocket's listeners received (type bits, the
 *                                          source's class)
 *   U size position available description the document's undo history (size, position), isUndoAvailable(),
 *                                          getUndoDescription()
 *   C size selected                        the combo's items and the index of its selected item
 *   I index | text                         each item (FlightConfiguration.toString())
 *   P index                                the item at index picked
 *   S selected rocketSelected | text       the combo's selected index, the index of the rocket's selected
 *                                          configuration, the combo's selected item's text
 *   L                                      the end of a design
 */
public class RibbonProbe {
	static Translator trans;
	static final List<String> EVENTS = new ArrayList<>();

	interface Design {
		Rocket rocket();

		OpenRocketDocument document();
	}

	record Made(Rocket rocket, OpenRocketDocument document) implements Design {
	}

	public static void main(String[] args) throws Exception {
		bootstrap();
		UITheme.Themes.LIGHT.applyTheme();
		trans = Application.getTranslator();
		File dir = new File(args[0]);

		String[] examples = new File(dir, ".").list((d, n) -> n.endsWith(".ork"));
		java.util.Arrays.sort(examples);
		for (String name : examples) {
			scenario("example:" + name, () -> load(dir, name), List.of());
		}
		String[] makers = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue", "makeIsoHaisu",
				"makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket", "makeEstesAlphaIIIWithPods",
				"makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor", "makeEstesAlphaIIIwithInlinePod",
				"makeClusterPods"};
		for (String maker : makers) {
			scenario("testrocket:" + maker, () -> {
				try {
					Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
					settle(rocket);
					return new Made(rocket, null);
				} catch (Exception e) {
					throw new RuntimeException(e);
				}
			}, List.of());
		}

		// A new design: one stage without children.
		scenario("special:new-design", () -> {
			OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
			return new Made(doc.getRocket(), doc);
		}, List.of());
		// The new design with a nose cone: one active stage, the last.
		scenario("special:new-design-with-nose", () -> {
			OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
			doc.getRocket().getChild(0).addChild(new NoseCone());
			return new Made(doc.getRocket(), doc);
		}, List.of());
		// Two stages, the booster's empty.
		scenario("special:simple2stage-empty-booster", () -> {
			Rocket rocket = TestRockets.makeSimple2Stage();
			RocketComponent booster = rocket.getChild(1);
			booster.removeChild(0);
			settle(rocket);
			return new Made(rocket, null);
		}, List.of());
		// The Falcon 9 Heavy with boosters on its boosters: payload off, core off (its boosters too), inner on.
		scenario("special:boosters-on-boosters", RibbonProbe::boostersOnBoosters, List.of(0, 1, 3));
		// The Beta: sustainer off, booster off (none active), sustainer on.
		scenario("special:beta-sequence", () -> {
			Rocket rocket = TestRockets.makeBeta();
			settle(rocket);
			return new Made(rocket, null);
		}, List.of(0, 1, 0));

		// The combo box of a design that does not change the rocket's configuration (the simulation dialog's).
		Design design = load(dir, "Two stage high power rocket.ork");
		ConfigurationComboBox combo = new ConfigurationComboBox(design.rocket(), false);
		System.out.println("D local:Two stage high power rocket.ork");
		listen(design.rocket());
		comboState(combo, design.rocket());
		for (int i = 0; i < combo.getModel().getSize(); i++) {
			pick(combo, design.rocket(), i);
		}
		System.out.println("L");
	}

	static Design boostersOnBoosters() {
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
		return new Made(nested, null);
	}

	/** A design's buttons, each button clicked twice on a fresh design, the sequence, and the combo. */
	static void scenario(String name, Supplier<Design> maker, List<Integer> sequence) throws Exception {
		System.out.println("D " + name);
		Design design = maker.get();
		StageSelector selector = new StageSelector(design.rocket());
		design.rocket().addChangeListener(selector);
		buttons(selector, design.rocket());
		List<JToggleButton> initial = buttonsOf(selector);
		if (sequence.isEmpty()) {
			for (int i = 0; i < initial.size(); i++) {
				if (!initial.get(i).isEnabled()) {
					continue;
				}
				Design fresh = maker.get();
				StageSelector freshSelector = new StageSelector(fresh.rocket());
				fresh.rocket().addChangeListener(freshSelector);
				listen(fresh.rocket());
				System.out.println("N");
				undo(fresh);
				click(freshSelector, fresh, i);
				if (buttonsOf(freshSelector).get(i).isEnabled()) {
					click(freshSelector, fresh, i);
				}
			}
		} else {
			Design fresh = maker.get();
			StageSelector freshSelector = new StageSelector(fresh.rocket());
			fresh.rocket().addChangeListener(freshSelector);
			listen(fresh.rocket());
			System.out.println("N");
			undo(fresh);
			for (int i : sequence) {
				click(freshSelector, fresh, i);
			}
		}

		Design fresh = maker.get();
		ConfigurationComboBox combo = new ConfigurationComboBox(fresh.rocket());
		listen(fresh.rocket());
		comboState(combo, fresh.rocket());
		for (int i = 0; i < combo.getModel().getSize(); i++) {
			pick(combo, fresh.rocket(), i);
		}
		System.out.println("L");
	}

	static void listen(Rocket rocket) {
		rocket.addChangeListener((EventObject e) -> {
			ComponentChangeEvent event = (ComponentChangeEvent) e;
			EVENTS.add("E " + event.getType() + " " + event.getSource().getClass().getSimpleName());
		});
	}

	static void click(StageSelector selector, Design design, int index) throws Exception {
		System.out.println("K " + index);
		EVENTS.clear();
		buttonsOf(selector).get(index).doClick(0);
		EVENTS.forEach(System.out::println);
		EVENTS.clear();
		undo(design);
		buttons(selector, design.rocket());
	}

	static void undo(Design design) throws Exception {
		OpenRocketDocument doc = design.document();
		if (doc == null) {
			return;
		}
		List<?> history = (List<?>) field(doc, OpenRocketDocument.class, "undoHistory");
		int position = (Integer) field(doc, OpenRocketDocument.class, "undoPosition");
		System.out.println("U " + history.size() + " " + position + " " + doc.isUndoAvailable() + " "
				+ doc.getUndoDescription());
	}

	@SuppressWarnings("unchecked")
	static List<JToggleButton> buttonsOf(StageSelector selector) throws Exception {
		return (List<JToggleButton>) field(selector, StageSelector.class, "buttons");
	}

	static void buttons(StageSelector selector, Rocket rocket) throws Exception {
		List<JToggleButton> list = buttonsOf(selector);
		for (int i = 0; i < list.size(); i++) {
			JToggleButton b = list.get(i);
			String icon = b.getIcon() == null ? "none" : b.getIcon() == Icons.COMPONENT_DISABLED ? "disabled" : "other";
			System.out.println("B " + i + " " + bit(b.isSelected()) + " " + bit(b.isEnabled()) + " "
					+ bit((b.getFont().getStyle() & Font.ITALIC) != 0) + " " + icon + " " + tooltip(b.getToolTipText())
					+ " | " + b.getText());
		}
		FlightConfiguration config = rocket.getSelectedConfiguration();
		StringBuilder flags = new StringBuilder();
		for (int n = 0; n < rocket.getStageCount(); n++) {
			flags.append(bit(config.isStageActive(n)));
		}
		System.out.println("F " + flags);
	}

	static String tooltip(String text) {
		if (text == null) {
			return "none";
		}
		if (text.equals(trans.get("RocketPanel.btn.Stages.Toggle.ttip"))) {
			return "toggle";
		}
		if (text.equals(trans.get("RocketPanel.btn.Stages.LastActive.ttip"))) {
			return "lastActive";
		}
		if (text.equals(trans.get("RocketPanel.btn.Stages.NoChildren.ttip"))) {
			return "noChildren";
		}
		return "other:" + text;
	}

	static void comboState(ConfigurationComboBox combo, Rocket rocket) {
		ComboBoxModel<FlightConfiguration> model = combo.getModel();
		System.out.println("C " + model.getSize() + " " + indexOf(model, (FlightConfiguration) combo.getSelectedItem()));
		for (int i = 0; i < model.getSize(); i++) {
			System.out.println("I " + i + " | " + model.getElementAt(i).toString());
		}
	}

	static int indexOf(ComboBoxModel<FlightConfiguration> model, FlightConfiguration config) {
		for (int i = 0; i < model.getSize(); i++) {
			if (model.getElementAt(i).getId().equals(config.getId())) {
				return i;
			}
		}
		return -1;
	}

	static void pick(ConfigurationComboBox combo, Rocket rocket, int index) {
		System.out.println("P " + index);
		EVENTS.clear();
		combo.setSelectedItem(combo.getModel().getElementAt(index));
		EVENTS.forEach(System.out::println);
		EVENTS.clear();
		ComboBoxModel<FlightConfiguration> model = combo.getModel();
		FlightConfiguration selected = (FlightConfiguration) combo.getSelectedItem();
		System.out.println("S " + indexOf(model, selected) + " " + indexOf(model, rocket.getSelectedConfiguration())
				+ " | " + selected.toString());
	}

	static String bit(boolean b) {
		return b ? "1" : "0";
	}

	static Object field(Object o, Class<?> type, String name) throws Exception {
		Field f = type.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(o);
	}

	static Design load(File dir, String name) {
		try {
			OpenRocketDocument doc = new GeneralRocketLoader(new File(dir, name)).load();
			Rocket rocket = doc.getRocket();
			settle(rocket);
			return new Made(rocket, doc);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	static void bootstrap() throws Exception {
		Path bundledDb = Path.of(System.getProperty("probe.motors"));
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
