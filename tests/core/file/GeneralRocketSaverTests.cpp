#include "QtRocket/file/GeneralRocketSaver.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/Attachment.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/motor/GeneralMotorLoader.h"
#include "QtRocket/file/motor/RockSimMotorWriter.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "TestPaths.h"
#include "TestTempDir.h"
#include "document/TestAttachments.h"
#include "document/TestDocuments.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/HandlerTestSupport.h"

// GeneralRocketSaver: the .ork archive of a design and its file. The archives of OpenRocket's
// sixteen examples are compared with OpenRocket's own in the golden test of the saver
// (goldens/ork_resave_diff_golden_tests.cpp, OrkResaveDiffGolden, which loads each example once
// for all its comparisons); the tests here build their designs.

namespace
{

using QtRocket::Appearance;
using QtRocket::Color;
using QtRocket::Coordinate;
using QtRocket::Decal;
using QtRocket::DecalImage;
using QtRocket::ErrorCode;
using QtRocket::FlightConfigurationId;
using QtRocket::GeneralRocketLoader;
using QtRocket::GeneralRocketSaver;
using QtRocket::InMemoryPreferences;
using QtRocket::LoadedDocument;
using QtRocket::Motor;
using QtRocket::MotorConfiguration;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::ThrustCurveMotor;
using QtRocket::ZipArchive;
using QtRocket::ZipWriter;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::FailingAttachment;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::MemoryAttachment;
using QtRocket::Test::TempDir;

/// A time for the entries, so that two saves give the same headers.
constexpr ZipWriter::EntryTime kEntryTime{
    .year = 2025, .month = 7, .day = 26, .hour = 19, .minute = 12, .second = 36};

/// The entries of @p bytes, an archive, by name, in their order; empty when it is no archive.
[[nodiscard]] std::vector<std::string> entryNames(const std::vector<std::byte>& bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    EXPECT_TRUE(archive.has_value()) << archive.error().toString();
    std::vector<std::string> names;
    if (archive.has_value())
    {
        for (const ZipArchive::Entry& entry : archive->entries())
        {
            names.push_back(entry.name);
        }
    }
    return names;
}

/// The text of the entry @p name of @p bytes, an archive; "(none)" when there is no such entry.
[[nodiscard]] std::string entryText(const std::vector<std::byte>& bytes, std::string_view name)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive.has_value() || archive->find(name) == nullptr)
    {
        return "(none)";
    }
    return QtRocket::bytesToString(*archive->find(name));
}

/// The text of OpenRocketSaver for @p document with @p options and @p preferences.
[[nodiscard]] std::string orkText(OpenRocketDocument& document, const StorageOptions& options,
                                  const InMemoryPreferences& preferences)
{
    QtRocket::WarningSet      warnings;
    QtRocket::ErrorSet        errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    return text.has_value() ? *text : "ERROR " + text.error().toString();
}

/// Loads @p bytes from memory, without a directory, in @p environment.
[[nodiscard]] Result<LoadedDocument> reload(LoaderEnvironment&            environment,
                                            const std::vector<std::byte>& bytes)
{
    return GeneralRocketLoader(environment.context()).load(bytes);
}

// A new design is an archive of one entry, rocket.ork, which is OpenRocketSaver's text with the
// document's default options, and it loads back from the bytes without a warning.
TEST(GeneralRocketSaver, ArchivesANewDesignAsItsText)
{
    const InMemoryPreferences                 preferences;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    GeneralRocketSaver saver(preferences, GeneralRocketSaver::Options{.entryTime = kEntryTime});
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*document, document->getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_TRUE(ZipArchive::looksLikeZip(*bytes));
    EXPECT_EQ(entryNames(*bytes), std::vector<std::string>{"rocket.ork"});
    EXPECT_EQ(entryText(*bytes, "rocket.ork"),
              orkText(*document, document->getDefaultStorageOptions(), preferences));
    EXPECT_TRUE(saver.getWarnings().empty());
    EXPECT_TRUE(saver.getErrors().empty());
    EXPECT_EQ(saver.getMissingDecal(), nullptr);

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, *bytes);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_TRUE(loaded->warnings.empty());
    EXPECT_EQ(loaded->document->getRocket().getChildCount(), 1U);
}

/// Bytes that stand for a PNG image (the saver does not look at them).
[[nodiscard]] std::vector<std::byte> previewBytes()
{
    std::string bytes = "\x89PNG\r\n\x1a\n preview ";
    bytes.push_back('\0');
    bytes += "\xff bytes";
    return QtRocket::stringToBytes(bytes);
}

// Port of the core half of PreviewPersistenceTest.previewPngIsEmbeddedInSavedArchive (swing): the
// preview image of the document's options is the entry preview.png, byte for byte, behind
// rocket.ork. The options hold no image after the save (Java: options.clearPreviewImage()).
TEST(GeneralRocketSaver, TestPreviewPngIsEmbeddedInSavedArchive)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    StorageOptions& options = document->getDefaultStorageOptions();
    options.setPreviewImage(previewBytes());

    const TempDir               directory;
    const std::filesystem::path tempFile = directory.resolve("preview-test.ork");
    const InMemoryPreferences   preferences;
    GeneralRocketSaver          saver(preferences);
    const Result<void>          saved = saver.save(tempFile, *document);
    ASSERT_TRUE(saved.has_value()) << saved.error().toString();

    const Result<std::vector<std::byte>> zip = QtRocket::readFile(tempFile);
    ASSERT_TRUE(zip.has_value()) << zip.error().toString();
    EXPECT_EQ(entryNames(*zip), (std::vector<std::string>{"rocket.ork", "preview.png"}));
    EXPECT_EQ(QtRocket::stringToBytes(entryText(*zip, "preview.png")), previewBytes())
        << "Saved preview should match the source PNG exactly";
    EXPECT_FALSE(options.getPreviewImage().has_value());
}

// An empty preview image writes no preview.png (Java: only an array that is not empty), and is
// cleared all the same.
TEST(GeneralRocketSaver, AnEmptyPreviewImageIsNoEntry)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    StorageOptions options;
    options.setPreviewImage(std::vector<std::byte>{});
    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(*document, options);
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_EQ(entryNames(*bytes), std::vector<std::string>{"rocket.ork"});
    EXPECT_FALSE(options.getPreviewImage().has_value());
}

/// A decal of the image @p name.
[[nodiscard]] Decal decalOf(std::string name)
{
    return {Coordinate(0, 0), Coordinate(0, 0),       Coordinate(1, 1), 0.0,
            std::move(name),  Decal::EdgeMode::REPEAT};
}

/// An appearance with a decal of the image @p name.
[[nodiscard]] Appearance appearanceWith(std::string name)
{
    return {Color(10, 20, 30), 0.5, decalOf(std::move(name))};
}

/// A design of a nose cone and a body tube whose appearances show decal images, and the images.
struct DecalDocument
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    QtRocket::NoseCone*                 nose{nullptr};
    QtRocket::BodyTube*                 body{nullptr};

    DecalDocument()
    {
        QtRocket::RocketComponent& stage = document->getRocket().getChild(0);
        nose                             = &stage.addChild(std::make_unique<QtRocket::NoseCone>());
        body                             = &stage.addChild(std::make_unique<QtRocket::BodyTube>());
    }

    /// Registers an image of the bytes @p text under @p name.
    void image(const std::string& name, std::string_view text) const
    {
        std::ignore = registerImage(name, text);
    }

    /// The same, giving the image.
    [[nodiscard]] std::shared_ptr<DecalImage> registerImage(const std::string& name,
                                                            std::string_view   text) const
    {
        return document->getDecalImage(std::make_shared<MemoryAttachment>(name, text));
    }
};

// The decal images of the components, of their appearances and of their inside appearances,
// each once, in the order of Java's String.compareTo() (by UTF-16 code units: the uppercase
// letters first, and a character beyond U+FFFF, a surrogate pair, before U+FF21), each with its
// bytes. An image no component shows is not written.
TEST(GeneralRocketSaver, WritesTheDecalImagesOfTheComponentsOnceInJavasOrder)
{
    DecalDocument     design;
    const std::string kEmoji     = "decals/\xF0\x9F\x98\x80.png";  // U+1F600
    const std::string kFullwidth = "decals/\xEF\xBC\xA1.png";      // U+FF21
    design.image("decals/b.png", "b");
    design.image("decals/a.png", "a");
    design.image("Decals/Z.png", "Z");
    design.image("decals/inside.png", "inside");
    design.image("decals/unused.png", "unused");
    design.image(kFullwidth, "fullwidth");
    design.image(kEmoji, "emoji");
    design.document->getRocket().setAppearance(appearanceWith(kFullwidth));
    design.nose->setAppearance(appearanceWith("decals/b.png"));
    design.nose->getInsideColorComponentHandler().setInsideAppearance(
        appearanceWith("decals/a.png"));
    design.body->setAppearance(appearanceWith("decals/b.png"));
    design.body->getInsideColorComponentHandler().setInsideAppearance(
        appearanceWith("decals/inside.png"));
    design.document->getRocket().getChild(0).setAppearance(appearanceWith("Decals/Z.png"));
    design.document->getRocket().getChild(0).getChild(0).setAppearance(appearanceWith(kEmoji));

    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_EQ(entryNames(*bytes),
              (std::vector<std::string>{"rocket.ork", "Decals/Z.png", "decals/a.png",
                                        "decals/b.png", "decals/inside.png", kEmoji, kFullwidth}));
    EXPECT_EQ(entryText(*bytes, "decals/inside.png"), "inside");
    EXPECT_EQ(entryText(*bytes, kEmoji), "emoji");

    // The archive loads with every decal where it was.
    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, *bytes);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_TRUE(loaded->warnings.empty());
    const std::shared_ptr<DecalImage> inside =
        loaded->document->findDecalImage("decals/inside.png");
    ASSERT_NE(inside, nullptr);
    const Result<std::vector<std::byte>> insideBytes = inside->getBytes();
    ASSERT_TRUE(insideBytes.has_value());
    EXPECT_EQ(QtRocket::bytesToString(*insideBytes), "inside");
}

// An image the user chose to leave out (isIgnored()) is not written, and is not left out of the
// next save: the saver clears the flag (Java: image.setIgnored(false)).
TEST(GeneralRocketSaver, LeavesAnIgnoredDecalOutOnceAndNoLongerIgnoresIt)
{
    DecalDocument                     design;
    const std::shared_ptr<DecalImage> ignored =
        design.registerImage("decals/ignored.png", "ignored");
    design.image("decals/kept.png", "kept");
    design.nose->setAppearance(appearanceWith("decals/ignored.png"));
    design.body->setAppearance(appearanceWith("decals/kept.png"));
    ignored->setIgnored(true);

    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> first =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_TRUE(first.has_value()) << first.error().toString();
    EXPECT_EQ(entryNames(*first), (std::vector<std::string>{"rocket.ork", "decals/kept.png"}));
    EXPECT_FALSE(ignored->isIgnored());

    const Result<std::vector<std::byte>> second =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_TRUE(second.has_value()) << second.error().toString();
    EXPECT_EQ(entryNames(*second),
              (std::vector<std::string>{"rocket.ork", "decals/ignored.png", "decals/kept.png"}));
}

// A decal image whose source is missing fails the save with decalNotFound() (Java:
// DecalNotFoundException) and nothing is written: a file that was there stays as it was, and no
// temporary file is left. The image is getMissingDecal(): once the caller leaves it out
// (setIgnored(true), as OpenRocket's frame does when the user declines to look for it) the save
// succeeds without it. The preview image of a failed save stays in the options.
TEST(GeneralRocketSaver, AMissingDecalFailsTheSaveAndNothingIsWritten)
{
    DecalDocument                     design;
    const std::shared_ptr<DecalImage> missing =
        design.document->getDecalImage(std::make_shared<FailingAttachment>("decals/gone.png"));
    design.image("decals/kept.png", "kept");
    design.nose->setAppearance(appearanceWith("decals/gone.png"));
    design.body->setAppearance(appearanceWith("decals/kept.png"));
    StorageOptions& options = design.document->getDefaultStorageOptions();
    options.setPreviewImage(previewBytes());

    const TempDir               directory;
    const std::filesystem::path file = directory.write("design.ork", "the old design");
    const InMemoryPreferences   preferences;
    GeneralRocketSaver          saver(preferences);
    const Result<void>          saved = saver.save(file, *design.document);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(saved.error().message, QtRocket::decalNotFound("decals/gone.png").error().message);
    ASSERT_EQ(saver.getMissingDecal(), missing);
    EXPECT_EQ(QtRocket::readTextFile(file).value_or(""), "the old design");
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()),
              1);
    EXPECT_TRUE(options.getPreviewImage().has_value());

    saver.getMissingDecal()->setIgnored(true);
    ASSERT_TRUE(saver.save(file, *design.document).has_value());
    EXPECT_EQ(saver.getMissingDecal(), nullptr);
    const Result<std::vector<std::byte>> zip = QtRocket::readFile(file);
    ASSERT_TRUE(zip.has_value());
    EXPECT_EQ(entryNames(*zip),
              (std::vector<std::string>{"rocket.ork", "preview.png", "decals/kept.png"}));
}

// A decal that names an image the document does not have fails as a missing image, without an
// image to leave out (Java's decal holds its image: there is no such decal there). Another failure
// of an image's bytes is passed on as it is.
TEST(GeneralRocketSaver, ADecalOfNoImageOfTheDocumentFailsAsAMissingImage)
{
    DecalDocument design;
    design.nose->setAppearance(appearanceWith("decals/nowhere.png"));
    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(bytes.error().message, QtRocket::decalNotFound("decals/nowhere.png").error().message);
    EXPECT_EQ(saver.getMissingDecal(), nullptr);

    std::ignore = design.document->getDecalImage(
        std::make_shared<FailingAttachment>("decals/unreadable.png", ErrorCode::IO, "broken"));
    design.nose->setAppearance(appearanceWith("decals/unreadable.png"));
    const Result<std::vector<std::byte>> unreadable =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_FALSE(unreadable.has_value());
    EXPECT_EQ(unreadable.error().code, ErrorCode::IO);
    EXPECT_EQ(unreadable.error().message, "broken");
    EXPECT_EQ(saver.getMissingDecal(), nullptr);
}

// An image named as another entry of the archive fails the save as Java's ZipOutputStream fails
// it ("duplicate entry: rocket.ork"), and so do two images that are stored under one name
// (OrkWriter::decalImageName()).
TEST(GeneralRocketSaver, RefusesADecalImageNamedAsAnotherEntry)
{
    DecalDocument design;
    design.image("rocket.ork", "<openrocket/>");
    design.nose->setAppearance(appearanceWith("rocket.ork"));
    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> duplicate =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, ErrorCode::IO);
    EXPECT_EQ(duplicate.error().message, "duplicate entry: rocket.ork");

    DecalDocument twins;
    twins.image("decals/c\x01.png", "control");
    twins.image("decals/c.png", "plain");
    twins.nose->setAppearance(appearanceWith("decals/c\x01.png"));
    twins.body->setAppearance(appearanceWith("decals/c.png"));
    const Result<std::vector<std::byte>> stored =
        saver.saveToBytes(*twins.document, twins.document->getDefaultStorageOptions());
    ASSERT_FALSE(stored.has_value());
    EXPECT_EQ(stored.error().code, ErrorCode::IO);
    EXPECT_EQ(stored.error().message, "duplicate entry: decals/c.png");
}

/// OpenRocketSaverTest.createEmbeddedTestMotor(@p digest): an Estes F12X of unknown type with
/// the delays 0, 3, 5 s and plugged, 24 mm by 70 mm, three points over 2 s, 100 g falling to
/// 40 g.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> createEmbeddedTestMotor(std::string digest)
{
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(QtRocket::Manufacturer::getManufacturer("Estes"))
            .setDesignation("F12X")
            .setDescription("Desc")
            .setCaseInfo("info")
            .setMotorType(Motor::Type::UNKNOWN)
            .setStandardDelays({0, 3, 5, Motor::kPluggedDelay})
            .setDiameter(0.024)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 1, 0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.100), Coordinate(0.033, 0, 0, 0.070),
                          Coordinate(0.031, 0, 0, 0.040)})
            .setDigest(std::move(digest))
            .build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// The same motor as @p motor with @p designation and @p digest.
[[nodiscard]] std::shared_ptr<const ThrustCurveMotor> motorNamed(std::string_view designation,
                                                                 std::string      digest)
{
    Result<ThrustCurveMotor> motor =
        ThrustCurveMotor::Builder()
            .setManufacturer(QtRocket::Manufacturer::getManufacturer("Estes"))
            .setDesignation(std::string(designation))
            .setMotorType(Motor::Type::SINGLE)
            .setStandardDelays({3})
            .setDiameter(0.018)
            .setLength(0.07)
            .setTimePoints({0, 1})
            .setThrustPoints({5, 0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.02), Coordinate(0.035, 0, 0, 0.01)})
            .setDigest(std::move(digest))
            .build();
    EXPECT_TRUE(motor.has_value());
    return motor.has_value() ? std::make_shared<const ThrustCurveMotor>(std::move(*motor))
                             : nullptr;
}

/// Gives @p mount the motor @p motor in the flight configuration @p fcid.
void putMotor(QtRocket::MotorMount& mount, const FlightConfigurationId& fcid,
              std::shared_ptr<const ThrustCurveMotor> motor)
{
    MotorConfiguration config(mount, fcid);
    config.setMotor(std::move(motor));
    mount.setMotorConfig(config, fcid);
}

// The thrust curve of every motor of a motor mount in the flight configurations of the rocket,
// once a digest, as a RockSim engine file: in the order of the components and, for each, of the
// configurations; the first motor of a digest is written; a motor without a digest is not; nor is
// the motor of a tube that no longer is a motor mount (isMotorMount()), whatever it holds.
TEST(GeneralRocketSaver, EmbedsTheThrustCurveOfEveryMotorOfAMountOnce)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    QtRocket::Rocket&    rocket = document->getRocket();
    QtRocket::BodyTube&  body = rocket.getChild(0).addChild(std::make_unique<QtRocket::BodyTube>());
    QtRocket::InnerTube& first  = body.addChild(std::make_unique<QtRocket::InnerTube>());
    QtRocket::InnerTube& second = body.addChild(std::make_unique<QtRocket::InnerTube>());
    const FlightConfigurationId one;
    const FlightConfigurationId two;
    rocket.createFlightConfiguration(one);
    rocket.createFlightConfiguration(two);

    const std::shared_ptr<const ThrustCurveMotor> d1      = motorNamed("A1", "d1");
    const std::shared_ptr<const ThrustCurveMotor> d1Again = motorNamed("A1-again", "d1");
    putMotor(body, one, motorNamed("B1", "d3"));  // a body tube that stops being a mount
    body.setMotorMount(false);
    putMotor(first, one, d1);
    putMotor(first, two, motorNamed("C1", ""));
    putMotor(second, one, motorNamed("D1", "d2"));
    putMotor(second, two, d1Again);
    ASSERT_TRUE(first.isMotorMount());
    ASSERT_FALSE(body.isMotorMount());

    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*document, document->getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_EQ(entryNames(*bytes), (std::vector<std::string>{"rocket.ork", "thrustcurves/d1.rse",
                                                            "thrustcurves/d2.rse"}));
    EXPECT_EQ(entryText(*bytes, "thrustcurves/d1.rse"), QtRocket::RockSimMotorWriter::write(*d1));
}

/// The digest OpenRocket's RASP motor loader gives the embedded motor of
/// OpenRocketSaverTest.testCustomMotorEmbeddedAsRseInOrk (createRaspStyleDigest()): the scouts'
/// Java probe of tier 10 (junit-deferred, SaverMakersProbe, out/probe.out).
constexpr std::string_view kEmbeddedMotorDigest = "8d4fa1320788059cc60a2c0b320c5414";

/// OpenRocket's thrustcurves/<digest>.rse entry for that motor (the same probe,
/// out/embedded-motor.rse).
constexpr std::string_view kEmbeddedMotorRse =
    "<engine-database>\n"
    " <engine-list>\n"
    "  <engine mfg=\"Estes\" code=\"F12X\" Type=\"unknown\" dia=\"24\" len=\"70\" initWt=\"100\" "
    "propWt=\"60\" delays=\"0,3,5,1000\" auto-calc-mass=\"0\" auto-calc-cg=\"0\">\n"
    "   <comments>Desc</comments>\n"
    "   <data>\n"
    "    <eng-data t=\"0\" f=\"0\" m=\"100\" cg=\"35\"/>\n"
    "    <eng-data t=\"1\" f=\"1\" m=\"70\" cg=\"33\"/>\n"
    "    <eng-data t=\"2\" f=\"0\" m=\"40\" cg=\"31\"/>\n"
    "   </data>\n"
    "  </engine>\n"
    " </engine-list>\n"
    "</engine-database>\n";

/// The first component of @p rocket that is a motor mount, or null.
[[nodiscard]] const QtRocket::MotorMount* firstMotorMount(const QtRocket::Rocket& rocket)
{
    for (const QtRocket::RocketComponent& c : rocket.subtree(true))
    {
        const auto* const mount = dynamic_cast<const QtRocket::MotorMount*>(&c);
        if (mount != nullptr && mount->isMotorMount())
        {
            return mount;
        }
    }
    return nullptr;
}

// Port of OpenRocketSaverTest.testCustomMotorEmbeddedAsRseInOrk: a motor that is in no database
// is written into the archive as thrustcurves/<digest>.rse, OpenRocket's text to the byte, which
// the motor loaders read as a motor of the type UNKNOWN, and the design loads back with it (the
// database of OpenRocket's test knows no motor of that digest) and with the nozzle exit diameter
// of its configuration.
TEST(GeneralRocketSaver, TestCustomMotorEmbeddedAsRseInOrk)
{
    auto rocket = std::make_unique<QtRocket::Rocket>();
    rocket->setName("embedded_motor_test");
    QtRocket::AxialStage& stage = rocket->addChild(std::make_unique<QtRocket::AxialStage>());
    stage.setName("Stage1");
    QtRocket::BodyTube& bodyTube =
        stage.addChild(std::make_unique<QtRocket::BodyTube>(12, 1, 0.05));
    QtRocket::InnerTube& innerTube = bodyTube.addChild(std::make_unique<QtRocket::InnerTube>());

    const FlightConfigurationId fcid;
    rocket->createFlightConfiguration(fcid);
    rocket->setSelectedConfiguration(fcid);

    const std::shared_ptr<const ThrustCurveMotor> prototype = createEmbeddedTestMotor("");
    ASSERT_NE(prototype, nullptr);
    const std::string motorDigest = QtRocket::Test::raspStyleDigest(*prototype);
    EXPECT_EQ(motorDigest, kEmbeddedMotorDigest);
    const std::shared_ptr<const ThrustCurveMotor> motor = createEmbeddedTestMotor(motorDigest);
    ASSERT_NE(motor, nullptr);

    MotorConfiguration motorConfig(innerTube, fcid);
    EXPECT_EQ(motorConfig.getNozzleExitDiameter(), 0.0);
    motorConfig.setMotor(motor);
    motorConfig.setEjectionDelay(5);
    ASSERT_TRUE(motorConfig.setNozzleExitDiameter(0.006).has_value());
    innerTube.setMotorConfig(motorConfig, fcid);

    rocket->enableEvents();
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    StorageOptions options;
    options.setSaveSimulationData(false);

    // GeneralRocketSaver writes the archive with its thrustcurves/ directory.
    const TempDir               directory;
    const std::filesystem::path file = directory.resolve("embedded.ork");
    const InMemoryPreferences   preferences;
    GeneralRocketSaver          saver(preferences);
    ASSERT_TRUE(saver.save(file, *rocketDoc, options).has_value());

    const Result<std::vector<std::byte>> zip = QtRocket::readFile(file);
    ASSERT_TRUE(zip.has_value());
    const std::string entryName = "thrustcurves/" + motorDigest + ".rse";
    const std::string rse       = entryText(*zip, entryName);
    EXPECT_EQ(rse, kEmbeddedMotorRse) << "Expected " << entryName << " entry in .ork zip";
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        QtRocket::GeneralMotorLoader().load(QtRocket::stringToBytes(rse), motorDigest + ".rse");
    ASSERT_TRUE(motors.has_value() && !motors->empty())
        << "Expected at least one motor from .rse file";
    const Result<ThrustCurveMotor> fromRse = motors->front().build();
    ASSERT_TRUE(fromRse.has_value());
    EXPECT_EQ(fromRse->getMotorType(), Motor::Type::UNKNOWN);

    // The motor round-trips through save and load.
    const std::unique_ptr<QtRocket::ThrustCurveMotorSetDatabase> database =
        QtRocket::Test::openRocketSaverTestMotors();
    const DesignFileEnvironment  environment(DesignFileEnvironment::Presets::NONE, *database);
    const Result<LoadedDocument> rocketDocLoaded =
        GeneralRocketLoader(environment.context()).load(file);
    ASSERT_TRUE(rocketDocLoaded.has_value()) << rocketDocLoaded.error().toString();
    const QtRocket::Rocket&     loadedRocket = rocketDocLoaded->document->getRocket();
    const FlightConfigurationId loadedFcid =
        loadedRocket.getSelectedConfiguration().getFlightConfigurationId();

    const QtRocket::MotorMount* const motorMount = firstMotorMount(loadedRocket);
    ASSERT_NE(motorMount, nullptr) << "Expected a motor mount in the loaded rocket";
    const MotorConfiguration& loadedMotorConfig = motorMount->getMotorConfig(loadedFcid);
    EXPECT_EQ(loadedMotorConfig.getNozzleExitDiameter(), 0.006);
    const auto* const loadedTcm =
        dynamic_cast<const ThrustCurveMotor*>(loadedMotorConfig.getMotor().get());
    ASSERT_NE(loadedTcm, nullptr) << "Expected motor to be loaded from embedded .rse file";
    EXPECT_EQ(motor->getDesignation(), loadedTcm->getDesignation());
    EXPECT_EQ(motor->getMotorType(), loadedTcm->getMotorType());
    EXPECT_EQ(motor->getDigest(), loadedTcm->getDigest());
    EXPECT_EQ(loadedTcm->getTimePoints().size(), 3U);
}

// Port of OpenRocketSaverTest.testSavedFileSizeWithSimulationData: "A simple model rocket", loaded
// as OpenRocket's test loads it (its motor database, no presets), saved with its simulation data
// is larger than without, and smaller than four times the file OpenRocket ships. Bounds only: the
// deflated sizes depend on the zlib (OpenRocket's: 5298 and 199504 bytes; the original 198735).
TEST(GeneralRocketSaver, TestSavedFileSizeWithSimulationData)
{
    const std::filesystem::path originalFile =
        QtRocket::Test::dataDir() / "examples" / "A simple model rocket.ork";
    const std::uintmax_t originalSize = std::filesystem::file_size(originalFile);
    ASSERT_GT(originalSize, 0U) << "Original example file is empty or missing";

    const std::unique_ptr<QtRocket::ThrustCurveMotorSetDatabase> database =
        QtRocket::Test::openRocketSaverTestMotors();
    const DesignFileEnvironment  environment(DesignFileEnvironment::Presets::NONE, *database);
    const Result<LoadedDocument> doc =
        GeneralRocketLoader(environment.context()).load(originalFile);
    ASSERT_TRUE(doc.has_value()) << doc.error().toString();

    const TempDir      directory;
    GeneralRocketSaver saver(environment.preferences());

    // Saved without the simulation data: the components alone.
    StorageOptions optsNoSim;
    optsNoSim.setSaveSimulationData(false);
    ASSERT_TRUE(saver.save(directory.resolve("nosim.ork"), *doc->document, optsNoSim).has_value());
    const std::uintmax_t sizeNoSim = std::filesystem::file_size(directory.resolve("nosim.ork"));

    // Saved with the simulation data.
    StorageOptions optsWithSim;
    optsWithSim.setSaveSimulationData(true);
    ASSERT_TRUE(
        saver.save(directory.resolve("withsim.ork"), *doc->document, optsWithSim).has_value());
    const std::uintmax_t sizeWithSim = std::filesystem::file_size(directory.resolve("withsim.ork"));

    EXPECT_GT(sizeWithSim, sizeNoSim) << "the simulation data must add to the file";
    EXPECT_LT(sizeWithSim, originalSize * 4)
        << "check the storage precision or unexpected data-volume changes";
}

// A save over a file replaces it with the archive.
TEST(GeneralRocketSaver, ReplacesTheFileItSavesTo)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const TempDir               directory;
    const std::filesystem::path file = directory.write("design.ork", "the old design");
    const InMemoryPreferences   preferences;
    GeneralRocketSaver          saver(preferences);
    ASSERT_TRUE(saver.save(file, *document).has_value());
    const Result<std::vector<std::byte>> zip = QtRocket::readFile(file);
    ASSERT_TRUE(zip.has_value());
    EXPECT_EQ(entryNames(*zip), std::vector<std::string>{"rocket.ork"});
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()),
              1);
}

// A save into a directory that does not exist fails before the design is written, as Java's
// makes its temporary file first: the preview image is still in the options.
TEST(GeneralRocketSaver, FailsInAMissingDirectoryBeforeTheDesignIsWritten)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->getDefaultStorageOptions().setPreviewImage(previewBytes());
    const TempDir             directory;
    const InMemoryPreferences preferences;
    GeneralRocketSaver        saver(preferences);
    const Result<void> saved = saver.save(directory.resolve("missing/design.ork"), *document);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::IO);
    EXPECT_TRUE(document->getDefaultStorageOptions().getPreviewImage().has_value());
    EXPECT_FALSE(std::filesystem::exists(directory.resolve("missing")));
}

/// What a save of @p document with storage options of @p type gives: "<code> <message> <estimate
/// of the file size>" for a failure, "saved" otherwise.
[[nodiscard]] std::string refusalOf(GeneralRocketSaver& saver, OpenRocketDocument& document,
                                    StorageOptions::FileType type)
{
    StorageOptions options;
    options.setFileType(type);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(document, options);
    if (bytes.has_value())
    {
        return "saved";
    }
    return std::format("{} {} {}", QtRocket::toString(bytes.error().code), bytes.error().message,
                       GeneralRocketSaver::estimateFileSize(document, options));
}

// RockSim and RASAero are not written yet (UNSUPPORTED_FORMAT, nothing written, the estimate 0);
// WAVEFRONT_OBJ writes the .ork archive, as OpenRocket's GeneralRocketSaver does.
TEST(GeneralRocketSaver, WritesTheOrkArchiveForWavefrontObjAndRefusesRockSimAndRasAero)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    GeneralRocketSaver        saver(preferences);
    EXPECT_EQ(refusalOf(saver, *document, StorageOptions::FileType::ROCKSIM),
              "UNSUPPORTED_FORMAT QtRocket cannot save a design in the ROCKSIM format yet. 0");
    EXPECT_EQ(refusalOf(saver, *document, StorageOptions::FileType::RASAERO),
              "UNSUPPORTED_FORMAT QtRocket cannot save a design in the RASAERO format yet. 0");

    StorageOptions obj;
    obj.setFileType(StorageOptions::FileType::WAVEFRONT_OBJ);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(*document, obj);
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_EQ(entryNames(*bytes), std::vector<std::string>{"rocket.ork"});
    EXPECT_EQ(entryText(*bytes, "rocket.ork"), orkText(*document, StorageOptions{}, preferences));
    EXPECT_EQ(GeneralRocketSaver::estimateFileSize(*document, obj),
              OpenRocketSaver::estimateFileSize(*document, obj));
}

// With an entry time two saves of one design are the same bytes, and every entry carries the time.
TEST(GeneralRocketSaver, TwoSavesWithAnEntryTimeAreTheSameBytes)
{
    DecalDocument design;
    design.image("decals/a.png", "a");
    design.nose->setAppearance(appearanceWith("decals/a.png"));
    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences, {.entryTime = kEntryTime});
    const Result<std::vector<std::byte>> first =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    const Result<std::vector<std::byte>> second =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_TRUE(first.has_value() && second.has_value());
    EXPECT_EQ(*first, *second);
    // The DOS date and time of the first local header (ZipArchiveTests pins every field).
    ASSERT_GT(first->size(), 14U);
    EXPECT_EQ(QtRocket::bytesToString(std::span(*first).subspan(10, 4)),
              QtRocket::bytesToString(QtRocket::stringToBytes("\x92\x99\xfa\x5a")));
}

// The warnings of a save are those of the last one: a name with a control character gives
// OpenRocketSaver's warning, and a save after the name was mended none.
TEST(GeneralRocketSaver, KeepsTheWarningsOfTheLastSave)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->getRocket().setName("bad\x01name");
    const InMemoryPreferences preferences;
    GeneralRocketSaver        saver(preferences);
    ASSERT_TRUE(saver.saveToBytes(*document, document->getDefaultStorageOptions()).has_value());
    ASSERT_EQ(saver.getWarnings().size(), 1U);
    EXPECT_EQ(saver.getWarnings().begin()->toString(), OpenRocketSaver::kDroppedCharacters);
    document->getRocket().setName("good name");
    ASSERT_TRUE(saver.saveToBytes(*document, document->getDefaultStorageOptions()).has_value());
    EXPECT_TRUE(saver.getWarnings().empty());
}

// The text of the warning for a file QtRocket would not open takes the bound from the loader.
TEST(GeneralRocketSaver, TheBoundWarningNamesTheLoadersBound)
{
    EXPECT_EQ(GeneralRocketSaver::fileBeyondLoaderBound(2000000000),
              "The saved file is 2000000000 bytes long; QtRocket loads a file of at most "
              "1073741824 bytes and will not open this one.");
}

// An image left out (isIgnored()) is no longer left out once its turn has come, also when an image
// after it then fails the save: the flag is cleared as the archive is made, as in Java.
TEST(GeneralRocketSaver, AnIgnoredImageBeforeAMissingOneIsNoLongerIgnored)
{
    DecalDocument                     design;
    const std::shared_ptr<DecalImage> ignored = design.registerImage("decals/a.png", "a");
    const std::shared_ptr<DecalImage> missing =
        design.document->getDecalImage(std::make_shared<FailingAttachment>("decals/b.png"));
    design.nose->setAppearance(appearanceWith("decals/a.png"));
    design.body->setAppearance(appearanceWith("decals/b.png"));
    ignored->setIgnored(true);

    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_FALSE(bytes.has_value());
    EXPECT_EQ(bytes.error().code, ErrorCode::NOT_FOUND);
    EXPECT_EQ(saver.getMissingDecal(), missing);
    EXPECT_FALSE(ignored->isIgnored());
}

/// A decal image of a name a design file cannot hold as it is (malformed UTF-8, a control
/// character, a backslash) or that an XML reader reads back otherwise (a tab, a line feed): its
/// name, the name it is stored under and its bytes.
struct OddName
{
    std::string_view name;
    std::string_view stored;
    std::string_view bytes;
};

constexpr std::array<OddName, 5> kOddNames{{
    {.name = "decals/a\tb\nc.png", .stored = "decals/a\tb\nc.png", .bytes = "whitespace"},
    {.name = "decals/back\\slash.png", .stored = "decals/back/slash.png", .bytes = "backslash"},
    {.name = "decals/caf\xE9.png", .stored = "decals/caf\xEF\xBF\xBD.png", .bytes = "latin-1"},
    {.name = "decals/ctl\x01.png", .stored = "decals/ctl.png", .bytes = "control"},
    {.name = "decals/zeta.png", .stored = "decals/zeta.png", .bytes = "plain"},
}};

/// A design whose rocket, stage, nose cone, body tube and the inside of the nose cone show the
/// images of kOddNames, in this order, with an inner tube behind them whose motor is in no
/// database (createEmbeddedTestMotor()), in the selected flight configuration.
[[nodiscard]] DecalDocument oddlyNamedDesign()
{
    DecalDocument design;
    for (const OddName& odd : kOddNames)
    {
        design.image(std::string(odd.name), odd.bytes);
    }
    QtRocket::Rocket& rocket = design.document->getRocket();
    rocket.setAppearance(appearanceWith(std::string(kOddNames[0].name)));
    rocket.getChild(0).setAppearance(appearanceWith(std::string(kOddNames[1].name)));
    design.nose->setAppearance(appearanceWith(std::string(kOddNames[2].name)));
    design.body->setAppearance(appearanceWith(std::string(kOddNames[3].name)));
    design.nose->getInsideColorComponentHandler().setInsideAppearance(
        appearanceWith(std::string(kOddNames[4].name)));
    QtRocket::InnerTube& inner = design.body->addChild(std::make_unique<QtRocket::InnerTube>());
    const FlightConfigurationId fcid;
    rocket.createFlightConfiguration(fcid);
    rocket.setSelectedConfiguration(fcid);
    putMotor(inner, fcid, createEmbeddedTestMotor(std::string(kEmbeddedMotorDigest)));
    return design;
}

/// The entries the archive of oddlyNamedDesign() has: rocket.ork, the images under their stored
/// names, the motor.
[[nodiscard]] std::vector<std::string> oddlyNamedEntries()
{
    std::vector<std::string> names{"rocket.ork"};
    for (const OddName& odd : kOddNames)
    {
        names.emplace_back(odd.stored);
    }
    names.push_back(std::format("thrustcurves/{}.rse", kEmbeddedMotorDigest));
    return names;
}

/// The texts of @p warnings, sorted.
[[nodiscard]] std::vector<std::string> warningTexts(const QtRocket::WarningSet& warnings)
{
    std::vector<std::string> texts;
    for (const QtRocket::Warning& warning : warnings)
    {
        texts.push_back(warning.toString());
    }
    std::ranges::sort(texts);
    return texts;
}

/// The bytes of each image of @p document under the stored names of kOddNames, joined by spaces
/// ("(none)" for one that is missing or cannot be read).
[[nodiscard]] std::string storedImages(const OpenRocketDocument& document)
{
    std::string texts;
    for (const OddName& odd : kOddNames)
    {
        const std::shared_ptr<DecalImage>    image = document.findDecalImage(odd.stored);
        const Result<std::vector<std::byte>> bytes =
            image != nullptr ? image->getBytes() : QtRocket::decalNotFound(odd.stored);
        texts += (bytes.has_value() ? QtRocket::bytesToString(*bytes) : "(none)") + " ";
    }
    return texts;
}

/// The digest of the motor of the first motor mount of the selected configuration of @p rocket,
/// or "(none)".
[[nodiscard]] std::string motorDigestOf(const QtRocket::Rocket& rocket)
{
    const QtRocket::MotorMount* const mount = firstMotorMount(rocket);
    if (mount == nullptr)
    {
        return "(none)";
    }
    const MotorConfiguration& config =
        mount->getMotorConfig(rocket.getSelectedConfiguration().getFlightConfigurationId());
    return config.isEmpty() ? "(none)" : config.getMotor()->getDigest();
}

// A decal image is stored under one name in the archive and in rocket.ork
// (OrkWriter::decalImageName(), reviews of tier 10a): the name of an image that a design file
// cannot hold as it is gets a save warning (decalImageRenamed(), and the dropped characters'),
// and the file loads back from its bytes with every image where it was and the motor that follows
// them in the archive (a name that is no UTF-8 would stop a reader of the archive's local headers
// at its entry, OpenRocket's too, and every entry behind it would be lost). A second save of the
// reload is the same archive, without a warning.
TEST(GeneralRocketSaver, StoresEachDecalImageUnderTheNameRocketOrkGivesIt)
{
    DecalDocument                        design = oddlyNamedDesign();
    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*design.document, design.document->getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    EXPECT_EQ(entryNames(*bytes), oddlyNamedEntries());
    std::vector<std::string> expected{
        std::string(OpenRocketSaver::kDroppedCharacters),
        GeneralRocketSaver::decalImageRenamed(kOddNames[1].name, kOddNames[1].stored),
        GeneralRocketSaver::decalImageRenamed(kOddNames[2].name, kOddNames[2].stored),
        GeneralRocketSaver::decalImageRenamed(kOddNames[3].name, kOddNames[3].stored)};
    std::ranges::sort(expected);
    EXPECT_EQ(warningTexts(saver.getWarnings()), expected);
    EXPECT_TRUE(
        entryText(*bytes, "rocket.ork").contains("<decal name=\"decals/a&#9;b&#10;c.png\""));

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, *bytes);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_TRUE(loaded->warnings.empty());
    EXPECT_EQ(storedImages(*loaded->document), "whitespace backslash latin-1 control plain ");
    EXPECT_EQ(motorDigestOf(loaded->document->getRocket()), kEmbeddedMotorDigest);

    const Result<std::vector<std::byte>> again =
        saver.saveToBytes(*loaded->document, loaded->document->getDefaultStorageOptions());
    ASSERT_TRUE(again.has_value()) << again.error().toString();
    EXPECT_EQ(entryNames(*again), oddlyNamedEntries());
    EXPECT_TRUE(saver.getWarnings().empty());
}

// The text of the warning of a decal image that is stored under another name.
TEST(GeneralRocketSaver, TheRenamedDecalWarningNamesBothNames)
{
    EXPECT_EQ(GeneralRocketSaver::decalImageRenamed("decals/a\\b.png", "decals/a/b.png"),
              "The decal image \"decals/a\\b.png\" is saved as \"decals/a/b.png\": a design file "
              "cannot hold its name as it is.");
    EXPECT_EQ(
        GeneralRocketSaver::decalImageRenamed("decals/\xE9.png", "decals/\xEF\xBF\xBD.png"),
        "The decal image \"decals/\xEF\xBF\xBD.png\" is saved as \"decals/\xEF\xBF\xBD.png\": a "
        "design file cannot hold its name as it is.");
}

// Decision E3 for an embedded RockSim engine file: a control character of a motor's texts is left
// out (with OpenRocketSaver's warning) and bytes that are no UTF-8 are U+FFFD, so that the file is
// one the motor loaders read; the markup is RockSimMotorWriter's.
TEST(GeneralRocketSaver, TheTextsOfAnEmbeddedMotorAreReadable)
{
    Result<ThrustCurveMotor> built =
        ThrustCurveMotor::Builder()
            .setManufacturer(QtRocket::Manufacturer::getManufacturer("Estes"))
            .setDesignation("F12X")
            .setDescription("Desc\x01ription\xFF")
            .setMotorType(Motor::Type::UNKNOWN)
            .setStandardDelays({0, 3, 5, Motor::kPluggedDelay})
            .setDiameter(0.024)
            .setLength(0.07)
            .setTimePoints({0, 1, 2})
            .setThrustPoints({0, 1, 0})
            .setCGPoints({Coordinate(0.035, 0, 0, 0.100), Coordinate(0.033, 0, 0, 0.070),
                          Coordinate(0.031, 0, 0, 0.040)})
            .setDigest("odd-texts")
            .build();
    ASSERT_TRUE(built.has_value());
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    QtRocket::BodyTube& body =
        document->getRocket().getChild(0).addChild(std::make_unique<QtRocket::BodyTube>());
    QtRocket::InnerTube&        inner = body.addChild(std::make_unique<QtRocket::InnerTube>());
    const FlightConfigurationId fcid;
    document->getRocket().createFlightConfiguration(fcid);
    putMotor(inner, fcid, std::make_shared<const ThrustCurveMotor>(std::move(*built)));

    const InMemoryPreferences            preferences;
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(*document, document->getDefaultStorageOptions());
    ASSERT_TRUE(bytes.has_value()) << bytes.error().toString();
    std::string expected(kEmbeddedMotorRse);
    expected.replace(expected.find("<comments>Desc</comments>"),
                     std::string_view("<comments>Desc</comments>").size(),
                     "<comments>Description\xEF\xBF\xBD</comments>");
    const std::string rse = entryText(*bytes, "thrustcurves/odd-texts.rse");
    EXPECT_EQ(rse, expected);
    EXPECT_EQ(warningTexts(saver.getWarnings()),
              std::vector<std::string>{std::string(OpenRocketSaver::kDroppedCharacters)});
    const Result<std::vector<ThrustCurveMotor::Builder>> motors =
        QtRocket::GeneralMotorLoader().load(QtRocket::stringToBytes(rse), "odd-texts.rse");
    ASSERT_TRUE(motors.has_value()) << motors.error().toString();
    EXPECT_EQ(motors->size(), 1U);
}

// The warnings, the errors and the missing decal image are those of the last save, also of a save
// that failed before the design was written (a missing directory): a caller that asks for the
// missing image (as OpenRocket's frame does) is not sent after the image of an earlier save.
TEST(GeneralRocketSaver, AFailedSaveForgetsTheLastOne)
{
    DecalDocument design;
    std::ignore =
        design.document->getDecalImage(std::make_shared<FailingAttachment>("decals/gone.png"));
    design.nose->setAppearance(appearanceWith("decals/gone.png"));
    design.document->getRocket().setName("bad\x01name");
    const TempDir             directory;
    const InMemoryPreferences preferences;
    GeneralRocketSaver        saver(preferences);
    ASSERT_FALSE(saver.save(directory.resolve("design.ork"), *design.document).has_value());
    ASSERT_NE(saver.getMissingDecal(), nullptr);
    ASSERT_FALSE(saver.getWarnings().empty());

    const Result<void> saved =
        saver.save(directory.resolve("no/such/design.ork"), *design.document);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::IO);
    EXPECT_EQ(saver.getMissingDecal(), nullptr);
    EXPECT_TRUE(saver.getWarnings().empty());
    EXPECT_TRUE(saver.getErrors().empty());
}

// A save whose file cannot be put in place (the target is a directory) fails after the archive was
// made, and the options keep their preview image for the next save: it is cleared once the file
// is there (Java clears it after the archive was written).
TEST(GeneralRocketSaver, KeepsThePreviewImageWhenTheFileCannotBeWritten)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    StorageOptions& options = document->getDefaultStorageOptions();
    options.setPreviewImage(previewBytes());
    const TempDir               directory;
    const std::filesystem::path target = directory.resolve("taken");
    std::ignore                        = directory.write("taken/inside.txt", "inside");
    const InMemoryPreferences preferences;
    GeneralRocketSaver        saver(preferences);
    const Result<void>        saved = saver.save(target, *document);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::IO);
    EXPECT_TRUE(options.getPreviewImage().has_value());
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()),
              1);

    ASSERT_TRUE(saver.save(directory.resolve("design.ork"), *document).has_value());
    EXPECT_FALSE(options.getPreviewImage().has_value());
}

/// An attachment whose bytes never fit in memory: getBytes() throws std::bad_alloc, as a decal
/// image of 32 MiB does when the memory has run out.
class ExhaustingAttachment final : public QtRocket::Attachment
{
public:
    explicit ExhaustingAttachment(std::string name) : Attachment(std::move(name)) { }

    [[nodiscard]] Result<std::vector<std::byte>> getBytes() const override
    {
        throw std::bad_alloc();
    }
};

// A save that runs out of memory is an Error, not an exception (nothing a document holds makes a
// save throw): the file stays as it was and no temporary file is left.
TEST(GeneralRocketSaver, RunningOutOfMemoryIsAnErrorThatLeavesTheFile)
{
    DecalDocument design;
    std::ignore =
        design.document->getDecalImage(std::make_shared<ExhaustingAttachment>("decals/huge.png"));
    design.nose->setAppearance(appearanceWith("decals/huge.png"));
    const TempDir               directory;
    const std::filesystem::path file = directory.write("design.ork", "the old design");
    const InMemoryPreferences   preferences;
    GeneralRocketSaver          saver(preferences);
    const Result<void>          saved = saver.save(file, *design.document);
    ASSERT_FALSE(saved.has_value());
    EXPECT_EQ(saved.error().code, ErrorCode::IO);
    EXPECT_EQ(saved.error().message, GeneralRocketSaver::kOutOfMemory);
    EXPECT_EQ(QtRocket::readTextFile(file).value_or(""), "the old design");
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path()),
                            std::filesystem::directory_iterator()),
              1);
}

}  // namespace
