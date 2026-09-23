#include "QtRocket/file/ZipArchive.h"

#include <mz.h>
#include <mz_strm.h>  // IWYU pragma: keep (mz_zip_rw.h needs its callback types declared first)
#include <mz_strm_mem.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "MinizipHandle.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

constexpr std::size_t kMaxArchiveSize =
    static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max());
constexpr std::int32_t kWriterGrowSize = 256 * 1024;

}  // namespace

bool ZipArchive::looksLikeZip(std::span<const std::byte> bytes) noexcept
{
    return bytes.size() >= 4 && bytes[0] == std::byte{'P'} && bytes[1] == std::byte{'K'} &&
           bytes[2] == std::byte{3} && bytes[3] == std::byte{4};
}

Result<ZipArchive> ZipArchive::fromBytes(std::span<const std::byte> bytes)
{
    if (bytes.size() > kMaxArchiveSize)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT, "zip archive larger than 2 GiB");
    }
    const Detail::ZipReader reader;
    if (!reader.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create reader");
    }
    // minizip reads from the caller's buffer (copy = 0) and keeps nothing of it after this call.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto* data = reinterpret_cast<const std::uint8_t*>(bytes.data());
    if (const std::int32_t err = mz_zip_reader_open_buffer(
            reader.get(), data, static_cast<std::int32_t>(bytes.size()), 0);
        err != MZ_OK)
    {
        return fail(ErrorCode::PARSE, std::format("not a zip archive (minizip error {})", err));
    }

    ZipArchive   archive;
    std::int32_t err = mz_zip_reader_goto_first_entry(reader.get());
    while (err == MZ_OK)
    {
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader.get(), &info) != MZ_OK || info == nullptr ||
            info->filename == nullptr)
        {
            return fail(ErrorCode::PARSE, "zip archive: cannot read an entry header");
        }
        if (mz_zip_reader_entry_is_dir(reader.get()) != MZ_OK)  // MZ_OK means "is a directory"
        {
            Entry entry;
            entry.name                = info->filename;
            const std::int32_t length = mz_zip_reader_entry_save_buffer_length(reader.get());
            if (length < 0)
            {
                return fail(ErrorCode::PARSE,
                            std::format("zip archive: entry '{}' has an invalid size", entry.name));
            }
            entry.data.resize(static_cast<std::size_t>(length));
            if (length > 0 &&
                mz_zip_reader_entry_save_buffer(reader.get(), entry.data.data(), length) != MZ_OK)
            {
                return fail(ErrorCode::PARSE,
                            std::format("zip archive: cannot extract entry '{}'", entry.name));
            }
            archive.m_entries.push_back(std::move(entry));
        }
        err = mz_zip_reader_goto_next_entry(reader.get());
    }
    if (err != MZ_END_OF_LIST)
    {
        return fail(ErrorCode::PARSE,
                    std::format("zip archive: corrupt central directory (minizip error {})", err));
    }
    mz_zip_reader_close(reader.get());
    return archive;
}

bool ZipArchive::contains(std::string_view name) const noexcept
{
    return find(name) != nullptr;
}

const std::vector<std::byte>* ZipArchive::find(std::string_view name) const noexcept
{
    const auto it = std::ranges::find(m_entries, name, &Entry::name);
    return it == m_entries.end() ? nullptr : &it->data;
}

void ZipWriter::add(std::string name, std::span<const std::byte> data)
{
    m_pending.push_back(Pending{.name = std::move(name), .data = {data.begin(), data.end()}});
}

Result<std::vector<std::byte>> ZipWriter::finish() const
{
    Detail::MemStream       stream;
    const Detail::ZipWriter writer;
    if (!stream.valid() || !writer.valid())
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot create writer");
    }
    mz_stream_mem_set_grow_size(stream.get(), kWriterGrowSize);
    if (!stream.open(MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE))
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open memory stream");
    }
    mz_zip_writer_set_compress_method(writer.get(), MZ_COMPRESS_METHOD_DEFLATE);
    mz_zip_writer_set_compress_level(writer.get(), MZ_COMPRESS_LEVEL_DEFAULT);
    if (mz_zip_writer_open(writer.get(), stream.get(), 0) != MZ_OK)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot open zip writer");
    }
    const std::time_t now = std::time(nullptr);
    for (const Pending& pending : m_pending)
    {
        if (pending.data.size() > kMaxArchiveSize)
        {
            return fail(ErrorCode::UNSUPPORTED_FORMAT,
                        std::format("zip entry '{}' larger than 2 GiB", pending.name));
        }
        mz_zip_file info{};
        info.filename           = pending.name.c_str();
        info.flag               = MZ_ZIP_FLAG_UTF8;
        info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
        info.modified_date      = now;
        info.uncompressed_size  = static_cast<std::int64_t>(pending.data.size());
        // minizip rejects a null buffer even for a zero-length entry, and an empty vector's data()
        // may be null.
        static constexpr std::byte kEmpty{};
        const void*                buffer =
            pending.data.empty() ? static_cast<const void*>(&kEmpty) : pending.data.data();
        if (mz_zip_writer_add_buffer(writer.get(), buffer,
                                     static_cast<std::int32_t>(pending.data.size()),
                                     &info) != MZ_OK)
        {
            return fail(ErrorCode::UNKNOWN,
                        std::format("minizip: cannot add entry '{}'", pending.name));
        }
    }
    if (mz_zip_writer_close(writer.get()) != MZ_OK)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: cannot finish the archive");
    }
    const void*  buffer = nullptr;
    std::int32_t length = 0;
    mz_stream_mem_get_buffer(stream.get(), &buffer);
    mz_stream_mem_get_buffer_length(stream.get(), &length);
    if (buffer == nullptr || length < 0)
    {
        return fail(ErrorCode::UNKNOWN, "minizip: no archive was produced");
    }
    std::vector<std::byte> out(static_cast<std::size_t>(length));
    std::memcpy(out.data(), buffer, out.size());
    return out;
}

}  // namespace QtRocket
