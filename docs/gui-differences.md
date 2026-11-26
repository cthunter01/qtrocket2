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
   value of its own, and starts these dialogs on "Change all configurations ..." for it: in the 66 designs the
   exit suite opens (the examples, the re-saves and the GUI reference), 271 of the 294 recovery and stage cells are
   dimmed, where OpenRocket dims none
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
   (`rocket/MotorConfigurationSet.h` in the core). So a motor mount pasted into another design shows its kept
   default in the configurations it has no motor for: "None" / "Automatic", not dimmed. OpenRocket's copy starts
   from a fresh default and shows "Default (Automatic)", dimmed. After saving and reopening, both show the cell
   alike (`tests/gui/exit/pasted_motor_mount_tests.cpp`).

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

### Simulations, plots and CSV files

These go on from the numbers of the list above, as the source's comments give them ("OpenRocket bug 23"). The
user decided to fix them (U-M4-4).

The "Flight simulations" tab:

22. **A simulation pasted into another design.** OpenRocket's pasted simulation still reports its changes to the
    design it was copied from: renaming it or changing its settings marks that design as changed and leaves its
    own design saved, so closing its own design does not ask to save the change. In QtRocket it belongs to the
    design it was pasted into (`simulation/SimulationEditing.h`; `document/SimulationCopy.h` in the core).
23. **Sorting by the Warnings column.** A click on its header leaves OpenRocket's order as it was. QtRocket sorts
    by the number of critical warnings, then of normal ones, then of informational ones; a simulation without
    results comes first in the ascending order (`simulation/SimulationColumns.h`).
24. **New, Duplicate, Paste and Plot / Export in a sorted table.** With the table sorted, OpenRocket's Paste puts
    the simulations elsewhere in the list than after the selected one, and New, Duplicate, Paste and Plot / Export
    then select other simulations than the new or the plotted ones. QtRocket pastes after the simulation of the
    topmost selected row and selects the new or the plotted simulations themselves
    (`simulation/SimulationEditing.h`, `SimulationPanel.h`).
25. **The statuses that Copy puts on the clipboard.** Copy also puts the selected simulations on the system
    clipboard as text, with a Status column. A simulation without results has no line there, and OpenRocket then
    gives every later line the status of the simulation before it and leaves out the last statuses. In QtRocket
    each line has its own simulation's status (`simulation/SimulationTableCsvExport.h`).
26. **Clicks after columns were hidden or moved.** OpenRocket goes by the place of the column clicked: after the
    Status or the Warnings column is hidden or moved, a click in whatever column comes first runs an out-of-date
    simulation, and a double click in the second column opens the Warnings tab. QtRocket goes by the column
    itself, wherever it is (`simulation/SimulationTableView.h`).
27. **Paste without a selected simulation.** OpenRocket pastes simulations only while one is selected, so never
    into a design without simulations. QtRocket's Paste needs only simulations on the clipboard: without a
    selected one, they go to the end of the list (while the tab shows its table, which it hides while no flight
    configuration has motors). The Edit menu's Paste does the same while the Flight simulations tab is shown
    (`simulation/SimulationPanel.h`, `main/RocketActions.h`).
28. **The selection in a sorted table.** When a change reorders a sorted table, such as a run that changes the
    statuses or the values the table is sorted by, OpenRocket selects the same row numbers again, which then show
    other simulations; and when the tab is shown again with nothing selected, it selects the row numbers that
    were selected when the tab was left. QtRocket keeps the selection on its simulations, and selects them again
    when the tab is shown again. Only after an undo or a redo that adds or removes simulations, which replaces
    them all, does it select the same row numbers, as OpenRocket does (`simulation/SimulationTableModel.h`,
    `SimulationTableView.h`, `SimulationPanel.h`).

The simulation's settings:

29. **An edit of the launch into the wind, the standard atmosphere or the geodetic calculations.** OpenRocket
    does not count an edit of only "Always launch directly up-wind or down-wind", "Use International Standard
    Atmosphere" or "Geodetic calculations" as a change: the simulation stays "Up to date" with results that no
    longer match, and the dialog's Cancel undoes the design change made before the dialog opened instead of the
    edit. QtRocket counts these edits, so the results are out of date and Cancel undoes the edit
    (`simulation/SimulationOptions.h` in the core). Nor does QtRocket's Cancel undo a change made before the
    dialog opened, other than New simulation's addition (bug 30): OpenRocket's also undoes that earlier change
    after an edit taken back by hand, such as a rod length changed and changed back
    (`dialogs/simulation/SimulationConfigDialog.h`).
30. **Cancel of a new simulation after an edit.** For a simulation that New simulation has just added, Cancel asks
    whether to undo adding it, but after an edit in the dialog OpenRocket's Yes only undoes the edit, and the new
    simulation stays. QtRocket's Yes removes it (`dialogs/simulation/SimulationConfigDialog.h`).

The launch conditions and the wind editor:

31. **Two wind levels at one altitude.** OpenRocket's wind profile editor takes an altitude that another level
    already has. Deleting one of the two then deletes both, and a design saved with the two cannot be opened again
    ("Wind level already exists for altitude"). QtRocket's editor refuses that altitude, and a row's Delete
    removes that row's level alone (`dialogs/simulation/MultiLevelWindTableModel.h`).
32. **The order of the wind levels.** After an altitude edit, OpenRocket's wind profile editor sorts its rows but
    not the levels themselves, so the wind is interpolated between the wrong levels until the design is saved and
    opened again. QtRocket sorts the levels themselves, and the launch conditions (the rod's direction when the
    rocket is launched into a multi-level wind) follow the new order at once
    (`dialogs/simulation/MultiLevelWindTableModel.h`).
33. **"Delete level" with the mouse.** OpenRocket's wind profile editor deselects the row when the button takes the
    focus, which disables the button before the mouse button is released, so "Delete level" never deletes with
    the mouse. QtRocket's table keeps its selection when it loses the focus, so the button deletes the selected
    level; the profile keeps that level highlighted meanwhile (`dialogs/simulation/MultiLevelWindTableView.h`).
34. **Wind levels imported with the separator SPACE or TAB.** OpenRocket splits the lines at the word "SPACE" or
    "TAB" itself, so the import fails ('altitude column "altitude" not found in header row.'), and it stores the
    word as the CSV export's separator, which the export then shows as "S" or "T". QtRocket splits at the space or
    the tab, and stores that (`dialogs/simulation/CsvImportSettingsDialog.h`, `MultiLevelWindEditDialog.h`).
35. **An extremely low temperature.** For a launch temperature of -80 degrees Celsius or less, with "Use
    International Standard Atmosphere" unticked, OpenRocket reports an internal error the first time, and the
    field's message is the key "simedtdlg.error.temperatureTooLow", a text missing from its translations.
    QtRocket shows "Extremely low temperature" (`dialogs/simulation/SimulationConditionsPanel.h`).
36. **The summary of the multi-level wind.** OpenRocket's launch conditions write the summary of the wind levels
    again only when the wind profile editor closes, so "Reset to default" leaves it out of date, and the highest
    altitude it shows is never below 0 m ("Altitude range: -50 m - 0 m" for levels at -50 m and -10 m).
    QtRocket's summary follows every change of the levels, and shows "-50 m - -10 m"
    (`dialogs/simulation/WindLevelSummary.h`).
37. **Importing wind levels from a file whose name does not end in ".csv".** OpenRocket adds ".csv" to the name of
    the file chosen ("wind.txt" becomes "wind.txt.csv") and then cannot load it. QtRocket imports the file chosen
    (`dialogs/simulation/MultiLevelWindEditDialog.h`).

New simulations and runs:

38. **The Simulation page's "Geodetic calculations" and the Warnings page's three thresholds.** The Simulation
    page says that its settings apply to newly created simulations, but OpenRocket's new simulations start with
    fixed values for these four (the spherical approximation, 20, 30.48 and 15.24 m/s); only the "Reset to
    default" buttons of the Simulation options and Warnings tabs read the preferences. QtRocket's new simulations
    take them, and so does the Launch conditions tab's "Reset to default", which also resets these four:
    OpenRocket's resets them to the fixed values (`simulation/DefaultSimulationOptionFactory.h` in the core).
39. **The launch rod's direction into a multi-level wind above ground level.** With "Always launch directly
    up-wind or down-wind" and a multi-level wind whose altitudes are above ground level (AGL), OpenRocket points
    the rod into the wind at the launch site's altitude above sea level, counted from the ground: at a site 1000 m
    above sea level, the wind 1000 m up. QtRocket points it into the wind at the ground, where the rocket leaves
    the rod. A design saved so stores that direction as the rod's, and so does "Save as default" (OpenRocket stores
    the other one); the stored direction matters only once the box is unticked (`simulation/SimulationOptions.h`
    in the core).
40. **Cancel in the run dialog.** OpenRocket keeps the part of the flight simulated before the Cancel as the
    simulation's results, marked up to date, and its earlier results are lost. QtRocket keeps the earlier results
    and status (`dialogs/simulation/SimulationRunDialog.h`; `simulation/SimulationRun.h` in the core).

Plots:

41. **Events drawn together.** OpenRocket's plot draws the events within 0.05 s of the first of them as one line
    (or one icon) with one label: at the time of one of them taken at random, with their names in a random order,
    both of which change from one run of OpenRocket to the next. QtRocket draws them at the first event's time,
    with the names in time order (`plot/SimulationPlotModel.h` in the core).
42. **The colours of a plot of several stages.** OpenRocket draws every quantity of an axis, and its legend entry,
    in the colour of the axis's first quantity, while the Plot data tab shows each quantity in its own colour.
    QtRocket draws each in the colour the tab shows (`plot/SimulationPlotModel.h` in the core).
43. **The label of the x axis.** When the last event lies within 1 % of the end of the x axis, as in the time
    plots of every flight longer than 49.5 s, OpenRocket's plot loses the label of its x axis, and each stage
    chosen and each "Show events" widens the axis again. QtRocket keeps the label and widens the axis once
    (`plot/SimulationPlot.h`; `plot/SimulationPlotModel.h` in the core).
44. **The box of an aborted simulation.** OpenRocket's plot dialog hides the "Simulation Abort" box when it opens,
    though "Show errors" is ticked; the box appears once "Show errors" is unticked and ticked again, or a stage is
    chosen. QtRocket shows it from the start (`plot/SimulationPlot.h`).
45. **A right axis of infinite range.** When a plot's left axis starts or ends at exactly zero and the data of its
    right axis span zero, OpenRocket gives the right axis an infinite range. QtRocket keeps the right axis's
    range, so its automatic choice of the axes can differ from OpenRocket's. Of the plots of the examples' stored
    simulations, one differs: "Dual parachute deployment"'s first simulation with the preset "Simulation time step
    and computation time", where QtRocket puts the time step on the right (`plot/PlotConfiguration.h` in the core).
46. **A stage without a quantity of the plot.** When one stage's flight data lack a quantity that the plot shows,
    OpenRocket's plot fails with an internal error, and its automatic ranges leave that quantity out. QtRocket
    plots the quantity for the stages that have it, in the range of their data (`plot/PlotConfiguration.h` and
    `plot/SimulationPlotModel.h` in the core).
47. **Ctrl and the mouse wheel.** OpenRocket's plot dialog says "ctrl-mouse wheel to zoom x axis only" but zooms
    both axes. QtRocket zooms the x axis alone, as its own help text says ("cmd" on macOS, where Ctrl is the
    Command key) (`plot/PlotInputBindings.h`).
48. **A stage without data points.** OpenRocket matches the legend entries, the colours, the point shapes and the
    stages to the curves by counting, so a stage without points gives the curves after it the entries, colours,
    shapes and stages of others. QtRocket gives each curve those of its own quantity and stage
    (`plot/SimulationPlotModel.h` in the core).

The CSV export:

49. **Exporting a simulation that is not up to date.** The Export data tab's Export first runs the simulation
    again when it is out of date, but OpenRocket then writes the flight data the dialog opened with, under the
    description of the new run (its status, its warnings). QtRocket writes the new run's flight data
    (`dialogs/simulation/SimulationExportPanel.h`).
50. **A field separator of several characters.** OpenRocket's CSV dialogs show a stored separator by its first
    character only, so the next export writes another one: "<->" comes back as "<", and "SPACE", which bug 34
    stores, as "S". QtRocket shows the whole separator, and a stored "SPACE" or "TAB" as that item, the space or
    the tab (`widgets/FieldSeparatorComboBox.h`, `settings/CsvOptionPreferences.h`).
51. **A stored number of decimal places outside 0 to 15.** OpenRocket's simulation dialog then fails with an
    internal error for a simulation with results, and so do "Export simulation table as CSV file" and "Export
    simulation(s) as CSV file", before their file dialog opens. QtRocket's CSV options take 0 or 15, whichever
    is nearer (`settings/CsvOptionPreferences.h`).
52. **A quantity whose name is longer than 80 characters.** OpenRocket's preferences refuse a key that long, and
    the CSV export stores whether each quantity is ticked under its name, so exporting a simulation that has such
    a quantity (a custom expression's, or one a file brought) fails with an internal error after the file was
    chosen, before anything is written. QtRocket's preferences take keys of any length
    (`settings/CsvOptionPreferences.h`).
53. **An export that cannot be written.** When the Export data tab's file cannot be opened, OpenRocket shows its
    internal error dialog; when a write fails, it says nothing; and the Cancel of its progress dialog deletes the
    file while the export goes on writing it. QtRocket says "Error writing file" ("An error occurred while
    writing to the file:" and the reason) in both cases, and its export has no Cancel
    (`dialogs/simulation/SimulationExportPanel.h`).

The design view's flight estimate:

54. **Turning the estimate off and on.** After "Update estimated flight parameters in design window" (Preferences,
    Design) is unticked and ticked again, OpenRocket's design view shows no flight estimate until the design
    changes. QtRocket shows it again when the Preferences dialog refreshes the design windows
    (`figure/BackgroundSimulation.h`).
55. **A flight configuration without a simulation.** When the selected flight configuration has no simulation,
    OpenRocket's design view shows N/A, but keeps the line "Simulation Aborted: ..." of the simulation it showed
    before, one of another configuration (and, with "Run out-dated simulations when you open the simulation tab"
    while that tab is shown, that simulation's values too). QtRocket shows N/A alone
    (`figure/BackgroundSimulation.h`).

More OpenRocket bugs, found while porting, that QtRocket fixes the same way:

- **The columns of the simulations table, all shown.** OpenRocket reads a stored empty list of hidden columns as
  no choice. So a design saved with every column shown (the "Sim. Stepper" column too) opens with that column
  hidden again, or with the columns that "Save as default" hid; and "Save as default" with every column shown
  makes new designs hide the stepper column all the same. A design whose stored choice is not a text (a file
  edited by hand) makes OpenRocket fail with an internal error. QtRocket keeps the choice as it was stored, and
  ignores one that is not a text (`simulation/SimulationColumnVisibility.h`).
- **Removing the last plot of the Plot data tab.** OpenRocket fails with an internal error when the last row of
  "Y axis types" is removed. QtRocket removes it (`dialogs/plot/PlotPanel.h`; `plot/PlotConfiguration.h` in the
  core).
- **Stage names such as "A", "A(1)" and "A".** OpenRocket numbers repeated stage names for its plot dialog and
  its Export data tab, and fails with an internal error for such names. QtRocket's plot dialog numbers them
  "A(1)", "A(1)(1)" and "A(2)" (`plot/PlotUtil.h` in the core).
- **A value below about 5.6e-309 in a plot's tool tip.** OpenRocket's tool tip fails with an internal error.
  QtRocket shows the value's first digits (`util/DecimalFormatter.h` in the core).
- **"Load from file...", the reload button and "Clear" of the aerodynamic lookup tables.** Each of them shows
  both of OpenRocket's tables again, which throws away an edit of the other table's text: its "(modified)" goes,
  and OK keeps the old rows. QtRocket leaves the other table alone while it holds an edit
  (`dialogs/simulation/AerodynamicLookupDialog.h`).
- **A stored column index below zero for the wind import.** OpenRocket's "Wind Data Import Settings" dialog, and
  with it the import, fails with an internal error until the preferences are reset. QtRocket starts that index
  at 0 (`dialogs/simulation/CsvImportSettingsDialog.h`).
- **A ticked quantity that the exported stage does not have.** The Export data tab lists the quantities of the
  first stage's flight data. When the stage chosen in "Stage to export" lacks one that is ticked, OpenRocket's
  export fails with an internal error after writing the file's comments. QtRocket says "Error writing file"
  with the quantity's name and writes nothing, as it also does when the flight data of a run the Export made
  lacks one (data that only QtRocket writes: bug 49) (`dialogs/simulation/SimulationExportPanel.h`).

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
- Combo boxes, the quantity and line style boxes of the simulation's Plot data tab too, choose with the wheel
  only while they have the focus. The keys on a closed combo box follow the platform, also on the quantity box,
  whose Down opens a plain list of the quantities in OpenRocket (`widgets/EnumComboBox.h`, `DataTypeComboBox.h`).
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
- When the selected flight configuration has several simulations, the flight estimate is always the first
  one's. OpenRocket's can be another of them, the last whose results changed (`figure/BackgroundSimulation.h`).
- While the window's simulation settings dialog or run dialog is open, the flight estimate waits: it starts no
  run and takes in no result until the dialog closes, and a run it had started is dropped and starts again
  then. In OpenRocket the estimate can change the simulation the settings dialog edits, and so what its Cancel
  undoes (`figure/BackgroundSimulation.h`).
- The simulations a run dialog runs leave the flight estimate: it does not run them again, and the run dialog's
  results are the ones kept. In OpenRocket the estimate and the run dialog can run the same simulation at once,
  and the run that ends last gives the results, which can be the estimate's flight, cut off where it hits the
  ground (`figure/BackgroundSimulation.h`).

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

The thrust curve preview of the motor chooser, its "Motor thrust curves" dialog and the simulation plot dialog
are drawn by [rocketplot](https://github.com/cthunter01/rocketplot), not JFreeChart, and so is the wind editor's
profile, which OpenRocket draws itself (the user's decisions U-M3-6a and b, and U-M4-6):

- They follow the platform's light or dark scheme, with rocketplot's fonts and grid lines. OpenRocket's plot
  dialogs and its wind profile are always light; its motor chooser's preview follows its theme
  (`plot/ThrustCurvePlot.h`, `DataPlot.h`, `WindProfilePlot.h`).
- The motor chooser's preview and the wind profile take no zoom or pan gesture and have no menu or crosshair,
  as in OpenRocket (the profile's dots have tool tips in both) (`plot/PlotInputBindings.h`).
- The mouse in the two plot dialogs: a left drag zooms to the box dragged out, Shift and a left drag pans, a
  double click resets the view, the wheel zooms about the pointer (over an axis that axis only; Ctrl and the
  wheel the time only, Shift and the wheel the thrust only), a trackpad scrolls and, with Ctrl, zooms, a pinch
  zooms, and the back and forward mouse buttons go through the views. JFreeChart restores the view after a drag
  up and to the left and zooms with the wheel only over the data (`plot/PlotInputBindings.h`).
- In the two plot dialogs, the right button opens rocketplot's menu (Back, Forward, Reset view, the crosshair,
  Copy image, Export...) in place of JFreeChart's. Export... writes PNG, JPEG, SVG, PDF or CSV files, adds the
  suffix of the chosen kind when the name has none of them, asks before replacing a file and says when the file
  cannot be written (`plot/PlotExport.h`).
- A legend entry of the two plot dialogs has a menu (the line width and markers, Visible, Show only this, Show
  all, Remove, Theme color), which OpenRocket's legends do not have. A curve recoloured there keeps its colour
  when the scheme changes.
- In the simulation plot dialog, a click on a legend entry hides or shows the curves of its quantity, a double
  click shows only them, pointing at it brings them forward, and it has the menu above. While a stage is chosen,
  all of these act on that stage's curves only. OpenRocket's legend only highlights the curves, drawn wider,
  while the mouse is over an entry or after a click on it (`plot/SimulationPlot.h`, `DataPlot.h`). The event
  icons of a curve hidden or removed there stay on the plot.
- The simulation plot dialog's mouse is the one above, and Ctrl with a left drag pans too, as in OpenRocket;
  Ctrl and the wheel zoom the x axis only, Shift and the wheel the y axes only. OpenRocket's right drag, a zoom
  that follows the pointer, is not there: the right button opens the menu. The view can be panned and zoomed
  beyond the ranges OpenRocket's axes keep to, so the zoom-out button acts from the start, where OpenRocket's
  does nothing until the plot was zoomed in; the zoom reset button and a double click come back to them. The
  help text below the plot is QtRocket's own, as the gestures differ ("cmd" for "ctrl" on macOS), and wraps at
  the dialog's width, where OpenRocket's one line sets the dialog's least width (`plot/PlotInputBindings.h`,
  `dialogs/plot/PlotDialog.h`).
- The readout is on from the start: the crosshair snaps to the data point nearest the pointer, and a tool tip
  shows OpenRocket's text for that point. OpenRocket shows the tool tip while the pointer is over a point
  (`plot/DataPlot.h`).
- The labels of the flight events are flags, boxes in a band above the plot area, each with its event's icon.
  OpenRocket writes them upright beside the event lines, inside the plot area, without icons. With a dark
  scheme the events' colours are lighter. The "Simulation Abort" box has rocketplot's font, frame and
  background, with its text in the error colour; OpenRocket's is bold on white, framed in the error colour. An
  event icon is placed against the axis of its own curve; OpenRocket maps it onto the left axis, which puts it
  in the same place until the axes are zoomed apart (`plot/SimulationPlot.h`, `PlotEventGraphics.h`).
- The data points of each stage have a shape of their own: rocketplot has 6, and a seventh stage takes the
  first again. JFreeChart has 10 (`plot/DataPlot.h`).
- A colour of the Plot data tab is chosen in Qt's colour dialog (with an alpha channel, as OpenRocket's), made
  anew at each click, and reaches the button when the dialog closes with OK. OpenRocket's chooser shows each
  colour picked in the button at once, puts the first back on Cancel, and keeps its recent colours from one
  click to the next (`widgets/ColorChooserButton.h`).
- "Export Image" says "Error writing file" when the PNG cannot be written. OpenRocket's says nothing
  (`dialogs/plot/PlotDialog.h`).
- The plot dialog belongs to the simulation's settings dialog, which cannot be closed while the plot is open;
  OpenRocket's belongs to the design window. In both, the plot keeps the settings dialog and the design window
  waiting (`dialogs/plot/SimulationPlotDialog.h`).

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

## The "Flight simulations" tab

- The table shows a change of the default units when the Preferences dialog refreshes the design windows, and a
  change of the motor chooser's name column (designations or common names, which the Configuration column shows
  too) when the chooser closes. OpenRocket's table shows both at its next repaint, a unit changed on the Units
  page even before Apply (`simulation/SimulationTableModel.h`, `main/MainWindow.h`).
- A press of the right button on a row selects it, and the menu opens. In OpenRocket a right click while no row
  is selected does nothing. The keyboard's menu key opens the menu too, at the current row, while a row is
  selected (`simulation/SimulationTableView.h`).
- New, Duplicate, Paste and Plot / Export scroll the table to the simulations they select. OpenRocket's table
  stays where it is (`simulation/SimulationTableView.h`).
- A third quick click in the table counts as a new click. OpenRocket ignores it
  (`simulation/SimulationTableView.h`).
- The tab's buttons have no mnemonic, as the design tab's have none: OpenRocket's New simulation, Edit
  simulation and Delete simulations answer Alt+N, Alt+E (the Edit menu's key too) and Alt+Delete. The popup
  menu's Edit, Copy and Duplicate have theirs (`simulation/SimulationPanel.h`).
- The tab's actions are enabled or disabled as soon as what they depend on changes. OpenRocket updates them
  when the selection changes, the motors come or go, or the Edit menu is updated, so that its "Export
  simulation table as CSV file" can stay disabled for a while after the first simulation came
  (`simulation/SimulationPanel.h`).
- When the run that Plot / Export starts leaves the simulation without results (a Cancel keeps the results it
  had: bug 40), the settings dialog opens on its Launch conditions tab. OpenRocket's opens on Plot data, with
  the part of the flight run before the Cancel (`simulation/SimulationPanel.h`).
- Paste gives the simulations the plot colours and line styles they had at Copy. OpenRocket's gives them those
  the copied simulations have at the paste (`document/SimulationCopy.h` in the core).
- "Export simulation table as CSV file" (in the File menu) and the table menu's "Export simulation(s) as CSV
  file" ask for the CSV options after the file is chosen, in a dialog of their own, "Save as CSV file", with
  the format settings and the comment character; its Cancel ends the export (the user's decision U-M4-6).
  OpenRocket shows the options beside the files in its file dialog (`dialogs/simulation/CsvOptionsDialog.h`,
  `simulation/SimulationPanel.h`).
- When the table's CSV file cannot be written, QtRocket says "Error writing file" ("An error occurred while
  writing to the file:" and the reason). OpenRocket shows a message titled "Message" with the error's text alone
  (`simulation/SimulationPanel.h`).
- The Undo item after New simulation, when the last settings dialog edited a simulation that an undo or a redo
  had replaced. An undo or a redo that adds or removes simulations replaces them all with copies whose launch
  conditions and options no longer tell the design of a change, in both programs, so the Undo item does not
  show the edits of such a copy and keeps the text it had. OpenRocket's new simulation tells the design of its
  name before it is added, and that names the undo step of those edits after their settings dialog. So once the
  new simulation is cancelled or undone, OpenRocket's Undo item names that dialog ("Undo (Edit Simulation 2)"),
  where QtRocket's reads "Undo": its design hears a simulation only once it is in the list
  (`simulation/Simulation.h` in the core).

## The simulation settings dialog

- Number fields that OpenRocket leaves without a maximum stop at the largest value a run accepts (the user's
  decision U-M4-7): the wind speed at 1000 m/s, the temperature at 1000 K, the pressure at 1000000 Pa, the
  relative humidity at 100 %, the launch rod's length at 1000 m, the constant gravity at 100 m/s^2, the
  maximum simulation time at 1000000 s, the recovery warnings' thresholds at 10000 m/s, and in the wind
  editor's table an altitude at 1000000 m (above which "Add new level" adds none) and a speed at 1000 m/s.
  So 150 % typed as the humidity gives 100 %, where OpenRocket takes 150 %. The Simulation, Warnings and Launch
  pages of the Preferences have the same maxima (`dialogs/simulation/SimulationConditionsPanel.h`,
  `SimulationOptionsPanel.h`, `RecoveryWarningsPanel.h`, `MultiLevelWindTableModel.h`;
  `simulation/SimulationInputRanges.h` in the core).
- A field whose value is doubtful or wrong (the wind speed, the temperature, the pressure and the rod angle of
  the launch conditions, the random seed, the columns of the wind import) gets an outline in the warning or
  error colour, drawn over the platform's frame; OpenRocket colours the field's own border. The message that
  shows for a while below the field stays inside the window, above the field when there is no room below,
  where OpenRocket's can reach beyond the window (`widgets/FieldValidator.h`).
- After "Reset to default" on the Simulation options tab, its "Geodetic calculations" shows the method
  restored. OpenRocket's box keeps showing the one chosen before, and choosing that one then changes nothing.
  The random seed follows the Launch conditions tab's "Reset to default", which resets the seed too:
  OpenRocket's field keeps the seed it showed, and its OK puts that seed back
  (`dialogs/simulation/SimulationOptionsPanel.h`, `RandomSeedControls.h`).
- The tool tip of the aerodynamic lookup's summary icon comes after the platform's usual delay. OpenRocket's
  comes at once (`dialogs/simulation/SimulationOptionsPanel.h`).
- The wind editor's levels are a table: a cell opens a field when it is clicked, double-clicked or typed into,
  or with F2; Enter, the arrow keys or a click elsewhere take the value, and Escape drops it. The unit
  selectors sit in the column headers, below the names. OpenRocket shows a row of fields for each level, with
  the unit selectors in a header above them. The Delete key deletes the selected level too, and the selection
  stays when the popup menu closes (OpenRocket's popup clears it). The Delete column is enabled whenever there
  is more than one level; OpenRocket's stays enabled for a lone level after "Reset levels" or an import, and
  does nothing (`dialogs/simulation/MultiLevelWindTableView.h`, `MultiLevelWindTableModel.h`).
- A row the wind editor changes flashes as in OpenRocket, but the selected row keeps its highlight afterwards.
  OpenRocket's flash ends in the row's plain colour, on the selected row too.
- The wind editor's profile has rocketplot's ticks, labels and grid. OpenRocket's has one labelled tick at the
  highest speed and altitude, and arrow heads at the ends of its axes. Levels below sea level lie within the
  profile's axes, where OpenRocket draws them below its altitude axis, or the profile upside down when every
  level is below sea level. An axis that would have no range (no wind, or every level at 0 m) goes from 0 to 1,
  where OpenRocket draws the dots on the axis (`plot/WindProfilePlot.h`).
- In the wind import's settings, the preview and the outlines of repeated columns follow each key typed in the
  column fields and the separator. OpenRocket's follow once Enter is pressed or the field is left. An empty
  separator fails before the file is read, with "The field separator is empty."; OpenRocket's fails with
  'altitude column "altitude" not found in header row.' (`dialogs/simulation/CsvImportSettingsDialog.h`).
- The file dialogs of the wind import and of the lookup tables' "Load from file..." list their CSV filter
  first, then All Files. OpenRocket's list All Files first.
- The aerodynamic lookup dialog closes with Escape, as Cancel; OpenRocket's has no key for it. Its text areas
  grow with the dialog. Its summary lists the stability table's columns in the table's order, "CN, CM, CP",
  where OpenRocket's order changes from one start to the next. A field separator that is not an ASCII
  character is refused: "The field separator must be an ASCII character."
  (`dialogs/simulation/AerodynamicLookupDialog.h`).
- Escape and the window's close button act as Cancel: on the first three tabs, the question whether to discard
  the changes comes while the dialog is still open, and No keeps it open. OpenRocket closes the dialog first and
  asks afterwards, so its No keeps the edits of a dialog already closed, and in a multi-simulation edit they are
  not copied to the other simulations (`dialogs/simulation/SimulationConfigDialog.h`).
- The run dialog that OK, Plot or Export starts belongs to the settings dialog, as the plot dialog does (see
  Plots), and stays above it. OpenRocket's belongs to the design window
  (`dialogs/simulation/SimulationConfigDialog.h`).
- When the run that Plot or Export starts leaves the simulation without results (a run refused before it
  starts), nothing is plotted or exported, and the dialog stays open. OpenRocket's plot dialog fails on the
  missing data, and its export writes the old results (`dialogs/simulation/SimulationConfigDialog.h`,
  `SimulationExportPanel.h`).
- The Export data tab's export writes the file at once, under a wait cursor, with no progress dialog and no
  Cancel. OpenRocket's shows "Exporting flight data" with a Cancel when an export takes two seconds or more
  (`dialogs/simulation/SimulationExportPanel.h`).
- The dialog's size is kept within its own screen, less 80 pixels; OpenRocket keeps it within the default
  screen. It cannot be made smaller than its top rows and buttons need (about 360 by 110 pixels), where
  OpenRocket's can (`dialogs/simulation/SimulationConfigDialog.h`).

## Running simulations

- The run dialog's Cancel, Escape and close button close the dialog once every run has stopped, at the end of
  the step it is taking. OpenRocket's dialog closes at once, while its runs go on to their next step
  (`dialogs/simulation/SimulationRunDialog.h`).
- The errors of several failed runs are shown one at a time. OpenRocket's messages can open over each other
  (`util/SimulationBatch.h`).
- A run that QtRocket refuses or stops fails with an ordinary error over the run dialog ("An error occurred
  during the simulation:" and the reason), where OpenRocket shows its internal error dialog. The cases: launch
  air of no positive density, with "Use International Standard Atmosphere" unticked (288.15 K at 50 % and 300 Pa,
  or 400 K at 100 % and sea level pressure: OpenRocket's flight ends in a NaN); launch conditions that the
  atmosphere model refuses (50 K at a site 10,000 m high: "Temperature must be positive (Kelvin)" in both); a
  value outside the range a run takes, refused before the run starts (a launch temperature below 1 K, which the
  field takes; OpenRocket fails at 0 K); and a flight whose numbers become NaN. A flight that needs more than
  1,000,000 steps in one stage fails too ("Simulation needs more than 1000000 steps for one flight data
  branch. ..."), where OpenRocket has no limit (`util/SimulationBatch.h`; `simulation/Simulation.h` and
  `SimulationInputRanges.h` in the core).
- While a simulation runs, the "Flight simulations" tab keeps showing its earlier results, until the run ends
  (the user's decision U-M4-6). OpenRocket drops them when the run starts (`simulation/SimulationRun.h` in the
  core).

## Preferences

- The preferences are kept in an INI file of QtRocket's own (`~/.config/QtRocket/QtRocket.ini` on Linux and
  macOS, `%APPDATA%\QtRocket\QtRocket.ini` on Windows), with window positions and maximized states in its own
  form. OpenRocket's preferences are not read or imported.
- The dialog has the General, Design, Simulation, Warnings, Launch and Units pages. The UI and Materials pages
  come with Milestone 5, the Graphics page (the decal editor and the 3D view) with Milestones 5 and 6. General
  has the user's thrust curves and component presets, "Open last design file on startup", the confirmation for
  discarding preferences, "Automatically open the Parts Library dialog" and "Reset all preferences". Left out:
  the language selector, the update checks, the import and export of preferences, and the RASAero and RockSim
  warnings (RockSim with Milestone 8).
- "Add" beside the user's thrust curves or component presets first asks in a menu, "File..." or "Directory...",
  then shows the platform's dialog for a file or for a directory (the user's decision U-M3-6c). OpenRocket's
  dialog shows files and directories at once. The filters are OpenRocket's, followed by their patterns where
  OpenRocket's text holds more than patterns (`dialogs/GeneralPreferencesPanel.h`).
- While a list is unset it shows QtRocket's own directory (see Start-up), which "Reset" puts back. The page
  makes no directory: the start does. A change takes effect at the next start, as the page says.
- A list of the user's files longer than 8192 characters is stored. OpenRocket refuses it with an error.
- Apply, OK and Cancel refresh every design window. OpenRocket refreshes only the window the dialog came from.
- "Reset all preferences" keeps the dialog open with new pages.
- After "Reset all preferences", the Launch page shows the default wind. OpenRocket's shows the wind from
  before the reset until it is started again (`simulation/LaunchPreferences.h` in the core).
- After "Reset to default" on the Simulation page, the tool tips of its combo boxes and the "Gravity value" row
  follow the values restored (the row hidden for WGS). OpenRocket's keep what they showed before the reset, so
  that its "Gravity value" stays shown beside WGS (`dialogs/SimulationPreferencesPanel.h`).

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

- Milestone 5: the Appearance tab and decal editing, the scale dialog, the material editor ("Add custom
  material", "Edit materials"), "Components of same color", "Scale Fin", "Save decal images", calipers, view
  capture, the preview image in saved files, the theme selector, Help > About and License.
- Milestone 6: the 3D views.
- Milestone 7: component analysis, optimization, custom expressions and the Tools menu; Monte Carlo analysis
  (the Monte Carlo button and the "Monte Carlo analysis..." item of the "Flight simulations" tab); adding,
  copying and configuring a simulation's extensions, which the Simulation options tab lists with their Help and
  Delete only.
- Milestone 8: RockSim import and export, SVG export, printing.
- Milestone 9: packaged releases of the GUI, the splash screen, Help > Bug report.

Not planned for QtRocket: RASAero export, Wavefront OBJ export, Photo Studio, guided tours, translations
(the language selector), the debug log, update checks, the motor search, download and database update of
thrustcurve.org, the component preset editor, the "banana for scale", and the scripting and Java code
extensions: a design's scripts are kept and saved but never run, and one that is enabled and holds code is
disabled, with a warning, when the design is opened (`simulation/extension/impl/ScriptingExtension.h` in the
core); a Java listener's class is never loaded, so a simulation whose Java code extension names a class (every
listener of an old design becomes one) fails with "Could not find class ..."
(`simulation/extension/impl/JavaCode.h` in the core).
