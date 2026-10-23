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
 * More cases of RocketActions' copy rule (fillInMissingSelections, copyComponentsMaintainParent, the
 * clipboard's filter), of the order of the clipboard, of paste positions, duplicate and delete, run
 * headless as the M2 scout's ActionsProbe does (the same tree: Sustainer[NC, BT1[IT[EB], CR1, Fins],
 * BT2[Chute]], Booster[BBT[Side boosters[PBT]]]). An exception at parentFrame.selectTab() (the last
 * statement of each action) is expected.
 */
public class ClipboardProbe {
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

		// The copy rule with deeper selections.
		copy("copy [Sustainer, IT] (a grandchild)", "Sustainer", "IT");
		copy("copy [Sustainer, EB] (a great-grandchild)", "Sustainer", "EB");
		copy("copy [BT1, EB] (a grandchild)", "BT1", "EB");
		copy("copy [IT, BT1] (child first)", "IT", "BT1");
		copy("copy [BT1, IT, CR1, Fins] (all children)", "BT1", "IT", "CR1", "Fins");
		copy("copy [BT1, IT, CR1, Fins, EB] (everything below)", "BT1", "IT", "CR1", "Fins", "EB");
		copy("copy [Sustainer, BT1, EB] (a child and a grandchild of it)", "Sustainer", "BT1", "EB");
		copy("copy [Booster, PBT] (through a booster set)", "Booster", "PBT");
		// The order of the clipboard.
		copy("copy [Chute, BT1, NC]", "Chute", "BT1", "NC");
		copy("copy [Fins, CR1, IT]", "Fins", "CR1", "IT");
		copy("copy [EB, BT2] (different depths)", "EB", "BT2");
		copy("copy [BT2, Booster, NC]", "BT2", "Booster", "NC");
		copy("copy [NC, Rocket] (the Rocket second)", "NC", "Rocket");
		// Paste.
		copyPaste("copy [Fins, CR1, IT], paste on BT2", new String[] {"Fins", "CR1", "IT"}, "BT2");
		copyPaste("copy [Fins, IT], paste on NC", new String[] {"Fins", "IT"}, "NC");
		copyPaste("copy [Chute], paste on Booster", new String[] {"Chute"}, "Booster");
		copyPaste("copy [BT2], paste on Booster", new String[] {"BT2"}, "Booster");
		copyPaste("copy [BT2], paste on PBT", new String[] {"BT2"}, "PBT");
		copyPaste("copy [NC, Chute], paste on EB", new String[] {"NC", "Chute"}, "EB");
		copyPaste("copy [Side boosters], paste on BT2", new String[] {"Side boosters"}, "BT2");
		copyPaste("copy [Sustainer, IT], paste on Rocket", new String[] {"Sustainer", "IT"}, "Rocket");
		// Duplicate.
		action("duplicate [Fins, IT]", "DuplicateAction", "duplicateComponents", "Fins", "IT");
		action("duplicate [Chute, NC]", "DuplicateAction", "duplicateComponents", "Chute", "NC");
		action("duplicate [Sustainer, IT]", "DuplicateAction", "duplicateComponents", "Sustainer", "IT");
		action("duplicate [NC, Rocket]", "DuplicateAction", "duplicateComponents", "NC", "Rocket");
		action("duplicate [Side boosters]", "DuplicateAction", "duplicateComponents", "Side boosters");
		// Cut.
		action("cut [CR1]", "CutAction", "cutComponents", "CR1");
		action("cut [IT, Chute]", "CutAction", "cutComponents", "IT", "Chute");
		action("cut [Fins, NC]", "CutAction", "cutComponents", "Fins", "NC");
		action("cut [NC, Rocket]", "CutAction", "cutComponents", "NC", "Rocket");
		// Delete.
		action("delete [IT, BT1] (child first)", "DeleteAction", "deleteComponents", "IT", "BT1");
		action("delete [Fins, NC, Chute]", "DeleteAction", "deleteComponents", "Fins", "NC", "Chute");
		action("delete [Booster, Sustainer] (all stages, reverse)", "DeleteAction", "deleteComponents", "Booster", "Sustainer");
		action("delete [Side boosters]", "DeleteAction", "deleteComponents", "Side boosters");
		// Move.
		action("move down [CR1, IT]", "MoveDownAction", "actionPerformed", "CR1", "IT");
		action("move up [Fins, CR1]", "MoveUpAction", "actionPerformed", "Fins", "CR1");
		action("move down [Sustainer]", "MoveDownAction", "actionPerformed", "Sustainer");
		action("move up [Fins, IT] (IT first in BT1)", "MoveUpAction", "actionPerformed", "Fins", "IT");
		action("move down [NC, Chute]", "MoveDownAction", "actionPerformed", "NC", "Chute");
		action("move up [BT2, Chute] (a parent and its only child)", "MoveUpAction", "actionPerformed", "BT2", "Chute");
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
		if (!note.isEmpty()) return false;
		return e;
	}

	static RocketComponent find(String name) {
		for (RocketComponent c : rocket) {
			if (c.getName().equals(name)) return c;
		}
		throw new IllegalArgumentException(name);
	}

	static void build() throws Exception {
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
		AxialStage booster = new AxialStage(); booster.setName("Booster"); rocket.addChild(booster);
		BodyTube bbt = new BodyTube(); bbt.setName("BBT"); booster.addChild(bbt);
		ParallelStage ps = new ParallelStage(); ps.setName("Side boosters"); bbt.addChild(ps);
		BodyTube pbt = new BodyTube(); pbt.setName("PBT"); ps.addChild(pbt);
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
		OpenRocketClipboard.setClipboard(new ArrayList<>());
	}

	static void select(String... names) {
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
				+ " down=" + enabled("MoveDownAction");
	}

	static void copy(String label, String... sel) throws Exception {
		System.out.println("== " + label);
		build();
		select(sel);
		System.out.println("  enabled with " + selection() + ": " + enables());
		String note = call(action("CopyAction"), "copyComponents");
		System.out.println("  copy" + note + ": clipboard=" + clip());
	}

	static void copyPaste(String label, String[] sel, String pasteOn) throws Exception {
		System.out.println("== " + label);
		build();
		select(sel);
		String note = call(action("CopyAction"), "copyComponents");
		System.out.println("  copy" + note + ": clipboard=" + clip());
		select(pasteOn);
		note = call(action("PasteAction"), "pasteComponents");
		System.out.println("  paste" + note + ": tree=" + tree(rocket));
		System.out.println("  selection=" + selection() + " undo='" + doc.getUndoDescription() + "'");
	}

	static void action(String label, String actionName, String method, String... sel) throws Exception {
		System.out.println("== " + label);
		build();
		select(sel);
		System.out.println("  enabled with " + selection() + ": " + enables());
		String note = call(action(actionName), method);
		System.out.println("  " + actionName + note + ": tree=" + tree(rocket));
		System.out.println("  clipboard=" + clip() + " selection=" + selection() + " undo='" + doc.getUndoDescription() + "'");
	}
}
