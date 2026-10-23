package probe;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.UUID;

import javax.swing.tree.DefaultTreeSelectionModel;
import javax.swing.tree.TreePath;
import javax.swing.tree.TreeSelectionModel;

import com.google.inject.Guice;
import com.google.inject.Injector;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.CenteringRing;
import info.openrocket.core.rocketcomponent.EngineBlock;
import info.openrocket.core.rocketcomponent.InnerTube;
import info.openrocket.core.rocketcomponent.NoseCone;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.ParallelStage;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.main.DocumentSelectionModel;
import info.openrocket.swing.gui.main.OpenRocketClipboard;
import info.openrocket.swing.gui.main.RocketActions;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import sun.misc.Unsafe;

/**
 * Runs the component edit actions of RocketActions (cut, copy, paste, duplicate, delete, move up/down)
 * headless: the RocketActions object and its inner actions are allocated without their constructors
 * (which need a BasicFrame and a SimulationPanel) and their private methods are called by reflection.
 * An exception at parentFrame.selectTab() (the last statement of each action) is expected.
 */
public class ActionsProbe {
	static Unsafe U;
	static OpenRocketDocument doc;
	static Rocket rocket;
	static RocketActions ra;
	static DefaultTreeSelectionModel tsm;
	static DocumentSelectionModel dsm;
	static Set<UUID> originalIds;

	public static void main(String[] args) throws Exception {
		Injector injector = Guice.createInjector(new ServicesForTesting(), new PluginModule());
		Application.setInjector(injector);
		Field f = Unsafe.class.getDeclaredField("theUnsafe");
		f.setAccessible(true);
		U = (Unsafe) f.get(null);

		// Copy and paste
		scenario("copy [BT1], paste on BT2", new String[] {"BT1"}, "CopyAction", "copyComponents", new String[] {"BT2"}, "paste");
		scenario("copy [BT1], paste on Sustainer", new String[] {"BT1"}, "CopyAction", "copyComponents", new String[] {"Sustainer"}, "paste");
		scenario("copy [BT1], paste on IT (nowhere fits)", new String[] {"BT1"}, "CopyAction", "copyComponents", new String[] {"IT"}, "paste");
		scenario("copy [BT1], paste with nothing selected", new String[] {"BT1"}, "CopyAction", "copyComponents", new String[] {}, "paste");
		scenario("copy [BT1, IT] (parent and one child)", new String[] {"BT1", "IT"}, "CopyAction", "copyComponents", new String[] {"Booster"}, "paste");
		scenario("copy [IT, Fins] (two siblings)", new String[] {"IT", "Fins"}, "CopyAction", "copyComponents", new String[] {"BT2"}, "paste");
		scenario("copy [Fins, IT] (two siblings, reverse order)", new String[] {"Fins", "IT"}, "CopyAction", "copyComponents", new String[] {"BT2"}, "paste");
		scenario("copy [NC, BT2], paste on BT1", new String[] {"NC", "BT2"}, "CopyAction", "copyComponents", new String[] {"BT1"}, "paste");
		scenario("copy [NC, Chute] (different parents), paste on BT1", new String[] {"NC", "Chute"}, "CopyAction", "copyComponents", new String[] {"BT1"}, "paste");
		scenario("copy [Sustainer], paste on Rocket", new String[] {"Sustainer"}, "CopyAction", "copyComponents", new String[] {"Rocket"}, "paste");
		scenario("copy [Sustainer], paste on BT1", new String[] {"Sustainer"}, "CopyAction", "copyComponents", new String[] {"BT1"}, "paste");
		scenario("copy [Rocket]", new String[] {"Rocket"}, "CopyAction", "copyComponents", new String[] {"Sustainer"}, "paste");
		scenario("copy [Chute], paste on [BT1, NC] (first selected decides)", new String[] {"Chute"}, "CopyAction", "copyComponents", new String[] {"BT1", "NC"}, "paste");
		scenario("copy [Chute], paste on [NC, BT1] (first selected decides)", new String[] {"Chute"}, "CopyAction", "copyComponents", new String[] {"NC", "BT1"}, "paste");

		// Cut
		scenario("cut [BT1]", new String[] {"BT1"}, "CutAction", "cutComponents", new String[] {"BT2"}, "paste");
		scenario("cut [BT1, IT]", new String[] {"BT1", "IT"}, "CutAction", "cutComponents", new String[] {"BT2"}, "paste");
		scenario("cut [Booster]", new String[] {"Booster"}, "CutAction", "cutComponents", new String[] {"Rocket"}, "paste");
		scenario("cut [Sustainer, Booster] (all stages)", new String[] {"Sustainer", "Booster"}, "CutAction", "cutComponents", null, null);
		scenario("cut [Pods-Booster] (parallel stage)", new String[] {"Side boosters"}, "CutAction", "cutComponents", new String[] {"BBT"}, "paste");

		// Duplicate
		scenario("duplicate [BT1]", new String[] {"BT1"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [IT]", new String[] {"IT"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [NC, BT2]", new String[] {"NC", "BT2"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [BT1, IT]", new String[] {"BT1", "IT"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [NC, IT] (different parents)", new String[] {"NC", "IT"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [IT, NC] (different parents, reverse)", new String[] {"IT", "NC"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [BT1, Chute] (different parents)", new String[] {"BT1", "Chute"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [Sustainer]", new String[] {"Sustainer"}, "DuplicateAction", "duplicateComponents", null, null);
		scenario("duplicate [Rocket]", new String[] {"Rocket"}, "DuplicateAction", "duplicateComponents", null, null);

		// Delete
		scenario("delete [BT1]", new String[] {"BT1"}, "DeleteAction", "deleteComponents", null, null);
		scenario("delete [NC, BT2, IT]", new String[] {"NC", "BT2", "IT"}, "DeleteAction", "deleteComponents", null, null);
		scenario("delete [BT1, IT] (parent and child)", new String[] {"BT1", "IT"}, "DeleteAction", "deleteComponents", null, null);
		scenario("delete [Booster]", new String[] {"Booster"}, "DeleteAction", "deleteComponents", null, null);
		scenario("delete [Sustainer, Booster] (all stages)", new String[] {"Sustainer", "Booster"}, "DeleteAction", "deleteComponents", null, null);
		scenario("delete [Rocket, NC]", new String[] {"Rocket", "NC"}, "DeleteAction", "deleteComponents", null, null);
		singleStage("delete the only stage");

		// Move up / down
		scenario("move up [BT2]", new String[] {"BT2"}, "MoveUpAction", "actionPerformed", null, null);
		scenario("move up [CR1, Fins]", new String[] {"CR1", "Fins"}, "MoveUpAction", "actionPerformed", null, null);
		scenario("move up [NC] (first child)", new String[] {"NC"}, "MoveUpAction", "actionPerformed", null, null);
		scenario("move up [IT, BT2] (IT is first in BT1)", new String[] {"IT", "BT2"}, "MoveUpAction", "actionPerformed", null, null);
		scenario("move up [BT1, IT] (parent and child)", new String[] {"BT1", "IT"}, "MoveUpAction", "actionPerformed", null, null);
		scenario("move down [NC, BT1]", new String[] {"NC", "BT1"}, "MoveDownAction", "actionPerformed", null, null);
		scenario("move down [BT2] (last child)", new String[] {"BT2"}, "MoveDownAction", "actionPerformed", null, null);
		scenario("move up [Booster]", new String[] {"Booster"}, "MoveUpAction", "actionPerformed", null, null);

		crossDocumentPaste();
		clipboardIsolation();
	}

	static void setField(Object o, Class<?> c, String name, Object value) throws Exception {
		Field f = c.getDeclaredField(name);
		f.setAccessible(true);
		f.set(o, value);
	}

	static Object action(String name) throws Exception {
		Class<?> c = Class.forName("info.openrocket.swing.gui.main.RocketActions$" + name);
		Object a = U.allocateInstance(c);
		setField(a, c, "this$0", ra);
		return a;
	}

	/** Calls a method; returns a note about an exception (expected at the end: selectTab on the null frame). */
	static String call(Object target, String method) throws Exception {
		Method m = null;
		for (Class<?> c = target.getClass(); c != null && m == null; c = c.getSuperclass()) {
			for (Method mm : c.getDeclaredMethods()) {
				if (mm.getName().equals(method) && (mm.getParameterCount() == 0 || method.equals("actionPerformed"))) {
					m = mm;
				}
			}
		}
		m.setAccessible(true);
		try {
			if (m.getParameterCount() == 0) m.invoke(target);
			else m.invoke(target, new Object[] {null});
			return "";
		} catch (InvocationTargetException e) {
			Throwable t = e.getCause();
			StackTraceElement top = t.getStackTrace()[0];
			StackTraceElement at = null;
			for (StackTraceElement s : t.getStackTrace()) {
				if (s.getClassName().startsWith("info.openrocket.swing.gui.main.RocketActions")) { at = s; break; }
			}
			return " [" + t.getClass().getSimpleName() + " at " + (at != null ? at.getClassName().replace("info.openrocket.swing.gui.main.", "") + ":" + at.getLineNumber() : top.toString()) + "]";
		}
	}

	static boolean enabled(String actionName) throws Exception {
		Object a = action(actionName);
		String note = call(a, "clipboardChanged");
		boolean e = ((javax.swing.Action) a).isEnabled();
		if (!note.isEmpty()) return false; // simulationPanel (null) asked: the component part was false
		return e;
	}

	static RocketComponent find(String name) {
		for (RocketComponent c : rocket) {
			if (c.getName().equals(name)) return c;
		}
		throw new IllegalArgumentException(name);
	}

	static void build(boolean twoStages) throws Exception {
		doc = OpenRocketDocumentFactory.createNewRocket();
		rocket = doc.getRocket();
		AxialStage s = (AxialStage) rocket.getChild(0);
		NoseCone nc = new NoseCone(); nc.setName("NC"); s.addChild(nc);
		BodyTube bt1 = new BodyTube(); bt1.setName("BT1"); s.addChild(bt1);
		InnerTube it = new InnerTube(); it.setName("IT"); bt1.addChild(it);
		EngineBlock eb = new EngineBlock(); eb.setName("EB"); it.addChild(eb);
		CenteringRing cr = new CenteringRing(); cr.setName("CR1"); bt1.addChild(cr);
		TrapezoidFinSet fins = new TrapezoidFinSet(); fins.setName("Fins"); bt1.addChild(fins);
		BodyTube bt2 = new BodyTube(); bt2.setName("BT2"); s.addChild(bt2);
		Parachute chute = new Parachute(); chute.setName("Chute"); bt2.addChild(chute);
		if (twoStages) {
			AxialStage booster = new AxialStage(); booster.setName("Booster"); rocket.addChild(booster);
			BodyTube bbt = new BodyTube(); bbt.setName("BBT"); booster.addChild(bbt);
			ParallelStage ps = new ParallelStage(); ps.setName("Side boosters"); bbt.addChild(ps);
			BodyTube pbt = new BodyTube(); pbt.setName("PBT"); ps.addChild(pbt);
		}
		originalIds = new HashSet<>();
		for (RocketComponent c : rocket) originalIds.add(c.getID());

		ra = (RocketActions) U.allocateInstance(RocketActions.class);
		tsm = new DefaultTreeSelectionModel();
		tsm.setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
		dsm = new DocumentSelectionModel(doc);
		dsm.attachComponentTreeSelectionModel(tsm);
		setField(ra, RocketActions.class, "document", doc);
		setField(ra, RocketActions.class, "rocket", rocket);
		setField(ra, RocketActions.class, "selectionModel", dsm);
	}

	static void select(String[] names) {
		tsm.clearSelection();
		for (String n : names) {
			tsm.addSelectionPath(ComponentTreeModel.makeTreePath(find(n)));
		}
	}

	static String tree(RocketComponent c) {
		StringBuilder sb = new StringBuilder();
		sb.append(c.getName());
		if (!originalIds.contains(c.getID())) sb.append('*');
		if (c.getChildCount() > 0) {
			sb.append('[');
			for (int i = 0; i < c.getChildCount(); i++) {
				if (i > 0) sb.append(", ");
				sb.append(tree(c.getChild(i)));
			}
			sb.append(']');
		}
		return sb.toString();
	}

	static String clip() {
		List<String> l = new ArrayList<>();
		for (RocketComponent c : OpenRocketClipboard.getClipboardComponents()) {
			l.add(tree(c) + (c.getParent() != null ? "(parent " + c.getParent().getName() + ")" : ""));
		}
		return l.toString();
	}

	static String selection() {
		List<String> l = new ArrayList<>();
		for (RocketComponent c : dsm.getSelectedComponents()) l.add(c.getName() + (originalIds.contains(c.getID()) ? "" : "*"));
		return l.toString();
	}

	static String enables() throws Exception {
		return "cut=" + enabled("CutAction") + " copy=" + enabled("CopyAction") + " paste=" + enabled("PasteAction")
				+ " dup=" + enabled("DuplicateAction") + " del=" + enabled("DeleteAction") + " up=" + enabled("MoveUpAction")
				+ " down=" + enabled("MoveDownAction") + " edit=" + enabled("EditAction");
	}

	static void scenario(String label, String[] sel, String actionName, String method, String[] pasteSel, String then) throws Exception {
		System.out.println("== " + label);
		build(true);
		OpenRocketClipboard.setClipboard(new ArrayList<>());
		select(sel);
		System.out.println("  enabled with " + selection() + ": " + enables());
		String note = call(action(actionName), method);
		System.out.println("  " + actionName + note + ": tree=" + tree(rocket));
		System.out.println("  clipboard=" + clip() + " selection=" + selection() + " undo='" + doc.getUndoDescription() + "'");
		if (then != null) {
			select(pasteSel);
			System.out.println("  paste enabled with " + selection() + ": " + enabled("PasteAction"));
			note = call(action("PasteAction"), "pasteComponents");
			System.out.println("  paste" + note + ": tree=" + tree(rocket));
			System.out.println("  selection=" + selection() + " undo='" + doc.getUndoDescription() + "'");
			note = call(action("PasteAction"), "pasteComponents");
			System.out.println("  paste again" + note + ": tree=" + tree(rocket));
		}
		if (doc.isUndoAvailable()) {
			doc.undo();
			System.out.println("  after undo: tree=" + tree(rocket) + " redo='" + doc.getRedoDescription() + "'");
		}
	}

	static void singleStage(String label) throws Exception {
		System.out.println("== " + label);
		build(false);
		select(new String[] {"Sustainer"});
		System.out.println("  enabled with " + selection() + ": " + enables());
		String note = call(action("DeleteAction"), "deleteComponents");
		System.out.println("  DeleteAction" + note + ": tree=" + tree(rocket) + " undo='" + doc.getUndoDescription() + "'");
		select(new String[] {"Sustainer"});
		note = call(action("CutAction"), "cutComponents");
		System.out.println("  CutAction" + note + ": tree=" + tree(rocket) + " clipboard=" + clip());
	}

	static void crossDocumentPaste() throws Exception {
		System.out.println("== copy [BT1] in one document, paste into another");
		build(true);
		select(new String[] {"BT1"});
		call(action("CopyAction"), "copyComponents");
		OpenRocketDocument first = doc;
		build(false);
		select(new String[] {"Sustainer"});
		String note = call(action("PasteAction"), "pasteComponents");
		System.out.println("  paste" + note + ": tree=" + tree(rocket) + " first document unchanged: " + tree(first.getRocket()));
	}

	static void clipboardIsolation() throws Exception {
		System.out.println("== the clipboard holds copies: changing the original after copy does not change the paste");
		build(true);
		select(new String[] {"BT1"});
		call(action("CopyAction"), "copyComponents");
		find("BT1").setName("BT1 renamed");
		find("IT").getParent().removeChild(find("IT"));
		select(new String[] {"BT2"});
		String note = call(action("PasteAction"), "pasteComponents");
		System.out.println("  paste" + note + ": tree=" + tree(rocket));
	}
}
