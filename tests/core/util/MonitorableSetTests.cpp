#include "QtRocket/util/MonitorableSet.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/ModId.h"
#include "QtRocket/util/Monitorable.h"

namespace
{

using QtRocket::ModId;
using QtRocket::MonitorableSet;

// The set can be watched as every Monitorable.
static_assert(QtRocket::Monitorable<MonitorableSet<int>>);

/// The elements of @p set in iteration order.
template <class E>
[[nodiscard]] std::vector<E> elements(const MonitorableSet<E>& set)
{
    return {set.begin(), set.end()};
}

/// Tells whether the modification id of a set changed since the last look.
class IdWatch
{
public:
    explicit IdWatch(const MonitorableSet<int>& set) : m_set(&set), m_last(set.modId()) { }

    [[nodiscard]] bool drew()
    {
        const ModId now     = m_set->modId();
        const bool  changed = now != m_last;
        EXPECT_GE(now, m_last) << "an id never goes backwards";
        m_last = now;
        return changed;
    }

private:
    const MonitorableSet<int>* m_set;
    ModId                      m_last;
};

TEST(MonitorableSet, ANewSetIsEmptyAndHasNoModificationId)
{
    const MonitorableSet<int> set;
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(set.size(), 0U);
    EXPECT_FALSE(set.contains(1));
    EXPECT_EQ(set.begin(), set.end());
    // Java's field is null until the first change.
    EXPECT_EQ(set.modId(), ModId::invalid());
}

TEST(MonitorableSet, AddKeepsOneOfEachElementInTheOrderTheyWereAdded)
{
    MonitorableSet<std::string> set;
    EXPECT_TRUE(set.add("b"));
    EXPECT_TRUE(set.add("a"));
    EXPECT_TRUE(set.add("c"));
    EXPECT_FALSE(set.add("a")) << "already there";
    EXPECT_EQ(set.size(), 3U);
    EXPECT_FALSE(set.empty());
    EXPECT_TRUE(set.contains("a"));
    EXPECT_FALSE(set.contains("d"));
    EXPECT_EQ(elements(set), (std::vector<std::string>{"b", "a", "c"}));
}

TEST(MonitorableSet, RemoveTakesAnElementOutAndClearAll)
{
    MonitorableSet<int> set;
    set.add(1);
    set.add(2);
    set.add(3);
    EXPECT_TRUE(set.remove(2));
    EXPECT_FALSE(set.remove(2));
    EXPECT_FALSE(set.remove(7));
    EXPECT_EQ(elements(set), (std::vector<int>{1, 3}));
    set.clear();
    EXPECT_TRUE(set.empty());
    EXPECT_EQ(elements(set), std::vector<int>{});
}

TEST(MonitorableSet, AddAllAddsWhatIsMissing)
{
    MonitorableSet<int> set;
    set.add(1);
    set.add(2);
    MonitorableSet<int> other;
    other.add(2);
    other.add(5);
    other.add(4);
    EXPECT_TRUE(set.addAll(other));
    EXPECT_EQ(elements(set), (std::vector<int>{1, 2, 5, 4}));
    EXPECT_FALSE(set.addAll(other)) << "nothing new";
    EXPECT_FALSE(set.addAll(MonitorableSet<int>{}));
    // A set added to itself stays as it is.
    EXPECT_FALSE(set.addAll(set));
    EXPECT_EQ(elements(set), (std::vector<int>{1, 2, 5, 4}));
}

TEST(MonitorableSet, EveryChangingCallDrawsAModificationIdWhetherOrNotTheSetChanges)
{
    // As Java's MonitorableSet: add(), addAll(), remove() and clear() draw an id first.
    MonitorableSet<int> set;
    IdWatch             watch(set);

    set.add(1);
    EXPECT_TRUE(watch.drew());
    set.add(1);
    EXPECT_TRUE(watch.drew()) << "an element that is there";
    set.remove(9);
    EXPECT_TRUE(watch.drew()) << "an element that is not there";
    set.remove(1);
    EXPECT_TRUE(watch.drew());
    set.addAll(MonitorableSet<int>{});
    EXPECT_TRUE(watch.drew()) << "an empty set";
    set.clear();
    EXPECT_TRUE(watch.drew()) << "an empty set cleared";

    // The readers draw none.
    static_cast<void>(set.contains(1));
    static_cast<void>(set.size());
    static_cast<void>(set.empty());
    static_cast<void>(elements(set));
    EXPECT_FALSE(watch.drew());
}

TEST(MonitorableSet, AddAllDrawsOneIdForTheCallAndOneForEveryElement)
{
    // Java's addAll() draws an id and then add()s each element, which draws one each. The ids
    // come from one counter, so the draws between two ids can be counted (nothing else draws
    // while this test runs).
    MonitorableSet<int> other;
    other.add(1);
    other.add(2);
    other.add(3);
    MonitorableSet<int> set;
    set.add(2);

    const ModId before;
    set.addAll(other);
    const ModId after;
    EXPECT_EQ(after.toInt() - before.toInt(), 1 + 3 + 1) << "addAll(), three add(), and `after`";
    EXPECT_EQ(set.modId().toInt(), after.toInt() - 1) << "the last add()'s id";

    const ModId beforeAdd;
    set.add(9);
    set.remove(9);
    set.clear();
    const ModId afterClear;
    EXPECT_EQ(afterClear.toInt() - beforeAdd.toInt(), 3 + 1);
}

TEST(MonitorableSet, ACopyHasTheElementsAndTheIdAndIsItsOwnSet)
{
    MonitorableSet<int> set;
    set.add(1);
    set.add(2);
    MonitorableSet<int> copy = set;
    EXPECT_EQ(copy.modId(), set.modId());
    EXPECT_EQ(elements(copy), (std::vector<int>{1, 2}));
    copy.add(3);
    EXPECT_EQ(set.size(), 2U);
    EXPECT_NE(copy.modId(), set.modId());
}

TEST(MonitorableSet, TwoSetsAreEqualWhenTheyHoldTheSameElementsInAnyOrder)
{
    MonitorableSet<int> a;
    a.add(1);
    a.add(2);
    MonitorableSet<int> b;
    b.add(2);
    b.add(1);
    EXPECT_EQ(a, b);
    EXPECT_NE(a.modId(), b.modId()) << "the ids do not count";
    b.add(3);
    EXPECT_NE(a, b);
    a.add(4);
    EXPECT_NE(a, b);
    EXPECT_EQ(MonitorableSet<int>{}, MonitorableSet<int>{});
}

TEST(MonitorableSet, HoldsPointersByTheirIdentity)
{
    // What the simulation status keeps: the deployed recovery devices, by pointer.
    const int                  first  = 1;
    const int                  second = 1;
    MonitorableSet<const int*> set;
    EXPECT_TRUE(set.add(&first));
    EXPECT_FALSE(set.add(&first));
    EXPECT_TRUE(set.add(&second)) << "an equal value at another address is another element";
    EXPECT_TRUE(set.contains(&second));
    EXPECT_TRUE(set.add(nullptr));
    EXPECT_EQ(set.size(), 3U);
}

}  // namespace
