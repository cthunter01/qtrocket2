# Differences from OpenRocket's GUI

QtRocket's GUI follows OpenRocket's Swing GUI (its development version at commit 5f164fd0e): its windows, menus,
dialogs, texts and rules. This page lists every difference a user can notice. Each one is also documented in the
source where it is made; the header named in parentheses says more (under `gui/include/QtRocket/gui/`, or
`include/QtRocket/` for the core). What OpenRocket has and QtRocket does not have yet is at the end.

## OpenRocket bugs that QtRocket fixes

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
  after resetting the preferences) name QtRocket.

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

## Preferences

- The preferences are kept in an INI file of QtRocket's own (`~/.config/QtRocket/QtRocket.ini` on Linux and
  macOS, `%APPDATA%\QtRocket\QtRocket.ini` on Windows), with window positions and maximized states in its own
  form. OpenRocket's preferences are not read or imported.
- The dialog has the General, Design and Units pages. General has "Open last design file on startup", the
  confirmation for discarding preferences and "Reset all preferences". Design has no simulation options
  (Milestone 4). Left out: the language selector, the update checks, the import and export of preferences, and
  the options of later milestones (the thrust-curve and preset paths, the parts library, the RockSim warning).
- Apply, OK and Cancel refresh every design window. OpenRocket refreshes only the window the dialog came from.
- "Reset all preferences" keeps the dialog open with new pages.

## Start-up and errors

- There is no splash screen, welcome dialog or update check.
- The motor database loads at start. If it cannot be read, a warning says so after the first window opens.
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

- Milestone 3: motors and the "Motors & Configuration" tab, the motor chooser, component presets and the parts
  library, the flight configuration dialogs.
- Milestone 4: simulations, plots and CSV export in the GUI; the flight block of the design view's overlay.
- Milestone 5: the Appearance tab and decal editing, the scale dialog, the material editor ("Add custom
  material", "Edit materials"), "Components of same color", "Scale Fin", "Save decal images", calipers, view
  capture, the preview image in saved files, the theme selector, Help > About and License.
- Milestone 6: the 3D views.
- Milestone 7: component analysis, optimization, custom expressions and the Tools menu.
- Milestone 8: RockSim import and export, SVG export, printing.
- Milestone 9: packaged releases of the GUI, the splash screen, Help > Bug report.

Not planned for QtRocket: RASAero export, Wavefront OBJ export, Photo Studio, guided tours, translations
(the language selector), the debug log, update checks and the "banana for scale".
