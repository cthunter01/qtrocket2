# Icons

The icons of the GUI, compiled into `QtRocket_gui` by `gui/resources/CMakeLists.txt` (each file at
`:/icons/<directory>/<file>`). All of them are copied unchanged from OpenRocket's
`swing/src/main/resources/pix/` (commit 5f164fd0e), except that every file name is in lower case (Qt's resource
paths are case-sensitive, and OpenRocket's `Icons.java` asks for `cd-override.svg` where the file is
`CD-override.svg`).

| Directory | Files | From OpenRocket | Licence |
| --- | --- | --- | --- |
| `actions/` | 38 SVG icons of the menus, the tree and the figure (`util/Icons`) | `pix/icons/lucide/` | Lucide (ISC; the Feather-derived icons also MIT) or OpenRocket's own edits and drawings of them (GPL-3.0-or-later) |
| `components/` | 71 PNG component icons, small and large, with the `_dark` variants (`util/ComponentIcons`) | `pix/componenticons/` | OpenRocket (GPL-3.0-or-later) |
| `app/` | 4 PNG window icons, 16 to 256 pixels (GUIUtil's) | `pix/icon/icon-016.png`, `-048`, `-064`, `-256` | OpenRocket (GPL-3.0-or-later) |

`actions/`: 15 of the 38 SVGs are plain exports of [Lucide](https://lucide.dev) (`class="lucide ..."`); the others
are OpenRocket's edits of Lucide icons (more colours, other strokes) or its own drawings in Lucide's style
(`cd-override.svg`, `cg-override.svg`, `magnet-click.svg`, `save-as.svg`, `zoom-fit.svg`, ...).
Lucide is under the ISC License, Copyright (c) 2026 Lucide Icons and Contributors; the icons derived from Feather
are also under the MIT License, Copyright (c) 2013-present Cole Bemis. Both texts are in `NOTICE` at the top of the
repository. OpenRocket's icons and its edits are part of OpenRocket, Copyright (C) 2007-2025 Sampo Niskanen and
others, GPL-3.0-or-later, like QtRocket.

The SVGs draw in placeholder colours (black `#000`/`#000000`/`black`/`currentColor`, OpenRocket's green `#499c54`,
blue `#2d2dbd`, red `#c80000`, yellow `#fac132`, ...), which `util/SvgIconEngine` replaces at paint time with the
colours of the palette and of `util/ThemeColors`, as OpenRocket's `Icons.java` maps them to its theme keys.

Which icons are here: those the Milestone 2 GUI shows (the File and Edit menus, the tree's markers, the figure's
ribbon, the configuration dialog's info button), each as `Icons::Id` in `util/Icons.h`. OpenRocket's other icons
come with the milestones that use them.
