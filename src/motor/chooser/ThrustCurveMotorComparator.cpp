#include "QtRocket/motor/chooser/ThrustCurveMotorComparator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// The most description characters that count.
constexpr std::size_t kMaxCommentLength = 100;

/// What one thrust point is worth.
constexpr std::int64_t kPointWeight = 10;

}  // namespace

std::int64_t ThrustCurveMotorComparator::calculateGoodness(const ThrustCurveMotor& motor)
{
    // 10 chars of comments correspond to one thrust point, max ten points.
    const std::size_t commentLength =
        std::min(Strings::javaLength(motor.getDescription()), kMaxCommentLength);
    return (static_cast<std::int64_t>(motor.getTimePoints().size()) * kPointWeight) +
           static_cast<std::int64_t>(commentLength);
}

int ThrustCurveMotorComparator::compare(const ThrustCurveMotor& o1, const ThrustCurveMotor& o2)
{
    const std::int64_t difference = calculateGoodness(o2) - calculateGoodness(o1);
    return static_cast<int>(std::clamp<std::int64_t>(difference, std::numeric_limits<int>::min(),
                                                     std::numeric_limits<int>::max()));
}

bool ThrustCurveMotorComparator::operator()(const std::shared_ptr<const ThrustCurveMotor>& a,
                                            const std::shared_ptr<const ThrustCurveMotor>& b) const
{
    QTROCKET_ASSERT(a != nullptr && b != nullptr);
    return compare(*a, *b) < 0;
}

}  // namespace QtRocket
