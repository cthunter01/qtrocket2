package probe;

import info.openrocket.core.document.OpenRocketDocument;
import info.openrocket.core.document.OpenRocketDocumentFactory;
import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.rocketcomponent.AxialStage;
import info.openrocket.core.rocketcomponent.BodyTube;
import info.openrocket.core.rocketcomponent.RocketComponent;
import info.openrocket.swing.gui.adaptors.PresetModel;

/**
 * What the Cancel of OpenRocket's component configuration dialog leaves of a component just added,
 * after a preset was chosen for it and without one, for QtRocket's
 * tests/gui/configdialog/ComponentConfigDialogTests.cpp (values pinned by hand: PresetGuiProbe
 * prints this transcript last, and hand-pinned.txt keeps it). This is bug 15 of QtRocket's plan
 * (section 10.3): the question of that Cancel says "undo adding this component", yet its one
 * undo() leaves the component in the rocket once a preset was chosen.
 *
 * The steps, as OpenRocket's GUI takes them: the add button's undo position "Add Body Tube" and
 * the new body tube (ComponentAddButtons); ComponentConfigDialog.showDialog() adds no undo position
 * for a new component; the parts library's Close applies the chosen preset through
 * PresetModel.setSelectedItem() (its undo position "Use Preset Body Tube", then loadPreset(); the
 * focus move after it throws on the null parent, as PresetModelProbe says); Cancel is one
 * document.undo() (RocketComponentConfig's cancel button). Each line: the document's undo and redo
 * descriptions and the tube (looked up by its id: an undo replaces every component), with its
 * preset's part number, outer radius and length, or "no tube". A second undo shows what the
 * question promised. The preset is the eighth body tube of PresetModelProbe's database.
 */
public class NewComponentCancelProbe {

	public static void main(String[] args) throws Exception {
		PresetModelProbe.bootstrap(args);
		run();
	}

	static String quoted(String s) {
		return s == null ? "null" : "'" + s + "'";
	}

	static void state(String title, OpenRocketDocument document, BodyTube added) {
		BodyTube found = null;
		for (RocketComponent c : document.getRocket()) {
			if (c.getID().equals(added.getID())) {
				found = (BodyTube) c;
			}
		}
		String tube = found == null ? "no tube"
				: "tube preset " + (found.getPresetComponent() == null ? "null" : found.getPresetComponent().getPartNo())
						+ " OR " + found.getOuterRadius() + " L " + found.getLength();
		System.out.println("  " + title + ": undo " + quoted(document.getUndoDescription()) + " redo "
				+ quoted(document.getRedoDescription()) + "; " + tube);
	}

	/** Prints the transcript (PresetGuiProbe runs it last, over PresetModelProbe's database). */
	static void run() {
		System.out.println("== a new component's Cancel");
		ComponentPreset preset = PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE).get(7);
		for (boolean withPreset : new boolean[] { true, false }) {
			String prefix = withPreset ? "with a preset, " : "without a preset, ";
			OpenRocketDocument document = OpenRocketDocumentFactory.createNewRocket();
			AxialStage stage = (AxialStage) document.getRocket().getChild(0);
			document.clearUndo();
			document.addUndoPosition("Add Body Tube");
			BodyTube tube = new BodyTube();
			stage.addChild(tube);
			state(prefix + "added", document, tube);
			if (withPreset) {
				PresetModelProbe.choose(new PresetModel(null, document, tube), preset);
				state(prefix + "chosen", document, tube);
			}
			document.undo();
			state(prefix + "after Cancel's undo", document, tube);
			if (document.isUndoAvailable()) {
				document.undo();
				state(prefix + "after a second undo", document, tube);
			}
		}
	}
}
