# The simulations reference of the GUI

The Flight simulations tab and its dialogs on the design of the flight-configurations scenario, in
OpenRocket's GUI: the reference of QtRocket's GUI exit test of Milestone 4 (`SimulationsExitGolden`,
plan section 11.6 E6), which does the same in QtRocket's GUI.

- `scenario.json`: the scenario (schema version 3), written by hand; both GUIs are driven by it.
  It opens `tests/data/goldens/gui/flight-configurations/rocket.ork` (OpenRocket's save of the
  flight-configurations scenario: three configurations, D12-5, C11-Plugged and one without motors,
  and three simulations without flight data, the third one's configuration without motors), shows the
  simulations tab (its first row selected) and then, in its 34 steps:
  - Edit of Simulation 1 cancelled without a change (no question, nothing changed);
  - Edit of Simulation 1: renamed "Simulation 1 - wind levels"; the multi-level wind chosen and its
    levels edited in the wind editor (two levels added, the speed of the second 6 m/s, the third's
    altitude 50 m, which puts it between the others, the altitude column in feet, the level at 50 m
    deleted with "Delete level", the altitude reference AGL); the launch rod 1.5 m; launch into wind
    off; OK, whose run of the simulation is cancelled before it starts;
  - Simulation 2 selected and edited on the options and warnings pages: the RK6 stepper, the WGS84
    geodetic method, a constant gravity of 9.8 m/s², the time step 0.02 s, the maximum time 600 s, the
    fixed random seed 12345, the recovery warning at 25 m/s and the dual deployment's high speed
    warning at 35 m/s (in its collapsed section, which the step expands), a drag lookup table loaded
    from `drag.csv` and its text edited in the lookup dialog, the standard atmosphere off and a
    temperature of 20 °C; OK (its run cancelled);
  - an edit of Simulation 2's launch altitude cancelled with its discard question (Yes): the
    simulations as before;
  - New simulation, its launch conditions set by property: renamed "Simulation 4 - new", the
    configuration D12-5, an average wind of 4 m/s with a turbulence of 20 % from 45°, the launch site
    at 150 m, 35° N and 100° W, launch into wind off, the rod direction 30° and angle 5°, the standard
    atmosphere off, 18 °C, 1000 mbar and a humidity of 40 %; OK (its run cancelled). OpenRocket's new
    simulation starts with the rocket's selected configuration (the default one, without motors) and
    keeps the status "can't run" it gets then (Simulation.getStatus()); its duplicate is "not
    simulated";
  - Duplicate; the new simulation and its duplicate edited together (the multi-simulation edit: the
    rod length 2 m, which OK copies to the other); the duplicate selected and copied; the first row
    selected; Paste after it; a Delete of the pasted simulation cancelled at its confirmation, then
    its Delete with "Do not ask me again"; the last two rows (Ctrl) cut, without the confirmation;
    Undo and Redo of the Cut;
  - the stepper column shown, the apogee column hidden (the document preference
    `simulation.table.hiddenColumns`);
  - Paste of the two simulations of the Cut after the first row;
  - two wind imports into Simulation 1's levels: `wind-levels.csv` (comma-separated, a header row,
    the default column names and units), then, after a Reset levels answered No, `wind-levels-space.csv`
    (space-separated, the altitudes in feet), each overwriting the levels (OK, the runs cancelled);
  - Simulation 2's lookup table cleared (OK), as OpenRocket would save its file's absolute path;
  - a cancelled New simulation (the discard question with "Do not ask me again"): the new simulation
    goes again;
  - Save as with a designer, file `rocket`.
- `wind-levels.csv`, `wind-levels-space.csv`, `drag.csv`: the files the scenario's answers open.
- `rocket.ork`: the design OpenRocket's Save as wrote at the end of the scenario (the document of the
  archive, as plain XML; the archive also held the thrust curves of its two motors, which `state.json`
  lists with their SHA-256).
- `state.json`: what OpenRocket's GUI had after every step (the version 2 record: the Undo and Redo
  actions, the selection, the window's tab, the questions asked, the Motors & Configuration tab, the
  component tree's digest; and the simulations tab's table, selection, actions and clipboard, every
  simulation's options, the questions of the simulations tab with what their answers left, the fixes the
  tool made, the system clipboard's text after Copy and Cut) and `rocket.ork` as OpenRocket loads it.

`rocket.ork` and `state.json` are written by `tools/openrocket-gui-reference/generate.sh`, which runs
OpenRocket's real `SimulationPanel` and its dialogs on the scenario on a virtual display, with OpenRocket
5f164fd0e (the commit of `tests/data/goldens/manifest.json`) and its preset submodule at 1512874 on JDK 17,
after a start-up with the bundled motors and component presets; the tool's README describes the steps, how
they run, and the format of `state.json`. Edit `scenario.json` by hand and regenerate the other two
(`generate.sh simulations`); edit those through the generator only. The input is the flight-configurations
reference: a change of that scenario (or of the from-scratch one before it) needs a regeneration of this one
too.

`tests/core/goldens/simulations_reference_golden_tests.cpp` (suite `SimulationsReferenceGolden`, label
`golden`) checks that `state.json` is the record of the committed scenario and input, that the reference
comes from the OpenRocket of the other goldens, that every step of the tab changes what it is for and every
question belongs to its step, and that QtRocket loads the input and `rocket.ork` without warnings into
OpenRocket's state, the simulations' options included; `tests/core/goldens/GoldenGuiReference.h` reads the
files.

No simulation is run: the run dialog each OK opens is cancelled before its simulations start, so the
simulations hold no flight data and the save no flight values.

Where QtRocket fixes an OpenRocket bug the scenario reaches (plan section 11.4), the record is OpenRocket's
with the fix made by the tool (its member `toolFixes`, the records `kGuiSimulationToolFixes` of
`GoldenGuiReference.h` lists), the golden test's expected record makes QtRocket's behaviour
(`guiExpectedRecord()`), or the scenario avoids the bug, as the tool's README lists:

- Bug 32: the third level's altitude edit (step 3) leaves OpenRocket's levels out of order; the tool sorts
  them, as QtRocket's table does.
- Bug 33: "Delete level" (step 3) deletes the selected level, as QtRocket's does (OpenRocket's never does with
  the mouse).
- Bug 34: the space-separated import (step 26) splits at a space, as QtRocket's does (OpenRocket's at the word
  SPACE).
- Bug 6: the undo of the cancelled edits (steps 6 and 29) and the Undo and Redo of the Cut (steps 19 and 20)
  keep the inner tube's default ignition, as QtRocket does.
- Bug 27: OpenRocket's Paste is disabled without a selected row (steps 29 to 33, with two simulations on the
  clipboard); QtRocket's is enabled.
- Bugs 29, 30, 31, 35, 37 and 38 are avoided: the cancelled edit changes a field OpenRocket's equality
  compares, the cancelled New has no edit, the levels never share an altitude, the temperatures are above
  -80 °C, the imported files end in .csv, and no "Save as default" changes the geodetic method or the
  thresholds.
