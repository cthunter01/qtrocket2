package probe;

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import javax.swing.JComponent;
import javax.swing.TransferHandler;
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
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.ServicesForTesting;
import info.openrocket.swing.gui.components.BasicTree;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeModel;
import info.openrocket.swing.gui.main.componenttree.ComponentTreeTransferHandler;
import info.openrocket.swing.gui.main.componenttree.RocketComponentTransferable;

/**
 * Runs ComponentTreeTransferHandler's drag (createTransferable) and the loop of importData() over
 * its private checkImportAction() and importDataTarget(), headless (a TransferSupport for a drop
 * cannot be made without a display). The compatibility check of canImport() is printed beside it.
 */
public class DndProbe {
	static OpenRocketDocument doc;
	static Rocket rocket;
	static BasicTree tree;
	static ComponentTreeTransferHandler handler;

	public static void main(String[] args) throws Exception {
		Injector injector = Guice.createInjector(new ServicesForTesting(), new PluginModule());
		Application.setInjector(injector);

		drop("move [BT2] to (Sustainer, 0)", new String[] {"BT2"}, "Sustainer", 0, TransferHandler.MOVE);
		drop("move [NC] to (Sustainer, 3) (end)", new String[] {"NC"}, "Sustainer", 3, TransferHandler.MOVE);
		drop("move [NC] to (Sustainer, 1) (same place)", new String[] {"NC"}, "Sustainer", 1, TransferHandler.MOVE);
		drop("move [NC] to (Sustainer, 0) (same place)", new String[] {"NC"}, "Sustainer", 0, TransferHandler.MOVE);
		drop("move [NC, BT2] to (Sustainer, 1)", new String[] {"NC", "BT2"}, "Sustainer", 1, TransferHandler.MOVE);
		drop("move [NC, BT2] to (Sustainer, 3)", new String[] {"NC", "BT2"}, "Sustainer", 3, TransferHandler.MOVE);
		drop("move [BT2, NC] to (Sustainer, 3)", new String[] {"BT2", "NC"}, "Sustainer", 3, TransferHandler.MOVE);
		drop("move [IT] on BT2 (index -1 -> 0)", new String[] {"IT"}, "BT2", -1, TransferHandler.MOVE);
		drop("move [CR1, Fins] to (BT2, 1)", new String[] {"CR1", "Fins"}, "BT2", 1, TransferHandler.MOVE);
		drop("move [CR1, Fins] to (Sustainer, 0) (fins not allowed there)", new String[] {"CR1", "Fins"}, "Sustainer", 0, TransferHandler.MOVE);
		drop("move [BT1] into IT (not compatible)", new String[] {"BT1"}, "IT", 0, TransferHandler.MOVE);
		drop("move [BT1] into its own child IT", new String[] {"IT"}, "IT", 0, TransferHandler.MOVE);
		drop("move [BT1, IT] to (Sustainer, 0)", new String[] {"BT1", "IT"}, "Sustainer", 0, TransferHandler.MOVE);
		drop("move [IT, Chute] to (BT2, 0) (different parents)", new String[] {"IT", "Chute"}, "BT2", 0, TransferHandler.MOVE);
		drop("move [Chute, IT] to (BT1, 0) (different parents)", new String[] {"Chute", "IT"}, "BT1", 0, TransferHandler.MOVE);
		drop("copy [BT1] to (Sustainer, 1)", new String[] {"BT1"}, "Sustainer", 1, TransferHandler.COPY);
		drop("copy [IT, CR1] to (BT2, 1)", new String[] {"IT", "CR1"}, "BT2", 1, TransferHandler.COPY);
		drop("copy [NC] to (Sustainer, 0) (same place, copy)", new String[] {"NC"}, "Sustainer", 0, TransferHandler.COPY);
		drop("move [Sustainer] to (Rocket, 2)", new String[] {"Sustainer"}, "Rocket", 2, TransferHandler.MOVE);
		drop("drag [Rocket]", new String[] {"Rocket"}, "Rocket", 0, TransferHandler.MOVE);
	}

	static RocketComponent find(String name) {
		for (RocketComponent c : rocket) {
			if (c.getName().equals(name)) return c;
		}
		throw new IllegalArgumentException(name);
	}

	static void build() {
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
		tree = new BasicTree();
		tree.setModel(new ComponentTreeModel(rocket, tree));
		DefaultTreeSelectionModel m = new DefaultTreeSelectionModel();
		m.setSelectionMode(TreeSelectionModel.DISCONTIGUOUS_TREE_SELECTION);
		tree.setSelectionModel(m);
		handler = new ComponentTreeTransferHandler(doc);
	}

	static String tree(RocketComponent c) {
		StringBuilder sb = new StringBuilder(c.getName());
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

	static void drop(String label, String[] sel, String destName, int index, int action) throws Exception {
		System.out.println("== " + label);
		build();
		tree.clearSelection();
		for (String n : sel) tree.addSelectionPath(ComponentTreeModel.makeTreePath(find(n)));
		Method create = ComponentTreeTransferHandler.class.getDeclaredMethod("createTransferable", JComponent.class);
		create.setAccessible(true);
		RocketComponentTransferable t = (RocketComponentTransferable) create.invoke(handler, tree);
		if (t == null) {
			System.out.println("  createTransferable: null (no drag)");
			return;
		}
		List<RocketComponent> children = t.getComponents();
		List<String> names = new ArrayList<>();
		for (RocketComponent c : children) names.add(c.getName());
		System.out.println("  dragged (transferable order): " + names);

		RocketComponent dest = find(destName);
		int destIndex = index < 0 ? 0 : index;
		Class<?> stClass = Class.forName("info.openrocket.swing.gui.main.componenttree.ComponentTreeTransferHandler$SourceTarget");
		Constructor<?> ctor = stClass.getDeclaredConstructor(ComponentTreeTransferHandler.class, RocketComponent.class, int.class,
				RocketComponent.class, int.class, RocketComponent.class);
		ctor.setAccessible(true);
		List<Object> targets = new ArrayList<>();
		boolean allowed = true;
		for (RocketComponent child : children) {
			RocketComponent srcParent = child.getParent();
			targets.add(ctor.newInstance(handler, srcParent, srcParent.getChildPosition(child), dest, destIndex, child));
			// canImportTarget(): compatibility and no cycle
			boolean ok = dest.isCompatible(child);
			for (RocketComponent p = dest; p != null; p = p.getParent()) {
				if (p.equals(child)) ok = false;
			}
			allowed &= ok;
		}
		System.out.println("  canImport=" + allowed);
		if (!allowed) return;

		Method check = ComponentTreeTransferHandler.class.getDeclaredMethod("checkImportAction", int.class, stClass, int.class);
		check.setAccessible(true);
		Method importTarget = ComponentTreeTransferHandler.class.getDeclaredMethod("importDataTarget", int.class, stClass, int.class);
		importTarget.setAccessible(true);
		String desc = action == TransferHandler.MOVE ? (targets.size() == 1 ? "Move component" : "Move components")
				: (targets.size() == 1 ? "Copy component" : "Copy components");
		doc.startUndo(desc);
		boolean result = true;
		int running = destIndex;
		for (Object target : targets) {
			if (!(Boolean) check.invoke(handler, action, target, running)) {
				result = false;
				break;
			}
			running = (Integer) importTarget.invoke(handler, action, target, running);
		}
		doc.stopUndo();
		System.out.println("  importData=" + result + " tree=" + tree(rocket) + " undo='" + doc.getUndoDescription() + "'");
	}
}
