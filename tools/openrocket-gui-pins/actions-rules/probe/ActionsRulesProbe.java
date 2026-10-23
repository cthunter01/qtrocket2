package probe;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import javax.swing.Action;
import javax.swing.tree.DefaultTreeSelectionModel;
import javax.swing.tree.TreeSelectionModel;

import com.google.inject.Guice;
import com.google.inject.Injector;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.startup.Application;
import info.openrocket.core.util.TestRockets;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.OpenRocketClipboard;
import info.openrocket.swing.gui.main.RocketActions;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import info.openrocket.swing.gui.util.SwingPreferences;
import sun.misc.Unsafe;

/**
 * M2 run 2, the fixer of group T (review finding fidelity-2): the enabled states of RocketActions'
 * component actions over OpenRocket's 13 test rockets (TestRockets, in the order of the goldens),
 * run headless on OpenRocket 5f164fd0e. For each rocket: the last component in pre-order hidden
 * (setVisible(false)), then the selections nothing, each component alone, and each component with
 * the one before it in pre-order (in that order: the component first); for each the states of
 * Edit, Cut, Copy, Paste, Duplicate, Delete, Deselect all, Move up, Move down, Hide/Show (the
 * menu's and the popup's) and Show all, after clipboardChanged(), with an empty clipboard. The
 * components are listed in pre-order with their index (0 = the Rocket), the hidden one marked; a
 * selection is written as their indexes. RocketActions is allocated
 * without its constructor (it needs a BasicFrame), as in the scouting's ActionsProbe; its
 * simulationPanel is null, so a rule whose component part is false and that then asks the
 * simulation panel throws: that state is the component part, false.
 */
public class ActionsRulesProbe {
	static Unsafe U;
	static RocketActions ra;
	static DefaultTreeSelectionModel tsm;
	static DocumentSelectionModel dsm;

	static final String[] MAKERS = {"makeEstesAlphaIII", "makeBeta", "makeSimple2Stage", "makeBigBlue",
			"makeIsoHaisu", "makeFalcon9Heavy", "makeMultiStageEventTestRocket", "makeEndPlateRocket",
			"makeEstesAlphaIIIWithPods", "makeEstesAlphaIIIWithMotorPods", "makeEstesAlphaIIIWithSecondMotor",
			"makeEstesAlphaIIIwithInlinePod", "makeClusterPods"};
	static final String[] ACTIONS = {"EditAction", "CutAction", "CopyAction", "PasteAction", "DuplicateAction",
			"DeleteAction", "DeselectAllAction", "MoveUpAction", "MoveDownAction", "ToggleVisibilityAction",
			"ContextMenuToggleVisibilityAction", "ShowAllComponentsAction"};

	public static void main(String[] args) throws Exception {
		Injector injector = Guice.createInjector(new ServicesForTesting(), new PluginModule());
		Application.setInjector(injector);
		((SwingPreferences) Application.getPreferences()).loadDefaultComponentMaterials();
		Field f = Unsafe.class.getDeclaredField("theUnsafe");
		f.setAccessible(true);
		U = (Unsafe) f.get(null);
		OpenRocketClipboard.setClipboard(new ArrayList<>());
		System.out.println("# states: " + String.join(" ", ACTIONS));
		for (String maker : MAKERS) {
			run(maker);
		}
	}

	static void setField(Object o, Class<?> c, String name, Object value) throws Exception {
		Field f = c.getDeclaredField(name);
		f.setAccessible(true);
		f.set(o, value);
	}

	static Object action(String name) throws Exception {
		Class<?> c = Class.forName("info.openrocket.swing.gui.main.RocketActions$" + name);
		Object a = U.allocateInstance(c);
		// Every inner class of the hierarchy has its own outer reference (the context menu's
		// visibility action extends the menu's).
		for (Class<?> k = c; k != null && k.getName().startsWith("info.openrocket.swing.gui.main.RocketActions$"); k = k.getSuperclass()) {
			setField(a, k, "this$0", ra);
		}
		return a;
	}

	static boolean enabled(String name) throws Exception {
		Object a = action(name);
		Method m = a.getClass().getDeclaredMethod("clipboardChanged");
		m.setAccessible(true);
		try {
			m.invoke(a);
		} catch (InvocationTargetException e) {
			if (!(e.getCause() instanceof NullPointerException)) throw e;
			if (System.getenv("PROBE_DEBUG") != null) e.getCause().printStackTrace();
			return false; // the simulation panel (null) was asked: the component part was false
		}
		return ((Action) a).isEnabled();
	}

	static void run(String maker) throws Exception {
		Rocket rocket = (Rocket) TestRockets.class.getMethod(maker).invoke(null);
		OpenRocketDocument doc = OpenRocketDocumentFactory.createDocumentFromRocket(rocket);
		ra = (RocketActions) U.allocateInstance(RocketActions.class);
		tsm = new DefaultTreeSelectionModel();
		tsm.setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
		dsm = new DocumentSelectionModel(doc);
		dsm.attachComponentTreeSelectionModel(tsm);
		setField(ra, RocketActions.class, "document", doc);
		setField(ra, RocketActions.class, "rocket", rocket);
		setField(ra, RocketActions.class, "selectionModel", dsm);

		List<RocketComponent> components = new ArrayList<>();
		for (RocketComponent c : rocket) components.add(c);
		components.get(components.size() - 1).setVisible(false);
		System.out.println("== " + maker);
		for (int i = 0; i < components.size(); i++) {
			System.out.println(i + " " + components.get(i).getName() + (components.get(i).isVisible() ? "" : " (hidden)"));
		}
		for (int i = 0; i < components.size(); i++) {
			print(components, i);
			if (i > 0) print(components, i, i - 1);
		}
		print(components);
	}

	static void print(List<RocketComponent> components, int... indexes) throws Exception {
		tsm.clearSelection();
		for (int i : indexes) tsm.addSelectionPath(ComponentTreeModel.makeTreePath(components.get(i)));
		List<String> selected = new ArrayList<>();
		for (RocketComponent c : dsm.getSelectedComponents()) selected.add(Integer.toString(components.indexOf(c)));
		StringBuilder states = new StringBuilder();
		for (String a : ACTIONS) states.append(enabled(a) ? '1' : '0');
		System.out.println("[" + String.join(", ", selected) + "] " + states);
	}
}
