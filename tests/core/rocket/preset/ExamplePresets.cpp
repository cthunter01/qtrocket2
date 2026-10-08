#include "rocket/preset/ExamplePresets.h"

#include <memory>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"

// The property values are the ones OpenRocket's .orc reader gives ComponentPresetFactory.create()
// for these six parts of estes_classic.orc (the scout probe PresetProbe.out of tier 9 lists them
// with their bits), in SI units and in the reader's order of put() calls. The .orc file holds
// inches and ounces; a literal here is the shortest decimal form of the converted double
// (Double.toString), so it parses back to the same bits. Built from these literals with
// OpenRocket's own factory the six digests are the example files' (PresetDatabaseProbe.java of
// the tier 9a probes, part F).

namespace QtRocket::Test
{

namespace
{

/// A material as the .orc reader makes the ones of a preset file: in the group OTHER,
/// user-defined, and a document material.
[[nodiscard]] Material orcMaterial(Material::Type type, const char* name, double density)
{
    return Material::newMaterial(type, name, density, MaterialGroup::OTHER, true, true);
}

/// The properties every one of the six presets has, in the reader's order.
[[nodiscard]] TypedPropertyMap estesPart(const char* partNo, const char* description)
{
    TypedPropertyMap props;
    props.put(ComponentPreset::kLegacy, false);
    props.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    props.put(ComponentPreset::kPartNo, partNo);
    props.put(ComponentPreset::kDescription, description);
    return props;
}

/// A BT-50 body tube of @p length.
[[nodiscard]] TypedPropertyMap bodyTube(const char* partNo, const char* description, double length)
{
    TypedPropertyMap props = estesPart(partNo, description);
    props.put(
        ComponentPreset::kMaterial,
        orcMaterial(Material::Type::BULK, "Paper, spiral kraft glassine, Estes avg, bulk", 894.4));
    props.put(ComponentPreset::kInnerDiameter, 0.02413);
    props.put(ComponentPreset::kOuterDiameter, 0.024790399999999997);
    props.put(ComponentPreset::kLength, length);
    props.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    return props;
}

/// A six-sided plastic parachute of @p diameter with six lines of that length.
[[nodiscard]] TypedPropertyMap parachute(const char* partNo, const char* description,
                                         double diameter)
{
    TypedPropertyMap props = estesPart(partNo, description);
    props.put(
        ComponentPreset::kMaterial,
        orcMaterial(Material::Type::SURFACE, "Polyethylene film, HDPE, 1.0 mil, bare", 0.0235));
    props.put(ComponentPreset::kType, ComponentPresetType::PARACHUTE);
    props.put(ComponentPreset::kDiameter, diameter);
    props.put(ComponentPreset::kLineCount, 6);
    props.put(ComponentPreset::kLineLength, diameter);
    props.put(ComponentPreset::kSides, 6);
    props.put(ComponentPreset::kLineMaterial,
              orcMaterial(Material::Type::LINE, "Carpet Thread", 3.3E-4));
    return props;
}

/// The dimensions of a plastic ogive nose cone with an aft shoulder.
struct NoseConeSize
{
    double mass;
    double aftOuterDiameter;
    double aftShoulderLength;
    double aftShoulderDiameter;
    double length;
};

/// A plastic ogive nose cone, 1.5748 mm (0.062 in) thick. The factory turns the mass into the
/// material's density, mass / volume of a NoseCone with these properties.
[[nodiscard]] TypedPropertyMap noseCone(const char* partNo, const char* description,
                                        const NoseConeSize& size)
{
    TypedPropertyMap props = estesPart(partNo, description);
    props.put(ComponentPreset::kMaterial,
              orcMaterial(Material::Type::BULK, "Polystyrene, cast, bulk", 1050.0));
    props.put(ComponentPreset::kMass, size.mass);
    props.put(ComponentPreset::kShape, TransitionShape::OGIVE);
    props.put(ComponentPreset::kAftOuterDiameter, size.aftOuterDiameter);
    props.put(ComponentPreset::kAftShoulderLength, size.aftShoulderLength);
    props.put(ComponentPreset::kAftShoulderDiameter, size.aftShoulderDiameter);
    props.put(ComponentPreset::kLength, size.length);
    props.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
    props.put(ComponentPreset::kThickness, 0.0015748);
    return props;
}

/// Makes the preset of @p props and adds it to @p database. A refusal by the factory throws
/// std::bad_expected_access, which fails the test that asked for the database.
void addPreset(ComponentPresetDatabase& database, const TypedPropertyMap& props)
{
    // Empty, as far as these presets are concerned, like OpenRocket's material databases: no
    // built-in material is called "Polystyrene, cast, bulk", so the nose cones' computed
    // material is a new one (user-defined, a document material, in the group CUSTOM).
    const MaterialStorage materials;
    database.add(std::make_shared<const ComponentPreset>(
        ComponentPresetFactory::create(props, materials).value()));
}

}  // namespace

ComponentPresetDatabase makeExamplePresetDatabase()
{
    ComponentPresetDatabase database;
    addPreset(database, bodyTube("BT-50FE, 30359", "Body tube, BT-50, 6.5 in., PN 30359", 0.1651));
    addPreset(database, bodyTube("BT-50, 30352", "Body tube, BT-50, 18 in., PN 30352", 0.4572));
    addPreset(database,
              parachute("PK-10, 2262", "Parachute, plastic, preassembled, 10 in., PN 2262", 0.254));
    addPreset(database, parachute("PK-8, 2260", "Parachute kit, plastic, 8 in., PN 2260", 0.2032));
    addPreset(database, noseCone("PNC-50YR, 72604", "Nose cone, plastic, PNC-50YR, PN 72604",
                                 {.mass                = 0.007937866468,
                                  .aftOuterDiameter    = 0.024790399999999997,
                                  .aftShoulderLength   = 0.019049999999999997,
                                  .aftShoulderDiameter = 0.02413,
                                  .length              = 0.104775}));
    addPreset(database,
              noseCone("PNC-20, 072606",
                       R"(Nose cone, plastic, BT-20, 2.65", ogive, PNC-20, white, PN 072606)",
                       {.mass                = 0.0034019427719999998,
                        .aftOuterDiameter    = 0.0186944,
                        .aftShoulderLength   = 0.0127,
                        .aftShoulderDiameter = 0.018033999999999998,
                        .length              = 0.06731}));
    return database;
}

}  // namespace QtRocket::Test
