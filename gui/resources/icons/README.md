# Icons

The icons of the GUI, compiled into `QtRocket_gui` by `gui/resources/CMakeLists.txt` (each file at
`:/icons/<directory>/<file>`). All of them are copied unchanged from OpenRocket's
`swing/src/main/resources/pix/` (commit 5f164fd0e), except that every file name is in lower case (Qt's resource
paths are case-sensitive, and OpenRocket's `Icons.java` asks for `cd-override.svg` where the file is
`CD-override.svg`).

| Directory | Files | From OpenRocket | Licence |
| --- | --- | --- | --- |
| `actions/` | 48 SVG icons of the menus, the tree, the figure, the flight configuration tab and the simulations (`util/Icons`) | `pix/icons/lucide/` | Lucide (ISC; the Feather-derived icons also MIT) or OpenRocket's own edits and drawings of them (GPL-3.0-or-later) |
| `components/` | 71 PNG component icons, small and large, with the `_dark` variants (`util/ComponentIcons`) | `pix/componenticons/` | OpenRocket (GPL-3.0-or-later) |
| `app/` | 4 PNG window icons, 16 to 256 pixels (GUIUtil's) | `pix/icon/icon-016.png`, `-048`, `-064`, `-256` | OpenRocket (GPL-3.0-or-later) |
| `status/` | 6 PNG icons, 16 pixels: the simulations' status icons and the plots' images of a warning (`util/SimulationIcons`) | `pix/icons/` | OpenRocket (GPL-3.0-or-later) |
| `events/` | 13 PNG images of the flight events in the plots, 16 pixels (`util/SimulationIcons`) | `pix/eventicons/` | OpenRocket (GPL-3.0-or-later), except `event-simulation-end.png`: famfamfam Silk (CC BY 2.5) |

`actions/`: 17 of the 48 SVGs are plain exports of [Lucide](https://lucide.dev) (`class="lucide ..."`); the others
are OpenRocket's edits of Lucide icons (more colours, other strokes) or its own drawings in Lucide's style
(`cd-override.svg`, `cg-override.svg`, `magnet-click.svg`, `rename.svg`, `save-as.svg`, `zoom-fit.svg`, ...).
Lucide is under the ISC License, Copyright (c) 2026 Lucide Icons and Contributors; the icons derived from Feather
are also under the MIT License, Copyright (c) 2013-present Cole Bemis. Both texts are in `NOTICE` at the top of the
repository. OpenRocket's icons and its edits are part of OpenRocket, Copyright (C) 2007-2025 Sampo Niskanen and
others, GPL-3.0-or-later, like QtRocket.

The SVGs draw in placeholder colours (black `#000`/`#000000`/`black`/`currentColor`, OpenRocket's green `#499c54`,
blue `#2d2dbd`, red `#c80000`, yellow `#fac132`, ...), which `util/SvgIconEngine` replaces at paint time with the
colours of the palette and of `util/ThemeColors`, as OpenRocket's `Icons.java` maps them to its theme keys. The PNGs
of `status/` and `events/` are drawn as they are, in both themes, as OpenRocket draws them.

`events/`: OpenRocket's `pix/eventicons/copyright.txt` says that `event-simulation-end.png` is from the
[famfamfam Silk](http://www.famfamfam.com/lab/icons/silk/) icon set by Mark James, under the Creative Commons
Attribution 2.5 License (`NOTICE` names it), and that all the others are drawn by Sampo Niskanen. OpenRocket's
`pix/icons/copyright.txt` does not list the files of `status/`; they come with OpenRocket's sources, GPL-3.0-or-later.

Which icons are here: those the Milestone 2 GUI shows (the File and Edit menus, the tree's markers, the figure's
ribbon, the configuration dialog's info button), Milestone 3's rename icon (the flight configuration tab's Rename
button, `EDIT_RENAME`) and Milestone 4's of the simulations, each as `Icons::Id` in `util/Icons.h` or through
`util/SimulationIcons.h`. OpenRocket's other icons come with the milestones that use them.

The files of Milestone 4:

| File | Shown as | From OpenRocket |
| --- | --- | --- |
| `actions/play-filled.svg` | `SIM_RUN`, in `OR.icons.play` | `pix/icons/lucide/play-filled.svg` |
| `actions/chart-spline.svg` | `SIM_PLOT`, its `#006cb6` in `OR.icons.plot` | `pix/icons/lucide/chart-spline.svg` |
| `actions/sheet.svg` | `SIM_TABLE_EXPORT` (Lucide's) | `pix/icons/lucide/sheet.svg` |
| `actions/import.svg` | `FILE_IMPORT`, its `#c80000` in `OR.icons.import` | `pix/icons/lucide/import.svg` |
| `actions/refresh-ccw.svg` | `REFRESH` (Lucide's) | `pix/icons/lucide/refresh-ccw.svg` |
| `actions/bookmark-check.svg` | `SET_AS_DEFAULT`, its `rgb(45,45,189)` in `OR.icons.tintColor` | `pix/icons/lucide/bookmark-check.svg` |
| `actions/info-filled.svg` | `WARNING_LOW`, in `OR.icons.warning.low` | `pix/icons/lucide/info-filled.svg` |
| `actions/triangle-alert-filled.svg` | `WARNING_NORMAL`, in `OR.icons.warning.normal` | `pix/icons/lucide/triangle-alert-filled.svg` |
| `actions/circle-alert-filled.svg` | `WARNING_HIGH`, in `OR.icons.warning.high` | `pix/icons/lucide/circle-alert-filled.svg` |
| `status/tick.png` | the status UPTODATE, LOADED and EXTERNAL | `pix/icons/tick.png` |
| `status/refresh.png` | the status NOT_SIMULATED and OUTDATED | `pix/icons/refresh.png` |
| `status/sim_cantrun.png` | the status CANT_RUN | `pix/icons/sim_cantrun.png` |
| `status/warning_low.png` | a SIM_WARN event of a LOW warning | `pix/icons/warning_low.png` |
| `status/warning_normal.png` | a SIM_WARN event of a NORMAL warning | `pix/icons/warning_normal.png` |
| `status/warning_high.png` | a SIM_WARN event of a HIGH warning | `pix/icons/warning_high.png` |
| `events/event-launch.png` | LAUNCH | `pix/eventicons/event-launch.png` |
| `events/event-liftoff.png` | LIFTOFF | `pix/eventicons/event-liftoff.png` |
| `events/event-launchrod.png` | LAUNCHROD | `pix/eventicons/event-launchrod.png` |
| `events/event-ignition.png` | IGNITION | `pix/eventicons/event-ignition.png` |
| `events/event-burnout.png` | BURNOUT | `pix/eventicons/event-burnout.png` |
| `events/event-ejection-charge.png` | EJECTION_CHARGE | `pix/eventicons/event-ejection-charge.png` |
| `events/event-stage-separation.png` | STAGE_SEPARATION | `pix/eventicons/event-stage-separation.png` |
| `events/event-apogee.png` | APOGEE | `pix/eventicons/event-apogee.png` |
| `events/event-recovery-device-deployment.png` | RECOVERY_DEVICE_DEPLOYMENT | `pix/eventicons/event-recovery-device-deployment.png` |
| `events/event-ground-hit.png` | GROUND_HIT | `pix/eventicons/event-ground-hit.png` |
| `events/event-simulation-end.png` | SIMULATION_END (famfamfam Silk, CC BY 2.5) | `pix/eventicons/event-simulation-end.png` |
| `events/event-tumble.png` | TUMBLE | `pix/eventicons/event-tumble.png` |
| `events/event-exception.png` | EXCEPTION and SIM_ABORT, and the status ABORTED | `pix/eventicons/event-exception.png` |
