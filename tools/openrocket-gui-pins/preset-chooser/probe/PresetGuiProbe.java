package probe;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

/**
 * The values of the parts library's helpers that QtRocket's tests pin by hand (the family's
 * hand-pinned.txt keeps this transcript): TableUiProbe (TableUIPreferences, for
 * tests/gui/settings/TableUiPreferencesTests.cpp), then PresetModelProbe (PresetModel, for
 * tests/gui/binding/PresetBindingTests.cpp) and PresetComboProbe (the preset combo box, for
 * tests/gui/widgets/PresetComboBoxTests.cpp) over the preset files given as arguments, then
 * TableRowTraversalProbe (TableRowTraversalPolicy, for tests/gui/widgets/TableRowTraversalTests.cpp),
 * then ComponentPresetTableProbe (the parts library's table, for tests/gui/dialogs/preset/) over the
 * same files and the components directory given as the last argument (the bundled presets), then
 * NewComponentCancelProbe (a new component's Cancel after a preset, for
 * tests/gui/configdialog/ComponentConfigDialogTests.cpp) over the same files.
 */
public class PresetGuiProbe {

	public static void main(String[] args) throws Exception {
		List<String> files = new ArrayList<>();
		String components = null;
		for (String argument : args) {
			if (new File(argument).isDirectory()) {
				components = argument;
			} else {
				files.add(argument);
			}
		}
		String[] presetFiles = files.toArray(new String[0]);
		TableUiProbe.run();
		PresetModelProbe.run(presetFiles);
		PresetComboProbe.run(presetFiles);
		TableRowTraversalProbe.run();
		ComponentPresetTableProbe.run(components);
		NewComponentCancelProbe.run();
	}
}
