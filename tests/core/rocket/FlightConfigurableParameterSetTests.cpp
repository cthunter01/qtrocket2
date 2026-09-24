#include "QtRocket/rocket/FlightConfigurableParameterSet.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/rocket/FlightConfigurableParameter.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::FlightConfigurationId;

/// ParameterSetTest's TestParameter: every new object (constructed, cloned or copied) takes the
/// next id from a counter, and equality compares the ids. Copying the C++ object itself keeps the
/// id (it is the same parameter, moved into a set).
class TestParameter
{
public:
    static constexpr std::string_view kTypeName = "TestParameter";

    TestParameter() = default;

    [[nodiscard]] int id() const noexcept { return m_id; }

    // Static: they make new parameters and need nothing of this one (called as members all the
    // same, which the concept asks for).
    [[nodiscard]] static TestParameter clone() { return TestParameter{}; }
    [[nodiscard]] static TestParameter copy(const FlightConfigurationId& /*copyId*/)
    {
        return TestParameter{};
    }
    void              update() noexcept { ++m_updates; }
    [[nodiscard]] int updates() const noexcept { return m_updates; }

    [[nodiscard]] std::string toString() const { return "tp#:" + std::to_string(m_id); }

    [[nodiscard]] bool operator==(const TestParameter& other) const noexcept
    {
        return m_id == other.m_id;
    }

    /// The counter, reset before every test (Java: gid = 0 in localSetUp()).
    static void resetIds() noexcept { s_gid = 0; }

private:
    static int s_gid;
    int        m_id{s_gid++};
    int        m_updates{0};
};

int TestParameter::s_gid = 0;

static_assert(QtRocket::FlightConfigurableParameter<TestParameter>);

using TestSet = QtRocket::FlightConfigurableParameterSet<TestParameter>;

// ---- Ported from ParameterSetTest.java ----

class ParameterSetTest : public ::testing::Test
{
protected:
    ParameterSetTest() : m_testSet(makeSet()) { }

    void TearDown() override { TestParameter::resetIds(); }

    static TestSet makeSet()
    {
        TestParameter::resetIds();
        return TestSet{TestParameter{}};
    }

    TestSet m_testSet;
};

TEST_F(ParameterSetTest, EmptySet)
{
    EXPECT_EQ(m_testSet.size(), 0U) << "set should contain zero overrides";
    const TestParameter dtp;
    m_testSet.setDefault(dtp);
    EXPECT_EQ(m_testSet.size(), 0U) << "set should contain zero overrides";
    EXPECT_EQ(m_testSet.getDefault(), dtp) << "set stores default value correctly";
}

TEST_F(ParameterSetTest, RetrieveDefault)
{
    const FlightConfigurationId fcid2;
    // Requesting the value of a non-existent config id gives the default.
    EXPECT_EQ(m_testSet.getDefault(), m_testSet.get(fcid2)) << "set stores id-value pair correctly";
    EXPECT_EQ(0U, m_testSet.size()) << "set contains wrong number of overrides";

    const FlightConfigurationId fcidDef = FlightConfigurationId::defaultValueId();
    EXPECT_EQ(m_testSet.getDefault(), m_testSet.get(fcidDef))
        << "retrieving the via the special default key should produce the default value";
    EXPECT_EQ(0U, m_testSet.size()) << "set should still contain zero overrides";
}

TEST_F(ParameterSetTest, SetGetSecond)
{
    EXPECT_EQ(m_testSet.size(), 0U);

    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);
    // fcid <=> tp2 should be stored.
    EXPECT_EQ(m_testSet.get(fcid2), tp2) << "set stores default value correctly";
    EXPECT_EQ(m_testSet.size(), 1U);
}

TEST_F(ParameterSetTest, GetByNegativeIndex)
{
    EXPECT_EQ(m_testSet.size(), 0U);
    EXPECT_THROW(static_cast<void>(m_testSet.get(-1)), BugError);
}

TEST_F(ParameterSetTest, GetByTooHighIndex)
{
    EXPECT_EQ(m_testSet.size(), 0U);
    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);
    EXPECT_EQ(m_testSet.size(), 1U) << "set should contain one override";

    EXPECT_THROW(static_cast<void>(m_testSet.get(1)), BugError);
}

TEST_F(ParameterSetTest, GetIdsLength)
{
    EXPECT_EQ(m_testSet.size(), 0U);

    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);

    const TestParameter         tp3;
    const FlightConfigurationId fcid3;
    m_testSet.set(fcid3, tp3);

    EXPECT_EQ(m_testSet.size(), 2U) << "set should contain two overrides";

    // getIds() returns the ids of the overrides only.
    EXPECT_EQ(m_testSet.getIds().size(), m_testSet.size()) << "getIds() broken!\n"
                                                           << m_testSet.toDebug();
}

TEST_F(ParameterSetTest, GetByIndex)
{
    EXPECT_EQ(m_testSet.size(), 0U);

    std::vector<FlightConfigurationId> refList;
    for (int i = 0; i < 4; ++i)
    {
        const TestParameter         tp;
        const FlightConfigurationId fcid;
        m_testSet.set(fcid, tp);
        refList.push_back(fcid);
    }

    EXPECT_EQ(m_testSet.size(), 4U);

    for (int i = 0; i < 4; ++i)
    {
        EXPECT_EQ(m_testSet.get(i), m_testSet.get(refList.at(static_cast<std::size_t>(i))))
            << "retrieve-by-index broken!\n"
            << m_testSet.toDebug();
    }
}

TEST_F(ParameterSetTest, RemoveSecond)
{
    EXPECT_EQ(m_testSet.size(), 0U);

    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);
    EXPECT_EQ(m_testSet.get(fcid2), tp2);
    EXPECT_EQ(m_testSet.size(), 1U);

    // Java: set(fcid2, null).
    m_testSet.remove(fcid2);
    EXPECT_EQ(m_testSet.get(fcid2), m_testSet.getDefault());
    EXPECT_EQ(m_testSet.size(), 0U);
}

TEST_F(ParameterSetTest, GetByValue)
{
    EXPECT_EQ(m_testSet.size(), 0U);
    EXPECT_EQ(m_testSet.getId(m_testSet.getDefault()), FlightConfigurationId::defaultValueId())
        << "retrieving the default value should produce the special default key";

    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);
    EXPECT_EQ(m_testSet.size(), 1U);
    EXPECT_EQ(m_testSet.get(fcid2), tp2);

    // Retrieve that same parameter by value.
    const std::optional<FlightConfigurationId> fcid3 = m_testSet.getId(tp2);
    EXPECT_EQ(m_testSet.size(), 1U);
    EXPECT_EQ(std::optional{fcid2}, fcid3);
    EXPECT_EQ(m_testSet.get(fcid3.value_or(FlightConfigurationId::errorId())), tp2);
}

TEST_F(ParameterSetTest, CloneSecond)
{
    EXPECT_EQ(m_testSet.size(), 0U);

    const TestParameter         tp2;
    const FlightConfigurationId fcid2;
    m_testSet.set(fcid2, tp2);
    EXPECT_EQ(m_testSet.size(), 1U);
    EXPECT_EQ(m_testSet.get(fcid2), tp2);

    const FlightConfigurationId fcid3;
    m_testSet.copyFlightConfiguration(fcid2, fcid3);
    EXPECT_EQ(m_testSet.size(), 2U);
    EXPECT_NE(m_testSet.get(fcid3), m_testSet.getDefault());
}

/// The overrides keep their insertion order.
TEST_F(ParameterSetTest, Ordering)
{
    std::vector<FlightConfigurationId> refList;
    for (int i = 0; i < 4; ++i)
    {
        const TestParameter         tp;
        const FlightConfigurationId fcid;
        m_testSet.set(fcid, tp);
        refList.push_back(fcid);
    }
    EXPECT_EQ(refList, m_testSet.getIds());
}

// ---- QtRocket additions ----

TEST_F(ParameterSetTest, SettingTheDefaultIdIsIgnored)
{
    m_testSet.set(FlightConfigurationId::defaultValueId(), TestParameter{});
    EXPECT_EQ(m_testSet.size(), 0U);
    EXPECT_EQ(m_testSet.getDefault().id(), 0);
}

TEST_F(ParameterSetTest, SetDefaultIgnoresAnEqualValue)
{
    const TestParameter& before = m_testSet.getDefault();
    const TestParameter  same   = before;  // the same id
    m_testSet.setDefault(same);
    EXPECT_EQ(&m_testSet.getDefault(), &before) << "an equal default must not replace the cell";

    m_testSet.setDefault(TestParameter{});
    EXPECT_NE(m_testSet.getDefault().id(), 0);
}

TEST_F(ParameterSetTest, SetReplacesAnOverrideInPlace)
{
    const FlightConfigurationId first;
    const FlightConfigurationId second;
    m_testSet.set(first, TestParameter{});
    m_testSet.set(second, TestParameter{});
    const TestParameter replacement;
    m_testSet.set(first, replacement);
    EXPECT_EQ(m_testSet.getIds(), (std::vector{first, second}));
    EXPECT_EQ(m_testSet.get(first), replacement);
}

TEST_F(ParameterSetTest, ContainsId)
{
    const FlightConfigurationId fcid;
    EXPECT_TRUE(m_testSet.containsId(FlightConfigurationId::defaultValueId()));
    EXPECT_FALSE(m_testSet.containsId(fcid));
    m_testSet.set(fcid, TestParameter{});
    EXPECT_TRUE(m_testSet.containsId(fcid));
}

TEST_F(ParameterSetTest, IsDefaultByValueAndById)
{
    const FlightConfigurationId fcid;
    const FlightConfigurationId absent;
    const TestParameter         defaultCopy = m_testSet.getDefault();
    EXPECT_TRUE(m_testSet.isDefault(defaultCopy));
    EXPECT_FALSE(m_testSet.isDefault(TestParameter{}));

    // By id: only the default's own cell is "the default", as Java compares the objects; an id
    // without an entry is not (Java compares the default with null).
    EXPECT_TRUE(m_testSet.isDefault(FlightConfigurationId::defaultValueId()));
    EXPECT_FALSE(m_testSet.isDefault(absent));
    m_testSet.set(fcid, defaultCopy);  // equal, but its own cell
    EXPECT_FALSE(m_testSet.isDefault(fcid));
}

TEST_F(ParameterSetTest, ResetOneAndAll)
{
    const FlightConfigurationId a;
    const FlightConfigurationId b;
    m_testSet.set(a, TestParameter{});
    m_testSet.set(b, TestParameter{});

    m_testSet.reset(a);
    EXPECT_EQ(m_testSet.getIds(), std::vector{b});

    // The error id is ignored, and the default entry is never removed.
    m_testSet.reset(FlightConfigurationId::errorId());
    m_testSet.reset(FlightConfigurationId::defaultValueId());
    m_testSet.remove(FlightConfigurationId::defaultValueId());
    EXPECT_EQ(m_testSet.size(), 1U);
    EXPECT_EQ(m_testSet.getDefault().id(), 0);

    m_testSet.reset();
    EXPECT_EQ(m_testSet.size(), 0U);
    EXPECT_EQ(m_testSet.getDefault().id(), 0);
}

TEST_F(ParameterSetTest, CopyOfTheDefaultForAnIdWithoutOverride)
{
    const FlightConfigurationId source;  // no override: the default is copied
    const FlightConfigurationId target;
    EXPECT_EQ(m_testSet.copyFlightConfiguration(source, target), target);
    EXPECT_TRUE(m_testSet.containsId(target));
    EXPECT_NE(m_testSet.get(target), m_testSet.getDefault());
}

TEST_F(ParameterSetTest, SetAndCopyUpdateEveryValue)
{
    const FlightConfigurationId fcid;
    m_testSet.set(fcid, TestParameter{});
    EXPECT_EQ(m_testSet.getDefault().updates(), 1);
    EXPECT_EQ(m_testSet.get(fcid).updates(), 1);
    m_testSet.update();
    EXPECT_EQ(m_testSet.getDefault().updates(), 2);
    // copyFlightConfiguration() updates twice, as Java (set() and then itself).
    m_testSet.copyFlightConfiguration(fcid, FlightConfigurationId{});
    EXPECT_EQ(m_testSet.getDefault().updates(), 4);
}

TEST_F(ParameterSetTest, CopyingTheSetClonesEveryValue)
{
    const FlightConfigurationId fcid;
    m_testSet.set(fcid, TestParameter{});
    const TestSet copy = m_testSet;
    EXPECT_EQ(copy.getIds(), m_testSet.getIds());
    EXPECT_NE(copy.getDefault(), m_testSet.getDefault()) << "clone() gives a new id";
    EXPECT_NE(copy.get(fcid), m_testSet.get(fcid));

    TestSet assigned{TestParameter{}};
    assigned = m_testSet;
    EXPECT_EQ(assigned.getIds(), m_testSet.getIds());

    const TestSet moved = std::move(assigned);
    EXPECT_EQ(moved.size(), 1U);
}

TEST_F(ParameterSetTest, ReferencesSurviveLaterInsertions)
{
    const FlightConfigurationId first;
    m_testSet.set(first, TestParameter{});
    const TestParameter& stored = m_testSet.get(first);
    const int            id     = stored.id();
    for (int i = 0; i < 20; ++i)
    {
        m_testSet.set(FlightConfigurationId{}, TestParameter{});
    }
    EXPECT_EQ(stored.id(), id);
    EXPECT_EQ(&stored, &m_testSet.get(first));
}

TEST_F(ParameterSetTest, ValuesIterateInOrderWithTheDefaultFirst)
{
    const FlightConfigurationId a;
    const FlightConfigurationId b;
    m_testSet.set(a, TestParameter{});
    m_testSet.set(b, TestParameter{});
    std::vector<int> ids;
    for (const TestParameter& value : std::as_const(m_testSet).values())
    {
        ids.push_back(value.id());
    }
    EXPECT_EQ(ids, (std::vector{0, 1, 2}));
}

TEST_F(ParameterSetTest, ToDebugListsTheOverrides)
{
    const FlightConfigurationId fcid =
        FlightConfigurationId::fromString("123e4567-e89b-12d3-a456-426614174000");
    m_testSet.set(fcid, TestParameter{});
    EXPECT_EQ(m_testSet.toDebug(),
              "====== Dumping ConfigurationSet<TestParameter> (1 configurations)\n"
              "    [123e4567    ]: tp#:1\n");
    // An override equal to the default is starred.
    m_testSet.set(fcid, m_testSet.getDefault());
    EXPECT_EQ(m_testSet.toDebug(),
              "====== Dumping ConfigurationSet<TestParameter> (1 configurations)\n"
              "    [*123e4567*  ]: tp#:0\n");
}

}  // namespace
