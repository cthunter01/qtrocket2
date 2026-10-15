#include "QtRocket/file/openrocket/OpenRocketSaver.h"

#include <cstddef>
#include <cstdint>
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
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Version.h"
#include "file/RocketLoaderTestSupport.h"
#include "rocket/TestRockets.h"

// OpenRocketSaver: the document level of the .ork text. The texts expected are what
// OpenRocket's OpenRocketSaver.save() writes for the same document (the probe of tier 10a S1,
// DocLevelProbe "document"), with the creator attribute set aside and without the <rocket>
// element, whose lines the component savers write (part S2 of tier 10a).

namespace
{

using QtRocket::CustomExpression;
using QtRocket::DocumentConfig;
using QtRocket::ErrorCode;
using QtRocket::ErrorSet;
using QtRocket::FlightConfigurationId;
using QtRocket::GeneralRocketLoader;
using QtRocket::InMemoryPreferences;
using QtRocket::LoadedDocument;
using QtRocket::Material;
using QtRocket::MaterialGroup;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketDocumentFactory;
using QtRocket::OpenRocketSaver;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::Test::LoaderEnvironment;

/// What a save gave: the text, or the Error's message, and the warnings.
struct Saved
{
    std::string text;
    std::string error;
    WarningSet  warnings;
};

/// Saves @p document with @p options and an empty preference store.
[[nodiscard]] Saved saveOf(OpenRocketDocument& document, const StorageOptions& options = {})
{
    const InMemoryPreferences preferences;
    Saved                     saved;
    ErrorSet                  errors;
    const Result<std::string> text =
        OpenRocketSaver::save(document, options, preferences, saved.warnings, errors);
    if (text.has_value())
    {
        saved.text = *text;
    }
    else
    {
        saved.error = text.error().message;
    }
    EXPECT_TRUE(errors.empty());
    return saved;
}

/// @p text with the value of its first creator attribute replaced by "CREATOR".
[[nodiscard]] std::string withoutCreator(std::string text)
{
    constexpr std::string_view kAttribute = "creator=\"";
    const std::size_t          start      = text.find(kAttribute);
    if (start == std::string::npos)
    {
        return text;
    }
    const std::size_t value = start + kAttribute.size();
    const std::size_t end   = text.find('"', value);
    return text.replace(value, end - value, "CREATOR");
}

/// @p text without the lines of its <rocket> element (from "  <rocket>" to "  </rocket>"), which
/// stand as one line "  <rocket/>" instead.
[[nodiscard]] std::string withoutRocket(const std::string& text)
{
    constexpr std::string_view kOpen  = "\n  <rocket>\n";
    constexpr std::string_view kClose = "\n  </rocket>\n";
    const std::size_t          open   = text.find(kOpen);
    const std::size_t          close  = text.find(kClose);
    if (open == std::string::npos || close == std::string::npos)
    {
        return text;
    }
    return text.substr(0, open) + "\n  <rocket/>\n" + text.substr(close + kClose.size());
}

/// The document level of @p text: withoutRocket() of withoutCreator().
[[nodiscard]] std::string documentLevel(const std::string& text)
{
    return withoutRocket(withoutCreator(text));
}

// A new design: the declaration with single quotes, the root element with the version 1.11 and
// QtRocket's creator, the rocket, a blank line, no <datatypes>, and the simulations, the photo
// studio and the document preferences, empty, each written all the same.
TEST(OpenRocketSaver, WritesTheHeaderAndTheElementsOfANewDesign)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const Saved saved = saveOf(*document);
    ASSERT_EQ(saved.error, "");
    EXPECT_TRUE(saved.text.starts_with(
        std::format("<?xml version='1.0' encoding='utf-8'?>\n<openrocket version=\"1.11\" "
                    "creator=\"{}\">\n  <rocket>\n",
                    QtRocket::creatorString())))
        << saved.text;
    EXPECT_EQ(documentLevel(saved.text),
              "<?xml version='1.0' encoding='utf-8'?>\n"
              "<openrocket version=\"1.11\" creator=\"CREATOR\">\n"
              "  <rocket/>\n"
              "\n"
              "  <simulations>\n"
              "  </simulations>\n"
              "  <photostudio>\n"
              "  </photostudio>\n"
              "  <docprefs>\n"
              "  </docprefs>\n"
              "</openrocket>\n");
    EXPECT_TRUE(saved.warnings.empty());
    // Only '\n', on every platform.
    EXPECT_EQ(saved.text.find('\r'), std::string::npos);
}

/// The document of DocLevelProbe "document": a new design with two custom expressions, a
/// document preference of each type, four document materials (two of one name) and three
/// settings of the photo studio.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> documentOfTheProbe()
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    document->addCustomExpression(CustomExpression("Kinetic energy", "Ek", "J", "0.5*m*v^2"));
    document->addCustomExpression(CustomExpression("Momentum", "p", "kg m/s", "m*v"));
    document->getDocumentPreferences().putBoolean("a", true);
    document->getDocumentPreferences().putInt("b", 42);
    document->getDocumentPreferences().putDouble("c", 0.1);
    document->getDocumentPreferences().putString("d", "text");
    using Type = Material::Type;
    document->getDocumentMaterials().addMaterial(Material::newMaterial(
        Type::SURFACE, "Zeta paint", 0.05, MaterialGroup::CUSTOM, true, true));
    document->getDocumentMaterials().addMaterial(Material::newMaterial(
        Type::BULK, "Alpha wood", 650.0, 1.0E9, MaterialGroup::WOODS, true, true));
    document->getDocumentMaterials().addMaterial(
        Material::newMaterial(Type::LINE, "Mid cord", 0.002, MaterialGroup::ELASTICS, true, true));
    document->getDocumentMaterials().addMaterial(
        Material::newMaterial(Type::BULK, "Alpha wood", 700.0, MaterialGroup::WOODS, true, true));
    document->setPhotoSettings(
        {{"roll", "3.14"},
         {"sunlight", "255 255 255 255"},
         {"sky", "net.sf.openrocket.gui.figure3d.photo.sky.builtin.Mountains"}});
    return document;
}

// What OpenRocket writes for the same document: <datatypes> after the rocket's blank line and
// followed by one of its own, the photo settings in their fixed order, the preferences with
// their types (OpenRocket's HashMap happens to give the sorted order for these keys), and the
// materials sorted by name and density across the three types.
TEST(OpenRocketSaver, WritesTheDocumentLevelAsOpenRocket)
{
    const std::unique_ptr<OpenRocketDocument> document = documentOfTheProbe();
    const Saved                               saved    = saveOf(*document);
    ASSERT_EQ(saved.error, "");
    EXPECT_EQ(documentLevel(saved.text),
              "<?xml version='1.0' encoding='utf-8'?>\n"
              "<openrocket version=\"1.11\" creator=\"CREATOR\">\n"
              "  <rocket/>\n"
              "\n"
              "  <datatypes>\n"
              "    <type source=\"customexpression\">\n"
              "      <name>Kinetic energy</name>\n"
              "      <symbol>Ek</symbol>\n"
              "      <unit unittype=\"auto\">J</unit>\n"
              "      <expression>0.5*m*v^2</expression>\n"
              "    </type>\n"
              "    <type source=\"customexpression\">\n"
              "      <name>Momentum</name>\n"
              "      <symbol>p</symbol>\n"
              "      <unit unittype=\"auto\">kg m/s</unit>\n"
              "      <expression>m*v</expression>\n"
              "    </type>\n"
              "  </datatypes>\n"
              "\n"
              "  <simulations>\n"
              "  </simulations>\n"
              "  <photostudio>\n"
              "    <roll>3.14</roll>\n"
              "    <sunlight red=\"255\" green=\"255\" blue=\"255\" alpha=\"255\"/>\n"
              "    <sky>net.sf.openrocket.gui.figure3d.photo.sky.builtin.Mountains</sky>\n"
              "  </photostudio>\n"
              "  <docprefs>\n"
              "    <pref key=\"a\" type=\"boolean\">true</pref>\n"
              "    <pref key=\"b\" type=\"integer\">42</pref>\n"
              "    <pref key=\"c\" type=\"double\">0.1</pref>\n"
              "    <pref key=\"d\" type=\"string\">text</pref>\n"
              "    <docmaterials>\n"
              "      <material>BULK|Alpha wood|650.0|1.0E9|Woods</material>\n"
              "      <material>BULK|Alpha wood|700.0|0.0|Woods</material>\n"
              "      <material>LINE|Mid cord|0.002|0.0|Elastics</material>\n"
              "      <material>SURFACE|Zeta paint|0.05|0.0|Custom</material>\n"
              "    </docmaterials>\n"
              "  </docprefs>\n"
              "</openrocket>\n");
    EXPECT_TRUE(saved.warnings.empty());
}

// Deviation: the document preferences are written sorted by key (OpenRocket: HashMap order).
TEST(OpenRocketSaver, WritesTheDocumentPreferencesSortedByKey)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->getDocumentPreferences().putString("RocketPanel.2DTextColor", "1,2,3");
    document->getDocumentPreferences().putBoolean("RocketPanel.3DShadowsEnabled", false);
    document->getDocumentPreferences().putInt("A", -1);
    const Saved saved = saveOf(*document);
    EXPECT_TRUE(saved.text.contains(
        "  <docprefs>\n"
        "    <pref key=\"A\" type=\"integer\">-1</pref>\n"
        "    <pref key=\"RocketPanel.2DTextColor\" type=\"string\">1,2,3</pref>\n"
        "    <pref key=\"RocketPanel.3DShadowsEnabled\" type=\"boolean\">false</pref>\n"
        "  </docprefs>\n"))
        << saved.text;
}

/// OpenRocketSaverTest.SIMULATION_EXTENSION_SCRIPT.
constexpr std::string_view kSimulationExtensionScript = "// Test <  &\n// >\n// <![CDATA[";

/// TestRockets.makeTestRocket_v110_withSimulationExtension(@p script): the Estes Alpha III with
/// one simulation holding an enabled JavaScript extension of @p script.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV110WithSimulationExtension(
    std::string_view script)
{
    QtRocket::Test::TestEstesAlphaIII   alpha;
    QtRocket::Rocket&                   rocket = *alpha.rocket;
    std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    const auto simulation = std::make_shared<QtRocket::Simulation>(rocket);
    const auto extension  = std::make_shared<QtRocket::ScriptingExtension>();
    extension->setEnabled(true);
    extension->setLanguage("JavaScript");
    extension->setScript(script);
    simulation->getSimulationExtensions().push_back(extension);
    document->addSimulation(simulation);

    rocket.enableEvents();
    return document;
}

// Port of OpenRocketSaverTest.testFileVersion111_withSimulationExtension.
TEST(OpenRocketSaver, TestFileVersion111WithSimulationExtension)
{
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript);
    EXPECT_EQ(OpenRocketSaver::calculateNecessaryFileVersion(*rocketDoc, StorageOptions{}), 111);
    EXPECT_EQ(OpenRocketSaver::kFileVersion, 111);
    EXPECT_EQ(OpenRocketSaver::fileVersionText(OpenRocketSaver::kFileVersion), "1.11");
    EXPECT_EQ(OpenRocketSaver::fileVersionText(OpenRocketSaver::kFileVersion),
              DocumentConfig::kSupportedVersions.back());
    EXPECT_EQ(OpenRocketSaver::fileVersionText(105), "1.5");
    // DocumentConfigTest.testFileVersionDivisor: the saver's divisor is the loader's.
    EXPECT_EQ(OpenRocketSaver::kFileVersionDivisor, DocumentConfig::kFileVersionDivisor);
}

/// The file types of the storage options for which a save of a new design does not give the
/// Error unsupportedFileType() alone (no text, no warning), by name.
[[nodiscard]] std::vector<std::string> fileTypesNotRefused()
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    std::vector<std::string> notRefused;
    for (const StorageOptions::FileType type :
         {StorageOptions::FileType::ROCKSIM, StorageOptions::FileType::RASAERO,
          StorageOptions::FileType::WAVEFRONT_OBJ})
    {
        StorageOptions options;
        options.setFileType(type);
        const Saved saved = saveOf(*document, options);
        if (saved.error != OpenRocketSaver::unsupportedFileType(type) || !saved.text.empty() ||
            !saved.warnings.empty())
        {
            notRefused.emplace_back(QtRocket::name(type));
        }
    }
    return notRefused;
}

// Only the OpenRocket format: the other file types are an Error, and nothing is written or warned
// of (deviation: Java's OpenRocketSaver writes its format whatever the options say).
TEST(OpenRocketSaver, RefusesTheOtherFileTypes)
{
    EXPECT_EQ(fileTypesNotRefused(), std::vector<std::string>{});
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    EXPECT_EQ(OpenRocketSaver::unsupportedFileType(StorageOptions::FileType::ROCKSIM),
              "OpenRocketSaver writes the OpenRocket format; the storage options ask for ROCKSIM.");

    const InMemoryPreferences preferences;
    WarningSet                warnings;
    ErrorSet                  errors;
    StorageOptions            options;
    options.setFileType(StorageOptions::FileType::RASAERO);
    const Result<std::string> refused =
        OpenRocketSaver::save(*document, options, preferences, warnings, errors);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::UNSUPPORTED_FORMAT);
}

// RocketSaver.save(dest, doc, warnings, errors): the document's default storage options.
TEST(OpenRocketSaver, SavesWithTheDefaultOptionsOfTheDocument)
{
    const std::unique_ptr<OpenRocketDocument> document = documentOfTheProbe();
    const InMemoryPreferences                 preferences;
    WarningSet                                warnings;
    ErrorSet                                  errors;
    const Result<std::string>                 text =
        OpenRocketSaver::save(*document, preferences, warnings, errors);
    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, saveOf(*document).text);

    document->getDefaultStorageOptions().setFileType(StorageOptions::FileType::ROCKSIM);
    const Result<std::string> refused =
        OpenRocketSaver::save(*document, preferences, warnings, errors);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error().code, ErrorCode::UNSUPPORTED_FORMAT);
}

/// A document whose texts OpenRocket writes unescaped hold markup: a custom expression, a
/// document preference's key, a document material's name, a setting of the photo studio.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> documentWithMarkup()
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    document->addCustomExpression(CustomExpression("A&B<C>", "s&", "m/s<", "a<b"));
    document->getDocumentPreferences().putString("k\"q<&", "v<&");
    document->getDocumentMaterials().addMaterial(Material::newMaterial(
        Material::Type::BULK, "M&M <x>", 1000.0, MaterialGroup::CUSTOM, true, true));
    document->setPhotoSettings({{"sky", "a<b&c"}});
    return document;
}

/// Loads @p text from memory, without a directory, in an environment of its own.
[[nodiscard]] Result<LoadedDocument> reload(LoaderEnvironment& environment, const std::string& text)
{
    const GeneralRocketLoader loader(environment.context());
    return loader.load(QtRocket::stringToBytes(text));
}

// Decision E3: the texts OpenRocket writes unescaped are escaped (OpenRocket writes
// "<name>A&B<C></name>", `<pref key="k"q<&" ...>`, "<material>BULK|M&M <x>|...</material>" and
// "<sky>a<b&c</sky>", and neither program reads such a file). The file reads back with every
// text as it was.
TEST(OpenRocketSaver, EscapesWhatOpenRocketWritesUnescapedAndTheFileReadsBack)
{
    const std::unique_ptr<OpenRocketDocument> document = documentWithMarkup();
    const Saved                               saved    = saveOf(*document);
    ASSERT_EQ(saved.error, "");
    EXPECT_TRUE(
        saved.text.contains("      <name>A&amp;B&lt;C&gt;</name>\n"
                            "      <symbol>s&amp;</symbol>\n"
                            "      <unit unittype=\"auto\">m/s&lt;</unit>\n"
                            "      <expression>a&lt;b</expression>\n"))
        << saved.text;
    EXPECT_TRUE(saved.text.contains(
        "    <pref key=\"k&quot;q&lt;&amp;\" type=\"string\">v&lt;&amp;</pref>\n"));
    EXPECT_TRUE(saved.text.contains(
        "      <material>BULK|M&amp;M &lt;x&gt;|1000.0|0.0|Custom</material>\n"));
    EXPECT_TRUE(saved.text.contains("    <sky>a&lt;b&amp;c</sky>\n"));
    EXPECT_TRUE(saved.warnings.empty());

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved.text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    const OpenRocketDocument& back = *loaded->document;
    ASSERT_EQ(back.getCustomExpressions().size(), 1U);
    EXPECT_EQ(back.getCustomExpressions().front().getName(), "A&B<C>");
    EXPECT_EQ(back.getCustomExpressions().front().getSymbol(), "s&");
    EXPECT_EQ(back.getCustomExpressions().front().getUnit(), "m/s<");
    EXPECT_EQ(back.getCustomExpressions().front().getExpressionString(), "a<b");
    EXPECT_EQ(back.getDocumentPreferences().getString("k\"q<&", "none"), "v<&");
    ASSERT_EQ(back.getDocumentMaterials().totalMaterialCount(), 1U);
    EXPECT_EQ(back.getDocumentMaterials().bulkMaterials().get(0).getName(), "M&M <x>");
    EXPECT_EQ(back.getPhotoSettings().at("sky"), "a<b&c");
}

// Decision E3: characters an XML document cannot hold are dropped from every text, and the save
// says so once, however many there were.
TEST(OpenRocketSaver, DropsWhatADesignFileCannotHoldWithOneWarning)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->addCustomExpression(CustomExpression("E\x01", "s", "u", "1"));
    document->getDocumentPreferences().putString("k\x02", "v\x03\xEF\xBF\xBF");
    document->setPhotoSettings({{"sky", "s\x1f"}});
    const Saved saved = saveOf(*document);
    ASSERT_EQ(saved.error, "");
    EXPECT_TRUE(saved.text.contains("<name>E</name>"));
    EXPECT_TRUE(saved.text.contains("<pref key=\"k\" type=\"string\">v</pref>"));
    EXPECT_TRUE(saved.text.contains("<sky>s</sky>"));
    ASSERT_EQ(saved.warnings.size(), 1U);
    EXPECT_EQ(saved.warnings.begin()->toString(), OpenRocketSaver::kDroppedCharacters);

    LoaderEnvironment            environment;
    const Result<LoadedDocument> loaded = reload(environment, saved.text);
    ASSERT_TRUE(loaded.has_value()) << loaded.error().toString();
    EXPECT_EQ(loaded->document->getCustomExpressions().front().getName(), "E");
}

// Invalid UTF-8 becomes U+FFFD (decision E3), without a warning.
TEST(OpenRocketSaver, ReplacesMalformedUtf8)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    document->addCustomExpression(CustomExpression("bad\xFF", "s", "u", "1"));
    const Saved saved = saveOf(*document);
    EXPECT_TRUE(saved.text.contains("<name>bad\xEF\xBF\xBD</name>"));
    EXPECT_TRUE(saved.warnings.empty());
}

/// A document saved (with the simulation data, as OpenRocketSaverTest's saveRocket() is asked
/// to) and loaded back from the text, with the environment of the load.
struct Reloaded
{
    /// Declared first, destroyed last: the document's simulations keep its preference store.
    std::unique_ptr<LoaderEnvironment>  environment = std::make_unique<LoaderEnvironment>();
    std::unique_ptr<OpenRocketDocument> document;
};

/// OpenRocketSaverTest's saveRocket() and loadRocket() of @p document: the reloaded document, or
/// none with a failure.
[[nodiscard]] Reloaded saveAndReload(OpenRocketDocument& document)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    const Saved saved = saveOf(document, options);
    EXPECT_EQ(saved.error, "");
    Reloaded               reloaded;
    Result<LoadedDocument> loaded = reload(*reloaded.environment, saved.text);
    if (!loaded)
    {
        ADD_FAILURE() << loaded.error().toString();
        reloaded.document = OpenRocketDocumentFactory::createNewRocket();
        return reloaded;
    }
    reloaded.document = std::move(loaded->document);
    return reloaded;
}

/// The activeness of stages 0, 1 and 2 in @p config, as "true false true".
[[nodiscard]] std::string activenessOf(const QtRocket::FlightConfiguration& config)
{
    return std::format("{} {} {}", config.isStageActive(0), config.isStageActive(1),
                       config.isStageActive(2));
}

// Port of OpenRocketSaverTest.testSaveStageActiveness: the activeness of the stages of every
// flight configuration survives a save and a load (<motorconfiguration><stage number active/>).
// TestRockets.makeTestRocket_v108_withDisabledStage(): the Falcon 9 Heavy with stage 0 of its
// selected configuration inactive.
TEST(OpenRocketSaver, TestSaveStageActiveness)
{
    QtRocket::Test::TestFalcon9Heavy falcon;
    falcon.rocket->getSelectedConfiguration().setStageActive(0, false, false);
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(falcon.rocket));

    // Save rockets, load, validate
    Reloaded first = saveAndReload(*rocketDoc);
    // Check that the stages activeness is saved
    QtRocket::FlightConfiguration* config = &first.document->getRocket().getSelectedConfiguration();
    EXPECT_EQ(activenessOf(*config), "false true true");

    // Disable second stage
    config->setStageActive(1, false, false);
    Reloaded second = saveAndReload(*first.document);
    config          = &second.document->getRocket().getSelectedConfiguration();
    EXPECT_EQ(activenessOf(*config), "false false true");

    // Re-enable first stage
    config->setStageActive(0, true, false);
    Reloaded third = saveAndReload(*second.document);
    config         = &third.document->getRocket().getSelectedConfiguration();
    EXPECT_EQ(activenessOf(*config), "true false true");

    // Check that other configurations are not affected
    const FlightConfigurationId    testFcid0 = QtRocket::Test::testFcid(0);
    QtRocket::FlightConfiguration* extraConfig =
        &third.document->getRocket().createFlightConfiguration(testFcid0);
    extraConfig->setAllStages();
    Reloaded fourth = saveAndReload(*third.document);
    config          = &fourth.document->getRocket().getSelectedConfiguration();
    extraConfig     = &fourth.document->getRocket().getFlightConfiguration(testFcid0);
    EXPECT_EQ(activenessOf(*config), "true false true");
    EXPECT_EQ(activenessOf(*extraConfig), "true true true");

    // Disable a stage in the extra config, and an extra one in the selected config
    extraConfig->setStageActive(0, false, false);
    config->setStageActive(2, false, false);
    Reloaded fifth = saveAndReload(*fourth.document);
    config         = &fifth.document->getRocket().getSelectedConfiguration();
    extraConfig    = &fifth.document->getRocket().getFlightConfiguration(testFcid0);
    EXPECT_EQ(activenessOf(*config), "true false false");
    EXPECT_EQ(activenessOf(*extraConfig), "false true true");

    // Test an empty rocket with no configurations
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    Reloaded empty = saveAndReload(*document);
    // Add a child, otherwise the stage is always marked inactive
    empty.document->getRocket().getStage(0)->addChild(std::make_unique<QtRocket::BodyTube>());
    config = &empty.document->getRocket().getSelectedConfiguration();
    EXPECT_TRUE(config->isStageActive(0))
        << " empty rocket, selected config, stage 0 should have been enabled after saving";
}

/// TestRockets.makeTestRocket_v106_withAppearance(): a stage holding a body tube (12 m, radius
/// 1 m, wall 5 cm) with an appearance (paint 100, 25, 50, shine 1, no decal).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> makeTestRocketV106WithAppearance()
{
    auto rocket = std::make_unique<QtRocket::Rocket>();
    rocket->setName("v106_withAppearance");

    // make stage
    auto& stage = rocket->addChild(std::make_unique<QtRocket::AxialStage>());
    stage.setName("Stage1");

    // make body tube with an appearance setting
    auto bodyTube = std::make_unique<QtRocket::BodyTube>(12, 1, 0.05);
    bodyTube->setAppearance(QtRocket::Appearance(QtRocket::Color(100, 25, 50), 1));
    stage.addChild(std::move(bodyTube));

    rocket->enableEvents();
    return OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
}

// Port of OpenRocketSaverTest.testAppearanceTextureOpacityRoundTrip: the paint's alpha and
// <opacityaffectstexture> survive a save and a load.
TEST(OpenRocketSaver, TestAppearanceTextureOpacityRoundTrip)
{
    const std::unique_ptr<OpenRocketDocument> rocketDoc = makeTestRocketV106WithAppearance();
    QtRocket::RocketComponent& bodyTube = rocketDoc->getRocket().getStage(0)->getChild(0);
    bodyTube.setAppearance(
        QtRocket::Appearance(QtRocket::Color(100, 25, 50, 128), 1, std::nullopt, true));

    LoaderEnvironment            environment;
    const Result<LoadedDocument> rocketDocLoaded = reload(environment, saveOf(*rocketDoc).text);
    ASSERT_TRUE(rocketDocLoaded.has_value()) << rocketDocLoaded.error().toString();
    const QtRocket::RocketComponent& loadedBodyTube =
        rocketDocLoaded->document->getRocket().getStage(0)->getChild(0);
    const std::optional<QtRocket::Appearance>& loadedAppearance = loadedBodyTube.getAppearance();

    ASSERT_TRUE(loadedAppearance.has_value());
    const QtRocket::Appearance appearance =
        loadedAppearance.value_or(QtRocket::Appearance::missing());
    EXPECT_EQ(appearance.getPaint().alpha(), 128);
    EXPECT_TRUE(appearance.isOpacityAffectsTexture());
}

// The texts of the bound warnings take their numbers from the loader's constants.
TEST(OpenRocketSaver, BoundWarningsNameTheLoadersBounds)
{
    EXPECT_EQ(OpenRocketSaver::documentBeyondLoaderBound(300000000),
              std::format("The saved design is 300000000 bytes long; QtRocket loads a design of "
                          "at most {} bytes and will not open this file.",
                          GeneralRocketLoader::kMaxDocumentBytes));
    EXPECT_EQ(GeneralRocketLoader::kMaxDocumentBytes, std::size_t{268435456});
    EXPECT_EQ(OpenRocketSaver::componentsBeyondLoaderBound(DocumentConfig::kMaxComponents + 1),
              std::format("The saved rocket has {} components; QtRocket loads at most {} and will "
                          "ignore the ones beyond when it opens this file.",
                          DocumentConfig::kMaxComponents + 1, DocumentConfig::kMaxComponents));
}

/// A new design whose body tube holds @p massComponents mass components: 2 + @p massComponents
/// components below the rocket.
[[nodiscard]] std::unique_ptr<OpenRocketDocument> documentWithMassComponents(
    std::size_t massComponents)
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    QtRocket::Rocket&                   rocket   = document->getRocket();
    rocket.enableEvents(false);
    QtRocket::BodyTube& tube = rocket.getChild(0).addChild(std::make_unique<QtRocket::BodyTube>());
    for (std::size_t i = 0; i < massComponents; ++i)
    {
        tube.addChild(std::make_unique<QtRocket::MassComponent>());
    }
    return document;
}

// Decision O12: a rocket that QtRocket's loader would not read in full is saved all the same,
// with a warning. DocumentConfig::componentCount() does not count the rocket.
TEST(OpenRocketSaver, WarnsOfMoreComponentsThanTheLoaderReads)
{
    const std::size_t                         atTheBound = DocumentConfig::kMaxComponents - 2;
    const std::unique_ptr<OpenRocketDocument> full       = documentWithMassComponents(atTheBound);
    ASSERT_EQ(DocumentConfig::componentCount(full->getRocket()), DocumentConfig::kMaxComponents);
    EXPECT_TRUE(saveOf(*full).warnings.empty());

    full->getRocket().getChild(0).getChild(0).addChild(std::make_unique<QtRocket::MassComponent>());
    const Saved beyond = saveOf(*full);
    ASSERT_EQ(beyond.error, "");
    EXPECT_TRUE(beyond.text.ends_with("</openrocket>\n"));
    ASSERT_EQ(beyond.warnings.size(), 1U);
    EXPECT_EQ(beyond.warnings.begin()->toString(),
              OpenRocketSaver::componentsBeyondLoaderBound(DocumentConfig::kMaxComponents + 1));
}

// Port of OpenRocketSaverTest.testEstimateFileSize, which asserts nothing; QtRocket pins the
// formula: 80 bytes per component (the rocket included), 100 per simulation, and 100 per data
// point when the data is saved. The estimates of the sixteen examples, OpenRocket's own, are
// pinned in OrkResaveDiffGolden.
TEST(OpenRocketSaver, EstimatesTheFileSizeAsOpenRocket)
{
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript);
    StorageOptions options;
    options.setSaveSimulationData(true);
    // The components of the Alpha III with the rocket, and its one simulation, without data.
    std::size_t components = 0;
    for ([[maybe_unused]] const QtRocket::RocketComponent& component :
         rocketDoc->getRocket().subtree(true))
    {
        ++components;
    }
    EXPECT_EQ(OpenRocketSaver::estimateFileSize(*rocketDoc, options),
              static_cast<std::int64_t>((components * 80) + 100));
    const std::unique_ptr<OpenRocketDocument> empty = OpenRocketDocumentFactory::createNewRocket();
    EXPECT_EQ(OpenRocketSaver::estimateFileSize(*empty, options), 160);
}

}  // namespace
