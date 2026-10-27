package probe;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.List;

import javax.swing.DefaultComboBoxModel;
import javax.swing.JComboBox;
import javax.swing.JLabel;
import javax.swing.JList;
import javax.swing.UIManager;

import com.formdev.flatlaf.FlatLightLaf;

import info.openrocket.core.preset.ComponentPreset;
import info.openrocket.core.preset.xml.OpenRocketComponentLoader;

/**
 * What the JComboBox of RocketComponentConfig's preset row (RocketComponentConfig.java:160-169)
 * does with its model, for QtRocket's PresetComboBoxTests.cpp (values pinned by hand:
 * PresetGuiProbe prints this transcript after PresetModelProbe's, and hand-pinned.txt keeps it):
 * which item it shows as selected, which choices reach the model's setSelectedItem() and which it
 * drops (JComboBox compares with equals(), ComponentPreset's digest), and the text its renderer
 * gives the closed combo box.
 *
 * The model is a DefaultComboBoxModel that records its setSelectedItem() calls, in the place of
 * PresetModel (whose setSelectedItem() needs a dialog), with PresetModel's items: "Custom"
 * ("<html><i>Custom</i></html>") and the favourites 31361 and 85865 of PresetModelProbe's database
 * (the body tubes 2 and 7). "The model selects ..." stands for PresetModel's selected item, the
 * component's preset, changing behind the combo box. A copy of 85865 is the same preset read
 * again from the files: equal, another object. Each recorded call is printed with the item's
 * number (0 Custom, 1 31361, 2 85865, 3 the copy, 4 85868). Last, whether the look and feel lets
 * the arrow keys of the open list choose (ComboBox.noActionOnKeyNavigation).
 */
public class PresetComboProbe {

	static final String CUSTOM = "<html><i>Custom</i></html>";
	static final List<String> calls = new ArrayList<>();

	static final class RecordingModel extends DefaultComboBoxModel<Object> {
		RecordingModel(List<Object> items) {
			for (Object item : items) {
				addElement(item);
			}
		}

		@Override
		public void setSelectedItem(Object item) {
			calls.add(text(item) + (item instanceof ComponentPreset ? "@" + id(item) : ""));
			super.setSelectedItem(item);
		}
	}

	static final List<Object> ids = new ArrayList<>();

	static String id(Object item) {
		for (int i = 0; i < ids.size(); i++) {
			if (ids.get(i) == item) {
				return String.valueOf(i);
			}
		}
		ids.add(item);
		return String.valueOf(ids.size() - 1);
	}

	static String text(Object item) {
		return item == null ? "null" : item.toString();
	}

	static String shown(JComboBox<Object> combo) {
		JList<Object> list = new JList<>();
		JLabel label = (JLabel) combo.getRenderer().getListCellRendererComponent(list, combo.getSelectedItem(), -1,
				false, false);
		return label.getText();
	}

	static void state(String title, JComboBox<Object> combo) {
		System.out.println("  " + title + ": index " + combo.getSelectedIndex() + " shown " + shown(combo) + " calls ["
				+ String.join(", ", calls) + "]");
		calls.clear();
	}

	static List<ComponentPreset> load(String path) throws Exception {
		File file = new File(path);
		try (InputStream in = new FileInputStream(file)) {
			return new ArrayList<>(new OpenRocketComponentLoader().load(in, file.getName()));
		}
	}

	public static void main(String[] args) throws Exception {
		PresetModelProbe.bootstrap(args);
		run(args);
	}

	/** Prints the transcript for the given preset files, once PresetModelProbe has loaded them. */
	static void run(String[] files) throws Exception {
		List<ComponentPreset> tubes = new ArrayList<>();
		List<ComponentPreset> copies = new ArrayList<>();
		for (ComponentPreset p : PresetModelProbe.presets.listForType(ComponentPreset.Type.BODY_TUBE)) {
			tubes.add(p);
		}
		// The same files read again: presets equal to the database's (one digest), other objects.
		for (String path : files) {
			for (ComponentPreset p : load(path)) {
				if (p.getType() == ComponentPreset.Type.BODY_TUBE) {
					copies.add(p);
				}
			}
		}
		ComponentPreset first = tubes.get(7);
		ComponentPreset second = tubes.get(2);
		ComponentPreset other = tubes.get(10);
		ComponentPreset firstCopy = null;
		for (ComponentPreset c : copies) {
			if (c.equals(first)) {
				firstCopy = c;
			}
		}
		System.out.println("== the preset combo box");
		System.out.println("  copy of " + first.getPartNo() + ": equals " + first.equals(firstCopy) + " same "
				+ (first == firstCopy));
		id(CUSTOM);
		id(second);
		id(first);
		id(firstCopy);
		id(other);
		List<Object> items = new ArrayList<>();
		items.add(CUSTOM);
		items.add(second);
		items.add(first);
		RecordingModel model = new RecordingModel(items);
		calls.clear(); // DefaultComboBoxModel.addElement() selects the first item
		JComboBox<Object> combo = new JComboBox<>(model);
		combo.setMaximumRowCount(25);
		System.out.println("  items " + combo.getItemCount() + " maximum row count " + combo.getMaximumRowCount()
				+ " editable " + combo.isEditable());
		state("made", combo);
		combo.setSelectedIndex(0);
		state("Custom chosen while Custom", combo);
		combo.setSelectedIndex(2);
		state("first chosen", combo);
		combo.setSelectedIndex(2);
		state("first chosen again", combo);
		combo.setSelectedItem(firstCopy);
		state("a copy of first chosen", combo);

		// The model's selected item changed behind the combo box (PresetModel: the component's preset).
		model.setSelectedItem(firstCopy);
		calls.clear();
		state("the model selects the copy", combo);
		combo.setSelectedIndex(2);
		state("first chosen while the copy is selected", combo);

		model.setSelectedItem(other);
		calls.clear();
		state("the model selects a preset no item holds", combo);
		combo.setSelectedIndex(2);
		state("first chosen", combo);
		model.setSelectedItem(other);
		calls.clear();
		combo.setSelectedIndex(0);
		state("Custom chosen while a preset no item holds is selected", combo);
		combo.setSelectedItem(other);
		state("the preset no item holds chosen (setSelectedItem)", combo);

		// Whether an arrow key in the open list chooses the item at once (false: it does), in the
		// look and feel of this probe and in OpenRocket's FlatLaf Light, read without installing it.
		System.out.println("  ComboBox.noActionOnKeyNavigation: " + UIManager.getLookAndFeel().getName() + " "
				+ UIManager.getBoolean("ComboBox.noActionOnKeyNavigation") + ", FlatLaf Light "
				+ new FlatLightLaf().getDefaults().getBoolean("ComboBox.noActionOnKeyNavigation"));
	}
}
