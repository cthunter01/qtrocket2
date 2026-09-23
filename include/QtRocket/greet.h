#pragma once

#include <string>
#include <string_view>

namespace QtRocket
{

/// Returns a greeting for @p name, e.g. "Hello, world!".
[[nodiscard]] std::string greet(std::string_view name);

}  // namespace QtRocket
