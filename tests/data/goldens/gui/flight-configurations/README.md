# The flight-configurations reference of the GUI

The Motors & Configuration tab on the design of the from-scratch scenario, in OpenRocket's GUI: the
reference of QtRocket's GUI exit test of Milestone 3 (`FlightConfigurationsExitGolden`, plan section
10.5), which does the same in QtRocket's GUI.

- `scenario.json`: the scenario (schema version 2), written by hand; both GUIs are driven by it.
  It opens `tests/data/goldens/gui/from-scratch/rocket.ork` (OpenRocket's save of the from-scratch
  scenario: an inner tube as the motor mount, no flight configuration), shows the tab and then:
  the body tube made a second motor mount with the motor mount table's check box, New with the
  motor chooser (Estes D12, delay 5) for the new row's last column while the body tube's cell
  shows its default ignition (`Default (Automatic)`, shaded), the body tube's check box cleared
  again; the chooser reopened for the nozzle exit diameter (5 mm) and once more cancelled (its
  search text stays); a second New (Estes C11, delay 7); the ignition for all configurations
  (Launch + 1 s), an override for the first one (Automatic), a cancelled ignition question, Reset
  ignition on both rows (Ctrl); on the Recovery sub-tab a deployment override (altitude 200 m), a
  cancelled deployment question, Reset deployment; Rename with "Save as default" of
  `Flight {motors}` (its message answered, the rename cancelled); New on the Recovery sub-tab with
  its deployment question for the drogue (Apogee + 1 s), the new configuration taking the new
  default name; renames `{motors} flight` and `{manufacturers | motors}`, a Plugged delay chosen in
  the chooser, Reset to default; Duplicate, Delete of the copy, Undo and Redo; Save as with a
  designer, file `rocket`.
- `rocket.ork`: the design OpenRocket's Save as wrote at the end of the scenario (the document of
  the archive, as plain XML; the archive also held the thrust curves of its two motors, which
  `state.json` lists with their SHA-256).
- `state.json`: what OpenRocket's GUI had after every step (the Undo and Redo actions, the
  selection, the open dialog, the window's tab, the questions asked, the tab's three tables, the
  motor mount table and buttons, the component tree) and `rocket.ork` as OpenRocket loads it.

`rocket.ork` and `state.json` are written by `tools/openrocket-gui-reference/generate.sh`, which runs
OpenRocket's real `FlightConfigurationPanel` and its dialogs on the scenario on a virtual display,
with OpenRocket 5f164fd0e (the commit of `tests/data/goldens/manifest.json`) and its preset submodule
at 1512874 on JDK 17, after a start-up with the bundled motors and component presets; the tool's
README describes the steps, how they run, and the format of `state.json`. Edit `scenario.json` by
hand and regenerate the other two; edit those through the generator only. QtRocket reads the files
through `tests/core/goldens/GoldenGuiReference.h`, and `FlightConfigurationsReferenceGolden`
(`tests/core/goldens/flight_configurations_reference_golden_tests.cpp`) checks that they belong
together and that QtRocket loads the input and `rocket.ork` into OpenRocket's states. The input is
the from-scratch reference: a change of that scenario needs a regeneration of this one too (the test
checks the SHA-256 of the input that `state.json` records).

Where QtRocket fixes an OpenRocket bug the scenario reaches (plan section 10.3), the record is
OpenRocket's with the fix made by the tool or OpenRocket's own, as the tool's README lists:

- Bug 5: New and the motor mount check box add their undo positions ("New Configuration", "Modify
  motor mount"; Duplicate "Duplicate Configuration"), which the tool adds in OpenRocket's run.
- Bug 6: the inner tube's default ignition (Launch + 1 s since step 9) survives the Undo and the
  Redo of the end, as QtRocket keeps it; the tool gives it back after OpenRocket's undo and redo.
- Bug 1: OpenRocket's deployment questions start on "this configuration" (steps 16 and 21, cells
  without an override), QtRocket's on "all configurations": `kGuiExpectedDifferences` of
  `GoldenGuiReference.h` lists them. The answers always name their scope, so the designs end alike.
- Bugs 2, 3, 4, 8, 20 and 21 are avoided: the ignition questions are asked on cells whose ignitions
  are overridden, the cancelled questions change nothing in OpenRocket either, Reset ignition acts
  on cells with motor configurations of their own, and the chooser is reopened only for cells
  whose motor the last search shows, with standard delays (Plugged is the last opening of its
  cell).
- The save writes no `<name>` for the two configurations named `Flight {motors}`, the default name
  at the save: their reload has OpenRocket's default name (`kGuiFlightConfigurationsReloadChanges`).
