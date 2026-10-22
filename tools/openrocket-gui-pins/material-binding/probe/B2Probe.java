package probe;

import java.lang.reflect.Field;
import java.util.List;
import java.util.Map;

import javax.swing.JCheckBox;
import javax.swing.JPanel;
import javax.swing.SpinnerModel;

import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.util.Modules;

import info.openrocket.core.ServicesForTesting;
import info.openrocket.core.database.Databases;
import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.material.Material;
import info.openrocket.core.material.MaterialGroup;
import info.openrocket.core.plugin.PluginModule;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.Transition;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.startup.Application;
import info.openrocket.swing.gui.adaptors.BooleanModel;
import info.openrocket.swing.gui.adaptors.EnumModel;
import info.openrocket.swing.gui.adaptors.IntegerModel;
import info.openrocket.swing.gui.adaptors.MaterialModel;
import info.openrocket.swing.gui.adaptors.TransitionShapeModel;
import info.openrocket.swing.gui.widgets.GroupableAndSearchableComboBox;
import info.openrocket.swing.gui.widgets.MaterialComboBox;

/** What part B2's bindings port: IntegerModel, BooleanModel, EnumModel, TransitionShapeModel, MaterialModel. */
public class B2Probe {
	static String show(Material m) {
		if (m == null)
			return "null";
		String text = m.toString();
		if (m.isUserDefined())
			text = "(ud) " + text;
		return text + " [" + m.getGroup().getDatabaseString() + (m.isDocumentMaterial() ? ", doc" : "") + "]";
	}

	static void listModel(String title, MaterialModel mm) {
		System.out.println(title + ": size=" + mm.getSize() + " selected=" + show((Material) mm.getSelectedItem()));
		System.out.println("  at(-1)=" + show(mm.getElementAt(-1)) + " at(size)=" + show(mm.getElementAt(mm.getSize())));
		for (int i = 0; i < mm.getSize(); i++) {
			System.out.println("  " + i + " " + show(mm.getElementAt(i)));
		}
	}

	@SuppressWarnings("unchecked")
	static void groups(String title, OpenRocketDocument doc, MaterialModel mm) throws Exception {
		GroupableAndSearchableComboBox<MaterialGroup, Material> combo = MaterialComboBox.createComboBox(doc, mm);
		Field f = GroupableAndSearchableComboBox.class.getDeclaredField("itemGroupMap");
		f.setAccessible(true);
		Map<MaterialGroup, List<Material>> map = (Map<MaterialGroup, List<Material>>) f.get(combo);
		System.out.println(title + " groups:");
		for (Map.Entry<MaterialGroup, List<Material>> e : map.entrySet()) {
			StringBuilder sb = new StringBuilder();
			for (Material m : e.getValue()) {
				if (sb.length() > 0)
					sb.append(" | ");
				sb.append(combo.getDisplayString(m));
			}
			System.out.println("  " + e.getKey().getDatabaseString() + ": " + sb);
		}
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

		// ---------------------------------------------------------------- MaterialModel
		System.out.println("== MaterialModel");
		MaterialModel bulk = new MaterialModel(null, doc, tube, Material.Type.BULK);
		listModel("BULK (built-in only)", bulk);

		// A user material in the application database, a document material set on the tube, and a
		// document material of the parachute's line.
		Material user = Material.newMaterial(Material.Type.BULK, "Aardvark resin", 1500, MaterialGroup.PLASTICS, true);
		Databases.BULK_MATERIAL.add(user);
		Material docFoam = Material.newMaterial(Material.Type.BULK, "Doc foam", 30, MaterialGroup.FOAMS, true, true);
		tube.setMaterial(docFoam);
		Material docCustom = Material.newMaterial(Material.Type.BULK, "Zz custom", 900, MaterialGroup.CUSTOM, true, true);
		doc.getDocumentPreferences().addMaterial(docCustom);
		Material docLine = Material.newMaterial(Material.Type.LINE, "Doc string", 0.002, MaterialGroup.NYLONS, true, true);
		chute.setLineMaterial(docLine);
		listModel("BULK (with user and document materials)", bulk);
		groups("BULK", doc, bulk);
		MaterialModel line = new MaterialModel(null, doc, chute, Material.Type.LINE, "LineMaterial");
		listModel("LINE", line);
		groups("LINE", doc, line);
		MaterialModel surface = new MaterialModel(null, doc, chute, Material.Type.SURFACE);
		listModel("SURFACE", surface);
		groups("SURFACE", doc, surface);
		Databases.BULK_MATERIAL.remove(user);

		// ---------------------------------------------------------------- IntegerModel
		System.out.println("== IntegerModel");
		TrapezoidFinSet fins = new TrapezoidFinSet();
		tube.addChild(fins);
		IntegerModel finCount = new IntegerModel(fins, "FinCount", 1, 8);
		SpinnerModel spin = finCount.getSpinnerModel();
		System.out.println("FinCount=" + finCount.getValue() + " next=" + spin.getNextValue() + " previous=" + spin.getPreviousValue());
		finCount.setValue(10);
		System.out.println("setValue(10): " + finCount.getValue() + " next=" + spin.getNextValue() + " previous=" + spin.getPreviousValue());
		finCount.setValue(-5);
		System.out.println("setValue(-5): " + finCount.getValue() + " next=" + spin.getNextValue() + " previous=" + spin.getPreviousValue());
		spin.setValue(4);
		System.out.println("spinner setValue(4): " + finCount.getValue());
		IntegerModel lines = new IntegerModel(chute, "LineCount", 0);
		SpinnerModel lineSpin = lines.getSpinnerModel();
		lines.setValue(0);
		System.out.println("LineCount=0 next=" + lineSpin.getNextValue() + " previous=" + lineSpin.getPreviousValue());
		lines.setValue(Integer.MAX_VALUE);
		System.out.println("LineCount=MAX next=" + lineSpin.getNextValue() + " previous=" + lineSpin.getPreviousValue());

		// ---------------------------------------------------------------- BooleanModel
		System.out.println("== BooleanModel");
		BooleanModel massOverridden = new BooleanModel(tube, "MassOverridden");
		JPanel follower = new JPanel();
		JPanel opposite = new JPanel();
		massOverridden.addEnableComponent(follower, true);
		massOverridden.addEnableComponent(opposite, false);
		System.out.println("value=" + massOverridden.getValue() + " enabled=" + massOverridden.isEnabled()
				+ " follower(true)=" + follower.isEnabled() + " follower(false)=" + opposite.isEnabled());
		tube.setMassOverridden(true);
		System.out.println("source set true: value=" + massOverridden.getValue() + " follower(true)=" + follower.isEnabled()
				+ " follower(false)=" + opposite.isEnabled());
		massOverridden.setValue(false);
		System.out.println("setValue(false): value=" + massOverridden.getValue() + " follower(true)=" + follower.isEnabled()
				+ " follower(false)=" + opposite.isEnabled());
		JCheckBox box = new JCheckBox(massOverridden);
		System.out.println("check box selected=" + box.isSelected());

		Transition transition = new Transition();
		stage.addChild(transition);
		BooleanModel clipped = new BooleanModel(transition, "Clipped");
		System.out.println("Transition " + transition.getShapeType() + ": clipped=" + clipped.getValue() + " enabled=" + clipped.isEnabled());
		transition.setShapeType(Transition.Shape.ELLIPSOID);
		System.out.println("Transition " + transition.getShapeType() + ": clipped=" + clipped.getValue() + " enabled=" + clipped.isEnabled());
		transition.setShapeType(Transition.Shape.OGIVE);
		System.out.println("Transition " + transition.getShapeType() + ": clipped=" + clipped.getValue() + " enabled=" + clipped.isEnabled());
		BooleanModel own = new BooleanModel(true);
		JPanel ownFollower = new JPanel();
		own.addEnableComponent(ownFollower);
		own.setValue(false);
		System.out.println("own value=" + own.getValue() + " follower=" + ownFollower.isEnabled());

		// ---------------------------------------------------------------- Enum and shape models
		System.out.println("== EnumModel");
		MassComponent mass = new MassComponent();
		tube.addChild(mass);
		EnumModel<MassComponent.MassComponentType> types = new EnumModel<>(mass, "MassComponentType",
				new MassComponent.MassComponentType[] { MassComponent.MassComponentType.MASSCOMPONENT,
						MassComponent.MassComponentType.ALTIMETER, MassComponent.MassComponentType.FLIGHTCOMPUTER,
						MassComponent.MassComponentType.DEPLOYMENTCHARGE, MassComponent.MassComponentType.TRACKER,
						MassComponent.MassComponentType.PAYLOAD, MassComponent.MassComponentType.RECOVERYHARDWARE,
						MassComponent.MassComponentType.BATTERY });
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < types.getSize(); i++)
			sb.append(types.getElementAt(i).name()).append(' ');
		System.out.println("MassComponentType size=" + types.getSize() + " selected=" + types.getSelectedItem() + " values=" + sb);
		types.setSelectedItem(MassComponent.MassComponentType.TRACKER);
		System.out.println("set TRACKER: source=" + mass.getMassComponentType().name());
		System.out.println("== TransitionShapeModel");
		TransitionShapeModel shapes = new TransitionShapeModel(transition);
		sb = new StringBuilder();
		for (int i = 0; i < shapes.getSize(); i++)
			sb.append(shapes.getElementAt(i).name()).append(' ');
		System.out.println("size=" + shapes.getSize() + " selected=" + ((Transition.Shape) shapes.getSelectedItem()).name() + " values=" + sb);
		shapes.setSelectedItem(Transition.Shape.HAACK);
		System.out.println("set HAACK: source=" + transition.getShapeType().name() + " clipped=" + transition.isClipped()
				+ " parameter=" + transition.getShapeParameter());
	}
}
