#pragma once

#include "QtRocket/unit/UnitGroup.h"

namespace QtRocket::Test
{

/// Puts UnitGroup.java's default units in place when constructed and again when it goes out of
/// scope, so that a test changing a default unit of the process-wide groups starts from the
/// defaults and leaves them as it found them, whatever it asserts on the way.
class DefaultUnitsGuard
{
public:
    DefaultUnitsGuard() { UnitGroup::resetDefaultUnits(); }
    ~DefaultUnitsGuard() { UnitGroup::resetDefaultUnits(); }
    DefaultUnitsGuard(const DefaultUnitsGuard&)            = delete;
    DefaultUnitsGuard& operator=(const DefaultUnitsGuard&) = delete;
    DefaultUnitsGuard(DefaultUnitsGuard&&)                 = delete;
    DefaultUnitsGuard& operator=(DefaultUnitsGuard&&)      = delete;
};

}  // namespace QtRocket::Test
