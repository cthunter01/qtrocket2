#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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

    [[nodiscard]] static Children copyChildren(const Children& children);

    Values   m_values;
    Children m_children;
};

}  // namespace QtRocket
