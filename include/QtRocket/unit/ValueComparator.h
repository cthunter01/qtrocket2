#pragma once

#include "QtRocket/unit/Value.h"

namespace QtRocket
{

/// OpenRocket's ValueComparator (ValueComparator.INSTANCE): orders Values by Value::compareTo, for
/// std::ranges::sort(values, ValueComparator{}) and the ordered containers. It is a strict weak
/// ordering: Values that compare equal are equivalent, whatever Value::operator== says.
struct ValueComparator
{
    [[nodiscard]] bool operator()(const Value& a, const Value& b) const
    {
        return a.compareTo(b) < 0;
    }
};

}  // namespace QtRocket
