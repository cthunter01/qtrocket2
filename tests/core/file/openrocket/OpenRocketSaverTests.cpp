#include "QtRocket/file/openrocket/OpenRocketSaver.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <initializer_list>
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
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/FlightDataType.h"
#include "QtRocket/simulation/LandingDispersionSettings.h"
#include "QtRocket/simulation/PlotAppearance.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/SimulationOptions.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/simulation/extension/impl/ScriptingExtension.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Version.h"
#include "document/TestDocuments.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"
#include "goldens/GoldenResaves.h"
#include "rocket/TestRockets.h"
#include "simulation/SimulationRunSupport.h"

// OpenRocketSaver: the document level of the .ork text. The texts expected are what
// OpenRocket's OpenRocketSaver.save() writes for the same document (the probe of tier 10a S1,
// DocLevelProbe "document"), with the creator attribute set aside and without the <rocket>
// element, whose lines the component savers write (part S2 of tier 10a). And the ports of
// OpenRocketSaverTest: those of the document level and the rocket, and those of the simulations
// (part S3), which save a document and load it back; the texts of the simulations are
// SimulationSaverTests'.

namespace
{

using QtRocket::Color;
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
using QtRocket::PlotAppearance;
using QtRocket::Result;
using QtRocket::Simulation;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::JavaTestPreferences;
using QtRocket::Test::junitEquals;
using QtRocket::Test::kSimulationExtensionScript;
using QtRocket::Test::LoaderEnvironment;
using QtRocket::Test::makeTestRocketV110WithSimulationExtension;
using QtRocket::Test::rocketElementOf;
using QtRocket::Test::withoutCreator;

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

/// @p text without the lines of its <rocket> element (rocketElementOf(), from "  <rocket>" to
/// "  </rocket>"), which stand as one line "  <rocket/>" instead.
[[nodiscard]] std::string withoutRocket(const std::string& text)
{
    const std::string rocket = rocketElementOf(text);
    const std::size_t at     = rocket.empty() ? std::string::npos : text.find(rocket);
    if (at == std::string::npos)
    {
        return text;
    }
    std::string result(text);
    return result.replace(at, rocket.size(), "  <rocket/>\n");
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

// Port of OpenRocketSaverTest.testFileVersion111_withSimulationExtension.
TEST(OpenRocketSaver, TestFileVersion111WithSimulationExtension)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript, preferences.store);
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

/// saveAndReload() with storage options of the test's (defined with the tests of part S3).
[[nodiscard]] Reloaded saveAndReload(OpenRocketDocument& document, const StorageOptions& options,
                                     bool findMotors);

/// OpenRocketSaverTest's saveRocket() and loadRocket() of @p document: the reloaded document, or
/// none with a failure.
[[nodiscard]] Reloaded saveAndReload(OpenRocketDocument& document)
{
    StorageOptions options;
    options.setSaveSimulationData(true);
    return saveAndReload(document, options, false);
}

/// One warning of OpenRocket's reload of a maker's document: OpenRocket's GeneralRocketSaver
/// archive of it, saved with the simulation data and loaded back in the set-up of
/// OpenRocketSaverTest (probe TestDocumentsProbe of run 10b, its "ZW" lines, with the English
/// texts). The archive holds a thrustcurves/<digest>.rse of each motor of the document: the
/// motors of TestRockets have made-up digests, which the motor read back from the file does not
/// give, so each one is also the warning that the attachment holds no motor of its digest.
struct ReloadWarning
{
    std::string_view maker;
    std::string_view warning;
};

/// The warnings, in the order of each reload; the makers not named here reload without one.
constexpr std::array<ReloadWarning, 17> kJavaReloadWarnings{{
    {.maker = "v104_withSimulationData", .warning = "Illegal time step defined, ignoring."},
    {.maker   = "v105_withComponentPreset",
     .warning = "No matching ComponentPreset for component Body Tube found matching "
                "manufacturer partno"},
    {.maker   = "v110_withSimulationExtension",
     .warning = "Embedded motor attachment 'thrustcurves/digest A8 test.rse' contains no motor "
                "matching digest 'digest A8 test'."},
    {.maker   = "v110_withSimulationExtension",
     .warning = "Embedded motor attachment 'thrustcurves/digest B4 test.rse' contains no motor "
                "matching digest 'digest B4 test'."},
    {.maker   = "v110_withSimulationExtension",
     .warning = "No motor with designation 'B4' for manufacturer 'Estes' found."},
    {.maker   = "v110_withSimulationExtension",
     .warning = "Embedded motor attachment 'thrustcurves/digest C6 test.rse' contains no motor "
                "matching digest 'digest C6 test'."},
    {.maker   = "v110_withSimulationExtension",
     .warning = "No motor with designation 'C6' for manufacturer 'Estes' found."},
    {.maker = "v110_withSimulationExtension", .warning = "Illegal time step defined, ignoring."},
    {.maker   = "v110_withSimulationExtension",
     .warning = "Untrusted scripts have been disabled.  You need to manually enable them in the "
                "Simulation options."},
    {.maker   = "v108_withBoosters",
     .warning = "Embedded motor attachment 'thrustcurves/digest M1350 test.rse' contains no "
                "motor matching digest 'digest M1350 test'."},
    {.maker   = "v108_withBoosters",
     .warning = "No motor with designation 'M1350' for manufacturer 'AeroTech' found."},
    {.maker   = "v108_withBoosters",
     .warning = "Embedded motor attachment 'thrustcurves/digest G77 test.rse' contains no motor "
                "matching digest 'digest G77 test'."},
    {.maker   = "v108_withBoosters",
     .warning = "No motor with designation 'G77' for manufacturer 'AeroTech' found."},
    {.maker   = "v108_withDisabledStage",
     .warning = "Embedded motor attachment 'thrustcurves/digest M1350 test.rse' contains no "
                "motor matching digest 'digest M1350 test'."},
    {.maker   = "v108_withDisabledStage",
     .warning = "No motor with designation 'M1350' for manufacturer 'AeroTech' found."},
    {.maker   = "v108_withDisabledStage",
     .warning = "Embedded motor attachment 'thrustcurves/digest G77 test.rse' contains no motor "
                "matching digest 'digest G77 test'."},
    {.maker   = "v108_withDisabledStage",
     .warning = "No motor with designation 'G77' for manufacturer 'AeroTech' found."},
}};

/// What OpenRocket's reload of a maker's document holds (the same probe): the number of
/// components, the rocket included, and the status of each simulation; a script extension as
/// "script enabled|disabled".
struct ReloadedDocument
{
    std::string_view maker;
    std::string_view summary;
};

constexpr std::array<ReloadedDocument, 16> kJavaReloadedDocuments{{
    {.maker = "v100", .summary = "3 components;"},
    {.maker = "v101_withFinTabs", .summary = "4 components;"},
    {.maker = "v101_withTubeCouplerChild", .summary = "5 components;"},
    {.maker = "v104_withSimulationData", .summary = "4 components; CANT_RUN CANT_RUN"},
    {.maker = "v104_withMotor", .summary = "4 components;"},
    {.maker = "v105_withComponentPreset", .summary = "3 components;"},
    {.maker = "v105_withCustomExpression", .summary = "3 components;"},
    {.maker = "v105_withLowerStageRecoveryDevice", .summary = "5 components;"},
    {.maker = "v106_withAppearance", .summary = "3 components;"},
    {.maker = "v106_withMotorMountIgnitionConfig", .summary = "4 components;"},
    {.maker = "v106_withRecoveryDeviceDeploymentConfig", .summary = "4 components;"},
    {.maker = "v106_withStageSeparationConfig", .summary = "6 components;"},
    {.maker = "v110_withSimulationExtension", .summary = "10 components; CANT_RUN script disabled"},
    {.maker = "v108_withBoosters", .summary = "16 components;"},
    {.maker = "v108_withDisabledStage", .summary = "16 components;"},
    {.maker = "for_estimateFileSize", .summary = "9 components; ABORTED ABORTED"},
}};

/// The summary kJavaReloadedDocuments gives @p maker; empty for a maker it does not name.
[[nodiscard]] std::string_view javaSummaryOf(std::string_view maker)
{
    for (const ReloadedDocument& java : kJavaReloadedDocuments)
    {
        if (java.maker == maker)
        {
            return java.summary;
        }
    }
    return {};
}

/// The summary of @p document as kJavaReloadedDocuments writes it.
[[nodiscard]] std::string summaryOf(OpenRocketDocument& document)
{
    std::size_t components = 0;
    for ([[maybe_unused]] const QtRocket::RocketComponent& component :
         document.getRocket().subtree(true))
    {
        ++components;
    }
    std::string summary = std::format("{} components;", components);
    for (std::size_t i = 0; i < document.getSimulationCount(); ++i)
    {
        Simulation& simulation = *document.getSimulation(i);
        summary += std::format(" {}", QtRocket::name(simulation.getStatus()));
        for (const std::shared_ptr<QtRocket::SimulationExtension>& extension :
             simulation.getSimulationExtensions())
        {
            if (const auto* script =
                    dynamic_cast<const QtRocket::ScriptingExtension*>(extension.get()))
            {
                summary += script->isEnabled() ? " script enabled" : " script disabled";
            }
        }
    }
    return summary;
}

/// What differs between OpenRocket's and QtRocket's save of @p maker's document through
/// GeneralRocketSaver (with the simulation data), loaded back from the bytes against
/// @p motors: whether it loads, the warnings of the load and the reloaded document's summary.
/// Empty when nothing does.
[[nodiscard]] std::string createLoadSaveProblems(
    const QtRocket::Test::TestDocumentMaker&     maker,
    const QtRocket::ThrustCurveMotorSetDatabase& motors)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc = maker.make(preferences.store);

    StorageOptions options;
    options.setSaveSimulationData(true);

    // Save rockets, load, validate
    QtRocket::GeneralRocketSaver         saver(preferences.store);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(*rocketDoc, options);
    if (!bytes.has_value())
    {
        return "the save failed: " + bytes.error().toString();
    }
    const DesignFileEnvironment  environment(DesignFileEnvironment::Presets::NONE, motors);
    const Result<LoadedDocument> rocketDocLoaded =
        GeneralRocketLoader(environment.context()).load(*bytes);
    if (!rocketDocLoaded.has_value())
    {
        return "the load failed: " + rocketDocLoaded.error().toString();
    }
    if (rocketDocLoaded->document == nullptr)
    {
        return "no document";
    }
    std::string expected;
    for (const ReloadWarning& warning : kJavaReloadWarnings)
    {
        if (warning.maker == maker.name)
        {
            expected += std::format("{}\n", warning.warning);
        }
    }
    std::string warnings;
    for (const QtRocket::Warning& warning : rocketDocLoaded->warnings)
    {
        warnings += std::format("{}\n", warning.toString());
    }
    std::string problems;
    if (warnings != expected)
    {
        problems += std::format("the warnings\n{}instead of OpenRocket's\n{}", warnings, expected);
    }
    const std::string summary = summaryOf(*rocketDocLoaded->document);
    if (summary != javaSummaryOf(maker.name))
    {
        problems += std::format("reloaded as \"{}\"\n", summary);
    }
    if (!saver.getWarnings().empty())
    {
        problems += "the save gave a warning\n";
    }
    return problems;
}

// Port of OpenRocketSaverTest.testCreateLoadSave: each of the sixteen document makers of
// TestRockets (document/TestDocuments.h) is saved with the simulation data and loaded back in
// the set-up of OpenRocketSaverTest: no component presets, a motor database of the Estes A8 of
// tests/data/motors/Estes_A8.rse and the "A" F12X "digestA". Java saves the plain XML to a file
// (OpenRocketSaver) and asserts only that a document is loaded; the save here goes through
// GeneralRocketSaver to bytes and is loaded from them (decision O5), and the warnings of each
// load and the reloaded document are those of OpenRocket's reload of its own GeneralRocketSaver
// archive of the same maker. (That the save itself is OpenRocket's is
// TestDocumentsGolden.EveryDocumentIsSavedAsOpenRocketSavesIt.)
TEST(OpenRocketSaver, TestCreateLoadSave)
{
    const std::unique_ptr<QtRocket::ThrustCurveMotorSetDatabase> motors =
        QtRocket::Test::openRocketSaverTestMotors();
    std::size_t loaded = 0;
    for (const QtRocket::Test::TestDocumentMaker& maker : QtRocket::Test::testDocumentMakers())
    {
        EXPECT_EQ(createLoadSaveProblems(maker, *motors), "") << maker.name;
        ++loaded;
    }
    EXPECT_EQ(loaded, 16U);
}

/// The activeness of stages 0, 1 and 2 in @p config, as "true false true".
[[nodiscard]] std::string activenessOf(const QtRocket::FlightConfiguration& config)
{
    return std::format("{} {} {}", config.isStageActive(0), config.isStageActive(1),
                       config.isStageActive(2));
}

// Port of OpenRocketSaverTest.testSaveStageActiveness: the activeness of the stages of every
// flight configuration survives a save and a load (<motorconfiguration><stage number active/>).
TEST(OpenRocketSaver, TestSaveStageActiveness)
{
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        QtRocket::Test::makeTestRocketV108WithDisabledStage();

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

// Port of OpenRocketSaverTest.testAppearanceTextureOpacityRoundTrip: the paint's alpha and
// <opacityaffectstexture> survive a save and a load.
TEST(OpenRocketSaver, TestAppearanceTextureOpacityRoundTrip)
{
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        QtRocket::Test::makeTestRocketV106WithAppearance();
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

// The formula of the estimate: 80 bytes per component (the rocket included), 100 per simulation,
// and 100 per data point when the data is saved. The estimates of the sixteen examples,
// OpenRocket's own, are pinned in OrkResaveDiffGolden, those of the document makers in
// TestEstimateFileSize.
TEST(OpenRocketSaver, EstimatesTheFileSizeAsOpenRocket)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript, preferences.store);
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

/// OpenRocket's estimate of the document of a maker, with the simulation data (probe
/// TestDocumentsProbe of run 10b).
struct JavaEstimate
{
    std::string_view maker;
    std::int64_t     estimate;
};

constexpr std::array<JavaEstimate, 16> kJavaEstimates{{
    {.maker = "v100", .estimate = 240},
    {.maker = "v101_withFinTabs", .estimate = 320},
    {.maker = "v101_withTubeCouplerChild", .estimate = 400},
    {.maker = "v104_withSimulationData", .estimate = 520},
    {.maker = "v104_withMotor", .estimate = 320},
    {.maker = "v105_withComponentPreset", .estimate = 240},
    {.maker = "v105_withCustomExpression", .estimate = 240},
    {.maker = "v105_withLowerStageRecoveryDevice", .estimate = 400},
    {.maker = "v106_withAppearance", .estimate = 240},
    {.maker = "v106_withMotorMountIgnitionConfig", .estimate = 320},
    {.maker = "v106_withRecoveryDeviceDeploymentConfig", .estimate = 320},
    {.maker = "v106_withStageSeparationConfig", .estimate = 480},
    {.maker = "v110_withSimulationExtension", .estimate = 900},
    {.maker = "v108_withBoosters", .estimate = 1280},
    {.maker = "v108_withDisabledStage", .estimate = 1280},
    // two simulations of one data point each
    {.maker = "for_estimateFileSize", .estimate = 1120},
}};

/// "<maker> <estimate>" of the document of @p maker, with the simulation data.
[[nodiscard]] std::string estimateOf(const QtRocket::Test::TestDocumentMaker& maker)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc = maker.make(preferences.store);
    StorageOptions                            options;
    options.setSaveSimulationData(true);
    return std::format("{} {}", maker.name, OpenRocketSaver::estimateFileSize(*rocketDoc, options));
}

// Port of OpenRocketSaverTest.testEstimateFileSize, which estimates the size of
// makeTestRocket_v104_withSimulationData() and asserts nothing ("TODO: fix estimateFileSize so
// that it's a lot more accurate"): QtRocket's estimate of the document of every maker is
// OpenRocket's.
TEST(OpenRocketSaver, TestEstimateFileSize)
{
    std::string ours;
    for (const QtRocket::Test::TestDocumentMaker& maker : QtRocket::Test::testDocumentMakers())
    {
        ours += estimateOf(maker) + "\n";
    }
    std::string theirs;
    for (const JavaEstimate& java : kJavaEstimates)
    {
        theirs += std::format("{} {}\n", java.maker, java.estimate);
    }
    EXPECT_EQ(ours, theirs);
}

// ============================================================ the simulations (part S3)

/// OpenRocketSaverTest's saveRocket() with @p options and loadRocket(): the reloaded document, or
/// none with a failure. With @p findMotors the loader finds a motor for every <motor> (Java's
/// test database finds the Alpha III's A8 by its designation), so that a configuration with a
/// motor keeps one.
[[nodiscard]] Reloaded saveAndReload(OpenRocketDocument& document, const StorageOptions& options,
                                     bool findMotors)
{
    const Saved saved = saveOf(document, options);
    EXPECT_EQ(saved.error, "");
    Reloaded reloaded;
    if (findMotors)
    {
        reloaded.environment->findEveryMotor();
    }
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

// Port of OpenRocketSaverTest.testUntrustedScriptDisabledOnLoad: an enabled script that is not
// trusted, whose text holds '<', '&' and "<![CDATA[" (escaped by the save), reads back disabled
// with its text as it was. (Decision D1 of 2026-10-14 disables every script with code on load;
// this one OpenRocket disables too.)
TEST(OpenRocketSaver, TestUntrustedScriptDisabledOnLoad)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(kSimulationExtensionScript, preferences.store);
    const StorageOptions options;
    const Reloaded       rocketDocLoaded = saveAndReload(*rocketDoc, options, false);
    ASSERT_EQ(rocketDocLoaded.document->getSimulationCount(), 1U);
    const std::vector<std::shared_ptr<QtRocket::SimulationExtension>>& extensions =
        rocketDocLoaded.document->getSimulation(0)->getSimulationExtensions();
    ASSERT_EQ(extensions.size(), 1U);
    const auto* ext = dynamic_cast<const QtRocket::ScriptingExtension*>(extensions.front().get());
    ASSERT_NE(ext, nullptr);
    EXPECT_FALSE(ext->isEnabled());
    EXPECT_EQ(ext->getScript(), kSimulationExtensionScript);
}

/// What the reload of makeTestRocketV110WithSimulationExtension(@p script) gives, saved by
/// OpenRocketSaver with new storage options and loaded back from the text in the set-up of
/// OpenRocketSaverTest (no presets, its database of two motors).
struct ScriptReload
{
    std::size_t simulations{0};
    std::size_t extensions{0};
    /// Whether the script extension is enabled, and its text.
    bool        enabled{false};
    std::string script{"(no script extension)"};
    /// The warnings of the load, one per line.
    std::string warnings;
};

[[nodiscard]] ScriptReload reloadScript(std::string_view script)
{
    JavaTestPreferences                       preferences;
    const std::unique_ptr<OpenRocketDocument> rocketDoc =
        makeTestRocketV110WithSimulationExtension(script, preferences.store);
    const Saved saved = saveOf(*rocketDoc);
    EXPECT_EQ(saved.error, "");

    const std::unique_ptr<QtRocket::ThrustCurveMotorSetDatabase> motors =
        QtRocket::Test::openRocketSaverTestMotors();
    const DesignFileEnvironment  environment(DesignFileEnvironment::Presets::NONE, *motors);
    const Result<LoadedDocument> rocketDocLoaded =
        GeneralRocketLoader(environment.context()).load(QtRocket::stringToBytes(saved.text));
    ScriptReload reload;
    if (!rocketDocLoaded.has_value())
    {
        ADD_FAILURE() << rocketDocLoaded.error().toString();
        return reload;
    }
    for (const QtRocket::Warning& warning : rocketDocLoaded->warnings)
    {
        reload.warnings += warning.toString() + "\n";
    }
    reload.simulations = rocketDocLoaded->document->getSimulationCount();
    if (reload.simulations == 0)
    {
        return reload;
    }
    const std::vector<std::shared_ptr<QtRocket::SimulationExtension>>& extensions =
        rocketDocLoaded->document->getSimulation(0)->getSimulationExtensions();
    reload.extensions = extensions.size();
    if (const auto* ext = extensions.empty() ? nullptr
                                             : dynamic_cast<const QtRocket::ScriptingExtension*>(
                                                   extensions.front().get()))
    {
        reload.enabled = ext->isEnabled();
        reload.script  = ext->getScript();
    }
    return reload;
}

/// OpenRocket's warnings of the reload of the document of
/// makeTestRocket_v110_withSimulationExtension() as plain XML, the warning of a disabled script
/// set aside (probe SaverPortsProbe of run 10b): the Alpha III's B4 and C6 are not in the
/// database of OpenRocketSaverTest, and the time step of its preferences is 0.
constexpr std::string_view kV110ReloadWarnings =
    "No motor with designation 'B4' for manufacturer 'Estes' found.\n"
    "No motor with designation 'C6' for manufacturer 'Estes' found.\n"
    "Illegal time step defined, ignoring.\n";

// Port of OpenRocketSaverTest.testTrustedScriptEnabledOnLoad, INVERTED by decision D1 (deviation;
// orchestrator decision Q3 of run 10b). Java marks the script "TESTING" as trusted on the
// computer (ScriptingUtil.setTrustedScript()) and expects it enabled after the reload, with its
// text. QtRocket keeps no such list and runs no scripts: every enabled script that holds code is
// disabled on load, and "TESTING" gets OpenRocket's own warning, as OpenRocket gives it for the
// script when it is not marked trusted (probe SaverPortsProbe: "untrusted"). An enabled script
// that holds nothing stays enabled without a warning, as in OpenRocket (decision E4; the probe:
// "empty" and "blank"). (The roll control script OpenRocket trusts by its hash, which QtRocket
// disables with ScriptingExtension::kTrustedScriptDisabledWarning, is in
// ScriptingExtensionTests and in the tests of the file v1.7 that holds it.)
TEST(OpenRocketSaver, TestTrustedScriptEnabledOnLoad)
{
    const ScriptReload testing = reloadScript("TESTING");
    EXPECT_EQ(testing.simulations, 1U);
    EXPECT_EQ(testing.extensions, 1U);
    EXPECT_FALSE(testing.enabled) << "decision D1: OpenRocket, which trusts it, keeps it enabled";
    EXPECT_EQ(testing.script, "TESTING");
    EXPECT_EQ(testing.warnings, std::string(kV110ReloadWarnings) +
                                    std::string(QtRocket::ScriptingExtension::kDisabledWarning) +
                                    "\n");

    const ScriptReload empty = reloadScript("");
    EXPECT_EQ(empty.extensions, 1U);
    EXPECT_TRUE(empty.enabled) << "decision E4";
    EXPECT_EQ(empty.script, "");
    EXPECT_EQ(empty.warnings, kV110ReloadWarnings);

    const ScriptReload blank = reloadScript(" \r\n\t");
    EXPECT_EQ(blank.extensions, 1U);
    EXPECT_TRUE(blank.enabled) << "decision E4";
    EXPECT_EQ(blank.warnings, kV110ReloadWarnings);
}

/// TestRockets.makeEstesAlphaIII() in a document, with the preference store of OpenRocket's
/// tests for its simulations (Java: new Simulation(rocket) reads the application's).
struct AlphaDocument
{
    /// Declared first, destroyed last: the simulations keep the store.
    JavaTestPreferences                 preferences;
    std::unique_ptr<OpenRocketDocument> document;
    QtRocket::Rocket*                   rocket{nullptr};

    AlphaDocument()
    {
        QtRocket::Test::TestEstesAlphaIII alpha;
        rocket   = alpha.rocket.get();
        document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(alpha.rocket));
    }

    /// Java's new Simulation(rocket).
    [[nodiscard]] std::shared_ptr<Simulation> newSimulation()
    {
        return std::make_shared<Simulation>(*rocket, preferences.store);
    }
};

/// Java's sim.getOptions().setISAAtmosphere(true); setTimeStep(0.05);
/// setFlightConfigurationId(TestRockets.TEST_FCID_0).
void setIsaAndStep(Simulation& simulation)
{
    simulation.getOptions().setIsaAtmosphere(true);
    simulation.getOptions().setTimeStep(0.05);
    simulation.setFlightConfigurationId(QtRocket::Test::testFcid(0));
}

// Port of OpenRocketSaverTest.TestSimStatus: the status of a simulation with and without data
// in the file. The reload finds a motor for each <motor> (see saveAndReload()).
TEST(OpenRocketSaver, TestSimStatus)
{
    AlphaDocument       alpha;
    OpenRocketDocument& rocketDoc = *alpha.document;

    // Hook up some simulations.
    // First sim will not have options set
    const std::shared_ptr<Simulation> sim1 = alpha.newSimulation();
    rocketDoc.addSimulation(sim1);

    // Second sim has options, but hasn't been simulated
    const std::shared_ptr<Simulation> sim2 = alpha.newSimulation();
    setIsaAndStep(*sim2);
    rocketDoc.addSimulation(sim2);

    // Third sim has been executed
    const std::shared_ptr<Simulation> sim3 = alpha.newSimulation();
    setIsaAndStep(*sim3);
    QtRocket::Test::simulateOrFail(*sim3);
    rocketDoc.addSimulation(sim3);

    // Fourth sim has been executed, then configuration changed
    const std::shared_ptr<Simulation> sim4 = alpha.newSimulation();
    setIsaAndStep(*sim4);
    QtRocket::Test::simulateOrFail(*sim4);
    sim4->getOptions().setTimeStep(0.1);
    rocketDoc.addSimulation(sim4);

    // save, then load document
    StorageOptions options;
    options.setSaveSimulationData(true);

    const Reloaded rocketDocLoaded = saveAndReload(rocketDoc, options, true);
    ASSERT_EQ(rocketDocLoaded.document->getSimulationCount(), 4U);
    EXPECT_EQ(rocketDocLoaded.document->getSimulation(0)->getStatus(),
              Simulation::Status::CANT_RUN);
    EXPECT_EQ(rocketDocLoaded.document->getSimulation(1)->getStatus(),
              Simulation::Status::NOT_SIMULATED);
    EXPECT_EQ(rocketDocLoaded.document->getSimulation(2)->getStatus(), Simulation::Status::LOADED);
    EXPECT_EQ(rocketDocLoaded.document->getSimulation(3)->getStatus(),
              Simulation::Status::OUTDATED);
}

// Port of OpenRocketSaverTest.testAtmosphereHumiditySavedAndLoaded: the conditions of the
// extended ISA atmosphere survive a save and a load.
TEST(OpenRocketSaver, TestAtmosphereHumiditySavedAndLoaded)
{
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> sim = alpha.newSimulation();
    sim->getOptions().setIsaAtmosphere(false);
    sim->getOptions().setLaunchTemperature(280.0);
    sim->getOptions().setLaunchPressure(95000.0);
    sim->getOptions().setLaunchRelativeHumidity(0.55);
    sim->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    alpha.document->addSimulation(sim);

    const Reloaded rocketDocLoaded = saveAndReload(*alpha.document, StorageOptions{}, false);

    ASSERT_EQ(rocketDocLoaded.document->getSimulationCount(), 1U);
    const QtRocket::SimulationOptions& loaded =
        rocketDocLoaded.document->getSimulation(0)->getOptions();
    EXPECT_FALSE(loaded.isIsaAtmosphere());
    EXPECT_TRUE(junitEquals(0.55, loaded.getLaunchRelativeHumidity(), 1e-12));
    EXPECT_TRUE(junitEquals(280.0, loaded.getLaunchTemperature(), 1e-12));
    EXPECT_TRUE(junitEquals(95000.0, loaded.getLaunchPressure(), 1e-9));
}

// Port of OpenRocketSaverTest.testFixedRandomSeedSavedAndLoaded: <randomseed> is written only
// for a fixed seed, which a load keeps fixed.
TEST(OpenRocketSaver, TestFixedRandomSeedSavedAndLoaded)
{
    AlphaDocument alpha;

    const std::shared_ptr<Simulation> fixedSeedSimulation = alpha.newSimulation();
    fixedSeedSimulation->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    fixedSeedSimulation->getOptions().setRandomSeed(-123456789);
    fixedSeedSimulation->getOptions().setRandomSeedFixed(true);
    alpha.document->addSimulation(fixedSeedSimulation);

    const std::shared_ptr<Simulation> randomSeedSimulation = alpha.newSimulation();
    randomSeedSimulation->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    randomSeedSimulation->getOptions().setRandomSeed(987654321);
    alpha.document->addSimulation(randomSeedSimulation);

    const Reloaded loadedDocument = saveAndReload(*alpha.document, StorageOptions{}, false);
    ASSERT_EQ(loadedDocument.document->getSimulationCount(), 2U);

    const Simulation& loadedFixedSeedSimulation = *loadedDocument.document->getSimulation(0);
    EXPECT_TRUE(loadedFixedSeedSimulation.getOptions().isRandomSeedFixed());
    EXPECT_EQ(loadedFixedSeedSimulation.getOptions().getRandomSeed(), -123456789);

    const Simulation& loadedRandomSeedSimulation = *loadedDocument.document->getSimulation(1);
    EXPECT_FALSE(loadedRandomSeedSimulation.getOptions().isRandomSeedFixed());
}

// Port of OpenRocketSaverTest.testLandingDispersionSettingsRemainAbsentUntilConfigured.
TEST(OpenRocketSaver, TestLandingDispersionSettingsRemainAbsentUntilConfigured)
{
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> simulation = alpha.newSimulation();
    simulation->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    alpha.document->addSimulation(simulation);

    EXPECT_FALSE(simulation->getLandingDispersionSettings().has_value());
    EXPECT_EQ(OpenRocketSaver::calculateNecessaryFileVersion(*alpha.document, StorageOptions{}),
              111);

    EXPECT_FALSE(saveOf(*alpha.document).text.contains("<landingdispersion"));
    const Reloaded loaded = saveAndReload(*alpha.document, StorageOptions{}, false);
    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);
    EXPECT_FALSE(loaded.document->getSimulation(0)->getLandingDispersionSettings().has_value());
}

// Port of OpenRocketSaverTest.testLandingDispersionSettingsSavedAndLoaded, on the settings as
// QtRocket keeps them until the Monte Carlo analysis exists (LandingDispersionSettings, the texts
// of the file; HOOK(monte-carlo)): Java's MonteCarloSettings of 750 runs, the seed -123456789 and
// three uncertainties (its builder and its number of threads, which a file does not store, are
// Milestone 7's). The texts are those OpenRocket writes for the same settings.
TEST(OpenRocketSaver, TestLandingDispersionSettingsSavedAndLoaded)
{
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> simulation = alpha.newSimulation();
    simulation->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    using Attributes = QtRocket::LandingDispersionSettings::Attributes;
    const QtRocket::LandingDispersionSettings settings(
        Attributes{{"runs", "750"}, {"seed", "-123456789"}},
        {Attributes{{"parameter", "windspeed"}, {"distribution", "uniform"}, {"spread", "1.25"}},
         Attributes{
             {"parameter", "airdensity"}, {"distribution", "lognormal"}, {"spread", "0.015"}},
         Attributes{{"parameter", "launchguidedirection"},
                    {"distribution", "normal"},
                    {"spread", "0.07853981633974483"}}});
    simulation->setLandingDispersionSettings(settings);
    alpha.document->addSimulation(simulation);

    EXPECT_EQ(OpenRocketSaver::calculateNecessaryFileVersion(*alpha.document, StorageOptions{}),
              111);
    const std::string xml = saveOf(*alpha.document).text;
    EXPECT_TRUE(xml.contains("<openrocket version=\"1.11\""));
    EXPECT_TRUE(xml.contains("<landingdispersion runs=\"750\" seed=\"-123456789\">"));
    EXPECT_TRUE(xml.contains("parameter=\"airdensity\" distribution=\"lognormal\""));

    const Reloaded loaded = saveAndReload(*alpha.document, StorageOptions{}, false);
    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);
    const std::optional<QtRocket::LandingDispersionSettings>& back =
        loaded.document->getSimulation(0)->getLandingDispersionSettings();
    ASSERT_TRUE(back.has_value());
    const QtRocket::LandingDispersionSettings none;
    EXPECT_EQ(back.value_or(none).getAttribute("runs"), settings.getAttribute("runs"));
    EXPECT_EQ(back.value_or(none).getAttribute("seed"), settings.getAttribute("seed"));
    EXPECT_EQ(back.value_or(none).getUncertainties(), settings.getUncertainties());
}

/// The flight data type @p id.
[[nodiscard]] const QtRocket::FlightDataType& dataType(QtRocket::FlightDataTypeId id)
{
    return QtRocket::FlightDataType::builtin(id);
}

/// The plot appearance @p simulation has for @p id, or a sentinel appearance whose color is
/// Color(1, 2, 3, 4) and whose line style is DASHDOT for none.
[[nodiscard]] PlotAppearance appearanceOf(const Simulation&          simulation,
                                          QtRocket::FlightDataTypeId id)
{
    return simulation.getPlotAppearance(dataType(id))
        .value_or(PlotAppearance(Color(1, 2, 3, 4), QtRocket::LineStyle::DASHDOT));
}

// Port of OpenRocketSaverTest.testPlotAppearanceSavedAndLoaded: a color with a line style, a
// color alone and a line style alone survive a save and a load.
TEST(OpenRocketSaver, TestPlotAppearanceSavedAndLoaded)
{
    using QtRocket::FlightDataTypeId;
    using QtRocket::LineStyle;
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> sim = alpha.newSimulation();
    sim->setFlightConfigurationId(QtRocket::Test::testFcid(0));

    // Set appearance for altitude (symbol "h"): custom color + dashed line
    sim->setPlotAppearance(dataType(FlightDataTypeId::TYPE_ALTITUDE),
                           PlotAppearance(Color(255, 0, 128, 200), LineStyle::DASHED));
    // Set appearance for velocity (symbol "Vz"): color only, no line style override
    sim->setPlotAppearance(dataType(FlightDataTypeId::TYPE_VELOCITY_Z),
                           PlotAppearance(Color(0, 100, 200), std::nullopt));
    // Set appearance for acceleration (symbol "Az"): line style only, no color
    sim->setPlotAppearance(dataType(FlightDataTypeId::TYPE_ACCELERATION_Z),
                           PlotAppearance(std::nullopt, LineStyle::DOTTED));
    alpha.document->addSimulation(sim);

    const Reloaded loaded = saveAndReload(*alpha.document, StorageOptions{}, false);
    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);
    const Simulation& loadedSim = *loaded.document->getSimulation(0);

    // Verify altitude appearance (color + line style)
    ASSERT_TRUE(loadedSim.getPlotAppearance(dataType(FlightDataTypeId::TYPE_ALTITUDE)).has_value())
        << "Altitude appearance should have been saved";
    const PlotAppearance altAppearance = appearanceOf(loadedSim, FlightDataTypeId::TYPE_ALTITUDE);
    EXPECT_EQ(altAppearance.getColor(), Color(255, 0, 128, 200))
        << "Altitude color should have been saved";
    EXPECT_EQ(altAppearance.getLineStyle(), LineStyle::DASHED);

    // Verify velocity appearance (color only)
    ASSERT_TRUE(
        loadedSim.getPlotAppearance(dataType(FlightDataTypeId::TYPE_VELOCITY_Z)).has_value())
        << "Velocity appearance should have been saved";
    const PlotAppearance velAppearance = appearanceOf(loadedSim, FlightDataTypeId::TYPE_VELOCITY_Z);
    EXPECT_EQ(velAppearance.getColor(), Color(0, 100, 200, 255))
        << "Velocity color should have been saved";
    // Line style was null, so it should not be set
    EXPECT_EQ(velAppearance.getLineStyle(), std::nullopt);

    // Verify acceleration appearance (line style only)
    ASSERT_TRUE(
        loadedSim.getPlotAppearance(dataType(FlightDataTypeId::TYPE_ACCELERATION_Z)).has_value())
        << "Acceleration appearance should have been saved";
    const PlotAppearance accAppearance =
        appearanceOf(loadedSim, FlightDataTypeId::TYPE_ACCELERATION_Z);
    EXPECT_EQ(accAppearance.getColor(), std::nullopt) << "Acceleration color should be null";
    EXPECT_EQ(accAppearance.getLineStyle(), LineStyle::DOTTED);
}

// Port of OpenRocketSaverTest.testPlotAppearanceNotSavedWhenEmpty.
TEST(OpenRocketSaver, TestPlotAppearanceNotSavedWhenEmpty)
{
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> sim = alpha.newSimulation();
    sim->setFlightConfigurationId(QtRocket::Test::testFcid(0));

    // Set an empty appearance (both null) -- should not be persisted
    sim->setPlotAppearance(dataType(QtRocket::FlightDataTypeId::TYPE_ALTITUDE),
                           PlotAppearance(std::nullopt, std::nullopt));
    alpha.document->addSimulation(sim);

    const Reloaded loaded = saveAndReload(*alpha.document, StorageOptions{}, false);
    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);
    EXPECT_FALSE(loaded.document->getSimulation(0)
                     ->getPlotAppearance(dataType(QtRocket::FlightDataTypeId::TYPE_ALTITUDE))
                     .has_value())
        << "Empty appearance should not be persisted";
}

// Port of OpenRocketSaverTest.testPlotAppearancePreservedAcrossMultipleSaves.
TEST(OpenRocketSaver, TestPlotAppearancePreservedAcrossMultipleSaves)
{
    AlphaDocument                     alpha;
    const std::shared_ptr<Simulation> sim = alpha.newSimulation();
    sim->setFlightConfigurationId(QtRocket::Test::testFcid(0));
    sim->setPlotAppearance(dataType(QtRocket::FlightDataTypeId::TYPE_ALTITUDE),
                           PlotAppearance(Color(10, 20, 30, 40), QtRocket::LineStyle::DASHDOT));
    alpha.document->addSimulation(sim);

    // First round-trip
    const Reloaded first = saveAndReload(*alpha.document, StorageOptions{}, false);
    // Second round-trip
    const Reloaded loaded = saveAndReload(*first.document, StorageOptions{}, false);

    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);
    const std::optional<PlotAppearance> appearance =
        loaded.document->getSimulation(0)->getPlotAppearance(
            dataType(QtRocket::FlightDataTypeId::TYPE_ALTITUDE));
    ASSERT_TRUE(appearance.has_value()) << "Appearance should survive two round-trips";
    const PlotAppearance kept = appearance.value_or(PlotAppearance(std::nullopt, std::nullopt));
    EXPECT_EQ(kept.getColor(), Color(10, 20, 30, 40));
    EXPECT_EQ(kept.getLineStyle(), QtRocket::LineStyle::DASHDOT);
}

// Port of OpenRocketSaverTest.testDatapointPrecision: a time of 0.012345678 s is stored with
// six decimals (STORAGE_DECIMAL_PLACES), so it reads back within 1 us.
TEST(OpenRocketSaver, TestDatapointPrecision)
{
    using QtRocket::FlightDataTypeId;
    // Build a minimal rocket
    auto  rocket = std::make_unique<QtRocket::Rocket>();
    auto& stage  = rocket->addChild(std::make_unique<QtRocket::AxialStage>());
    stage.addChild(std::make_unique<QtRocket::BodyTube>(0.3, 0.025, 0.002));
    rocket->enableEvents();
    QtRocket::Rocket&                         rocketRef = *rocket;
    const std::unique_ptr<OpenRocketDocument> doc =
        OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));

    // A time value in the 0.001-0.1 s range: %.3f would store "0.012" (off by ~0.345 ms)
    // but STORAGE_DECIMAL_PLACES=6 stores "0.012346" (within 1 us of the original).
    const double precisionValue = 0.012345678;

    const auto branch = std::make_shared<QtRocket::FlightDataBranch>(
        "Sustainer",
        std::initializer_list<std::reference_wrapper<const QtRocket::FlightDataType>>{
            dataType(FlightDataTypeId::TYPE_TIME), dataType(FlightDataTypeId::TYPE_ALTITUDE)});
    branch->addPoint();
    branch->setValue(dataType(FlightDataTypeId::TYPE_TIME), precisionValue);
    branch->setValue(dataType(FlightDataTypeId::TYPE_ALTITUDE), 1000.123456789);
    branch->immute();

    const auto flightData = std::make_shared<QtRocket::FlightData>(
        std::initializer_list<std::shared_ptr<QtRocket::FlightDataBranch>>{branch});

    JavaTestPreferences preferences;
    doc->addSimulation(std::make_shared<Simulation>(
        doc.get(), rocketRef, Simulation::Status::UPTODATE, "precision test",
        QtRocket::SimulationOptions(preferences.store),
        std::vector<std::shared_ptr<QtRocket::SimulationExtension>>{}, flightData));

    StorageOptions opts;
    opts.setSaveSimulationData(true);
    const Reloaded loaded = saveAndReload(*doc, opts, false);
    ASSERT_EQ(loaded.document->getSimulationCount(), 1U);

    const std::shared_ptr<QtRocket::FlightData>& loadedData =
        loaded.document->getSimulation(0)->getSimulatedData();
    ASSERT_NE(loadedData, nullptr) << "Simulation data must be saved when saveSimulationData=true";
    ASSERT_GE(loadedData->getBranchCount(), 1U);

    const std::vector<double>* times =
        loadedData->getBranch(0).getView(dataType(FlightDataTypeId::TYPE_TIME));
    ASSERT_NE(times, nullptr);
    ASSERT_FALSE(times->empty());

    // Tolerance of 1e-6 s (1 us): passes with STORAGE_DECIMAL_PLACES=6,
    // would fail with the old DEFAULT_DECIMAL_PLACES=3 which stores only "0.012".
    EXPECT_TRUE(junitEquals(precisionValue, times->front(), 1e-6))
        << "Time value lost precision in .ork round-trip -- check STORAGE_DECIMAL_PLACES";
}

}  // namespace
