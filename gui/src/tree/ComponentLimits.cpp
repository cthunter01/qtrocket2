#include "QtRocket/gui/tree/ComponentLimits.h"

#include <cstddef>
#include <optional>

#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui
{

std::size_t ComponentLimits::treeSize(const RocketComponent& tree)
{
    return 1 + tree.getAllChildren().size();
}

std::optional<UserPrompts::Message> ComponentLimits::refusal(const OpenRocketDocument& document,
                                                             std::size_t    newComponents,
                                                             std::size_t    newDecalImages,
                                                             const QString& title)
{
    QStringList       lines;
    const std::size_t components =
        DocumentConfig::componentCount(document.getRocket()) + newComponents;
    if (newComponents > 0 && components > DocumentConfig::kMaxComponents)
    {
        //: QtRocket's own text (decision O27): a paste, duplicate, drop or add refused
        lines.append(tr("The rocket would have %1 components. QtRocket opens at most %2 "
                        "components of a design and would ignore the ones beyond when it opens "
                        "this design again.")
                         .arg(components)
                         .arg(DocumentConfig::kMaxComponents));
    }
    const std::size_t images = document.getDecalImageCount() + newDecalImages;
    if (newDecalImages > 0 && images > DocumentConfig::kMaxDecalImages)
    {
        //: QtRocket's own text (decision O27): a paste or drop of decal images refused
        lines.append(tr("The design would have %1 decal images. QtRocket opens at most %2 decal "
                        "images of a design and would ignore the decals of the ones beyond when "
                        "it opens this design again.")
                         .arg(images)
                         .arg(DocumentConfig::kMaxDecalImages));
    }
    if (lines.isEmpty())
    {
        return std::nullopt;
    }
    return UserPrompts::Message{
        .severity = UserPrompts::Severity::WARNING, .title = title, .lines = lines};
}

}  // namespace QtRocket::Gui
