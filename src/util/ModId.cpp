#include "QtRocket/util/ModId.h"

#include <atomic>
#include <cstdint>
#include <string>

namespace QtRocket
{

namespace
{

/// ModID.nextId: the next id to hand out, starting at 1 so every drawn id is positive.
[[nodiscard]] std::atomic<std::uint64_t>& nextId() noexcept
{
    static std::atomic<std::uint64_t> s_next{1};
    return s_next;
}

}  // namespace

ModId::ModId() noexcept
  : m_id(static_cast<std::int64_t>(nextId().fetch_add(1, std::memory_order_relaxed)))
{
}

std::string ModId::toString() const
{
    return std::to_string(m_id);
}

}  // namespace QtRocket
