#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>

#include "QtRocket/util/Md5.h"

namespace QtRocket
{

class Motor;
class ThrustCurveMotor;

/// A digest of a motor (OpenRocket's MotorDigest): a string that identifies a motor's functional
/// data like a checksum, so that two motors with the same digest behave alike with a very high
/// probability. .ork files store it to find the motor again, so it must match OpenRocket's byte
/// for byte.
///
/// The data is fed as typed blocks in increasing DataType order. Each value is rounded to a
/// limited precision first, so that rounding errors do not change the digest: v = next(v),
/// v *= multiplier, v = next(v), where next(v) = v + signum(v) * 1e-11; then Java's
/// (int) Math.round(v), which rounds half up (MathUtil::javaRound) and keeps the low 32 bits of
/// the long. Every block feeds the MD5 with the type's order, the number of values and the
/// values, each as a big-endian 32-bit integer; the digest is the MD5 in lowercase hex.
class MotorDigest
{
public:
    /// The kinds of data a digest covers, in the order they must be fed.
    enum class DataType
    {
        TIME_ARRAY,      ///< time points at which data is available, in ms (order 0, x1000)
        MASS_SPECIFIC,   ///< mass at a few points (initial and empty), in 0.1 g (order 1, x10000)
        MASS_PER_TIME,   ///< mass per time point, in 0.1 g (order 2, x10000)
        CG_SPECIFIC,     ///< CG at a few points (initial and final), in mm (order 3, x1000)
        CG_PER_TIME,     ///< CG per time point, in mm (order 4, x1000)
        FORCE_PER_TIME,  ///< thrust per time point, in mN (order 5, x1000)
    };

    /// The small value added away from zero before and after scaling (EPSILON).
    static constexpr double kEpsilon = 0.00000000001;

    /// The position of @p type in the feeding order (getOrder()).
    [[nodiscard]] static int getOrder(DataType type) noexcept;

    /// The factor that turns SI values into the digested unit (getMultiplier()).
    [[nodiscard]] static int getMultiplier(DataType type) noexcept;

    MotorDigest() = default;

    /// Rounds @p values as described above and feeds them as one block of @p type.
    /// @throws BugError when @p type does not come after the previous block's type (OpenRocket:
    ///         IllegalArgumentException); the order is fixed by the calling code.
    void update(DataType type, std::span<const double> values);

    /// update(type, values) for a list of values (Java's varargs call).
    void update(DataType type, std::initializer_list<double> values);

    /// The digest of everything fed so far, as lowercase hex.
    /// @throws BugError when called a second time (OpenRocket: IllegalStateException).
    [[nodiscard]] std::string getDigest();

    /// The digest of a motor's time, mass, CG and thrust points: TIME_ARRAY, MASS_PER_TIME,
    /// CG_PER_TIME and FORCE_PER_TIME (digestMotor()).
    [[nodiscard]] static std::string digestMotor(const ThrustCurveMotor& motor);

    /// True when @p digest is the motor's own digest or, for a ThrustCurveMotor, the digest of its
    /// data in one of the layouts motor loaders have used over time: TIME_ARRAY with
    /// MASS_SPECIFIC (launch and burnout mass) and FORCE_PER_TIME (RASP files); the same with
    /// CG_PER_TIME; TIME_ARRAY and FORCE_PER_TIME only; with MASS_PER_TIME; with CG_PER_TIME.
    /// .ork loading uses it to accept an embedded curve for the digest the file refers to.
    /// (OpenRocket's null motor or digest gives false.)
    [[nodiscard]] static bool isDigestCompatible(const Motor& motor, std::string_view digest);

    /// The digest of a comment: runs of whitespace collapsed to one space and trimmed
    /// (Strings::collapseWhitespace), then the MD5 of the UTF-8 bytes in lowercase hex
    /// (digestComment()).
    [[nodiscard]] static std::string digestComment(std::string_view comment);

private:
    /// Feeds one block of already rounded values.
    void updateRounded(DataType type, std::span<const std::int32_t> values);

    /// Feeds @p value as four big-endian bytes.
    void updateInt(std::int32_t value) noexcept;

    Md5  m_digest;
    bool m_used{false};
    int  m_lastOrder{-1};
};

}  // namespace QtRocket
