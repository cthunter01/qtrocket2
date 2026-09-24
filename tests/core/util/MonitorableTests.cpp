#include "QtRocket/util/Monitorable.h"

#include <cstdint>

#include <gtest/gtest.h>

#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/util/ModId.h"

namespace
{

using QtRocket::ErrorSet;
using QtRocket::ModId;
using QtRocket::Monitorable;
using QtRocket::Warning;
using QtRocket::WarningSet;

/// A value that is watched the way Java's Monitorable doc describes: a fresh id per change.
class Counter
{
public:
    [[nodiscard]] ModId modId() const noexcept { return m_modId; }
    [[nodiscard]] int   value() const noexcept { return m_value; }
    void                setValue(int value)
    {
        m_value = value;
        m_modId = ModId{};
    }

private:
    int   m_value{0};
    ModId m_modId{ModId::zero()};
};

struct NoModId
{
    int value{0};
};

class NonConstModId
{
public:
    [[nodiscard]] ModId modId() noexcept { return m_modId; }

private:
    ModId m_modId{ModId::zero()};
};

class IntegerModId
{
public:
    [[nodiscard]] std::int64_t modId() const noexcept { return m_modId; }

private:
    std::int64_t m_modId{0};
};

class ReferenceModId
{
public:
    [[nodiscard]] const ModId& modId() const noexcept { return m_modId; }

private:
    ModId m_modId{ModId::zero()};
};

static_assert(Monitorable<Counter>);
static_assert(Monitorable<WarningSet>);
static_assert(Monitorable<ErrorSet>);
static_assert(!Monitorable<NoModId>);
static_assert(!Monitorable<NonConstModId>);
static_assert(!Monitorable<IntegerModId>);
static_assert(!Monitorable<ReferenceModId>);
static_assert(!Monitorable<ModId>);
static_assert(!Monitorable<int>);

/// What a cache does with a Monitorable: keep the id it last saw and ask whether it moved.
template <Monitorable M>
[[nodiscard]] bool changedSince(const M& object, ModId seen)
{
    return object.modId() != seen;
}

TEST(Monitorable, ConceptConstrainsAReaderOfAnyMonitorableType)
{
    Counter counter;
    ModId   seen = counter.modId();
    EXPECT_EQ(seen, ModId::zero());
    EXPECT_FALSE(changedSince(counter, seen));
    counter.setValue(1);
    EXPECT_TRUE(changedSince(counter, seen));
    seen = counter.modId();
    EXPECT_FALSE(changedSince(counter, seen));

    WarningSet warnings;
    seen = warnings.modId();
    EXPECT_FALSE(changedSince(warnings, seen));
    warnings.add(Warning::kThickFin);
    EXPECT_TRUE(changedSince(warnings, seen));
    seen = warnings.modId();
    EXPECT_FALSE(changedSince(warnings, seen));
}

TEST(Monitorable, IdsOfOneObjectOnlyIncrease)
{
    Counter counter;
    ModId   previous = counter.modId();
    for (int i = 1; i <= 5; ++i)
    {
        counter.setValue(i);
        EXPECT_GT(counter.modId(), previous);
        previous = counter.modId();
    }
    // Setting the same value again is still a change of state for the contract's purposes.
    counter.setValue(5);
    EXPECT_GT(counter.modId(), previous);
}

}  // namespace
