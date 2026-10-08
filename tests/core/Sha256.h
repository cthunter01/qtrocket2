#pragma once

// SHA-256 for the tests: data files whose exact bytes matter (tests/data/ork) are pinned by
// their size and their SHA-256. Test-only: the library digests with MD5 (QtRocket/util/Md5.h),
// which is what OpenRocket writes into its files.

#include <cstddef>
#include <span>
#include <string>

namespace QtRocket::Test
{

/// The SHA-256 digest (FIPS 180-4) of @p data as 64 lower-case hexadecimal digits, as sha256sum
/// prints it.
[[nodiscard]] std::string sha256Hex(std::span<const std::byte> data);

}  // namespace QtRocket::Test
