# The from-scratch reference of the GUI

A rocket designed from scratch in OpenRocket's GUI, the reference of QtRocket's GUI exit test of
Milestone 2 (`FromScratchExitGolden`, plan section 9.5), which does the same in QtRocket's GUI:

- `scenario.json`: the scenario, what a user does step by step at the level of the GUI's actions
  (File > New, clicks on rows of the component tree, the add buttons by component class, the
  fields of the configuration dialogs by their Java property name with the typed text and its
  unit, the fin point table, Convert to freeform, Undo, Redo, Copy, Paste, Save as). Written by
  hand; both GUIs are driven by it.
- `rocket.ork`: the design OpenRocket's Save as wrote at the end of the scenario (the document of
  the archive, as plain XML).
- `state.json`: what OpenRocket's GUI had after every step (the Undo and Redo actions, the
  selection, the open dialog, the component tree) and `rocket.ork` as OpenRocket loads it.

`rocket.ork` and `state.json` are written by `tools/openrocket-gui-reference/generate.sh`, which
runs OpenRocket's own Swing GUI code (add buttons, models, actions) headless on the scenario, with
OpenRocket 5f164fd0e (the commit of `tests/data/goldens/manifest.json`) on JDK 17; the tool's
README describes the scenario's steps, what runs and what is replaced, and the format of
state.json. Edit `scenario.json` by hand and regenerate the other two; edit those through the
generator only. QtRocket reads the files through `tests/core/goldens/GoldenGuiReference.h`, and
`FromScratchReferenceGolden` (`tests/core/goldens/from_scratch_reference_golden_tests.cpp`) checks
that they belong together and that QtRocket loads `rocket.ork` into OpenRocket's state.
