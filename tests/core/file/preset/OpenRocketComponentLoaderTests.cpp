// Tests of OpenRocketComponentLoader with the fixtures of tests/data/presets (its README.md):
// what OpenRocket's own loader makes of each is the probe tools/openrocket-probes/orc-loader
// (OrcFileProbe, the lines "== file ..." of its expected.txt), pinned here by hand, written as the
// probe writes them (OrcProbeTranscript.h). The probe's 337 edge cases are
// orc_loader_edge_case_tests.cpp.

#include "QtRocket/file/preset/OpenRocketComponentLoader.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/preset/OrcDocument.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "TestPaths.h"
#include "file/preset/OrcProbeTranscript.h"

namespace
{

using QtRocket::OpenRocketComponentLoader;
using QtRocket::Result;
using QtRocket::WarningSet;
using Presets = std::vector<OpenRocketComponentLoader::PresetPtr>;

/// A fixture of tests/data/presets, its size and its SHA-256.
struct FixtureDocument
{
    std::string_view file;
    std::size_t      size;
    std::string_view sha256;
};

// clang-format off
/// The size and SHA-256 of each fixture, as the probe printed them ("document <size> bytes, sha256
/// <digest>").
constexpr auto kFixtures = std::to_array<FixtureDocument>({
    {.file = "broken.orc", .size = 1010, .sha256 = "12cb1962d660ab177c5b5ad32a785abc4f3e65077a314fc2e0f75bae8ed682d3"},
    {.file = "every-kind.orc", .size = 7363, .sha256 = "bcd4e51559ba04ab2a41607d0862e69e8a633321c294e36b11d50bbb11bf3fe7"},
    {.file = "legacy.orc", .size = 1293, .sha256 = "2ae5ad5308ae0589f37085a7c7f147ab17390265c20a126ab34742f66813bc61"},
    {.file = "materials.orc", .size = 5351, .sha256 = "6bc711c52ea64cd4b29d212d528d892248e13015a823312f4718ad98a70afa31"},
    {.file = "replaced-materials.orc", .size = 1101, .sha256 = "eccefeadbf251f89b854c0aa61a463e931d52d0e7628b66a640d888fb5696456"},
    {.file = "section-order.orc", .size = 1524, .sha256 = "fdd7eeaab1616be0ac207a7ba60adc03dfd38ef0941ecb202c2625f3a9a145f9"},
    {.file = "skipped.orc", .size = 1938, .sha256 = "cc91eec23839ffae2d3640e426880c434f06ebec88b179c8d2b942238dae3dcd"},
    {.file = "thumbnail.orc", .size = 850, .sha256 = "9662f73e39d0cce5eab314132cee78f92d7cbebb801ef60f97d94744e9adf8c0"},
    {.file = "unknown-group.orc", .size = 741, .sha256 = "d33ece27a4bd824f8da8dd07cd44a81ea871f279592d19df77894f99ccc74c8d"},
});

/// OpenRocket's presets of every-kind.orc.
constexpr auto kEveryKind = std::to_array<std::string_view>({
    "Legacy=false; Manufacturer=Estes; PartNo=BT-20; Description=Body tube, BT-20, 18 in.; Material=[Bulk 'Paper, spiral kraft glassine, bulk' 799.0 shear 0.0 Other user]; InnerDiameter=0.018033999999999998; OuterDiameter=0.0186944; Length=0.4572; Type=BODY_TUBE; Thickness=3.30200000000001E-4; digest=09167b248e4c15bda33af70e9c6e3f0c",
    "Legacy=false; Manufacturer=Estes; PartNo=JT-20C; Description=Tube coupler, BT-20; Material=[Bulk 'Paper, spiral kraft glassine, bulk' 1746.1570899225069 shear 0.0 Custom user]; Mass=0.0012; InnerDiameter=0.017; OuterDiameter=0.018000000000000002; Length=0.025; Type=TUBE_COUPLER; Thickness=5.000000000000004E-4; digest=d69911afe519f415b438e81de0e64589",
    "Legacy=false; Manufacturer=Estes; PartNo=PNC-20A; Description=Nose cone, plastic, BT-20; Material=[Bulk 'Polystyrene, cast, bulk' 1510.3112310514273 shear 0.0 Custom user]; Mass=0.008504856929999999; Filled=false; Shape=Ogive; AftOuterDiameter=0.0186944; AftShoulderLength=0.019049999999999997; AftShoulderDiameter=0.017983199999999998; Length=0.06985; Type=NOSE_CONE; Thickness=0.0015748; digest=5de47e4676546d8251eaf85c61a17b6f",
    "Legacy=false; Manufacturer=Estes; PartNo=TA-2050; Description=Transition, BT-20 to BT-50, balsa; Material=[Bulk 'Balsa, bulk' 128.0 shear 2.5E8 Woods user]; Filled=true; Shape=Conical; ForeOuterDiameter=0.0186944; ForeShoulderDiameter=0.018033999999999998; ForeShoulderLength=0.0127; AftOuterDiameter=0.024790399999999997; AftShoulderDiameter=0.02413; AftShoulderLength=0.0127; Length=0.03175; Type=TRANSITION; digest=d6f9f2c9e909ee3703bb8a510464d0ea",
    "Legacy=false; Manufacturer=Estes; PartNo=BHB-20; Description=Bulkhead, balsa, BT-20; Material=[Bulk 'Balsa' 170.0 shear 2.3E8 Woods builtin]; OuterDiameter=0.0178562; Length=0.00635; Type=BULK_HEAD; digest=d03550910bc350944141d3a3d6220a2f",
    "Legacy=false; Manufacturer=Estes; PartNo=RA-1320; Description=Centering ring, BT-5 to BT-20; Material=[Bulk 'Paper, spiral kraft glassine, bulk' 799.0 shear 0.0 Other user]; InnerDiameter=0.013792200000000001; OuterDiameter=0.018033999999999998; Length=0.0015875; Type=CENTERING_RING; Thickness=0.0021208999999999985; digest=4d941f84d87b0120898a95067f017ae0",
    "Legacy=false; Manufacturer=Estes; PartNo=EB-20A; Description=Engine block, BT-20; Material=[Bulk 'Paper, spiral kraft glassine, bulk' 799.0 shear 0.0 Other user]; InnerDiameter=0.0127; OuterDiameter=0.018033999999999998; Length=0.00635; Type=ENGINE_BLOCK; Thickness=0.0026669999999999992; digest=44a0958809a12bd4227562f98e5ff8d7",
    "Legacy=false; Manufacturer=Estes; PartNo=LL-2A; Description=Launch lug, 1/8 in. x 2 in.; Material=[Bulk 'Paper, spiral kraft glassine, bulk' 799.0 shear 0.0 Other user]; InnerDiameter=0.003302; OuterDiameter=0.0039624; Length=0.0508; Type=LAUNCH_LUG; Thickness=3.301999999999999E-4; digest=ba3f9e2b52385c9b00021fd1b7ab2dc8",
    "Legacy=false; Manufacturer=Rail-Buttons.com; PartNo=1010; Description=Standard 1010 rail button; Material=[Bulk 'Delrin' 964.3031373843921 shear 0.0 Custom user]; Mass=4.4500000000000003E-4; InnerDiameter=0.0058039; OuterDiameter=0.011112499999999999; Height=0.007556499999999999; BaseHeight=0.0018541999999999999; FlangeHeight=0.0018541999999999999; ScrewHeight=0.0; ScrewMass=0.001395; NutMass=0.0010251187562; Type=RAIL_BUTTON; digest=a23528ffd1abf44cc53660506846b880",
    "Manufacturer=Top Flight Recovery; PartNo=STR-2; Description=Streamer, 2 in. x 40 in.; Material=[Surface 'Nylon fabric, ripstop, 1.7 oz actual' 0.067 shear 0.0 Other user]; Legacy=false; Width=0.0508; Thickness=5.08E-5; Length=1.016; Type=STREAMER; digest=dabe0419e37198eb99aff3de2bdb58e0",
    "Legacy=false; Manufacturer=Estes; PartNo=PK-12; Description=Parachute, 12 in., 6 lines; Material=[Surface 'Nylon fabric, ripstop, 1.7 oz actual' 0.067 shear 0.0 Other user]; Type=PARACHUTE; Diameter=0.30479999999999996; PackedDiameter=0.017779999999999997; PackedLength=0.038099999999999995; DragCoefficient=0.75; LineCount=6; LineLength=0.30479999999999996; Sides=6; LineMaterial=[Line 'Nylon Paracord, 110 lb, 1/16 in. dia.' 9.7E-4 shear 0.0 Other user]; digest=9be18762756e64763ddd6d596be33706",
});

/// OpenRocket's presets of legacy.orc.
constexpr auto kLegacy = std::to_array<std::string_view>({
    "Manufacturer=Quest; PartNo=ST-1; Description=Streamer, mylar, 1 in. x 30 in.; Material=[Surface 'Mylar, 1 mil' 0.0355 shear 0.0 Other user]; Legacy=true; Width=0.0254; Thickness=2.54E-5; Length=0.762; Type=STREAMER; digest=95f0645fac39b372d91b27153c48035a",
    "Legacy=true; Manufacturer=Quest; PartNo=BH-1 ; Description=Bulkhead\\tplywood; Material=[Bulk 'Plywood (birch)' 1952.5290437923866 shear 0.0 Custom user]; Mass=0.00283495231; OuterDiameter=0.02413; Length=0.003175; Type=BULK_HEAD; digest=b7a56afadef920f03c13d529a5e9828b",
});

/// OpenRocket's presets of materials.orc.
constexpr auto kMaterials = std::to_array<std::string_view>({
    "Legacy=false; Manufacturer=Estes; PartNo=section; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=32a6b34a24a512190d43b707def28ebd",
    "Legacy=false; Manufacturer=Estes; PartNo=section g/cm3; Material=[Bulk 'Paper test' 800.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=b69186f83e32067c81af0ceac0360f14",
    "Legacy=false; Manufacturer=Estes; PartNo=section shear and group; Material=[Bulk 'Balsa, bulk' 128.0 shear 2.5E8 Woods user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=69730fd07d38ff1afc7c48baf0089267",
    "Legacy=false; Manufacturer=Estes; PartNo=section SI; Material=[Bulk 'Basswood, bulk' 500.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=b99a22867a2dcc0166dcefec4ba0f011",
    "Legacy=false; Manufacturer=Estes; PartNo=built-in ignoring case; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=2c997c2024d51d4b06ebe8648a91c747",
    "Legacy=false; Manufacturer=Estes; PartNo=other type; Material=[Surface 'Paper test' 0.0 shear 0.0 Other user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=db64a725e0e29b31e2c4ea9351972ef9",
    "Legacy=false; Manufacturer=Estes; PartNo=new in a group; Material=[Bulk 'Unobtainium' 0.0 shear 0.0 Woods user]; InnerDiameter=0.02413; OuterDiameter=0.024790399999999997; Length=0.4572; Type=BODY_TUBE; Thickness=3.3019999999999924E-4; digest=9583bc6cba6f7089dddc42f5504deca0",
    "Legacy=false; Manufacturer=Estes; PartNo=ThreadsLines elastic; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=6; LineLength=0.30479999999999996; LineMaterial=[Line 'Elastic cord (round 2 mm, 1/16 in)' 0.0018 shear 0.0 Elastics builtin]; digest=5a7dabe32b3f51fa85d06af73a154912",
    "Legacy=false; Manufacturer=Estes; PartNo=ThreadsLines other; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=6; LineLength=0.30479999999999996; LineMaterial=[Line 'Thread (heavy-duty)' 0.0 shear 0.0 Other user]; digest=faaad9bf0df02015bbd2bd5ecc71d287",
    "Legacy=false; Manufacturer=Estes; PartNo=ThreadsLines unknown; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=6; LineLength=0.30479999999999996; LineMaterial=[Line 'Mystery cord' 0.0 shear 0.0 Other user]; digest=3dc78a83f310341b9cac64f634bf991f",
    "Legacy=false; Manufacturer=Estes; PartNo=Kevlars unknown; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=6; LineLength=0.30479999999999996; LineMaterial=[Line 'Mystery cord' 0.0 shear 0.0 Kevlars user]; digest=4f9981cc936ed88d6c9462cc3f9a8e8b",
});

/// OpenRocket's presets of section-order.orc.
constexpr auto kSectionOrder = std::to_array<std::string_view>({
    "Legacy=false; Manufacturer=Estes; PartNo=BT-20; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.018033999999999998; OuterDiameter=0.0186944; Length=0.4572; Type=BODY_TUBE; Thickness=3.30200000000001E-4; digest=f91a6ac72879c0e22d5aa2f0c75537af",
});

/// OpenRocket's presets of thumbnail.orc.
constexpr auto kThumbnail = std::to_array<std::string_view>({
    "Legacy=false; Manufacturer=manufacturer; PartNo=partno; Mass=100.0; Image=bytes76:89504e470d0a1a0a0000000d494844520000000200000002080600000072b60d24000000134944415478da63f8cfc0f01f0c813408340000494909789c5117920000000049454e44ae426082; InnerDiameter=1.0; OuterDiameter=2.0; Length=2.0; Type=BODY_TUBE; Thickness=0.5; Material=[Bulk 'TubeCustom' 21.22065907891938 shear 0.0 Custom user]; digest=51382687eda9ff0e822918c836cb4b21",
});

/// OpenRocket's presets of skipped.orc.
constexpr auto kSkipped = std::to_array<std::string_view>({
    "Legacy=false; Manufacturer=Estes; PartNo=BT-20; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.018033999999999998; OuterDiameter=0.0186944; Length=0.4572; Type=BODY_TUBE; Thickness=3.30200000000001E-4; digest=f91a6ac72879c0e22d5aa2f0c75537af",
    "Legacy=false; Manufacturer=Estes; PartNo=PK-12; Material=[Surface 'Ripstop nylon' 0.067 shear 0.0 Fabrics builtin]; Type=PARACHUTE; Diameter=0.30479999999999996; LineCount=<null>; LineLength=0.30479999999999996; digest=d8619facbd8acc1e5f6f646bbb100275",
    "Legacy=false; Manufacturer=Estes; PartNo=BT-INF; Material=[Bulk 'Cardboard' 680.0 shear 4.0E8 PaperProducts builtin]; InnerDiameter=0.018033999999999998; OuterDiameter=0.0186944; Length=Infinity; Type=BODY_TUBE; Thickness=3.30200000000001E-4; digest=1cfc2185bdbd91328b274d3e5649ff01",
    "Legacy=false; Manufacturer=Estes; PartNo=BH-N; Material=[Bulk 'Neutronium' Infinity shear 0.0 Other user]; OuterDiameter=0.02413; Length=0.003175; Type=BULK_HEAD; digest=e589aec13d41a87f686bc91ea26bf7fa",
});

// clang-format on

/// The fixture @p name of tests/data/presets.
[[nodiscard]] std::filesystem::path presetFixture(std::string_view name)
{
    return QtRocket::Test::testDataDir() / "presets" / QtRocket::pathFromUtf8(name);
}

/// The bytes of the fixture @p name.
[[nodiscard]] std::vector<std::byte> presetFixtureBytes(std::string_view name)
{
    Result<std::vector<std::byte>> bytes = QtRocket::readFile(presetFixture(name));
    EXPECT_TRUE(bytes) << (bytes ? std::string() : bytes.error().toString());
    return bytes.value_or(std::vector<std::byte>{});
}

/// The built-in materials, as OpenRocket's ServicesForTesting gives them to the probe.
[[nodiscard]] std::unique_ptr<QtRocket::MaterialStorage> builtinMaterialStorage()
{
    auto materials = std::make_unique<QtRocket::MaterialStorage>();
    QtRocket::addBuiltinMaterials(*materials);
    return materials;
}

/// The texts of @p warnings, in order.
[[nodiscard]] std::vector<std::string> warningTexts(const WarningSet& warnings)
{
    std::vector<std::string> texts;
    for (const QtRocket::Warning& warning : warnings)
    {
        texts.push_back(warning.toString());
    }
    return texts;
}

/// @p lines as strings.
[[nodiscard]] std::vector<std::string> expectedLines(std::span<const std::string_view> lines)
{
    return {lines.begin(), lines.end()};
}

/// "<ErrorCode>: <message>" of the failure of @p result, empty when it is no failure.
[[nodiscard]] std::string failureOf(const Result<Presets>& result)
{
    return result ? std::string()
                  : std::string(QtRocket::toString(result.error().code)) + ": " +
                        result.error().message;
}

/// What the loader makes of the fixture @p name read as its bytes, with the default limits.
struct LoadedFixture
{
    Result<Presets> presets;
    WarningSet      warnings;
};

[[nodiscard]] LoadedFixture loadFixture(std::string_view name)
{
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    LoadedFixture                   loaded{.presets = Presets{}, .warnings = {}};
    loaded.presets = loader.loadFile(presetFixture(name), loaded.warnings);
    return loaded;
}

/// The probe's lines of the presets of @p loaded, which must have read its file.
[[nodiscard]] std::vector<std::string> linesOf(const LoadedFixture& loaded)
{
    EXPECT_TRUE(loaded.presets) << failureOf(loaded.presets);
    return loaded.presets ? QtRocket::Test::orcProbeLines(*loaded.presets)
                          : std::vector<std::string>{};
}

/// An .orc document of the components @p components (no materials section), as bytes.
[[nodiscard]] std::vector<std::byte> orcDocument(std::string_view components)
{
    std::string text =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<OpenRocketComponent><Version>0.1</Version><Components>";
    text += components;
    text += "</Components></OpenRocketComponent>\n";
    const std::span<const std::byte> bytes = std::as_bytes(std::span(text));
    return {bytes.begin(), bytes.end()};
}

TEST(OpenRocketComponentLoader, FixturesAreTheProbesDocuments)
{
    std::set<std::string> probed;
    for (const FixtureDocument& fixture : kFixtures)
    {
        probed.emplace(fixture.file);
        const std::vector<std::byte> bytes = presetFixtureBytes(fixture.file);
        EXPECT_EQ(bytes.size(), fixture.size) << fixture.file;
        EXPECT_EQ(QtRocket::sha256Hex(bytes), fixture.sha256) << fixture.file;
    }
    std::set<std::string> fixtures;
    for (const std::filesystem::directory_entry& entry :
         std::filesystem::directory_iterator(QtRocket::Test::testDataDir() / "presets"))
    {
        if (entry.path().extension() == ".orc")
        {
            fixtures.emplace(QtRocket::pathToUtf8(entry.path().filename()));
        }
    }
    EXPECT_EQ(fixtures, probed);
}

TEST(OpenRocketComponentLoader, LoadsEveryKindAsOpenRocket)
{
    // The eleven kinds with their keys in the order of their DTOs: kLegacy first but in the
    // streamer (after the base properties), kType right after the base properties in the
    // parachute, kThickness after kType in the nose cone; the units of every kind converted.
    const LoadedFixture loaded = loadFixture("every-kind.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(kEveryKind));
    EXPECT_TRUE(loaded.warnings.empty());
}

TEST(OpenRocketComponentLoader, LoadsALegacyFile)
{
    const LoadedFixture loaded = loadFixture("legacy.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(kLegacy));
    ASSERT_TRUE(loaded.presets);
    for (const OpenRocketComponentLoader::PresetPtr& preset : *loaded.presets)
    {
        EXPECT_EQ(preset->getLegacy(), true);
    }
    // Strings are not trimmed: the part number keeps its space.
    EXPECT_EQ(loaded.presets->at(1)->getPartNo(), "BH-1 ");
}

TEST(OpenRocketComponentLoader, FindsMaterialsAsOpenRocket)
{
    // The section by type and exact name (in its unit of measure, with a shear modulus and a
    // group, in SI units without a unit of measure), the built-in materials ignoring case, a
    // new material where the type differs or nothing has the name, and "ThreadsLines".
    const LoadedFixture loaded = loadFixture("materials.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(kMaterials));
}

TEST(OpenRocketComponentLoader, ReadsTheLastComponentsAndStopsAtTheFirstMaterialThatMatches)
{
    const LoadedFixture loaded = loadFixture("section-order.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(kSectionOrder));
    EXPECT_TRUE(loaded.warnings.empty());
}

// Port of BaseComponentDTOTest.testImage, replaced: the Java test marshals a BodyTubeDTO with a
// PNG image through JAXB and unmarshals it again; QtRocket has no .orc saver (U-M3-1) and no
// image codec in the core, so a hand-written .orc file holds the base64 text of a PNG file, and
// the preset's image must be that file's bytes. The other attributes as the Java test asserts
// them, with its tolerance.
TEST(OpenRocketComponentLoader, ReadsAThumbnailAsTheBytesOfItsImage)
{
    const LoadedFixture loaded = loadFixture("thumbnail.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(kThumbnail));
    ASSERT_TRUE(loaded.presets);
    ASSERT_EQ(loaded.presets->size(), 1U);
    const QtRocket::ComponentPreset& preset = *loaded.presets->front();
    ASSERT_TRUE(preset.has(QtRocket::ComponentPreset::kImage));
    EXPECT_EQ(preset.get(QtRocket::ComponentPreset::kImage), presetFixtureBytes("thumbnail.png"));
    EXPECT_NEAR(preset.get(QtRocket::ComponentPreset::kInnerDiameter), 1.0, 0.00001);
    EXPECT_NEAR(preset.get(QtRocket::ComponentPreset::kLength), 2.0, 0.00001);
    EXPECT_NEAR(preset.get(QtRocket::ComponentPreset::kOuterDiameter), 2.0, 0.00001);
    EXPECT_FALSE(preset.has(QtRocket::ComponentPreset::kDescription));
    EXPECT_EQ(preset.getManufacturer().getSimpleName(), "manufacturer");
    EXPECT_NEAR(preset.get(QtRocket::ComponentPreset::kMass), 100.0, 0.00001);
    EXPECT_EQ(preset.getPartNo(), "partno");
}

TEST(OpenRocketComponentLoader, RefusesAFileWithABrokenRequiredElementAndReadsTheNextFile)
{
    // OpenRocket: a NullPointerException, which ends its whole preset load (the later files and
    // the user's); QtRocket refuses the one file (bug 18), and the loader reads the next one.
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    WarningSet                      warnings;
    EXPECT_EQ(failureOf(loader.loadFile(presetFixture("broken.orc"), warnings)),
              "PARSE: Unable to parse component preset file: broken.orc: the BodyTube 'BT-50' "
              "(component 2) has no <Length>");
    const Result<Presets> next = loader.loadFile(presetFixture("every-kind.orc"), warnings);
    ASSERT_TRUE(next) << failureOf(next);
    EXPECT_EQ(QtRocket::Test::orcProbeLines(*next), expectedLines(kEveryKind));
    EXPECT_TRUE(warnings.empty());
}

TEST(OpenRocketComponentLoader, RefusesAMaterialOfAnUnknownGroup)
{
    // OpenRocket: IllegalArgumentException "Unknown material group: Rocks", which ends its
    // whole preset load.
    EXPECT_EQ(failureOf(loadFixture("unknown-group.orc").presets),
              "PARSE: Unable to parse component preset file: unknown-group.orc: the BodyTube "
              "'BT-20' (component 1) has a <Material> of the unknown Group 'Rocks'");
}

TEST(OpenRocketComponentLoader, ChecksTheUnitsOfEveryMaterialsSection)
{
    // OpenRocket: IOException "Unable to parse component preset file: replaced-materials.orc
    // (invalid format or corrupted file)": JAXB unmarshals the replaced section too.
    EXPECT_EQ(failureOf(loadFixture("replaced-materials.orc").presets),
              "PARSE: Unable to parse component preset file: replaced-materials.orc (invalid "
              "format or corrupted file): material 1 of <Materials> gives its density in the "
              "unknown unit 'lb/in\u00B3'");
}

TEST(OpenRocketComponentLoader, SkipsThePresetsOpenRocketCannotUseWithAWarning)
{
    // OpenRocket loads all four presets (kSkipped): the parachute's LineCount is null, which
    // throws a BugException when the preset is applied (D5), and the length and the density are
    // infinite (bug 19).
    const LoadedFixture loaded = loadFixture("skipped.orc");
    EXPECT_EQ(linesOf(loaded), expectedLines(std::span(kSkipped).first(1)));
    EXPECT_EQ(warningTexts(loaded.warnings),
              (std::vector<std::string>{
                  "Component preset file skipped.orc: the Parachute 'PK-12' (component 2) was "
                  "skipped: it has no <LineCount> that is an integer",
                  "Component preset file skipped.orc: the BodyTube 'BT-INF' (component 3) was "
                  "skipped: its <Length> is not a finite number (Infinity)",
                  "Component preset file skipped.orc: the BulkHead 'BH-N' (component 4) was "
                  "skipped: the density of its <Material> 'Neutronium' is not a finite number "
                  "(Infinity)"}));
}

TEST(OpenRocketComponentLoader, KeepsANonFiniteValueTheFactoryDerives)
{
    // internal/semroc-legacy.orc's transition BC-08542 [R] has no volume, so the density of the
    // material its mass makes is infinite: kept, as in OpenRocket (its digest is that of the
    // scouting's dump of OpenRocket's presets).
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    WarningSet                      warnings;
    const Result<Presets>           presets =
        loader.load(orcDocument("<Transition><Manufacturer>SEMROC Astronautics</Manufacturer>"
                                "<PartNumber>BC-08542 [R]</PartNumber>"
                                "<Description>Balsa Nose Cone - Reversed</Description>"
                                "<Material Type=\"BULK\">Balsa</Material>"
                                "<Mass Unit=\"kg\">0.004252428464999999</Mass><Filled>true</Filled>"
                                "<Shape>OGIVE</Shape>"
                                "<ForeOutsideDiameter Unit=\"m\">0.0</ForeOutsideDiameter>"
                                "<ForeShoulderDiameter Unit=\"m\">0.021589999999999998"
                                "</ForeShoulderDiameter>"
                                "<ForeShoulderLength Unit=\"m\">0.0127</ForeShoulderLength>"
                                "<AftOutsideDiameter Unit=\"m\">2.54E-6</AftOutsideDiameter>"
                                "<AftShoulderDiameter Unit=\"m\">0.0</AftShoulderDiameter>"
                                "<AftShoulderLength Unit=\"m\">0.0</AftShoulderLength>"
                                "<Length Unit=\"m\">0.10668</Length></Transition>"),
                    "semroc-legacy.orc", warnings);
    ASSERT_TRUE(presets) << failureOf(presets);
    ASSERT_EQ(presets->size(), 1U);
    const QtRocket::Material& material =
        presets->front()->get(QtRocket::ComponentPreset::kMaterial);
    EXPECT_TRUE(std::isinf(material.getDensity()));
    EXPECT_GT(material.getDensity(), 0.0);
    EXPECT_EQ(presets->front()->getDigest(), "f2289dee557fa551eeec7428c22cb6a0");
    EXPECT_TRUE(warnings.empty());
}

TEST(OpenRocketComponentLoader, AddsItsWarningsToTheCallersSetOnlyForAFileItReads)
{
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    WarningSet                      warnings;
    warnings.add("an earlier warning");
    // A preset skipped, then one that refuses the file: no warning for the file.
    const std::vector<std::byte> refused = orcDocument(
        "<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>A</PartNumber>"
        "<InsideDiameter>NaN</InsideDiameter><OutsideDiameter>0.02</OutsideDiameter>"
        "<Length>0.1</Length></BodyTube>"
        "<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>B</PartNumber>"
        "</BodyTube>");
    EXPECT_EQ(failureOf(loader.load(refused, "refused.orc", warnings)),
              "PARSE: Unable to parse component preset file: refused.orc: the BodyTube 'B' "
              "(component 2) has no <InsideDiameter>");
    EXPECT_EQ(warningTexts(warnings), std::vector<std::string>{"an earlier warning"});
    // The same skipped preset in a file that is read: its warning after the earlier one.
    const std::vector<std::byte> read = orcDocument(
        "<BodyTube><Manufacturer>Estes</Manufacturer><PartNumber>A</PartNumber>"
        "<InsideDiameter>NaN</InsideDiameter><OutsideDiameter>0.02</OutsideDiameter>"
        "<Length>0.1</Length></BodyTube>");
    const Result<Presets> presets = loader.load(read, "read.orc", warnings);
    ASSERT_TRUE(presets) << failureOf(presets);
    EXPECT_TRUE(presets->empty());
    EXPECT_EQ(warningTexts(warnings),
              (std::vector<std::string>{"an earlier warning",
                                        "Component preset file read.orc: the BodyTube 'A' "
                                        "(component 1) was skipped: its <InsideDiameter> is not "
                                        "a finite number (NaN)"}));
}

TEST(OpenRocketComponentLoader, ReportsTheFirstProblemInTheOrderOfTheDtos)
{
    // Java's statements in the order of the DTO: the manufacturer before the dimensions, the
    // inside diameter before the length, the material before the mass; the components are
    // numbered among the eleven kinds only.
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    WarningSet                      warnings;
    EXPECT_EQ(failureOf(loader.load(orcDocument("<FinSet/><BodyTube><PartNumber>A</PartNumber>"
                                                "</BodyTube>"),
                                    "a.orc", warnings)),
              "PARSE: Unable to parse component preset file: a.orc: the BodyTube 'A' "
              "(component 1) has no <Manufacturer>");
    EXPECT_EQ(failureOf(loader.load(
                  orcDocument("<BulkHead><Manufacturer>Estes</Manufacturer>"
                              "<PartNumber>B</PartNumber><OutsideDiameter>0.02</OutsideDiameter>"
                              "<Length>0.01</Length></BulkHead>"
                              "<BodyTube><Manufacturer>Estes</Manufacturer>"
                              "<PartNumber>C</PartNumber>"
                              "<InsideDiameter Unit=\"yard\">1</InsideDiameter></BodyTube>"),
                  "b.orc", warnings)),
              "PARSE: Unable to parse component preset file: b.orc: the BodyTube 'C' "
              "(component 2) gives its <InsideDiameter> in the unknown unit 'yard'");
    EXPECT_EQ(failureOf(loader.load(
                  orcDocument("<BodyTube><Manufacturer>Estes</Manufacturer>"
                              "<PartNumber>D</PartNumber><Material Type=\"ROCK\">Granite</Material>"
                              "<Mass Unit=\"stone\">1</Mass></BodyTube>"),
                  "c.orc", warnings)),
              "PARSE: Unable to parse component preset file: c.orc: the BodyTube 'D' "
              "(component 1) has a <Material> of the Type 'ROCK', which is none of BULK, SURFACE "
              "and LINE");
    EXPECT_TRUE(warnings.empty());
}

TEST(OpenRocketComponentLoader, HasTheBoundsOfTheLoaderPolicy)
{
    // Decision D7 of the scouting (the policy of Milestone 1's loaders).
    EXPECT_EQ(OpenRocketComponentLoader::kMaxFileSize, std::size_t{64} * 1024 * 1024);
    EXPECT_EQ(OpenRocketComponentLoader::kMaxMaterials, 10000U);
    EXPECT_EQ(OpenRocketComponentLoader::kMaxPresets, 100000U);
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    EXPECT_EQ(loader.limits().fileSize, OpenRocketComponentLoader::kMaxFileSize);
    EXPECT_EQ(loader.limits().elements, QtRocket::OrcDocument::kMaxElements);
    EXPECT_EQ(loader.limits().materials, OpenRocketComponentLoader::kMaxMaterials);
    EXPECT_EQ(loader.limits().presets, OpenRocketComponentLoader::kMaxPresets);
}

TEST(OpenRocketComponentLoader, RefusesAFileLargerThanItsBound)
{
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials, {.fileSize = 7363});
    WarningSet                      warnings;
    const std::vector<std::byte>    bytes = presetFixtureBytes("every-kind.orc");
    EXPECT_TRUE(loader.load(bytes, "every-kind.orc", warnings));
    const OpenRocketComponentLoader smaller(*materials, {.fileSize = 7362});
    EXPECT_EQ(failureOf(smaller.load(bytes, "every-kind.orc", warnings)),
              "UNSUPPORTED_FORMAT: Unable to parse component preset file: every-kind.orc: the "
              "file is larger than 7362 bytes");
    // A file is refused before it is read.
    EXPECT_EQ(failureOf(smaller.loadFile(presetFixture("every-kind.orc"), warnings)),
              "IO: Unable to parse component preset file: every-kind.orc: Input exceeds maximum "
              "size of 7362 bytes");
    EXPECT_TRUE(warnings.empty());
}

TEST(OpenRocketComponentLoader, RefusesAFileOfMoreElementsOrMaterialsThanItsBounds)
{
    const auto                      materials = builtinMaterialStorage();
    WarningSet                      warnings;
    const std::vector<std::byte>    bytes = presetFixtureBytes("materials.orc");
    const OpenRocketComponentLoader fewElements(*materials, {.elements = 10});
    EXPECT_EQ(failureOf(fewElements.load(bytes, "materials.orc", warnings)),
              "UNSUPPORTED_FORMAT: Unable to parse component preset file: materials.orc (invalid "
              "format or corrupted file): The document has more than 10 elements.");
    // materials.orc has four section materials.
    const OpenRocketComponentLoader fourMaterials(*materials, {.materials = 4});
    EXPECT_TRUE(fourMaterials.load(bytes, "materials.orc", warnings));
    const OpenRocketComponentLoader threeMaterials(*materials, {.materials = 3});
    EXPECT_EQ(failureOf(threeMaterials.load(bytes, "materials.orc", warnings)),
              "UNSUPPORTED_FORMAT: Unable to parse component preset file: materials.orc: the "
              "file has more than 3 materials");
    EXPECT_TRUE(warnings.empty());
}

TEST(OpenRocketComponentLoader, ReadsTheComponentsUpToItsBoundWithAWarning)
{
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials, {.presets = 2});
    WarningSet                      warnings;
    const Result<Presets>           presets =
        loader.load(presetFixtureBytes("every-kind.orc"), "every-kind.orc", warnings);
    ASSERT_TRUE(presets) << failureOf(presets);
    EXPECT_EQ(QtRocket::Test::orcProbeLines(*presets),
              expectedLines(std::span(kEveryKind).first(2)));
    EXPECT_EQ(warningTexts(warnings),
              std::vector<std::string>{"Component preset file every-kind.orc: it has more than 2 "
                                       "components; the first 2 were read"});
}

TEST(OpenRocketComponentLoader, NamesAFileByItsNameAndFailsForOneItCannotRead)
{
    const auto                      materials = builtinMaterialStorage();
    const OpenRocketComponentLoader loader(*materials);
    WarningSet                      warnings;
    const std::filesystem::path     missing = presetFixture("missing.orc");
    EXPECT_EQ(failureOf(loader.loadFile(missing, warnings)),
              "IO: Unable to parse component preset file: missing.orc: cannot open '" +
                  QtRocket::pathToUtf8(missing) + "' for reading");
    EXPECT_TRUE(warnings.empty());
}

}  // namespace
