#pragma once

// SHA-256 under the name the golden tests call it by. The digest itself is the library's
// (QtRocket/util/Sha256.h, where it moved when the scripting extension came to need it); a test
// includes that header. This one is left for tests/core/goldens/goldens_schema_tests.cpp alone:
// whoever next changes that file includes the library's header there and removes this one.

#include <cstddef>
#include <span>
#include <string>

#include "QtRocket/util/Sha256.h"

namespace QtRocket::Test
{

/// QtRocket::sha256Hex(@p data).
[[nodiscard]] inline std::string sha256Hex(std::span<const std::byte> data)
{
    return QtRocket::sha256Hex(data);
}

}  // namespace QtRocket::Test
