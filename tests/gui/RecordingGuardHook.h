#pragma once

#include <vector>

#include "QtRocket/gui/util/Guard.h"

namespace QtRocket::Gui::Test
{

/// Records every report of the error hook (Guard) for as long as it lives, instead of the hook
/// in force, which it puts back when it is destroyed.
class RecordingGuardHook
{
public:
    RecordingGuardHook();
    ~RecordingGuardHook();
    RecordingGuardHook(const RecordingGuardHook&)            = delete;
    RecordingGuardHook& operator=(const RecordingGuardHook&) = delete;
    RecordingGuardHook(RecordingGuardHook&&)                 = delete;
    RecordingGuardHook& operator=(RecordingGuardHook&&)      = delete;

    /// Every report so far, in order.
    [[nodiscard]] const std::vector<Guard::Failure>& failures() const noexcept
    {
        return m_failures;
    }

private:
    std::vector<Guard::Failure> m_failures;
    Guard::Hook                 m_previous;
};

}  // namespace QtRocket::Gui::Test
