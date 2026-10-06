#include "QtRocket/simulation/listeners/ProgressListener.h"

#include <memory>
#include <utility>

#include "QtRocket/util/BugError.h"

namespace QtRocket
{

namespace
{

/// The callback of a new listener, which must not be empty.
[[nodiscard]] std::shared_ptr<const ProgressListener::Callback> requireCallback(
    ProgressListener::Callback callback)
{
    if (callback == nullptr)
    {
        bug("A progress listener needs a callback");
    }
    return std::make_shared<const ProgressListener::Callback>(std::move(callback));
}

}  // namespace

ProgressListener::ProgressListener(Callback callback)
  : m_callback(requireCallback(std::move(callback)))
{
}

void ProgressListener::postStep(SimulationStatus& status)
{
    (*m_callback)(status);
}

}  // namespace QtRocket
