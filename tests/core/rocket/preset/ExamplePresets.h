#pragma once

#include <array>
#include <string_view>

#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"

/// The component presets the 16 example designs refer to, for the tests that load the examples
/// (tier 9, runs 9b and 9c): six presets of Estes, all from OpenRocket's estes_classic.orc.
/// Reading .orc preset files is Milestone 3, so the six are built here with
/// ComponentPresetFactory::create() from the property values OpenRocket's .orc reader hands its
/// factory. They are the same presets: the digests equal the ones the example files store
/// (example_presets_tests.cpp pins them), so a loader that looks them up finds them, as
/// OpenRocket does with its full preset database when the golden data is made.
///
/// With a database that lacks them (the product until Milestone 3) the loader gives one "No
/// matching ComponentPreset for component <name> found matching Estes <part number>" warning per
/// reference, the components keep the values the files store, and the two nose cones' fore
/// shoulder thickness stays 0 instead of the presets' 0.0015748 m.
namespace QtRocket::Test
{

/// A <preset> element of an example design: what the file stores about the preset.
struct ExamplePresetReference
{
    /// The example file, without its directory.
    std::string_view example;
    /// The type="" attribute.
    ComponentPresetType type;
    /// The manufacturer="" attribute.
    std::string_view manufacturer;
    /// The partno="" attribute.
    std::string_view partNo;
    /// The digest="" attribute.
    std::string_view digest;
};

/// The six <preset> elements of the example designs (the other 321 of their 327 components have
/// none), by file name and then in document order.
inline constexpr std::array<ExamplePresetReference, 6> kExamplePresetReferences{{
    {.example      = "3D printable nose cone and fins.ork",
     .type         = ComponentPresetType::BODY_TUBE,
     .manufacturer = "Estes",
     .partNo       = "BT-50FE, 30359",
     .digest       = "e9c3e97d32e96762f65e051d74cb9fd7"},
    {.example      = "Deployable payload.ork",
     .type         = ComponentPresetType::PARACHUTE,
     .manufacturer = "Estes",
     .partNo       = "PK-10, 2262",
     .digest       = "b595c8a31baaf325291d5c957f3940f8"},
    {.example      = "Deployable payload.ork",
     .type         = ComponentPresetType::PARACHUTE,
     .manufacturer = "Estes",
     .partNo       = "PK-8, 2260",
     .digest       = "9a96b8806749e790db29bacfbc70eb1e"},
    {.example      = "Pods--powered with recovery deployment.ork",
     .type         = ComponentPresetType::NOSE_CONE,
     .manufacturer = "Estes",
     .partNo       = "PNC-50YR, 72604",
     .digest       = "18363dec5642206a109389ef48efcb32"},
    {.example      = "Pods--powered with recovery deployment.ork",
     .type         = ComponentPresetType::BODY_TUBE,
     .manufacturer = "Estes",
     .partNo       = "BT-50, 30352",
     .digest       = "a59dec8e4034a2fee5955dbf4ff07f1c"},
    {.example      = "Pods--powered with recovery deployment.ork",
     .type         = ComponentPresetType::NOSE_CONE,
     .manufacturer = "Estes",
     .partNo       = "PNC-20, 072606",
     .digest       = "6f564a198c80de3af437608827fa1bc1"},
}};

/// A new database holding the six presets of kExamplePresetReferences, added with add() (so in
/// the order of their part numbers: "BT-50, 30352", "BT-50FE, 30359", "PK-10, 2262",
/// "PK-8, 2260", "PNC-20, 072606", "PNC-50YR, 72604"). Their materials are user-defined
/// document materials, as the materials of an .orc file are: loading one of the presets into a
/// component of a document registers its material with the document. The presets are owned by
/// the database and by the components that load them (std::shared_ptr): the database may go
/// before the rockets.
[[nodiscard]] ComponentPresetDatabase makeExamplePresetDatabase();

}  // namespace QtRocket::Test
