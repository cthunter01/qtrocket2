# Differences from OpenRocket's GUI

QtRocket's GUI follows OpenRocket's Swing GUI (its development version at commit 5f164fd0e): its windows, menus,
dialogs, texts and rules. This page lists every difference a user can notice. Each one is also documented in the
source where it is made; the header named in parentheses says more (under `gui/include/QtRocket/gui/`, or
`include/QtRocket/` for the core). What OpenRocket has and QtRocket does not have yet is at the end.

## OpenRocket bugs that QtRocket fixes

### The Design tab, files and preferences

1. **Dragging several components in the tree.** OpenRocket moves them one at a time and can stop half way,
   move nothing or change their order. QtRocket checks the whole drop first, then moves them together in tree
   order (`tree/ComponentDrop.h`).
2. **Cut or Delete of every stage, of a stage that holds a booster set, or with the Rocket selected.** OpenRocket
   fails with an internal error, sometimes after deleting part of the selection. QtRocket keeps the last stage
   (as Delete does), cuts or deletes the stage with its boosters, and leaves the Rocket out
   (`main/ComponentEditing.h`).
3. **The default deployment, separation and ignition.** In OpenRocket, editing a recovery device's default
   deployment (event, delay, altitude), a stage's separation (event, delay) or a motor mount's default ignition
   (event, delay) does not mark the design as changed, and Cancel does not undo it. In QtRocket the design is
   marked as changed and Cancel undoes it (`configdialog/RecoveryDeviceConfig.h`, `AxialStageConfig.h`,
   `MotorConfig.h`).
4. **Cancel with nothing changed.** OpenRocket counts what a dialog changes while it opens (an automatic radius
   switched off when no neighbour gives one), so its Cancel undoes the step before the dialog, such as adding
   the component. QtRocket counts only what changes after the dialog opened
   (`configdialog/ComponentConfigDialog.h`).
5. **An inner tube's "Relative" separation.** OpenRocket stores "absolute" for both choices, so its cluster tab
   always opens on the absolute separation. QtRocket remembers "Relative" (`configdialog/InnerTubeConfig.h`).
6. **Component names with `<` or `&`.** OpenRocket puts names into its tooltips and labels as HTML, so
   "My <nose>" shows as "My ". QtRocket shows every name as typed (`tree/ComponentToolTip.h`,
   `configdialog/ComponentConfigDialog.h`).
7. **Right-click on overlapping components in the figure.** OpenRocket selects the last one under the mouse (the
   one drawn below, such as the body tube under an engine block). QtRocket selects the topmost
   (`figure/RocketPanel.h`).
8. **The deployment charge's symbol.** OpenRocket draws its rays at random on every paint, so they flicker.
   QtRocket draws them from the component's id: the same on every paint and every platform
   (`figure/MassComponentShapes.h` in the core).
9. **Shapes at the same depth.** OpenRocket draws them in an order that changes from run to run. QtRocket's
   order is stable (`figure/RocketFigureGeometry.h` in the core).
10. **Opening a file that is already open.** OpenRocket opens a second window on it, and the two windows then
    overwrite each other's saves. QtRocket brings the open window to the front (`main/DocumentFileOperations.h`).
11. **The Examples menu.** OpenRocket compares the example names with its ordered list exactly, so "3D printable
    nose cone and fins" ends up last, after a separator. QtRocket also matches names that differ only in case
    (`main/ExampleDesigns.h`).
12. **Warnings while saving an `.ork` file.** OpenRocket shows none. QtRocket shows them with OpenRocket's own
    "Warnings while saving file" dialog; the file is saved all the same (`main/DocumentFileOperations.h`).
13. **An infinite number in a field.** OpenRocket stores the field's bound or Infinity for "1/0" or "2^2000".
    QtRocket refuses it, like invalid text (`binding/DoubleBinding.h`).
14. **Cancel in the Preferences.** OpenRocket can leave a unit changed on the Units page in force. QtRocket
    restores the units, and every other preference, as they were when the dialog opened
    (`dialogs/PreferencesDialog.h`).
15. **"Design name" in Save Design Info** (the user's decision R3-1). OpenRocket's field starts empty, and
    leaving it renames the rocket to its default name. QtRocket's field shows the rocket's name
    (`dialogs/SaveDesignInfoDialog.h`).
16. **Undo and redo of the design information** (the user's decision R3-2). OpenRocket's undo and redo keep the
    rocket's current designer, revision, kit name and design type, so a Cancel in the Rocket's dialog undoes its
    name and comment but not those four. QtRocket restores them too (`rocket/Rocket.h` in the core).

More OpenRocket bugs, found while porting, that QtRocket fixes the same way:

- **Several components selected for a structural edit.** With several fin sets selected, OpenRocket's "Convert to
  freeform" and "Split fins" act on only the first two. With four or more selected, they fail with an internal
  error. "Split pods" and "Split boosters" do the same with several pod sets or booster sets, and can leave an
  undo step open. QtRocket converts or splits every selected component (`configdialog/FinSetConfig.h`,
  `ComponentAssemblyConfig.h`).
- **Opening a nose cone or transition dialog for several components.** OpenRocket silently gives every other
  selected nose cone or transition the main one's shape, and this cannot be undone. QtRocket changes nothing
  (`binding/TransitionShapeBinding.h`).
- **The freeform fin editor with several fin sets selected.** After the editor's own undo or redo, OpenRocket's
  fin sets share one point list, so the next edit moves their points twice. QtRocket keeps each fin set's
  points apart (`figure/FinPointSelection.h`).
- **The freeform fin editor during a drag.** A drag whose mouse release never comes (the dialog closed, an undo)
  leaves OpenRocket's design frozen. A second mouse button pressed during a drag freezes it twice. QtRocket ends
  the drag (`figure/FinPointScrollArea.h`).
- **Import from image.** An image whose outline never comes back to the bottom row makes OpenRocket run until it
  is out of memory. QtRocket reports "Could not deduce fin shape from image.". An indexed-colour image without
  pure black imports as a single point in OpenRocket; QtRocket imports its outline (`util/CustomFinImporter.h`).
- **A stored preference choice out of range.** OpenRocket's preferences page fails with an internal error.
  QtRocket shows the first choice (`dialogs/PrefChoiceSelector.h`).
- **A `$` or `\` in a file name.** OpenRocket's overwrite question fails or drops characters. QtRocket shows the
  name as it is (`main/WidgetUserPrompts.h`).
- **A paste that fits nowhere.** OpenRocket adds an empty undo step, whose name then labels the next change.
  QtRocket adds none (`main/ComponentEditing.h`).

### Motors, flight configurations and component presets

These have the numbers the source's comments give them ("bug 6"). Bugs 20 and 21, and the second halves of 3
and 4, are the user's later decisions.

The "Motors & Configuration" tab:

1. **Cells that use the default.** OpenRocket compares configurations by identity, so it never shows a recovery
   device's or a stage's cell in the dim italics of a default value, and its deployment and separation dialogs
   always start on "Override for the ... flight configuration only". QtRocket dims every such cell that has no
   value of its own, and starts these dialogs on "Change all configurations ..." for it
   (`flightconfig/FlightConfigurationCellText.h`, `FlightConfigurationEditing.h`).
2. **Where the ignition dialog starts.** OpenRocket's starts on "Change all configurations ..." exactly when the
   cell has no motor. QtRocket's starts there when the cell has no ignition of its own, and then shows the
   default ignition the cell shows, so that OK without a change keeps the default
   (`flightconfig/FlightConfigurationEditing.h`).
3. **Cancel in the ignition, separation and deployment dialogs.** OpenRocket's ignition Cancel still marks the
   ignition as overridden (on a cell without a motor, the mount's default ignition), its close button keeps the
   edits, and its separation dialog keeps its edits on Cancel. With several cells selected, the other cells
   then take the first cell's values, also after a Cancel of the deployment dialog. In QtRocket, Cancel and the
   close button change nothing, in any of the three (`flightconfig/FlightConfigurationEditing.h`).
4. **The ignition of a cell without a motor.** OpenRocket's "Override for the ... flight configuration only"
   writes the values into the mount's default ignition, which every configuration without an ignition of its
   own uses, and its "Reset ignition" resets that default. QtRocket gives the cell an ignition of its own, and
   its "Reset ignition" leaves such a cell alone (`flightconfig/FlightConfigurationEditing.h`).
5. **New Configuration, Duplicate Configuration and the motor mount check box.** OpenRocket adds no undo step for
   them, so Undo takes them back together with the change before. QtRocket gives each an undo step of its own:
   "New Configuration", "Duplicate Configuration", "Modify motor mount"
   (`flightconfig/FlightConfigurationEditing.h`).
6. **A motor mount's default ignition.** OpenRocket loses it on undo, redo, copy and paste, duplicate and when a
   cluster is split: it comes back as "Automatic" with no delay. QtRocket keeps it
   (`rocket/MotorConfigurationSet.h` in the core).

The motor chooser and its plot:

7. **The close button after an earlier OK.** OpenRocket's chooser remembers the OK of its last use, so closing
   its window takes the motor as chosen. QtRocket's close button cancels, as Cancel and Escape do
   (`dialogs/motor/MotorChooserDialog.h`).
8. **A delay that is not one of the motor's standard delays.** Opened for such a motor, OpenRocket's chooser
   shows the closest standard delay instead, and OK writes it: a C6 set to 6 s comes back with 7 s. QtRocket
   keeps the delay that was set (`motor/chooser/ThrustCurveMotorSelection.h` in the core).
9. **"Hide motors already used in the mount".** OpenRocket's box also hides the motors of every mount the
   chooser was opened for before. QtRocket's hides those of the current mount only
   (`motor/chooser/MotorRowFilter.h` in the core).
10. **Switching the motor name column.** With the table sorted by its Name column, OpenRocket selects another
    motor when the column switches between common names and designations: the one that comes into the same
    row. QtRocket keeps the selected motor and scrolls to it (`dialogs/motor/ThrustCurveMotorSelectionPanel.h`).
11. **"NaN" as a delay.** OpenRocket takes it, shows it and writes it into the design. QtRocket keeps the delay as
    it was (`motor/chooser/ThrustCurveMotorSelection.h` in the core).
12. **The units of the thrust curve plot.** OpenRocket's "Motor thrust curves" dialog labels its axes in the
    default units, such as lbf, but plots seconds and newtons. QtRocket plots in the units the axes name
    (`plot/ThrustCurvePlot.h`).

Two more in the chooser, found later:

20. **The chosen thrust curve.** With "Hide very similar thrust curves" on, OpenRocket's curve list can end on
    another curve when it fills again, for seven curves of six motors (the AeroTech E15W's among them): when the
    chooser opens for that curve, when it is the motor's preferred curve, and when the box is ticked while it is
    chosen. QtRocket keeps the chosen curve (`dialogs/motor/ThrustCurveMotorSelectionPanel.h`).
21. **The configured motor when the mount's limits hide it.** After the chooser was used for another mount,
    OpenRocket keeps that motor selected when "Limit motor length to mount length" or "Limit motor diameter to
    mount diameter" hides the configured one, and OK then writes the other motor (in "Airstart timing", the
    K550W of its 54 mm mount becomes the I211W of its 38 mm mount). QtRocket starts from the configured motor
    (`dialogs/motor/ThrustCurveMotorSelectionPanel.h`).

The parts library:

13. **Sort in the column header's menu.** With the Legacy column hidden, OpenRocket's "Sort Ascending" and "Sort
    Descending" sort by another column: Manufacturer sorts by Legacy. QtRocket sorts by the column clicked
    (`dialogs/preset/ComponentPresetTable.h`).
14. **Showing or hiding the Legacy column.** OpenRocket's header menu ticks or unticks "Show Legacy Database"
    but leaves the legacy parts as they were, and hiding whatever column is second unticks the box and hides the
    legacy parts. In QtRocket the column, the box and the legacy parts always agree
    (`dialogs/preset/ComponentPresetTable.h`).
15. **Cancel after a preset for a new component.** The question says that the new component will be removed,
    but OpenRocket's Cancel takes back the preset and leaves the component. QtRocket removes it
    (`configdialog/ComponentConfigDialog.h`).
16. **"Match fore diameter" for a flipped nose cone.** OpenRocket compares with the previous component's fore
    diameter, which is right only behind a tube. QtRocket compares with its aft diameter, where the cone meets
    it (`binding/PresetDiameterFilters.h`).
17. **"Filter by text" and the check box columns.** OpenRocket's filter also searches the "true" and "false" of
    the Favorite, Legacy and Filled columns, so "e" matches every part and "true" every legacy or filled one.
    QtRocket searches the text columns only (`dialogs/preset/ComponentPresetFilterModel.h`).

The component preset files:

18. **A broken component preset file.** A user's preset file with certain errors (a required element missing,
    an unknown shape or unit) ends OpenRocket's whole preset load with an internal error, so the files after it,
    the user's included, are left out. QtRocket leaves out that file alone and names it in the message after
    start-up (`file/preset/ComponentPresetDatabaseLoader.h` in the core).
19. **A number that is not finite** (NaN, INF, 1e999) in a preset file. OpenRocket gives it to the part.
    QtRocket leaves the part out (`file/preset/OpenRocketComponentLoader.h` in the core).

More OpenRocket bugs, found while porting, that QtRocket fixes the same way:

- **Changes the tab does not record.** Some answers of OpenRocket's ignition, deployment and separation dialogs
  change the design unseen: "Change all configurations ..." for a deployment on a cell with its own deployment,
  a separation whose altitude alone changes, and a few more. Its tables and its undo history miss them. QtRocket
  records every change (`flightconfig/FlightConfigurationEditing.h`).
- **Tab and the arrow keys in the parts library's table.** OpenRocket moves the selection without scrolling, so
  the selected part can leave the visible rows; and in a table that the filter emptied, Tab and then Close fail
  with an internal error. QtRocket scrolls to the new row and does nothing in an empty table
  (`widgets/TableRowTraversal.h`).
- **A stored column order below zero.** OpenRocket's parts library fails with an internal error. QtRocket
  ignores it (`settings/TableUiPreferences.h`).
- **`<` or `&` in a motor's cases or a file's name.** OpenRocket's "Compatible Cases" and its warnings about
  files it cannot import read them as HTML. QtRocket shows them as they are
  (`dialogs/motor/MotorInformationPanel.h`, `main/WidgetUserPrompts.h`).
- **Presets that cannot work.** A parachute without a line count, or a recovery device whose material is of the
  wrong kind, loads in OpenRocket and fails with an internal error when it is chosen, and a part without a part
  number stops OpenRocket's load as in bug 18. QtRocket leaves out the part, or the file without a part number
  (`file/preset/OpenRocketComponentLoader.h` in the core).
- **A special file among the preset or thrust-curve files.** A named pipe in a directory of preset or
  thrust-curve files makes OpenRocket's load wait forever, and a link to a device such as `/dev/zero` makes it
  read without an end. QtRocket reads regular files only, so its start, which reads these files before the first
  window, cannot hang on them (`file/preset/ComponentPresetDatabaseLoader.h` and
  `file/motor/MotorDatabaseLoader.h` in the core).

## Editing several components at once

In OpenRocket, a dialog for several selected components forwards each edit to the other components. Values
derived from the edit, however, come from the main component (the one whose values the fields show). QtRocket
calls each component's own setter, the others first and the main component last, in one undo step (the user's
decision U3). The results differ for 13 setters, listed in `binding/ComponentSelection.h`. For example, an inner
diameter gives each tube a wall from its own outer diameter, a density gives each mass component its own mass,
and a sweep angle gives each fin set a sweep from its own height. In OpenRocket, the others get the main
component's wall, mass or sweep. Also:

- The placement field and slider move every selected component. OpenRocket moves the main component alone.
- A value the main component already has still reaches the others. OpenRocket leaves them alone then, for
  example for a nose cone's or transition's length.
- Text that is not a number sets nothing. OpenRocket sets the main component's value again, on every selected
  component.
- A component keeps its preset unless its own value changed.
- Switching the CD override on gives each component its own calculated CD.
- Choosing the preset the main component already has still gives it to the others, in the undo step "Use
  Preset ...". OpenRocket gives it to them outside the undo history (`binding/PresetBinding.h`).

## Windows, menus and keys

- QtRocket uses the platform's own widget style and its light or dark scheme, not OpenRocket's FlatLaf look.
  OpenRocket's icons are recoloured to match.
- On macOS, unsaved changes show as the dot in the window's close button, not as a `*` before the title.
- Redo uses the platform's standard keys: Ctrl+Shift+Z on Linux, Cmd+Shift+Z on macOS, Ctrl+Y on Windows.
  OpenRocket binds Ctrl+Y (Cmd+Y) everywhere. The other items with standard keys (Close design, Cut, Copy, ...)
  also answer the platform's alternatives, such as Ctrl+F4 on Windows.
- Paste and Delete have no menu mnemonic: their mnemonic letters (V, and the Delete key) are not in their texts.
- Undo, Redo, Save, Save as and Close design (also through Quit) first commit the number or text still being typed
  in the configuration dialog. OpenRocket loses it on an undo or redo, and when it closes a saved design; QtRocket
  then asks whether to save the change. A commit that is itself a change ends the redo history, so Redo then does
  nothing more (`main/UndoRedoActions.h`, `main/DocumentFileOperations.h`, `main/WindowManager.h`).
- The component tree has the keyboard focus when a window opens.
- On macOS, Preferences is in the application menu only while a design window is open.
- Left out until later milestones: the menu items listed in the last section. The Help menu is hidden while it is
  empty.

## Files

- Saved designs have no preview image, so OpenRocket's file chooser shows none for them (Milestone 5).
- The storage options ("Save options") are asked only for a design that holds simulated data, as a question
  after the file dialog. OpenRocket's Save as dialog always shows them beside the files.
- File > Open's filter is "OpenRocket designs (*.ork *.ork.gz)". The RockSim filter comes with Milestone 8.
- The platform's native file dialogs are used, but for two that are Qt's own: Save as on macOS (the native panel
  would ask whether to overwrite before QtRocket forces the `.ork` extension) and Import from image (it shows
  OpenRocket's description of the import beside the files).
- Designs open and save without OpenRocket's progress dialog and its Cancel. Even the largest example takes a
  fraction of a second.
- A failed save shows QtRocket's error text after "An I/O error occurred while saving:", where OpenRocket shows
  the Java exception.
- Components pasted or dropped into another design take their decal images along. A name that design already
  uses gets a number (`a (1).png`), and both images are saved. OpenRocket's paste leaves the images out of the
  target's decal list, and its save writes only one image of each name (`document/DecalRegistry.h` in the core).
- Each file of a macOS open-files request opens. OpenRocket opens only the first.

## Component tree and editing

- A click to the right of a name, in the indentation or on the expand handle selects the row. OpenRocket
  treats these as clicks on empty space and clears the selection.
- Shift+click keeps the rows picked before with Ctrl.
- The keyboard's menu key opens the component menu.
- The selection highlight covers the whole row, and a name that does not fit is shortened with "...". OpenRocket's
  tree grows sideways instead.
- Dropping onto the lower edge of an expanded row with children puts the components after that row.
  OpenRocket makes them its first children.
- Paste, Duplicate, a copying drop and the add buttons refuse, with a message, to make a design of more than 1000
  components or with more decal images than a design may hold (`tree/ComponentLimits.h`).
- The add buttons wrap where the panel ends, with the platform's spacing.

## Configuration dialogs and fields

- The dialog remembers its position, not its size. It takes its size from its contents and has no scroll bars.
- A dialog for components of several classes opens on its first tab. OpenRocket selects the second tab.
- Choosing the design type the Rocket's combo box already shows changes nothing. OpenRocket marks the dialog as
  changed.
- A stage's separation fields always show the flight configuration selected at the moment. OpenRocket's keep the
  one selected when the dialog opened.
- Number fields:
  - Enter commits the value and keeps the focus. OpenRocket moves the focus away.
  - PageUp and PageDown step ten times.
  - The mouse wheel steps a field only while it has the focus. OpenRocket's fields ignore the wheel.
  - Any focus loss commits the text being typed, also when a menu opens, such as the unit selector's.
    OpenRocket keeps the edit pending then.
  - After a step with the arrows, the text is selected.
- Count fields refuse a key that cannot make a number in their range.
- A unit selector opens on the press of any mouse button.
- Combo boxes choose with the wheel only while they have the focus. The keys on a closed combo box follow the
  platform.
- The name field and the comment area show changes made elsewhere, unless they hold an edit. OpenRocket
  overwrites the name field even while it is edited, and never refreshes the comment area.
- The material combo box's search lists every material that matches, including two whose names differ only in
  case. The check mark goes to the material equal to the component's.
- Texts in which OpenRocket names itself (a tube fin set's and a mass component's notes, "Reload QtRocket"
  after resetting the preferences, the preferences' note on thrust curve files) name QtRocket.

## Freeform fin editor

- In the point table, Enter commits the cell and closes its editor. OpenRocket moves on to the cell below. The
  arrow keys move without starting an edit. The column headers follow the default length unit.
- A press on a point in the figure selects its row also when the editor did not have the focus.
- A click is a press and release at the same pixel. A double click counts as two clicks.
- "Ctrl" (Ctrl+click to delete a point, Ctrl+Shift to snap) is the Command key on macOS.
- Export CSV ends its lines with `\n` on every platform. A failed write shows a message; OpenRocket only logs it.
- Import from image reads the formats Qt reads. A greyscale image with alpha, and an image with a colour profile,
  keep their stored grey levels: OpenRocket converts them first, so a pixel near the threshold can fall on the
  other side.

## Design view

- A double click follows the platform's interval and distance. In OpenRocket, a third quick click counts as a
  double click again.
- The view and zoom combo boxes ignore the mouse wheel. The zoom box completes nothing as it is typed.
- After an undo, the selected components stay highlighted. OpenRocket highlights nothing until the next
  selection.
- The information overlay's text sizes follow the platform's font.

## The "Motors & Configuration" tab

- The items of the tables' popup menus are disabled whenever their buttons are (the user's decision U-M3-6d).
  OpenRocket's items always act.
- A sub-tab that is disabled while it is shown gives way to Motors (U-M3-6f). OpenRocket stays on Recovery or
  Stages when they are disabled because no motor mount is active.
- In the ignition, deployment and separation dialogs, Escape (which answers OK there, as in OpenRocket) takes
  the number still being typed. OpenRocket's loses it (`dialogs/FlightConfigurationValueDialog.h`).
- When "Change all configurations using the default separation event" also changes other selected cells that
  use a default (of another stage, say), each gets a copy of the new default as its own separation. In
  OpenRocket each gets the default itself, so a later change of the default still reaches it
  (`flightconfig/FlightConfigurationEditing.h`).
- A column resized by hand takes its width from the last column. OpenRocket's takes it from all the columns after
  it.
- Cut, Copy and Paste in a table also answer the platform's other keys for them, such as Ctrl+Insert on Linux
  and Windows, as the Edit menu does (`flightconfig/TableTransferKeys.h`).
- After "Save as default" with an empty name, the configurations made since follow the next default name that
  is saved. OpenRocket's keep the empty name (`document/OpenRocketDocument.h` in the core).

## The motor chooser

- OK and Cancel are in the platform's order. The chooser opens at the size its contents ask for, but at most two
  thirds of the screen; OpenRocket's ignores the screen's size.
- The range sliders are the platform's sliders with two thumbs. A thumb dragged onto the other takes exactly its
  value, where OpenRocket's can stop one step short or pass it (`widgets/RangeSlider.h`).
- The length fields refuse an infinite value, as every number field does.
- Opening the delay list commits the delay being typed. OpenRocket keeps the edit until an item is chosen.
- The search ignores case by Unicode's rules, and the text columns of the motor table and the parts library sort
  in English order, whatever the system's language. OpenRocket follows the language (in Turkish, `I` is not the
  upper case of `i`) (`motor/chooser/MotorRowFilter.h`, `util/JavaCollator.h` in the core).
- "Motor DB version" is always the version of the motor database QtRocket ships, which it does not update
  (`app/MotorDatabaseMetadataIO.h`).

## Plots

The thrust curve preview of the motor chooser and its "Motor thrust curves" dialog are drawn by
[rocketplot](https://github.com/cthunter01/rocketplot), not JFreeChart (the user's decisions U-M3-6a and b):

- They follow the platform's light or dark scheme, with rocketplot's fonts and grid lines. OpenRocket's dialog
  plot is always white (`plot/ThrustCurvePlot.h`).
- The mouse: a left drag zooms to the box dragged out, Shift and a left drag pans, a double click resets the
  view, the wheel zooms about the pointer (over an axis that axis only; Ctrl and the wheel the time only, Shift
  and the wheel the thrust only), a trackpad scrolls and, with Ctrl, zooms, a pinch zooms, and the back and
  forward mouse buttons go through the views. JFreeChart restores the view after a drag up and to the left and
  zooms with the wheel only over the data (`plot/PlotInputBindings.h`).
- The right button opens rocketplot's menu (Back, Forward, Reset view, the crosshair, Copy image, Export...) in
  place of JFreeChart's. Export... writes PNG, JPEG, SVG, PDF or CSV files, adds the suffix of the chosen kind
  when the name has none of them, asks before replacing a file and says when the file cannot be written
  (`plot/PlotExport.h`).
- A legend entry of the dialog has a menu (the line width and markers, Visible, Show only this, Show all,
  Remove, Theme color), which OpenRocket's legends do not have. A curve recoloured there keeps its colour when
  the scheme changes.
- In the dialog, the selected curve is drawn under the others, where OpenRocket draws it on top. This will
  change with a later rocketplot.

## The parts library

- "Filter by text" takes a Qt regular expression, ignoring case by Unicode's rules (the user's decision
  U-M3-6e). A pattern that is not valid matches nothing, where OpenRocket shows every part
  (`dialogs/preset/ComponentPresetFilterModel.h`).
- The columns keep their widths, and the table scrolls sideways when they do not fit. OpenRocket's table fits
  them to its width. The Favorite check boxes sit where the platform's style puts them.
- Choosing a unit in a column header's menu keeps the selected part. OpenRocket clears the selection.
- A part whose manufacturer and part number together are longer than about 80 characters can be a favourite.
  OpenRocket refuses to store it, with an error (`rocket/preset/ComponentPresetFavorites.h` in the core).
- The keypad's Left and Right arrows move to the previous or next part, as the other Left and Right arrows do.
  In OpenRocket they move between the columns (`widgets/TableRowTraversal.h`).
- In the open list of a configuration dialog's preset box, the arrow keys only move the highlight; Enter or a
  click chooses. OpenRocket's list chooses at every arrow key, each choice a preset in an undo step of its own
  (`widgets/PresetComboBox.h`).

## Component preset files

- QtRocket refuses a preset file over 64 MiB or with more than 1,000,000 XML elements or 10,000 materials, reads
  the first 100,000 parts of a larger file, takes at most 10,000 files and 100 levels of a directory, and at
  most 1,000,000 presets in all. OpenRocket reads any amount (`file/preset/OpenRocketComponentLoader.h`,
  `ComponentPresetDatabaseLoader.h` in the core).
- A file whose document type declares anything (an XML entity) is refused, as design files are.
- A directory's files are read in the order of their names. OpenRocket's order is the system's.

<!-- RUN 3 MERGE: the deliberate differences that groups X and Y (the tab's exit suite, the open-designs exit and
the parts library per component) report go into the sections above, or here. Remove this comment then. -->

## Preferences

- The preferences are kept in an INI file of QtRocket's own (`~/.config/QtRocket/QtRocket.ini` on Linux and
  macOS, `%APPDATA%\QtRocket\QtRocket.ini` on Windows), with window positions and maximized states in its own
  form. OpenRocket's preferences are not read or imported.
- The dialog has the General, Design and Units pages. General has the user's thrust curves and component
  presets, "Open last design file on startup", the confirmation for discarding preferences, "Automatically open
  the Parts Library dialog" and "Reset all preferences". Design has no simulation options (Milestone 4). Left
  out: the language selector, the update checks, the import and export of preferences, and the RASAero and
  RockSim warnings (RockSim with Milestone 8).
- "Add" beside the user's thrust curves or component presets first asks in a menu, "File..." or "Directory...",
  then shows the platform's dialog for a file or for a directory (the user's decision U-M3-6c). OpenRocket's
  dialog shows files and directories at once. The filters are OpenRocket's, followed by their patterns where
  OpenRocket's text holds more than patterns (`dialogs/GeneralPreferencesPanel.h`).
- While a list is unset it shows QtRocket's own directory (see Start-up), which "Reset" puts back. The page
  makes no directory: the start does. A change takes effect at the next start, as the page says.
- A list of the user's files longer than 8192 characters is stored. OpenRocket refuses it with an error.
- Apply, OK and Cancel refresh every design window. OpenRocket refreshes only the window the dialog came from.
- "Reset all preferences" keeps the dialog open with new pages.

## Start-up and errors

- There is no splash screen, welcome dialog or update check.
- The motor database, the component presets and the user's thrust curves and presets are read before the first
  window opens, in a fraction of a second. OpenRocket reads them in the background and shows "Loading motors"
  when a motor is wanted before they are read (`app/ApplicationContext.h`).
- The user's own files are in QtRocket's data directory: `~/.local/share/QtRocket` on Linux,
  `~/Library/Application Support/QtRocket` on macOS, `%LOCALAPPDATA%\QtRocket` on Windows (the user's decision
  U-M3-3), with the folders `ThrustCurves` and `Components`, made at start while their lists are unset.
  OpenRocket's are in `~/.openrocket`, `~/Library/Application Support/OpenRocket` or `%APPDATA%\OpenRocket`
  (`app/DataPaths.h`).
- The user's files that cannot be imported are listed together in one message after the first window opens,
  with OpenRocket's texts: "Error upon thrust curve import", "Error upon component preset import", or both
  titles; the message scrolls when the files are many. OpenRocket shows a warning for each file, always on top,
  while its window opens. Both name only the files that were refused. For a preset file, QtRocket's message
  adds the reason after OpenRocket's `Unable to parse component preset file: <name>`. The other problems (a
  listed file that is missing, say) OpenRocket writes to its log, which QtRocket does not have
  (`main/WidgetUserPrompts.h`).
- If the motor database QtRocket ships cannot be read, a warning says so after the first window opens, and the
  user's thrust curves are not read either. OpenRocket stops the same way, with an internal error.
- The internal error dialog has QtRocket's texts and the buttons Continue, Copy details and Quit QtRocket.
  OpenRocket offers a bug report.
- On macOS, a new design opens whenever the application becomes active without a window, also through
  Cmd-Tab. OpenRocket does so only when the Dock icon is clicked.

## Expressions in number fields

- The minus sign is `-` in every language. OpenRocket also reads the locale's minus sign, such as U+2212 in
  Swedish.
- An expression longer than 1000 bytes, or with more than 100 brackets open at once, is refused.

(`util/ExpressionParser.h` in the core.)

## Not in QtRocket yet

These come with later milestones of QtRocket:

- Milestone 4: the Simulations tab (the simulations that New and Duplicate Configuration add are kept in the
  saved design), the simulations' plots and CSV export in the GUI; the flight block of the design view's
  overlay.
- Milestone 5: the Appearance tab and decal editing, the scale dialog, the material editor ("Add custom
  material", "Edit materials"), "Components of same color", "Scale Fin", "Save decal images", calipers, view
  capture, the preview image in saved files, the theme selector, Help > About and License.
- Milestone 6: the 3D views.
- Milestone 7: component analysis, optimization, custom expressions and the Tools menu.
- Milestone 8: RockSim import and export, SVG export, printing.
- Milestone 9: packaged releases of the GUI, the splash screen, Help > Bug report.

Not planned for QtRocket: RASAero export, Wavefront OBJ export, Photo Studio, guided tours, translations
(the language selector), the debug log, update checks, the motor search, download and database update of
thrustcurve.org, the component preset editor, and the "banana for scale".
