#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <utility>

namespace QtRocket
{

class Motor;

/// The similarities the motor chooser's "Hide very similar thrust curves" has asked for, kept per
/// pair of curves (QtRocket's own: OpenRocket computes MotorCorrelation::similarity() again at
/// every selection, 1.4 ms a pair in Java, about 30 ms in a debug build here).
///
/// similarity() is symmetric, bit for bit (MotorCorrelation), so one entry serves both orders of a
/// pair. An entry keeps its two motors alive, so a pair's key (the two addresses) can never come to
/// name other motors. Not thread-safe: it belongs to the chooser that owns it.
class MotorSimilarityCache
{
public:
    /// MotorCorrelation::similarity(*a, *b), computed the first time either order of the pair is
    /// asked for and kept.
    /// @throws BugError when either motor is null, and as MotorCorrelation::similarity() does.
    [[nodiscard]] double similarity(const std::shared_ptr<const Motor>& a,
                                    const std::shared_ptr<const Motor>& b);

    /// How many pairs are kept.
    [[nodiscard]] std::size_t size() const noexcept { return m_entries.size(); }

    /// Forgets every pair (and lets go of the motors).
    void clear() noexcept { m_entries.clear(); }

private:
    struct Entry
    {
        std::shared_ptr<const Motor> first;
        std::shared_ptr<const Motor> second;
        double                       similarity;
    };

    using Key = std::pair<const Motor*, const Motor*>;

    /// The pair's addresses, the lower first (std::less, which orders any pointers).
    std::map<Key, Entry, std::less<>> m_entries;
};

}  // namespace QtRocket
