# Component preset fixtures

Small `.orc` component preset files for the tests of `OpenRocketComponentLoader`
(`tests/core/file/preset/OpenRocketComponentLoaderTests.cpp`), written by hand for QtRocket as
plain XML in the layout of OpenRocket's bundled files (`data/components`). What OpenRocket's own
loader makes of each is the second probe of `tools/openrocket-probes/orc-loader`
(`OrcFileProbe`, the lines "== file ..." of its `expected.txt`), which the tests pin by hand.
`.gitattributes` keeps their bytes (`-text`): the probe prints the size and SHA-256 of each.

| File | What it is | What the loaders do with it |
|---|---|---|
| `every-kind.orc` | One component of each of the eleven kinds, with the optional elements of each, units of every kind (`in`, `mm`, `cm`, `in/64`, `g`, `oz`, `lb`) and a materials section of the three types and four units of measure. | Eleven presets, the same in both. |
| `legacy.orc` | A `<Legacy/>` file: a streamer, whose base properties come before the Legacy flag, and a bulkhead whose part number ends in a space, whose description has a tab and whose mass makes its material. | Two legacy presets, the same in both. |
| `materials.orc` | How a `<Material>` and a parachute's `<LineMaterial>` find their material: the section (by type and exact name, in its unit of measure, with a shear modulus and a group, in SI units without a unit of measure), the built-in materials ignoring case, another type, a new material in a group, and the old group `ThreadsLines`. | Eleven presets, the same in both. |
| `section-order.orc` | A section material of no known type behind the one a component finds, and a first `<Components>` (with such a lookup) that a second one replaces; an unknown `<FinSet>` before the body tube. | One body tube, the same in both. |
| `thumbnail.orc`, `thumbnail.png` | A body tube with a base64 `<Thumbnail>` of the PNG image `thumbnail.png` (2 x 2 pixels), with the properties of BaseComponentDTOTest's preset. | The preset's image is the PNG's bytes, the same in both. |
| `broken.orc` | A body tube, then one without a `<Length>`. | OpenRocket throws a NullPointerException, which ends its whole preset load; QtRocket refuses this file. |
| `unknown-group.orc` | A material of the unknown group `Rocks` that no database has. | OpenRocket throws an IllegalArgumentException, which ends its whole preset load; QtRocket refuses this file. |
| `replaced-materials.orc` | A `<Materials>` with an unknown unit of measure (`lb/in3`), replaced by a second one. | Both refuse the file: JAXB reads the replaced section too. |
| `skipped.orc` | A body tube, then a parachute whose `<LineCount>` is no integer, a body tube of an infinite length and a bulkhead whose section material's density is beyond the doubles once converted from g/cm3. | OpenRocket loads all four presets; QtRocket loads the first and skips the others with a warning each. |
