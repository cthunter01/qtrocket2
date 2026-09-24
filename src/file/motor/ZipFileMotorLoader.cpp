#include "QtRocket/file/motor/ZipFileMotorLoader.h"

#include <cstddef>
#include <expected>
#include <format>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/file/motor/MotorLoader.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

namespace
{

/// How deep in nested archives this thread is reading: ZipFileMotorLoader::load() and the entry
/// loader call each other for an archive inside an archive.
[[nodiscard]] int& nesting() noexcept
{
    thread_local int s_nesting = 0;
    return s_nesting;
}

/// Counts one level of nesting while it lives.
class NestingLevel
{
public:
    NestingLevel() noexcept { nesting()++; }
    ~NestingLevel() { nesting()--; }
    NestingLevel(const NestingLevel&)            = delete;
    NestingLevel(NestingLevel&&)                 = delete;
    NestingLevel& operator=(const NestingLevel&) = delete;
    NestingLevel& operator=(NestingLevel&&)      = delete;

    [[nodiscard]] static bool tooDeep() noexcept
    {
        return nesting() > ZipFileMotorLoader::kMaxNesting;
    }
};

/// The contents of @p zip's current entry, or a failure (running out of memory included).
[[nodiscard]] Result<std::vector<std::byte>> extract(ZipInputStream& zip, std::string_view name)
{
    try
    {
        return zip.readEntry();
    }
    catch (const std::bad_alloc&)
    {
        return fail(ErrorCode::IO, std::format("Out of memory reading ZIP entry {}", name));
    }
}

}  // namespace

ZipFileMotorLoader::ZipFileMotorLoader()
  : m_ownLoader(std::make_unique<GeneralMotorLoader>()), m_loader(m_ownLoader.get())
{
}

ZipFileMotorLoader::ZipFileMotorLoader(const MotorLoader& loader) noexcept : m_loader(&loader) { }

ZipFileMotorLoader::~ZipFileMotorLoader() = default;

Result<std::vector<ThrustCurveMotor::Builder>> ZipFileMotorLoader::load(
    std::span<const std::byte> data, std::string_view /*filename*/) const
{
    const NestingLevel level;
    if (NestingLevel::tooDeep())
    {
        return fail(ErrorCode::PARSE, "ZIP archives nested too deeply");
    }

    std::vector<ThrustCurveMotor::Builder> motors;
    ZipInputStream                         zip(data);
    while (true)
    {
        Result<std::optional<ZipInputStream::Entry>> next = zip.nextEntry();
        if (!next)
        {
            return std::unexpected(std::move(next.error()));
        }
        if (!next->has_value())
        {
            break;
        }
        const ZipInputStream::Entry& entry = **next;
        // Directories, and entries the loader would refuse by name before reading them (Could not
        // read ZIP entry, which OpenRocket logs): the next nextEntry() reads through them.
        if (entry.directory || !m_loader->canLoad(entry.name))
        {
            continue;
        }

        Result<std::vector<std::byte>> contents = extract(zip, entry.name);
        if (!contents)
        {
            return std::unexpected(std::move(contents.error()));
        }
        Result<std::vector<ThrustCurveMotor::Builder>> loaded =
            m_loader->load(*contents, entry.name);
        if (!loaded)
        {
            if (loaded.error().code == ErrorCode::UNSUPPORTED_FORMAT)
            {
                continue;  // Could not read ZIP entry (OpenRocket logs this)
            }
            return std::unexpected(std::move(loaded.error()));
        }
        motors.insert(motors.end(), std::make_move_iterator(loaded->begin()),
                      std::make_move_iterator(loaded->end()));
    }

    return motors;
}

}  // namespace QtRocket
