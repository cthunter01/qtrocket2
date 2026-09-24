#include "QtRocket/util/Signal.h"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/util/BugError.h"

namespace
{

using QtRocket::BugError;
using QtRocket::Signal;

using VoidSignal = Signal<>;
using IntSignal  = Signal<int>;

// ListenerListTest.addListenerRejectsDuplicatesByIdentity: a std::function has no identity, so
// connecting the same callable twice makes two connections (see the class comment); the null
// listener check is kept.
TEST(Signal, EveryConnectIsASeparateConnection)
{
    VoidSignal signal;
    int        calls    = 0;
    const auto listener = [&calls] { ++calls; };
    const auto first    = signal.connect(listener);
    const auto second   = signal.connect(listener);
    EXPECT_EQ(signal.size(), 2U);
    EXPECT_TRUE(first.connected());
    EXPECT_TRUE(second.connected());
    EXPECT_NE(first, second);

    signal.emit();
    EXPECT_EQ(calls, 2);
}

TEST(Signal, StartsEmpty)
{
    VoidSignal signal;
    EXPECT_TRUE(signal.empty());
    EXPECT_EQ(signal.size(), 0U);
    signal.connect([] { });
    EXPECT_FALSE(signal.empty());
    EXPECT_EQ(signal.size(), 1U);
}

TEST(Signal, RejectsAnEmptySlot)
{
    VoidSignal signal;
    EXPECT_THROW(static_cast<void>(signal.connect(VoidSignal::Slot{})), BugError);
    EXPECT_THROW(static_cast<void>(signal.connect(nullptr)), BugError);
    EXPECT_TRUE(signal.empty());
}

// ListenerListTest.removeListenerOperatesOnObjectIdentity
TEST(Signal, DisconnectRemovesExactlyThatConnection)
{
    IntSignal  signal;
    int        sum1 = 0;
    int        sum2 = 0;
    const auto c1   = signal.connect([&sum1](int v) { sum1 += v; });
    const auto c2   = signal.connect([&sum2](int v) { sum2 += v; });

    EXPECT_TRUE(signal.disconnect(c1));
    EXPECT_FALSE(signal.disconnect(c1));  // already gone
    EXPECT_FALSE(signal.disconnect(IntSignal::Connection{}));
    EXPECT_EQ(signal.size(), 1U);

    signal.emit(5);
    EXPECT_EQ(sum1, 0);
    EXPECT_EQ(sum2, 5);
}

TEST(Signal, DisconnectUpdatesTheHandle)
{
    VoidSignal signal;
    const auto c1 = signal.connect([] { });
    const auto c2 = signal.connect([] { });
    EXPECT_TRUE(signal.disconnect(c1));
    EXPECT_FALSE(c1.connected());
    EXPECT_TRUE(c2.connected());
}

TEST(Signal, ConnectionHandleDisconnectsItself)
{
    VoidSignal signal;
    int        calls = 0;
    auto       c     = signal.connect([&calls] { ++calls; });
    EXPECT_TRUE(c.connected());
    EXPECT_TRUE(c.disconnect());
    EXPECT_FALSE(c.connected());
    EXPECT_FALSE(c.disconnect());
    EXPECT_EQ(c, VoidSignal::Connection{});  // emptied
    signal.emit();
    EXPECT_EQ(calls, 0);
}

TEST(Signal, HandlesFromAnotherSignalAreLeftAlone)
{
    VoidSignal a;
    VoidSignal b;
    const auto onA = a.connect([] { });
    EXPECT_FALSE(b.disconnect(onA));
    EXPECT_TRUE(onA.connected());
    EXPECT_EQ(a.size(), 1U);
    EXPECT_EQ(b.size(), 0U);
}

TEST(Signal, PassesArgumentsToEverySlotInConnectionOrder)
{
    Signal<int, const std::string&> signal;
    std::vector<std::string>        log;
    signal.connect(
        [&log](int n, const std::string& s) { log.push_back("a" + s + std::to_string(n)); });
    signal.connect(
        [&log](int n, const std::string& s) { log.push_back("b" + s + std::to_string(n)); });
    signal.emit(1, "x");
    signal(2, "y");  // operator() is emit() for code where Qt's `emit` macro is in force
    EXPECT_EQ(log, (std::vector<std::string>{"ax1", "bx1", "ay2", "by2"}));
}

TEST(Signal, EmitWithNoConnectionsDoesNothing)
{
    const IntSignal signal;
    EXPECT_NO_THROW(signal.emit(1));
    EXPECT_NO_THROW(signal(2));
}

// ListenerListTest.iteratorProvidesSnapshotAndDoesNotSupportRemove: a listener added while the
// list is being walked is not part of that walk. The other half of that test, iterator.remove()
// throwing UnsupportedOperationException, has no counterpart: the port exposes no iterator.
TEST(Signal, SlotConnectedDuringEmissionRunsOnTheNextOne)
{
    VoidSignal               signal;
    std::vector<std::string> log;
    signal.connect([&] {
        log.emplace_back("first");
        if (log.size() == 1)
        {
            signal.connect([&log] { log.emplace_back("late"); });
        }
    });
    signal.connect([&log] { log.emplace_back("second"); });

    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "second"}));
    EXPECT_EQ(signal.size(), 3U);

    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "second", "first", "second", "late"}));
}

TEST(Signal, SlotDisconnectedDuringEmissionDoesNotRunIfItHasNotYet)
{
    VoidSignal               signal;
    std::vector<std::string> log;
    VoidSignal::Connection   second;
    signal.connect([&] {
        log.emplace_back("first");
        signal.disconnect(second);
    });
    second = signal.connect([&log] { log.emplace_back("second"); });
    signal.connect([&log] { log.emplace_back("third"); });

    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "third"}));
    EXPECT_EQ(signal.size(), 2U);
    EXPECT_FALSE(second.connected());
}

TEST(Signal, SlotThatAlreadyRanCanBeDisconnectedByALaterOne)
{
    VoidSignal               signal;
    std::vector<std::string> log;
    auto                     first = signal.connect([&log] { log.emplace_back("first"); });
    signal.connect([&] {
        log.emplace_back("second");
        first.disconnect();
    });

    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "second"}));
    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "second", "second"}));
}

TEST(Signal, SlotMayDisconnectItselfWhileRunning)
{
    VoidSignal                                    signal;
    int                                           calls = 0;
    const std::shared_ptr<VoidSignal::Connection> self = std::make_shared<VoidSignal::Connection>();
    // The slot owns a large string: it must stay alive for the rest of the call even though
    // disconnecting drops the signal's reference to it.
    *self = signal.connect([&calls, self, payload = std::string(10'000, 'p')] {
        self->disconnect();
        ++calls;
        EXPECT_EQ(payload.size(), 10'000U);
    });
    signal.emit();
    signal.emit();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(signal.empty());
}

TEST(Signal, NestedEmissionIsAllowed)
{
    IntSignal        signal;
    std::vector<int> log;
    signal.connect([&](int depth) {
        log.push_back(depth);
        if (depth < 3)
        {
            signal.emit(depth + 1);
        }
    });
    signal.emit(1);
    EXPECT_EQ(log, (std::vector<int>{1, 2, 3}));
}

/// True when emitting @p signal throws a std::runtime_error (EXPECT_THROW, kept out of the test
/// body for clang-tidy's cognitive complexity limit).
[[nodiscard]] bool emitThrowsRuntimeError(const VoidSignal& signal)
{
    try
    {
        signal.emit();
    }
    catch (const std::runtime_error&)
    {
        return true;
    }
    return false;
}

TEST(Signal, ThrowingSlotStopsTheEmissionAndLeavesTheSignalIntact)
{
    // As in Java, where the exception unwinds out of the for-each over the ListenerList iterator:
    // the slots after the throwing one do not run, and the signal itself is untouched.
    VoidSignal               signal;
    std::vector<std::string> log;
    signal.connect([&log] { log.emplace_back("first"); });
    signal.connect([] { throw std::runtime_error("second fails"); });
    signal.connect([&log] { log.emplace_back("third"); });

    EXPECT_TRUE(emitThrowsRuntimeError(signal));
    EXPECT_EQ(log, (std::vector<std::string>{"first"}));
    EXPECT_EQ(signal.size(), 3U);

    // Still usable, and still failing the same way.
    EXPECT_TRUE(emitThrowsRuntimeError(signal));
    EXPECT_EQ(log, (std::vector<std::string>{"first", "first"}));
    EXPECT_EQ(signal.size(), 3U);
}

TEST(Signal, DisconnectAllDuringEmissionSkipsTheRest)
{
    VoidSignal               signal;
    std::vector<std::string> log;
    signal.connect([&] {
        log.emplace_back("first");
        signal.disconnectAll();  // the snapshot keeps this slot alive for the rest of the call
        log.emplace_back("first done");
    });
    signal.connect([&log] { log.emplace_back("second"); });
    signal.connect([&log] { log.emplace_back("third"); });

    signal.emit();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "first done"}));
    EXPECT_TRUE(signal.empty());
    signal.emit();
    EXPECT_EQ(log.size(), 2U);
}

/// Four slots A, B, O and C, where O's ScopedConnection is owned by B's closure (a helper object
/// that subscribed to the same signal, kept alive by the slot). Destroying B's slot destroys that
/// ScopedConnection, which disconnects O: erase() re-entered from inside the removal of B. The
/// signal must take the entry out of its list before the slot dies, or the nested removal
/// operates on a list in mid-mutation (the asan preset's _GLIBCXX_SANITIZE_VECTOR reports it).
class SignalOwnedConnection : public ::testing::Test
{
protected:
    /// Connects A, O, B, C when @p ownedBeforeB, else A, B, O, C.
    void connectAll(bool ownedBeforeB)
    {
        m_a        = m_signal.connect([this] { ++m_calls; });
        auto owner = std::make_shared<VoidSignal::ScopedConnection>();
        if (ownedBeforeB)
        {
            connectOwned(*owner);
        }
        m_b = m_signal.connect([owner] { });
        if (!ownedBeforeB)
        {
            connectOwned(*owner);
        }
        m_c     = m_signal.connect([this] { ++m_calls; });
        m_owned = owner->connection();
        owner.reset();  // B's closure is now the only owner of O's ScopedConnection
        ASSERT_EQ(m_signal.size(), 4U);
        ASSERT_TRUE(m_owned.connected());
    }

    /// After B (and with it O) is gone: A and C are left and are the only ones that run.
    void expectOnlyAAndCRemain()
    {
        EXPECT_EQ(m_signal.size(), 2U);
        EXPECT_FALSE(m_b.connected());
        EXPECT_FALSE(m_owned.connected());
        EXPECT_TRUE(m_a.connected());
        EXPECT_TRUE(m_c.connected());
        m_signal.emit();
        EXPECT_EQ(m_calls, 2);
    }

    VoidSignal             m_signal;
    int                    m_calls = 0;
    VoidSignal::Connection m_a;
    VoidSignal::Connection m_b;
    VoidSignal::Connection m_owned;
    VoidSignal::Connection m_c;

private:
    void connectOwned(VoidSignal::ScopedConnection& owner)
    {
        owner = m_signal.connect([this] { ++m_calls; });
    }
};

TEST_F(SignalOwnedConnection, DisconnectingTheOwnerAlsoDropsTheOwnedConnectionBeforeIt)
{
    connectAll(true);
    EXPECT_TRUE(m_signal.disconnect(m_b));
    expectOnlyAAndCRemain();
}

TEST_F(SignalOwnedConnection, DisconnectingTheOwnerAlsoDropsTheOwnedConnectionAfterIt)
{
    connectAll(false);
    EXPECT_TRUE(m_b.disconnect());
    expectOnlyAAndCRemain();
}

TEST_F(SignalOwnedConnection, DisconnectAllWithAnOwnedConnectionBeforeItsOwner)
{
    connectAll(true);
    m_signal.disconnectAll();
    EXPECT_TRUE(m_signal.empty());
    EXPECT_FALSE(m_owned.connected());
    m_signal.emit();
    EXPECT_EQ(m_calls, 0);
}

TEST_F(SignalOwnedConnection, DisconnectAllWithAnOwnedConnectionAfterItsOwner)
{
    connectAll(false);
    m_signal.disconnectAll();
    EXPECT_TRUE(m_signal.empty());
    EXPECT_FALSE(m_owned.connected());
    m_signal.emit();
    EXPECT_EQ(m_calls, 0);
}

TEST_F(SignalOwnedConnection, SignalDestructionWithAnOwnedConnection)
{
    connectAll(false);
    const VoidSignal::Connection b = m_b;
    m_signal                       = VoidSignal{};  // drops every connection, like the destructor
    EXPECT_FALSE(b.connected());
    EXPECT_FALSE(m_owned.connected());
    EXPECT_TRUE(m_signal.empty());
}

TEST(Signal, DisconnectAllEmptiesTheSignal)
{
    VoidSignal signal;
    int        calls = 0;
    const auto c1    = signal.connect([&calls] { ++calls; });
    const auto c2    = signal.connect([&calls] { ++calls; });
    signal.disconnectAll();
    EXPECT_TRUE(signal.empty());
    EXPECT_FALSE(c1.connected());
    EXPECT_FALSE(c2.connected());
    signal.emit();
    EXPECT_EQ(calls, 0);
    // Still usable afterwards.
    signal.connect([&calls] { ++calls; });
    signal.emit();
    EXPECT_EQ(calls, 1);
}

TEST(Signal, HandlesOutliveTheirSignal)
{
    VoidSignal::Connection dangling;
    {
        VoidSignal signal;
        dangling = signal.connect([] { });
        EXPECT_TRUE(dangling.connected());
    }
    EXPECT_FALSE(dangling.connected());
    EXPECT_FALSE(dangling.disconnect());
}

// A temporary ScopedConnection disconnects at the end of its full expression, so the statement
// `VoidSignal::ScopedConnection{signal.connect(slot)};` would silently undo the connect. The class
// is [[nodiscard]] so the compiler warns about it; that cannot be checked at run time.
TEST(Signal, ScopedConnectionDisconnectsWhenDestroyed)
{
    VoidSignal signal;
    int        calls = 0;
    {
        const VoidSignal::ScopedConnection scoped{signal.connect([&calls] { ++calls; })};
        EXPECT_TRUE(scoped.connected());
        EXPECT_TRUE(scoped.connection().connected());
        signal.emit();
        EXPECT_EQ(calls, 1);
    }
    EXPECT_TRUE(signal.empty());
    signal.emit();
    EXPECT_EQ(calls, 1);
}

TEST(Signal, ScopedConnectionReleaseKeepsTheSlot)
{
    VoidSignal             signal;
    int                    calls = 0;
    VoidSignal::Connection kept;
    {
        VoidSignal::ScopedConnection scoped{signal.connect([&calls] { ++calls; })};
        kept = scoped.release();
        EXPECT_FALSE(scoped.connected());
    }
    EXPECT_TRUE(kept.connected());
    signal.emit();
    EXPECT_EQ(calls, 1);
    EXPECT_TRUE(kept.disconnect());
}

TEST(Signal, ScopedConnectionExplicitDisconnect)
{
    VoidSignal                   signal;
    VoidSignal::ScopedConnection scoped{signal.connect([] { })};
    EXPECT_TRUE(scoped.disconnect());
    EXPECT_FALSE(scoped.disconnect());
    EXPECT_FALSE(scoped.connected());
    EXPECT_TRUE(signal.empty());
}

TEST(Signal, ScopedConnectionMoveConstructionTransfersOwnership)
{
    VoidSignal                         signal;
    VoidSignal::ScopedConnection       source{signal.connect([] { })};
    const VoidSignal::ScopedConnection target{std::move(source)};
    // The moved-from handle is empty.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_FALSE(source.connected());
    EXPECT_TRUE(target.connected());
    EXPECT_EQ(signal.size(), 1U);
}

TEST(Signal, ScopedConnectionMoveAssignmentDropsItsOwnConnection)
{
    VoidSignal                   signal;
    VoidSignal::ScopedConnection target{signal.connect([] { })};
    VoidSignal::ScopedConnection other{signal.connect([] { })};
    EXPECT_EQ(signal.size(), 2U);
    other = std::move(target);  // other's own connection is dropped, target's taken over
    EXPECT_EQ(signal.size(), 1U);
    EXPECT_TRUE(other.connected());
    // The moved-from handle is empty.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_FALSE(target.connected());
}

TEST(Signal, ScopedConnectionSelfMoveAssignmentIsHarmless)
{
    VoidSignal                   signal;
    VoidSignal::ScopedConnection scoped{signal.connect([] { })};
    // Through a reference, which is how it happens by accident in real code.
    VoidSignal::ScopedConnection& alias = scoped;
    scoped                              = std::move(alias);
    EXPECT_TRUE(scoped.connected());
    EXPECT_EQ(signal.size(), 1U);
}

TEST(Signal, ScopedConnectionAssignedAConnectionDropsTheOldOne)
{
    VoidSignal                   signal;
    VoidSignal::ScopedConnection scoped{signal.connect([] { })};
    const VoidSignal::Connection second = signal.connect([] { });
    EXPECT_EQ(signal.size(), 2U);
    scoped = second;
    EXPECT_EQ(signal.size(), 1U);
    EXPECT_EQ(scoped.connection(), second);
    EXPECT_TRUE(second.connected());
}

TEST(Signal, ScopedConnectionOutlivingTheSignalIsHarmless)
{
    auto                         signal = std::make_unique<VoidSignal>();
    VoidSignal::ScopedConnection scoped{signal->connect([] { })};
    signal.reset();
    EXPECT_FALSE(scoped.connected());
    EXPECT_FALSE(scoped.disconnect());
}

TEST(Signal, DefaultConstructedHandlesAreEmpty)
{
    const VoidSignal::Connection       connection;
    const VoidSignal::ScopedConnection scoped;
    EXPECT_FALSE(connection.connected());
    EXPECT_FALSE(scoped.connected());
    EXPECT_EQ(connection, VoidSignal::Connection{});
    EXPECT_EQ(scoped.connection(), connection);
}

TEST(Signal, MovingASignalTakesTheConnectionsAlong)
{
    IntSignal  source;
    int        sum = 0;
    const auto c   = source.connect([&sum](int v) { sum += v; });

    IntSignal target{std::move(source)};
    // The moved-from signal is empty.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(target.size(), 1U);
    EXPECT_TRUE(c.connected());
    target.emit(3);
    EXPECT_EQ(sum, 3);
    EXPECT_TRUE(target.disconnect(c));
    EXPECT_FALSE(c.connected());
}

TEST(Signal, MovedFromSignalIsStillUsable)
{
    IntSignal source;
    int       sum = 0;
    source.connect([&sum](int v) { sum += v; });
    const IntSignal target{std::move(source)};

    // NOLINTNEXTLINE(bugprone-use-after-move)
    source.connect([&sum](int v) { sum += 10 * v; });
    source.emit(1);
    EXPECT_EQ(sum, 10);
    target.emit(1);
    EXPECT_EQ(sum, 11);
}

TEST(Signal, MoveAssignmentDropsTheTargetsOldConnections)
{
    IntSignal  source;
    IntSignal  target;
    int        fromSource = 0;
    int        fromTarget = 0;
    const auto onSource   = source.connect([&fromSource](int v) { fromSource += v; });
    const auto onTarget   = target.connect([&fromTarget](int v) { fromTarget += v; });

    target = std::move(source);
    EXPECT_FALSE(onTarget.connected());
    EXPECT_TRUE(onSource.connected());
    EXPECT_EQ(target.size(), 1U);
    target.emit(2);
    EXPECT_EQ(fromSource, 2);
    EXPECT_EQ(fromTarget, 0);
}

TEST(Signal, SlotsAreCalledThroughStdFunction)
{
    // Any callable works: a std::function, a function pointer, a bound member.
    struct Counter
    {
        int  n = 0;
        void bump(int by) { n += by; }
    };
    Counter   counter;
    IntSignal signal;
    signal.connect(std::bind_front(&Counter::bump, &counter));
    signal.connect(IntSignal::Slot{[&counter](int v) { counter.n += 100 * v; }});
    signal.emit(2);
    EXPECT_EQ(counter.n, 202);
}

}  // namespace
