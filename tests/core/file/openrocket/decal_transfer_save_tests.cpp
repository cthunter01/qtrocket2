// A design that received components of another design, with the decal images they show
// (ComponentCopy), saves with those images and loads them again.
//
// The scenario is the M2 scout's decal probe: the first body component with a decal of
// OpenRocket's example "Parallel booster staging" (its body tube, which holds the booster set,
// the fins, the recovery and a rail button) is pasted into the stage of a new design, which is
// saved. Before ComponentCopy the save failed ("Could not find decal source file
// '/datafiles/textures/balsa.jpg'"), and without telling which image was missing. The Java
// lines are those of DecalTransferProbe.java of Milestone 2 (5f164fd0e, JDK 17), which does the
// same with OpenRocket's own classes; the comments name its sections.

#include <array>
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

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AppearanceBuilder.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "document/TestAttachments.h"
#include "file/DesignFileEnvironment.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::ComponentCopy;
using QtRocket::DecalImage;
using QtRocket::GeneralRocketLoader;
using QtRocket::GeneralRocketSaver;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::ZipArchive;
using QtRocket::Test::DesignFileEnvironment;
using Placement = ComponentCopy::Placement;

/// "== probe", "saved:": the entries of OpenRocket's archive, an image with its size.
constexpr std::array<std::string_view, 7> kJavaEntries{
    "rocket.ork",
    "/datafiles/textures/balsa.jpg 54032 bytes",
    "/datafiles/textures/chute.jpg 6686 bytes",
    "/datafiles/textures/hardboard.jpg 12599 bytes",
    "/datafiles/textures/spiral-wound-alpha.png 2044 bytes",
    "/datafiles/textures/wadding.png 12495 bytes",
    "/datafiles/textures/wood.jpg 43305 bytes",
};

/// "== probe", "reloaded decals:": the decals of the reloaded design, each with the size of its
/// image (Java prints its CRC too; the bytes are compared with the source's here).
constexpr std::array<std::string_view, 16> kJavaReloadedDecals{
    "Body Tube outside /datafiles/textures/spiral-wound-alpha.png (2044 bytes)",
    "Body Tube inside /datafiles/textures/spiral-wound-alpha.png (2044 bytes)",
    "Booster Body Tube outside /datafiles/textures/spiral-wound-alpha.png (2044 bytes)",
    "Booster Body Tube inside /datafiles/textures/spiral-wound-alpha.png (2044 bytes)",
    "Trapezoidal Fin Set outside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Trapezoidal Fin Set inside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Trapezoidal Fin Set outside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Trapezoidal Fin Set inside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Centering Ring outside /datafiles/textures/wood.jpg (43305 bytes)",
    "Centering Ring outside /datafiles/textures/wood.jpg (43305 bytes)",
    "Booster Motor Tube outside /datafiles/textures/spiral-wound-alpha.png (2044 bytes)",
    "Engine Block outside /datafiles/textures/hardboard.jpg (12599 bytes)",
    "Trapezoidal Fin Set outside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Trapezoidal Fin Set inside /datafiles/textures/balsa.jpg (54032 bytes)",
    "Shock Cord outside /datafiles/textures/wadding.png (12495 bytes)",
    "Parachute, 36 in., nylon, 8 lines outside /datafiles/textures/chute.jpg (6686 bytes)",
};

/// The example the scenario copies from.
constexpr std::string_view kExample = "Parallel booster staging.ork";

/// @p lines as a vector of strings, for comparing with what a test collects.
template <std::size_t N>
[[nodiscard]] std::vector<std::string> linesOf(const std::array<std::string_view, N>& lines)
{
    return {lines.begin(), lines.end()};
}

/// What the tests need: the loading environment, the example loaded and its first body
/// component with a decal.
class Scenario
{
public:
    Scenario() : m_source(load(QtRocket::Test::dataDir() / "examples" / kExample))
    {
        for (const RocketComponent& component : m_source.document->getRocket().subtree())
        {
            const std::optional<Appearance>& appearance = component.getAppearance();
            if (QtRocket::isBodyComponent(component.kind()) && appearance.has_value() &&
                appearance->getTexture().has_value())
            {
                m_withDecal = &component;
                break;
            }
        }
    }

    [[nodiscard]] OpenRocketDocument&    source() const noexcept { return *m_source.document; }
    [[nodiscard]] const RocketComponent& withDecal() const noexcept { return *m_withDecal; }

    /// A copy of the body tube with its images, as the clipboard keeps it.
    [[nodiscard]] ComponentCopy copyOfBodyTube() const
    {
        Result<ComponentCopy> copy = ComponentCopy::copyOf(source(), {m_withDecal});
        EXPECT_TRUE(copy.has_value());
        return copy.has_value() ? std::move(*copy) : ComponentCopy();
    }

    /// The design file @p file, loaded.
    [[nodiscard]] LoadedDocument load(const std::filesystem::path& file) const
    {
        Result<LoadedDocument> loaded = GeneralRocketLoader(m_environment.context()).load(file);
        EXPECT_TRUE(loaded.has_value()) << (loaded.has_value() ? "" : loaded.error().toString());
        return std::move(loaded).value();
    }

    /// The design that @p bytes, a saved archive, hold, loaded.
    [[nodiscard]] LoadedDocument load(std::vector<std::byte> bytes) const
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context()).load(std::move(bytes));
        EXPECT_TRUE(loaded.has_value()) << (loaded.has_value() ? "" : loaded.error().toString());
        return std::move(loaded).value();
    }

    /// @p document saved; the save must succeed without warnings.
    [[nodiscard]] std::vector<std::byte> save(OpenRocketDocument& document) const
    {
        GeneralRocketSaver             saver(m_environment.preferences());
        Result<std::vector<std::byte>> bytes =
            saver.saveToBytes(document, document.getDefaultStorageOptions());
        EXPECT_TRUE(bytes.has_value()) << (bytes.has_value() ? "" : bytes.error().toString());
        EXPECT_EQ(saver.getMissingDecal(), nullptr);
        EXPECT_TRUE(saver.getWarnings().empty());
        return bytes.has_value() ? std::move(*bytes) : std::vector<std::byte>{};
    }

private:
    DesignFileEnvironment  m_environment;
    LoadedDocument         m_source;
    const RocketComponent* m_withDecal{nullptr};
};

/// The entries of the archive @p bytes as kJavaEntries writes them.
[[nodiscard]] std::vector<std::string> entriesOf(const std::vector<std::byte>& bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    EXPECT_TRUE(archive.has_value());
    std::vector<std::string> entries;
    if (!archive.has_value())
    {
        return entries;
    }
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        entries.push_back(entry.name == "rocket.ork"
                              ? entry.name
                              : std::format("{} {} bytes", entry.name, entry.data.size()));
    }
    return entries;
}

/// The size of the image named @p name of @p document, or "no image".
[[nodiscard]] std::string sizeOf(const OpenRocketDocument& document, const std::string& name)
{
    const std::shared_ptr<DecalImage> image = document.findDecalImage(name);
    if (image == nullptr)
    {
        return "no image";
    }
    const auto bytes = image->getBytes();
    return bytes.has_value() ? std::format("{} bytes", bytes->size()) : "unreadable";
}

/// Adds "<component> <side> <image> (<size>)" for the decal of @p appearance, when it has one.
void addDecalLine(std::vector<std::string>& lines, const OpenRocketDocument& document,
                  const RocketComponent& component, std::string_view side,
                  const std::optional<Appearance>& appearance)
{
    if (appearance.has_value() && appearance->getTexture().has_value())
    {
        const std::string& name = appearance->getTexture()->getImageName();
        lines.push_back(
            std::format("{} {} {} ({})", component.getName(), side, name, sizeOf(document, name)));
    }
}

/// The decals of @p document's rocket as kJavaReloadedDecals lists them.
[[nodiscard]] std::vector<std::string> decalLines(const OpenRocketDocument& document)
{
    std::vector<std::string> lines;
    for (const RocketComponent& component : document.getRocket().subtree())
    {
        addDecalLine(lines, document, component, "outside", component.getAppearance());
        if (const auto* const inside =
                dynamic_cast<const QtRocket::InsideColorComponent*>(&component))
        {
            addDecalLine(lines, document, component, "inside",
                         inside->getInsideColorComponentHandler().getInsideAppearance());
        }
    }
    return lines;
}

/// The names of @p document's decal list.
[[nodiscard]] std::vector<std::string> listOf(const OpenRocketDocument& document)
{
    std::vector<std::string> names;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        names.push_back(image->getName());
    }
    return names;
}

/// Whether the image named @p name has the same bytes in @p a as in @p b.
[[nodiscard]] bool haveSameBytes(const OpenRocketDocument& a, const OpenRocketDocument& b,
                                 std::string_view name)
{
    const std::shared_ptr<DecalImage> first  = a.findDecalImage(name);
    const std::shared_ptr<DecalImage> second = b.findDecalImage(name);
    if (first == nullptr || second == nullptr)
    {
        return false;
    }
    const auto bytesA = first->getBytes();
    const auto bytesB = second->getBytes();
    return bytesA.has_value() && bytesB.has_value() && *bytesA == *bytesB;
}

/// A fresh copy of @p clipboard pasted at the end of the first stage of @p target, after the
/// undo position, as the GUI's paste does.
void pasteIntoFirstStage(const ComponentCopy& clipboard, OpenRocketDocument& target)
{
    RocketComponent& stage = target.getRocket().getChild(0);
    target.addUndoPosition("Paste Body Tube");
    const std::vector<RocketComponent*> pasted = clipboard.copy().insertInto(
        target, {Placement{.component = 0, .parent = &stage, .index = stage.getChildCount()}});
    EXPECT_EQ(pasted.size(), 1U);
}

/// The six image names of the example.
[[nodiscard]] std::vector<std::string> sixImages()
{
    return {"/datafiles/textures/balsa.jpg",     "/datafiles/textures/chute.jpg",
            "/datafiles/textures/hardboard.jpg", "/datafiles/textures/spiral-wound-alpha.png",
            "/datafiles/textures/wadding.png",   "/datafiles/textures/wood.jpg"};
}

/// The names of the six images whose bytes differ between @p a and @p b, or that one of them
/// lacks.
[[nodiscard]] std::vector<std::string> imagesWithOtherBytes(const OpenRocketDocument& a,
                                                            const OpenRocketDocument& b)
{
    std::vector<std::string> names = sixImages();
    std::erase_if(names, [&a, &b](const std::string& name) { return haveSameBytes(a, b, name); });
    return names;
}

/// kJavaReloadedDecals as the design of the name clash has them: its own nose cone first, and
/// the pasted balsa image under its new name "decals//datafiles/textures/balsa.jpg".
[[nodiscard]] std::vector<std::string> decalLinesAfterTheClash()
{
    std::vector<std::string> lines{"Nose Cone outside /datafiles/textures/balsa.jpg (22 bytes)"};
    for (const std::string_view line : kJavaReloadedDecals)
    {
        std::string       text(line);
        const std::size_t at = text.find("/datafiles/textures/balsa.jpg");
        if (at != std::string::npos)
        {
            text.insert(at, "decals/");
        }
        lines.push_back(std::move(text));
    }
    return lines;
}

// "== probe".
TEST(DecalTransferSave, ThePastedDesignSavesAllSixImagesAndLoadsThemAgain)
{
    const Scenario scenario;
    EXPECT_EQ(scenario.withDecal().getName(), "Body Tube");
    const ComponentCopy                       clipboard = scenario.copyOfBodyTube();
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();

    pasteIntoFirstStage(clipboard, *target);
    // Java: "target after the paste decal list=[]".
    EXPECT_EQ(listOf(*target), sixImages());

    std::vector<std::byte> bytes = scenario.save(*target);
    EXPECT_EQ(entriesOf(bytes), linesOf(kJavaEntries));

    const LoadedDocument again = scenario.load(std::move(bytes));
    EXPECT_EQ(listOf(*again.document), sixImages());
    EXPECT_EQ(decalLines(*again.document), linesOf(kJavaReloadedDecals));
    EXPECT_EQ(imagesWithOtherBytes(*again.document, scenario.source()), std::vector<std::string>{});
}

// The source closed before the paste: the clipboard keeps the images with the archive's bytes.
TEST(DecalTransferSave, TheSourceMayBeClosedBeforeThePaste)
{
    auto                scenario  = std::make_unique<Scenario>();
    const ComponentCopy clipboard = scenario->copyOfBodyTube();
    scenario.reset();

    const Scenario                            saving;
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    pasteIntoFirstStage(clipboard, *target);
    std::vector<std::byte> bytes = saving.save(*target);
    EXPECT_EQ(entriesOf(bytes), linesOf(kJavaEntries));
    const LoadedDocument again = saving.load(std::move(bytes));
    EXPECT_EQ(decalLines(*again.document), linesOf(kJavaReloadedDecals));
}

// "== clash": OpenRocket writes one "/datafiles/textures/balsa.jpg", the target's 22 bytes, and
// the pasted fin sets come back with them ("Trapezoidal Fin Set outside
// /datafiles/textures/balsa.jpg (22 bytes, ...)"). Here the pasted image is renamed and both are
// saved.
TEST(DecalTransferSave, ANameClashSavesBothImages)
{
    const Scenario                            scenario;
    const ComponentCopy                       clipboard = scenario.copyOfBodyTube();
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    RocketComponent&                          stage  = target->getRocket().getChild(0);
    RocketComponent&            nose = stage.addChild(std::make_unique<QtRocket::NoseCone>());
    QtRocket::AppearanceBuilder builder;
    builder.setImage(target
                         ->getDecalImage(std::make_shared<QtRocket::Test::MemoryAttachment>(
                             "/datafiles/textures/balsa.jpg", "the target's own balsa"))
                         ->getName());
    nose.setAppearance(builder.getAppearance());

    pasteIntoFirstStage(clipboard, *target);
    std::vector<std::byte> bytes = scenario.save(*target);
    EXPECT_EQ(entriesOf(bytes),
              (std::vector<std::string>{"rocket.ork", "/datafiles/textures/balsa.jpg 22 bytes",
                                        "/datafiles/textures/chute.jpg 6686 bytes",
                                        "/datafiles/textures/hardboard.jpg 12599 bytes",
                                        "/datafiles/textures/spiral-wound-alpha.png 2044 bytes",
                                        "/datafiles/textures/wadding.png 12495 bytes",
                                        "/datafiles/textures/wood.jpg 43305 bytes",
                                        "decals//datafiles/textures/balsa.jpg 54032 bytes"}));

    const LoadedDocument again = scenario.load(std::move(bytes));
    EXPECT_EQ(decalLines(*again.document), decalLinesAfterTheClash());
}

// "== undo": OpenRocket's list stays "[]", and the save after the redo has the six images.
// Here the undone paste saves without them (nothing shows them), and the redone one with them.
TEST(DecalTransferSave, AnUndoneAndRedonePasteSaves)
{
    const Scenario                            scenario;
    const ComponentCopy                       clipboard = scenario.copyOfBodyTube();
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    pasteIntoFirstStage(clipboard, *target);

    target->undo();
    EXPECT_EQ(target->getRocket().getChild(0).getChildCount(), 0U);
    EXPECT_TRUE(listOf(*target).empty());
    EXPECT_EQ(entriesOf(scenario.save(*target)), std::vector<std::string>{"rocket.ork"});

    target->redo();
    EXPECT_EQ(listOf(*target), sixImages());
    EXPECT_EQ(entriesOf(scenario.save(*target)), linesOf(kJavaEntries));
}

// "== twice": one entry per image.
TEST(DecalTransferSave, TwoPastesSaveEachImageOnce)
{
    const Scenario                            scenario;
    const ComponentCopy                       clipboard = scenario.copyOfBodyTube();
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    pasteIntoFirstStage(clipboard, *target);
    pasteIntoFirstStage(clipboard, *target);
    EXPECT_EQ(listOf(*target), sixImages());
    EXPECT_EQ(entriesOf(scenario.save(*target)), linesOf(kJavaEntries));
}

// "== same": "list before 6 after 6", the target's images are other objects of the same names
// and bytes. Here they are shared too (DecalTransfer.AnImageOfTheSameNameAndBytesIsShared). The
// example's own motors are saved too ("thrustcurves/..."), which the probe, loading without a
// motor database, did not find; they are left out of the comparison.
TEST(DecalTransferSave, ADesignLoadedFromTheSameFileKeepsItsImages)
{
    const Scenario       scenario;
    const ComponentCopy  clipboard = scenario.copyOfBodyTube();
    const LoadedDocument target = scenario.load(QtRocket::Test::dataDir() / "examples" / kExample);
    const std::vector<std::shared_ptr<DecalImage>> before = target.document->getDecalList();

    pasteIntoFirstStage(clipboard, *target.document);
    EXPECT_EQ(target.document->getDecalList(), before);
    std::vector<std::string> entries = entriesOf(scenario.save(*target.document));
    std::erase_if(entries,
                  [](const std::string& entry) { return entry.starts_with("thrustcurves/"); });
    EXPECT_EQ(entries, linesOf(kJavaEntries));
}

// The scout's other finding: the failed save did not tell which image was missing, so the GUI
// could not ask for it. A pasted image whose bytes are gone is now the target's, and the save
// names it as the missing decal (GeneralRocketSaver::getMissingDecal()), for the dialog that
// asks the user to look for the file.
TEST(DecalTransferSave, APastedImageWhoseBytesAreGoneIsTheMissingDecal)
{
    const std::unique_ptr<OpenRocketDocument> source = OpenRocketDocumentFactory::createNewRocket();
    RocketComponent&                          tube =
        source->getRocket().getChild(0).addChild(std::make_unique<QtRocket::BodyTube>());
    QtRocket::AppearanceBuilder builder;
    builder.setImage(
        source
            ->getDecalImage(std::make_shared<QtRocket::Test::FailingAttachment>("decals/gone.png"))
            ->getName());
    tube.setAppearance(builder.getAppearance());
    Result<ComponentCopy> clipboard = ComponentCopy::copyOf(*source, {&tube});
    ASSERT_TRUE(clipboard.has_value());
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    pasteIntoFirstStage(*clipboard, *target);

    const QtRocket::InMemoryPreferences  preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*target, target->getDefaultStorageOptions());
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, QtRocket::ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message, QtRocket::decalNotFound("decals/gone.png").error().message);
    ASSERT_NE(saver.getMissingDecal(), nullptr);
    EXPECT_EQ(saver.getMissingDecal(), target->findDecalImage("decals/gone.png"));
}

}  // namespace
