#include "QtRocket/figure/PathSegment.h"

#include <cstddef>
#include <format>
#include <ostream>

namespace QtRocket
{

char pathSegmentLetter(PathSegment::Type type) noexcept
{
    switch (type)
    {
        case PathSegment::Type::MOVE_TO:
            return 'M';
        case PathSegment::Type::LINE_TO:
            return 'L';
        case PathSegment::Type::QUAD_TO:
            return 'Q';
        case PathSegment::Type::CUBIC_TO:
            return 'C';
        case PathSegment::Type::CLOSE:
            return 'Z';
    }
    return '?';
}

std::ostream& operator<<(std::ostream& os, const PathSegment& segment)
{
    os << pathSegmentLetter(segment.type);
    const auto count = static_cast<std::size_t>(segment.pointCount());
    for (std::size_t i = 0; i < count; ++i)
    {
        os << std::format(" {},{}", segment.points.at(i).x, segment.points.at(i).y);
    }
    return os;
}

}  // namespace QtRocket
