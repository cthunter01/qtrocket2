#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/Error.h"

namespace QtRocket
{

/// Reads the motors of a motor file (OpenRocket's MotorLoader). A loader returns one
/// ThrustCurveMotor::Builder per motor in the file, in file order, with the motor's digest set;
/// the caller builds them, and a builder whose data ThrustCurveMotor rejects fails to build, as in
/// OpenRocket.
///
/// Deviations: OpenRocket reads an InputStream and throws IOException; here the loader takes the
/// file's bytes and returns a failure instead (ErrorCode::PARSE for malformed content with
/// OpenRocket's message, ErrorCode::UNSUPPORTED_FORMAT where OpenRocket throws
/// UnknownFileTypeException). A RuntimeException OpenRocket lets escape from malformed data (an
/// IndexOutOfBoundsException from a one-point curve) is a PARSE failure carrying Java's message.
class MotorLoader
{
public:
    virtual ~MotorLoader() = default;

    /// The motors in @p data, the contents of a file called @p filename (a bare name or a path
    /// inside an archive: loaders dispatch on its extension and quote it in messages).
    [[nodiscard]] virtual Result<std::vector<ThrustCurveMotor::Builder>> load(
        std::span<const std::byte> data, std::string_view filename) const = 0;

    /// Whether load() reads a file called @p filename at all: false when it fails with
    /// ErrorCode::UNSUPPORTED_FORMAT for the name alone, whatever the data (OpenRocket's
    /// GeneralMotorLoader throws UnknownFileTypeException before it reads anything). An archive
    /// reader asks this to skip such an entry without extracting it. True unless a loader says
    /// otherwise.
    [[nodiscard]] virtual bool canLoad(std::string_view /*filename*/) const { return true; }

protected:
    MotorLoader()                              = default;
    MotorLoader(const MotorLoader&)            = default;
    MotorLoader(MotorLoader&&)                 = default;
    MotorLoader& operator=(const MotorLoader&) = default;
    MotorLoader& operator=(MotorLoader&&)      = default;
};

}  // namespace QtRocket
