#include "QtRocket/file/preset/OpenRocketComponentLoader.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/preset/JaxbLexical.h"
#include "QtRocket/file/preset/OrcDocument.h"
#include "QtRocket/file/preset/OrcElement.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Chars.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/FloatingDecimal.h"

namespace QtRocket
{

namespace
{

using Preset    = ComponentPreset;
using PresetPtr = OpenRocketComponentLoader::PresetPtr;

/// The root element of an .orc file (OpenRocketComponentDTO's @XmlRootElement).
constexpr std::string_view kRootElement = "OpenRocketComponent";

/// An element of <Components> that makes a preset: the @XmlRootElement name of one of the DTOs
/// OpenRocketComponentDTO's @XmlElementRefs list, and the type of the preset it makes.
struct ComponentElement
{
    std::string_view    name;
    ComponentPresetType type;
};

constexpr std::array<ComponentElement, 11> kComponentElements{{
    {.name = "BodyTube", .type = ComponentPresetType::BODY_TUBE},
    {.name = "TubeCoupler", .type = ComponentPresetType::TUBE_COUPLER},
    {.name = "NoseCone", .type = ComponentPresetType::NOSE_CONE},
    {.name = "Transition", .type = ComponentPresetType::TRANSITION},
    {.name = "BulkHead", .type = ComponentPresetType::BULK_HEAD},
    {.name = "CenteringRing", .type = ComponentPresetType::CENTERING_RING},
    {.name = "EngineBlock", .type = ComponentPresetType::ENGINE_BLOCK},
    {.name = "LaunchLug", .type = ComponentPresetType::LAUNCH_LUG},
    {.name = "RailButton", .type = ComponentPresetType::RAIL_BUTTON},
    {.name = "Streamer", .type = ComponentPresetType::STREAMER},
    {.name = "Parachute", .type = ComponentPresetType::PARACHUTE},
}};

/// MaterialTypeDTO's constants, whose names are their texts, and the types they stand for.
constexpr std::array<std::string_view, 3> kMaterialTypeNames{"LINE", "SURFACE", "BULK"};
constexpr std::array<Material::Type, 3>   kMaterialTypes{
    Material::Type::LINE, Material::Type::SURFACE, Material::Type::BULK};

/// MaterialGroupDTO's texts (@XmlEnumValue), in the order of the groups they stand for in
/// kAllMaterialGroups.
constexpr std::array<std::string_view, 13> kMaterialGroupNames{
    "Metals", "Woods",    "Plastics", "Fabrics", "PaperProducts", "Foams", "Composites",
    "Fibers", "Elastics", "Kevlars",  "Nylons",  "Other",         "Custom"};
static_assert(kMaterialGroupNames.size() == kAllMaterialGroups.size());

/// ShapeDTO's constants, whose names are their texts, in the order of the shapes they stand for
/// in kAllTransitionShapes.
constexpr std::array<std::string_view, 6> kShapeNames{"CONICAL", "OGIVE",     "ELLIPSOID",
                                                      "POWER",   "PARABOLIC", "HAACK"};
static_assert(kShapeNames.size() == kAllTransitionShapes.size());

/// The group that replaced "ThreadsLines" (BaseComponentDTO.find()).
[[nodiscard]] constexpr bool isThreadsLinesGroup(MaterialGroup group) noexcept
{
    return group == MaterialGroup::ELASTICS || group == MaterialGroup::KEVLARS ||
           group == MaterialGroup::NYLONS;
}

/// AnnotatedMaterialDTO.getORMaterialType(): the type the Type attribute @p text names exactly,
/// or nullopt where Java throws.
[[nodiscard]] std::optional<Material::Type> annotatedMaterialType(std::string_view text) noexcept
{
    if (text == "BULK")
    {
        return Material::Type::BULK;
    }
    if (text == "SURFACE")
    {
        return Material::Type::SURFACE;
    }
    if (text == "LINE")
    {
        return Material::Type::LINE;
    }
    return std::nullopt;
}

/// The enum field @p name of @p element: the index of its text among @p names, or nullopt for
/// JAXB's null (no element, or a text that is none of the names).
[[nodiscard]] std::optional<std::size_t> enumProperty(const OrcElement&                 element,
                                                      std::string_view                  name,
                                                      std::span<const std::string_view> names)
{
    const std::optional<std::string_view> text = element.stringProperty(name);
    return text ? JaxbLexical::parseEnum(*text, names) : std::nullopt;
}

/// The unit of measure @p uom as MaterialDTO.afterUnmarshal() makes it: every '2' a superscript
/// two, every '3' a superscript three ("kg/m3" becomes kg/m and a superscript three).
[[nodiscard]] std::string densityUnitName(std::string_view uom)
{
    std::string name;
    for (const char c : uom)
    {
        if (c == '2')
        {
            name += Chars::kSquared;
        }
        else if (c == '3')
        {
            name += Chars::kCubed;
        }
        else
        {
            name += c;
        }
    }
    return name;
}

/// The messages of one file, named @p fileName.
class FileMessages
{
public:
    explicit FileMessages(std::string_view fileName) noexcept : m_fileName(fileName) { }

    /// The file refused where OpenRocket's JAXB unmarshalling fails ("... (invalid format or
    /// corrupted file)").
    [[nodiscard]] std::unexpected<Error> invalidFormat(ErrorCode        code,
                                                       std::string_view reason) const
    {
        return fail(code, std::format("Unable to parse component preset file: {} (invalid "
                                      "format or corrupted file): {}",
                                      m_fileName, reason));
    }

    /// The file refused for what a DTO or the factory finds.
    [[nodiscard]] std::unexpected<Error> refused(std::string_view reason) const
    {
        return refused(ErrorCode::PARSE, reason);
    }

    [[nodiscard]] std::unexpected<Error> refused(ErrorCode code, std::string_view reason) const
    {
        return fail(
            code, std::format("Unable to parse component preset file: {}: {}", m_fileName, reason));
    }

    /// A warning about the file.
    [[nodiscard]] std::string warning(std::string_view text) const
    {
        return std::format("Component preset file {}: {}", m_fileName, text);
    }

private:
    std::string_view m_fileName;
};

/// A <Material> of <Materials> as JAXB binds it to a MaterialDTO, after its afterUnmarshal().
struct SectionMaterial
{
    std::optional<std::string>    name;
    double                        density{0.0};
    std::optional<Material::Type> type;
    std::optional<double>         shearModulus;
    std::optional<MaterialGroup>  group;
};

/// MaterialDTO.asMaterial() of @p material, whose type is @p type and name @p name.
[[nodiscard]] Material asMaterial(const SectionMaterial& material, Material::Type type,
                                  std::string_view name, const MaterialStorage& storage)
{
    const MaterialGroup group = material.group.value_or(MaterialGroup::OTHER);
    if (material.shearModulus)
    {
        return storage.findMaterial(type, name, material.density, *material.shearModulus, group);
    }
    return storage.findMaterial(type, name, material.density, group);
}

/// The MaterialDTO of @p element, the material @p number of its <Materials>, or why JAXB's
/// unmarshalling fails on it (its afterUnmarshal() finds no unit of the unit of measure).
[[nodiscard]] std::expected<SectionMaterial, std::string> readSectionMaterial(
    const OrcElement& element, std::size_t number)
{
    SectionMaterial material;
    if (const std::optional<std::string_view> name = element.stringProperty("Name"))
    {
        material.name.emplace(*name);
    }
    material.density = element.doubleProperty("Density").value_or(0.0);
    if (const std::optional<std::size_t> type = enumProperty(element, "Type", kMaterialTypeNames))
    {
        material.type = kMaterialTypes.at(*type);
    }
    material.shearModulus = element.boxedDoubleProperty("ShearModulus");
    if (const std::optional<std::size_t> group =
            enumProperty(element, "Group", kMaterialGroupNames))
    {
        material.group = kAllMaterialGroups.at(*group);
    }

    // afterUnmarshal(): the density in SI units.
    const std::optional<std::string_view> uom = element.attribute("UnitsOfMeasure");
    if (uom && material.type)
    {
        const std::string unitName = densityUnitName(*uom);
        const Unit*       unit     = unitGroup(unitGroupId(*material.type)).getUnit(unitName);
        if (unit == nullptr)
        {
            return std::unexpected(
                std::format("material {} of <Materials> gives its density in the unknown unit '{}'",
                            number, unitName));
        }
        material.density = unit->fromUnit(material.density);
    }
    return material;
}

/// The materials section of the document @p root: the MaterialDTOs of its last <Materials>.
/// JAXB reads every <Materials>, so an unknown unit of measure in one that a later one replaces
/// fails the file too.
[[nodiscard]] Result<std::vector<SectionMaterial>> readSection(const OrcElement&   root,
                                                               const FileMessages& messages,
                                                               std::size_t         maxMaterials)
{
    const std::vector<OrcElement> wrappers = root.children("Materials");
    std::size_t                   count    = 0;
    for (const OrcElement& wrapper : wrappers)
    {
        count += wrapper.children("Material").size();
    }
    if (count > maxMaterials)
    {
        return messages.refused(ErrorCode::UNSUPPORTED_FORMAT,
                                std::format("the file has more than {} materials", maxMaterials));
    }

    std::vector<SectionMaterial> section;
    for (const OrcElement& wrapper : wrappers)
    {
        std::vector<SectionMaterial> materials;
        for (const OrcElement& element : wrapper.children("Material"))
        {
            std::expected<SectionMaterial, std::string> material =
                readSectionMaterial(element, materials.size() + 1);
            if (!material)
            {
                return messages.invalidFormat(ErrorCode::PARSE, material.error());
            }
            materials.push_back(std::move(*material));
        }
        section = std::move(materials);
    }
    return section;
}

/// Whether a component element must be there (a required DTO field: Java's getter throws a
/// NullPointerException without it) or is read when it is there.
enum class Need
{
    REQUIRED,
    IF_PRESENT,
};

/// What the DTO of one component of <Components> puts into its TypedPropertyMap
/// (asComponentPreset()), one property at a time in Java's order: the properties, the first
/// failure where Java throws (which refuses the file), and the first reason to skip the preset
/// (QtRocket's: a null LineCount, a number that is not finite). After a failure the put*()
/// functions do nothing, as Java's statements after a throw.
class ComponentReader
{
public:
    /// The component @p element, of the kind @p kind, the component @p number of its
    /// <Components>, in a file that is legacy or not, with the materials section @p section
    /// and the materials of @p storage; the referenced objects must outlive the reader.
    ComponentReader(const OrcElement& element, const ComponentElement& kind, std::size_t number,
                    bool legacy, std::span<const SectionMaterial> section,
                    const MaterialStorage& storage) noexcept
      : m_element(element),
        m_kind(&kind),
        m_number(number),
        m_legacy(legacy),
        m_section(section),
        m_storage(&storage)
    {
    }

    /// LEGACY.
    void putLegacy()
    {
        if (ok())
        {
            m_properties.put(Preset::kLegacy, m_legacy);
        }
    }

    /// TYPE.
    void putType()
    {
        if (ok())
        {
            m_properties.put(Preset::kType, m_kind->type);
        }
    }

    /// BaseComponentDTO.addProps(): MANUFACTURER, PARTNO, and when given DESCRIPTION, MATERIAL,
    /// MASS, FILLED and IMAGE.
    void putBase()
    {
        if (!ok())
        {
            return;
        }
        const std::optional<std::string_view> manufacturer =
            m_element.stringProperty("Manufacturer");
        if (!manufacturer)
        {
            fail("has no <Manufacturer>");
            return;
        }
        m_properties.put(Preset::kManufacturer,
                         std::cref(Manufacturer::getManufacturer(*manufacturer)));
        // Deviation (D5): Java puts a null part number, which fails later.
        const std::optional<std::string_view> partNo = m_element.stringProperty("PartNumber");
        if (!partNo)
        {
            fail("has no <PartNumber>");
            return;
        }
        m_properties.put(Preset::kPartNo, std::string(*partNo));
        if (const std::optional<std::string_view> description =
                m_element.stringProperty("Description"))
        {
            m_properties.put(Preset::kDescription, std::string(*description));
        }
        putMaterial(Preset::kMaterial, "Material");
        putMass(Preset::kMass, "Mass", Need::IF_PRESENT);
        if (!ok())
        {
            return;
        }
        if (const std::optional<bool> filled = m_element.booleanProperty("Filled"))
        {
            m_properties.put(Preset::kFilled, *filled);
        }
        if (std::optional<std::vector<std::byte>> image =
                m_element.base64BinaryProperty("Thumbnail"))
        {
            m_properties.put(Preset::kImage, std::move(*image));
        }
    }

    /// SHAPE (NoseConeDTO, TransitionDTO): Java's shape.getORShape() throws for a null shape.
    void putShape()
    {
        if (!ok())
        {
            return;
        }
        const std::optional<std::string_view> text = m_element.stringProperty("Shape");
        if (!text)
        {
            fail("has no <Shape>");
            return;
        }
        const std::optional<std::size_t> shape = JaxbLexical::parseEnum(*text, kShapeNames);
        if (!shape)
        {
            fail(
                std::format("has the <Shape> '{}', which is none of CONICAL, OGIVE, ELLIPSOID, "
                            "POWER, PARABOLIC and HAACK",
                            *text));
            return;
        }
        m_properties.put(Preset::kShape, kAllTransitionShapes.at(*shape));
    }

    /// A length from the AnnotatedLengthDTO @p element.
    void putLength(const TypedKey<double>& key, std::string_view element, Need need)
    {
        putQuantity(key, element, need, UnitGroupId::LENGTH, "m");
    }

    /// A mass from the AnnotatedMassDTO @p element.
    void putMass(const TypedKey<double>& key, std::string_view element, Need need)
    {
        putQuantity(key, element, need, UnitGroupId::MASS, "kg");
    }

    /// ParachuteDTO's LINE_COUNT. Deviation (D5): Java puts a null Integer, whose preset throws
    /// a BugException when it is applied; the preset is skipped.
    void putLineCount()
    {
        if (!ok())
        {
            return;
        }
        if (const std::optional<int> count = m_element.integerProperty("LineCount"))
        {
            m_properties.put(Preset::kLineCount, *count);
            return;
        }
        skip("it has no <LineCount> that is an integer");
    }

    /// ParachuteDTO's SIDES, when not null.
    void putSides()
    {
        if (!ok())
        {
            return;
        }
        if (const std::optional<int> sides = m_element.integerProperty("Sides"))
        {
            m_properties.put(Preset::kSides, *sides);
        }
    }

    /// A material from the AnnotatedMaterialDTO @p element, when given (MATERIAL, LINE_MATERIAL).
    void putMaterial(const TypedKey<Material>& key, std::string_view element)
    {
        if (!ok())
        {
            return;
        }
        const std::optional<OrcElement> reference = m_element.lastChild(element);
        if (!reference)
        {
            return;
        }
        std::optional<Material> material = findMaterial(*reference, element);
        if (!material)
        {
            return;
        }
        checkFinite(material->getDensity(), [&] {
            return std::format("the density of its <{}> '{}'", element, material->getName());
        });
        checkFinite(material->getInPlaneShearModulus(), [&] {
            return std::format("the shear modulus of its <{}> '{}'", element, material->getName());
        });
        m_properties.put(key, std::move(*material));
    }

    /// The first failure: what is wrong with the component, after describe().
    [[nodiscard]] const std::optional<std::string>& failure() const noexcept { return m_failure; }

    /// The first reason to skip the preset.
    [[nodiscard]] const std::optional<std::string>& skipReason() const noexcept
    {
        return m_skipReason;
    }

    [[nodiscard]] const TypedPropertyMap& properties() const noexcept { return m_properties; }

    /// "the BodyTube 'BT-20' (component 3)": the element, its part number when it has one, and
    /// its number among the components.
    [[nodiscard]] std::string describe() const
    {
        if (const std::optional<std::string_view> partNo = m_element.stringProperty("PartNumber"))
        {
            return std::format("the {} '{}' (component {})", m_kind->name, *partNo, m_number);
        }
        return std::format("the {} (component {})", m_kind->name, m_number);
    }

private:
    [[nodiscard]] bool ok() const noexcept { return !m_failure.has_value(); }

    void fail(std::string reason)
    {
        if (!m_failure)
        {
            m_failure = std::move(reason);
        }
    }

    void skip(std::string reason)
    {
        if (!m_skipReason)
        {
            m_skipReason = std::move(reason);
        }
    }

    /// Bug 19: a number that is not finite skips the preset; @p subject names it ("its
    /// <Length>").
    template <class Subject>
    void checkFinite(double value, const Subject& subject)
    {
        if (!std::isfinite(value))
        {
            skip(std::format("{} is not a finite number ({})", subject(),
                             FloatingDecimal::toJavaFormatString(value)));
        }
    }

    /// AnnotatedLengthDTO / AnnotatedMassDTO.getValue() of the last element @p element, a
    /// quantity of the unit group @p group whose unit is @p defaultUnit without a Unit attribute.
    void putQuantity(const TypedKey<double>& key, std::string_view element, Need need,
                     UnitGroupId group, std::string_view defaultUnit)
    {
        if (!ok())
        {
            return;
        }
        const std::optional<OrcElement> leaf = m_element.lastChild(element);
        if (!leaf)
        {
            if (need == Need::REQUIRED)
            {
                fail(std::format("has no <{}>", element));
            }
            return;
        }
        const std::string_view unitName = leaf->attribute("Unit").value_or(defaultUnit);
        const Unit*            unit     = unitGroup(group).getUnit(unitName);
        if (unit == nullptr)
        {
            fail(std::format("gives its <{}> in the unknown unit '{}'", element, unitName));
            return;
        }
        const double value = unit->fromUnit(leaf->doubleValue().value_or(0.0));
        checkFinite(value, [&] { return std::format("its <{}>", element); });
        m_properties.put(key, value);
    }

    /// BaseComponentDTO.find() of the AnnotatedMaterialDTO @p reference (the element @p
    /// element): a material of the section, else of the storage, else a new one; nullopt where
    /// Java throws.
    [[nodiscard]] std::optional<Material> findMaterial(const OrcElement& reference,
                                                       std::string_view  element)
    {
        const std::optional<std::string_view> type = reference.attribute("Type");
        const std::string&                    name = reference.text();

        // The loop asks every section material for the name of its type, and those of the type
        // for their names, until one matches.
        std::size_t number = 0;
        for (const SectionMaterial& material : m_section)
        {
            number++;
            if (!material.type)
            {
                fail(
                    std::format("has a <{}>, but material {} of <Materials> has no <Type> that "
                                "is LINE, SURFACE or BULK",
                                element, number));
                return std::nullopt;
            }
            if (type != toString(*material.type))
            {
                continue;
            }
            if (!material.name)
            {
                fail(std::format("has a <{}>, but material {} of <Materials> has no <Name>",
                                 element, number));
                return std::nullopt;
            }
            if (*material.name == name)
            {
                return asMaterial(material, *material.type, *material.name, *m_storage);
            }
        }

        const std::optional<Material::Type> materialType =
            type ? annotatedMaterialType(*type) : std::nullopt;
        if (!materialType)
        {
            fail(type ? std::format("has a <{}> of the Type '{}', which is none of BULK, SURFACE "
                                    "and LINE",
                                    element, *type)
                      : std::format("has a <{}> without a Type", element));
            return std::nullopt;
        }
        return findOutsideSection(*materialType, name, reference.attribute("Group"), element);
    }

    /// The rest of find(): a material of @p type named @p name among the storage's, unless the
    /// Group @p group is "ThreadsLines" and its group did not replace that one; else a new one
    /// of density 0 in the group @p group names.
    [[nodiscard]] std::optional<Material> findOutsideSection(Material::Type                  type,
                                                             std::string_view                name,
                                                             std::optional<std::string_view> group,
                                                             std::string_view element)
    {
        double density = 0.0;
        if (std::optional<Material> found = m_storage->findMaterial(type, name))
        {
            density = found->getDensity();
            if (group != "ThreadsLines" || isThreadsLinesGroup(found->getGroup()))
            {
                return found;
            }
        }
        std::optional<MaterialGroup> resolved = MaterialGroup::OTHER;
        if (group)
        {
            resolved =
                m_storage->materialGroupFromLegacyDatabaseString(*group, type, name, density);
            if (!resolved)
            {
                fail(std::format("has a <{}> of the unknown Group '{}'", element, *group));
                return std::nullopt;
            }
        }
        return m_storage->findMaterial(type, name, 0.0, resolved);
    }

    OrcElement                       m_element;
    const ComponentElement*          m_kind;
    std::size_t                      m_number;
    bool                             m_legacy;
    std::span<const SectionMaterial> m_section;
    const MaterialStorage*           m_storage;
    TypedPropertyMap                 m_properties;
    std::optional<std::string>       m_failure;
    std::optional<std::string>       m_skipReason;
};

/// BodyTubeDTO (with TubeCouplerDTO, CenteringRingDTO and EngineBlockDTO) and LaunchLugDTO.
void readTube(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putLength(Preset::kInnerDiameter, "InsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kOuterDiameter, "OutsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kLength, "Length", Need::REQUIRED);
    reader.putType();
}

/// BulkHeadDTO.
void readBulkHead(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putLength(Preset::kOuterDiameter, "OutsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kLength, "Length", Need::REQUIRED);
    reader.putType();
}

/// NoseConeDTO: the outside diameter and the shoulder are the aft ones.
void readNoseCone(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putShape();
    reader.putLength(Preset::kAftOuterDiameter, "OutsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kAftShoulderLength, "ShoulderLength", Need::IF_PRESENT);
    reader.putLength(Preset::kAftShoulderDiameter, "ShoulderDiameter", Need::IF_PRESENT);
    reader.putLength(Preset::kLength, "Length", Need::REQUIRED);
    reader.putType();
    reader.putLength(Preset::kThickness, "Thickness", Need::IF_PRESENT);
}

/// TransitionDTO.
void readTransition(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putShape();
    reader.putLength(Preset::kForeOuterDiameter, "ForeOutsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kForeShoulderDiameter, "ForeShoulderDiameter", Need::REQUIRED);
    reader.putLength(Preset::kForeShoulderLength, "ForeShoulderLength", Need::REQUIRED);
    reader.putLength(Preset::kAftOuterDiameter, "AftOutsideDiameter", Need::REQUIRED);
    reader.putLength(Preset::kAftShoulderDiameter, "AftShoulderDiameter", Need::REQUIRED);
    reader.putLength(Preset::kAftShoulderLength, "AftShoulderLength", Need::REQUIRED);
    reader.putLength(Preset::kLength, "Length", Need::REQUIRED);
    reader.putType();
    reader.putLength(Preset::kThickness, "Thickness", Need::IF_PRESENT);
}

/// RailButtonDTO, which reads no <DragCoefficient>.
void readRailButton(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putLength(Preset::kInnerDiameter, "InnerDiameter", Need::REQUIRED);
    reader.putLength(Preset::kOuterDiameter, "OuterDiameter", Need::REQUIRED);
    reader.putLength(Preset::kHeight, "Height", Need::REQUIRED);
    reader.putLength(Preset::kBaseHeight, "BaseHeight", Need::REQUIRED);
    reader.putLength(Preset::kFlangeHeight, "FlangeHeight", Need::REQUIRED);
    reader.putLength(Preset::kScrewHeight, "ScrewHeight", Need::REQUIRED);
    reader.putMass(Preset::kScrewMass, "ScrewMass", Need::REQUIRED);
    reader.putMass(Preset::kNutMass, "NutMass", Need::REQUIRED);
    reader.putType();
}

/// StreamerDTO: the base properties come before LEGACY.
void readStreamer(ComponentReader& reader)
{
    reader.putBase();
    reader.putLegacy();
    reader.putLength(Preset::kWidth, "Width", Need::REQUIRED);
    reader.putLength(Preset::kThickness, "Thickness", Need::REQUIRED);
    reader.putLength(Preset::kLength, "Length", Need::REQUIRED);
    reader.putType();
}

/// ParachuteDTO: TYPE right after the base properties; the drag coefficient is read as a
/// length (an AnnotatedLengthDTO in Java), so a Unit attribute converts it.
void readParachute(ComponentReader& reader)
{
    reader.putLegacy();
    reader.putBase();
    reader.putType();
    reader.putLength(Preset::kDiameter, "Diameter", Need::REQUIRED);
    reader.putLength(Preset::kPackedDiameter, "PackedDiameter", Need::IF_PRESENT);
    reader.putLength(Preset::kPackedLength, "PackedLength", Need::IF_PRESENT);
    reader.putLength(Preset::kCd, "DragCoefficient", Need::IF_PRESENT);
    reader.putLineCount();
    reader.putLength(Preset::kLineLength, "LineLength", Need::IF_PRESENT);
    reader.putSides();
    reader.putMaterial(Preset::kLineMaterial, "LineMaterial");
}

/// The DTO of the kind of preset @p type.
void readComponent(ComponentReader& reader, ComponentPresetType type)
{
    switch (type)
    {
        case ComponentPresetType::BODY_TUBE:
        case ComponentPresetType::TUBE_COUPLER:
        case ComponentPresetType::CENTERING_RING:
        case ComponentPresetType::ENGINE_BLOCK:
        case ComponentPresetType::LAUNCH_LUG:
            readTube(reader);
            break;
        case ComponentPresetType::BULK_HEAD:
            readBulkHead(reader);
            break;
        case ComponentPresetType::NOSE_CONE:
            readNoseCone(reader);
            break;
        case ComponentPresetType::TRANSITION:
            readTransition(reader);
            break;
        case ComponentPresetType::RAIL_BUTTON:
            readRailButton(reader);
            break;
        case ComponentPresetType::STREAMER:
            readStreamer(reader);
            break;
        case ComponentPresetType::PARACHUTE:
            readParachute(reader);
            break;
    }
}

/// The component of <Components> @p element is, if it is one.
[[nodiscard]] const ComponentElement* componentElementOf(const OrcElement& element) noexcept
{
    // Through a span: the iterators of std::array are pointers in some libraries, classes in
    // others.
    const std::span<const ComponentElement> kinds(kComponentElements);
    const auto                              found = std::ranges::find_if(
        kinds, [&](const ComponentElement& kind) { return element.isNamed(kind.name); });
    return found == kinds.end() ? nullptr : &*found;
}

/// The components of the last <Components> of @p root, in document order, with their kinds.
[[nodiscard]] std::vector<std::pair<OrcElement, const ComponentElement*>> componentsOf(
    const OrcElement& root)
{
    std::vector<std::pair<OrcElement, const ComponentElement*>> components;
    if (const std::optional<OrcElement> wrapper = root.lastChild("Components"))
    {
        for (const OrcElement& child : wrapper->children())
        {
            if (const ComponentElement* kind = componentElementOf(child))
            {
                components.emplace_back(child, kind);
            }
        }
    }
    return components;
}

/// Why OpenRocket's unmarshaller refuses the root @p root, which is no .orc file's.
[[nodiscard]] std::string rootProblem(const OrcElement& root)
{
    if (root.localName() == kRootElement)
    {
        return std::format("the root element <{}> is in the namespace '{}'", kRootElement,
                           root.namespaceUri());
    }
    return std::format("the root element is <{}>, not <{}>", root.localName(), kRootElement);
}

}  // namespace

OpenRocketComponentLoader::OpenRocketComponentLoader(const MaterialStorage& materials) noexcept
  : OpenRocketComponentLoader(materials, Limits{})
{
}

OpenRocketComponentLoader::OpenRocketComponentLoader(const MaterialStorage& materials,
                                                     const Limits&          limits) noexcept
  : m_materials(&materials), m_limits(limits)
{
}

Result<std::vector<PresetPtr>> OpenRocketComponentLoader::load(std::span<const std::byte> data,
                                                               std::string_view           fileName,
                                                               WarningSet& warnings) const
{
    const FileMessages messages(fileName);
    if (data.size() > m_limits.fileSize)
    {
        return messages.refused(ErrorCode::UNSUPPORTED_FORMAT,
                                std::format("the file is larger than {} bytes", m_limits.fileSize));
    }
    const Result<OrcDocument> document = OrcDocument::parse(data, m_limits.elements);
    if (!document)
    {
        return messages.invalidFormat(document.error().code, document.error().message);
    }
    const OrcElement root = document->root();
    if (!root.isNamed(kRootElement))
    {
        return messages.invalidFormat(ErrorCode::PARSE, rootProblem(root));
    }
    const Result<std::vector<SectionMaterial>> section =
        readSection(root, messages, m_limits.materials);
    if (!section)
    {
        return std::unexpected(section.error());
    }

    const bool             legacy     = root.stringProperty("Legacy").has_value();
    const auto             components = componentsOf(root);
    const std::size_t      count      = std::min(components.size(), m_limits.presets);
    WarningSet             fileWarnings;
    std::vector<PresetPtr> presets;
    presets.reserve(count);
    for (std::size_t i = 0; i < count; i++)
    {
        const auto& [element, kind] = components[i];
        ComponentReader reader(element, *kind, i + 1, legacy, *section, *m_materials);
        readComponent(reader, kind->type);
        if (const std::optional<std::string>& failure = reader.failure())
        {
            return messages.refused(std::format("{} {}", reader.describe(), *failure));
        }
        if (const std::optional<std::string>& reason = reader.skipReason())
        {
            fileWarnings.add(
                messages.warning(std::format("{} was skipped: {}", reader.describe(), *reason)));
            continue;
        }
        Result<ComponentPreset> preset =
            ComponentPresetFactory::create(reader.properties(), *m_materials);
        if (!preset)
        {
            return messages.refused(std::format("{} is not a valid preset: {}", reader.describe(),
                                                preset.error().message));
        }
        presets.push_back(std::make_shared<const ComponentPreset>(std::move(*preset)));
    }
    if (components.size() > count)
    {
        fileWarnings.add(messages.warning(std::format(
            "it has more than {0} components; the first {0} were read", m_limits.presets)));
    }
    warnings.addAll(fileWarnings);
    return presets;
}

Result<std::vector<PresetPtr>> OpenRocketComponentLoader::loadFile(
    const std::filesystem::path& file, WarningSet& warnings) const
{
    const std::string                    fileName = pathToUtf8(file.filename());
    const Result<std::vector<std::byte>> data     = readFile(file, m_limits.fileSize);
    if (!data)
    {
        return fail(ErrorCode::IO, std::format("Unable to parse component preset file: {}: {}",
                                               fileName, data.error().message));
    }
    return load(*data, fileName, warnings);
}

}  // namespace QtRocket
