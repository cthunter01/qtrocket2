package probe;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Module;
import com.google.inject.util.Modules;

import info.openrocket.core.database.ComponentPresetDao;
import info.openrocket.core.database.ComponentPresetDatabase;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.preferences.ApplicationPreferences;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.adaptors.PresetModel;
import info.openrocket.swing.gui.util.SwingPreferences;

/**
 * What OpenRocket's PresetModel (the model of a component dialog's preset combo box) offers and
 * does, for QtRocket's PresetBindingTests.cpp (values pinned by hand: PresetGuiProbe prints this
 * transcript second, and hand-pinned.txt keeps it): its items
 * before and after favourites, its selected item, and what setSelectedItem() does to the rocket
 * (the events, the presets and the undo history), for one body tube and for two (the second a
 * config listener of the first, as a multi-component dialog makes it).
 *
 * The presets are QtRocket's small test database (tests/core/rocket/preset/TestPresetDatabase.h):
 * the files given as arguments, loaded in their order, each file's presets added with
 * Database.addAll() (add() of each). SwingPreferences keeps the favourites, in the fresh
 * java.util.prefs store generate.sh gives every probe. PresetModel's parent is null: its setSelectedItem() moves the focus of the
 * dialog (RocketComponentConfig.setFocusElement()) after it changed the rocket, which throws a
 * NullPointerException here; the probe catches it.
 */
public class PresetModelProbe {

	static final ComponentPresetDatabase presets = new ComponentPresetDatabase();
	static final List<String> events = new ArrayList<>();

	static void bootstrap(String[] files) throws Exception {
		Module overrides = new AbstractModule() {
			@Override
			protected void configure() {
				bind(ComponentPresetDao.class).toInstance(presets);
				// swing's PreferencesForTesting keeps no favourites (its getComponentFavorites() is null).
				bind(ApplicationPreferences.class).to(SwingPreferences.class);
			}
		};
		Application.setInjector(Guice.createInjector(Modules.override(new ServicesForTesting()).with(overrides),
				new PluginModule()));
		for (String path : files) {
			File file = new File(path);
			try (InputStream in = new FileInputStream(file)) {
				presets.addAll(new OpenRocketComponentLoader().load(in, file.getName()));
			}
		}
	}

	static String text(Object item) {
		return item == null ? "null" : item.toString();
	}

	static String quoted(String s) {
		return s == null ? "null" : "'" + s + "'";
	}

	static void items(String title, PresetModel model) {
		StringBuilder line = new StringBuilder();
		for (int i = 0; i < model.getSize(); i++) {
			line.append(i == 0 ? "" : " | ").append(text(model.getElementAt(i)));
		}
		System.out.println("  " + title + ": size " + model.getSize() + ": " + line + "; selected " +
				text(model.getSelectedItem()));
	}

	static String tube(BodyTube tube) {
		ComponentPreset preset = tube.getPresetComponent();
		return tube.getName() + " " + (preset == null ? "null" : preset.getManufacturer().getSimpleName() + " " +
				preset.getPartNo()) + " OR " + tube.getOuterRadius() + " L " + tube.getLength();
	}

	static void history(String title, Design design) {
		StringBuilder line = new StringBuilder();
		for (BodyTube t : design.tubes()) {
			line.append("; ").append(tube(t));
		}
		System.out.println("  " + title + ": undo " + quoted(design.document.getUndoDescription()) + " redo " +
				quoted(design.document.getRedoDescription()) + line);
	}

	static void printEvents(String title) {
		System.out.println("  " + title + " events: " + String.join(", ", events));
		events.clear();
	}

	static void choose(PresetModel model, Object item) {
		try {
			model.setSelectedItem(item);
		} catch (NullPointerException e) {
			// RocketComponentConfig.setFocusElement() on the null parent, after the preset was loaded.
		}
	}

	/**
	 * A new design whose stage holds the body tubes "A" and "B", its events recorded. An undo
	 * replaces the rocket's components (Rocket.loadFrom()): tubes() looks them up again, and a
	 * PresetModel is made for each scenario anew (OpenRocket closes the dialog on an undo).
	 */
	static final class Design {
		final OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
		final BodyTube a = new BodyTube();
		final BodyTube b = new BodyTube();

		Design() {
			Rocket rocket = document.getRocket();
			AxialStage stage = (AxialStage) rocket.getChild(0);
			a.setName("A");
			b.setName("B");
			stage.addChild(a);
			stage.addChild(b);
			document.clearUndo();
			rocket.addComponentChangeListener(e -> events.add(e.getType() + " " + e.getSource().getName()));
		}

		List<BodyTube> tubes() {
			List<BodyTube> list = new ArrayList<>();
			for (var child : document.getRocket().getChild(0).getChildren()) {
				list.add((BodyTube) child);
			}
			return list;
		}
	}

	public static void main(String[] args) throws Exception {
		run(args);
	}

	/** Prints the transcript for the given preset files (PresetGuiProbe runs it second). */
	static void run(String[] files) throws Exception {
		bootstrap(files);
		ApplicationPreferences preferences = Application.getPreferences();
		List<ComponentPreset> tubes = presets.listForType(ComponentPreset.Type.BODY_TUBE);
		System.out.println("presets " + presets.size() + ", body tubes " + tubes.size());
		for (int i = 0; i < tubes.size(); i++) {
			ComponentPreset p = tubes.get(i);
			System.out.println("  " + i + ": " + p.preferenceKey() + " OR " + p.get(ComponentPreset.OUTER_DIAMETER) / 2 +
					" L " + p.get(ComponentPreset.LENGTH));
		}
		ComponentPreset first = tubes.get(7);
		ComponentPreset second = tubes.get(2);
		ComponentPreset other = tubes.get(10);

		System.out.println("== items");
		Design design = new Design();
		items("no favourite", new PresetModel(null, design.document, design.a));
		preferences.setComponentFavorite(first, ComponentPreset.Type.BODY_TUBE, true);
		preferences.setComponentFavorite(second, ComponentPreset.Type.BODY_TUBE, true);
		System.out.println("  favourites " + new java.util.TreeSet<>(
				preferences.getComponentFavorites(ComponentPreset.Type.BODY_TUBE)));
		PresetModel model = new PresetModel(null, design.document, design.a);
		items("two favourites", model);
		events.clear();

		System.out.println("== one tube, a clean session");
		design.document.addUndoPosition("Modify Body Tube");
		choose(model, first);
		printEvents("chosen");
		history("chosen", design);
		items("chosen", model);
		choose(model, first);
		printEvents("chosen again");
		history("chosen again", design);
		design.a.setLength(0.25);
		events.clear();
		history("length edited", design);
		choose(model, first);
		printEvents("chosen after the edit");
		history("chosen after the edit", design);
		design.document.undo();
		history("one undo", design);
		design.document.undo();
		history("two undos", design);
		events.clear();

		System.out.println("== not a favourite");
		Design loose = new Design();
		loose.a.loadPreset(other);
		PresetModel looseModel = new PresetModel(null, loose.document, loose.a);
		events.clear();
		items("other", looseModel);
		choose(looseModel, looseModel.getElementAt(0));
		printEvents("Custom");
		history("Custom", loose);
		items("Custom", looseModel);
		choose(looseModel, looseModel.getElementAt(0));
		printEvents("Custom again");

		System.out.println("== two tubes, B a config listener of A");
		Design two = new Design();
		PresetModel twoModel = new PresetModel(null, two.document, two.a);
		two.a.addConfigListener(two.b);
		two.document.addUndoPosition("Modify Body Tube");
		two.a.setLength(0.4);
		events.clear();
		history("edited", two);
		choose(twoModel, second);
		printEvents("chosen");
		history("chosen", two);
		two.document.undo();
		history("one undo", two);
		two.document.redo();
		events.clear();
		history("redo", two);

		System.out.println("== the main component's preset chosen again, the other's another one");
		Design again = new Design();
		again.a.loadPreset(first);
		again.b.loadPreset(second);
		PresetModel againModel = new PresetModel(null, again.document, again.a);
		again.a.addConfigListener(again.b);
		again.document.clearUndo();
		again.document.addUndoPosition("Modify Body Tube");
		events.clear();
		choose(againModel, first);
		printEvents("chosen");
		history("chosen", again);
		choose(againModel, againModel.getElementAt(0));
		printEvents("Custom");
		history("Custom", again);
		items("Custom", againModel);
	}
}
