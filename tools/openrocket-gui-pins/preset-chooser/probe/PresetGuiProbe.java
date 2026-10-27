package probe;

/**
 * The values of the parts library's helpers that QtRocket's tests pin by hand (the family's
 * hand-pinned.txt keeps this transcript): TableUiProbe (TableUIPreferences, for
 * tests/gui/settings/TableUiPreferencesTests.cpp), then PresetModelProbe (PresetModel, for
 * tests/gui/binding/PresetBindingTests.cpp) and PresetComboProbe (the preset combo box, for
 * tests/gui/widgets/PresetComboBoxTests.cpp) over the preset files given as arguments, then
 * TableRowTraversalProbe (TableRowTraversalPolicy, for tests/gui/widgets/TableRowTraversalTests.cpp).
 */
public class PresetGuiProbe {

	public static void main(String[] args) throws Exception {
		TableUiProbe.run();
		PresetModelProbe.run(args);
		PresetComboProbe.run(args);
		TableRowTraversalProbe.run();
	}
}
