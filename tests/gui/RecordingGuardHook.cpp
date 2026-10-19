#include "RecordingGuardHook.h"

#include <utility>

#include "QtRocket/gui/util/Guard.h"

namespace QtRocket::Gui::Test
{

RecordingGuardHook::RecordingGuardHook()
  : m_previous(
        Guard::setHook([this](const Guard::Failure& failure) { m_failures.push_back(failure); }))
{
}

RecordingGuardHook::~RecordingGuardHook()
{
    static_cast<void>(Guard::setHook(std::move(m_previous)));
}

}  // namespace QtRocket::Gui::Test
