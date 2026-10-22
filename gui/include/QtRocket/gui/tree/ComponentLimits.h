#pragma once

#include <cstddef>
#include <optional>

#include <QCoreApplication>
#include <QString>

#include "QtRocket/gui/util/UserPrompts.h"

namespace QtRocket
{
class OpenRocketDocument;
class RocketComponent;
}  // namespace QtRocket

namespace QtRocket::Gui
{

/// The bounds the GUI keeps when it puts components into a design (decision O27): a paste, a
/// duplicate, a drop or an add button that would give the design more than the .ork loader reads
/// back is refused with a message, before anything changes; otherwise the design would save with
/// the saver's warning that QtRocket will ignore what is beyond when it opens the file again. The
/// bounds are the loader's (decision U6 of tier 9; OpenRocket has none):
/// - the components of the rocket (DocumentConfig::kMaxComponents), counted as
///   DocumentConfig::componentCount() counts them, the Rocket not counted (decision O27);
/// - how deep a component stands below the Rocket (ComponentHandler::kMaxDepth, a stage at level
///   1), for components that are moved as for new ones (an extension of O27 by the M2 run 2
///   review);
/// - the stages, booster sets and pod sets of the rocket (DocumentConfig::kMaxAssemblies),
///   counted as DocumentConfig::assemblyCount() counts them (the same extension);
/// - the decal images of the document (DocumentConfig::kMaxDecalImages), counted as
///   OpenRocketDocument::getDecalImageCount() (the registry) counts them, which grows by what
///   ComponentCopy::countNewDecalImages() says.
/// The bounds of the component instances (DocumentConfig::kMaxInstances and kMaxCount) are not
/// kept here: the configuration dialog's edits of a count can exceed them as well.
///
/// Texts of QtRocket's own (OpenRocket has none), through tr().
class ComponentLimits
{
    Q_DECLARE_TR_FUNCTIONS(ComponentLimits)

public:
    ComponentLimits() = delete;

    /// What an operation puts into a design, as the bounds count it.
    struct Addition
    {
        /// The new components, each tree with its root (treeSize()).
        std::size_t components = 0;
        /// The stages, booster sets and pod sets among them.
        std::size_t assemblies = 0;
        /// The decal images the document would register for them.
        std::size_t decalImages = 0;
        /// The level below the Rocket of the deepest component the operation puts somewhere,
        /// new or moved (a stage at 1, its body tube at 2); 0 for none.
        std::size_t deepestLevel = 0;

        /// Adds @p other to this: the counts summed, the deeper of the two levels kept.
        Addition& operator+=(const Addition& other) noexcept;
    };

    /// The number of components of the tree @p tree, @p tree itself counted.
    [[nodiscard]] static std::size_t treeSize(const RocketComponent& tree);

    /// How many components stand above @p component in its tree: 0 for the Rocket (or the root of
    /// a tree without one), 1 for a stage.
    [[nodiscard]] static std::size_t levelOf(const RocketComponent& component);

    /// How many levels the tree @p tree reaches below its root: 0 for a component without
    /// children.
    [[nodiscard]] static std::size_t heightOf(const RocketComponent& tree);

    /// The addition of the new tree @p tree, put as a child of @p parent: its components, the
    /// assemblies among them and the level of its deepest component (levelOf(parent) + 1 +
    /// heightOf(tree)); no decal images.
    [[nodiscard]] static Addition placing(const RocketComponent& tree,
                                          const RocketComponent& parent);

    /// What moving the tree @p tree, a component of the rocket, to @p parent adds: nothing but
    /// the level its deepest component would have there (a move keeps the components, and the
    /// rocket's assemblies with them).
    [[nodiscard]] static Addition moving(const RocketComponent& tree,
                                         const RocketComponent& parent);

    /// Why putting @p addition into @p document is refused: a warning titled @p title with a line
    /// for each bound it would exceed; nullopt when it stays within all. Nothing new never
    /// exceeds a count, even in a document that is beyond its bound already, and a component is
    /// too deep only where the addition puts it.
    [[nodiscard]] static std::optional<UserPrompts::Message> refusal(
        const OpenRocketDocument& document, const Addition& addition, const QString& title);
};

}  // namespace QtRocket::Gui
