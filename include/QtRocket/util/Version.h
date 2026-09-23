#pragma once

#include <string_view>

namespace QtRocket
{

/// The project version, e.g. "0.1.0" (project(VERSION) in CMakeLists.txt).
[[nodiscard]] std::string_view version() noexcept;

/// The "creator" string written into saved files, e.g. "QtRocket 0.1.0".
[[nodiscard]] std::string_view creatorString() noexcept;

}  // namespace QtRocket
