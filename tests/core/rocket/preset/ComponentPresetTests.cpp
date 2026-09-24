#include "QtRocket/rocket/preset/ComponentPreset.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Md5.h"
#include "QtRocket/util/Strings.h"

namespace
{

using QtRocket::AnyTypedKey;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetFactory;
using QtRocket::ComponentPresetType;
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::TransitionShape;
using QtRocket::TypedPropertyMap;
using QtRocket::UnitGroupId;

/// The names of @p keys, in order.
[[nodiscard]] std::vector<std::string_view> namesOf(std::span<const AnyTypedKey> keys)
{
    std::vector<std::string_view> names;
    names.reserve(keys.size());
    for (const AnyTypedKey& key : keys)
    {
        names.push_back(key.getName());
    }
    return names;
}

/// The preset the factory makes of @p props (with an empty material storage); a refusal throws
/// std::bad_expected_access, which fails the test.
[[nodiscard]] ComponentPreset make(const TypedPropertyMap& props)
{
    const QtRocket::MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

// ---- the five presets whose digests OpenRocket computed (JDK 17, ComponentPresetFactory.create)

[[nodiscard]] TypedPropertyMap bodyTubeSpec()
{
    TypedPropertyMap p;
    p.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    p.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    p.put(ComponentPreset::kPartNo, "BT-20");
    // "Body tube BT-20 µm été 🚀": writeBytes keeps the low byte of each
    // UTF-16 unit.
    p.put(ComponentPreset::kDescription,
          "Body tube BT-20 \xC2\xB5m \xC3\xA9t\xC3\xA9 \xF0\x9F\x9A\x80");
    p.put(ComponentPreset::kLength, 0.3);
    p.put(ComponentPreset::kOuterDiameter, 0.0247);
    p.put(ComponentPreset::kInnerDiameter, 0.0237);
    p.put(ComponentPreset::kLegacy, true);
    p.put(ComponentPreset::kMaterial, Material::newMaterial(Material::Type::BULK, "Paper (office)",
                                                            820.0, MaterialGroup::PAPER, false));
    return p;
}

[[nodiscard]] TypedPropertyMap noseConeSpec()
{
    TypedPropertyMap n;
    n.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
    n.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("LOC/Precision"));
    n.put(ComponentPreset::kPartNo, "PNC-3.00");
    n.put(ComponentPreset::kLength, 0.2413);
    n.put(ComponentPreset::kShape, TransitionShape::OGIVE);
    n.put(ComponentPreset::kAftOuterDiameter, 0.07874);
    n.put(ComponentPreset::kAftShoulderDiameter, 0.0762);
    n.put(ComponentPreset::kAftShoulderLength, 0.0635);
    n.put(ComponentPreset::kFilled, false);
    n.put(ComponentPreset::kThickness, 0.002);
    n.put(ComponentPreset::kFinish, Finish::SMOOTH);
    n.put(ComponentPreset::kCd, std::numeric_limits<double>::quiet_NaN());
    return n;
}

[[nodiscard]] TypedPropertyMap parachuteSpec()
{
    TypedPropertyMap c;
    c.put(ComponentPreset::kType, ComponentPresetType::PARACHUTE);
    c.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Rocketman"));
    c.put(ComponentPreset::kPartNo, "R4C");
    c.put(ComponentPreset::kDescription, "4 ft Classic");
    c.put(ComponentPreset::kDiameter, 1.2192);
    c.put(ComponentPreset::kLineCount, 8);
    c.put(ComponentPreset::kSides, 8);
    c.put(ComponentPreset::kLineLength, 1.2);
    c.put(ComponentPreset::kCd, 0.97);
    c.put(ComponentPreset::kMass, -0.0);
    c.put(ComponentPreset::kMaterial,
          Material::newMaterial(Material::Type::SURFACE, "Ripstop nylon", 0.067,
                                MaterialGroup::FABRICS, false));
    c.put(ComponentPreset::kLineMaterial,
          Material::newMaterial(Material::Type::LINE, "Braided nylon (2 mm, 1/16 in)", 0.001,
                                MaterialGroup::NYLONS, false));
    c.put(ComponentPreset::kImage,
          std::vector<std::byte>{std::byte{1}, std::byte{2}, std::byte{3}});
    c.put(ComponentPreset::kCanopyShape, TransitionShape::HAACK);
    return c;
}

[[nodiscard]] TypedPropertyMap streamerSpec()
{
    TypedPropertyMap s;
    s.put(ComponentPreset::kType, ComponentPresetType::STREAMER);
    // "a" is an alias of AeroTech.
    s.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("a"));
    s.put(ComponentPreset::kPartNo, "");
    s.put(ComponentPreset::kLength, 1.0);
    s.put(ComponentPreset::kWidth, 0.05);
    return s;
}

[[nodiscard]] TypedPropertyMap centeringRingSpec()
{
    TypedPropertyMap r;
    r.put(ComponentPreset::kType, ComponentPresetType::CENTERING_RING);
    r.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    r.put(ComponentPreset::kPartNo, "RA-2050");
    r.put(ComponentPreset::kLength, 0.003);
    r.put(ComponentPreset::kOuterDiameter, 0.0332);
    r.put(ComponentPreset::kThickness, 0.0045);
    return r;
}

TEST(ComponentPreset, DigestsMatchOpenRocket)
{
    EXPECT_EQ(make(bodyTubeSpec()).getDigest(), "969303205d66134d65c638ac2c6ab0fb");
    EXPECT_EQ(make(noseConeSpec()).getDigest(), "2a1aa9dfd3c677c88841620c646fde15");
    EXPECT_EQ(make(parachuteSpec()).getDigest(), "a340eb161ed76f761f64ff06db6abdef");
    EXPECT_EQ(make(streamerSpec()).getDigest(), "ed6beb4fed48c4c03b4ddb1651621f3c");
    EXPECT_EQ(make(centeringRingSpec()).getDigest(), "a3315e28442e45f30579285979c1fdb1");
}

TEST(ComponentPreset, HashCodeIsTheDigestsJavaHash)
{
    EXPECT_EQ(make(bodyTubeSpec()).hashCode(), -1122048392);
    EXPECT_EQ(make(noseConeSpec()).hashCode(), -1570750385);
    EXPECT_EQ(make(parachuteSpec()).hashCode(), 1724224075);
    EXPECT_EQ(make(streamerSpec()).hashCode(), 1096144250);
}

TEST(ComponentPreset, DigestSkipsLegacyAndIgnoresOrder)
{
    const std::string digest = make(bodyTubeSpec()).getDigest();

    TypedPropertyMap noLegacy = bodyTubeSpec();
    ASSERT_TRUE(noLegacy.remove(ComponentPreset::kLegacy));
    EXPECT_EQ(make(noLegacy).getDigest(), digest);

    TypedPropertyMap legacyFalse = bodyTubeSpec();
    legacyFalse.put(ComponentPreset::kLegacy, false);
    EXPECT_EQ(make(legacyFalse).getDigest(), digest);

    // The keys are sorted by name, so the insertion order does not matter.
    TypedPropertyMap reordered;
    reordered.put(ComponentPreset::kMaterial,
                  Material::newMaterial(Material::Type::BULK, "Paper (office)", 820.0,
                                        MaterialGroup::PAPER, false));
    reordered.put(ComponentPreset::kInnerDiameter, 0.0237);
    reordered.put(ComponentPreset::kOuterDiameter, 0.0247);
    reordered.put(ComponentPreset::kLength, 0.3);
    reordered.put(ComponentPreset::kDescription,
                  "Body tube BT-20 \xC2\xB5m \xC3\xA9t\xC3\xA9 \xF0\x9F\x9A\x80");
    reordered.put(ComponentPreset::kPartNo, "BT-20");
    reordered.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    reordered.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(make(reordered).getDigest(), digest);

    // Any change of a counted value changes it.
    TypedPropertyMap longer = bodyTubeSpec();
    longer.put(ComponentPreset::kLength, 0.30000000000000004);
    EXPECT_NE(make(longer).getDigest(), digest);
    TypedPropertyMap denser = bodyTubeSpec();
    denser.put(ComponentPreset::kMaterial,
               Material::newMaterial(Material::Type::BULK, "Paper (office)", 821.0, false));
    EXPECT_NE(make(denser).getDigest(), digest);
}

TEST(ComponentPreset, DigestCountsIntsAndImagesByNameOnly)
{
    // computeDigest has no branch for Integer nor byte[]: only the key's name is written.
    const std::string digest    = make(parachuteSpec()).getDigest();
    TypedPropertyMap  moreLines = parachuteSpec();
    moreLines.put(ComponentPreset::kLineCount, 12);
    moreLines.put(ComponentPreset::kSides, 3);
    moreLines.put(ComponentPreset::kImage, std::vector<std::byte>(100, std::byte{7}));
    EXPECT_EQ(make(moreLines).getDigest(), digest);
    // But their presence counts.
    TypedPropertyMap noSides = parachuteSpec();
    ASSERT_TRUE(noSides.remove(ComponentPreset::kSides));
    EXPECT_NE(make(noSides).getDigest(), digest);
}

TEST(ComponentPreset, DigestWritesNaNCanonically)
{
    // Double.doubleToLongBits collapses every NaN to 0x7ff8000000000000.
    const std::string digest  = make(noseConeSpec()).getDigest();
    TypedPropertyMap  payload = noseConeSpec();
    payload.put(ComponentPreset::kCd, std::bit_cast<double>(std::uint64_t{0x7ff8000000000123ULL}));
    EXPECT_EQ(make(payload).getDigest(), digest);
    TypedPropertyMap negative = noseConeSpec();
    negative.put(ComponentPreset::kCd, -std::numeric_limits<double>::quiet_NaN());
    EXPECT_EQ(make(negative).getDigest(), digest);
}

/// Appends what DataOutputStream.writeBytes writes for the ASCII @p text: one byte a character.
void writeAscii(std::vector<std::byte>& out, std::string_view text)
{
    for (const char c : text)
    {
        out.push_back(static_cast<std::byte>(c));
    }
}

/// Appends what DataOutputStream.writeDouble writes: the IEEE bits, big-endian.
void writeDouble(std::vector<std::byte>& out, double value)
{
    const auto bits = std::bit_cast<std::uint64_t>(value);
    for (unsigned shift = 64; shift > 0;)
    {
        shift -= 8;
        out.push_back(static_cast<std::byte>((bits >> shift) & 0xFFU));
    }
}

TEST(ComponentPreset, DigestWritesKeysOfOneNameInInsertionOrder)
{
    // Keys of different value types may share a name. Java's List.sort is stable, so such keys
    // are written in the order they were put, whatever the platform's sort.
    static constexpr QtRocket::TypedKey<double>      kZetaDouble{"Zeta"};
    static constexpr QtRocket::TypedKey<std::string> kZetaString{"Zeta"};

    TypedPropertyMap doubleFirst = streamerSpec();
    doubleFirst.put(kZetaDouble, 1.5);
    doubleFirst.put(kZetaString, "z");
    TypedPropertyMap stringFirst = streamerSpec();
    stringFirst.put(kZetaString, "z");
    stringFirst.put(kZetaDouble, 1.5);

    // The streamer's properties sorted by name, as computeDigest writes them.
    std::vector<std::byte> streamer;
    writeAscii(streamer, "Length");
    writeDouble(streamer, 1.0);
    writeAscii(streamer, "Manufacturer");
    writeAscii(streamer, Manufacturer::getManufacturer("a").getSimpleName());
    writeAscii(streamer, "PartNo");
    writeAscii(streamer, "Type");
    writeAscii(streamer, "STREAMER");
    writeAscii(streamer, "Width");
    writeDouble(streamer, 0.05);
    ASSERT_EQ(QtRocket::Strings::hexString(QtRocket::md5(streamer)),
              make(streamerSpec()).getDigest());

    std::vector<std::byte> expectedDoubleFirst = streamer;
    writeAscii(expectedDoubleFirst, "Zeta");
    writeDouble(expectedDoubleFirst, 1.5);
    writeAscii(expectedDoubleFirst, "Zeta");
    writeAscii(expectedDoubleFirst, "z");
    std::vector<std::byte> expectedStringFirst = streamer;
    writeAscii(expectedStringFirst, "Zeta");
    writeAscii(expectedStringFirst, "z");
    writeAscii(expectedStringFirst, "Zeta");
    writeDouble(expectedStringFirst, 1.5);

    EXPECT_EQ(make(doubleFirst).getDigest(),
              QtRocket::Strings::hexString(QtRocket::md5(expectedDoubleFirst)));
    EXPECT_EQ(make(stringFirst).getDigest(),
              QtRocket::Strings::hexString(QtRocket::md5(expectedStringFirst)));
    EXPECT_NE(make(doubleFirst).getDigest(), make(stringFirst).getDigest());
}

TEST(ComponentPreset, Getters)
{
    const ComponentPreset bodyTube = make(bodyTubeSpec());
    EXPECT_EQ(bodyTube.getType(), ComponentPresetType::BODY_TUBE);
    EXPECT_EQ(&bodyTube.getManufacturer(), &Manufacturer::getManufacturer("Estes"));
    EXPECT_EQ(bodyTube.getPartNo(), "BT-20");
    EXPECT_EQ(bodyTube.toString(), "BT-20");
    EXPECT_EQ(bodyTube.getLegacy(), true);
    EXPECT_EQ(make(streamerSpec()).getLegacy(), std::nullopt);
    EXPECT_TRUE(bodyTube.has(ComponentPreset::kLength));
    EXPECT_FALSE(bodyTube.has(ComponentPreset::kWidth));
    EXPECT_EQ(bodyTube.get(ComponentPreset::kLength), 0.3);
    EXPECT_EQ(bodyTube.get(ComponentPreset::kMaterial).getName(), "Paper (office)");
    // The factory computed the thickness: 0.0247 / 2 - 0.0237 / 2, as OpenRocket
    // (5.000000000000004E-4).
    EXPECT_EQ(bodyTube.get(ComponentPreset::kThickness),
              std::bit_cast<double>(std::uint64_t{0x3f40624dd2f1aa00ULL}));
    EXPECT_EQ(bodyTube.getProperties().size(), 10U);

    const ComponentPreset ring = make(centeringRingSpec());
    // 0.0242, 0.0332 and 0.0045 bit for bit.
    EXPECT_EQ(ring.get(ComponentPreset::kInnerDiameter),
              std::bit_cast<double>(std::uint64_t{0x3f98c7e28240b780ULL}));
    EXPECT_EQ(ring.get(ComponentPreset::kOuterDiameter), 0.0332);
    EXPECT_EQ(ring.get(ComponentPreset::kThickness), 0.0045);
}

/// Whether the part number can be reached through the properties of a @p Preset.
template <class Preset>
concept PropertiesPointInto =
    requires { std::declval<Preset>().getProperties().get(ComponentPreset::kPartNo); };

TEST(ComponentPreset, ATemporaryPresetGivesCopies)
{
    // The references a preset hands out live as long as the preset; a temporary preset gives
    // copies instead, so that a reference bound to the result does not dangle.
    using Lvalue    = const ComponentPreset&;
    using Temporary = const ComponentPreset&&;
    static_assert(std::is_same_v<decltype(std::declval<Lvalue>().getDigest()), const std::string&>);
    static_assert(std::is_same_v<decltype(std::declval<Temporary>().getDigest()), std::string>);
    static_assert(std::is_same_v<decltype(std::declval<Lvalue>().get(ComponentPreset::kMaterial)),
                                 const Material&>);
    static_assert(
        std::is_same_v<decltype(std::declval<Temporary>().get(ComponentPreset::kMaterial)),
                       Material>);
    static_assert(
        std::is_same_v<decltype(std::declval<Lvalue>().getProperties()), const TypedPropertyMap&>);
    static_assert(
        std::is_same_v<decltype(std::declval<Temporary>().getProperties()), TypedPropertyMap>);
    // That copy is a temporary in turn, so what would point into it does not compile.
    static_assert(PropertiesPointInto<Lvalue>);
    static_assert(!PropertiesPointInto<Temporary>);
    static_assert(!PropertiesPointInto<ComponentPreset>);
    // The part number is a copy whatever the preset, as Java's String: a preset reached through a
    // temporary Result's operator-> is an lvalue.
    static_assert(std::is_same_v<decltype(std::declval<Lvalue>().getPartNo()), std::string>);
    static_assert(std::is_same_v<decltype(std::declval<Lvalue>().toString()), std::string>);

    const QtRocket::MaterialStorage materials;
    const std::string&              partNo =
        ComponentPresetFactory::create(bodyTubeSpec(), materials)->getPartNo();
    EXPECT_EQ(partNo, "BT-20");
    const std::string& digest = make(bodyTubeSpec()).getDigest();
    EXPECT_EQ(digest, "969303205d66134d65c638ac2c6ab0fb");
    const Material& material = make(bodyTubeSpec()).get(ComponentPreset::kMaterial);
    EXPECT_EQ(material.getName(), "Paper (office)");
    const double& length = make(bodyTubeSpec()).get(ComponentPreset::kLength);
    EXPECT_EQ(length, 0.3);
    const TypedPropertyMap& properties = make(bodyTubeSpec()).getProperties();
    EXPECT_EQ(properties.size(), 10U);
    const std::string* bound = properties.get(ComponentPreset::kPartNo);
    ASSERT_NE(bound, nullptr);
    EXPECT_EQ(*bound, "BT-20");
}

TEST(ComponentPreset, MissingKeyIsABug)
{
    const ComponentPreset ring = make(centeringRingSpec());
    try
    {
        static_cast<void>(ring.get(ComponentPreset::kWidth));
        FAIL() << "no BugError";
    }
    catch (const QtRocket::BugError& error)
    {
        const std::string what = error.what();
        EXPECT_NE(what.find("Preset did not contain key TypedKey [name=Width] TypedPropertyMap: { "
                            "TypedKey [name=Type] => CENTERING_RING"),
                  std::string::npos)
            << what;
    }
}

TEST(ComponentPreset, IdentityCompareAndKeys)
{
    const ComponentPreset bodyTube  = make(bodyTubeSpec());
    const ComponentPreset noseCone  = make(noseConeSpec());
    const ComponentPreset parachute = make(parachuteSpec());
    const ComponentPreset streamer  = make(streamerSpec());

    // Pinned from OpenRocket: "Estes" vs "LOC/Precision", "AeroTech" vs "Rocketman".
    EXPECT_EQ(bodyTube.compareTo(noseCone), -7);
    EXPECT_EQ(noseCone.compareTo(bodyTube), 7);
    EXPECT_EQ(streamer.compareTo(parachute), -17);
    EXPECT_EQ(bodyTube.compareTo(bodyTube), 0);
    // Same manufacturer: by part number.
    EXPECT_LT(bodyTube.compareTo(make(centeringRingSpec())), 0);

    // Equal by digest.
    EXPECT_TRUE(bodyTube == make(bodyTubeSpec()));
    EXPECT_FALSE(bodyTube == noseCone);

    // ComponentPreset.preferenceKey() uses the manufacturer's display name.
    EXPECT_EQ(bodyTube.preferenceKey(), "Estes|BT-20");
    EXPECT_EQ(streamer.preferenceKey(), "AeroTech|");
    TypedPropertyMap cti = streamerSpec();
    cti.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("CTI"));
    EXPECT_EQ(make(cti).preferenceKey(), "Cesaroni Technology Inc.|");
}

TEST(ComponentPreset, OrkElementAndLookup)
{
    const ComponentPreset bodyTube = make(bodyTubeSpec());
    EXPECT_EQ(bodyTube.toOrkElement(),
              R"(<preset type="BODY_TUBE" manufacturer="Estes" partno="BT-20" )"
              R"(digest="969303205d66134d65c638ac2c6ab0fb"/>)");

    // The simple name is written, and both it and the part number are escaped.
    TypedPropertyMap special = centeringRingSpec();
    special.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("CTI"));
    special.put(ComponentPreset::kPartNo, "A&B \"<1>\"");
    const ComponentPreset escaped = make(special);
    EXPECT_EQ(escaped.toOrkElement(),
              R"(<preset type="CENTERING_RING" manufacturer="Cesaroni Technology" )"
              R"(partno="A&amp;B &quot;&lt;1&gt;&quot;" digest=")" +
                  escaped.getDigest() + R"("/>)");

    // ComponentPresetDao.find: the manufacturer by any of its names, the part number exactly.
    EXPECT_TRUE(bodyTube.matches("Estes", "BT-20"));
    EXPECT_TRUE(bodyTube.matches("estes", "BT-20"));
    EXPECT_TRUE(bodyTube.matches("ES", "BT-20"));
    EXPECT_FALSE(bodyTube.matches("Estes", "bt-20"));
    EXPECT_FALSE(bodyTube.matches("Estes", "BT-20 "));
    EXPECT_FALSE(bodyTube.matches("Apogee", "BT-20"));
}

// ---- keys and types

TEST(ComponentPreset, AllKeysHaveJavasNamesTypesAndUnits)
{
    const std::vector<std::string_view> expected{"Legacy",
                                                 "Manufacturer",
                                                 "PartNo",
                                                 "Description",
                                                 "Type",
                                                 "Length",
                                                 "Height",
                                                 "Width",
                                                 "InnerDiameter",
                                                 "OuterDiameter",
                                                 "ForeShoulderLength",
                                                 "ForeShoulderDiameter",
                                                 "ForeOuterDiameter",
                                                 "AftShoulderLength",
                                                 "AftShoulderDiameter",
                                                 "AftOuterDiameter",
                                                 "DragCoefficient",
                                                 "Shape",
                                                 "Material",
                                                 "Finish",
                                                 "Thickness",
                                                 "Filled",
                                                 "Mass",
                                                 "Diameter",
                                                 "Image",
                                                 "BaseHeight",
                                                 "FlangeHeight",
                                                 "ScrewHeight",
                                                 "ScrewMass",
                                                 "NutMass",
                                                 "CanopyShape",
                                                 "SpillDia",
                                                 "SurfaceArea",
                                                 "Sides",
                                                 "LineCount",
                                                 "LineLength",
                                                 "LineMaterial",
                                                 "PackedLength",
                                                 "PackedDiameter"};
    EXPECT_EQ(namesOf(ComponentPreset::allKeys()), expected);
    const std::set<std::string_view> unique(expected.begin(), expected.end());
    EXPECT_EQ(unique.size(), expected.size());
}

/// The unit group OpenRocket gives @p key: COEFFICIENT for the drag coefficient, MASS for the
/// masses, LENGTH for every other double (SurfaceArea included, as there), none for the rest.
[[nodiscard]] std::optional<UnitGroupId> javaUnitGroup(const AnyTypedKey& key)
{
    if (!key.hasType<double>())
    {
        return std::nullopt;
    }
    if (key == ComponentPreset::kCd)
    {
        return UnitGroupId::COEFFICIENT;
    }
    if (key == ComponentPreset::kMass || key == ComponentPreset::kScrewMass ||
        key == ComponentPreset::kNutMass)
    {
        return UnitGroupId::MASS;
    }
    return UnitGroupId::LENGTH;
}

TEST(ComponentPreset, KeysHaveJavasUnitGroups)
{
    for (const AnyTypedKey& key : ComponentPreset::allKeys())
    {
        EXPECT_EQ(key.getUnitGroup(), javaUnitGroup(key)) << key.getName();
    }
}

TEST(ComponentPreset, KeysHaveJavasValueTypes)
{
    EXPECT_TRUE(ComponentPreset::kSides.hasType<int>());
    EXPECT_TRUE(ComponentPreset::kLineCount.hasType<int>());
    EXPECT_TRUE(ComponentPreset::kCanopyShape.hasType<TransitionShape>());
    EXPECT_TRUE(ComponentPreset::kLineMaterial.hasType<Material>());
    EXPECT_TRUE(ComponentPreset::kFinish.hasType<Finish>());
    EXPECT_TRUE(ComponentPreset::kType.hasType<ComponentPresetType>());
    EXPECT_TRUE(ComponentPreset::kFilled.hasType<bool>());
}

TEST(ComponentPreset, OrderedKeyList)
{
    const std::vector<std::string_view> expected{"Legacy",
                                                 "Manufacturer",
                                                 "PartNo",
                                                 "Description",
                                                 "OuterDiameter",
                                                 "ForeOuterDiameter",
                                                 "AftOuterDiameter",
                                                 "InnerDiameter",
                                                 "Length",
                                                 "Width",
                                                 "AftShoulderDiameter",
                                                 "AftShoulderLength",
                                                 "ForeShoulderDiameter",
                                                 "ForeShoulderLength",
                                                 "BaseHeight",
                                                 "FlangeHeight",
                                                 "ScrewHeight",
                                                 "Shape",
                                                 "Thickness",
                                                 "Filled",
                                                 "Diameter",
                                                 "Sides",
                                                 "LineCount",
                                                 "LineLength",
                                                 "LineMaterial",
                                                 "Mass",
                                                 "ScrewMass",
                                                 "NutMass",
                                                 "Finish",
                                                 "Material"};
    EXPECT_EQ(namesOf(ComponentPreset::orderedKeyList()), expected);
}

TEST(ComponentPreset, DisplayedColumns)
{
    using Names = std::vector<std::string_view>;
    const Names tube{"Legacy",        "Manufacturer",  "PartNo", "Description",
                     "InnerDiameter", "OuterDiameter", "Length"};
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::BODY_TUBE)), tube);
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::CENTERING_RING)),
              tube);
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::ENGINE_BLOCK)), tube);
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::LAUNCH_LUG)), tube);
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::NOSE_CONE)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "Shape", "AftOuterDiameter",
                     "AftShoulderDiameter", "AftShoulderLength", "Length"}));
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::TRANSITION)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "Shape",
                     "ForeOuterDiameter", "ForeShoulderDiameter", "ForeShoulderLength",
                     "AftOuterDiameter", "AftShoulderDiameter", "AftShoulderLength", "Length"}));
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::TUBE_COUPLER)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "OuterDiameter",
                     "InnerDiameter", "Length"}));
    EXPECT_EQ(
        namesOf(ComponentPreset::displayedColumns(ComponentPresetType::BULK_HEAD)),
        (Names{"Legacy", "Manufacturer", "PartNo", "Description", "OuterDiameter", "Length"}));
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::RAIL_BUTTON)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "BaseHeight",
                     "FlangeHeight", "ScrewHeight", "Height", "InnerDiameter", "OuterDiameter",
                     "Mass", "ScrewMass", "NutMass", "DragCoefficient"}));
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::STREAMER)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "Length", "Width",
                     "Thickness", "Material"}));
    EXPECT_EQ(namesOf(ComponentPreset::displayedColumns(ComponentPresetType::PARACHUTE)),
              (Names{"Legacy", "Manufacturer", "PartNo", "Description", "CanopyShape", "Diameter",
                     "SpillDia", "SurfaceArea", "Material", "Sides", "LineCount", "LineLength",
                     "LineMaterial", "DragCoefficient", "PackedDiameter", "PackedLength"}));
}

}  // namespace
