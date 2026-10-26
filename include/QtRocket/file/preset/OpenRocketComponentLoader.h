#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/file/preset/OrcDocument.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

class ComponentPreset;
class MaterialStorage;
class WarningSet;

/// Reads an .orc component preset file into ComponentPresets: OpenRocket's
/// OpenRocketComponentLoader, with the read half of OpenRocketComponentSaver
/// (fromOpenRocketComponent()) and the DTOs of info.openrocket.core.preset.xml that JAXB binds the
/// file to (OpenRocketComponentDTO, MaterialDTO, BaseComponentDTO and one DTO per kind of
/// component). The JAXB binding is OrcDocument's and OrcElement's: the elements and attributes in
/// no namespace, the last of a repeated element, the texts JAXB hands a field, untrimmed strings,
/// JAXB's lexical rules (JaxbLexical). This class is what the DTOs do with them, step by step in
/// Java's order:
/// - The root must be <OpenRocketComponent> in no namespace. <Version> is never read. The file is
///   legacy when it has a <Legacy> element (with any text).
/// - The materials section is the <Material> list of the last <Materials> (a later wrapper
///   replaces the list of an earlier one; so does a later <Components>). Each MaterialDTO has a
///   <Name>, a <Density>, a <Type> (LINE, SURFACE or BULK, exactly), an optional <ShearModulus>
///   and an optional <Group> (Metals ... Custom, exactly; none or another text is Other), and an
///   attribute UnitsOfMeasure: when it and the type are given, its '2' and '3' become the
///   superscripts and the density is converted from that unit of the type's density units to SI;
///   an unknown unit fails the file, whichever <Materials> the material is in.
/// - <Components> holds the components, in document order: BodyTube, TubeCoupler, NoseCone,
///   Transition, BulkHead, CenteringRing, EngineBlock, LaunchLug, RailButton, Streamer and
///   Parachute; any other element is skipped. Each makes one preset through
///   ComponentPresetFactory::create(), with the properties in the order its DTO puts them (that
///   is the order of ComponentPreset::getProperties()): kLegacy, then the base properties
///   (kManufacturer through Manufacturer::getManufacturer(), kPartNo, and when given kDescription,
///   kMaterial, kMass, kFilled and kImage from a base64 <Thumbnail>), then the kind's own, ending
///   with kType; a Streamer puts the base properties before kLegacy, and a Parachute puts kType
///   right after them. The kinds' elements: BodyTube, TubeCoupler, CenteringRing, EngineBlock and
///   LaunchLug <InsideDiameter>, <OutsideDiameter> and <Length>; BulkHead <OutsideDiameter> and
///   <Length>; NoseCone <Shape>, <OutsideDiameter> (kAftOuterDiameter), optional <ShoulderLength>
///   and <ShoulderDiameter> (the aft shoulder), <Length>, optional <Thickness>; Transition <Shape>,
///   the fore and aft <*OutsideDiameter>, <*ShoulderDiameter> and <*ShoulderLength>, <Length>,
///   optional <Thickness>; RailButton <InnerDiameter>, <OuterDiameter>, <Height>, <BaseHeight>,
///   <FlangeHeight>, <ScrewHeight>, <ScrewMass> and <NutMass>; Streamer <Width>, <Thickness>,
///   <Length>; Parachute <Diameter>, optional <PackedDiameter>, <PackedLength> and
///   <DragCoefficient>, <LineCount>, optional <LineLength>, <Sides> and <LineMaterial>. The
///   elements without "optional" are required. A RailButton reads no <DragCoefficient>, and a
///   Parachute reads its <DragCoefficient> as a length, so a Unit attribute converts it, Java's
///   quirk (Unit="in" makes 0.8 a CD of 0.02032).
/// - A length is a number with an attribute Unit, "m" when there is none, one of the length units
///   by its exact name ("mm", "cm", "m", "in", "in/64", "ft"; "in/64" reads inches, as the unit
///   only shows them in 64ths); a mass the same with the mass units, "kg" when there is none ("g",
///   "kg", "oz", "lb"). A number JAXB does not read is 0.0.
/// - A <Material> (and a parachute's <LineMaterial>) names a material by its text, with attributes
///   Type and Group (BaseComponentDTO.find()): the first section material whose type is the Type
///   and whose name is the text exactly, else a material of the type with that name, ignoring
///   case and translated, among
///   @p materials' (MaterialStorage::findMaterial(type, name)), unless the Group is "ThreadsLines"
///   and that one is not of one of the groups that replaced it, else a new one of density 0 in
///   the Group the file gives (MaterialStorage::materialGroupFromLegacyDatabaseString(), Other
///   without one). The Type must be exactly BULK, SURFACE or LINE.
///
/// What OpenRocket makes of 337 edge cases and of the fixtures of tests/data/presets is the probe
/// tools/openrocket-probes/orc-loader, which orc_loader_edge_case_tests.cpp and
/// OpenRocketComponentLoaderTests.cpp pin. The 31 bundled files (data/components) give
/// OpenRocket's 5231 presets, property for property and digest for digest.
///
/// Errors (bug 18 of Milestone 3, the user's decision U-M3-5): a file is read whole or refused
/// with an Error, and refusing it is the only failure; nothing is thrown. OpenRocket refuses the
/// file with an IOException "Unable to parse component preset file: <file name>" (with " (invalid
/// format or corrupted file)" when JAXB fails) for an XML error, another root, an unknown unit of
/// measure in the section and a preset ComponentPresetFactory refuses (its message after the
/// file's; QtRocket's factory refuses more, see the deviations); QtRocket's message starts as
/// Java's and says why after a colon. For what OpenRocket's
/// DTOs throw a RuntimeException on, which ends OpenRocket's whole preset load, the later files
/// and the user's included, QtRocket refuses only the file, with a message that names the
/// component ("the BodyTube 'BT-20' (component 3)": its element, its <PartNumber> and its number
/// among the components of <Components>) and what is wrong with it: a required element missing,
/// no <Manufacturer>, no <Shape> that is one of ShapeDTO's, an unknown Unit, a <Material> whose
/// Type is none of the three or whose Group is unknown, a section material of no known <Type> or
/// without a <Name> that the lookup comes to. ErrorCode::PARSE for these; an XML error keeps
/// OrcDocument's code (an unsupported document type declaration is UNSUPPORTED_FORMAT).
///
/// Deviations:
/// - A component without a <PartNumber> refuses the file (decision D5 of the scouting). OpenRocket
///   makes a preset with a null part number, whose comparison in the database throws.
/// - A Parachute whose <LineCount> is missing or no integer (Java's null) is skipped with a
///   warning (D5); OpenRocket makes a preset that throws a BugException when it is applied.
/// - A component with a number that is not finite (NaN, INF, -INF, or beyond the doubles, such as
///   1e999), in its lengths, masses and CD or in the density or shear modulus of its material
///   (section materials included, after the unit conversion), is skipped with a warning (bug 19,
///   the rule of the design files). OpenRocket loads it. A negative number passes, as the factory
///   documents, and so does what the factory derives: internal/semroc-legacy.orc's transition
///   BC-08542 [R] has a material of infinite density, as in OpenRocket.
/// - A Streamer or Parachute whose <Material> is not a SURFACE material, or a Parachute whose
///   <LineMaterial> is not a LINE one, is skipped with a warning (the class of D5: a preset
///   OpenRocket loads but cannot use). OpenRocket loads it, and it fails only when it is applied
///   (RecoveryDevice casts the material to Material.Surface). QtRocket's ComponentPresetFactory
///   refuses such a preset (Milestone 1's deviation), which would refuse the whole file, so the
///   loader skips the component before the factory sees it; the factory's other refusals still
///   refuse the file, as OpenRocket's InvalidComponentPresetException does.
/// - A component that is skipped is never given to the factory, so a problem the factory would
///   find in it (which refuses the file in OpenRocket) is not looked for; its other problems, the
///   ones the DTO meets, still refuse the file.
/// - The kind of a component is the name of its element alone: an xsi:type attribute is ignored.
///   OpenRocket's JAXB substitutes the DTO class an xsi:type names on a <BodyTube> element
///   (xsi:type="centeringRingDTO" makes a centering ring; "baseComponentDTO" refuses the file)
///   and ignores it on the other kinds. No file OpenRocket writes has one.
/// - Bounds (decision D7, the policy of the loaders of Milestone 1): a file of more than
///   Limits::fileSize bytes (through loadFile() too, before it is opened), more than
///   Limits::elements elements (OrcDocument) or more than Limits::materials <Material> elements in
///   its <Materials> is refused (ErrorCode::UNSUPPORTED_FORMAT); of the components of more than
///   Limits::presets, the first that many are read, with a warning. OpenRocket reads any file.
/// - OrcDocument's: a document type declaration that declares anything is refused (Java expands
///   its entities), and so is a byte order mark, as in Java.
/// - The warnings are QtRocket's (OpenRocket's loader has none). They go to the caller's set only
///   when the file is read; a caller that loads many files bounds the set (MessageSet::
///   setCapacity(), as the .ork loader does with DocumentConfig::kMaxWarnings).
///
/// The materials of @p materials are only read: the lookups give copies, and a material made for
/// a file is a new user-defined one in the preset, never added to the storage. MaterialStorage is
/// not thread-safe, so a loader runs on the thread that owns it.
class OpenRocketComponentLoader final
{
public:
    /// A preset as the databases hold it (ComponentPresetDatabase::PresetPtr).
    using PresetPtr = std::shared_ptr<const ComponentPreset>;

    /// The largest file a loader reads: 64 MiB, sixty-four times the largest bundled file
    /// (semroc.orc, 978252 bytes).
    static constexpr std::size_t kMaxFileSize = std::size_t{64} * 1024 * 1024;
    /// The most <Material> elements a file's <Materials> may have, fifty times the most of a
    /// bundled file (generic_materials.orc, 200). The lookup of a component's material goes
    /// through them all.
    static constexpr std::size_t kMaxMaterials = 10000;
    /// The most components a loader reads from one file, sixty times the most of a bundled file
    /// (semroc.orc, 1676).
    static constexpr std::size_t kMaxPresets = 100000;

    /// The bounds of a load. An application keeps the defaults; the tests of the bounds lower
    /// them.
    struct Limits
    {
        /// The largest file, in bytes.
        std::size_t fileSize{kMaxFileSize};
        /// The most elements of the document, the root included (OrcDocument).
        std::size_t elements{OrcDocument::kMaxElements};
        /// The most <Material> elements of all <Materials> of the root together.
        std::size_t materials{kMaxMaterials};
        /// The most components of <Components> read.
        std::size_t presets{kMaxPresets};
    };

    /// A loader that looks materials up in @p materials, which must outlive it, with the default
    /// limits.
    explicit OpenRocketComponentLoader(const MaterialStorage& materials) noexcept;
    /// A loader with the limits @p limits.
    OpenRocketComponentLoader(const MaterialStorage& materials, const Limits& limits) noexcept;

    /// The presets of the .orc file @p data, in document order (load(stream, filename)); @p
    /// fileName names the file in the messages (OpenRocket passes File.getName()). Bytes are
    /// decoded as UTF-8, as OpenRocket's InputStreamReader does (OrcDocument::parse()). The
    /// warnings of the file (skipped components, the bound of the components) are added to @p
    /// warnings when the file is read, and none when it is refused.
    [[nodiscard]] Result<std::vector<PresetPtr>> load(std::span<const std::byte> data,
                                                      std::string_view           fileName,
                                                      WarningSet&                warnings) const;

    /// load() of the file @p file, named by its file name (without the directories). A file
    /// whose size is larger than Limits::fileSize is refused before it is opened, as load()
    /// refuses it (ErrorCode::UNSUPPORTED_FORMAT, "... the file is larger than <N> bytes"). A file
    /// that cannot be read, or a source that gives more bytes than the bound although its size
    /// said otherwise (a device, a pipe, a growing file), fails with ErrorCode::IO and
    /// readFile()'s message after "Unable to parse component preset file: <file name>: ".
    [[nodiscard]] Result<std::vector<PresetPtr>> loadFile(const std::filesystem::path& file,
                                                          WarningSet& warnings) const;

    /// The limits of the loads.
    [[nodiscard]] const Limits& limits() const noexcept { return m_limits; }

private:
    const MaterialStorage* m_materials;
    Limits                 m_limits;
};

}  // namespace QtRocket
