#pragma once

// The design of the scouting's Java probes of the component actions and of drag and drop
// (ActionsProbe and DndProbe of the M2 component-tree scout, ClipboardProbe of Milestone 2 run 2,
// part T3: OpenRocket 5f164fd0e), and the texts they print, so that a test compares what QtRocket
// does with OpenRocket's transcript line for line.

#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "QtRocket/util/Uuid.h"

namespace QtRocket
{
class ComponentCopy;
class OpenRocketDocument;
class Rocket;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui::Test
{

/// A new document (OpenRocketDocumentFactory::createNewRocket(), stage "Sustainer") whose stage
/// holds NC (a nose cone), BT1 (a body tube with IT, an inner tube holding EB, an engine block;
/// CR1, a centering ring; and Fins, a trapezoidal fin set) and BT2 (a body tube with Chute, a
/// parachute), built as the probes build it (each component named, then added), and, after the
/// stage, a second stage "Booster" as the probe has it.
class ProbeDesign
{
public:
    /// What follows the Sustainer.
    enum class Booster
    {
        /// Nothing: the single-stage design of ActionsProbe's "delete the only stage" and
        /// "copy [BT1] in one document, paste into another".
        NONE,
        /// An empty stage "Booster" (DndProbe).
        EMPTY,
        /// The stage "Booster" with BBT, a body tube holding "Side boosters", a booster set with
        /// PBT, a body tube (ActionsProbe, ClipboardProbe).
        WITH_SIDE_BOOSTERS,
    };

    explicit ProbeDesign(Booster booster = Booster::WITH_SIDE_BOOSTERS);
    ~ProbeDesign();

    ProbeDesign(const ProbeDesign&)            = delete;
    ProbeDesign& operator=(const ProbeDesign&) = delete;
    ProbeDesign(ProbeDesign&&)                 = delete;
    ProbeDesign& operator=(ProbeDesign&&)      = delete;

    [[nodiscard]] OpenRocketDocument& document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&             rocket() const;

    /// The first component of the rocket in tree order named @p name, now (an undo replaces
    /// every object); a test failure and the Rocket when there is none.
    [[nodiscard]] RocketComponent& find(std::string_view name) const;

    /// find() of each of @p names, in their order.
    [[nodiscard]] std::vector<RocketComponent*> find(
        std::initializer_list<std::string_view> names) const;

    /// The probes' tree(): "Name[child, child[...]]", with '*' after the name of a component
    /// that is not one of the design as built (a copy: its id is new).
    [[nodiscard]] std::string tree(const RocketComponent& component) const;

    /// tree() of the rocket.
    [[nodiscard]] std::string tree() const;

    /// Whether @p component is one of the design as built (by its id).
    [[nodiscard]] bool isOriginal(const RocketComponent& component) const;

    /// The DndProbe's tree(): the same without the '*'.
    [[nodiscard]] static std::string plainTree(const RocketComponent& component);

    /// "[Name, Name*]" of @p components, as Java prints a list of names.
    [[nodiscard]] std::string names(const std::vector<const RocketComponent*>& components) const;

    /// names() of @p components.
    [[nodiscard]] std::string names(const std::vector<RocketComponent*>& components) const;

    /// The probes' clip(): "[tree, tree]" of what @p copy holds.
    [[nodiscard]] std::string clipboard(const ComponentCopy& copy) const;

private:
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unordered_set<Uuid>            m_originalIds;
};

}  // namespace QtRocket::Gui::Test
