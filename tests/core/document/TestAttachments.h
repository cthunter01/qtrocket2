#pragma once

// Attachments for the tests of document/: one that keeps its bytes in memory and one that fails.
// Neither is a FileSystemAttachment, so the decal registry finds their images by name.

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"

namespace QtRocket::Test
{

/// An attachment whose bytes are the text it was made with. It counts how often it was read.
class MemoryAttachment final : public Attachment
{
public:
    MemoryAttachment(std::string name, std::string_view text)
      : Attachment(std::move(name)), m_bytes(stringToBytes(text))
    {
    }

    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override
    {
        ++m_reads;
        return m_bytes;
    }

    /// How often getBytes() was called.
    [[nodiscard]] int reads() const noexcept { return m_reads; }

private:
    std::vector<std::byte> m_bytes;
    mutable int            m_reads{0};
};

/// An attachment that cannot be read: getBytes() fails with the Error it was made with. The
/// default is the failure of an attachment whose source is missing (Java: an attachment that
/// throws DecalNotFoundException, as the one of an archive without the entry).
class FailingAttachment final : public Attachment
{
public:
    explicit FailingAttachment(std::string name)
      : Attachment(std::move(name)), m_error(decalNotFound(getName()).error())
    {
    }
    FailingAttachment(std::string name, ErrorCode code, std::string message)
      : Attachment(std::move(name)),
        m_error{.code = code, .message = std::move(message), .where = {}}
    {
    }

    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override
    {
        return std::unexpected(m_error);
    }

private:
    Error m_error;
};

}  // namespace QtRocket::Test
