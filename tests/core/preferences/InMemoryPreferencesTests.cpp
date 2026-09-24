#include "QtRocket/preferences/InMemoryPreferences.h"

#include <optional>
#include <string>
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
