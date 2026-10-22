package probe;

import java.awt.Component;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Map;

import javax.swing.JCheckBoxMenuItem;
import javax.swing.JList;
import javax.swing.JMenu;
import javax.swing.JPopupMenu;
import javax.swing.ListModel;

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.Databases;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.material.Material;
import info.openrocket.core.material.MaterialGroup;
import info.openrocket.core.motor.IgnitionEvent;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.DeploymentConfiguration;
import info.openrocket.core.rocketcomponent.DesignType;
import info.openrocket.core.rocketcomponent.ExternalComponent;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.StageSeparationConfiguration;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.position.AxialMethod;
import info.openrocket.core.rocketcomponent.position.RadiusMethod;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.adaptors.MaterialModel;
import info.openrocket.swing.gui.widgets.GroupableAndSearchableComboBox;
import info.openrocket.swing.gui.widgets.MaterialComboBox;

/**
 * What part B4's widgets port: the material combo box's group popup (titles, check marks), its
 * search (GroupableAndSearchableComboBox.filter(): the filtered list, its order, the highlighted
 * index and the check mark of the selected item), its selection, and the texts of the enums the
 * dialogs' combo boxes show (toString()).
 */
public class B4Probe {
	static GroupableAndSearchableComboBox<MaterialGroup, Material> combo;

	static Object field(String name) throws Exception {
		Field f = GroupableAndSearchableComboBox.class.getDeclaredField(name);
		f.setAccessible(true);
		return f.get(combo);
	}

	static void groupsPopup(String title) throws Exception {
		JPopupMenu popup = (JPopupMenu) field("groupsPopup");
		Method contains = GroupableAndSearchableComboBox.class.getDeclaredMethod("containsSelectedItem",
				info.openrocket.core.util.Group.class, info.openrocket.core.util.Groupable.class);
		contains.setAccessible(true);
		System.out.println(title + " groups popup:");
		for (Component c : popup.getComponents()) {
			if (c instanceof JMenu menu) {
				Object selected = combo.getSelectedItem();
				MaterialGroup group = null;
				for (MaterialGroup g : MaterialGroup.ALL_GROUPS) {
					if (g.toString().equals(menu.getText())) {
						group = g;
					}
				}
				boolean check = group != null && (Boolean) contains.invoke(combo, group, selected);
				StringBuilder sb = new StringBuilder();
				for (Component item : menu.getMenuComponents()) {
					if (item instanceof JCheckBoxMenuItem cb) {
						if (sb.length() > 0)
							sb.append(" | ");
						sb.append(cb.isSelected() ? "[x] " : "").append(cb.getText());
					}
				}
				System.out.println("  menu '" + menu.getText() + "'" + (check ? " [checked]" : "") + ": " + sb);
			} else {
				System.out.println("  " + c.getClass().getSimpleName()
						+ (c instanceof javax.swing.JTextField tf ? " text='" + tf.getText() + "'" : ""));
			}
		}
	}

	@SuppressWarnings("unchecked")
	static void search(String text) throws Exception {
		Method filter = GroupableAndSearchableComboBox.class.getDeclaredMethod("filter", String.class);
		filter.setAccessible(true);
		filter.invoke(combo, text);
		JList<Material> list = (JList<Material>) field("filteredList");
		ListModel<Material> model = list.getModel();
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < model.getSize(); i++) {
			Material m = model.getElementAt(i);
			if (sb.length() > 0)
				sb.append(" | ");
			sb.append(m == combo.getSelectedItem() ? "✓ " : "").append(combo.getDisplayString(m));
		}
		System.out.println("search '" + text + "': " + model.getSize() + ": " + sb);
	}

	public static void main(String[] args) throws Exception {
		Injector injector = Guice.createInjector(Modules.override(new ServicesForTesting()).with(new PluginModule()));
		Application.setInjector(injector);

		OpenRocketDocument doc = OpenRocketDocumentFactory.createNewRocket();
		Rocket rocket = doc.getRocket();
		AxialStage stage = rocket.getStage(0);
		BodyTube tube = new BodyTube();
		stage.addChild(tube);
		Parachute chute = new Parachute();
		tube.addChild(chute);

		// B2's probe additions: a user material in the application database, a document material
		// set on the tube, another added to the document.
		Material user = Material.newMaterial(Material.Type.BULK, "Aardvark resin", 1500, MaterialGroup.PLASTICS, true);
		Databases.BULK_MATERIAL.add(user);
		Material docFoam = Material.newMaterial(Material.Type.BULK, "Doc foam", 30, MaterialGroup.FOAMS, true, true);
		tube.setMaterial(docFoam);
		Material docCustom = Material.newMaterial(Material.Type.BULK, "Zz custom", 900, MaterialGroup.CUSTOM, true, true);
		doc.getDocumentPreferences().addMaterial(docCustom);

		System.out.println("== material combo (BULK, tube material Doc foam)");
		MaterialModel bulk = new MaterialModel(null, doc, tube, Material.Type.BULK);
		combo = MaterialComboBox.createComboBox(doc, bulk);
		System.out.println("placeholder='" + field("placeHolderText") + "'");
		System.out.println("selected display='" + combo.getDisplayString((Material) combo.getSelectedItem()) + "'");
		groupsPopup("Doc foam");
		for (String text : new String[] { "alu", "ALU", "nylon", "(ud)", "UD", "g/cm", "an", "re", "PL", "zzz",
				" ", "1.05", "crêpe", "³" }) {
			search(text);
		}
		System.out.println("highlightedListIdx=" + field("highlightedListIdx"));

		// Select through the combo: the model's setter.
		Material balsa = null;
		for (int i = 0; i < bulk.getSize(); i++) {
			if (bulk.getElementAt(i).getName().equals("Balsa"))
				balsa = bulk.getElementAt(i);
		}
		combo.setSelectedItem(balsa);
		System.out.println("after setSelectedItem(Balsa): tube material=" + tube.getMaterial().getName()
				+ " same object=" + (tube.getMaterial() == balsa));
		combo.updateItemsFromModel();
		groupsPopup("Balsa");
		search("bal");

		// A material that is in no database: equal by name and density to none.
		Material loose = Material.newMaterial(Material.Type.BULK, "Loose", 123, MaterialGroup.WOODS, true, false);
		tube.setMaterial(loose);
		combo.updateItemsFromModel();
		System.out.println("loose: selected display='" + combo.getDisplayString((Material) combo.getSelectedItem())
				+ "' index=" + combo.getSelectedIndex());
		groupsPopup("Loose");
		Databases.BULK_MATERIAL.remove(user);

		// ---------------------------------------------------------------- enum texts
		System.out.println("== enum texts (toString)");
		StringBuilder sb = new StringBuilder();
		for (AxialMethod m : AxialMethod.axialOffsetMethods)
			sb.append(" | ").append(m);
		System.out.println("AxialMethod.axialOffsetMethods:" + sb);
		sb = new StringBuilder();
		for (AxialMethod m : AxialMethod.values())
			sb.append(" | ").append(m);
		System.out.println("AxialMethod.values:" + sb);
		sb = new StringBuilder();
		for (RadiusMethod m : RadiusMethod.choices())
			sb.append(" | ").append(m);
		System.out.println("RadiusMethod.choices:" + sb);
		sb = new StringBuilder();
		for (ExternalComponent.Finish f : ExternalComponent.Finish.values())
			sb.append(" | ").append(f);
		System.out.println("Finish:" + sb);
		sb = new StringBuilder();
		for (FinSet.CrossSection c : FinSet.CrossSection.values())
			sb.append(" | ").append(c);
		System.out.println("CrossSection:" + sb);
		sb = new StringBuilder();
		for (MassComponent.MassComponentType t : MassComponent.MassComponentType.values())
			sb.append(" | ").append(t);
		System.out.println("MassComponentType:" + sb);
		sb = new StringBuilder();
		for (DeploymentConfiguration.DeployEvent e : DeploymentConfiguration.DeployEvent.values())
			sb.append(" | ").append(e);
		System.out.println("DeployEvent:" + sb);
		sb = new StringBuilder();
		for (StageSeparationConfiguration.SeparationEvent e : StageSeparationConfiguration.SeparationEvent.values())
			sb.append(" | ").append(e);
		System.out.println("SeparationEvent:" + sb);
		sb = new StringBuilder();
		for (IgnitionEvent e : IgnitionEvent.values())
			sb.append(" | ").append(e);
		System.out.println("IgnitionEvent:" + sb);
		sb = new StringBuilder();
		for (Transition.Shape s : Transition.Shape.values())
			sb.append(" | ").append(s);
		System.out.println("Transition.Shape:" + sb);
		sb = new StringBuilder();
		for (DesignType d : DesignType.values())
			sb.append(" | ").append(d);
		System.out.println("DesignType:" + sb);
		sb = new StringBuilder();
		for (MaterialGroup g : MaterialGroup.ALL_GROUPS)
			sb.append(" | ").append(g);
		System.out.println("MaterialGroup:" + sb);
	}
}
