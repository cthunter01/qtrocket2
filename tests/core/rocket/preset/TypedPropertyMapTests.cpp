#include "QtRocket/rocket/preset/TypedPropertyMap.h"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/rocket/preset/TypedKey.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "unit/DefaultUnitsGuard.h"

namespace
{

using QtRocket::AnyTypedKey;
using QtRocket::ComponentPreset;
using QtRocket::ComponentPresetType;
using QtRocket::Finish;
using QtRocket::Manufacturer;
using QtRocket::Material;
using QtRocket::TransitionShape;
using QtRocket::TypedKey;
using QtRocket::TypedPropertyMap;

// ---- type safety, checked at compile time

// get() gives a pointer to the key's value type.
static_assert(
    std::is_same_v<decltype(std::declval<const TypedPropertyMap&>().get(ComponentPreset::kLength)),
                   const double*>);
static_assert(std::is_same_v<
              decltype(std::declval<const TypedPropertyMap&>().get(ComponentPreset::kManufacturer)),
              const QtRocket::ManufacturerRef*>);
static_assert(
    std::is_same_v<decltype(std::declval<const TypedPropertyMap&>().get(ComponentPreset::kShape)),
                   const TransitionShape*>);

// get(), getValue() and entries() point into the map: they compile on a named map only, not on
// a temporary one that is gone by the end of the full expression.
template <class Map>
concept CanGet = requires { std::declval<Map>().get(ComponentPreset::kLength); };
template <class Map>
concept CanGetValue = requires { std::declval<Map>().getValue(ComponentPreset::kLength); };
template <class Map>
concept CanListEntries = requires { std::declval<Map>().entries(); };
static_assert(CanGet<const TypedPropertyMap&> && CanGet<TypedPropertyMap&>);
static_assert(!CanGet<TypedPropertyMap> && !CanGet<const TypedPropertyMap>);
static_assert(CanGetValue<const TypedPropertyMap&> && CanGetValue<TypedPropertyMap&>);
static_assert(!CanGetValue<TypedPropertyMap> && !CanGetValue<const TypedPropertyMap>);
static_assert(CanListEntries<const TypedPropertyMap&> && CanListEntries<TypedPropertyMap&>);
static_assert(!CanListEntries<TypedPropertyMap> && !CanListEntries<const TypedPropertyMap>);

// toString(const TypedValue&) takes a TypedValue itself and nothing that converts to one: it is
// neither a catch-all for other values nor, through argument-dependent lookup, the toString() of
// a QtRocket type that has none of its own.
template <class T>
concept HasQtRocketToString = requires(const T& value) { QtRocket::toString(value); };
template <class T>
concept HasToStringByAdl = requires(const T& value) { toString(value); };
static_assert(HasQtRocketToString<QtRocket::TypedValue> && HasToStringByAdl<QtRocket::TypedValue>);
static_assert(HasQtRocketToString<Finish> && HasToStringByAdl<Finish>);
static_assert(!HasQtRocketToString<int> && !HasQtRocketToString<double>);
static_assert(!HasQtRocketToString<float> && !HasQtRocketToString<const char*>);
static_assert(!HasQtRocketToString<std::string>);
static_assert(!HasQtRocketToString<TransitionShape> && !HasToStringByAdl<TransitionShape>);
static_assert(!HasQtRocketToString<ComponentPresetType> && !HasToStringByAdl<ComponentPresetType>);
static_assert(!HasQtRocketToString<Material> && !HasToStringByAdl<Material>);

// put() takes only values of the key's type (or ones that convert to it).
template <class Key, class Value>
concept Puttable =
    requires(TypedPropertyMap map, const Key& key, Value value) { map.put(key, value); };
static_assert(Puttable<TypedKey<double>, double>);
static_assert(!Puttable<TypedKey<double>, std::string>);
static_assert(!Puttable<TypedKey<std::string>, double>);
static_assert(!Puttable<TypedKey<TransitionShape>, Finish>);
static_assert(!Puttable<TypedKey<ComponentPresetType>, TransitionShape>);
static_assert(!Puttable<TypedKey<Material>, double>);
static_assert(Puttable<TypedKey<QtRocket::ManufacturerRef>, const Manufacturer&>);
static_assert(!Puttable<TypedKey<QtRocket::ManufacturerRef>, std::string>);

// Nor the implicit conversions Java's put(TypedKey<T>, T) refuses and C++ would make silently:
// a string or a number for a bool key (it would store true), a bool for any other key, a
// character for a number key.
static_assert(!Puttable<TypedKey<bool>, const char*>);
static_assert(!Puttable<TypedKey<bool>, decltype("false")>);
static_assert(!Puttable<TypedKey<bool>, double>);
static_assert(!Puttable<TypedKey<bool>, int>);
static_assert(!Puttable<TypedKey<double>, bool>);
static_assert(!Puttable<TypedKey<double>, const bool&>);
static_assert(!Puttable<TypedKey<int>, bool>);
static_assert(!Puttable<TypedKey<std::string>, bool>);
static_assert(!Puttable<TypedKey<double>, char>);
static_assert(!Puttable<TypedKey<int>, char>);
static_assert(!Puttable<TypedKey<int>, char16_t>);
// Not a byte count for the image either (std::vector's size constructor is explicit).
static_assert(!Puttable<TypedKey<std::vector<std::byte>>, std::size_t>);
// The conversions that stay: a bool for a bool key however it is held, an int for a double key
// (exact), a C string for a string key.
static_assert(Puttable<TypedKey<bool>, bool>);
static_assert(Puttable<TypedKey<bool>, const bool&>);
static_assert(Puttable<TypedKey<double>, int>);
static_assert(Puttable<TypedKey<int>, int>);
static_assert(Puttable<TypedKey<std::string>, const char*>);
static_assert(Puttable<TypedKey<std::string>, decltype("false")>);

// Only the preset value types make keys.
static_assert(QtRocket::TypedValueType<double>);
static_assert(QtRocket::TypedValueType<std::vector<std::byte>>);
static_assert(!QtRocket::TypedValueType<float>);
static_assert(!QtRocket::TypedValueType<const char*>);

/// The value of @p key in @p map, or nullopt when it has none.
template <class T>
[[nodiscard]] std::optional<T> valueOf(const TypedPropertyMap& map, const TypedKey<T>& key)
{
    const T* value = map.get(key);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    return *value;
}

[[nodiscard]] Material testMaterial()
{
    return Material::newMaterial(Material::Type::BULK, "test", 2.0, true);
}

TEST(TypedPropertyMap, StartsEmpty)
{
    const TypedPropertyMap map;
    EXPECT_TRUE(map.isEmpty());
    EXPECT_EQ(map.size(), 0U);
    EXPECT_FALSE(map.containsKey(ComponentPreset::kLength));
    EXPECT_EQ(map.get(ComponentPreset::kLength), nullptr);
    EXPECT_TRUE(map.keySet().empty());
    EXPECT_TRUE(map.entries().empty());
    EXPECT_EQ(map.toString(), "TypedPropertyMap: { }");
}

TEST(TypedPropertyMap, EveryValueTypeRoundTrips)
{
    TypedPropertyMap    map;
    const Manufacturer& estes = Manufacturer::getManufacturer("Estes");
    map.put(ComponentPreset::kLegacy, true);
    map.put(ComponentPreset::kLineCount, 8);
    map.put(ComponentPreset::kLength, 0.25);
    map.put(ComponentPreset::kPartNo, "BT-20");
    map.put(ComponentPreset::kManufacturer, estes);
    map.put(ComponentPreset::kType, ComponentPresetType::NOSE_CONE);
    map.put(ComponentPreset::kShape, TransitionShape::HAACK);
    map.put(ComponentPreset::kMaterial, testMaterial());
    map.put(ComponentPreset::kFinish, Finish::POLISHED);
    map.put(ComponentPreset::kImage, std::vector<std::byte>{std::byte{1}, std::byte{0xFF}});

    EXPECT_EQ(map.size(), 10U);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLegacy), true);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLineCount), 8);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLength), 0.25);
    EXPECT_EQ(valueOf(map, ComponentPreset::kPartNo), "BT-20");
    EXPECT_EQ(valueOf(map, ComponentPreset::kType), ComponentPresetType::NOSE_CONE);
    EXPECT_EQ(valueOf(map, ComponentPreset::kShape), TransitionShape::HAACK);
    EXPECT_EQ(valueOf(map, ComponentPreset::kFinish), Finish::POLISHED);

    const QtRocket::ManufacturerRef* manufacturer = map.get(ComponentPreset::kManufacturer);
    ASSERT_NE(manufacturer, nullptr);
    EXPECT_EQ(&manufacturer->get(), &estes);
    const Material* material = map.get(ComponentPreset::kMaterial);
    ASSERT_NE(material, nullptr);
    EXPECT_EQ(material->getName(), "test");
    EXPECT_EQ(material->getDensity(), 2.0);
    const std::vector<std::byte>* image = map.get(ComponentPreset::kImage);
    ASSERT_NE(image, nullptr);
    EXPECT_EQ(image->size(), 2U);
}

TEST(TypedPropertyMap, KeysOfTheSameTypeDoNotShareValues)
{
    TypedPropertyMap map;
    map.put(ComponentPreset::kLength, 0.25);
    map.put(ComponentPreset::kMaterial, testMaterial());
    map.put(ComponentPreset::kShape, TransitionShape::HAACK);
    EXPECT_EQ(map.get(ComponentPreset::kWidth), nullptr);
    EXPECT_EQ(map.get(ComponentPreset::kLineMaterial), nullptr);
    EXPECT_EQ(map.get(ComponentPreset::kCanopyShape), nullptr);
}

TEST(TypedPropertyMap, KeepsInsertionOrderAndReplacesInPlace)
{
    TypedPropertyMap map;
    map.put(ComponentPreset::kType, ComponentPresetType::BODY_TUBE);
    map.put(ComponentPreset::kLength, 1.0);
    map.put(ComponentPreset::kOuterDiameter, 2.0);
    map.put(ComponentPreset::kLength, 3.0);  // LinkedHashMap: the key keeps its place

    const std::vector<AnyTypedKey> keys = map.keySet();
    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], ComponentPreset::kType);
    EXPECT_EQ(keys[1], ComponentPreset::kLength);
    EXPECT_EQ(keys[2], ComponentPreset::kOuterDiameter);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLength), 3.0);
    ASSERT_EQ(map.entries().size(), 3U);
    EXPECT_EQ(map.entries()[1].key.getName(), "Length");
    EXPECT_EQ(std::get<double>(map.entries()[1].value), 3.0);
}

TEST(TypedPropertyMap, RemoveAndClear)
{
    TypedPropertyMap map;
    map.put(ComponentPreset::kLength, 1.0);
    map.put(ComponentPreset::kWidth, 2.0);
    map.put(ComponentPreset::kThickness, 3.0);

    EXPECT_TRUE(map.remove(ComponentPreset::kWidth));
    EXPECT_FALSE(map.remove(ComponentPreset::kWidth));
    EXPECT_FALSE(map.containsKey(ComponentPreset::kWidth));
    ASSERT_EQ(map.keySet().size(), 2U);
    EXPECT_EQ(map.keySet()[0], ComponentPreset::kLength);
    EXPECT_EQ(map.keySet()[1], ComponentPreset::kThickness);

    // A re-added key goes to the end.
    map.put(ComponentPreset::kWidth, 4.0);
    EXPECT_EQ(map.keySet()[2], ComponentPreset::kWidth);

    map.clear();
    EXPECT_TRUE(map.isEmpty());
}

TEST(TypedPropertyMap, PutAllMergesInOrder)
{
    TypedPropertyMap first;
    first.put(ComponentPreset::kLength, 1.0);
    first.put(ComponentPreset::kWidth, 2.0);

    TypedPropertyMap second;
    second.put(ComponentPreset::kThickness, 3.0);
    second.put(ComponentPreset::kLength, 4.0);

    first.putAll(second);
    const std::vector<AnyTypedKey> keys = first.keySet();
    ASSERT_EQ(keys.size(), 3U);
    EXPECT_EQ(keys[0], ComponentPreset::kLength);
    EXPECT_EQ(keys[1], ComponentPreset::kWidth);
    EXPECT_EQ(keys[2], ComponentPreset::kThickness);
    EXPECT_EQ(valueOf(first, ComponentPreset::kLength), 4.0);
    EXPECT_EQ(valueOf(first, ComponentPreset::kThickness), 3.0);
    // The source is unchanged.
    EXPECT_EQ(second.size(), 2U);

    // With itself: nothing changes.
    first.putAll(first);
    EXPECT_EQ(first.size(), 3U);
    EXPECT_EQ(valueOf(first, ComponentPreset::kLength), 4.0);
}

TEST(TypedPropertyMap, ErasedAccess)
{
    TypedPropertyMap  map;
    const AnyTypedKey length = ComponentPreset::kLength;
    EXPECT_EQ(map.getValue(length), nullptr);

    map.putValue(length, QtRocket::TypedValue(0.5));
    const QtRocket::TypedValue* value = map.getValue(length);
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(std::get<double>(*value), 0.5);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLength), 0.5);

    // Replaced in place.
    map.put(ComponentPreset::kWidth, 1.0);
    map.putValue(length, QtRocket::TypedValue(0.75));
    EXPECT_EQ(map.keySet()[0], ComponentPreset::kLength);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLength), 0.75);

    // A value of another type is a bug (Java would fail later with a ClassCastException).
    EXPECT_THROW(map.putValue(length, QtRocket::TypedValue(3)), QtRocket::BugError);
    EXPECT_THROW(map.putValue(ComponentPreset::kShape, QtRocket::TypedValue(Finish::MIRROR)),
                 QtRocket::BugError);
    EXPECT_EQ(map.size(), 2U);
}

TEST(TypedPropertyMap, CopiesAreIndependent)
{
    TypedPropertyMap original;
    original.put(ComponentPreset::kLength, 1.0);
    TypedPropertyMap copy = original;  // clone()
    copy.put(ComponentPreset::kLength, 2.0);
    copy.put(ComponentPreset::kWidth, 3.0);
    EXPECT_EQ(valueOf(original, ComponentPreset::kLength), 1.0);
    EXPECT_FALSE(original.containsKey(ComponentPreset::kWidth));
    EXPECT_EQ(valueOf(copy, ComponentPreset::kLength), 2.0);
}

TEST(TypedPropertyMap, KeysAreEqualByNameAndType)
{
    // TypedKey.equals: the name and the value type; the unit group does not count.
    constexpr TypedKey<double>      kSameLength{"Length"};
    constexpr TypedKey<int>         kIntLength{"Length"};
    constexpr TypedKey<std::string> kStringLength{"Length", QtRocket::UnitGroupId::LENGTH};
    EXPECT_TRUE(kSameLength == ComponentPreset::kLength);
    EXPECT_FALSE(kIntLength == ComponentPreset::kLength);
    EXPECT_FALSE(kStringLength == ComponentPreset::kLength);
    EXPECT_FALSE(ComponentPreset::kWidth == ComponentPreset::kLength);
    // SIDES and LINE_COUNT share a type; CANOPY_SHAPE and SHAPE too.
    EXPECT_FALSE(ComponentPreset::kSides == ComponentPreset::kLineCount);
    EXPECT_FALSE(ComponentPreset::kCanopyShape == ComponentPreset::kShape);

    TypedPropertyMap map;
    map.put(ComponentPreset::kLength, 1.0);
    // An equal key finds the entry; a key of another type does not.
    EXPECT_TRUE(map.containsKey(kSameLength));
    EXPECT_EQ(valueOf(map, kSameLength), 1.0);
    EXPECT_FALSE(map.containsKey(kIntLength));
    EXPECT_EQ(map.get(kIntLength), nullptr);
    map.put(kIntLength, 7);
    EXPECT_EQ(map.size(), 2U);
    EXPECT_EQ(valueOf(map, kIntLength), 7);
    EXPECT_EQ(valueOf(map, ComponentPreset::kLength), 1.0);
}

TEST(TypedPropertyMap, KeyAccessors)
{
    EXPECT_EQ(ComponentPreset::kOuterDiameter.getName(), "OuterDiameter");
    EXPECT_EQ(ComponentPreset::kOuterDiameter.getUnitGroup(), QtRocket::UnitGroupId::LENGTH);
    EXPECT_EQ(ComponentPreset::kPartNo.getUnitGroup(), std::nullopt);
    EXPECT_EQ(ComponentPreset::kLength.toString(), "TypedKey [name=Length]");
    EXPECT_TRUE(ComponentPreset::kLength.hasType<double>());
    EXPECT_FALSE(ComponentPreset::kLength.hasType<int>());
    EXPECT_TRUE(ComponentPreset::kImage.hasType<std::vector<std::byte>>());
    const AnyTypedKey erased = ComponentPreset::kSides;
    EXPECT_TRUE(erased.hasType<int>());
    EXPECT_EQ(erased.getName(), "Sides");
}

TEST(TypedPropertyMap, ToStringMatchesJava)
{
    // Pinned from OpenRocket: the map a CENTERING_RING spec builds.
    TypedPropertyMap map;
    map.put(ComponentPreset::kType, ComponentPresetType::CENTERING_RING);
    map.put(ComponentPreset::kManufacturer, Manufacturer::getManufacturer("Estes"));
    map.put(ComponentPreset::kPartNo, "RA-2050");
    map.put(ComponentPreset::kLength, 0.003);
    map.put(ComponentPreset::kOuterDiameter, 0.0332);
    map.put(ComponentPreset::kThickness, 0.0045);
    EXPECT_EQ(map.toString(),
              "TypedPropertyMap: { TypedKey [name=Type] => CENTERING_RINGTypedKey "
              "[name=Manufacturer] => EstesTypedKey [name=PartNo] => RA-2050TypedKey [name=Length] "
              "=> 0.003TypedKey [name=OuterDiameter] => 0.0332TypedKey [name=Thickness] => "
              "0.0045}");
}

TEST(TypedPropertyMap, ValuesAsJavaWritesThem)
{
    using QtRocket::toString;
    using QtRocket::TypedValue;
    // A finish is written in the default roughness unit: start from (and restore) the defaults.
    const QtRocket::Test::DefaultUnitsGuard defaults;
    EXPECT_EQ(toString(TypedValue(true)), "true");
    EXPECT_EQ(toString(TypedValue(false)), "false");
    EXPECT_EQ(toString(TypedValue(-8)), "-8");
    EXPECT_EQ(toString(TypedValue(1e-5)), "1.0E-5");
    EXPECT_EQ(toString(TypedValue(2.0)), "2.0");
    EXPECT_EQ(toString(TypedValue(std::string("x|y"))), "x|y");
    EXPECT_EQ(toString(TypedValue(QtRocket::ManufacturerRef(Manufacturer::getManufacturer("CTI")))),
              "Cesaroni Technology Inc.");
    EXPECT_EQ(toString(TypedValue(ComponentPresetType::BULK_HEAD)), "BULK_HEAD");
    EXPECT_EQ(toString(TypedValue(TransitionShape::POWER)), "Power series");
    EXPECT_EQ(toString(TypedValue(Finish::NORMAL)), "Regular paint (60 \xC2\xB5m)");
    ASSERT_TRUE(QtRocket::unitGroup(QtRocket::UnitGroupId::ROUGHNESS).setDefaultUnit("mil"));
    EXPECT_EQ(toString(TypedValue(Finish::NORMAL)), "Regular paint (2.36 mil)");
    const Material material = Material::newMaterial(Material::Type::SURFACE, "Ripstop nylon", 0.067,
                                                    QtRocket::MaterialGroup::FABRICS, false);
    EXPECT_EQ(toString(TypedValue(material)), material.toString());
    // Deviation: Java prints "[B@" and an identity hash.
    EXPECT_EQ(toString(TypedValue(std::vector<std::byte>(3))), "byte[3]");
}

}  // namespace
