package probe;

/**
 * The values of OpenRocket's parts library shown on xvfb's screen that QtRocket's tests pin by hand
 * (the family's hand-pinned.txt keeps this transcript after PresetGuiProbe's and
 * ConfigPresetRowProbe's): with ShownChooser's windows, DialogKeysProbe (the keys of the parts
 * library), SpaceRobotProbe (Space in its table) and PresetTableInputProbe (typed text and clicks
 * in its table, the widths of a first open), over the preset files given as arguments
 * (PresetModelProbe's database). A failure ends the program (its windows would keep it running).
 */
public class PresetShownProbe {

	public static void main(String[] args) {
		try {
			PresetModelProbe.bootstrap(args);
			ShownChooser.setUp();
			DialogKeysProbe.run();
			SpaceRobotProbe.run();
			PresetTableInputProbe.run();
			ShownChooser.tearDown();
		} catch (Throwable e) {
			e.printStackTrace();
			System.exit(1);
		}
		System.exit(0);
	}
}
