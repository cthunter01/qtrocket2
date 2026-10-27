package probe;

import java.awt.Component;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

import javax.swing.JButton;
import javax.swing.JComboBox;
import javax.swing.JDialog;
import javax.swing.JSpinner;
import javax.swing.JTabbedPane;
import javax.swing.JTextField;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Bulkhead;
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.EllipticalFinSet;
import info.openrocket.core.rocketcomponent.EngineBlock;
import info.openrocket.core.rocketcomponent.FreeformFinSet;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.LaunchLug;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.PodSet;
import info.openrocket.core.rocketcomponent.RailButton;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.ShockCord;
import info.openrocket.core.rocketcomponent.Streamer;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.TubeCoupler;
import info.openrocket.core.rocketcomponent.TubeFinSet;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.configdialog.RocketComponentConfig;

/**
 * The preset row of OpenRocket's component configuration panels, for QtRocket's
 * tests/gui/configdialog/RocketComponentConfigTests.cpp (values pinned by hand: hand-pinned.txt
 * keeps the transcript after PresetGuiProbe's). The panels need a parent dialog, which
 * a headless AWT refuses to make, so it runs under xvfb-run; none is shown.
 *
 * "== the preset row": the panel ComponentConfigDialog makes for each component of a design with a
 * component of every kind (the class it finds by reflection, as findDialogContentsConstructor()
 * does, given a null parent dialog: the panels are never shown), and for a selection of two body
 * tubes and of a body tube with a nose cone (config listeners, as a multi-component dialog makes
 * them): whether it has the preset combo box (RocketComponentConfig.presetComboBox) and its
 * tooltip, maximum row count and editable state, the "Parts Library" button's tooltip, the first
 * entries of the panel's focus order (RocketComponentConfig.order: the name field, the combo box,
 * the button, then the tabs), and the field focused once a preset is chosen (focusElement: the
 * property of its DoubleModel). The presets are PresetModelProbe's database.
 */
public class ConfigPresetRowProbe {

	/** A failure ends the program (the panels' parent dialogs would keep it running). */
	public static void main(String[] args) {
		try {
			bootstrap(args);
			run();
		} catch (Throwable e) {
			e.printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}

	/**
	 * PresetModelProbe's database and preferences, and the application's watch service, which the
	 * panels' appearance tab takes from the injector (EditDecalHelper; ServicesForTesting binds none).
	 */
	static void bootstrap(String[] args) throws Exception {
		PresetModelProbe.bootstrap(args);
		com.google.inject.Module overrides = new com.google.inject.AbstractModule() {
			@Override
			protected void configure() {
				bind(info.openrocket.core.database.ComponentPresetDao.class).toInstance(PresetModelProbe.presets);
				bind(info.openrocket.core.preferences.ApplicationPreferences.class)
						.to(info.openrocket.swing.gui.util.SwingPreferences.class);
				bind(info.openrocket.swing.gui.watcher.WatchService.class)
						.to(info.openrocket.swing.gui.watcher.WatchServiceImpl.class);
			}
		};
		info.openrocket.core.startup.Application.setInjector(com.google.inject.Guice.createInjector(
				com.google.inject.util.Modules.override(new info.openrocket.swing.ServicesForTesting()).with(overrides),
				new info.openrocket.core.plugin.PluginModule()));
	}

	/** Prints the transcript. */
	static void run() throws Exception {
		presetRows();
	}

	// ------------------------------------------------------------------ the preset row

	static Object field(Object object, String name) throws Exception {
		Field f = RocketComponentConfig.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(object);
	}

	@SuppressWarnings("unchecked")
	static RocketComponentConfig panelOf(OpenRocketDocument document, RocketComponent component) throws Exception {
		List<RocketComponent> listeners = component.getConfigListeners();
		if (!component.checkAllClassesEqual(listeners)) {
			return new RocketComponentConfig(document, component, new JDialog());
		}
		for (Class<?> c = component.getClass(); c != null && c != Object.class; c = c.getSuperclass()) {
			try {
				Class<?> config = Class.forName("info.openrocket.swing.gui.configdialog." + c.getSimpleName() + "Config");
				Constructor<? extends RocketComponentConfig> constructor =
						(Constructor<? extends RocketComponentConfig>) config.getConstructor(OpenRocketDocument.class,
								RocketComponent.class, JDialog.class);
				return constructor.newInstance(document, component, new JDialog());
			} catch (ClassNotFoundException | NoSuchMethodException e) {
				// the next superclass
			}
		}
		throw new IllegalStateException("no configurator for " + component);
	}

	static String quoted(String s) {
		return s == null ? "null" : "'" + s + "'";
	}

	/** What an entry of the focus order is. */
	static String orderName(RocketComponentConfig panel, Component entry) throws Exception {
		if (entry == field(panel, "componentNameField")) {
			return "name";
		}
		if (entry != null && entry == field(panel, "presetComboBox")) {
			return "combo";
		}
		if (entry instanceof JButton) {
			return "button " + quoted(((JButton) entry).getText());
		}
		if (entry instanceof JTabbedPane) {
			return "tabs";
		}
		if (entry instanceof JTextField) {
			return "text field";
		}
		return entry == null ? "null" : entry.getClass().getSimpleName();
	}

	/** The property of the DoubleModel of a spinner, or the class of another component. */
	static String focusName(Object element) throws Exception {
		if (element == null) {
			return "null";
		}
		if (element instanceof JSpinner) {
			Object model = ((JSpinner) element).getModel();
			if (model instanceof DoubleModel.ValueSpinnerModel) {
				Field outer = model.getClass().getDeclaredField("this$0");
				outer.setAccessible(true);
				Object doubleModel = outer.get(model);
				Field name = DoubleModel.class.getDeclaredField("valueName");
				name.setAccessible(true);
				return "spinner " + name.get(doubleModel);
			}
			return "spinner " + model.getClass().getSimpleName();
		}
		return element.getClass().getSimpleName();
	}

	static void printRow(String title, OpenRocketDocument document, RocketComponent component) throws Exception {
		RocketComponentConfig panel = panelOf(document, component);
		StringBuilder line = new StringBuilder("  " + title + " (" + panel.getClass().getSimpleName() + "): ");
		JComboBox<?> combo = (JComboBox<?>) field(panel, "presetComboBox");
		if (combo == null) {
			line.append("no combo");
		} else {
			line.append("combo ttip=").append(quoted(combo.getToolTipText())).append(" rows=")
					.append(combo.getMaximumRowCount()).append(" editable=").append(combo.isEditable());
		}
		@SuppressWarnings("unchecked")
		List<Component> order = (List<Component>) field(panel, "order");
		JButton button = null;
		List<String> names = new ArrayList<>();
		for (int i = 0; i < order.size() && names.size() < 4; i++) {
			Component entry = order.get(i);
			if (entry instanceof JButton && combo != null && button == null) {
				button = (JButton) entry;
			}
			names.add(orderName(panel, entry));
		}
		if (button != null) {
			line.append("; button ").append(quoted(button.getText())).append(" ttip=")
					.append(quoted(button.getToolTipText()));
		}
		line.append("; order ").append(String.join(", ", names));
		line.append("; focus ").append(focusName(field(panel, "focusElement")));
		System.out.println(line);
	}

	/** A design with a component of every kind, in tree order. */
	static List<RocketComponent> everyKind(OpenRocketDocument document) {
		Rocket rocket = document.getRocket();
		AxialStage stage = (AxialStage) rocket.getChild(0);
		BodyTube body = new BodyTube();
		stage.addChild(new NoseCone());
		stage.addChild(body);
		stage.addChild(new Transition());
		body.addChild(new TrapezoidFinSet());
		body.addChild(new EllipticalFinSet());
		body.addChild(new FreeformFinSet());
		body.addChild(new TubeFinSet());
		body.addChild(new LaunchLug());
		body.addChild(new RailButton());
		InnerTube inner = new InnerTube();
		body.addChild(inner);
		inner.addChild(new EngineBlock());
		body.addChild(new TubeCoupler());
		body.addChild(new CenteringRing());
		body.addChild(new Bulkhead());
		body.addChild(new MassComponent());
		body.addChild(new ShockCord());
		body.addChild(new Parachute());
		body.addChild(new Streamer());
		PodSet pods = new PodSet();
		body.addChild(pods);
		pods.addChild(new BodyTube());
		ParallelStage boosters = new ParallelStage();
		body.addChild(boosters);
		boosters.addChild(new BodyTube());
		List<RocketComponent> all = new ArrayList<>();
		for (RocketComponent c : rocket) {
			all.add(c);
		}
		return all;
	}

	static void presetRows() throws Exception {
		System.out.println("== the preset row");
		OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
		List<RocketComponent> all = everyKind(document);
		for (RocketComponent c : all) {
			try {
				printRow(c.getClass().getSimpleName(), document, c);
			} catch (java.lang.reflect.InvocationTargetException e) {
				System.out.println("  " + c.getClass().getSimpleName() + ": FAILS " + e.getCause());
			}
		}
		// Multi-component selections: the first component's panel with the others as its config
		// listeners (ComponentConfigDialog's multi-component dialog).
		OpenRocketDocument multi = OpenRocketDocumentFactory.createNewRocket();
		AxialStage stage = (AxialStage) multi.getRocket().getChild(0);
		NoseCone nose = new NoseCone();
		BodyTube a = new BodyTube();
		BodyTube b = new BodyTube();
		stage.addChild(nose);
		stage.addChild(a);
		stage.addChild(b);
		a.addConfigListener(b);
		printRow("BodyTube with BodyTube", multi, a);
		a.clearConfigListeners();
		a.addConfigListener(nose);
		printRow("BodyTube with NoseCone", multi, a);
		a.clearConfigListeners();
	}
}
