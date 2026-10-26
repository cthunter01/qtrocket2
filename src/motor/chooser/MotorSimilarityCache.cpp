#include "QtRocket/motor/chooser/MotorSimilarityCache.h"

#include <functional>
#include <memory>

#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorCorrelation.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

double MotorSimilarityCache::similarity(const std::shared_ptr<const Motor>& a,
                                        const std::shared_ptr<const Motor>& b)
{
    QTROCKET_ASSERT(a != nullptr && b != nullptr);
    const Key key = std::less<>{}(a.get(), b.get()) ? Key{a.get(), b.get()} : Key{b.get(), a.get()};
    if (const auto found = m_entries.find(key); found != m_entries.end())
    {
        return found->second.similarity;
    }
    const double value = MotorCorrelation::similarity(*a, *b);
    m_entries.emplace(key, Entry{.first = a, .second = b, .similarity = value});
    return value;
}

}  // namespace QtRocket
