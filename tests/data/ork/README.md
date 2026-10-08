# Legacy .ork test files

Designs that OpenRocket 0.9.3 to 23.09 wrote, for the tests of the `.ork` loader. The sixteen example
designs of `data/examples/` are all zip archives of the file format versions 1.10 and 1.11; these
files reach what they never do: plain XML and gzip containers, the versions 1.0 to 1.9, components
without an `<id>` (every file here but `v1.9-chute-release.ork`), motors without a digest or
without a `<type>`, motors that no database has, `<listener>` elements, extension ids of the
`net.sf.openrocket` era, flight data stored as summaries only, transitions, legacy appearance
elements.

Every file is a byte-for-byte copy of a blob of OpenRocket's git repository
(`git cat-file blob <blob id>`); nothing was edited. `.gitattributes` turns line-end conversion
off for this directory (`tests/data/ork/** -text`): three of the files are plain XML and must
keep their bytes. `tests/core/file/openrocket/legacy_ork_files_tests.cpp` checks each file against
this table (size, SHA-256, container, version and creator of the root element, the entries of a
zip archive) and that the directory holds no other design. A file that is added or replaced needs
its row here and its entry there.

The files are OpenRocket example and test designs, Copyright (C) 2007-2025 Sampo Niskanen and
others, licensed under the GNU General Public License version 3 or later (see `NOTICE`).

## The files

| File | Version | Container | Size (bytes) | SHA-256 |
|---|---|---|---|---|
| `simplerocket.ork` | 1.2 | gzip | 2177 | `cd69389d569a69ec5a00e1a44c910fff90a8815dd8300a63d83c54368db70133` |
| `v1.0-roll-stabilized.ork` | 1.0 | plain XML | 63794 | `b919470a5dd702dda1e282ea99b9e464afde9b914d05c8ba44c91fd75b5412bc` |
| `v1.4-roll-stabilized.ork` | 1.4 | plain XML | 69469 | `c10171709ee4a24892ac53e98f47780fc410c790c9f92ca4a66ad5582841b504` |
| `v1.5-preset-usage.ork` | 1.5 | gzip | 2074 | `510539d7ede719fe8aa29da8d1d9f9b60f8259a2947ecee27122a9dce8cc4612` |
| `v1.6-a-simple-model-rocket.ork` | 1.6 | zip | 3494 | `1730385211fd4bd30b3e11bec0a13a31761c382f938fc00074e35c8460d501e7` |
| `v1.6-apocd.ork` | 1.6 | plain XML | 30942 | `7df43dcdbfa02163091a19f8160ce3f7acf2d5a1442f9afeed9640835d08dbbd` |
| `v1.6-boosted-dart.ork` | 1.6 | zip | 13052 | `540376adb5cdae6c44c26538e8529379f5af3ac30dc0aae992e7221cf740eaf6` |
| `v1.6-high-power-airstart.ork` | 1.6 | zip | 3682 | `f788dae84beba6038bd97273be31b2a4414f226cc2aa5843013ef4b17be6965f` |
| `v1.6-preset-usage-decals-first.ork` | 1.6 | zip | 5228 | `03bcfbbb050d39fab7f3bcfe1cee06d0ecdc08a4cfbac0a31d8cec0c0e281b4f` |
| `v1.6-simulation-listeners.ork` | 1.6 | zip | 18811 | `0fa06afa1bf649f71f4189928b4d3b6793722b8ce16f6e8056f86ea0c8316a9b` |
| `v1.6-tarc-payloader.ork` | 1.6 | zip | 4316 | `218dd51b7436c83b1404393a385f35a865cc3aa4fbe408ba075b8693073ec339` |
| `v1.6-three-stage-rocket.ork` | 1.6 | zip | 3541 | `2a3d965c8d92d663a028db124c49406088b6784e5c7fdb8d80e363b54cf0f1c0` |
| `v1.7-simulation-extensions-and-scripting.ork` | 1.7 | zip | 19459 | `b2d05a79569b60418e64a3653fffeb035e2c405e4d2df42597d7ed5e7eae9c36` |
| `v1.7-tube-fin.ork` | 1.7 | zip | 34520 | `f0d4caade4c958df622064d8343e62e835aa3357fe6bb8b520c024ee93a07295` |
| `v1.8-logo-rocket.ork` | 1.8 | zip | 1628 | `532d5899247a025336ffb4d1877ef57fceeee610255ceedaf324255324b93266` |
| `v1.8-parallel-staging-example.ork` | 1.8 | zip | 2525 | `a13a92808020852b3763a82dbd0c3d0d16461d978cdf9ea7ae0a89551e3d9c27` |
| `v1.8-pods-example.ork` | 1.8 | zip | 2482 | `95b1abaa386150e53a934816dc5bfaeb0781782045859774f1fe2d1d7162cfd2` |
| `v1.9-chute-release.ork` | 1.9 | zip | 82364 | `0a31a72cbd42261e42aaf02730418c65f61c3294b8b908d81e859640743080ea` |

The version is the `version` attribute of the root element `<openrocket>`. The versions 1.1 and
1.3 are not here: the files of 1.1 in OpenRocket's history are examples of 230 kB and more, and
none of 1.3 was found in the sixteen release tags that were searched. The versions 1.10 and 1.11
are those of `data/examples/`.

## Where they come from

The tag is the first release tag of OpenRocket's repository that holds the blob (the earliest by
the date of the tag, of all its 46 tags), the path is the one in that tag's tree, and the creator
is the `creator` attribute of the root element.

| File | Tag | Path in the tag | Blob id | Creator |
|---|---|---|---|---|
| `simplerocket.ork` | `release-1.1.3` | `test/net/sf/openrocket/simplerocket.ork` | `453cdec9a03e132a5609755c1a233304bfb853f2` | OpenRocket 1.1.3pre |
| `v1.0-roll-stabilized.ork` | `release-0.9.3` | `datafiles/examples/Roll-stabilized rocket.ork` | `6661d0a3335868629c80b35895c1c4cce3edf17c` | OpenRocket 0.9.3 |
| `v1.4-roll-stabilized.ork` | `release-12.03` | `core/resources/datafiles/examples/Roll-stabilized rocket.ork` | `fe1799721b4d8c591efdc881f0cc163529281968` | OpenRocket 12.03 |
| `v1.5-preset-usage.ork` | `release-12.09` | `core/resources/datafiles/examples/Preset Usage.ork` | `9e78075fc3f0be5ff4ccd4a0bf679790918b6dd3` | OpenRocket 12.03dev |
| `v1.6-a-simple-model-rocket.ork` | `release-13.05` | `core/resources/datafiles/examples/A simple model rocket.ork` | `febaadaeb62cbb70c86e35d634a1faafb38ba40a` | OpenRocket 13.04beta1 |
| `v1.6-apocd.ork` | `release-13.05` | `core/test-writing/apocd.ork` | `7f67b35ab1a5ad948c72fc72418627d2ec19b81c` | OpenRocket 12.03dev |
| `v1.6-boosted-dart.ork` | `release-13.05` | `core/resources/datafiles/examples/Boosted Dart.ork` | `7e6f31cf17bf2969ecbe296d5db395c0a07335d5` | OpenRocket 13.04beta1 |
| `v1.6-high-power-airstart.ork` | `release-13.05` | `core/resources/datafiles/examples/High Power Airstart.ork` | `87ce4e0d1d60bc9dd0f16bbae40eee17ff2a1ae0` | OpenRocket 13.04beta1 |
| `v1.6-preset-usage-decals-first.ork` | `release-13.05` | `core/resources/datafiles/examples/Preset Usage.ork` | `932e24dcd6d2fa01684c009a50d5b73ec434bf22` | OpenRocket 13.04beta1 |
| `v1.6-simulation-listeners.ork` | `release-13.05` | `core/resources/datafiles/examples/Simulation listeners.ork` | `8c1070fd58ac7e71b0e9c64199fdcd7b7e3a01aa` | OpenRocket 13.04beta1 |
| `v1.6-tarc-payloader.ork` | `release-13.05` | `core/resources/datafiles/examples/TARC Payloader.ork` | `8508ea1a32ffef4f0070c5fc420e4ac35e66825f` | OpenRocket 13.04beta1 |
| `v1.6-three-stage-rocket.ork` | `release-13.05` | `core/resources/datafiles/examples/Three-stage rocket.ork` | `1775c67745059fd5cb16c2d76da08e435e42e66f` | OpenRocket 13.04beta1 |
| `v1.7-simulation-extensions-and-scripting.ork` | `release-15.03` | `swing/resources/datafiles/examples/Simulation extensions and scripting.ork` | `a1592fc1166e071b01d92edfef27eb4a1cda4099` | OpenRocket 14.11dev |
| `v1.7-tube-fin.ork` | `release-15.03` | `swing/resources/datafiles/examples/Tube Fin.ork` | `211b210b384a243d7a786b1f3a81c2922ca5f029` | OpenRocket 14.11dev |
| `v1.8-logo-rocket.ork` | `release-22.02` | `core/resources-src/pix/icon/logo_rocket.ork` | `87bc4de03f70348382d3db5d80e7c0b026edc523` | OpenRocket 22.02.beta.05 |
| `v1.8-parallel-staging-example.ork` | `release-22.02.beta.01` | `swing/resources/datafiles/examples/Parallel Staging Example.ork` | `83ddb6d268305cab7c407fc23f0c3c1e0ca1b83f` | OpenRocket 19-xx-alpha-12 |
| `v1.8-pods-example.ork` | `release-22.02.beta.01` | `swing/resources/datafiles/examples/Pods Example.ork` | `813eaa39fbeae23b5d4026974967f6246842612c` | OpenRocket 19-xx-alpha-12 |
| `v1.9-chute-release.ork` | `release-23.09` | `swing/resources/datafiles/examples/Chute release.ork` | `cc5f5739acfff616a16966f6edd54d4f2f8bd074` | OpenRocket 23.09.beta.01 |

Three of the blobs are still in OpenRocket's tree at the commit of the goldens (5f164fd0e):
`swing/src/test/resources/simplerocket.ork`, `test-writing/apocd.ork` and
`swing/resources-src/pix/icon/logo_rocket.ork`.

## The zip archives

The entries in the order of the file, with the size of their contents in bytes. OpenRocket reads an
archive with `java.util.zip.ZipInputStream` and looks at its first entry only.

| File | Entries |
|---|---|
| `v1.6-a-simple-model-rocket.ork` | `rocket.ork` 16126, `decals/BodyStripe.png` 257, `decals/TailStripe.png` 267 |
| `v1.6-boosted-dart.ork` | `rocket.ork` 22766, `decals/openRocket.png` 7256, `decals/us.png` 2486 |
| `v1.6-high-power-airstart.ork` | `rocket.ork` 23354, `decals/patriot.png` 2121 |
| `v1.6-preset-usage-decals-first.ork` | `decals/` (a directory), `rocket.ork` 9182, `decals/beta.png` 674, `decals/open.png` 1901 (stored, not deflated) |
| `v1.6-simulation-listeners.ork` | `rocket.ork` 21086, `decals/skunk.jpg` 15601, `decals/sticker.gif` 1109 |
| `v1.6-tarc-payloader.ork` | `rocket.ork` 12606, `decals/spiral-wound-alpha.png` 2044 |
| `v1.6-three-stage-rocket.ork` | `rocket.ork` 24903, `decals/gStripe.png` 225 |
| `v1.7-simulation-extensions-and-scripting.ork` | `rocket.ork` 24241, `decals/skunk.jpg` 15601, `decals/sticker.gif` 1109 |
| `v1.7-tube-fin.ork` | `rocket.ork` 111881 |
| `v1.8-logo-rocket.ork` | `rocket.ork` 7856 |
| `v1.8-parallel-staging-example.ork` | `rocket.ork` 12032 |
| `v1.8-pods-example.ork` | `rocket.ork` 12011 |
| `v1.9-chute-release.ork` | `rocket.ork` 359199, `decals/BodyStripe.png` 257, `decals/TailStripe.png` 267 |

The archives of the versions 1.6 and 1.7 (but `v1.7-tube-fin.ork`) have entries with a data
descriptor and no UTF-8 flag (general purpose flags 0x0008; those of
`v1.6-preset-usage-decals-first.ork` have neither: 0x0000); the later ones have both (0x0808), as
the examples of `data/examples/` do.

## What OpenRocket makes of them

Measured with OpenRocket at the commit of the goldens (5f164fd0e), `GeneralRocketLoader` with the
bundled motor database (`initial_motors.db`) and the component preset database bound, as the
golden harness sets OpenRocket up (the probes of the tier 9 scouts; their output is not part of
the repository). "Warnings" are the loader's; a simulation's own stored warnings are named with
it. Components are counted with the rocket; a status is `Simulation.getStatus()` after loading.
This is OpenRocket's result, not a specification of QtRocket's: the tests of QtRocket's loader pin
what it makes of each file, and say where that differs on purpose. Two differences are known
beforehand: QtRocket has no component presets before Milestone 3, so a preset reference that
OpenRocket resolves gives the warning of an unknown preset there (`v1.5-preset-usage.ork`,
`v1.7-tube-fin.ork`); and QtRocket's plan takes the `*.ork` entry of an archive as the document,
where OpenRocket looks at the first entry only (`v1.6-preset-usage-decals-first.ork`).

- **`simplerocket.ork`** (OpenRocket's own test file). Warnings: 1, "Multiple motors with
  designation 'A8' for manufacturer 'Estes' found, one chosen arbitrarily." (the motor has a
  `<digest>`, which the loader does not use in a file older than 1.4). Rocket "A simple model
  rocket": 1 stage, 14 components, 5 flight configurations, of which the file defines one,
  `[A8-3]`: four of the five simulations have an empty `<configid>`, for each of which the loader
  makes a new configuration without motors. 5 simulations, the first out of date, the others
  "can't be run"; no stored flight data branch.
- **`v1.0-roll-stabilized.ork`**. Warnings: 1, the missing motor "No motor with designation 'D7'
  for manufacturer 'WECO Feuerwerk' found." Rocket "Roll-stabilized rocket": 1 stage, 14
  components, 2 configurations (selected `[D12-5]`), one motor, Estes D12 with a delay of 5 s.
  1 simulation, loaded from the file with one branch, "Main": 171 rows, 47 data types, 10 events.
  The stored type "Position parallel to wind" is unknown to OpenRocket (it becomes a type without
  symbol or unit, with a log message and no warning). The two motors have neither a `<type>` nor
  a `<digest>`, and the ignition is given for the mount only.
- **`v1.4-roll-stabilized.ork`** (the same design, saved by OpenRocket 12.03). Warnings: 1, the
  same missing motor. 14 components, 2 configurations (selected `[No motors]`). 1 simulation,
  loaded from the file with one branch, "MAIN": 174 rows, 51 data types, 10 events; the same
  unknown type.
- **`v1.5-preset-usage.ork`**. Warnings: 1, "No matching ComponentPreset for component Nose cone
  found matching SEMROC Astronautics BNC-55F". Rocket "3FNC Using Presets": 1 stage, 10
  components, 6 of them with a preset that OpenRocket finds (SEMROC BT-55, RA-5055 twice, BT-50J,
  LL-117, PN-18), 1 configuration, `[D12-7]`. 1 simulation, out of date, no stored branch.
  11 document materials.
- **`v1.6-a-simple-model-rocket.ork`**. Warnings: none. 1 stage, 13 components, 5 configurations
  (selected `[C6-5]`; Estes A8, B4, C6), 5 appearances, 2 decal images (`decals/BodyStripe.png`,
  `decals/TailStripe.png`). 5 simulations, loaded from the file as summaries only (no branch);
  "Simulation 3 - too short delay" has the stored warning "Recovery device deployment at high
  speed (100 ft/s)." as text.
- **`v1.6-apocd.ork`** (from OpenRocket's `test-writing` directory, plain XML). Warnings: 3,
  "Unknown attributes in element 'ambient', ignoring.", the same for 'diffuse' and for 'specular'
  (children of `<appearance>` that today's loader does not know). Rocket "apocalypse": 1 stage,
  31 components, no flight configuration but the default, 20 appearances, 8 components with a decal
  of 5 different names (`decals/Apocalypse_CONE_pointsFrfl.jpg`,
  `decals/Apocalypse_logo_HAUT_fr2.jpg`, `decals/Apocalypse_logo_bas_coup2.jpg`,
  `decals/Apocalypse_logo_medium2.jpg`, `decals/fin rad-test5.jpg`); a plain XML design looks
  for them next to the file, where there is none (no warning; reading the image fails later).
  No simulation. 10 document materials. A transition, `<overridesubcomponents>`.
- **`v1.6-boosted-dart.ork`**. Warnings: 1, the missing motor "No motor with designation
  'J1000-LW' for manufacturer 'Loki Research' found." Rocket "Nike Apache Boosted Dart": 2
  stages, 26 components, 2 configurations (selected "Separation @1s"), no motor (the two motor
  elements name the missing one), 17 appearances, 2 decal images. 2 simulations, both "can't be
  run", each with the stored warning "Discontinuity in rocket body diameter." Four transitions,
  line styles.
- **`v1.6-high-power-airstart.ork`**. Warnings: none. 1 stage, 18 components, 5 configurations,
  four of them with names of their own ("Airstart @1s", "Airstart @2s", "airstart @4s",
  "airstart @6s", the selected one), 10 motor entries (AeroTech K550W and I211W, all plugged, the
  I211W with ignition delays of 1, 2, 4 and 6 s in the four named configurations), 8
  appearances, 1 decal image. 5 simulations, out of date, each with the stored warning "Recovery
  device deployment at high speed (93.0 ft/s)."
- **`v1.6-preset-usage-decals-first.ork`**. The first entry of the archive is the directory
  `decals/`, not the document. OpenRocket looks at the first entry only and returns an **empty
  design without a warning**: a rocket named "Rocket" with no stage, no flight configuration and
  no simulation. (The document that it does not read is the "Preset Usage" example of 13.05: one
  stage, one motor configuration, one simulation, seven preset references and two decals.)
- **`v1.6-simulation-listeners.ork`**. Warnings: none. Rocket "Simulation listener example": 1
  stage, 24 components, 1 configuration, `[L540-P]` (HyperTEK L540, plugged), 9 appearances, 2
  decal images. 3 simulations, out of date; the three `<listener>` elements become `JavaCode`
  extensions: "Java code: net.sf.openrocket.simulation.listeners.example.RollControlListener" in
  "Active roll control", and that one and "Java code: ...example.AirStart" in "Roll control +
  air-start".
- **`v1.6-tarc-payloader.ork`**. Warnings: none. Rocket "Example TARC Payloader": 2 stages, 17
  components, 1 configuration, `[None; F50-9]` (AeroTech F50T), 8 appearances, 1 decal image.
  1 simulation, loaded from the file as a summary. A recovery device that is deployed at the
  separation of the lower stage.
- **`v1.6-three-stage-rocket.ork`**. Warnings: none. Rocket "Three-staged rocket": 3 stages, 28
  components, 3 configurations (9 motor entries: Estes A8, B6 and C6), 10 appearances, 1 decal
  image. 3 simulations, out of date.
- **`v1.7-simulation-extensions-and-scripting.ork`**. Warnings: 1, "Simulation extension with id
  'info.openrocket.core.simulation.extension.impl.AirStart' not found." (the file names
  `net.sf.openrocket.simulation.extension.impl.AirStart`, which the loader maps to the package
  of today, where the class is `example.AirStart`). Rocket "Simulation extension example": 1
  stage, 24 components, 1 configuration, 9 appearances, 2 decal images. 3 simulations, loaded
  from the file as summaries; "Active roll control" and "Roll control + air-start" each have one
  `ScriptingExtension` (JavaScript, a script of 1233 characters, stored as enabled and still
  enabled after OpenRocket has loaded it).
- **`v1.7-tube-fin.ork`**. Warnings: none. Rocket "Rocket": 1 stage, 6 components, one with a
  preset that OpenRocket finds (FlisKits BT-50-18), 1 configuration, `[D12-7]`. 1 simulation,
  loaded from the file with one branch, "Sustainer": 257 rows, 54 data types, 10 events, and the
  stored warnings "Tube fin support is experimental" and "Too many parallel fins". Unknown stored
  types: "Position parallel to wind", "Propellant mass".
- **`v1.8-logo-rocket.ork`** (the design of OpenRocket's logo). Warnings: none. 1 stage, 9
  components, no flight configuration but the default, 6 appearances, 25 photo studio settings.
  No simulation. A pod set, two transitions.
- **`v1.8-parallel-staging-example.ork`**. Warnings: none. Rocket "Booster Rocket": 3 stages (one
  of them a `<parallelstage>`), 16 components, 1 configuration (AeroTech I132W and D21T, ignited
  at the launch). 1 simulation, out of date, with the stored warnings "Discontinuity in rocket
  body diameter.", "Large angle of attack encountered (52.8)." and "Simulation values exceeded
  limits.  Try selecting a shorter time step." (with two spaces). A transition.
- **`v1.8-pods-example.ork`**. Warnings: none. Rocket "Pod Rocket": 2 stages, 17 components, 1
  configuration (the same motors). 1 simulation, out of date. A pod set, a transition.
- **`v1.9-chute-release.ork`**. Warnings: none. 1 stage, 15 components, 2 configurations
  (AeroTech G40W and G80T; selected `[G80-10]`), 6 appearances, 2 decal images, 5 document
  materials. The only file here whose components have an `<id>`.
  2 simulations with their flight data: "Simulation 2", out of date, one branch of 440 rows;
  "Simulation 3", loaded from the file, one branch of 534 rows (54 data types and 11 events each).

Nothing here reaches `<boosterset>`, inside appearances, `<datatypes>`, `<pref>`, the extended ISA
atmosphere, constant gravity, a fixed random seed, the multi-level wind, lookup tables, a disabled
stage or the cause of an abort: the loader tests write XML of their own for those.
