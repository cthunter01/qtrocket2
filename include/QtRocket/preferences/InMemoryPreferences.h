#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/preferences/Preferences.h"

namespace QtRocket
{

/// A Preferences store held in memory: a sorted map of string values and a map of named child
/// nodes, each an InMemoryPreferences of its own. It is what the core and the tests use, and the
/// preferences a new document takes its defaults from (every typed getter of Preferences gives
/// OpenRocket's default when its key is absent, so an empty store is OpenRocket's factory
/// state). The GUI adds a QSettings-backed store for the user's persistent preferences.
///
/// A copy is a snapshot: it holds the same values and child nodes (copied recursively) but none
/// of the connections to changed(), which stay with the original; assigning a snapshot back
/// restores the values and nodes without touching the connections either. reset() empties the
/// whole subtree, which is the way back to the factory state.
///
/// Node references from getNode() and findNode() stay valid until the child is removed by
/// reset() or an assignment, or its parent is destroyed.
///
/// Threads: every node guards its values and its list of child nodes with a mutex of its own,
/// so that the store may be read and written from several threads at once, as java.util.prefs
/// may (see Preferences, "Threads"): a simulation that runs on a worker thread reads the store
/// it was made with while the thread that owns the store goes on writing to it. Every method
/// is safe to call at any time, with the one limit that node references have anyway: reset()
/// and an assignment destroy child nodes, so they must not run while another thread still uses
/// a reference to a child (or copies or compares the node, which walks the children). A lock
/// is held for one node at a time, or for a node and then its children while a snapshot is
/// taken; never the other way round, so the methods cannot deadlock each other.
class InMemoryPreferences final : public Preferences
{
public:
    InMemoryPreferences()           = default;
    ~InMemoryPreferences() override = default;

    /// A snapshot of @p other (see the class comment).
    InMemoryPreferences(const InMemoryPreferences& other);
    /// Replaces this node's values and child nodes with a snapshot of @p other's.
    InMemoryPreferences& operator=(const InMemoryPreferences& other);

    InMemoryPreferences(InMemoryPreferences&&)            = delete;
    InMemoryPreferences& operator=(InMemoryPreferences&&) = delete;

    [[nodiscard]] std::optional<std::string> get(std::string_view key) const override;
    void put(std::string_view key, std::string_view value) override;
    void remove(std::string_view key) override;
    void clear() override;
    [[nodiscard]] std::vector<std::string>   keys() const override;
    [[nodiscard]] std::vector<std::string>   childrenNames() const override;
    [[nodiscard]] InMemoryPreferences&       getNode(std::string_view name) override;
    [[nodiscard]] const InMemoryPreferences* findNode(
        std::string_view name) const noexcept override;

    /// Removes every key and every child node of this node, recursively: back to the factory
    /// state. References to the removed nodes dangle.
    void reset();

    /// True when this node holds no keys and no child nodes.
    [[nodiscard]] bool empty() const noexcept;

    /// Two nodes are equal when they hold the same keys with the same values and the same child
    /// names with equal children; the connections to changed() do not count.
    [[nodiscard]] bool operator==(const InMemoryPreferences& other) const;

private:
    using Values   = std::map<std::string, std::string, std::less<>>;
    using Children = std::map<std::string, std::unique_ptr<InMemoryPreferences>, std::less<>>;

    /// What a node holds at one moment: a copy of its values, and its children by name.
    struct Contents
    {
        Values                                                               values;
        std::vector<std::pair<std::string_view, const InMemoryPreferences*>> children;
    };

    /// The copy constructor's work, done while the caller holds @p lock on @p other's mutex.
    InMemoryPreferences(const InMemoryPreferences& other, const std::scoped_lock<std::mutex>& lock);

    [[nodiscard]] static Children copyChildren(const Children& children);

    /// The contents of this node, read under its lock. The child pointers and their names stay
    /// valid as node references do (see the class comment).
    [[nodiscard]] Contents contents() const;

    /// Guards m_values and m_children.
    mutable std::mutex m_mutex;
    Values             m_values;
    Children           m_children;
};

}  // namespace QtRocket
