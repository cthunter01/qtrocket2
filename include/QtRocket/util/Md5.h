#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace QtRocket
{

/// A self-contained RFC 1321 MD5 digest (no OpenSSL). It replaces java.security.MessageDigest
/// "MD5" for the motor and preset digests, which must stay byte-for-byte compatible with the
/// values OpenRocket writes into .ork files.
///
/// Usage: feed the bytes in any number of update() calls, then call finish(); as with
/// MessageDigest.digest(), finish() resets the object so it can digest the next message.
class Md5
{
public:
    static constexpr std::size_t kDigestSize = 16;
    static constexpr std::size_t kBlockSize  = 64;

    using Digest = std::array<std::byte, kDigestSize>;

    Md5() noexcept;

    /// Appends @p data to the message being digested.
    void update(std::span<const std::byte> data) noexcept;

    /// Pads the message, returns its digest and resets this object to its initial state.
    [[nodiscard]] Digest finish() noexcept;

private:
    void processBlock(std::span<const std::byte, kBlockSize> block) noexcept;
    void reset() noexcept;

    std::array<std::uint32_t, 4>      m_state{};
    std::array<std::byte, kBlockSize> m_buffer{};
    std::size_t                       m_bufferSize{0};
    std::uint64_t                     m_totalBytes{0};
};

/// The MD5 digest of @p data in one call.
[[nodiscard]] Md5::Digest md5(std::span<const std::byte> data) noexcept;

/// @p bytes as lowercase hexadecimal, two digits per byte (TextUtil.hexString).
[[nodiscard]] std::string toHex(std::span<const std::byte> bytes);

}  // namespace QtRocket
