#pragma once

// RAII owners for minizip-ng's opaque handles (internal to the file/ subsystem).

#include <mz.h>
#include <mz_strm.h>
#include <mz_strm_mem.h>
#include <mz_strm_zlib.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <cstdint>

namespace QtRocket::Detail
{

/// Owns a handle whose Delete function also releases everything the handle acquired (zip reader and
/// writer).
template <void* (*Create)(), void (*Delete)(void**)>
class MzHandle
{
public:
    MzHandle() : m_handle(Create()) { }
    ~MzHandle()
    {
        if (m_handle != nullptr)
        {
            Delete(&m_handle);
        }
    }
    MzHandle(const MzHandle&)            = delete;
    MzHandle& operator=(const MzHandle&) = delete;
    MzHandle(MzHandle&&)                 = delete;
    MzHandle& operator=(MzHandle&&)      = delete;

    [[nodiscard]] void* get() const noexcept { return m_handle; }
    [[nodiscard]] bool  valid() const noexcept { return m_handle != nullptr; }

private:
    void* m_handle;
};

/// Owns a minizip stream. Unlike the zip reader and writer, a stream's Delete does not close it
/// (the zlib stream would leak its inflate/deflate state), so an open stream is closed here on
/// destruction.
template <void* (*Create)(), void (*Delete)(void**)>
class MzStream
{
public:
    MzStream() : m_handle(Create()) { }
    ~MzStream()
    {
        if (m_handle != nullptr)
        {
            if (m_open)
            {
                mz_stream_close(m_handle);
            }
            Delete(&m_handle);
        }
    }
    MzStream(const MzStream&)            = delete;
    MzStream& operator=(const MzStream&) = delete;
    MzStream(MzStream&&)                 = delete;
    MzStream& operator=(MzStream&&)      = delete;

    [[nodiscard]] void* get() const noexcept { return m_handle; }
    [[nodiscard]] bool  valid() const noexcept { return m_handle != nullptr; }

    /// Opens the stream in @p mode (MZ_OPEN_MODE_*). Returns false on failure.
    [[nodiscard]] bool open(std::int32_t mode) noexcept
    {
        m_open = mz_stream_open(m_handle, nullptr, mode) == MZ_OK;
        return m_open;
    }

    /// Closes an open stream (for a compressing stream this flushes the trailer). Returns false on
    /// failure.
    [[nodiscard]] bool close() noexcept
    {
        if (!m_open)
        {
            return true;
        }
        m_open = false;
        return mz_stream_close(m_handle) == MZ_OK;
    }

private:
    void* m_handle;
    bool  m_open = false;
};

using MemStream  = MzStream<mz_stream_mem_create, mz_stream_mem_delete>;
using ZlibStream = MzStream<mz_stream_zlib_create, mz_stream_zlib_delete>;
using ZipReader  = MzHandle<mz_zip_reader_create, mz_zip_reader_delete>;
using ZipWriter  = MzHandle<mz_zip_writer_create, mz_zip_writer_delete>;

}  // namespace QtRocket::Detail
