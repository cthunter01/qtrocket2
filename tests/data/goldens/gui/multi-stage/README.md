# The multi-stage reference of the GUI

The Motors & Configuration tab on a two-stage design, in OpenRocket's GUI: the second reference of
QtRocket's GUI exit test of Milestone 3 (`FlightConfigurationsExitGolden`, plan section 10.5), which
does the same in QtRocket's GUI.

- `scenario.json`: the scenario (schema version 2), written by hand; both GUIs are driven by it.
  It opens `tests/data/goldens/example-two-stage-high-power-rocket/resave/rocket.ork` (OpenRocket's
  re-save of its example "Two stage high power rocket": two configurations, a motor in each stage,
  no simulation data), shows the tab and then: on the Stages sub-tab the booster's separation for
  each configuration (burnout + 1 s; 300 ft during the descent), for all configurations (ejection
  + 0.5 s, which removes the overrides) over both rows (Shift), Never for the second one, Reset
  separation; New on the Stages sub-tab with its separation question (apogee + 2 s), Undo of that
  separation (Undo then names New's own undo position) and Redo; on the Recovery sub-tab the
  deployment of the sustainer's drogue for all configurations, a question of an upper stage (its
  events include the lower stage's separation: lower stage separation + 1 s), which keeps the
  overrides of the first two configurations (the default separation removed them); on the Motors
  sub-tab the upper stage's ignition of the second configuration (burnout + 2 s) and the booster's
  motor of that configuration deleted; on the design tab the sustainer's motor mount made a cluster
  of four (`4-ring` in its configuration dialog: the tab shows `4× ...`); back on the tab, Undo and
  Redo; Save as, file `rocket`.
- `rocket.ork`: the design OpenRocket's Save as wrote at the end of the scenario (the document of
  the archive, as plain XML; the archive also held the thrust curves of its two motors, which
  `state.json` lists with their SHA-256).
- `state.json`: what OpenRocket's GUI had after every step and `rocket.ork` as OpenRocket loads it,
  as for `flight-configurations`.

`rocket.ork` and `state.json` are written by `tools/openrocket-gui-reference/generate.sh` as those of
`flight-configurations` are (see its README.md); edit `scenario.json` by hand and regenerate the
other two. `FlightConfigurationsReferenceGolden`
(`tests/core/goldens/flight_configurations_reference_golden_tests.cpp`) checks them, and the
SHA-256 of the input `state.json` records: a regeneration of the goldens
(`tools/openrocket-goldens/generate.sh`) that changes the re-save needs a regeneration of this
reference.

Where QtRocket fixes an OpenRocket bug the scenario reaches (plan section 10.3):

- Bug 5: New adds its undo position ("New Configuration"), which the tool adds in OpenRocket's
  run: the Undo of step 14 shows it.
- Bug 6: the motor mounts' default ignitions (overrides since OpenRocket's load of the example)
  survive both pairs of Undo and Redo (steps 14 and 15, 30 and 31), as QtRocket keeps them; the
  tool gives them back after OpenRocket's undo and redo.
- `kGuiToolFixes` of `GoldenGuiReference.h` lists the records where these two fixes show.
- Bug 1: OpenRocket's separation and deployment questions start on "this configuration" (steps 10,
  13 and 18, cells without an override), QtRocket's on "all configurations":
  `kGuiExpectedDifferences` of `GoldenGuiReference.h` lists them. OpenRocket's Recovery and Stages
  tables shade no cell; the records hold each cell's shading and whether its configuration has an
  override of its own, and the exit test expects QtRocket's shading of every cell without one
  (`guiExpectedRecord()`).
- Bugs 2, 3 and 4 are avoided: the ignition question is asked on a cell whose ignition is
  overridden, no question is cancelled, nothing is reset on a cell without a motor configuration.
- The reload is the saved rocket (`kGuiMultiStageReloadChanges` is empty).
