#include "QtRocket/preferences/InMemoryPreferences.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Signal.h"

namespace
{

using QtRocket::BugError;
using QtRocket::InMemoryPreferences;
using QtRocket::Preferences;

using Names = std::vector<std::string>;

/// Counts the emissions of a signal.
class ChangeCounter
{
public:
    explicit ChangeCounter(QtRocket::Signal<>& signal)
      : m_connection(signal.connect([this] { ++m_count; }))
    {
    }
    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                  m_count{0};
    QtRocket::Signal<>::ScopedConnection m_connection;
};

// OpenRocket has no tests of its preference stores; these cover the java.util.prefs node
// behaviour the in-memory store reproduces.

TEST(InMemoryPreferences, StartsEmpty)
{
    const InMemoryPreferences prefs;
    EXPECT_TRUE(prefs.empty());
    EXPECT_EQ(prefs.keys(), Names{});
    EXPECT_EQ(prefs.childrenNames(), Names{});
    EXPECT_EQ(prefs.get("anything"), std::nullopt);
    EXPECT_EQ(prefs.findNode("anything"), nullptr);
}

TEST(InMemoryPreferences, PutGetRemove)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    EXPECT_FALSE(prefs.empty());
    EXPECT_EQ(prefs.get("a"), "1");
    EXPECT_EQ(prefs.get("b"), std::nullopt);

    prefs.put("a", "2");  // overwrite
    EXPECT_EQ(prefs.get("a"), "2");
    EXPECT_EQ(prefs.keys(), Names{"a"});

    prefs.remove("a");
    EXPECT_EQ(prefs.get("a"), std::nullopt);
    EXPECT_TRUE(prefs.empty());
    prefs.remove("a");  // absent: nothing happens
    EXPECT_TRUE(prefs.empty());
}

TEST(InMemoryPreferences, KeysAreSorted)
{
    InMemoryPreferences prefs;
    prefs.put("zeta", "");
    prefs.put("alpha", "");
    prefs.put("Mid", "");
    EXPECT_EQ(prefs.keys(), (Names{"Mid", "alpha", "zeta"}));
}

TEST(InMemoryPreferences, ValuesMayBeEmptyOrHoldAnyBytes)
{
    InMemoryPreferences prefs;
    prefs.put("empty", "");
    prefs.put("text", "a\nb,c|d \t\xC2\xB0");
    EXPECT_EQ(prefs.get("empty"), "");
    EXPECT_EQ(prefs.get("text"), "a\nb,c|d \t\xC2\xB0");
    EXPECT_EQ(prefs.get(""), std::nullopt);
    prefs.put("", "empty key");
    EXPECT_EQ(prefs.get(""), "empty key");
}

TEST(InMemoryPreferences, GetNodeCreatesAndReturnsTheSameChild)
{
    InMemoryPreferences  prefs;
    InMemoryPreferences& node = prefs.getNode("componentColors");
    EXPECT_TRUE(node.empty());
    EXPECT_EQ(prefs.childrenNames(), Names{"componentColors"});
    EXPECT_FALSE(prefs.empty());  // a child node counts

    node.put("BodyTube", "1,2,3");
    EXPECT_EQ(&prefs.getNode("componentColors"), &node);
    EXPECT_EQ(prefs.getNode("componentColors").get("BodyTube"), "1,2,3");
    EXPECT_EQ(prefs.findNode("componentColors"), &node);
    // The child's keys are its own, not the parent's.
    EXPECT_EQ(prefs.get("BodyTube"), std::nullopt);
    EXPECT_EQ(prefs.keys(), Names{});
}

TEST(InMemoryPreferences, FindNodeNeverCreates)
{
    InMemoryPreferences prefs;
    EXPECT_EQ(prefs.findNode("missing"), nullptr);
    EXPECT_EQ(prefs.findNode("missing/deeper"), nullptr);
    EXPECT_EQ(prefs.childrenNames(), Names{});
    EXPECT_TRUE(prefs.empty());
}

TEST(InMemoryPreferences, SlashSeparatedPathsWalkNestedNodes)
{
    InMemoryPreferences  prefs;
    InMemoryPreferences& transform = prefs.getNode("OBJExportOptions/CoordTransform");
    transform.put("xAxis", "Y");

    EXPECT_EQ(prefs.childrenNames(), Names{"OBJExportOptions"});
    EXPECT_EQ(prefs.getNode("OBJExportOptions").childrenNames(), Names{"CoordTransform"});
    EXPECT_EQ(&prefs.getNode("OBJExportOptions").getNode("CoordTransform"), &transform);
    EXPECT_EQ(prefs.findNode("OBJExportOptions/CoordTransform"), &transform);
    EXPECT_EQ(prefs.findNode("OBJExportOptions")->findNode("CoordTransform"), &transform);
    EXPECT_EQ(prefs.findNode("OBJExportOptions/Other"), nullptr);
    EXPECT_EQ(prefs.getInNode("OBJExportOptions/CoordTransform", "xAxis"), "Y");
}

TEST(InMemoryPreferences, EmptyNodeNamesAreABug)
{
    InMemoryPreferences prefs;
    EXPECT_THROW(static_cast<void>(prefs.getNode("")), BugError);
    EXPECT_THROW(static_cast<void>(prefs.getNode("a//b")), BugError);
    EXPECT_THROW(static_cast<void>(prefs.getNode("/a")), BugError);
    EXPECT_THROW(static_cast<void>(prefs.getNode("a/")), BugError);
    // findNode is the query form: nothing is created, nothing thrown.
    EXPECT_EQ(prefs.findNode(""), nullptr);
    EXPECT_EQ(prefs.findNode("a/"), nullptr);
    // "a" was created before "a/" failed on its empty second segment, as java.util.prefs would
    // have done up to the bad name.
    EXPECT_EQ(prefs.childrenNames(), Names{"a"});
}

TEST(InMemoryPreferences, ClearRemovesKeysAndKeepsChildren)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    prefs.getNode("child").put("b", "2");

    prefs.clear();
    EXPECT_EQ(prefs.keys(), Names{});
    EXPECT_EQ(prefs.childrenNames(), Names{"child"});
    EXPECT_EQ(prefs.getNode("child").get("b"), "2");
}

TEST(InMemoryPreferences, ResetRemovesEverything)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    prefs.getNode("child").getNode("grandchild").put("b", "2");

    prefs.reset();
    EXPECT_TRUE(prefs.empty());
    EXPECT_EQ(prefs.keys(), Names{});
    EXPECT_EQ(prefs.childrenNames(), Names{});
    EXPECT_EQ(prefs.findNode("child"), nullptr);
    // Back to the factory state: every typed getter gives OpenRocket's default again.
    EXPECT_EQ(prefs.getLaunchRodLength(), 1.0);
}

TEST(InMemoryPreferences, CopyIsADeepSnapshot)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    prefs.getNode("child").put("b", "2");

    const InMemoryPreferences snapshot(prefs);
    EXPECT_EQ(snapshot, prefs);
    EXPECT_EQ(snapshot.get("a"), "1");
    ASSERT_NE(snapshot.findNode("child"), nullptr);
    EXPECT_NE(snapshot.findNode("child"), prefs.findNode("child"));  // its own node objects
    EXPECT_EQ(snapshot.findNode("child")->get("b"), "2");

    // Changing either side afterwards leaves the other alone.
    prefs.put("a", "changed");
    prefs.getNode("child").put("b", "changed");
    static_cast<void>(prefs.getNode("new"));
    EXPECT_EQ(snapshot.get("a"), "1");
    EXPECT_EQ(snapshot.findNode("child")->get("b"), "2");
    EXPECT_EQ(snapshot.childrenNames(), Names{"child"});
    EXPECT_NE(snapshot, prefs);
}

TEST(InMemoryPreferences, CopyDoesNotCopyConnections)
{
    InMemoryPreferences prefs;
    const ChangeCounter counter(prefs.changed());

    InMemoryPreferences snapshot(prefs);
    EXPECT_TRUE(snapshot.changed().empty());
    EXPECT_EQ(prefs.changed().size(), 1U);

    snapshot.setLaunchRodLength(2.0);  // emits on the copy only
    EXPECT_EQ(counter.count(), 0);
    prefs.setLaunchRodLength(3.0);
    EXPECT_EQ(counter.count(), 1);
}

TEST(InMemoryPreferences, AssignmentRestoresValuesAndKeepsConnections)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    prefs.getNode("child").put("b", "2");
    const InMemoryPreferences snapshot(prefs);

    const ChangeCounter counter(prefs.changed());
    prefs.put("a", "edited");
    prefs.getNode("child").put("b", "edited");
    prefs.getNode("extra").put("c", "3");
    EXPECT_NE(prefs, snapshot);

    prefs = snapshot;  // "cancel": back to the snapshot
    EXPECT_EQ(prefs, snapshot);
    EXPECT_EQ(prefs.get("a"), "1");
    EXPECT_EQ(prefs.getNode("child").get("b"), "2");
    EXPECT_EQ(prefs.childrenNames(), Names{"child"});
    EXPECT_EQ(prefs.changed().size(), 1U);
    prefs.setLaunchRodLength(2.0);
    EXPECT_EQ(counter.count(), 1);
}

TEST(InMemoryPreferences, SelfAssignmentIsHarmless)
{
    InMemoryPreferences prefs;
    prefs.put("a", "1");
    prefs.getNode("child").put("b", "2");
    InMemoryPreferences& self = prefs;
    prefs                     = self;
    EXPECT_EQ(prefs.get("a"), "1");
    EXPECT_EQ(prefs.getNode("child").get("b"), "2");
}

TEST(InMemoryPreferences, EqualityComparesValuesAndChildren)
{
    InMemoryPreferences a;
    InMemoryPreferences b;
    EXPECT_EQ(a, b);

    a.put("k", "v");
    EXPECT_NE(a, b);
    b.put("k", "v");
    EXPECT_EQ(a, b);
    b.put("k", "w");
    EXPECT_NE(a, b);
    b.put("k", "v");

    static_cast<void>(a.getNode("n"));
    EXPECT_NE(a, b);  // an empty child still counts
    static_cast<void>(b.getNode("n"));
    EXPECT_EQ(a, b);
    a.getNode("n").put("x", "1");
    EXPECT_NE(a, b);
    b.getNode("n").put("x", "1");
    EXPECT_EQ(a, b);
    static_cast<void>(b.getNode("m"));
    EXPECT_NE(a, b);  // different child names, same count would still differ
}

// A node may be compared with, and assigned from, a node of its own subtree: no method holds
// the locks of two nodes it was merely given (a node and its own child with the same child
// names would otherwise lock one mutex twice).
TEST(InMemoryPreferences, ANodeAndItsOwnDescendantCanBeComparedAndAssigned)
{
    InMemoryPreferences root;
    root.put("k", "root");
    InMemoryPreferences& child = root.getNode("n");
    child.put("k", "child");
    child.getNode("n").put("k", "grandchild");

    EXPECT_NE(root, child);
    EXPECT_NE(child, root);
    EXPECT_EQ(child, child);

    const InMemoryPreferences childSnapshot(child);
    root = child;  // the child is replaced by a copy of its own child while it is copied from
    EXPECT_EQ(root, childSnapshot);
    EXPECT_EQ(root.get("k"), "child");
    ASSERT_NE(root.findNode("n"), nullptr);
    EXPECT_EQ(root.findNode("n")->get("k"), "grandchild");
    EXPECT_EQ(root.findNode("n/n"), nullptr);
}

/// Whether a snapshot of @p store, taken while the other threads write to it, holds @p value
/// under @p key (which only the calling thread writes). The snapshot is also compared with the
/// store: whatever the answer, the comparison must come back.
[[nodiscard]] bool snapshotHolds(const InMemoryPreferences& store, const std::string& key,
                                 const std::string& value)
{
    // The copy is what is tested.
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    const InMemoryPreferences snapshot(store);
    static_cast<void>(snapshot == store);
    return snapshot.get(key) == value;
}

/// What one thread of the store test does: @p rounds times it writes, reads back and removes
/// keys of its own (so the maps keep changing their structure), writes a key every thread
/// writes, walks to a child node every thread uses, lists the keys and the children, and takes
/// and compares a snapshot of the whole store. Counts what did not read back in @p mismatches.
void hammerTheStore(InMemoryPreferences& store, int thread, int rounds, int& mismatches)
{
    const std::string own = "thread." + std::to_string(thread);
    for (int round = 0; round < rounds; round++)
    {
        const std::string key   = own + "." + std::to_string(round % 5);
        const std::string value = std::to_string(round);
        store.put(key, value);
        mismatches += store.get(key) == value ? 0 : 1;
        store.put("shared", value);
        InMemoryPreferences& node = store.getNode("node/" + std::to_string(round % 3));
        node.put(key, value);
        mismatches += node.get(key) == value ? 0 : 1;
        mismatches += store.findNode("node") != nullptr ? 0 : 1;
        mismatches += store.keys().empty() ? 1 : 0;
        mismatches += store.childrenNames() == Names{"node"} ? 0 : 1;
        if (round % 16 == 0)
        {
            mismatches += snapshotHolds(store, key, value) ? 0 : 1;
        }
        node.remove(key);
        store.remove(key);
        mismatches += store.empty() ? 1 : 0;
    }
    store.put(own, "done");
}

/// Runs hammerTheStore() on @p threads threads at the same time, @p rounds rounds each, and
/// returns the number of mismatches of each.
[[nodiscard]] std::vector<int> hammerFromThreads(InMemoryPreferences& store, int threads,
                                                 int rounds)
{
    std::vector<int> mismatches(static_cast<std::size_t>(threads), 0);
    {
        std::vector<std::jthread> running;
        running.reserve(mismatches.size());
        for (int thread = 0; thread < threads; thread++)
        {
            running.emplace_back(hammerTheStore, std::ref(store), thread, rounds,
                                 std::ref(mismatches[static_cast<std::size_t>(thread)]));
        }
    }  // joins
    return mismatches;
}

/// The names of the child nodes of @p node that hold something.
[[nodiscard]] Names childrenThatAreNotEmpty(const InMemoryPreferences& node)
{
    Names names;
    for (const std::string& name : node.childrenNames())
    {
        const InMemoryPreferences* child = node.findNode(name);
        if (child == nullptr || !child->empty())
        {
            names.push_back(name);
        }
    }
    return names;
}

// The store is used from several threads at once (see Preferences, "Threads"): a simulation on
// a worker thread reads it while the thread that owns it writes. Every node locks itself. The
// tsan preset is what finds an access that is not locked.
TEST(InMemoryPreferences, IsSafeToUseFromSeveralThreadsAtOnce)
{
    constexpr int kThreads = 4;
    constexpr int kRounds  = 400;

    InMemoryPreferences store;
    store.put("shared", "start");
    EXPECT_EQ(hammerFromThreads(store, kThreads, kRounds), std::vector<int>(kThreads, 0));

    // What is left: the key every thread wrote, with the last value one of them gave it, the
    // "done" key of each thread, and the three child nodes, empty again.
    EXPECT_EQ(store.keys(), (Names{"shared", "thread.0", "thread.1", "thread.2", "thread.3"}));
    EXPECT_EQ(store.get("shared"), std::to_string(kRounds - 1));
    ASSERT_NE(store.findNode("node"), nullptr);
    EXPECT_EQ(store.findNode("node")->childrenNames(), (Names{"0", "1", "2"}));
    EXPECT_EQ(childrenThatAreNotEmpty(*store.findNode("node")), Names{});
}

TEST(InMemoryPreferences, IsAPreferences)
{
    // The typed layer works through the abstract interface, and a child node is a full
    // Preferences of its own (java.util.prefs nodes are all alike).
    InMemoryPreferences store;
    Preferences&        prefs = store;
    prefs.putBoolean("flag", true);
    EXPECT_EQ(store.get("flag"), "true");
    EXPECT_TRUE(prefs.getBoolean("flag", false));

    Preferences& node = prefs.getNode("sub");
    node.putInt("n", 7);
    EXPECT_EQ(store.getNode("sub").getInt("n", 0), 7);
    EXPECT_EQ(prefs.getInNode("sub", "n"), "7");
    // The base overloads stay reachable on the concrete class.
    EXPECT_EQ(store.getInNode("sub", "n"), "7");
    EXPECT_EQ(store.getString("sub", "n", "-"), "7");
    EXPECT_EQ(store.getString("sub", "missing", "-"), "-");
}

TEST(InMemoryPreferences, TypedLayerDefaultsOnAnEmptyStore)
{
    // An empty store is OpenRocket's factory state; a few spot checks, the rest is in
    // PreferencesTests.
    const InMemoryPreferences prefs;
    EXPECT_TRUE(prefs.getLaunchIntoWind());
    EXPECT_EQ(prefs.getLaunchLatitude(), 28.61);
    EXPECT_EQ(prefs.getDefaultFlightConfigName(), "[{motors}]");
    EXPECT_EQ(prefs.getGravityModelName(), "WGS");
}

}  // namespace
