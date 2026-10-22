#include "QtRocket/gui/figure/FinPointEditHistory.h"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"

namespace QtRocket::Gui
{

FinPointEditHistory::FinPointEditHistory(FinPointSelection target, std::size_t maxSize)
  : m_target(std::move(target)), m_maxSize(std::max<std::size_t>(1, maxSize))
{
    reset();
}

void FinPointEditHistory::reset()
{
    m_history.clear();
    m_position = 0;
    if (std::optional<std::vector<Coordinate>> current = snapshotCurrent())
    {
        m_history.push_back(std::move(*current));
    }
}

void FinPointEditHistory::clear()
{
    m_history.clear();
    m_position = 0;
    m_pendingBefore.reset();
    m_editInProgress = false;
    m_applying       = false;
}

bool FinPointEditHistory::canUndo() const noexcept
{
    return !m_history.empty() && m_position > 0;
}

bool FinPointEditHistory::canRedo() const noexcept
{
    return !m_history.empty() && m_position + 1 < m_history.size();
}

void FinPointEditHistory::beginEdit()
{
    if (m_applying || m_editInProgress)
    {
        return;
    }
    m_pendingBefore  = snapshotCurrent();
    m_editInProgress = true;
}

void FinPointEditHistory::commitEdit()
{
    if (m_applying || !m_editInProgress)
    {
        return;
    }
    const std::optional<std::vector<Coordinate>> before = std::exchange(m_pendingBefore, {});
    m_editInProgress                                    = false;

    std::optional<std::vector<Coordinate>> after = snapshotCurrent();
    if (before.has_value() && after.has_value() && !snapshotsEqual(*before, *after))
    {
        push(std::move(*after));
    }
}

void FinPointEditHistory::undo()
{
    if (isEditInProgress() || !canUndo())
    {
        return;
    }
    --m_position;
    applyFromHistory();
}

void FinPointEditHistory::redo()
{
    if (isEditInProgress() || !canRedo())
    {
        return;
    }
    ++m_position;
    applyFromHistory();
}

bool FinPointEditHistory::snapshotsEqual(std::span<const Coordinate> a,
                                         std::span<const Coordinate> b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        const Coordinate& p1 = a[i];
        const Coordinate& p2 = b[i];
        if (!MathUtil::equals(p1.x, p2.x) || !MathUtil::equals(p1.y, p2.y) ||
            !MathUtil::equals(p1.z, p2.z) || !MathUtil::equals(p1.weight, p2.weight))
        {
            return false;
        }
    }
    return true;
}

std::optional<std::vector<Coordinate>> FinPointEditHistory::snapshotCurrent() const
{
    const FreeformFinSet* fins = m_target.main();
    if (fins == nullptr)
    {
        return std::nullopt;
    }
    return fins->getFinPoints();
}

void FinPointEditHistory::push(std::vector<Coordinate> snapshot)
{
    // Discard redo states
    if (m_position + 1 < m_history.size())
    {
        m_history.erase(std::next(m_history.begin(), static_cast<std::ptrdiff_t>(m_position + 1)),
                        m_history.end());
    }

    // Don't add duplicates
    if (!m_history.empty() && snapshotsEqual(m_history[m_position], snapshot))
    {
        return;
    }

    m_history.push_back(std::move(snapshot));
    m_position = m_history.size() - 1;

    // Limit size
    while (m_history.size() > m_maxSize)
    {
        m_history.erase(m_history.begin());
        m_position = m_position > 0 ? m_position - 1 : 0;
    }
}

void FinPointEditHistory::applyFromHistory()
{
    if (m_position >= m_history.size())
    {
        return;
    }
    m_applying = true;
    try
    {
        // A copy: setting the points may reach this history again (an edit's commit).
        const std::vector<Coordinate> points = m_history[m_position];
        m_target.setPoints(points);
    }
    catch (...)
    {
        m_applying = false;
        throw;
    }
    m_applying = false;
}

}  // namespace QtRocket::Gui
