package info.qtrocket.guireference;

import java.util.HashMap;
import java.util.Map;
import java.util.TreeSet;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

import javax.swing.Action;
import javax.swing.ComboBoxModel;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.material.Material;
import info.openrocket.core.rocketcomponent.ExternalComponent;
import info.openrocket.core.rocketcomponent.FinSet;
import info.openrocket.core.rocketcomponent.MassComponent;
import info.openrocket.core.rocketcomponent.Parachute;
import info.openrocket.core.rocketcomponent.Rocket;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.core.rocketcomponent.TrapezoidFinSet;
import info.openrocket.core.rocketcomponent.position.AxialMethod;
import info.openrocket.core.unit.Unit;
import info.openrocket.core.unit.UnitGroup;
import info.openrocket.core.util.Invalidatable;
import info.openrocket.core.util.StateChangeListener;
import info.openrocket.swing.gui.adaptors.BooleanModel;
import info.openrocket.swing.gui.adaptors.DoubleModel;
import info.openrocket.swing.gui.adaptors.EnumModel;
import info.openrocket.swing.gui.adaptors.IntegerModel;
import info.openrocket.swing.gui.adaptors.MaterialModel;
import info.openrocket.swing.gui.adaptors.TransitionShapeModel;

/**
 * The fields of OpenRocket's configuration dialogs that the scenario sets, each with the model its
 * dialog builds for it: the same class with the same arguments (the source line of the dialog is
 * given with each field), so that a value goes through the code a Swing widget calls.
 * <ul>
 * <li>A spinner is a {@link DoubleModel} (or an {@link IntegerModel}): the unit selector beside it
 * calls {@link DoubleModel#setCurrentUnit}, and the spinner's editor commits the typed text as a
 * String to the model's {@code ValueSpinnerModel.setValue}, which parses it with OpenRocket's
 * ExpressionParser and converts it from the selected unit (an integer spinner commits an Integer).</li>
 * <li>A combo box is an {@link EnumModel}, a {@link TransitionShapeModel} or a {@link MaterialModel}:
 * choosing an item calls {@code setSelectedItem} with it, and only the model's own items can be
 * chosen.</li>
 * <li>A check box is a {@link BooleanModel} ({@code setValue}).</li>
 * <li>A text field (the name, the comment, the design information) is what the dialog's
 * TextFieldListener does when the field loses focus: the setter, when the text differs.</li>
 * </ul>
 * The dialogs themselves are not built: they are JDialogs, which a headless JVM cannot make (their
 * panels need the dialog as their parent). A dialog is named by its panel class, as
 * ComponentConfigDialog finds it ({@code findDialogContentsConstructor}).
 */
final class DialogFields {

	/** One widget of an open dialog: the model the dialog would have built, and what a user does to it. */
	interface Control extends Invalidatable {
		default void type(String text, String unit) {
			throw new IllegalArgumentException("one cannot type into this field");
		}

		default void choose(String item) {
			throw new IllegalArgumentException("this field is no combo box");
		}

		default void check(boolean checked) {
			throw new IllegalArgumentException("this field is no check box");
		}

		/** The DoubleModel of a spinner, for its automatic check box; null for other fields. */
		default DoubleModel doubleModel() {
			return null;
		}

		@Override
		default void invalidateMe() {
		}
	}

	/** What makes a control: the document and the component of the dialog. */
	private interface Factory {
		Control make(OpenRocketDocument document, RocketComponent component);
	}

	/** The fields of each dialog (by its panel's simple class name), by Java property name. */
	private static final Map<String, Map<String, Factory>> DIALOGS = new HashMap<>();

	private DialogFields() {
	}

	/** The control of @p property in the dialog @p dialog of @p component. */
	static Control make(String dialog, String property, OpenRocketDocument document, RocketComponent component) {
		Map<String, Factory> fields = DIALOGS.get(dialog);
		if (fields == null) {
			throw new IllegalArgumentException("the tool knows no field of the dialog " + dialog);
		}
		Factory factory = fields.get(property);
		if (factory == null) {
			throw new IllegalArgumentException("the dialog " + dialog + " has no field " + property
					+ " the tool knows (" + new TreeSet<>(fields.keySet()) + ")");
		}
		return factory.make(document, component);
	}

	// ------------------------------------------------------------------ controls

	/** A spinner bound to a DoubleModel, with its unit selector. */
	private static Control spinner(DoubleModel model) {
		return new Control() {
			@Override
			public void type(String text, String unit) {
				if (unit != null) {
					// UnitSelector: the user picks the unit from the selector's list of the model's group.
					Unit chosen = null;
					UnitGroup group = model.getUnitGroup();
					for (int i = 0; i < group.getUnitCount(); i++) {
						if (group.getUnit(i).getUnit().equals(unit)) {
							chosen = group.getUnit(i);
						}
					}
					if (chosen == null) {
						throw new IllegalArgumentException("the unit selector has no unit '" + unit + "'");
					}
					model.setCurrentUnit(chosen);
				} else if (model.getUnitGroup() != UnitGroup.UNITS_NONE) {
					throw new IllegalArgumentException("a field with a unit selector needs the unit of the text");
				}
				// SpinnerEditor (a JSpinner.DefaultEditor): the formatter commits the text as a String.
				model.getSpinnerModel().setValue(text);
			}

			@Override
			public DoubleModel doubleModel() {
				return model;
			}

			@Override
			public void invalidateMe() {
				model.invalidateMe();
			}
		};
	}

	/** A spinner bound to an IntegerModel (no unit). */
	private static Control integerSpinner(IntegerModel model) {
		return new Control() {
			@Override
			public void type(String text, String unit) {
				if (unit != null) {
					throw new IllegalArgumentException("an integer field has no unit");
				}
				// The default editor's formatter makes an Integer of the text, as the spinner's value is one.
				model.getSpinnerModel().setValue(Integer.valueOf(text.trim()));
			}

			@Override
			public void invalidateMe() {
				model.invalidateMe();
			}
		};
	}

	/** A combo box of an enum model: the item is the name of one of the model's constants. */
	private static Control combo(ComboBoxModel<?> model, Invalidatable invalidatable) {
		return new Control() {
			@Override
			public void choose(String item) {
				for (int i = 0; i < model.getSize(); i++) {
					Object element = model.getElementAt(i);
					if (element instanceof Enum<?> constant && constant.name().equals(item)) {
						model.setSelectedItem(element);
						return;
					}
				}
				throw new IllegalArgumentException("the combo box has no item " + item);
			}

			@Override
			public void invalidateMe() {
				invalidatable.invalidateMe();
			}
		};
	}

	/** A material combo box: the item is the name of a material of the model's list. */
	private static Control materials(MaterialModel model) {
		return new Control() {
			@Override
			public void choose(String item) {
				for (int i = 0; i < model.getSize(); i++) {
					Material material = model.getElementAt(i);
					if (material.getName().equals(item)) {
						model.setSelectedItem(material);
						return;
					}
				}
				throw new IllegalArgumentException("the material combo box has no material " + item);
			}

			@Override
			public void invalidateMe() {
				model.invalidateMe();
			}
		};
	}

	/** A check box bound to a BooleanModel: a click toggles it, so a user clicks only to change it. */
	private static Control checkBox(BooleanModel model) {
		return new Control() {
			@Override
			public void check(boolean checked) {
				if (model.getValue() != checked) {
					model.setValue(checked);
				}
			}

			@Override
			public void invalidateMe() {
				model.invalidateMe();
			}
		};
	}

	/**
	 * The "Automatic" check box beside a spinner: a JCheckBox made with the spinner model's
	 * {@link DoubleModel#getAutomaticAction()}; its click stores the new state as the action's
	 * SELECTED_KEY, which sets the automatic flag (DoubleModel.AutomaticActionModel.putValue).
	 */
	static Control automatic(DoubleModel model) {
		Action action = model.getAutomaticAction();
		if (!action.isEnabled()) {
			throw new IllegalArgumentException("the field has no automatic value");
		}
		return new Control() {
			@Override
			public void check(boolean checked) {
				if (!Boolean.valueOf(checked).equals(action.getValue(Action.SELECTED_KEY))) {
					action.putValue(Action.SELECTED_KEY, checked);
				}
			}

			@Override
			public void invalidateMe() {
				// The action listens to the spinner's model, which the spinner's control invalidates.
				model.removeChangeListener((StateChangeListener) action);
			}
		};
	}

	/** A text field whose listener calls the setter when the text differs from the value. */
	private static Control text(Supplier<String> getter, Consumer<String> setter) {
		return new Control() {
			@Override
			public void type(String text, String unit) {
				if (unit != null) {
					throw new IllegalArgumentException("a text field has no unit");
				}
				if (!getter.get().equals(text)) {
					setter.accept(text);
				}
			}
		};
	}

	// ------------------------------------------------------------------ the table

	private static Map<String, Factory> dialog(String name, String... panels) {
		Map<String, Factory> fields = new HashMap<>();
		// RocketComponentConfig.java:150, :817 and its TextFieldListener (:1040-1047): every dialog of a component.
		if (!name.equals("SaveDesignInfoPanel")) {
			fields.put("Name", (d, c) -> text(c::getName, c::setName));
			fields.put("Comment", (d, c) -> text(c::getComment, c::setComment));
		}
		for (String panel : panels) {
			fields.putAll(PANELS.get(panel));
		}
		DIALOGS.put(name, fields);
		return fields;
	}

	private static Factory length(String property, double multiplier, double min) {
		return (d, c) -> spinner(new DoubleModel(c, property, multiplier, UnitGroup.UNITS_LENGTH, min));
	}

	/** The panels several dialogs share. */
	private static final Map<String, Map<String, Factory>> PANELS = new HashMap<>();

	private static void panel(String name, Function<Map<String, Factory>, Map<String, Factory>> fill) {
		PANELS.put(name, fill.apply(new HashMap<>()));
	}

	static {
		// PlacementPanel.java:41 and :50.
		panel("PlacementPanel", f -> {
			f.put("AxialMethod", (d, c) -> {
				EnumModel<AxialMethod> model = new EnumModel<>(c, "AxialMethod", AxialMethod.axialOffsetMethods);
				return combo(model, model);
			});
			f.put("AxialOffset", (d, c) -> spinner(new DoubleModel(c, "AxialOffset", UnitGroup.UNITS_LENGTH)));
			return f;
		});
		// MaterialPanel.java:51 (bulk materials, the property "Material") and :73 (the finish, of an
		// external component only).
		panel("MaterialPanel:BULK", f -> {
			f.put("Material", (d, c) -> materials(new MaterialModel(null, d, c, Material.Type.BULK, "Material")));
			f.put("Finish", (d, c) -> {
				if (!(c instanceof ExternalComponent)) {
					throw new IllegalArgumentException("an internal component has no surface finish");
				}
				EnumModel<ExternalComponent.Finish> model = new EnumModel<>(c, "Finish");
				return combo(model, model);
			});
			return f;
		});
		// ShockCordConfig.java:72: MaterialPanel of line materials.
		panel("MaterialPanel:LINE", f -> {
			f.put("Material", (d, c) -> materials(new MaterialModel(null, d, c, Material.Type.LINE, "Material")));
			return f;
		});
		// InstancesPanel.java:36 and :46.
		panel("InstancesPanel", f -> {
			f.put("InstanceCount", (d, c) -> integerSpinner(new IntegerModel(c, "InstanceCount", 1)));
			f.put("InstanceSeparation",
					(d, c) -> spinner(new DoubleModel(c, "InstanceSeparation", UnitGroup.UNITS_LENGTH)));
			return f;
		});
		// MotorConfig.java:48 and :64 (the motor tab of BodyTubeConfig and InnerTubeConfig).
		panel("MotorConfig", f -> {
			f.put("MotorMount", (d, c) -> checkBox(new BooleanModel(c, "MotorMount")));
			f.put("MotorOverhang", (d, c) -> spinner(new DoubleModel(c, "MotorOverhang", UnitGroup.UNITS_LENGTH)));
			return f;
		});

		// NoseConeConfig.java: :61 TransitionShapeModel, :82 ShapeParameter, :103 Length, :118 BaseRadius
		// (shown as the diameter), :139 Thickness, :152 Filled, :163 Flipped, :190 MaterialPanel.
		Map<String, Factory> f = dialog("NoseConeConfig", "MaterialPanel:BULK");
		f.put("ShapeType", (d, c) -> {
			TransitionShapeModel model = new TransitionShapeModel(c);
			return combo(model, model);
		});
		f.put("ShapeParameter",
				(d, c) -> spinner(new DoubleModel(c, "ShapeParameter", UnitGroup.UNITS_SHAPE_PARAMETER, 0, 1)));
		f.put("Length", length("Length", 1, 0));
		f.put("BaseRadius", length("BaseRadius", 2.0, 0));
		f.put("Thickness", length("Thickness", 1, 0));
		f.put("Filled", (d, c) -> checkBox(new BooleanModel(c, "Filled")));
		f.put("Flipped", (d, c) -> checkBox(new BooleanModel(c, "Flipped")));

		// BodyTubeConfig.java: :45 Length, :61 OuterRadius (the diameter), :83 InnerRadius (the diameter),
		// :97 Thickness, :108 Filled, :117 MaterialPanel, :127 MotorConfig.
		f = dialog("BodyTubeConfig", "MaterialPanel:BULK", "MotorConfig");
		f.put("Length", length("Length", 1, 0));
		f.put("OuterRadius", length("OuterRadius", 2, 0));
		f.put("InnerRadius", length("InnerRadius", 2, 0));
		f.put("Thickness", length("Thickness", 1, 0));
		f.put("Filled", (d, c) -> checkBox(new BooleanModel(c, "Filled")));

		// TrapezoidFinSetConfig.java: :48 FinCount (1 to 8), :64 CantAngle, :80 RootChord, :95 TipChord,
		// :110 Height, :125 Sweep, :146 SweepAngle, :163 CrossSection, :173 Thickness, :192 PlacementPanel,
		// :216 MaterialPanel.
		f = dialog("TrapezoidFinSetConfig", "PlacementPanel", "MaterialPanel:BULK");
		f.put("FinCount", (d, c) -> integerSpinner(new IntegerModel(c, "FinCount", 1, 8)));
		f.put("CantAngle", (d, c) -> spinner(new DoubleModel(c, "CantAngle", UnitGroup.UNITS_ANGLE,
				-FinSet.MAX_CANT_RADIANS, FinSet.MAX_CANT_RADIANS)));
		f.put("RootChord", length("RootChord", 1, 0));
		f.put("TipChord", length("TipChord", 1, 0));
		f.put("Height", length("Height", 1, 0));
		f.put("Sweep", (d, c) -> spinner(new DoubleModel(c, "Sweep", UnitGroup.UNITS_LENGTH)));
		f.put("SweepAngle", (d, c) -> spinner(new DoubleModel(c, "SweepAngle", UnitGroup.UNITS_ANGLE,
				-TrapezoidFinSet.MAX_SWEEP_ANGLE, TrapezoidFinSet.MAX_SWEEP_ANGLE)));
		f.put("CrossSection", (d, c) -> {
			EnumModel<FinSet.CrossSection> model = new EnumModel<>(c, "CrossSection");
			return combo(model, model);
		});
		f.put("Thickness", length("Thickness", 1, 0));

		// FreeformFinSetConfig.java: :140 FinCount (1 to 8), :170 CrossSection; the point table is
		// FinPoints (see Session), the placement and the material as for the trapezoidal fin set.
		f = dialog("FreeformFinSetConfig", "PlacementPanel", "MaterialPanel:BULK");
		f.put("FinCount", (d, c) -> integerSpinner(new IntegerModel(c, "FinCount", 1, 8)));
		f.put("CrossSection", (d, c) -> {
			EnumModel<FinSet.CrossSection> model = new EnumModel<>(c, "CrossSection");
			return combo(model, model);
		});

		// InnerTubeConfig.java: :88 Length, :104 OuterRadius (the diameter), :126 InnerRadius (the
		// diameter), :149 Thickness, :166 PlacementPanel, :171 MaterialPanel, :180 MotorConfig.
		f = dialog("InnerTubeConfig", "PlacementPanel", "MaterialPanel:BULK", "MotorConfig");
		f.put("Length", length("Length", 1, 0));
		f.put("OuterRadius", length("OuterRadius", 2, 0));
		f.put("InnerRadius", length("InnerRadius", 2, 0));
		f.put("Thickness", length("Thickness", 1, 0));

		// CenteringRingConfig.java:23 builds RingComponentConfig.generalTab with a length (labelled
		// "Thickness"), an outer and an inner diameter and no thickness: RingComponentConfig.java:48 Length,
		// :68 OuterRadius (the diameter), :95 InnerRadius (the diameter), :145 PlacementPanel, :150
		// MaterialPanel.
		f = dialog("CenteringRingConfig", "PlacementPanel", "MaterialPanel:BULK");
		f.put("Length", length("Length", 1, 0));
		f.put("OuterRadius", length("OuterRadius", 2, 0));
		f.put("InnerRadius", length("InnerRadius", 2, 0));

		// ParachuteConfig.java: :68 Diameter, :81 SpillDia, :97 the canopy material (surface materials),
		// :113 CD, :144 LineCount, :155 LineLength, :174 LineMaterial (line materials), :188 PlacementPanel,
		// :195 Length (packed), :210 Radius (packed, the diameter), and on the default deployment
		// configuration :244 DeployEvent, :264 DeployDelay, :279 DeployAltitude.
		f = dialog("ParachuteConfig", "PlacementPanel");
		f.put("Diameter", length("Diameter", 1, 0));
		f.put("SpillDia", (d, c) -> spinner(new DoubleModel(c, "SpillDia", UnitGroup.UNITS_LENGTH, 0, 0.08)));
		f.put("Material", (d, c) -> materials(new MaterialModel(null, d, c, Material.Type.SURFACE)));
		f.put("CD", (d, c) -> spinner(new DoubleModel(c, "CD", UnitGroup.UNITS_COEFFICIENT, 0)));
		f.put("LineCount", (d, c) -> integerSpinner(new IntegerModel(c, "LineCount", 0)));
		f.put("LineLength", length("LineLength", 1, 0));
		f.put("LineMaterial",
				(d, c) -> materials(new MaterialModel(null, d, c, Material.Type.LINE, "LineMaterial")));
		f.put("Length", length("Length", 1, 0));
		f.put("Radius", length("Radius", 2, 0));
		f.put("DeployEvent", (d, c) -> {
			EnumModel<info.openrocket.core.rocketcomponent.DeploymentConfiguration.DeployEvent> model =
					new EnumModel<>(((Parachute) c).getDeploymentConfigurations().getDefault(), "DeployEvent");
			return combo(model, model);
		});
		f.put("DeployDelay", (d, c) -> spinner(
				new DoubleModel(((Parachute) c).getDeploymentConfigurations().getDefault(), "DeployDelay", 0)));
		f.put("DeployAltitude", (d, c) -> spinner(new DoubleModel(
				((Parachute) c).getDeploymentConfigurations().getDefault(), "DeployAltitude", UnitGroup.UNITS_DISTANCE,
				0)));

		// ShockCordConfig.java: :53 CordLength, :72 MaterialPanel (line materials), :83 PlacementPanel,
		// :90 Length (packed), :106 Radius (packed, the diameter).
		f = dialog("ShockCordConfig", "MaterialPanel:LINE", "PlacementPanel");
		f.put("CordLength", length("CordLength", 1, 0));
		f.put("Length", length("Length", 1, 0));
		f.put("Radius", length("Radius", 2, 0));

		// LaunchLugConfig.java: :41 Length, :58 OuterRadius (the diameter), :75 InnerRadius (the diameter),
		// :92 Thickness, :104 InstancesPanel, :115 PlacementPanel, :122 AngleOffset, :135 MaterialPanel.
		f = dialog("LaunchLugConfig", "InstancesPanel", "PlacementPanel", "MaterialPanel:BULK");
		f.put("Length", length("Length", 1, 0));
		f.put("OuterRadius", length("OuterRadius", 2, 0));
		f.put("InnerRadius", length("InnerRadius", 2, 0));
		f.put("Thickness", length("Thickness", 1, 0));
		f.put("AngleOffset",
				(d, c) -> spinner(new DoubleModel(c, "AngleOffset", UnitGroup.UNITS_ANGLE, -Math.PI, Math.PI)));

		// RailButtonConfig.java: :54 OuterDiameter, :65 InnerDiameter, :76 BaseHeight, :88 FlangeHeight,
		// :100 TotalHeight, :112 ScrewHeight, :123 InstancesPanel, :135 PlacementPanel, :141 AngleOffset (its
		// bounds -180 and +180 are in radians, as written there), :153 MaterialPanel.
		f = dialog("RailButtonConfig", "InstancesPanel", "PlacementPanel", "MaterialPanel:BULK");
		f.put("OuterDiameter", length("OuterDiameter", 1, 0));
		f.put("InnerDiameter", length("InnerDiameter", 1, 0));
		f.put("BaseHeight", length("BaseHeight", 1, 0));
		f.put("FlangeHeight", length("FlangeHeight", 1, 0));
		f.put("TotalHeight", length("TotalHeight", 1, 0));
		f.put("ScrewHeight", length("ScrewHeight", 1, 0));
		f.put("AngleOffset", (d, c) -> spinner(new DoubleModel(c, "AngleOffset", UnitGroup.UNITS_ANGLE, -180, +180)));

		// MassComponentConfig.java: :49 MassComponentType (six types), :69 ComponentMass, :83 Density, :100
		// Length, :116 Radius (the diameter), :150 PlacementPanel.
		f = dialog("MassComponentConfig", "PlacementPanel");
		f.put("MassComponentType", (d, c) -> {
			EnumModel<MassComponent.MassComponentType> model = new EnumModel<>(c, "MassComponentType",
					new MassComponent.MassComponentType[] { MassComponent.MassComponentType.MASSCOMPONENT,
							MassComponent.MassComponentType.ALTIMETER, MassComponent.MassComponentType.FLIGHTCOMPUTER,
							MassComponent.MassComponentType.DEPLOYMENTCHARGE, MassComponent.MassComponentType.TRACKER,
							MassComponent.MassComponentType.PAYLOAD, MassComponent.MassComponentType.RECOVERYHARDWARE,
							MassComponent.MassComponentType.BATTERY });
			return combo(model, model);
		});
		f.put("ComponentMass",
				(d, c) -> spinner(new DoubleModel(c, "ComponentMass", UnitGroup.UNITS_MASS, 0)));
		f.put("Density", (d, c) -> spinner(new DoubleModel(c, "Density", UnitGroup.UNITS_DENSITY_BULK, 0)));
		f.put("Length", length("Length", 1, 0));
		f.put("Radius", length("Radius", 2, 0));

		// SaveDesignInfoPanel extends RocketConfig: the designer (RocketConfig.java:66), the kit name and the
		// revision history (:136) of the rocket, through RocketConfig's TextFieldListener (:225-229).
		f = dialog("SaveDesignInfoPanel");
		f.put("Designer", (d, c) -> text(((Rocket) c)::getDesigner, ((Rocket) c)::setDesigner));
		f.put("KitName", (d, c) -> text(((Rocket) c)::getKitName, ((Rocket) c)::setKitName));
		f.put("Revision", (d, c) -> text(((Rocket) c)::getRevision, ((Rocket) c)::setRevision));
	}
}
