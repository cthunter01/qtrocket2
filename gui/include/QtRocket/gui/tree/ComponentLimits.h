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
/// duplicate, a copying drop or an add button that would give the rocket more components than
/// the .ork loader reads back (DocumentConfig::kMaxComponents), or the document more decal
/// images (DocumentConfig::kMaxDecalImages), is refused with a message, before anything
/// changes; otherwise the design would save with the saver's warning that QtRocket will ignore
/// what is beyond when it opens the file again. OpenRocket has no such bound (decision U6 of
/// tier 9 made them the loader's).
///
/// The components are counted as DocumentConfig::componentCount() counts them, the Rocket not
/// counted; the images as OpenRocketDocument::getDecalImageCount() (the registry), which grows
/// by what ComponentCopy::countNewDecalImages() says.
///
/// Texts of QtRocket's own (OpenRocket has none), through tr().
class ComponentLimits
{
    Q_DECLARE_TR_FUNCTIONS(ComponentLimits)

public:
    ComponentLimits() = delete;

    /// The number of components of the tree @p tree, @p tree itself counted.
    [[nodiscard]] static std::size_t treeSize(const RocketComponent& tree);

    /// Why putting @p newComponents more components and @p newDecalImages more decal images into
    /// @p document is refused: a warning titled @p title with a line for each bound it would
    /// exceed; nullopt when both fit. Nothing new never exceeds a bound, even in a document
    /// that is beyond it already.
    [[nodiscard]] static std::optional<UserPrompts::Message> refusal(
        const OpenRocketDocument& document, std::size_t newComponents, std::size_t newDecalImages,
        const QString& title);
};

}  // namespace QtRocket::Gui
