// A design built as with OpenRocket's add buttons, of components ComponentFactory makes, saves as
// OpenRocket saves it.
//
// The reference is OpenRocket's (5f164fd0e, JDK 17): tools/openrocket-component-factory
// (ComponentFactoryDumper.java, after the M2 scout's AddProbe) clicks 25 times on the add buttons
// of OpenRocket's own design window (ComponentAddButtons; both insert preferences "add to the
// end", OpenRocket's start-up defaults loadDefaultComponentMaterials() loaded) in a new design
// (OpenRocketDocumentFactory.createNewRocket()). Its report, tests/data/component-factory/
// report.txt, gives in its section "== clicks" each click's undo text and where the new
// component went, and the tree the clicks built; add-buttons-rocket.ork.xml there is the
// rocket.ork of the archive OpenRocket's GeneralRocketSaver writes for the design, byte for byte.
//
// Here each click makes its component with ComponentFactory::create(), adds the undo position
// "Add <component name>" and attaches the component where OpenRocket's button did (which position
// a button chooses is the GUI's: Milestone 2, run 2). The design is saved with
// GeneralRocketSaver and its rocket.ork compared with OpenRocket's, line for line, after two
// normalisations of both texts (normalised()):
// - the text of every <id> element, a random UUID, becomes "#<n>", n numbering the distinct ids
//   from 1 in the order of their first appearance, so that a reference between ids would still
//   be compared (the design has none: no simulation, no flight configuration of its own);
// - the value of the creator attribute becomes "*": QtRocket writes its own name and version.
// Nothing else is normalised: rocket.ork holds no time stamp (the archive's entry times are not
// compared), and every number of the design is written as Java writes it.

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialPreferences.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/ComponentFactory.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "rocket/ComponentFactoryReport.h"

namespace
{

using QtRocket::ComponentFactory;
using QtRocket::ComponentKind;
using QtRocket::GeneralRocketSaver;
using QtRocket::InMemoryPreferences;
using QtRocket::MaterialStorage;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::ZipArchive;

/// The ids normalised() has met, in the order of their first appearance.
class IdNumbers
{
public:
    /// The number of @p id: 1 for the first id met, 2 for the second, ...
    [[nodiscard]] std::size_t numberOf(std::string_view id)
    {
        for (std::size_t i = 0; i < m_seen.size(); ++i)
        {
            if (m_seen[i] == id)
            {
                return i + 1;
            }
        }
        m_seen.emplace_back(id);
        return m_seen.size();
    }

private:
    std::vector<std::string> m_seen;
};

/// @p text with the text of each <id> element replaced by "#<n>" (IdNumbers), and the value of
/// each creator attribute by "*".
[[nodiscard]] std::string normalised(std::string_view text)
{
    constexpr std::string_view kIdOpen       = "<id>";
    constexpr std::string_view kIdClose      = "</id>";
    constexpr std::string_view kCreatorOpen  = "creator=\"";
    constexpr std::string_view kCreatorClose = "\"";
    constexpr std::size_t      kNone         = std::string_view::npos;
    IdNumbers                  ids;
    std::string                result;
    std::size_t                at = 0;
    while (at < text.size())
    {
        const std::size_t id      = text.find(kIdOpen, at);
        const std::size_t creator = text.find(kCreatorOpen, at);
        const bool        isId    = id != kNone && id <= creator;
        const std::size_t open    = isId ? id : creator;
        const std::size_t valueAt =
            open == kNone ? kNone : open + (isId ? kIdOpen.size() : kCreatorOpen.size());
        const std::size_t close =
            valueAt == kNone ? kNone : text.find(isId ? kIdClose : kCreatorClose, valueAt);
        if (close == kNone)
        {
            result += text.substr(at);
            break;
        }
        result += text.substr(at, valueAt - at);
        result += isId ? std::format("#{}", ids.numberOf(text.substr(valueAt, close - valueAt)))
                       : std::string("*");
        at = close;
    }
    return result;
}

TEST(AddButtonsSave, TheNormalisationNumbersTheIdsAndHidesTheCreator)
{
    EXPECT_EQ(normalised("<openrocket version=\"1.11\" creator=\"QtRocket 0.2.0\">\n"
                         "<id>b</id><id>a</id>\n<id>b</id><x>a</x>\n</openrocket>\n"),
              "<openrocket version=\"1.11\" creator=\"*\">\n"
              "<id>#1</id><id>#2</id>\n<id>#1</id><x>a</x>\n</openrocket>\n");
    EXPECT_EQ(normalised("no element <id>open"), "no element <id>open");
    EXPECT_EQ(normalised(""), "");
}

/// "name[child, child, ...]" of @p component and its subtree, as the dumper's tree() writes it.
[[nodiscard]] std::string treeOf(const RocketComponent& component)
{
    std::string tree = component.getName();
    if (component.getChildCount() > 0)
    {
        tree += "[";
        for (std::size_t i = 0; i < component.getChildCount(); ++i)
        {
            tree += (i > 0 ? ", " : "") + treeOf(component.getChild(i));
        }
        tree += "]";
    }
    return tree;
}

/// The add buttons of a design as the scenario clicks them: each click makes a component with
/// ComponentFactory::create(), adds the undo position, attaches the component and logs the click
/// as the report's section "== clicks" does.
class AddButtons
{
public:
    AddButtons()
    {
        QtRocket::addBuiltinMaterials(m_materials);
        QtRocket::loadDefaultComponentMaterials(m_preferences, m_materials);
    }

    [[nodiscard]] OpenRocketDocument&        document() noexcept { return *m_document; }
    [[nodiscard]] const InMemoryPreferences& preferences() const noexcept { return m_preferences; }
    /// The log of the clicks, then the tree of the rocket.
    [[nodiscard]] std::string logAndTree() const
    {
        return std::format("{}tree={}\n", m_log, treeOf(m_document->getRocket()));
    }

    /// The button of @p kind clicked while @p selected is selected, which puts the new component
    /// at the end of @p parent (where OpenRocket's button put it). Returns the new component.
    RocketComponent& click(ComponentKind kind, const RocketComponent& selected,
                           RocketComponent& parent)
    {
        std::unique_ptr<RocketComponent> component =
            ComponentFactory::create(kind, m_preferences, m_materials);
        m_document->addUndoPosition("Add " + component->getComponentName());
        RocketComponent&                 added    = parent.addChild(std::move(component));
        const std::optional<std::size_t> position = parent.getChildPosition(&added);
        m_log +=
            std::format("{} on {}: undo='{}' selected={} parent={}@{}\n", QtRocket::className(kind),
                        selected.getName(), m_document->getUndoDescription().value_or("null"),
                        added.getName(), parent.getName(), position.value_or(0));
        return added;
    }

private:
    InMemoryPreferences                 m_preferences;
    MaterialStorage                     m_materials;
    std::unique_ptr<OpenRocketDocument> m_document = OpenRocketDocumentFactory::createNewRocket();
    std::string                         m_log;
};

/// The 25 clicks of the scenario, in the order of ComponentFactoryDumper.clicks().
void clickTheScenario(AddButtons& buttons)
{
    RocketComponent&       rocket    = buttons.document().getRocket();
    RocketComponent&       sustainer = rocket.getChild(0);
    const RocketComponent& nose = buttons.click(ComponentKind::NOSE_CONE, sustainer, sustainer);
    RocketComponent&       tube = buttons.click(ComponentKind::BODY_TUBE, nose, sustainer);
    const RocketComponent& transition = buttons.click(ComponentKind::TRANSITION, tube, sustainer);
    static_cast<void>(buttons.click(ComponentKind::BODY_TUBE, transition, sustainer));
    for (const ComponentKind kind :
         {ComponentKind::TRAPEZOID_FIN_SET, ComponentKind::ELLIPTICAL_FIN_SET,
          ComponentKind::FREEFORM_FIN_SET, ComponentKind::TUBE_FIN_SET, ComponentKind::RAIL_BUTTON,
          ComponentKind::LAUNCH_LUG, ComponentKind::INNER_TUBE, ComponentKind::TUBE_COUPLER,
          ComponentKind::CENTERING_RING, ComponentKind::BULKHEAD, ComponentKind::ENGINE_BLOCK,
          ComponentKind::PARACHUTE, ComponentKind::STREAMER, ComponentKind::SHOCK_CORD,
          ComponentKind::MASS_COMPONENT})
    {
        static_cast<void>(buttons.click(kind, tube, tube));
    }
    RocketComponent& pods     = buttons.click(ComponentKind::POD_SET, tube, tube);
    RocketComponent& boosters = buttons.click(ComponentKind::PARALLEL_STAGE, tube, tube);
    RocketComponent& stage    = buttons.click(ComponentKind::AXIAL_STAGE, rocket, rocket);
    static_cast<void>(buttons.click(ComponentKind::BODY_TUBE, stage, stage));
    static_cast<void>(buttons.click(ComponentKind::BODY_TUBE, pods, pods));
    static_cast<void>(buttons.click(ComponentKind::NOSE_CONE, boosters, boosters));
}

/// OpenRocket's rocket.ork of the design (tests/data/component-factory).
[[nodiscard]] std::string openRocketsDesign()
{
    const std::filesystem::path path =
        QtRocket::Test::testDataDir() / "component-factory" / "add-buttons-rocket.ork.xml";
    const Result<std::vector<std::byte>> bytes = QtRocket::readFile(path);
    EXPECT_TRUE(bytes.has_value()) << bytes.error().toString();
    return bytes.has_value() ? QtRocket::bytesToString(*bytes) : std::string();
}

/// The rocket.ork of @p bytes, a design archive, which must be its only entry; empty (and a
/// failure) otherwise.
[[nodiscard]] std::string theOnlyDesignOf(const std::vector<std::byte>& bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive.has_value() || archive->entries().size() != 1 ||
        archive->find("rocket.ork") == nullptr)
    {
        ADD_FAILURE() << "not an archive of rocket.ork alone";
        return {};
    }
    return QtRocket::bytesToString(*archive->find("rocket.ork"));
}

TEST(AddButtonsSave, TheClicksGiveOpenRocketsUndoTextsAndPlaces)
{
    AddButtons buttons;
    clickTheScenario(buttons);
    EXPECT_EQ(buttons.logAndTree(), QtRocket::Test::componentFactoryReportBlock(
                                        "== clicks (both insert preferences 'add to the end')"));
}

TEST(AddButtonsSave, TheDesignSavesAsOpenRocketsSave)
{
    AddButtons buttons;
    clickTheScenario(buttons);
    GeneralRocketSaver                   saver(buttons.preferences());
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(buttons.document(), buttons.document().getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_TRUE(saver.getWarnings().empty());

    const std::string expected = openRocketsDesign();
    ASSERT_FALSE(expected.empty());
    EXPECT_EQ(normalised(theOnlyDesignOf(*bytes)), normalised(expected));
}

}  // namespace
