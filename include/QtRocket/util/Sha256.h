#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace QtRocket
{

/// The SHA-256 digest (FIPS 180-4) of @p data as 64 lower-case hexadecimal digits, as sha256sum
/// prints it. A self-contained implementation (no OpenSSL), beside Md5.
///
/// It replaces java.security.MessageDigest "SHA-256" where OpenRocket uses it: the hash by
/// which ScriptingUtil knows the scripts it trusts (ScriptingExtension). The digests OpenRocket
/// writes into its files, of motors and of component presets, are MD5 (Md5.h). The tests pin
/// data files whose exact bytes matter by this digest.
[[nodiscard]] std::string sha256Hex(std::span<const std::byte> data);

}  // namespace QtRocket
