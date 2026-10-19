#include "tree/TreeEditingSupport.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"

namespace QtRocket::Gui::Test
{

std::string javaBool(bool value)
{
    return value ? "true" : "false";
}

std::string undoText(const OpenRocketDocument& document)
{
    return "undo='" + document.getUndoDescription().value_or("null") + "'";
}

std::string redoText(const OpenRocketDocument& document)
{
    return "redo='" + document.getRedoDescription().value_or("null") + "'";
}

std::vector<std::string> linesOf(std::string_view text)
{
    std::vector<std::string> lines;
    std::size_t              start = 0;
    while (start <= text.size())
    {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        lines.emplace_back(text.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

void fillUpTo(Rocket& rocket, RocketComponent& parent, std::size_t count)
{
    rocket.freeze();
    while (DocumentConfig::componentCount(rocket) < count)
    {
        parent.addChild(std::make_unique<MassComponent>());
    }
    rocket.thaw();
}

std::vector<std::string> decalListOf(const OpenRocketDocument& document)
{
    const std::vector<std::shared_ptr<DecalImage>> images = document.getDecalList();
    std::vector<std::string>                       names;
    names.reserve(images.size());
    for (const std::shared_ptr<DecalImage>& image : images)
    {
        names.push_back(image->getName());
    }
    return names;
}

const RocketComponent* firstBodyComponentWithDecal(const OpenRocketDocument& document)
{
    for (const RocketComponent& component : document.getRocket().subtree())
    {
        const std::optional<Appearance>& appearance = component.getAppearance();
        if (isBodyComponent(component.kind()) && appearance.has_value() &&
            appearance->getTexture().has_value())
        {
            return &component;
        }
    }
    return nullptr;
}

std::vector<std::string> archiveEntries(const std::vector<std::byte>& bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive.has_value())
    {
        ADD_FAILURE() << archive.error().toString();
        return {};
    }
    std::vector<std::string> entries;
    entries.reserve(archive->entries().size());
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        entries.push_back(entry.name == "rocket.ork"
                              ? entry.name
                              : entry.name + " " + std::to_string(entry.data.size()) + " bytes");
    }
    return entries;
}

std::vector<std::string> decalProbeArchiveEntries()
{
    return {"rocket.ork",
            "/datafiles/textures/balsa.jpg 54032 bytes",
            "/datafiles/textures/chute.jpg 6686 bytes",
            "/datafiles/textures/hardboard.jpg 12599 bytes",
            "/datafiles/textures/spiral-wound-alpha.png 2044 bytes",
            "/datafiles/textures/wadding.png 12495 bytes",
            "/datafiles/textures/wood.jpg 43305 bytes"};
}

std::vector<std::byte> savedWithoutWarnings(OpenRocketDocument& document)
{
    GeneralRocketSaver             saver(QtRocket::Test::goldenExampleEnvironment().preferences());
    Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(document, document.getDefaultStorageOptions());
    if (!bytes.has_value())
    {
        ADD_FAILURE() << bytes.error().toString();
        return {};
    }
    EXPECT_EQ(saver.getMissingDecal(), nullptr);
    EXPECT_TRUE(saver.getWarnings().empty());
    return std::move(*bytes);
}

}  // namespace QtRocket::Gui::Test
