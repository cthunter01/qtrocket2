// Components carried from one document into another with the decal images they show
// (ComponentCopy, DecalRegistry::importImage(), OpenRocketDocument::importDecalImage()): what the
// GUI's clipboard, paste and drop are built on (Milestone 2).
//
// OpenRocket has no such operation: a pasted or dropped component keeps the source document's
// image objects (its Decal holds them), which the target never registers and its saver writes
// as it meets them. What OpenRocket does in each case is the probe DecalTransferProbe.java of
// Milestone 2 (5f164fd0e, JDK 17; the M2 scout's DecalProbe extended); the comments quote its
// sections. The saving half, with OpenRocket's example designs, is in
// file/openrocket/decal_transfer_save_tests.cpp.

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
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Uuid.h"
#include "document/DocumentTestSupport.h"
#include "document/TestAttachments.h"

namespace
{

using QtRocket::Appearance;
using QtRocket::Attachment;
using QtRocket::BugError;
using QtRocket::Color;
using QtRocket::ComponentChangeEvent;
using QtRocket::ComponentCopy;
using QtRocket::Coordinate;
using QtRocket::Decal;
using QtRocket::DecalImage;
using QtRocket::ErrorCode;
using QtRocket::InsideColorComponent;
using QtRocket::OpenRocketDocument;
using QtRocket::Result;
using QtRocket::RocketComponent;
using QtRocket::Test::AlphaDocument;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::RocketEventRecorder;
using Placement = ComponentCopy::Placement;

/// An attachment whose bytes are @p text.
[[nodiscard]] std::shared_ptr<const Attachment> memory(std::string name, std::string_view text)
{
    return std::make_shared<MemoryAttachment>(std::move(name), text);
}

/// An appearance with a decal of the image named @p image, its other fields away from their
/// defaults, so that a renamed decal shows that they are kept.
[[nodiscard]] Appearance decalAppearance(const std::string& image)
{
    return {Color(10, 20, 30, 200), 0.25,
            Decal(Coordinate(0.125, 0.25), Coordinate(0.375, 0.5), Coordinate(1.5, 2.5), 0.75,
                  image, Decal::EdgeMode::MIRROR),
            true};
}

/// decalAppearance() with the image named @p image instead: the decal a rename gives.
[[nodiscard]] bool isDecalAppearanceOf(const std::optional<Appearance>& appearance,
                                       const std::string&               image)
{
    return appearance.has_value() && *appearance == decalAppearance(image);
}

/// The name of the image of @p appearance's decal, "-" without one.
[[nodiscard]] std::string imageOf(const std::optional<Appearance>& appearance)
{
    if (!appearance.has_value() || !appearance->getTexture().has_value())
    {
        return "-";
    }
    return appearance->getTexture()->getImageName();
}

/// "<outside image> <inside image>" of @p component, "-" for none (an inside only for an
/// InsideColorComponent).
[[nodiscard]] std::string decalsOf(const RocketComponent& component)
{
    const auto* const inside = dynamic_cast<const InsideColorComponent*>(&component);
    return std::format(
        "{} {}", imageOf(component.getAppearance()),
        inside == nullptr
            ? "-"
            : imageOf(inside->getInsideColorComponentHandler().getInsideAppearance()));
}

/// The names of @p document's decal list, as Java prints a list: "[decals/a.png, decals/b.png]".
[[nodiscard]] std::string listOf(const OpenRocketDocument& document)
{
    std::vector<std::string> names;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        names.push_back(image->getName());
    }
    return "[" + QtRocket::Test::joined(names, ", ") + "]";
}

/// The bytes of @p image as text, "unreadable" when they cannot be read.
[[nodiscard]] std::string textOf(const DecalImage& image)
{
    const auto bytes = image.getBytes();
    return bytes.has_value() ? QtRocket::bytesToString(*bytes) : "unreadable";
}

/// The bytes of @p document's image named @p name as text, "none" when it has no such image.
[[nodiscard]] std::string bytesOf(const OpenRocketDocument& document, std::string_view name)
{
    const std::shared_ptr<DecalImage> image = document.findDecalImage(name);
    return image == nullptr ? "none" : textOf(*image);
}

/// The source of the tests: the Alpha III of DocumentTestSupport with three images. The body
/// tube shows "decals/a.png" outside and "decals/b.png" inside, its fin set "decals/a.png"; the
/// nose cone shows "decals/c.png", and the parachute nothing.
class Source
{
public:
    Source()
      : m_a(document().getDecalImage(memory("decals/a.png", "source a"))),
        m_b(document().getDecalImage(memory("decals/b.png", "source b"))),
        m_c(document().getDecalImage(memory("decals/c.png", "source c")))
    {
        m_alpha.body().setAppearance(decalAppearance("decals/a.png"));
        m_alpha.body().getInsideColorComponentHandler().setInsideAppearance(
            decalAppearance("decals/b.png"));
        m_alpha.body().getChild(0).setAppearance(decalAppearance("decals/a.png"));
        m_alpha.nose().setAppearance(decalAppearance("decals/c.png"));
    }

    [[nodiscard]] const AlphaDocument& alpha() const noexcept { return m_alpha; }
    [[nodiscard]] OpenRocketDocument&  document() const noexcept { return m_alpha.document(); }
    [[nodiscard]] const std::shared_ptr<DecalImage>& a() const noexcept { return m_a; }
    [[nodiscard]] const std::shared_ptr<DecalImage>& b() const noexcept { return m_b; }

    /// A copy of the body tube (and its subtree).
    [[nodiscard]] ComponentCopy copyOfBody() const
    {
        Result<ComponentCopy> copy = ComponentCopy::copyOf(document(), {&m_alpha.body()});
        EXPECT_TRUE(copy.has_value());
        return copy.has_value() ? std::move(*copy) : ComponentCopy();
    }

private:
    AlphaDocument               m_alpha;
    std::shared_ptr<DecalImage> m_a;
    std::shared_ptr<DecalImage> m_b;
    std::shared_ptr<DecalImage> m_c;
};

/// The placement of the copy's only component at the end of @p alpha's stage, after the nose
/// cone and the body tube.
[[nodiscard]] std::vector<Placement> atTheEndOfTheStage(const AlphaDocument& alpha)
{
    return {Placement{.component = 0, .parent = &alpha.stage(), .index = 2}};
}

/// What a paste of the GUI does: the undo position, then a fresh copy of @p clipboard into the
/// stage of @p target. Returns the pasted component.
RocketComponent& pasteIntoStage(const ComponentCopy& clipboard, const AlphaDocument& target)
{
    target.document().addUndoPosition("Paste Body tube");
    const std::vector<RocketComponent*> pasted =
        clipboard.copy().insertInto(target.document(), atTheEndOfTheStage(target));
    EXPECT_EQ(pasted.size(), 1U);
    return *pasted.at(0);
}

// ---------------------------------------------------------------------------------- the copy

TEST(DecalTransfer, ACopyTakesTheImagesItsComponentsShow)
{
    const Source        source;
    const ComponentCopy copy = source.copyOfBody();

    ASSERT_EQ(copy.size(), 1U);
    EXPECT_EQ(copy.getComponent(0).getName(), source.alpha().body().getName());
    EXPECT_NE(copy.getComponent(0).getId(), source.alpha().body().getId());
    EXPECT_EQ(copy.getComponent(0).getParent(), nullptr);
    EXPECT_EQ(copy.getComponents(), std::vector<const RocketComponent*>{&copy.getComponent(0)});

    // The body tube's two images and not the nose cone's.
    ASSERT_EQ(copy.getDecalImages().size(), 2U);
    EXPECT_EQ(copy.getDecalImages()[0]->getName(), "decals/a.png");
    EXPECT_EQ(copy.getDecalImages()[1]->getName(), "decals/b.png");
    EXPECT_EQ(copy.findDecalImage("decals/c.png"), nullptr);

    // Copies of the source's images: the same source and bytes, another object.
    const std::shared_ptr<const DecalImage> a = copy.findDecalImage("decals/a.png");
    ASSERT_NE(a, nullptr);
    EXPECT_NE(a.get(), source.a().get());
    EXPECT_TRUE(a->hasSameSource(*source.a()));
    EXPECT_EQ(textOf(*a), "source a");
    EXPECT_EQ(textOf(*copy.findDecalImage("decals/b.png")), "source b");
}

TEST(DecalTransfer, ACopyOfTheCopyHasNewIdsAndTheSameImages)
{
    const Source        source;
    const ComponentCopy copy  = source.copyOfBody();
    const ComponentCopy again = copy.copy();

    ASSERT_EQ(again.size(), 1U);
    EXPECT_NE(again.getComponent(0).getId(), copy.getComponent(0).getId());
    EXPECT_EQ(again.getComponent(0).getName(), copy.getComponent(0).getName());
    EXPECT_EQ(again.getDecalImages(), copy.getDecalImages());
}

TEST(DecalTransfer, AnEmptyCopyHasNothing)
{
    const ComponentCopy copy;
    EXPECT_TRUE(copy.empty());
    EXPECT_EQ(copy.size(), 0U);
    EXPECT_TRUE(copy.getDecalImages().empty());
    EXPECT_THROW(static_cast<void>(copy.getComponent(0)), BugError);
}

TEST(DecalTransfer, ACopyKeepsTheTreesItIsGivenAndTheirIds)
{
    const Source                                  source;
    std::vector<std::unique_ptr<RocketComponent>> trees;
    trees.push_back(source.alpha().body().getChild(0).copyWithOriginalId());
    const QtRocket::Uuid  id   = trees[0]->getId();
    Result<ComponentCopy> copy = ComponentCopy::fromDetached(source.document(), std::move(trees));

    ASSERT_TRUE(copy.has_value());
    ASSERT_EQ(copy->size(), 1U);
    EXPECT_EQ(copy->getComponent(0).getId(), id);
    ASSERT_EQ(copy->getDecalImages().size(), 1U);
    EXPECT_EQ(copy->getDecalImages()[0]->getName(), "decals/a.png");
}

TEST(DecalTransfer, ADecalWhoseImageTheSourceDoesNotHaveIsRefused)
{
    const Source source;
    source.alpha().chute().setAppearance(decalAppearance("decals/none.png"));

    const Result<ComponentCopy> copy =
        ComponentCopy::copyOf(source.document(), {&source.alpha().chute()});
    ASSERT_FALSE(copy.has_value());
    EXPECT_EQ(copy.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(copy.error().message,
              std::format("The component '{}' shows the decal image 'decals/none.png', which its "
                          "document does not have",
                          source.alpha().chute().getName()));
}

TEST(DecalTransfer, AnImageRemovedFromTheListIsStillTaken)
{
    // removeDecal() keeps the image; an undo of the removal puts its name back on the body
    // tube, and a copy of the body tube then takes the image along.
    const Source        source;
    OpenRocketDocument& d = source.document();
    d.clearUndo();
    d.addUndoPosition("Remove decal");
    EXPECT_TRUE(d.removeDecal(source.a().get()));
    d.undo();
    EXPECT_EQ(listOf(d), "[decals/b.png, decals/c.png]");

    const ComponentCopy copy = source.copyOfBody();
    ASSERT_NE(copy.findDecalImage("decals/a.png"), nullptr);
    EXPECT_EQ(textOf(*copy.findDecalImage("decals/a.png")), "source a");
}

TEST(DecalTransfer, ACopyRefusesWhatIsNoComponentOfTheSource)
{
    const Source        source;
    const AlphaDocument other;
    EXPECT_THROW(
        {
            const Result<ComponentCopy> copy =
                ComponentCopy::copyOf(source.document(), {&other.body()});
        },
        BugError);
    EXPECT_THROW(
        { const Result<ComponentCopy> copy = ComponentCopy::copyOf(source.document(), {nullptr}); },
        BugError);

    std::vector<std::unique_ptr<RocketComponent>> attached;
    attached.emplace_back(nullptr);
    EXPECT_THROW(
        {
            const Result<ComponentCopy> copy =
                ComponentCopy::fromDetached(source.document(), std::move(attached));
        },
        BugError);
}

// ------------------------------------------------------------------- into another document

// DecalTransferProbe, "== probe": "target after the paste decal list=[]", and the saved file
// holds the images. Here the target registers them under their names.
TEST(DecalTransfer, APasteIntoAnotherDocumentRegistersTheImages)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;

    const RocketComponent& pasted = pasteIntoStage(clipboard, target);

    EXPECT_EQ(&pasted, &target.stage().getChild(2));
    EXPECT_EQ(decalsOf(pasted), "decals/a.png decals/b.png");
    EXPECT_EQ(decalsOf(pasted.getChild(0)), "decals/a.png -");
    EXPECT_EQ(listOf(target.document()), "[decals/a.png, decals/b.png]");
    EXPECT_EQ(bytesOf(target.document(), "decals/a.png"), "source a");
    EXPECT_EQ(bytesOf(target.document(), "decals/b.png"), "source b");

    // The target's own image: not the source's object, nor the clipboard's.
    const std::shared_ptr<DecalImage> a = target.document().findDecalImage("decals/a.png");
    EXPECT_NE(a.get(), source.a().get());
    EXPECT_NE(a.get(), clipboard.findDecalImage("decals/a.png").get());
    EXPECT_TRUE(a->hasSameSource(*source.a()));
    EXPECT_EQ(target.document().countDecalUsage(*a), 2);
    // The source is as it was.
    EXPECT_EQ(listOf(source.document()), "[decals/a.png, decals/b.png, decals/c.png]");
}

// A component without decals carries no image, and the target's list stays as it is.
TEST(DecalTransfer, AComponentWithoutDecalsCarriesNoImage)
{
    const Source                      source;
    const AlphaDocument               target;
    OpenRocketDocument&               d   = target.document();
    const std::shared_ptr<DecalImage> own = d.getDecalImage(memory("decals/own.png", "own"));
    Result<ComponentCopy>             copy =
        ComponentCopy::copyOf(source.document(), {&source.alpha().chute()});
    ASSERT_TRUE(copy.has_value());
    EXPECT_TRUE(copy->getDecalImages().empty());

    d.addUndoPosition("Paste Parachute");
    const std::vector<RocketComponent*> pasted = std::move(*copy).insertInto(
        d, {Placement{.component = 0, .parent = &target.body(), .index = 0}});
    ASSERT_EQ(pasted.size(), 1U);
    EXPECT_EQ(pasted[0], &target.body().getChild(0));
    EXPECT_EQ(decalsOf(*pasted[0]), "- -");
    EXPECT_EQ(listOf(d), "[decals/own.png]");
    d.undo();
    EXPECT_EQ(listOf(d), "[decals/own.png]");
}

// DecalTransferProbe, "== clash": OpenRocket saves one image under the name and the pasted fin
// sets come back with the target's bytes. Here the pasted image gets a unique name, and the
// decals that show it follow; everything else of the decal stays.
TEST(DecalTransfer, ANameClashRenamesTheImageAndTheComponentsFollow)
{
    const Source                      source;
    const ComponentCopy               clipboard = source.copyOfBody();
    const AlphaDocument               target;
    OpenRocketDocument&               d   = target.document();
    const std::shared_ptr<DecalImage> own = d.getDecalImage(memory("decals/a.png", "target a"));
    target.nose().setAppearance(decalAppearance("decals/a.png"));

    const RocketComponent& pasted = pasteIntoStage(clipboard, target);

    EXPECT_EQ(listOf(d), "[decals/a (1).png, decals/a.png, decals/b.png]");
    EXPECT_EQ(bytesOf(d, "decals/a (1).png"), "source a");
    EXPECT_EQ(bytesOf(d, "decals/a.png"), "target a");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), own);
    // The pasted components name the renamed image, outside of the body tube and its fin set;
    // the inside image kept its name.
    EXPECT_EQ(decalsOf(pasted), "decals/a (1).png decals/b.png");
    EXPECT_EQ(decalsOf(pasted.getChild(0)), "decals/a (1).png -");
    EXPECT_TRUE(isDecalAppearanceOf(pasted.getAppearance(), "decals/a (1).png"));
    EXPECT_TRUE(isDecalAppearanceOf(pasted.getChild(0).getAppearance(), "decals/a (1).png"));
    // The target's own nose cone is as it was.
    EXPECT_EQ(decalsOf(target.nose()), "decals/a.png -");
    EXPECT_TRUE(isDecalAppearanceOf(target.nose().getAppearance(), "decals/a.png"));
    // The clipboard is as it was.
    EXPECT_EQ(decalsOf(clipboard.getComponent(0)), "decals/a.png decals/b.png");
}

// The name the registry gives a taken name without the "decals/" prefix: OpenRocket's
// makeUniqueName() rule.
TEST(DecalTransfer, ATakenNameWithoutThePrefixGetsThePrefix)
{
    const AlphaDocument source;
    static_cast<void>(
        source.document().getDecalImage(memory("/datafiles/textures/balsa.jpg", "balsa")));
    source.nose().setAppearance(decalAppearance("/datafiles/textures/balsa.jpg"));
    Result<ComponentCopy> copy = ComponentCopy::copyOf(source.document(), {&source.nose()});
    ASSERT_TRUE(copy.has_value());

    const AlphaDocument target;
    static_cast<void>(
        target.document().getDecalImage(memory("/datafiles/textures/balsa.jpg", "other balsa")));
    target.document().addUndoPosition("Paste Nose cone");
    const std::vector<RocketComponent*> pasted =
        std::move(*copy).insertInto(target.document(), atTheEndOfTheStage(target));

    ASSERT_EQ(pasted.size(), 1U);
    EXPECT_EQ(decalsOf(*pasted[0]), "decals//datafiles/textures/balsa.jpg -");
    EXPECT_EQ(listOf(target.document()),
              "[/datafiles/textures/balsa.jpg, decals//datafiles/textures/balsa.jpg]");
    EXPECT_EQ(bytesOf(target.document(), "decals//datafiles/textures/balsa.jpg"), "balsa");
}

// The source document closed before the paste: the clipboard keeps the images and their bytes.
TEST(DecalTransfer, TheSourceMayBeClosedBeforeThePaste)
{
    auto                source    = std::make_unique<Source>();
    const ComponentCopy clipboard = source->copyOfBody();
    source.reset();

    const AlphaDocument    target;
    const RocketComponent& pasted = pasteIntoStage(clipboard, target);

    EXPECT_EQ(decalsOf(pasted), "decals/a.png decals/b.png");
    EXPECT_EQ(bytesOf(target.document(), "decals/a.png"), "source a");
    EXPECT_EQ(bytesOf(target.document(), "decals/b.png"), "source b");
}

// DecalTransferProbe, "== twice": one image per name in the saved file. Here the second paste
// finds the images of the first, also the renamed one, and registers nothing.
TEST(DecalTransfer, TheSameImagePastedTwiceIsOneImage)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    static_cast<void>(d.getDecalImage(memory("decals/a.png", "target a")));

    const RocketComponent& first = pasteIntoStage(clipboard, target);
    EXPECT_EQ(listOf(d), "[decals/a (1).png, decals/a.png, decals/b.png]");
    const std::shared_ptr<DecalImage> renamed = d.findDecalImage("decals/a (1).png");

    const RocketComponent& second = pasteIntoStage(clipboard, target);
    EXPECT_EQ(listOf(d), "[decals/a (1).png, decals/a.png, decals/b.png]");
    EXPECT_EQ(decalsOf(first), "decals/a (1).png decals/b.png");
    EXPECT_EQ(decalsOf(second), "decals/a (1).png decals/b.png");
    EXPECT_EQ(d.findDecalImage("decals/a (1).png"), renamed);
    EXPECT_EQ(d.countDecalUsage(*renamed), 4);
}

// DecalTransferProbe, "== same": a target loaded from the same file has images of the same
// names and bytes, other objects; OpenRocket writes one entry per name. Decision: an image of
// the same name and the same bytes is the target's own image, shared and not copied, so that
// two designs that exchange components do not collect numbered copies of one picture (every
// example design stores OpenRocket's built-in textures under the same names and bytes).
TEST(DecalTransfer, AnImageOfTheSameNameAndBytesIsShared)
{
    const Source                      source;
    const ComponentCopy               clipboard = source.copyOfBody();
    const AlphaDocument               target;
    OpenRocketDocument&               d   = target.document();
    const std::shared_ptr<DecalImage> own = d.getDecalImage(memory("decals/a.png", "source a"));

    const RocketComponent& pasted = pasteIntoStage(clipboard, target);

    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png]");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), own);
    EXPECT_EQ(decalsOf(pasted), "decals/a.png decals/b.png");
}

// Only the name and the bytes together make the same image: the bytes under another name are
// another image, which keeps its name.
TEST(DecalTransfer, TheSameBytesUnderAnotherNameAreAnotherImage)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    static_cast<void>(d.getDecalImage(memory("decals/x.png", "source a")));

    const RocketComponent& pasted = pasteIntoStage(clipboard, target);

    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/x.png]");
    EXPECT_EQ(decalsOf(pasted), "decals/a.png decals/b.png");
}

// An image whose bytes cannot be read is not the same as any other: the pasted one is copied
// under a unique name, whichever of the two cannot be read.
TEST(DecalTransfer, AnImageThatCannotBeReadIsNotTheSameAsAnother)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    static_cast<void>(d.getDecalImage(std::make_shared<FailingAttachment>("decals/a.png")));

    const RocketComponent& pasted = pasteIntoStage(clipboard, target);
    EXPECT_EQ(listOf(d), "[decals/a (1).png, decals/a.png, decals/b.png]");
    EXPECT_EQ(decalsOf(pasted), "decals/a (1).png decals/b.png");
    EXPECT_EQ(bytesOf(d, "decals/a (1).png"), "source a");

    const AlphaDocument broken;
    static_cast<void>(
        broken.document().getDecalImage(std::make_shared<FailingAttachment>("decals/a.png")));
    broken.nose().setAppearance(decalAppearance("decals/a.png"));
    Result<ComponentCopy> copy = ComponentCopy::copyOf(broken.document(), {&broken.nose()});
    ASSERT_TRUE(copy.has_value());
    const AlphaDocument other;
    static_cast<void>(other.document().getDecalImage(memory("decals/a.png", "source a")));
    const std::vector<RocketComponent*> nose =
        std::move(*copy).insertInto(other.document(), atTheEndOfTheStage(other));
    ASSERT_EQ(nose.size(), 1U);
    EXPECT_EQ(decalsOf(*nose[0]), "decals/a (1).png -");
    EXPECT_EQ(bytesOf(other.document(), "decals/a (1).png"), "unreadable");
}

/// Records, for every event of a document's rocket, its source, whether it is a TREE change, the
/// decal list and the bytes of "decals/a.png": "Stage tree=true list=[...] a=source a".
class DecalsAtEvents
{
public:
    explicit DecalsAtEvents(OpenRocketDocument& document)
      : m_connection(document.getRocket().addComponentChangeListener(
            [this, &document](const ComponentChangeEvent& event) {
                m_events.push_back(std::format(
                    "{} tree={} list={} a={}", event.getSource()->getName(), event.isTreeChange(),
                    listOf(document), bytesOf(document, "decals/a.png")));
            }))
    {
    }

    /// What was recorded since the last call.
    [[nodiscard]] std::vector<std::string> take() noexcept { return std::exchange(m_events, {}); }

private:
    std::vector<std::string>                          m_events;
    QtRocket::ComponentChangeSignal::ScopedConnection m_connection;
};

// The images are in the target before the first event of the insertion, and every pasted
// component fires its own (as Java's paste adds its copies one by one).
TEST(DecalTransfer, TheImagesAreThereBeforeTheFirstEvent)
{
    const Source          source;
    Result<ComponentCopy> copy =
        ComponentCopy::copyOf(source.document(), {&source.alpha().nose(), &source.alpha().body()});
    ASSERT_TRUE(copy.has_value());
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    DecalsAtEvents      seen(d);

    const std::vector<RocketComponent*> pasted = std::move(*copy).insertInto(
        d, {Placement{.component = 0, .parent = &target.stage(), .index = 2},
            Placement{.component = 1, .parent = &target.stage(), .index = 3}});

    ASSERT_EQ(pasted.size(), 2U);
    const std::string event =
        std::format("{} tree=true list=[decals/a.png, decals/b.png, decals/c.png] a=source a",
                    target.stage().getName());
    EXPECT_EQ(seen.take(), (std::vector<std::string>{event, event}));
}

// An imported image is heard as the document's own: a change of it fires TEXTURE_CHANGE on
// the components that show it.
TEST(DecalTransfer, AnImportedImageIsHeardForTextureChanges)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    static_cast<void>(pasteIntoStage(clipboard, target));
    RocketEventRecorder events(target.rocket());

    target.document().findDecalImage("decals/b.png")->fireChangeEvent();
    EXPECT_EQ(events.take(), "C[BodyTube,texture,nonfunctional]");
}

// ------------------------------------------------------------------------------- placements

TEST(DecalTransfer, ThePlacementsGoInInTheirOrderWithTheirIndices)
{
    const Source          source;
    Result<ComponentCopy> copy = ComponentCopy::copyOf(
        source.document(),
        {&source.alpha().nose(), &source.alpha().body(), &source.alpha().chute()});
    ASSERT_TRUE(copy.has_value());
    const RocketComponent* const nose = &copy->getComponent(0);
    const RocketComponent* const body = &copy->getComponent(1);
    const AlphaDocument          target;
    RocketComponent&             targetNose = target.nose();

    // The indices are addChild()'s, one placement after the other; the parachute has none and
    // goes with the copy.
    const std::vector<RocketComponent*> pasted = std::move(*copy).insertInto(
        target.document(), {Placement{.component = 1, .parent = &target.stage(), .index = 0},
                            Placement{.component = 0, .parent = &target.stage(), .index = 0}});

    ASSERT_EQ(pasted.size(), 2U);
    EXPECT_EQ(pasted[0], body);
    EXPECT_EQ(pasted[1], nose);
    ASSERT_EQ(target.stage().getChildCount(), 4U);
    EXPECT_EQ(&target.stage().getChild(0), nose);
    EXPECT_EQ(&target.stage().getChild(1), body);
    EXPECT_EQ(&target.stage().getChild(2), &targetNose);
    // The parachute's images would have been none anyway; the nose cone's and the body tube's
    // came.
    EXPECT_EQ(listOf(target.document()), "[decals/a.png, decals/b.png, decals/c.png]");
}

/// "refused" when insertInto() of a fresh copy of @p clipboard with @p placements throws a
/// BugError and leaves @p target's rocket and decal list as they were (an Alpha III without
/// images), else what happened.
[[nodiscard]] std::string refusal(const ComponentCopy& clipboard, const AlphaDocument& target,
                                  const std::vector<Placement>& placements)
{
    std::string outcome = "inserted";
    try
    {
        static_cast<void>(clipboard.copy().insertInto(target.document(), placements));
    }
    catch (const BugError&)
    {
        outcome = "refused";
    }
    if (target.stage().getChildCount() != 2 || listOf(target.document()) != "[]")
    {
        outcome += std::format(", changed: {} children, list {}", target.stage().getChildCount(),
                               listOf(target.document()));
    }
    return outcome;
}

// Every placement is checked before anything changes: a refused insertion leaves the target's
// rocket and its decal list as they were.
TEST(DecalTransfer, ARefusedPlacementChangesNothing)
{
    const Source          source;
    Result<ComponentCopy> two =
        ComponentCopy::copyOf(source.document(), {&source.alpha().nose(), &source.alpha().body()});
    ASSERT_TRUE(two.has_value());
    const ComponentCopy& clipboard = *two;
    const AlphaDocument  target;
    const AlphaDocument  other;
    RocketComponent&     stage = target.stage();

    // The second placement's index is beyond the end, the first counted.
    EXPECT_EQ(refusal(clipboard, target,
                      {Placement{.component = 0, .parent = &stage, .index = 2},
                       Placement{.component = 1, .parent = &stage, .index = 4}}),
              "refused");
    EXPECT_EQ(refusal(clipboard, target, {Placement{.component = 0, .parent = &stage, .index = 3}}),
              "refused");
    EXPECT_EQ(refusal(clipboard, target, {Placement{.component = 2, .parent = &stage, .index = 0}}),
              "refused");
    EXPECT_EQ(
        refusal(clipboard, target, {Placement{.component = 0, .parent = nullptr, .index = 0}}),
        "refused");
    // The same component twice.
    EXPECT_EQ(refusal(clipboard, target,
                      {Placement{.component = 1, .parent = &stage, .index = 0},
                       Placement{.component = 1, .parent = &stage, .index = 0}}),
              "refused");
    // A body tube in the rocket, and one in the stage of another document.
    EXPECT_EQ(refusal(clipboard, target,
                      {Placement{.component = 1, .parent = &target.rocket(), .index = 0}}),
              "refused");
    EXPECT_EQ(refusal(clipboard, target,
                      {Placement{.component = 1, .parent = &other.stage(), .index = 0}}),
              "refused");
    EXPECT_EQ(other.stage().getChildCount(), 2U);
}

// The copy that ComponentCopy::copyOf() made of a component has new ids; one with the ids of
// the target's components is refused (the GUI copies before it pastes).
TEST(DecalTransfer, AComponentWithAnIdOfTheTargetIsRefused)
{
    const Source                                  source;
    std::vector<std::unique_ptr<RocketComponent>> trees;
    trees.push_back(source.alpha().body().copyWithOriginalId());
    Result<ComponentCopy> copy = ComponentCopy::fromDetached(source.document(), std::move(trees));
    ASSERT_TRUE(copy.has_value());
    OpenRocketDocument& d = source.document();
    EXPECT_THROW(
        static_cast<void>(std::move(*copy).insertInto(d, atTheEndOfTheStage(source.alpha()))),
        BugError);
    EXPECT_EQ(source.alpha().stage().getChildCount(), 2U);
}

// ------------------------------------------------------------------------------------- undo

// DecalTransferProbe, "== undo": OpenRocket's list is "[]" throughout, as its target never
// registers the pasted images. Here the undo of the paste takes the images it registered out of
// the list again, and the redo puts them back: the registry keeps them (their names stay taken
// and findDecalImage() finds them) for the components the redo brings back.
TEST(DecalTransfer, AnUndoOfThePasteRestoresTheDecalList)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    static_cast<void>(d.getDecalImage(memory("decals/t.png", "target t")));
    d.clearUndo();

    static_cast<void>(pasteIntoStage(clipboard, target));
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/t.png]");
    EXPECT_EQ(d.getUndoDescription(), "Paste Body tube");
    const std::shared_ptr<DecalImage> a = d.findDecalImage("decals/a.png");

    d.undo();
    EXPECT_EQ(target.stage().getChildCount(), 2U);
    EXPECT_EQ(listOf(d), "[decals/t.png]");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), a);
    EXPECT_EQ(bytesOf(d, "decals/a.png"), "source a");

    d.redo();
    ASSERT_EQ(target.stage().getChildCount(), 3U);
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/t.png]");
    EXPECT_EQ(decalsOf(target.stage().getChild(2)), "decals/a.png decals/b.png");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), a);

    // Undone again, the names stay taken: another paste of an image "decals/a.png" with other
    // bytes is renamed.
    d.undo();
    const AlphaDocument other;
    static_cast<void>(other.document().getDecalImage(memory("decals/a.png", "other a")));
    other.nose().setAppearance(decalAppearance("decals/a.png"));
    Result<ComponentCopy> copy = ComponentCopy::copyOf(other.document(), {&other.nose()});
    ASSERT_TRUE(copy.has_value());
    d.addUndoPosition("Paste Nose cone");
    const std::vector<RocketComponent*> pasted =
        std::move(*copy).insertInto(d, atTheEndOfTheStage(target));
    ASSERT_EQ(pasted.size(), 1U);
    EXPECT_EQ(decalsOf(*pasted[0]), "decals/a (1).png -");
    EXPECT_EQ(listOf(d), "[decals/a (1).png, decals/t.png]");
}

// A drop from another window is bracketed by startUndo() and stopUndo() (Java's
// ComponentTreeTransferHandler, "Copy component"): its undo takes the images out as well.
TEST(DecalTransfer, AnUndoOfADropBetweenStartUndoAndStopUndoRestoresTheDecalList)
{
    const Source        source;
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    d.clearUndo();
    target.nose().setName("Changed before the drop");

    d.startUndo("Copy component");
    Result<ComponentCopy> dropped =
        ComponentCopy::copyOf(source.document(), {&source.alpha().body()});
    ASSERT_TRUE(dropped.has_value());
    static_cast<void>(std::move(*dropped).insertInto(d, atTheEndOfTheStage(target)));
    d.stopUndo();
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png]");
    EXPECT_EQ(d.getUndoDescription(), "Copy component");

    d.undo();
    EXPECT_EQ(target.stage().getChildCount(), 2U);
    EXPECT_EQ(target.nose().getName(), "Changed before the drop");
    EXPECT_EQ(listOf(d), "[]");
    d.redo();
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png]");
}

// A paste back into the document copied from finds its images: nothing is registered or
// renamed. An image removed from the list since is registered again, and an undo of the paste
// takes it out again.
TEST(DecalTransfer, InsideOneDocumentTheImagesAreTheDocumentsOwn)
{
    const Source                       source;
    OpenRocketDocument&                d         = source.document();
    const ComponentCopy                clipboard = source.copyOfBody();
    const std::shared_ptr<DecalImage>& b         = source.b();

    const RocketComponent& pasted = pasteIntoStage(clipboard, source.alpha());
    EXPECT_EQ(decalsOf(pasted), "decals/a.png decals/b.png");
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/c.png]");
    EXPECT_EQ(d.findDecalImage("decals/a.png"), source.a());
    EXPECT_EQ(d.countDecalUsage(*source.a()), 4);

    // "decals/b.png" taken off the components and out of the list, then pasted again.
    d.addUndoPosition("Remove decal");
    EXPECT_TRUE(d.removeDecal(b.get()));
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/c.png]");
    const RocketComponent& again = pasteIntoStage(clipboard, source.alpha());
    EXPECT_EQ(decalsOf(again), "decals/a.png decals/b.png");
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/c.png]");
    EXPECT_EQ(d.findDecalImage("decals/b.png"), b);

    d.undo();
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/c.png]");
    d.redo();
    EXPECT_EQ(listOf(d), "[decals/a.png, decals/b.png, decals/c.png]");
}

// An imported image that the user removes, or chooses again, no longer follows the undo
// history: undo and redo leave it alone from then on, as they leave every other image (and as
// in Java).
TEST(DecalTransfer, AnImportedImageTheUserRemovesNoLongerFollowsTheHistory)
{
    const Source        source;
    const ComponentCopy clipboard = source.copyOfBody();
    const AlphaDocument target;
    OpenRocketDocument& d = target.document();
    d.clearUndo();
    const RocketComponent& pasted = pasteIntoStage(clipboard, target);
    static_cast<void>(pasted);

    d.addUndoPosition("Remove decal");
    EXPECT_TRUE(d.removeDecal(d.findDecalImage("decals/a.png").get()));
    EXPECT_EQ(listOf(d), "[decals/b.png]");

    // The removal undone: the components show the image again, which stays out of the list
    // (OpenRocketDocument, "Removed images").
    d.undo();
    EXPECT_EQ(listOf(d), "[decals/b.png]");
    EXPECT_EQ(decalsOf(target.stage().getChild(2)), "decals/a.png decals/b.png");
    // The paste undone: "decals/b.png" follows the history, "decals/a.png" no longer.
    d.undo();
    EXPECT_EQ(listOf(d), "[]");
    d.redo();
    EXPECT_EQ(listOf(d), "[decals/b.png]");

    // Chosen again by the user (getDecalImage() of its attachment): out of the history too.
    const std::shared_ptr<const Attachment> attachment =
        std::make_shared<MemoryAttachment>("decals/b.png", "ignored");
    EXPECT_EQ(d.getDecalImage(attachment)->getName(), "decals/b.png");
    d.undo();
    EXPECT_EQ(listOf(d), "[decals/b.png]");
}

}  // namespace
