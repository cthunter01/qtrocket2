#include "QtRocket/util/Config.h"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FloatingDecimal.h"

namespace
{

using QtRocket::BigDecimal;
using QtRocket::BugError;
using QtRocket::Config;

using List  = Config::List;
using Value = Config::Value;

/// The exact decimal expansion of the double pi: Java's new BigDecimal(Math.PI), which TestConfig
/// stores.
constexpr std::string_view kPiExpansion = "3.141592653589793115997963468544185161590576171875";

/// The BigDecimal @p text parses to; zero (and a failed expectation) when it does not parse.
[[nodiscard]] BigDecimal big(std::string_view text)
{
    const std::optional<BigDecimal> value = BigDecimal::parse(text);
    EXPECT_TRUE(value.has_value()) << text;
    return value.value_or(BigDecimal{});
}

/// Whether the value of @p key is stored with the type @p T.
template <typename T>
[[nodiscard]] bool holds(const Config& config, std::string_view key)
{
    const std::optional<Value> value = config.get(key);
    return value.has_value() && std::holds_alternative<T>(value->variant());
}

/// intValue(), longValue() and Double.toString(doubleValue()) of @p value, as ConfigProbe prints
/// Config.getInt(), getLong() and getDouble() of a stored number; "none" for a value that is no
/// number.
[[nodiscard]] std::string numbers(const Value& value)
{
    const std::optional<std::int32_t> intValue    = value.intValue();
    const std::optional<std::int64_t> longValue   = value.longValue();
    const std::optional<double>       doubleValue = value.doubleValue();
    if (!intValue.has_value() || !longValue.has_value() || !doubleValue.has_value())
    {
        return "none";
    }
    return std::format("{} {} {}", *intValue, *longValue,
                       QtRocket::FloatingDecimal::toJavaFormatString(*doubleValue));
}

/// Whether config.put("key", value) compiles for a value of type @p T.
template <typename T>
concept Puttable = requires(Config& config, T value) { config.put("key", value); };

// ----------------------------------------------------------------- TestConfig.java

// TestConfig.testDoubles
TEST(Config, Doubles)
{
    Config config;
    config.put("double", std::numbers::pi);
    config.put("bigdecimal", big(kPiExpansion));
    EXPECT_EQ(config.getDouble("double"), std::numbers::pi);
    EXPECT_EQ(config.getDouble("bigdecimal"), std::numbers::pi);
    EXPECT_EQ(config.getInt("double"), 3);
}

// TestConfig.testInts
TEST(Config, Ints)
{
    Config config;
    config.put("int", 123);
    config.put("biginteger", big(kPiExpansion));
    config.put("bigdecimal", big(kPiExpansion));
    EXPECT_EQ(config.getInt("int"), 123);
    EXPECT_EQ(config.getInt("bigdecimal"), 3);
    EXPECT_EQ(config.getInt("biginteger"), 3);
}

// TestConfig.testDefaultValue
TEST(Config, DefaultValue)
{
    const Config config;
    EXPECT_TRUE(config.getBoolean("foo", true));
    EXPECT_EQ(config.getInt("foo", 123), 123);
    EXPECT_EQ(config.getLong("foo", std::int64_t{123}), std::int64_t{123});
    EXPECT_EQ(config.getDouble("foo", 1.23), 1.23);
    EXPECT_EQ(config.getString("foo", "bar"), "bar");
    EXPECT_TRUE(config.getList("foo", List{Value{"foo"}}) == List{Value{"foo"}});
}

// TestConfig.testNullDefaultValue: Java's null default is the getter without a default here.
TEST(Config, NullDefaultValue)
{
    const Config config;
    EXPECT_EQ(config.getBoolean("foo"), std::nullopt);
    EXPECT_EQ(config.getInt("foo"), std::nullopt);
    EXPECT_EQ(config.getLong("foo"), std::nullopt);
    EXPECT_EQ(config.getDouble("foo"), std::nullopt);
    EXPECT_EQ(config.getString("foo"), std::nullopt);
    EXPECT_FALSE(config.getList("foo").has_value());
}

// TestConfig.testStoringList
TEST(Config, StoringList)
{
    Config config;
    List   list;
    list.emplace_back("Foo");
    list.emplace_back(123);
    list.emplace_back(std::numbers::pi);
    list.emplace_back(true);
    config.put("list", list);
    const List expected{Value{"Foo"}, Value{123}, Value{std::numbers::pi}, Value{true}};
    EXPECT_TRUE(config.getList("list") == expected);
}

// TestConfig.testModifyingStoredList
TEST(Config, ModifyingStoredList)
{
    Config config;
    List   list;
    list.emplace_back("Foo");
    list.emplace_back(123);
    list.emplace_back(std::numbers::pi);
    list.emplace_back(true);
    config.put("list", list);
    list.emplace_back("hello");
    const List expected{Value{"Foo"}, Value{123}, Value{std::numbers::pi}, Value{true}};
    EXPECT_TRUE(config.getList("list") == expected);
}

// TestConfig.testModifyingStoredNumber. Java puts an AtomicInteger, a mutable Number that Config
// stores as the BigDecimal of its text; a C++ number is a value to begin with, so both halves of
// that are shown: the variable can change afterwards, and a BigDecimal reads back as an int.
TEST(Config, ModifyingStoredNumber)
{
    Config config;
    int    ai = 100;
    config.put("ai", ai);
    ++ai;
    EXPECT_EQ(config.getInt("ai"), 100);
    EXPECT_EQ(ai, 101);

    config.put("atomicinteger", BigDecimal::valueOf(100));
    EXPECT_TRUE(holds<BigDecimal>(config, "atomicinteger"));
    EXPECT_EQ(config.getInt("atomicinteger"), 100);
}

// TestConfig.testClone: Java's clone() is the copy constructor. Java puts an AtomicInteger, which
// Config stores as the BigDecimal of its text, so the clone copies a BigDecimal entry.
TEST(Config, Clone)
{
    Config config;
    config.put("string", "foo");
    config.put("int", 123);
    config.put("double", std::numbers::pi);

    int ai = 100;
    config.put("atomicinteger", BigDecimal::valueOf(ai));

    List list;
    list.emplace_back("Foo");
    config.put("list", list);

    const Config copy = config;

    config.put("extra", "foo");
    ++ai;

    EXPECT_FALSE(copy.containsKey("extra"));
    EXPECT_EQ(copy.getString("string"), "foo");
    EXPECT_EQ(copy.getInt("int"), 123);
    EXPECT_EQ(copy.getInt("atomicinteger"), 100);
    EXPECT_TRUE(holds<BigDecimal>(copy, "atomicinteger"));
    EXPECT_EQ(copy.getDouble("double"), std::numbers::pi);
    EXPECT_TRUE(copy.getList("list") == List{Value{"Foo"}});
    EXPECT_EQ(ai, 101);
}

// TestConfig.testStoringNullValue: of Java's four null puts (Boolean, String, Number, List) only
// the String has a C++ counterpart, a null `const char*`; the others are values here.
TEST(Config, StoringNullValue)
{
    Config            config;
    const char* const null = nullptr;
    EXPECT_THROW(config.put("foo", null), BugError);
    EXPECT_THROW(static_cast<void>(Value{null}), BugError);
    // A nullptr is that null String, not the Boolean false.
    EXPECT_THROW(config.put("foo", nullptr), BugError);
    EXPECT_THROW(static_cast<void>(Value{nullptr}), BugError);
    EXPECT_FALSE(config.containsKey("foo"));
}

/// What the member pointers of StoringInvalidTypesDoesNotCompile point into.
struct Thing
{
    int member{0};
};

// TestConfig.testStoringListWithInvalidTypes and testStoringListWithNull (the two have the same
// body in Java: a list holding a java.util.Date is refused with IllegalArgumentException). A List
// holds Values, and a Value cannot be made from another type, so the refusal is the compiler's.
TEST(Config, StoringInvalidTypesDoesNotCompile)
{
    static_assert(!Puttable<std::chrono::system_clock::time_point>);
    static_assert(!std::constructible_from<Value, std::chrono::system_clock::time_point>);
    static_assert(!Puttable<std::vector<std::chrono::system_clock::time_point>>);
    // Java's Character is no Number, and Java has no unsigned integers.
    static_assert(!Puttable<char>);
    static_assert(!Puttable<unsigned int>);
    static_assert(!Puttable<std::uint64_t>);
    // A pointer converts to bool, and must not become a Boolean that way: not the address of a
    // value (a Date in Java's test), not a string of another character type.
    static_assert(!Puttable<int*>);
    static_assert(!Puttable<const int*>);
    static_assert(!Puttable<void*>);
    static_assert(!Puttable<std::string*>);
    static_assert(!Puttable<std::chrono::system_clock::time_point*>);
    static_assert(!Puttable<const wchar_t*>);
    static_assert(!Puttable<const char8_t*>);
    static_assert(!Puttable<const char16_t*>);
    static_assert(!Puttable<const char32_t*>);
    static_assert(!Puttable<void (*)()>);
    static_assert(!Puttable<int Thing::*>);
    static_assert(!Puttable<void (Thing::*)()>);
    static_assert(!std::constructible_from<Value, int*>);
    static_assert(!std::constructible_from<Value, void*>);
    static_assert(!std::constructible_from<Value, std::string*>);
    static_assert(!std::constructible_from<Value, const wchar_t*>);
    // A wide or UTF-8 string literal, as the array it is and as the pointer it decays to.
    static_assert(!std::constructible_from<Value, decltype(L"wide")>);
    static_assert(!std::constructible_from<Value, decltype(u8"utf8")>);
    static_assert(!std::constructible_from<Value, decltype(+L"wide")>);
    static_assert(!std::constructible_from<Value, decltype(+u8"utf8")>);
    static_assert(!std::constructible_from<Value, void (*)()>);
    static_assert(!std::constructible_from<Value, int Thing::*>);
    // Nor does anything else that only converts to bool.
    static_assert(!std::constructible_from<Value, std::true_type>);
    static_assert(!std::constructible_from<Value, std::vector<bool>::reference>);
    // A nullptr is the null String, refused when it is stored (see StoringNullValue).
    static_assert(Puttable<std::nullptr_t>);
    static_assert(std::constructible_from<Value, bool&>);
    static_assert(std::constructible_from<Value, const bool&>);
    // What does compile:
    static_assert(Puttable<bool>);
    static_assert(Puttable<std::int8_t>);
    static_assert(Puttable<std::int16_t>);
    static_assert(Puttable<int>);
    static_assert(Puttable<std::int64_t>);
    static_assert(Puttable<float>);
    static_assert(Puttable<double>);
    static_assert(Puttable<BigDecimal>);
    static_assert(Puttable<std::string>);
    static_assert(Puttable<std::string_view>);
    static_assert(Puttable<const char*>);
    static_assert(Puttable<List>);
    static_assert(Puttable<Value>);
    SUCCEED();
}

// ------------------------------------------------- beyond OpenRocket's tests: the stored types

TEST(Config, AValueKeepsItsJavaType)
{
    Config config;
    config.put("boolean", true);
    config.put("byte", std::int8_t{5});
    config.put("short", std::int16_t{5});
    config.put("integer", 5);
    config.put("long", std::int64_t{5});
    config.put("float", 5.0F);
    config.put("double", 5.0);
    config.put("bigdecimal", big("5"));
    config.put("string", "5");
    config.put("list", List{Value{5}});

    EXPECT_TRUE(holds<bool>(config, "boolean"));
    EXPECT_TRUE(holds<std::int8_t>(config, "byte"));
    EXPECT_TRUE(holds<std::int16_t>(config, "short"));
    EXPECT_TRUE(holds<std::int32_t>(config, "integer"));
    EXPECT_TRUE(holds<std::int64_t>(config, "long"));
    EXPECT_TRUE(holds<float>(config, "float"));
    EXPECT_TRUE(holds<double>(config, "double"));
    EXPECT_TRUE(holds<BigDecimal>(config, "bigdecimal"));
    EXPECT_TRUE(holds<std::string>(config, "string"));
    EXPECT_TRUE(holds<List>(config, "list"));
}

TEST(Config, ABooleanIsMadeFromABoolOnly)
{
    // Whatever kind of bool expression it is; a pointer is none (see
    // StoringInvalidTypesDoesNotCompile).
    Config     config;
    bool       variable = true;
    const bool constant = false;
    const int  one      = 1;
    const int  two      = 2;
    config.put("variable", variable);
    config.put("constant", constant);
    config.put("comparison", one < two);
    config.put("element", bool{std::vector<bool>{true}.front()});
    // The value is stored, not the variable.
    variable = false;
    EXPECT_FALSE(variable);
    EXPECT_TRUE(holds<bool>(config, "variable"));
    EXPECT_TRUE(holds<bool>(config, "constant"));
    EXPECT_TRUE(holds<bool>(config, "comparison"));
    EXPECT_TRUE(holds<bool>(config, "element"));
    EXPECT_EQ(config.getBoolean("variable"), true);
    EXPECT_EQ(config.getBoolean("constant"), false);
    EXPECT_EQ(config.getBoolean("comparison"), true);
    EXPECT_EQ(config.getBoolean("element"), true);
    // A Boolean is no number.
    EXPECT_EQ(config.getInt("variable"), std::nullopt);

    List list;
    list.emplace_back(true);
    list.emplace_back(constant);
    EXPECT_TRUE(list == (List{Value{true}, Value{false}}));
}

TEST(Config, IntegersAreStoredByTheirWidth)
{
    // Whatever the platform calls its 64-bit type, it is a Long; long itself is 64 bits wide on
    // Linux and macOS and 32 on Windows.
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(Value{5LL}.variant()));
    EXPECT_TRUE(std::holds_alternative<std::int64_t>(Value{std::int64_t{5}}.variant()));
    EXPECT_TRUE(std::holds_alternative<std::int32_t>(Value{std::int32_t{5}}.variant()));
    EXPECT_EQ(std::holds_alternative<std::int64_t>(Value{5L}.variant()),
              sizeof(long) == sizeof(std::int64_t));
    EXPECT_EQ(std::holds_alternative<std::int32_t>(Value{5L}.variant()),
              sizeof(long) == sizeof(std::int32_t));
    const short shortValue = 5;
    EXPECT_TRUE(std::holds_alternative<std::int16_t>(Value{shortValue}.variant()));
    const signed char byteValue = 5;
    EXPECT_TRUE(std::holds_alternative<std::int8_t>(Value{byteValue}.variant()));
}

TEST(Config, StringsFromEveryStringType)
{
    Config config;
    config.put("literal", "a");
    config.put("string", std::string("b"));
    config.put("view", std::string_view("c"));
    const char* const pointer = "d";
    config.put("pointer", pointer);
    EXPECT_EQ(config.getString("literal"), "a");
    EXPECT_EQ(config.getString("string"), "b");
    EXPECT_EQ(config.getString("view"), "c");
    EXPECT_EQ(config.getString("pointer"), "d");
    // A string literal is a String, not a Boolean (the pointer-to-bool conversion C++ prefers).
    EXPECT_TRUE(holds<std::string>(config, "literal"));
    EXPECT_EQ(config.getBoolean("literal"), std::nullopt);
}

// ------------------------------------- Java's Number conversions (ConfigProbe: getInt, getLong,
// getDouble of each stored type)

TEST(Config, NumberConversionsOfTheIntegerTypes)
{
    EXPECT_EQ(numbers(Value{std::int8_t{5}}), "5 5 5.0");
    EXPECT_EQ(numbers(Value{std::numeric_limits<std::int8_t>::min()}), "-128 -128 -128.0");
    EXPECT_EQ(numbers(Value{std::int16_t{-300}}), "-300 -300 -300.0");
    EXPECT_EQ(numbers(Value{123}), "123 123 123.0");
    EXPECT_EQ(numbers(Value{std::numeric_limits<std::int32_t>::min()}),
              "-2147483648 -2147483648 -2.147483648E9");
    EXPECT_EQ(numbers(Value{std::int64_t{123}}), "123 123 123.0");
}

TEST(Config, IntValueOfALongKeepsTheLowBits)
{
    EXPECT_EQ(numbers(Value{std::int64_t{2147483648}}), "-2147483648 2147483648 2.147483648E9");
    EXPECT_EQ(numbers(Value{std::int64_t{4294967303}}), "7 4294967303 4.294967303E9");
    EXPECT_EQ(numbers(Value{std::numeric_limits<std::int64_t>::max()}),
              "-1 9223372036854775807 9.223372036854776E18");
    EXPECT_EQ(numbers(Value{std::numeric_limits<std::int64_t>::min()}),
              "0 -9223372036854775808 -9.223372036854776E18");
    // 2^53 + 1 is not a double: the nearest one is 2^53.
    EXPECT_EQ(numbers(Value{std::int64_t{9007199254740993}}),
              "1 9007199254740993 9.007199254740992E15");
}

TEST(Config, NumberConversionsOfAFloat)
{
    // A float widens to the double of the same value; the casts truncate and saturate.
    EXPECT_EQ(numbers(Value{1.5F}), "1 1 1.5");
    EXPECT_EQ(numbers(Value{0.1F}), "0 0 0.10000000149011612");
    EXPECT_EQ(numbers(Value{-2.9F}), "-2 -2 -2.9000000953674316");
    EXPECT_EQ(numbers(Value{1e20F}), "2147483647 9223372036854775807 1.0000000200408773E20");
    EXPECT_EQ(numbers(Value{-1e20F}), "-2147483648 -9223372036854775808 -1.0000000200408773E20");
    EXPECT_EQ(numbers(Value{std::numeric_limits<float>::quiet_NaN()}), "0 0 NaN");
    EXPECT_EQ(numbers(Value{std::numeric_limits<float>::infinity()}),
              "2147483647 9223372036854775807 Infinity");
}

TEST(Config, NumberConversionsOfADouble)
{
    EXPECT_EQ(numbers(Value{std::numbers::pi}), "3 3 3.141592653589793");
    EXPECT_EQ(numbers(Value{-2.9}), "-2 -2 -2.9");
    EXPECT_EQ(numbers(Value{2.9}), "2 2 2.9");
    EXPECT_EQ(numbers(Value{1e10}), "2147483647 10000000000 1.0E10");
    EXPECT_EQ(numbers(Value{-1e10}), "-2147483648 -10000000000 -1.0E10");
    EXPECT_EQ(numbers(Value{1e19}), "2147483647 9223372036854775807 1.0E19");
    EXPECT_EQ(numbers(Value{-1e19}), "-2147483648 -9223372036854775808 -1.0E19");
    EXPECT_EQ(numbers(Value{std::numeric_limits<double>::quiet_NaN()}), "0 0 NaN");
    EXPECT_EQ(numbers(Value{std::numeric_limits<double>::infinity()}),
              "2147483647 9223372036854775807 Infinity");
    EXPECT_EQ(numbers(Value{-std::numeric_limits<double>::infinity()}),
              "-2147483648 -9223372036854775808 -Infinity");
    EXPECT_EQ(numbers(Value{-0.0}), "0 0 -0.0");
}

TEST(Config, NumberConversionsOfABigDecimal)
{
    EXPECT_EQ(numbers(Value{big(kPiExpansion)}), "3 3 3.141592653589793");
    EXPECT_EQ(numbers(Value{big("0.10")}), "0 0 0.1");
    EXPECT_EQ(numbers(Value{big("-1E-400")}), "0 0 -0.0");
    // Unlike a double, a BigDecimal beyond the int and long ranges keeps its low bits.
    EXPECT_EQ(numbers(Value{big("1E+30")}), "1073741824 5076944270305263616 1.0E30");
    // 2^70 and -(2^70) - 5, which Java's reader hands over as BigIntegers.
    EXPECT_EQ(numbers(Value{big("1180591620717411303424")}), "0 0 1.1805916207174113E21");
    EXPECT_EQ(numbers(Value{big("-1180591620717411303429")}), "-5 -5 -1.1805916207174113E21");
    EXPECT_EQ(numbers(Value{big("100")}), "100 100 100.0");
}

TEST(Config, OnlyNumbersAreNumbers)
{
    EXPECT_TRUE(Value{std::int8_t{1}}.isNumber());
    EXPECT_TRUE(Value{std::int16_t{1}}.isNumber());
    EXPECT_TRUE(Value{1}.isNumber());
    EXPECT_TRUE(Value{std::int64_t{1}}.isNumber());
    EXPECT_TRUE(Value{1.0F}.isNumber());
    EXPECT_TRUE(Value{1.0}.isNumber());
    EXPECT_TRUE(Value{big("1")}.isNumber());
    EXPECT_FALSE(Value{true}.isNumber());
    EXPECT_FALSE(Value{"1"}.isNumber());
    EXPECT_FALSE(Value{List{Value{1}}}.isNumber());
    EXPECT_EQ(numbers(Value{true}), "none");
    EXPECT_EQ(numbers(Value{"12"}), "none");
    EXPECT_EQ(numbers(Value{List{Value{1}}}), "none");
}

TEST(Config, AGetterOfAnotherTypeGivesTheDefault)
{
    // ConfigProbe: "7 8 9.0 null false d null true 12"
    Config config;
    config.put("s", "12");
    config.put("b", true);
    config.put("l", List{Value{1}});
    EXPECT_EQ(config.getInt("s", 7), 7);
    EXPECT_EQ(config.getLong("b", 8), 8);
    EXPECT_EQ(config.getDouble("l", 9.0), 9.0);
    EXPECT_EQ(config.getInt("missing"), std::nullopt);
    EXPECT_FALSE(config.getBoolean("s", false));
    EXPECT_EQ(config.getString("b", "d"), "d");
    EXPECT_FALSE(config.getList("s").has_value());
    EXPECT_TRUE(config.getBoolean("b", false));
    EXPECT_EQ(config.getString("s", "d"), "12");

    EXPECT_EQ(config.getInt("s"), std::nullopt);
    EXPECT_EQ(config.getLong("b"), std::nullopt);
    EXPECT_EQ(config.getDouble("l"), std::nullopt);
    EXPECT_EQ(config.getBoolean("s"), std::nullopt);
    EXPECT_EQ(config.getString("b"), std::nullopt);
    EXPECT_EQ(config.getString("l"), std::nullopt);
    EXPECT_TRUE(config.getList("b", List{Value{2}}) == List{Value{2}});
    EXPECT_TRUE(config.getList("l", List{Value{2}}) == List{Value{1}});
}

TEST(Config, TheNumberGettersConvertWhateverNumberIsStored)
{
    Config config;
    config.put("double", 2.9);
    config.put("long", std::int64_t{4294967303});
    config.put("int", 7);
    EXPECT_EQ(config.getInt("double", 0), 2);
    EXPECT_EQ(config.getLong("double", 0), 2);
    EXPECT_EQ(config.getInt("long", 0), 7);
    EXPECT_EQ(config.getDouble("long", 0), 4294967303.0);
    EXPECT_EQ(config.getDouble("int", 0), 7.0);
    EXPECT_EQ(config.getLong("int", 0), 7);
}

// ----------------------------------------------------------------- keys and their order

TEST(Config, KeysKeepTheOrderTheyWereFirstPutIn)
{
    // ConfigProbe: [zeta, alpha, mid, beta] zeta=2.5 Double
    Config config;
    config.put("zeta", 1);
    config.put("alpha", "a");
    config.put("mid", true);
    config.put("zeta", 2.5);
    config.put("beta", List{Value{"x"}, Value{1}, Value{2.5}, Value{false},
                            Value{List{Value{std::int64_t{7}}}}});
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"zeta", "alpha", "mid", "beta"}));
    // Putting a key again replaces the value, type included, and keeps the key's place.
    EXPECT_TRUE(holds<double>(config, "zeta"));
    EXPECT_EQ(config.getDouble("zeta"), 2.5);
    EXPECT_TRUE(config.containsKey("beta"));
    EXPECT_FALSE(config.containsKey("nope"));
    EXPECT_FALSE(config.containsKey("Beta"));

    const Config copy = config;
    config.put("gamma", 1);
    EXPECT_EQ(copy.keySet(), (std::vector<std::string>{"zeta", "alpha", "mid", "beta"}));
    EXPECT_EQ(config.keySet(), (std::vector<std::string>{"zeta", "alpha", "mid", "beta", "gamma"}));
}

TEST(Config, AnEmptyConfig)
{
    const Config config;
    EXPECT_TRUE(config.keySet().empty());
    EXPECT_FALSE(config.containsKey(""));
    EXPECT_FALSE(config.get("anything").has_value());
}

TEST(Config, GetReturnsTheValueOrTheDefault)
{
    Config config;
    config.put("n", 5);
    EXPECT_TRUE(config.get("n").has_value());
    EXPECT_TRUE(config.get("n") == Value{5});
    EXPECT_FALSE(config.get("missing").has_value());
    EXPECT_TRUE(config.get("n", Value{"default"}) == Value{5});
    EXPECT_TRUE(config.get("missing", Value{"default"}) == Value{"default"});
    // An empty key is a key like any other.
    config.put("", "empty");
    EXPECT_EQ(config.getString(""), "empty");
}

TEST(Config, PutCopiesAndGetListCopies)
{
    Config config;
    List   inner{Value{1}};
    List   outer{Value{inner}, Value{"x"}};
    config.put("list", outer);
    inner.emplace_back(2);
    outer.emplace_back(3);
    const List expected{Value{List{Value{1}}}, Value{"x"}};
    EXPECT_TRUE(config.getList("list") == expected);

    // Deviation: Java hands out the stored list itself; here the caller gets a copy.
    List got = config.getList("list", List{});
    EXPECT_TRUE(got == expected);
    got.clear();
    EXPECT_TRUE(config.getList("list") == expected);
}

TEST(Config, ACopyIsDeep)
{
    Config config;
    config.put("list", List{Value{List{Value{"deep"}}}});
    Config copy = config;
    copy.put("list", List{Value{"changed"}});
    copy.put("more", 1);
    EXPECT_TRUE(config.getList("list") == List{Value{List{Value{"deep"}}}});
    EXPECT_FALSE(config.containsKey("more"));

    Config assigned;
    assigned.put("old", 1);
    assigned = config;
    EXPECT_EQ(assigned.keySet(), (std::vector<std::string>{"list"}));
}

// --------------------------------- Java's equals() of the stored objects (ConfigProbe), which
// document/Simulation's configEqual() compares two extensions' configurations with

TEST(Config, ValuesOfDifferentTypesAreNeverEqual)
{
    EXPECT_TRUE(Value{5} == Value{5});
    EXPECT_FALSE(Value{5} == Value{std::int64_t{5}});
    EXPECT_FALSE(Value{5} == Value{5.0});
    EXPECT_TRUE(Value{std::int64_t{5}} == Value{std::int64_t{5}});
    EXPECT_FALSE(Value{std::int8_t{5}} == Value{std::int16_t{5}});
    EXPECT_TRUE(Value{5.0} == Value{5.0});
    EXPECT_FALSE(Value{5.0F} == Value{5.0});
    EXPECT_FALSE(Value{big("1")} == Value{1});
    EXPECT_FALSE(Value{true} == Value{"true"});
    EXPECT_FALSE(Value{1} == Value{true});
    EXPECT_FALSE(Value{5} == Value{6});
    EXPECT_TRUE(Value{5} != Value{6});
}

TEST(Config, FloatingValuesCompareAsJavaEqualsDoes)
{
    const double nan      = std::numeric_limits<double>::quiet_NaN();
    const float  floatNan = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(Value{nan} == Value{nan});
    EXPECT_FALSE(Value{0.0} == Value{-0.0});
    EXPECT_TRUE(Value{floatNan} == Value{floatNan});
    EXPECT_FALSE(Value{0.0F} == Value{-0.0F});
    EXPECT_TRUE(Value{-0.0} == Value{-0.0});
    EXPECT_TRUE(Value{1.5F} == Value{1.5F});
    EXPECT_FALSE(Value{1.5} == Value{2.5});
}

TEST(Config, BigDecimalStringAndBooleanValues)
{
    EXPECT_FALSE(Value{big("1.0")} == Value{big("1.00")});
    EXPECT_TRUE(Value{big("1.0")} == Value{big("1.0")});
    EXPECT_TRUE(Value{"a"} == Value{"a"});
    EXPECT_FALSE(Value{"a"} == Value{"b"});
    EXPECT_FALSE(Value{"a"} == Value{"A"});
    EXPECT_TRUE(Value{true} == Value{true});
    EXPECT_FALSE(Value{true} == Value{false});
}

TEST(Config, ListsCompareElementByElement)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Value  intAndString{List{Value{1}, Value{"a"}}};
    const Value  sameAgain{List{Value{1}, Value{"a"}}};
    const Value  longAndString{List{Value{std::int64_t{1}}, Value{"a"}}};
    const Value  stringAndInt{List{Value{"a"}, Value{1}}};
    const Value  one{List{Value{1}}};
    const Value  oneAndTwo{List{Value{1}, Value{2}}};
    const Value  nestedNan{List{Value{List{Value{nan}}}}};
    const Value  nestedNanAgain{List{Value{List{Value{nan}}}}};
    EXPECT_TRUE(intAndString == sameAgain);
    EXPECT_FALSE(intAndString == longAndString);
    EXPECT_FALSE(one == oneAndTwo);
    EXPECT_TRUE(Value{List{}} == Value{List{}});
    EXPECT_TRUE(nestedNan == nestedNanAgain);
    EXPECT_FALSE(one == Value{1});
    EXPECT_FALSE(stringAndInt == intAndString);
}

// document/Simulation.configEqual(): `a.keySet().equals(b.keySet())` compares two Sets, so the
// order the keys were put in does not count (SameEntriesProbe prints the same answers).
TEST(Config, SameEntriesIgnoresTheOrderOfTheKeys)
{
    Config a;
    a.put("altitude", 100.0);
    a.put("name", "x");
    a.put("list", List{Value{1}, Value{"a"}});
    Config b;
    b.put("list", List{Value{1}, Value{"a"}});
    b.put("name", "x");
    b.put("altitude", 100.0);
    EXPECT_NE(a.keySet(), b.keySet());
    EXPECT_TRUE(a.sameEntries(b));
    EXPECT_TRUE(b.sameEntries(a));
    EXPECT_TRUE(a.sameEntries(a));
    EXPECT_TRUE(Config{}.sameEntries(Config{}));
    // A copy has the same entries, in the same order.
    const Config copy = a;
    EXPECT_TRUE(copy.sameEntries(a));
    EXPECT_EQ(copy.keySet(), a.keySet());
}

TEST(Config, SameEntriesComparesTheKeys)
{
    Config a;
    a.put("k", 1);
    a.put("m", 2);
    Config b;
    b.put("k", 1);
    // A key more on either side.
    EXPECT_FALSE(a.sameEntries(b));
    EXPECT_FALSE(b.sameEntries(a));
    EXPECT_FALSE(a.sameEntries(Config{}));
    EXPECT_FALSE(Config{}.sameEntries(a));
    // As many keys, but other ones.
    b.put("n", 2);
    EXPECT_FALSE(a.sameEntries(b));
    EXPECT_FALSE(b.sameEntries(a));
    // Keys are compared exactly.
    Config c;
    c.put("K", 1);
    c.put("m", 2);
    EXPECT_FALSE(a.sameEntries(c));
}

TEST(Config, SameEntriesComparesTheValuesWithTheirTypes)
{
    Config a;
    a.put("k", 5);
    Config b;
    b.put("k", 5.0);
    // The Integer 5 is not the Double 5.0.
    EXPECT_FALSE(a.sameEntries(b));
    EXPECT_FALSE(b.sameEntries(a));
    b.put("k", std::int64_t{5});
    EXPECT_FALSE(a.sameEntries(b));
    b.put("k", 6);
    EXPECT_FALSE(a.sameEntries(b));
    b.put("k", 5);
    EXPECT_TRUE(a.sameEntries(b));

    // Double.equals(): a NaN equals a NaN, and 0.0 does not equal -0.0.
    a.put("x", std::numeric_limits<double>::quiet_NaN());
    b.put("x", std::numeric_limits<double>::quiet_NaN());
    EXPECT_TRUE(a.sameEntries(b));
    a.put("x", 0.0);
    b.put("x", -0.0);
    EXPECT_FALSE(a.sameEntries(b));
    b.put("x", 0.0);

    // Lists compare element by element, and a BigDecimal with its scale.
    a.put("list", List{Value{1}, Value{List{Value{"deep"}}}});
    b.put("list", List{Value{1}, Value{List{Value{"deeper"}}}});
    EXPECT_FALSE(a.sameEntries(b));
    b.put("list", List{Value{1}, Value{List{Value{"deep"}}}});
    EXPECT_TRUE(a.sameEntries(b));
    a.put("big", big("1.0"));
    b.put("big", big("1.00"));
    EXPECT_FALSE(a.sameEntries(b));
}

TEST(Config, OptionalValuesCompareAsObjectsEqualsDoes)
{
    // Objects.equals(a.get(key, null), b.get(key, null)): two absent values are equal.
    Config a;
    Config b;
    EXPECT_TRUE(a.get("k") == b.get("k"));
    a.put("k", 1);
    EXPECT_FALSE(a.get("k") == b.get("k"));
    b.put("k", 1);
    EXPECT_TRUE(a.get("k") == b.get("k"));
    b.put("k", 1.0);
    EXPECT_FALSE(a.get("k") == b.get("k"));
}

}  // namespace
