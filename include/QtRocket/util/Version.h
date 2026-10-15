#pragma once

#include <string_view>

namespace QtRocket
{

/// The project version, e.g. "0.2.0": project(VERSION) in the top-level CMakeLists.txt.
[[nodiscard]] std::string_view version() noexcept;

/// The "creator" string written into saved files and printed by QtRocket_cli --version, e.g.
/// "QtRocket 0.2.0".
[[nodiscard]] std::string_view creatorString() noexcept;

}  // namespace QtRocket
