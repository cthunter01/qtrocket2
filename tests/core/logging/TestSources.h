#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/logging/Message.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Test
{

/// The id of a test component: a short tag such as "fs-1" packed into a Uuid byte for byte, so
/// that the same tag is the same component and another tag another one, without spelling UUIDs
/// in the tests. A tag is at most eight characters (checked): a longer one would lose its
/// leading bytes and collide with its own eight-character suffix.
inline Uuid componentId(std::string_view tag)
{
    QTROCKET_ASSERT(tag.size() <= 8);
    std::uint64_t bits = 0;
    for (const char c : tag)
    {
        bits = (bits << 8U) | static_cast<unsigned char>(c);
    }
    return Uuid{bits, 0U};
}

/// A message source (Java: a RocketComponent) for a test, see componentId().
inline MessageSource source(std::string_view tag, std::string name)
{
    return MessageSource{componentId(tag), std::move(name)};
}

}  // namespace QtRocket::Test
