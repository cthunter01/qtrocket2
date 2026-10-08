#pragma once

// A Config value as OpenRocket's probes print one, for the tests of the entry handlers.
// Test-only.

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <variant>

#include "QtRocket/util/BigDecimal.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket::Test
{

/// @p value with its Java type: "Boolean true", "Integer 5", "Long 4294967296", "Double 2.5"
/// (as Java prints a double), "BigDecimal 2.50 unscaled=250 scale=2", "String text" and
/// "List[Integer 1, String two, ]".
[[nodiscard]] inline std::string describe(const Config::Value& value)
{
    struct Visitor
    {
        [[nodiscard]] std::string operator()(bool v) const { return std::format("Boolean {}", v); }
        [[nodiscard]] std::string operator()(std::int8_t v) const
        {
            return std::format("Byte {}", static_cast<int>(v));
        }
        [[nodiscard]] std::string operator()(std::int16_t v) const
        {
            return std::format("Short {}", v);
        }
        [[nodiscard]] std::string operator()(std::int32_t v) const
        {
            return std::format("Integer {}", v);
        }
        [[nodiscard]] std::string operator()(std::int64_t v) const
        {
            return std::format("Long {}", v);
        }
        [[nodiscard]] std::string operator()(float v) const
        {
            return "Float " + Strings::javaDoubleToString(static_cast<double>(v));
        }
        [[nodiscard]] std::string operator()(double v) const
        {
            return "Double " + Strings::javaDoubleToString(v);
        }
        [[nodiscard]] std::string operator()(const BigDecimal& v) const
        {
            return std::format("BigDecimal {} unscaled={} scale={}", v.toString(),
                               v.unscaledValue(), v.scale());
        }
        [[nodiscard]] std::string operator()(const std::string& v) const { return "String " + v; }
        [[nodiscard]] std::string operator()(const Config::List& list) const
        {
            std::string text = "List[";
            for (const Config::Value& element : list)
            {
                text += describe(element) + ", ";
            }
            return text + "]";
        }
    };
    return std::visit(Visitor{}, value.variant());
}

/// The entries of @p config in the order of its keys: "{key = <value>; key = <value>; }".
[[nodiscard]] inline std::string describe(const Config& config)
{
    std::string text = "{";
    for (const std::string& key : config.keySet())
    {
        if (const std::optional<Config::Value> value = config.get(key))
        {
            text += key + " = " + describe(*value) + "; ";
        }
    }
    return text + "}";
}

}  // namespace QtRocket::Test
