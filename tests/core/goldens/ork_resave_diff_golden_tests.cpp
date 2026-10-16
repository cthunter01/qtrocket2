// QtRocket's .ork save of OpenRocket's designs against OpenRocket's own re-save of them
// (tests/data/goldens/<input>/resave/rocket.ork, written by the golden harness with
// OpenRocketSaver and saveSimulationData=false, on the design as loaded or built, before
// anything was settled), the whole text against the whole text (compareOrkTexts() of
// GoldenResaves.h: the creator attribute of the root is the only text set aside; a number written
// otherwise but of the same value would be counted, and none is).
//
// The inputs (decision O4 of tier 10):
// - The sixteen examples, each loaded fresh from the ORIGINAL archive under
//   goldenExampleEnvironment() with the six presets and the bundled motor database, and saved
//   without simulation data with the environment's own preference store, as the harness saved
//   them: the save is the first thing done to the loaded design (the getters of automatic
//   dimensions store what they compute, so a save depends on what was asked before it; see
//   AutomaticDimensions.h). Every line is OpenRocket's: the declaration and the root, the
//   rocket with the <motormount> blocks and the motors of the database, the simulations (the
//   conditions, the extensions, the summary values and the stored warnings), the photo studio
//   and the document preferences.
// - The same design saved again: three more saves of each loaded example. OpenRocket's own
//   saves of one loaded document differ in one example only, the dual parachute deployment, in
//   three lines of its rocket (the probe RepeatSaveProbe of the scouts of tier 10,
//   roundtrip-goldens out/repeat/: a body tube's automatic radius and a parachute's automatic
//   packed radius are stored by the first save's getters, then the packed length by the second's),
//   and the fourth save is the third. QtRocket's saves have to do the same, which holds the order
//   of the getter calls of the savers to OpenRocket's.
// - The same design saved once more WITH its simulation data (all sixteen store full flight
//   data), then as an .ork archive with the document's default storage options
//   (GeneralRocketSaver), which is OpenRocket's archive: the same entries in the same order
//   (rocket.ork, the decal images by name, a RockSim engine file of every thrust-curve motor; the
//   scouts' FixedPointProbe, roundtrip-goldens out/fp/), rocket.ork the save with data, every decal
//   image the bytes of the original archive, every thrust curve OpenRocket's text (SHA-256
//   digests). The archive is loaded back from memory, without a directory (decision O5 of tier 10),
//   with no warning: every flight comes back bit for bit (flightDifferences(): the summary values,
//   the stored warnings, the branches with their events and every value), and a save of the reload
//   is the first save with data, the sixteen stored warnings of the type
//   RecoveryHighSpeedDeployment (in six examples) included: the loader reads that type back as it
//   was written (the user's decision E2 of 2026-10-14), where OpenRocket's reads it as a warning of
//   the type Other without its parameter. OpenRocket's own first
//   save with data of the same sixteen loads (the scouts' SaverProbe, saver-simulations out/java/,
//   not part of the repository) is QtRocket's save with data outside the rocket element, byte for
//   byte: the SHA-256 digests of those texts are pinned (kJavaSavesWithData). For two examples,
//   "Deployable payload" and "Pods--powered with recovery deployment", the goldens hold that save
//   itself (resave-data/rocket.ork.gz, the manifest's "resaves.withData": stored events of every
//   kind the examples have but TUMBLE, which three other examples store and kJavaSavesWithData
//   covers, stored warnings with their sources, the boosters' branches, 70 columns), and
//   QtRocket's save with data is it, the whole text but for the creator.
// - The three examples with a <preset> again with no presets (Presets::NONE): the six
//   references that cannot be resolved are kept (the user's decision U8 of 2026-10-08) and
//   written back, so the file is the re-save's all the same.
// - The thirteen test rockets as save(load(re-save)) (loadResave() of GoldenResaves.h, with the
//   seven motors of TestRockets and OpenRocket's test preferences): the re-save but for one line,
//   the wall of the pod body of the Alpha III with pods, 2 mm in the file and none in the loaded
//   tube (a tube without a radius keeps no wall), as OpenRocket's own reload has it.
// - The eighteen design files of tests/data/ork (the legacy files: OpenRocket 0.9 to 23.09, the
//   file formats 1.0 to 1.9), each loaded as the harness loaded it for its re-save
//   (legacy-*/resave/rocket.ork; the harness's environment with the six presets of the examples,
//   which are the presets of the tests; DesignFileEnvironment) and saved without simulation data.
//   A loader draws an id wherever the file stores none (every component of the formats 1.0 to
//   1.8, which store no component id; a flight configuration the file does not name, such as
//   those of simplerocket.ork's simulations with an empty <configid>): the drawn ids are numbered
//   by their first occurrence on both sides (withDrawnIdsNumbered()). The save is the re-save but
//   for QtRocket's own decisions (kLegacySaveChanges, every line in its place): the references to
//   presets the six presets do not hold (U8), the scripts disabled on load (D1) and the extension
//   of an unknown id that is kept (D11).
// - The saver-coverage documents of the harness (coverage-*/resave/rocket.ork, built by
//   CoverageDocuments.java and fixed points of OpenRocket's own save and load): what no other
//   re-save holds (a kit name, a custom reference length, CD and CG overrides with the flags of
//   their subcomponents, a line style, a drogue, a nozzle exit diameter, a custom expression,
//   document preferences, all 29 photo settings, the extended ISA atmosphere, a constant gravity,
//   landing dispersion settings, plot appearances, drag and stability lookup tables, stored flight
//   data with an abort and an event after the landing), so that with the other goldens they hold
//   every element and attribute name OpenRocket's saver writes. Each is loaded from its re-save
//   and saved again as the harness saved it: the re-save, every line.
//
// The same loop pins OpenRocket's estimateFileSize() of each loaded example (one load per
// example serves everything).

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/file/openrocket/OpenRocketSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenResaves.h"
#include "simulation/FlightDifferences.h"

namespace
{

using QtRocket::ErrorSet;
using QtRocket::FlightData;
using QtRocket::GeneralRocketLoader;
using QtRocket::GeneralRocketSaver;
using QtRocket::LoadedDocument;
using QtRocket::OpenRocketDocument;
using QtRocket::OpenRocketSaver;
using QtRocket::Preferences;
using QtRocket::Result;
using QtRocket::StorageOptions;
using QtRocket::WarningSet;
using QtRocket::ZipArchive;
using QtRocket::Test::changedLines;
using QtRocket::Test::compareOrkTexts;
using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::ExampleSource;
using QtRocket::Test::flightDifferences;
using QtRocket::Test::GoldenExample;
using QtRocket::Test::goldenExampleEnvironment;
using QtRocket::Test::goldenExamples;
using QtRocket::Test::GoldenInput;
using QtRocket::Test::kMaxReportedLines;
using QtRocket::Test::kPods;
using QtRocket::Test::lineViewsOf;
using QtRocket::Test::LoadedResave;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::loadResave;
using QtRocket::Test::OrkTextComparison;
using QtRocket::Test::placedLineChanges;
using QtRocket::Test::rocketElementOf;
using QtRocket::Test::sameByValue;
using QtRocket::Test::testRocketInputs;
using QtRocket::Test::withDrawnIdsNumbered;
using QtRocket::Test::withoutCreator;

using Presets = DesignFileEnvironment::Presets;

/// OpenRocket's estimateFileSize() of an example as loaded, with the document's default storage
/// options (which save the simulation data of all sixteen): the scouts' Java probe of tier 10
/// (saver-simulations, out/estimates.txt).
struct EstimatedSize
{
    std::string_view file;
    std::int64_t     bytes;
};

constexpr std::array<EstimatedSize, 16> kEstimatedSizes{{
    {.file = "3D printable nose cone and fins.ork", .bytes = 200680},
    {.file = "Airstart timing.ork", .bytes = 328440},
    {.file = "ARC payload rocket.ork", .bytes = 142760},
    {.file = "A simple model rocket.ork", .bytes = 259540},
    {.file = "Chute release.ork", .bytes = 102800},
    {.file = "Clustered motors.ork", .bytes = 153060},
    {.file = "Deployable payload.ork", .bytes = 422520},
    {.file = "Dual parachute deployment.ork", .bytes = 335500},
    {.file = "Parallel booster staging.ork", .bytes = 240040},
    {.file = "Pods--airframes and winglets.ork", .bytes = 243520},
    {.file = "Pods--powered with recovery deployment.ork", .bytes = 38700},
    {.file = "Simulation extensions.ork", .bytes = 310820},
    {.file = "Simulation scripting.ork", .bytes = 329120},
    {.file = "Three stage low power rocket.ork", .bytes = 394640},
    {.file = "Tube fin rocket.ork", .bytes = 65320},
    {.file = "Two stage high power rocket.ork", .bytes = 160300},
}};

/// OpenRocket's own save WITH simulation data of an example as loaded, its first save (the scouts'
/// SaverProbe of tier 10, saver-simulations out/java/<example>.save1.xml, not in the repository):
/// the SHA-256 digest of its text without the creator and without the rocket element
/// (withoutCreator(), withoutRocket(); the rocket of a first save is not that of the later saves
/// the test compares: kDualParachuteSaves). Written by scripts/make_java_save_digests.py of the
/// probes of tier 10a's fixer of the group 'saver' from those files.
struct JavaSaveWithData
{
    std::string_view file;
    std::string_view sha256;
};

constexpr std::array<JavaSaveWithData, 16> kJavaSavesWithData{{
    {.file   = "3D printable nose cone and fins.ork",
     .sha256 = "1f5ac695a30bca373035c4dc652f956fb3db0fc83bf81a264ef81c77df91e035"},
    {.file   = "Airstart timing.ork",
     .sha256 = "571283ab8f8313228f09ed9247f439ec096a4748e9304ad7b16e46b98c44701a"},
    {.file   = "ARC payload rocket.ork",
     .sha256 = "05b9a77fe36646ff4571a971188e04db4828ce8a02e4aa16ef3e0256cc75c0a3"},
    {.file   = "A simple model rocket.ork",
     .sha256 = "d81c33ec6a84aa29b24887e73f4a237fbad890645de32f1e5c0b484b29ead8ad"},
    {.file   = "Chute release.ork",
     .sha256 = "a5abe999864d068f2aaf2316eff5cd172df267d42e07fe977312d26a5535c77d"},
    {.file   = "Clustered motors.ork",
     .sha256 = "35d1b5a5e01a10c1dfd3c5466c81ca45ea24f551e845d16cedddc89b5ba66242"},
    {.file   = "Deployable payload.ork",
     .sha256 = "caafea4d587b28cc9172200f0dea7ab603a015d924d5ba2962bd19b352853350"},
    {.file   = "Dual parachute deployment.ork",
     .sha256 = "f5fd5cb7ad00dcd87fc06359da5536668ccf68705c404c8ddc87c2a646fd8234"},
    {.file   = "Parallel booster staging.ork",
     .sha256 = "8ba9675d3cb7efb6c3eb4e15efb47c5587e42b7d10c0a773b23bd36f9fd066fb"},
    {.file   = "Pods--airframes and winglets.ork",
     .sha256 = "46a1d20779eed1f5c0b3cdc970307b002a7ed9077375a407668b07ab9af49f14"},
    {.file   = "Pods--powered with recovery deployment.ork",
     .sha256 = "9e3cb252a0677eabb847d06ef6e41f7f6284834455bcb8cefc92d142ded329ac"},
    {.file   = "Simulation extensions.ork",
     .sha256 = "69195380e3add32ef11ce7c0df3dac951243366029028bf6156261cf77b63828"},
    {.file   = "Simulation scripting.ork",
     .sha256 = "37d12c53ca5085f3e29857855503485617f6723ceadc579178611b9248d34ab2"},
    {.file   = "Three stage low power rocket.ork",
     .sha256 = "4f3fa0ce994f7b6a94c447342b037f3fb3b7704004402d9d1434ef56c4c0af59"},
    {.file   = "Tube fin rocket.ork",
     .sha256 = "09869f8b618012be46b83ebb60c722e12d76e57b6d251f52b1887cdb14e0ee4d"},
    {.file   = "Two stage high power rocket.ork",
     .sha256 = "ed55479f33575adc1613ec6f00fd661b3dcc11cbc91cd1997f7e695b5bbf7d72"},
}};

/// OpenRocket's .ork archive of an example: the entries of GeneralRocketSaver's save of the
/// example as loaded, with the document's default storage options, one name per line in their
/// order (the probe FixedPointProbe of the scouts of tier 10, roundtrip-goldens out/fp/<example>/
/// Z.ork, in the golden harness's environment): rocket.ork, the decal images in the order of
/// their names, and a RockSim engine file of every thrust-curve motor, once a digest.
struct JavaArchive
{
    std::string_view example;
    std::string_view entries;
};

/// The SHA-256 digest of OpenRocket's thrustcurves/<digest>.rse entry of each motor of those
/// archives (RockSimMotorWriter), by entry name.
struct EmbeddedCurve
{
    std::string_view entry;
    std::string_view sha256;
};

// Written by make_archive_tables.py (the probes of tier 10a S4) from those files.
constexpr std::array<JavaArchive, 16> kJavaArchives{{
    {.example = "example-3d-printable-nose-cone-and-fins",
     .entries = "rocket.ork\n"
                "decals/dont-print-sticker.png\n"
                "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse\n"
                "thrustcurves/74472299ac7ffc451f7b434d6c81b89c.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"},
    {.example = "example-a-simple-model-rocket",
     .entries = "rocket.ork\n"
                "decals/BodyStripe.png\n"
                "decals/TailStripe.png\n"
                "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse\n"
                "thrustcurves/be11a726b56813c4b1aea0574c8302b2.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"},
    {.example = "example-airstart-timing",
     .entries = "rocket.ork\n"
                "decals/patriot.png\n"
                "thrustcurves/9e8c531f225566c6c75eedaeacfcfef1.rse\n"
                "thrustcurves/8a203416fee4400cdaf09edf369235af.rse\n"},
    {.example = "example-arc-payload-rocket",
     .entries = "rocket.ork\n"
                "decals/spiral-wound-alpha.png\n"
                "thrustcurves/88ed07b96422ec99767fb35bf6a51966.rse\n"},
    {.example = "example-chute-release",
     .entries = "rocket.ork\n"
                "decals/BodyStripe.png\n"
                "decals/TailStripe.png\n"
                "thrustcurves/7c6080928783078289d9a473efecc134.rse\n"
                "thrustcurves/6438359c302fd7c031af855f17d8d27e.rse\n"},
    {.example = "example-clustered-motors",
     .entries = "rocket.ork\n"
                "decals/grad.png\n"
                "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse\n"
                "thrustcurves/be11a726b56813c4b1aea0574c8302b2.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"},
    {.example = "example-deployable-payload",
     .entries = "rocket.ork\n"
                "decals/BodyStripe.png\n"
                "decals/TailStripe.png\n"
                "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse\n"
                "thrustcurves/be11a726b56813c4b1aea0574c8302b2.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"},
    {.example = "example-dual-parachute-deployment",
     .entries = "rocket.ork\n"
                "thrustcurves/26a5e7834018943090396d419ca64662.rse\n"
                "thrustcurves/cff44a78b2c4bc9e2ef19f6a2ded4da0.rse\n"
                "thrustcurves/6eb7fe0c078fe0365e2052d8c877f6a4.rse\n"
                "thrustcurves/871f7fe309f61ec7e45e4b29833349d9.rse\n"
                "thrustcurves/e26c7159754a0c58db92d8c5b3568902.rse\n"
                "thrustcurves/0fa1ede88d6407eadc5bbc413810100d.rse\n"},
    {.example = "example-parallel-booster-staging",
     .entries = "rocket.ork\n"
                "/datafiles/textures/balsa.jpg\n"
                "/datafiles/textures/chute.jpg\n"
                "/datafiles/textures/hardboard.jpg\n"
                "/datafiles/textures/spiral-wound-alpha.png\n"
                "/datafiles/textures/wadding.png\n"
                "/datafiles/textures/wood.jpg\n"
                "thrustcurves/0039ed088e61360d934d9bd8503fad92.rse\n"
                "thrustcurves/7baff5a049a60829dbb8316d1b183e86.rse\n"},
    {.example = "example-pods-airframes-and-winglets",
     .entries = "rocket.ork\n"
                "thrustcurves/bd060845629e4cfceec7e9b19297ab9f.rse\n"
                "thrustcurves/74472299ac7ffc451f7b434d6c81b89c.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"
                "thrustcurves/a47e144089f502e95b8a16130db9b416.rse\n"
                "thrustcurves/597863e31e02f245b23d2bfd550f615e.rse\n"},
    {.example = "example-pods-powered-with-recovery-deployment",
     .entries = "rocket.ork\n"
                "thrustcurves/e5b53def203dd437ebf0d67846f6cd3b.rse\n"
                "thrustcurves/bd060845629e4cfceec7e9b19297ab9f.rse\n"},
    {.example = "example-simulation-extensions",
     .entries = "rocket.ork\n"
                "decals/skunk.jpg\n"
                "decals/sticker.gif\n"
                "thrustcurves/5498ead583ab6cd6900a533b1cb69df8.rse\n"},
    {.example = "example-simulation-scripting",
     .entries = "rocket.ork\n"
                "decals/skunk.jpg\n"
                "decals/sticker.gif\n"
                "thrustcurves/5498ead583ab6cd6900a533b1cb69df8.rse\n"},
    {.example = "example-three-stage-low-power-rocket",
     .entries = "rocket.ork\n"
                "decals/gStripe.png\n"
                "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse\n"
                "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse\n"
                "thrustcurves/74472299ac7ffc451f7b434d6c81b89c.rse\n"},
    {.example = "example-tube-fin-rocket",
     .entries = "rocket.ork\n"
                "decals/Background.00.01.png\n"
                "decals/Background.00.02.png\n"
                "thrustcurves/c26987c1c7e95810bbb6f2e284861494.rse\n"},
    {.example = "example-two-stage-high-power-rocket",
     .entries = "rocket.ork\n"
                "thrustcurves/a06234b6049c1079e394cb9ecd4607a9.rse\n"
                "thrustcurves/a0006978c9a542518b425c0caa67042b.rse\n"
                "thrustcurves/1a3327383625336706131b2a9d198139.rse\n"},
}};

constexpr std::array<EmbeddedCurve, 26> kEmbeddedCurves{{
    {.entry  = "thrustcurves/0039ed088e61360d934d9bd8503fad92.rse",
     .sha256 = "05cc3be07128a737f996563e550e1f11f184f5d6ef02293be1eb928ac10a55d7"},
    {.entry  = "thrustcurves/0fa1ede88d6407eadc5bbc413810100d.rse",
     .sha256 = "bb59f4f07df23829b3b0ccbddb5805de28fcf0492b63d29706c8fc3bbd5fc084"},
    {.entry  = "thrustcurves/1a3327383625336706131b2a9d198139.rse",
     .sha256 = "1331cab3cbd7c791d1fea6b74d2dde9133a68f716ff38ab0b7b3949e42ceeda0"},
    {.entry  = "thrustcurves/26a5e7834018943090396d419ca64662.rse",
     .sha256 = "bab4132b683b0c472b557d922285b901347757ca36890bcd5e66d5e3e5eb4cee"},
    {.entry  = "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse",
     .sha256 = "fb8905d12726004d7f87788ef09b0f6da3ea3e62f4811092dcff5081efb81bc1"},
    {.entry  = "thrustcurves/5498ead583ab6cd6900a533b1cb69df8.rse",
     .sha256 = "3d2ae0dba8886208619accec22b7a374c27b2895ea4f7f7c57d386e90538b457"},
    {.entry  = "thrustcurves/597863e31e02f245b23d2bfd550f615e.rse",
     .sha256 = "52944ff4a6d73d13807dde018092eb7b57d5bd2ff115754f75c5304ef1bfaeaa"},
    {.entry  = "thrustcurves/6438359c302fd7c031af855f17d8d27e.rse",
     .sha256 = "cc3c6e7ed6f23ff92aad1787120b029c63383fe591acb343bf71fc1045c99a8b"},
    {.entry  = "thrustcurves/6eb7fe0c078fe0365e2052d8c877f6a4.rse",
     .sha256 = "b7f6a3464c01146d8fa0a2e8fcb172dff13bd7e94a98977a0269ae1396cb5a35"},
    {.entry  = "thrustcurves/74472299ac7ffc451f7b434d6c81b89c.rse",
     .sha256 = "d366dcde730b4cad8e79065919c96b8d18a3b4f4532504e0fcc3584dac09e354"},
    {.entry  = "thrustcurves/7baff5a049a60829dbb8316d1b183e86.rse",
     .sha256 = "3d700d2fa8bc9ecda7b444ba937ab4c289bd7ee86f4c95dc415617dfdce7185f"},
    {.entry  = "thrustcurves/7c6080928783078289d9a473efecc134.rse",
     .sha256 = "ab3457bd2937445da8d9347698506ac04abf681989acc36af96d171f02d03182"},
    {.entry  = "thrustcurves/871f7fe309f61ec7e45e4b29833349d9.rse",
     .sha256 = "bd94166c6576b637015b15e02a8b1584307c44e0be3bce11147aac8d4ca3e988"},
    {.entry  = "thrustcurves/88ed07b96422ec99767fb35bf6a51966.rse",
     .sha256 = "9fab6023cc067958e4af2ae7317859e44ee848e8916123bbb8350fbf7221aa88"},
    {.entry  = "thrustcurves/8a203416fee4400cdaf09edf369235af.rse",
     .sha256 = "3f7af18d4af15820bca0ea2d0cc726f51b38d63f5cf6376332e35f2bd28f0e2c"},
    {.entry  = "thrustcurves/9e8c531f225566c6c75eedaeacfcfef1.rse",
     .sha256 = "61610bc5c87bdb4824fe1ec49043f0ce35c34e820d5e5290ddd56cd43928146d"},
    {.entry  = "thrustcurves/a0006978c9a542518b425c0caa67042b.rse",
     .sha256 = "403c285441e6026fd574e7c0b822575a9024ff2f92751d5203e23b1ea21f32ba"},
    {.entry  = "thrustcurves/a06234b6049c1079e394cb9ecd4607a9.rse",
     .sha256 = "6e07da23264096bb7ad4f6a2fb28156840574a01453093fe81e57926d5f2b72a"},
    {.entry  = "thrustcurves/a47e144089f502e95b8a16130db9b416.rse",
     .sha256 = "d30bd750b16495bbc19d0f383ff8b8d9057c3800dea1c11d3159be1fe8d1a880"},
    {.entry  = "thrustcurves/bd060845629e4cfceec7e9b19297ab9f.rse",
     .sha256 = "80ee7098c2eabed3f6b48b997a90969a9a5bc3e93546c57557cc7cd8ca9a8d69"},
    {.entry  = "thrustcurves/be11a726b56813c4b1aea0574c8302b2.rse",
     .sha256 = "9813edcc6ee48beae173a1437ecea967d0f23242ae60329bfbfa49ca32161590"},
    {.entry  = "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse",
     .sha256 = "01dab7641eddf5452b84301d68648ba7974dbb8f7aa01b4654355844f7319c13"},
    {.entry  = "thrustcurves/c26987c1c7e95810bbb6f2e284861494.rse",
     .sha256 = "bfbd921aa5b545f2efef5be69856e2f5dfe25148cb909916bd40a296d84afbe9"},
    {.entry  = "thrustcurves/cff44a78b2c4bc9e2ef19f6a2ded4da0.rse",
     .sha256 = "e7825f6f5811ec6f5e6ac03c7caf75057fac9cd119d0e7c759f0b58fa1f6ee64"},
    {.entry  = "thrustcurves/e26c7159754a0c58db92d8c5b3568902.rse",
     .sha256 = "db595c8e1b80be9a58d9a23221b659c11338c82cdfb47c7e8159705963ef44c2"},
    {.entry  = "thrustcurves/e5b53def203dd437ebf0d67846f6cd3b.rse",
     .sha256 = "51aa1b8ded55083e45a4d0db3d6d4179d6deff10c756f4b4b91d74b1aea50505"},
}};

/// The entries of OpenRocket's archive of @p example (kJavaArchives), or "(none)".
[[nodiscard]] std::string_view javaArchiveOf(std::string_view example)
{
    for (const JavaArchive& archive : kJavaArchives)
    {
        if (archive.example == example)
        {
            return archive.entries;
        }
    }
    return "(none)";
}

/// The SHA-256 digest of OpenRocket's entry @p entry (kEmbeddedCurves), or "(none)".
[[nodiscard]] std::string_view javaCurveDigestOf(std::string_view entry)
{
    for (const EmbeddedCurve& curve : kEmbeddedCurves)
    {
        if (curve.entry == entry)
        {
            return curve.sha256;
        }
    }
    return "(none)";
}

/// The digest of OpenRocket's save with data of the example in @p file (kJavaSavesWithData), or
/// "(none)".
[[nodiscard]] std::string_view javaSaveDigestOf(std::string_view file)
{
    for (const JavaSaveWithData& save : kJavaSavesWithData)
    {
        if (save.file == file)
        {
            return save.sha256;
        }
    }
    return "(none)";
}

/// The pinned estimate of the example in @p file, or -1.
[[nodiscard]] std::int64_t estimatedSizeOf(std::string_view file)
{
    for (const EstimatedSize& size : kEstimatedSizes)
    {
        if (size.file == file)
        {
            return size.bytes;
        }
    }
    return -1;
}

/// Saves @p document, with its simulation data when @p withData, with @p preferences; the
/// warnings of the save go to @p warnings. The text, or the Error's text.
[[nodiscard]] std::string saveOf(OpenRocketDocument& document, const Preferences& preferences,
                                 WarningSet& warnings, bool withData)
{
    StorageOptions options;
    options.setSaveSimulationData(withData);
    ErrorSet                  errors;
    const Result<std::string> saved =
        OpenRocketSaver::save(document, options, preferences, warnings, errors);
    if (!saved)
    {
        return "ERROR " + saved.error().toString();
    }
    return *saved;
}

/// Saves @p document without simulation data with @p preferences, as the harness wrote the
/// re-saves (saveOf()).
[[nodiscard]] std::string saveWithoutData(OpenRocketDocument& document,
                                          const Preferences& preferences, WarningSet& warnings)
{
    return saveOf(document, preferences, warnings, false);
}

/// What a set of saves compared with their re-saves gave.
struct SavesCompared
{
    int         designs{0};      ///< the designs saved and compared
    std::size_t lines{0};        ///< the lines of their saves
    std::size_t byValue{0};      ///< the lines of the same numbers written otherwise
    std::size_t components{0};   ///< the components of the saves (the <id> lines of the rockets)
    std::size_t presets{0};      ///< the <preset> lines of the saves
    std::size_t motors{0};       ///< the <motor> elements of the saves
    std::size_t simulations{0};  ///< the <simulation> elements of the saves
    std::size_t warnings{0};     ///< the stored warnings of the saves
    std::string problems;        ///< what differs, by design: empty when nothing does
};

/// Whether @p line is an <id> element, with any indentation.
[[nodiscard]] bool isIdLine(std::string_view line)
{
    const std::size_t start = line.find_first_not_of(' ');
    return start != std::string_view::npos && line.substr(start).starts_with("<id>");
}

/// Counts the components, presets and motors of the rocket of @p saved and its simulations and
/// stored warnings into @p compared.
void countParts(SavesCompared& compared, std::string_view saved)
{
    for (const std::string_view line : lineViewsOf(rocketElementOf(saved)))
    {
        compared.components += isIdLine(line) ? 1U : 0U;
        compared.presets += line.contains("<preset ") ? 1U : 0U;
        compared.motors += line.contains("<motor configid=") ? 1U : 0U;
    }
    for (const std::string_view line : lineViewsOf(saved))
    {
        compared.simulations += line.contains("<simulation status=") ? 1U : 0U;
        compared.warnings += line.contains("<warning type=") ? 1U : 0U;
    }
}

/// Compares @p saved, a save of QtRocket, with @p resave, the whole text (compareOrkTexts()), and
/// counts the save, its lines and its parts into @p compared.
OrkTextComparison countedComparison(SavesCompared& compared, std::string_view saved,
                                    std::string_view resave)
{
    const OrkTextComparison comparison = compareOrkTexts(saved, resave);
    compared.designs++;
    compared.lines += comparison.lines;
    compared.byValue += comparison.byValue;
    countParts(compared, saved);
    return comparison;
}

/// Compares @p saved, QtRocket's save of the design @p name, with @p resave, the whole text, and
/// adds the result to @p compared; a difference is a problem unless it is @p expected
/// (changedLines() from the re-save to the save).
void compareSaves(SavesCompared& compared, std::string_view name, std::string_view saved,
                  std::string_view resave, std::string_view expected = "")
{
    const OrkTextComparison comparison = countedComparison(compared, saved, resave);
    if (saved.empty() || resave.empty())
    {
        compared.problems += std::format("{}: nothing to compare\n", name);
        return;
    }
    if (expected.empty() ? comparison.differing != 0
                         : changedLines(withoutCreator(resave), withoutCreator(saved)) != expected)
    {
        compared.problems +=
            std::format("{}: the save differs from the re-save\n{}", name, comparison.differences);
    }
}

/// The lines in which OpenRocket's saves 2, 3 and 4 of one loaded document differ from the save
/// before, by example (changedLines()): the dual parachute deployment's (RepeatSaveProbe); no
/// other example's change.
struct RepeatedSaves
{
    std::string_view example;
    std::string_view second;
    std::string_view third;
};

constexpr RepeatedSaves kDualParachuteSaves{
    .example = "example-dual-parachute-deployment",
    .second =
        "<radius>auto 0.025</radius> -> <radius>auto 0.028321</radius>\n"
        "<packedradius>auto 0.024009400000000004</packedradius> -> "
        "<packedradius>auto 0.0273304</packedradius>\n",
    .third =
        "<packedlength>0.11285771168170697</packedlength> -> "
        "<packedlength>0.08709673571783902</packedlength>\n"};

/// Saves @p document three more times after its first save @p first and checks the whole texts
/// against kDualParachuteSaves (nothing changes for any other example). Returns the problems.
[[nodiscard]] std::string repeatedSaveProblems(const GoldenExample& example,
                                               OpenRocketDocument&  document,
                                               const std::string&   first)
{
    const Preferences& preferences = goldenExampleEnvironment().preferences();
    const bool         dual        = example.name == kDualParachuteSaves.example;
    std::string        problems;
    std::string        previous = first;
    for (int save = 2; save <= 4; ++save)
    {
        WarningSet       warnings;
        std::string      text = saveWithoutData(document, preferences, warnings);
        std::string_view expected;
        if (dual && save == 2)
        {
            expected = kDualParachuteSaves.second;
        }
        else if (dual && save == 3)
        {
            expected = kDualParachuteSaves.third;
        }
        const std::string changes = changedLines(previous, text);
        if (changes != expected)
        {
            problems += std::format("{}: save {} changes\n{}where OpenRocket's changes\n{}",
                                    example.name, save, changes, expected);
        }
        previous = std::move(text);
    }
    return problems;
}

/// The line that begins a stored warning of the type RecoveryHighSpeedDeployment, without its
/// indentation.
constexpr std::string_view kHighSpeedWarning = "<warning type=\"RecoveryHighSpeedDeployment\">";

/// The stored warnings of the type RecoveryHighSpeedDeployment (what the saver writes, Java's
/// class name) in @p text. The loader reads that type back as well as HighSpeedDeployment (the
/// user's decision E2 of 2026-10-14), so a round trip keeps them.
[[nodiscard]] std::size_t highSpeedWarningsIn(std::string_view text)
{
    std::size_t count = 0;
    for (const std::string_view line : lineViewsOf(text))
    {
        if (QtRocket::Strings::trim(line) == kHighSpeedWarning)
        {
            ++count;
        }
    }
    return count;
}

/// @p text without its <rocket> element (rocketElementOf()).
[[nodiscard]] std::string withoutRocket(std::string_view text)
{
    const std::string rocket = rocketElementOf(text);
    std::string       result(text);
    const std::size_t at = result.find(rocket);
    if (!rocket.empty() && at != std::string::npos)
    {
        result.erase(at, rocket.size());
    }
    return result;
}

/// What the round trips of the examples' flight data gave.
struct DataRoundTrips
{
    std::size_t simulations{0};  ///< the simulations whose flight data were compared
    std::size_t branches{0};     ///< their branches
    /// The stored warnings of the type RecoveryHighSpeedDeployment in the saves compared
    /// (highSpeedWarningsIn()).
    std::size_t highSpeedWarnings{0};
    std::size_t javaSaves{0};  ///< the saves with data compared with OpenRocket's (digests)
    /// The saves with data compared with OpenRocket's re-saves with data, the whole text
    /// (dataResaveProblem()), and their lines.
    std::size_t dataResaves{0};
    std::size_t dataResaveLines{0};
    std::size_t archives{0};  ///< the .ork archives compared with OpenRocket's
    std::size_t decals{0};    ///< their decal images
    std::size_t curves{0};    ///< their thrust curves
    std::string problems;     ///< what differs, by example: empty when nothing does
};

/// What differs between @p entry, of QtRocket's archive of an example, and OpenRocket's:
/// rocket.ork is @p first, OpenRocketSaver's text; a thrust curve is OpenRocket's text
/// (kEmbeddedCurves); a decal image has the bytes of the entry of its name in @p original, the
/// archive OpenRocket ships. Counts the decals and the curves into @p trips.
[[nodiscard]] std::string entryProblem(const ZipArchive::Entry& entry, const ZipArchive& original,
                                       std::string_view first, DataRoundTrips& trips)
{
    if (entry.name == "rocket.ork")
    {
        return QtRocket::bytesToString(entry.data) == first
                   ? ""
                   : "rocket.ork is not OpenRocketSaver's text with the default options\n";
    }
    if (entry.name.starts_with("thrustcurves/"))
    {
        trips.curves++;
        return QtRocket::sha256Hex(entry.data) == javaCurveDigestOf(entry.name)
                   ? ""
                   : std::format("{} is not OpenRocket's text\n", entry.name);
    }
    trips.decals++;
    const std::vector<std::byte>* const image = original.find(entry.name);
    return image != nullptr && *image == entry.data
               ? ""
               : std::format("{} is not the image of the original archive\n", entry.name);
}

/// What differs between @p bytes, QtRocket's archive of @p example, and OpenRocket's (the names
/// and the order of the entries, kJavaArchives, and each entry, entryProblem()); counted into
/// @p trips.
[[nodiscard]] std::string archiveDifferences(const GoldenExample&          example,
                                             const std::vector<std::byte>& bytes,
                                             std::string_view first, DataRoundTrips& trips)
{
    const Result<ZipArchive>             archive       = ZipArchive::fromBytes(bytes);
    const Result<std::vector<std::byte>> originalBytes = QtRocket::readFile(example.original);
    if (!archive || !originalBytes)
    {
        return std::format("{}: an archive cannot be read\n", example.name);
    }
    const Result<ZipArchive> original = ZipArchive::fromBytes(*originalBytes);
    if (!original)
    {
        return std::format("{}: the original archive cannot be read\n", example.name);
    }
    trips.archives++;
    std::string problems;
    std::string names;
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        names += entry.name + "\n";
        problems += entryProblem(entry, *original, first, trips);
    }
    if (names != javaArchiveOf(example.name))
    {
        problems += std::format("the entries\n{}where OpenRocket's are\n{}", names,
                                javaArchiveOf(example.name));
    }
    return problems.empty()
               ? ""
               : std::format("{}: the archive is not OpenRocket's\n{}", example.name, problems);
}

/// Saves @p document, an example, as an .ork archive with its default storage options, which
/// save the simulation data of every example (GeneralRocketSaver), right after @p first, its
/// save with the data, and compares the archive with OpenRocket's (archiveDifferences()). The
/// archive, or nothing; the problems go to @p trips.
[[nodiscard]] std::vector<std::byte> archiveOf(const GoldenExample& example,
                                               OpenRocketDocument& document, std::string_view first,
                                               DataRoundTrips& trips)
{
    GeneralRocketSaver             saver(goldenExampleEnvironment().preferences());
    Result<std::vector<std::byte>> bytes =
        saver.saveToBytes(document, document.getDefaultStorageOptions());
    if (!bytes)
    {
        trips.problems +=
            std::format("{}: no archive: {}\n", example.name, bytes.error().toString());
        return {};
    }
    if (!document.getDefaultStorageOptions().getSaveSimulationData() ||
        !saver.getWarnings().empty())
    {
        trips.problems += std::format(
            "{}: the default options save no simulation data, or the archive gave {} warnings\n",
            example.name, saver.getWarnings().size());
    }
    trips.problems += archiveDifferences(example, *bytes, first, trips);
    return std::move(*bytes);
}

/// The flight data of the simulations of @p document against those of @p reloaded, the same
/// document saved and loaded back: what differs (flightDifferences()), counted into @p trips.
[[nodiscard]] std::string flightProblems(const OpenRocketDocument& document,
                                         const OpenRocketDocument& reloaded, DataRoundTrips& trips)
{
    if (document.getSimulationCount() != reloaded.getSimulationCount())
    {
        return std::format("{} simulations back, not {}\n", reloaded.getSimulationCount(),
                           document.getSimulationCount());
    }
    std::string problems;
    for (std::size_t i = 0; i < document.getSimulationCount(); ++i)
    {
        const std::shared_ptr<FlightData>& ours = document.getSimulation(i)->getSimulatedData();
        const std::shared_ptr<FlightData>& back = reloaded.getSimulation(i)->getSimulatedData();
        if (ours == nullptr || back == nullptr)
        {
            problems += ours == back ? "" : std::format("simulation {}: flight data lost\n", i);
            continue;
        }
        trips.simulations++;
        trips.branches += ours->getBranchCount();
        const std::string differences = flightDifferences(*ours, *back);
        if (!differences.empty())
        {
            problems += std::format("simulation {}:\n{}", i, differences);
        }
    }
    return problems;
}

/// What differs between @p first, QtRocket's save with data of @p example, and OpenRocket's re-save
/// with data of the example when the goldens have one (two examples: the manifest's
/// "resaves.withData"), the whole text but for the creator; counted into @p trips. That re-save is
/// OpenRocket's first save of a load of the example; @p first follows four saves without data,
/// which change nothing in an example that is not stale as loaded (kDualParachuteSaves), as
/// neither of the two is.
[[nodiscard]] std::string dataResaveProblem(const GoldenExample& example, std::string_view first,
                                            DataRoundTrips& trips)
{
    const QtRocket::Test::ResaveWithData* const data =
        QtRocket::Test::resaveWithDataOf(example.name);
    if (data == nullptr)
    {
        return {};
    }
    const std::string theirs =
        QtRocket::Test::documentOfDesignFile(QtRocket::Test::goldensDir() / data->resave);
    const OrkTextComparison comparison = compareOrkTexts(first, theirs);
    trips.dataResaves++;
    trips.dataResaveLines += comparison.lines;
    return comparison.differing == 0 && comparison.byValue == 0
               ? ""
               : std::format("{}: the save with data is not OpenRocket's re-save with data\n{}",
                             example.name, comparison.differences);
}

/// Saves @p document, an example, with its simulation data and compares that text outside the
/// rocket with OpenRocket's own (kJavaSavesWithData), and the whole text with OpenRocket's
/// re-save with data where there is one (dataResaveProblem()), then as an .ork archive
/// (archiveOf(), which compares the archive with OpenRocket's), loads the archive back from memory
/// without a directory (decision O5 of tier 10), with no warning, and compares the flight data and
/// the save of the reload with the first save, all but the rocket: the first save of a load writes
/// the automatic dimensions the dual parachute deployment's first save writes
/// (kDualParachuteSaves), not those of a design saved before. Adds the result to @p trips.
void roundTripTheData(const GoldenExample& example, OpenRocketDocument& document,
                      DataRoundTrips& trips)
{
    const Preferences& preferences = goldenExampleEnvironment().preferences();
    WarningSet         warnings;
    const std::string  first   = saveOf(document, preferences, warnings, true);
    const std::string  outside = withoutRocket(withoutCreator(first));
    if (QtRocket::sha256Hex(std::as_bytes(std::span(outside))) != javaSaveDigestOf(example.file))
    {
        trips.problems += std::format(
            "{}: the save with data is not OpenRocket's outside the rocket (kJavaSavesWithData)\n",
            example.name);
    }
    trips.javaSaves++;
    trips.problems += dataResaveProblem(example, first, trips);
    const std::vector<std::byte> archive = archiveOf(example, document, first, trips);
    const GeneralRocketLoader    loader(goldenExampleEnvironment().context());
    const Result<LoadedDocument> loaded = loader.load(archive);
    if (!loaded)
    {
        trips.problems += std::format("{}: {}\n", example.name, loaded.error().toString());
        return;
    }
    if (!loaded->warnings.empty())
    {
        trips.problems += std::format("{}: {} warnings when the archive is loaded\n", example.name,
                                      loaded->warnings.size());
    }
    const std::string flights = flightProblems(document, *loaded->document, trips);
    if (!flights.empty())
    {
        trips.problems += std::format("{}: the flight data differ\n{}", example.name, flights);
    }
    const std::string second =
        withoutRocket(saveOf(*loaded->document, preferences, warnings, true));
    const std::string expected = withoutRocket(first);
    trips.highSpeedWarnings += highSpeedWarningsIn(expected);
    if (second != expected)
    {
        trips.problems += std::format("{}: the second save with data differs from the first\n{}",
                                      example.name, compareOrkTexts(second, expected).differences);
    }
    if (!warnings.empty())
    {
        trips.problems += std::format("{}: {} save warnings\n", example.name, warnings.size());
    }
}

/// The estimate of @p document, an example, with options that save no simulation data: 80 bytes a
/// component, the rocket included, and 100 a simulation (Java's estimateFileSize() counts the data
/// points only when the options save them). What differs, or "".
[[nodiscard]] std::string estimateWithoutDataProblem(const GoldenExample&      example,
                                                     const OpenRocketDocument& document)
{
    std::int64_t components = 0;
    for ([[maybe_unused]] const QtRocket::RocketComponent& component :
         document.getRocket().subtree(true))
    {
        ++components;
    }
    const std::int64_t expected = (OpenRocketSaver::kBytesPerComponentCompressed * components) +
                                  (OpenRocketSaver::kBytesPerSimulationCompressed *
                                   static_cast<std::int64_t>(document.getSimulationCount()));
    StorageOptions     withoutData;
    withoutData.setSaveSimulationData(false);
    const std::int64_t estimate = OpenRocketSaver::estimateFileSize(document, withoutData);
    return estimate == expected
               ? ""
               : std::format("{}: estimateFileSize without data {} where it is {}\n", example.name,
                             estimate, expected);
}

/// Loads @p example fresh, pins its estimate, saves it and compares the whole text with the
/// re-save, then saves it three times more (repeatedSaveProblems()) and once with its data
/// (roundTripTheData()). Adds the comparisons to @p saves and @p trips; returns the other
/// problems.
[[nodiscard]] std::string exampleProblems(const GoldenExample& example, SavesCompared& saves,
                                          DataRoundTrips& trips)
{
    const Result<LoadedDocument> loaded = loadGoldenExample(example);
    if (!loaded)
    {
        return std::format("{}: {}\n", example.name, loaded.error().toString());
    }
    std::string         problems;
    OpenRocketDocument& document = *loaded->document;

    const std::int64_t estimate =
        OpenRocketSaver::estimateFileSize(document, document.getDefaultStorageOptions());
    if (estimate != estimatedSizeOf(example.file))
    {
        problems += std::format("{}: estimateFileSize {} where OpenRocket's is {}\n", example.name,
                                estimate, estimatedSizeOf(example.file));
    }
    problems += estimateWithoutDataProblem(example, document);

    WarningSet        warnings;
    const std::string saved =
        saveWithoutData(document, goldenExampleEnvironment().preferences(), warnings);
    const Result<std::string> resave = QtRocket::readTextFile(example.resave);
    if (!resave)
    {
        return problems + std::format("{}: no re-save to compare\n", example.name);
    }
    if (!warnings.empty())
    {
        problems += std::format("{}: {} save warnings\n", example.name, warnings.size());
    }
    compareSaves(saves, example.name, saved, *resave);
    problems += repeatedSaveProblems(example, document, saved);
    roundTripTheData(example, document, trips);
    return problems;
}

/// Saves each of the sixteen examples (exampleProblems()): the problems besides those of the
/// comparisons, which go to @p saves and @p trips.
[[nodiscard]] std::string saveTheExamples(SavesCompared& saves, DataRoundTrips& trips)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        problems += exampleProblems(example, saves, trips);
    }
    return problems;
}

// Each of the sixteen examples is saved as OpenRocket re-saved it, every one of the 10000 lines:
// the declaration, the root of version 1.11, the rocket (327 components, 65 motors and the six
// presets), the 54 simulations with their conditions, extensions, summary values and 20 stored
// warnings, no custom expression, the photo studio (empty in fourteen and 24 settings in two), no
// document preference and the document materials of each. Not one number is written otherwise
// than OpenRocket writes it. With it: OpenRocket's estimate of each file's size, with and without
// the simulation data; the saves of one loaded document that follow the first change as
// OpenRocket's do; the saves with data are OpenRocket's outside the rocket element (the digests of
// kJavaSavesWithData: the stored flights, events and warnings as Java writes them); the sixteen
// .ork archives are OpenRocket's, entry for entry (23 decal images, 41 thrust curves); and loaded
// back from them, the flights of the 54 simulations (69 branches) come back bit for bit, and a
// second save is the first, the sixteen warnings of decision E2 included. The saves with data of
// the two examples whose re-saves with data the goldens hold are those re-saves, all 5752 lines.
TEST(OrkResaveDiffGolden, TheExamplesAreSavedAsOpenRocketResavedThem)
{
    SavesCompared  saves;
    DataRoundTrips trips;
    EXPECT_EQ(saveTheExamples(saves, trips), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 16);
    EXPECT_EQ(saves.lines, 10000U);  // the lines of the sixteen re-saves, as it happens
    EXPECT_EQ(saves.components, 327U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.presets, 6U);
    EXPECT_EQ(saves.motors, 65U);
    EXPECT_EQ(saves.simulations, 54U);
    EXPECT_EQ(saves.warnings, 20U);
    EXPECT_EQ(goldenExamples().size(), kEstimatedSizes.size());

    EXPECT_EQ(trips.problems, "");
    EXPECT_EQ(trips.javaSaves, 16U);
    EXPECT_EQ(trips.dataResaves, 2U);
    EXPECT_EQ(trips.dataResaveLines, 5752U);
    EXPECT_EQ(trips.simulations, 54U);
    EXPECT_EQ(trips.branches, 69U);
    EXPECT_EQ(trips.archives, 16U);
    EXPECT_EQ(trips.decals, 23U);
    EXPECT_EQ(trips.curves, 41U);
    // Decision E2 of 2026-10-14: the loader reads the type RecoveryHighSpeedDeployment back, so
    // these warnings survive the round trip above.
    EXPECT_EQ(trips.highSpeedWarnings, 16U);
}

/// The beginnings of the document materials whose order the presets change.
constexpr std::string_view kSpiralKraft =
    "<material>BULK|Paper, spiral kraft glassine, Estes avg, bulk|894.4|0.0|";
constexpr std::string_view kCarpetThread = "<material>LINE|Carpet Thread|3.3E-4|0.0|";
constexpr std::string_view kHdpeFilm =
    "<material>SURFACE|Polyethylene film, HDPE, 1.0 mil, bare|0.0235|0.0|";

/// changedLines() of two document materials @p prefix ... that changed places: the one of the
/// group Other and the one of the group Custom.
[[nodiscard]] std::string swappedMaterials(std::string_view prefix)
{
    return std::format(
        "{0}Other</material> -> {0}Custom</material>\n"
        "{0}Custom</material> -> {0}Other</material>\n",
        prefix);
}

/// The lines in which the save of an example loaded without presets differs from OpenRocket's
/// re-save, which was made with the presets (changedLines() from the re-save to the save): the
/// order of two document materials of one name and density, one with the group of the file
/// (Custom) and one with that of a preset (Other). OpenRocket's own save of the same loads
/// without presets has QtRocket's order (the probe MaterialsProbe of tier 10a S3, Boot2 "none"):
/// Java's Database puts a material before one that compares equal (name, then density), so two
/// such materials stand in the order the load added them, which the presets change.
[[nodiscard]] std::string withoutPresetsChanges(std::string_view name)
{
    if (name == "example-3d-printable-nose-cone-and-fins")
    {
        return swappedMaterials(kSpiralKraft);
    }
    if (name == "example-deployable-payload")
    {
        return swappedMaterials(kCarpetThread) + swappedMaterials(kHdpeFilm);
    }
    return {};
}

/// Saves the examples whose re-save holds a <preset>, loaded without presets, into @p saves;
/// counts the others in @p withoutPresets. Returns what could not be loaded or read.
[[nodiscard]] std::string saveTheExamplesWithoutPresets(SavesCompared& saves, int& withoutPresets)
{
    std::string problems;
    for (const GoldenExample& example : goldenExamples())
    {
        const Result<std::string> resave = QtRocket::readTextFile(example.resave);
        if (!resave)
        {
            problems += std::format("{}: no re-save\n", example.name);
            continue;
        }
        if (!resave->contains("<preset "))
        {
            withoutPresets++;
            continue;
        }
        const Result<LoadedDocument> loaded =
            loadGoldenExample(example, ExampleSource::ORIGINAL, Presets::NONE);
        if (!loaded)
        {
            problems += std::format("{}: {}\n", example.name, loaded.error().toString());
            continue;
        }
        WarningSet        warnings;
        const std::string saved = saveWithoutData(
            *loaded->document, goldenExampleEnvironment(Presets::NONE).preferences(), warnings);
        compareSaves(saves, example.name, saved, *resave, withoutPresetsChanges(example.name));
    }
    return problems;
}

// The three examples with a <preset> loaded without presets (Presets::NONE: six "No matching
// ComponentPreset" warnings) keep the six references (decision U8) and write them back: the file
// is the re-save's, line for line, but for the order of three pairs of document materials, which
// OpenRocket's own saves without presets have too (withoutPresetsChanges()). The thirteen other
// re-saves hold no <preset>.
TEST(OrkResaveDiffGolden, TheExamplesWithoutPresetsWriteTheirReferencesBack)
{
    SavesCompared saves;
    int           withoutPresets = 0;
    EXPECT_EQ(saveTheExamplesWithoutPresets(saves, withoutPresets), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 3);
    EXPECT_EQ(saves.lines, 1658U);
    EXPECT_EQ(withoutPresets, 13);
    EXPECT_EQ(saves.presets, 6U);
    EXPECT_EQ(saves.byValue, 0U);
}

/// The one line in which QtRocket's save of the loaded re-save of the Alpha III with pods
/// differs from the re-save (changedLines() from the re-save to the save): the wall of the pod
/// body.
constexpr std::string_view kPodBodyWall =
    "<thickness>0.002</thickness> -> <thickness>0.0</thickness>\n";

/// Loads the re-save of each test rocket and saves it again into @p saves. Returns what could
/// not be loaded or read, and the saves that warned.
[[nodiscard]] std::string saveTheTestRockets(SavesCompared& saves)
{
    std::string                    problems;
    const std::vector<GoldenInput> inputs = testRocketInputs(problems);
    for (const GoldenInput& input : inputs)
    {
        const Result<LoadedResave> resave = loadResave(input);
        const Result<std::string>  file =
            QtRocket::readTextFile(QtRocket::Test::goldensDir() / input.resave);
        if (!resave || !file)
        {
            problems += std::format("{}: no load or no re-save\n", input.name);
            continue;
        }
        WarningSet        warnings;
        const std::string saved =
            saveWithoutData(*resave->loaded.document, resave->environment->preferences(), warnings);
        if (!warnings.empty())
        {
            problems += std::format("{}: {} save warnings\n", input.name, warnings.size());
        }
        compareSaves(saves, input.name, saved, *file, input.name == kPods ? kPodBodyWall : "");
    }
    return problems;
}

// The thirteen test rockets, loaded from their re-saves against the seven motors of TestRockets
// and saved again, are their re-saves but for the wall of the pod body of the Alpha III with pods
// (OpenRocket's own reload has no wall there either: ResaveGoldenExceptions): 5090 lines with 151
// components, 48 motors and 50 simulations (a fixed seed, multi-level winds, the WGS84
// computation).
//
// One line of them goes through libm, which decision O11 of tier 10 says never to pin across
// platforms: the <launchroddirection> of the simulation "[C6-5] multi-level wind" of the Estes
// Alpha III (testrocket-estes-alpha-iii, re-save line 564, "90.0"), which launches into a
// multi-level wind, so that its direction is the wind's at time 0 and the launch altitude
// (MultiLevelPinkNoiseWindModel::getWindDirection()): std::sin and std::cos of the level's
// direction pi/2 (the double 1.5707963267948966; standard deviation 0, so no noise is added) and
// std::atan2 of the result. At that point the exact results lie far inside half an ulp of 1.0, of
// 6.123233995736766e-17 and of the same double pi/2 (atan2's exact result differs from it by
// about 1e-33), so every faithful libm (glibc, Apple's, MSVC's UCRT) gives Java's numbers, and the
// line is exact on every platform; a libm that misses one of them by an ulp would turn this test
// red in that one line, which then may be compared by value within 1 ulp (decision O5).
TEST(OrkResaveDiffGolden, TheTestRocketsAreSavedAsOpenRocketResavedThem)
{
    SavesCompared saves;
    EXPECT_EQ(saveTheTestRockets(saves), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 13);
    EXPECT_EQ(saves.lines, 5090U);
    EXPECT_EQ(saves.components, 151U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.presets, 0U);
    EXPECT_EQ(saves.motors, 48U);
    EXPECT_EQ(saves.simulations, 50U);
}

// =========================================================================== the legacy files

/// The lines in which QtRocket's save of a design file of tests/data/ork, loaded as OpenRocket's
/// harness loaded it, differs from OpenRocket's re-save (placedLineChanges() from the re-save to
/// the save, both with the creator set aside and the ids the loaders drew numbered:
/// withDrawnIdsNumbered()): each line with its number, in the re-save for a "-" line and in the
/// save for a "+" line, and its indentation, so that a line written in another place or indented
/// otherwise differs. Every other file is saved as OpenRocket re-saved it.
struct LegacySaveChanges
{
    std::string_view input;
    std::string_view changes;
};

// The references to presets of SEMROC that v1.5-preset-usage.ork names, and a copy of it with
// its decal images, which the six presets of the legacy files' environment do not hold (as
// OpenRocket's test environment does not): OpenRocket's loader warns and forgets them, so its
// re-save has none; QtRocket's warns and keeps them, and writes them back (decision U8 of
// 2026-10-08), each after the <id> of its component: the nose cone, the body tube, two centering
// rings, the inner tube, the launch lug and the parachute.
constexpr std::array<LegacySaveChanges, 4> kLegacySaveChanges{{
    {.input   = "legacy-v1-5-preset-usage",
     .changes = "+ 23:             <preset type=\"NOSE_CONE\" manufacturer=\"SEMROC Astronautics\" "
                "partno=\"BNC-55F\" digest=\"b2ffefd485ea774f3c63322e1b8820b3\"/>\n"
                "+ 41:             <preset type=\"BODY_TUBE\" manufacturer=\"SEMROC Astronautics\" "
                "partno=\"BT-55\" digest=\"3c547f48d153d6bef75cf178357e3fc7\"/>\n"
                "+ 52:                 <preset type=\"CENTERING_RING\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"RA-5055\" digest=\"1dec8a924dc469f50cd54cf2725ae484\"/>\n"
                "+ 68:                 <preset type=\"CENTERING_RING\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"RA-5055\" digest=\"1dec8a924dc469f50cd54cf2725ae484\"/>\n"
                "+ 84:                 <preset type=\"BODY_TUBE\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"BT-50J\" digest=\"4dd60fdb119e05990a4903fc36e67bf9\"/>\n"
                "+ 119:                 <preset type=\"LAUNCH_LUG\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"LL-117\" digest=\"a0af9e35fb430dda0ca561850ce3921e\"/>\n"
                "+ 136:                 <preset type=\"PARACHUTE\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"PN-18\" digest=\"57273aa3aefaad85242e3b591e847671\"/>\n"},
    {.input   = "legacy-v1-6-preset-usage-decals-first",
     .changes = "+ 23:             <preset type=\"NOSE_CONE\" manufacturer=\"SEMROC Astronautics\" "
                "partno=\"BNC-55F\" digest=\"b2ffefd485ea774f3c63322e1b8820b3\"/>\n"
                "+ 46:             <preset type=\"BODY_TUBE\" manufacturer=\"SEMROC Astronautics\" "
                "partno=\"BT-55\" digest=\"3c547f48d153d6bef75cf178357e3fc7\"/>\n"
                "+ 67:                 <preset type=\"CENTERING_RING\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"RA-5055\" digest=\"1dec8a924dc469f50cd54cf2725ae484\"/>\n"
                "+ 88:                 <preset type=\"CENTERING_RING\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"RA-5055\" digest=\"1dec8a924dc469f50cd54cf2725ae484\"/>\n"
                "+ 104:                 <preset type=\"BODY_TUBE\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"BT-50J\" digest=\"4dd60fdb119e05990a4903fc36e67bf9\"/>\n"
                "+ 144:                 <preset type=\"LAUNCH_LUG\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"LL-117\" digest=\"a0af9e35fb430dda0ca561850ce3921e\"/>\n"
                "+ 161:                 <preset type=\"PARACHUTE\" manufacturer=\"SEMROC "
                "Astronautics\" partno=\"PN-18\" digest=\"57273aa3aefaad85242e3b591e847671\"/>\n"},
    // The roll control script of OpenRocket's example, stored enabled in the simulations "Active
    // roll control" and "Roll control + air-start", which OpenRocket trusts by its hash and keeps
    // enabled; QtRocket runs no script and disables every script with code on load (decisions D1
    // and E4), so its save writes them disabled. And the extension of the id
    // net.sf.openrocket.simulation.extension.impl.AirStart of "Roll control + air-start", which no
    // provider knows today: OpenRocket's loader warns and drops it, QtRocket's warns and keeps it
    // with its configuration (decision D11), so its save writes it back in its place, the first
    // extension of that simulation, before the script (the re-save's line 686).
    {.input   = "legacy-v1-7-simulation-extensions-and-scripting",
     .changes = "- 632:         <entry key=\"enabled\" type=\"boolean\">true</entry>\n"
                "+ 632:         <entry key=\"enabled\" type=\"boolean\">false</entry>\n"
                "+ 686:       <extension extensionid=\"info.openrocket.core.simulation.extension."
                "impl.AirStart\">\n"
                "+ 687:         <entry key=\"launchAltitude\" type=\"number\">1000.0</entry>\n"
                "+ 688:       </extension>\n"
                "- 747:         <entry key=\"enabled\" type=\"boolean\">true</entry>\n"
                "+ 750:         <entry key=\"enabled\" type=\"boolean\">false</entry>\n"},
    // A reference to a body tube of FlisKits, as the references of SEMROC above.
    {.input   = "legacy-v1-7-tube-fin",
     .changes = "+ 40:             <preset type=\"BODY_TUBE\" manufacturer=\"FlisKits\" "
                "partno=\"BT-50-18\" digest=\"e64b2b02cb13361924b3d78c0a68911d\"/>\n"},
}};

/// The changes of kLegacySaveChanges for @p input, or none.
[[nodiscard]] std::string_view legacySaveChangesOf(std::string_view input)
{
    for (const LegacySaveChanges& changes : kLegacySaveChanges)
    {
        if (changes.input == input)
        {
            return changes.changes;
        }
    }
    return {};
}

/// What the saves of the legacy files gave, beyond what SavesCompared counts.
struct LegacySaves
{
    int         loadWarnings{0};    ///< the warnings of the loads
    std::size_t differingLines{0};  ///< the lines that differ where they stand (compareOrkTexts())
    std::size_t changedLines{0};    ///< the lines of the changes (kLegacySaveChanges)
};

/// Loads the legacy file of @p input in an environment of its own (that of the harness's legacy
/// files: the bundled motor database, the six presets of the examples, OpenRocket's test
/// preferences), saves it without simulation data and compares the save with OpenRocket's
/// re-save (kLegacySaveChanges). Adds the comparison to @p saves; returns the problems.
[[nodiscard]] std::string legacySaveProblems(const QtRocket::Test::ResaveInput& input,
                                             SavesCompared& saves, LegacySaves& legacy)
{
    const DesignFileEnvironment  environment;
    const std::filesystem::path  file = QtRocket::Test::legacyFileOf(input);
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> loaded = loader.load(file);
    const Result<std::string>    resave =
        QtRocket::readTextFile(QtRocket::Test::goldensDir() / input.resave);
    if (!loaded || !resave)
    {
        return std::format("{}: no load or no re-save\n", input.name);
    }
    legacy.loadWarnings += static_cast<int>(loaded->warnings.size());
    WarningSet        warnings;
    const std::string saved =
        saveWithoutData(*loaded->document, environment.preferences(), warnings);
    const std::string       original   = QtRocket::Test::documentOfDesignFile(file);
    const std::string       ours       = withDrawnIdsNumbered(withoutCreator(saved), original);
    const std::string       theirs     = withDrawnIdsNumbered(withoutCreator(*resave), original);
    const OrkTextComparison comparison = countedComparison(saves, ours, theirs);
    legacy.differingLines += comparison.differing;
    const std::string changes = placedLineChanges(theirs, ours);
    legacy.changedLines += lineViewsOf(changes).size();
    std::string problems;
    if (!warnings.empty())
    {
        problems += std::format("{}: {} save warnings\n", input.name, warnings.size());
    }
    if (changes != legacySaveChangesOf(input.name))
    {
        problems += std::format("{}: the save differs from the re-save in\n{}where it is to in\n{}",
                                input.name, changes, legacySaveChangesOf(input.name));
    }
    return problems;
}

/// legacySaveProblems() of each legacy file of the manifest.
[[nodiscard]] std::string saveTheLegacyFiles(SavesCompared& saves, LegacySaves& legacy)
{
    std::string problems;
    for (const QtRocket::Test::ResaveInput& input :
         QtRocket::Test::resaveInputsOf(QtRocket::Test::kLegacyKind, problems))
    {
        problems += legacySaveProblems(input, saves, legacy);
    }
    return problems;
}

// The eighteen design files of tests/data/ork (OpenRocket 0.9 to 23.09, the file formats 1.0 to
// 1.9), each loaded as the harness loaded it (22 warnings of the loads) and saved without
// simulation data, are OpenRocket's re-saves of them, line for line (8405 lines with 306
// components, 39 motors, 36 simulations and 25 stored warnings; the ids the loaders drew
// numbered), but for the 22 lines of kLegacySaveChanges, each in its place and with its
// indentation: the 15 references to presets QtRocket keeps, the two scripts it disables and the
// extension it keeps.
TEST(OrkResaveDiffGolden, TheLegacyFilesAreSavedAsOpenRocketResavedThem)
{
    SavesCompared saves;
    LegacySaves   legacy;
    EXPECT_EQ(saveTheLegacyFiles(saves, legacy), "");
    EXPECT_EQ(saves.designs, 18);
    EXPECT_EQ(saves.lines, 8405U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.components, 306U);
    EXPECT_EQ(saves.presets, 15U);
    EXPECT_EQ(saves.motors, 39U);
    EXPECT_EQ(saves.simulations, 36U);
    EXPECT_EQ(saves.warnings, 25U);
    EXPECT_EQ(legacy.loadWarnings, 22);
    EXPECT_EQ(legacy.changedLines, 22U);
    // The lines that differ where they stand (compareOrkTexts()): those of the changes and the
    // lines after an added one, which stand a line or more lower than in the re-save.
    EXPECT_EQ(legacy.differingLines, 682U);
}

// ====================================================================== the coverage documents

/// @p resave with the lookup file the harness names, "/lookups/drag.csv" (CoverageDocuments.java),
/// written as a save on this platform writes it. The options keep a lookup file as Java's
/// Path.toAbsolutePath().normalize() makes it, which on Windows puts the current drive in front
/// and the native separators in ("D:\lookups\drag.csv"), in OpenRocket on Windows too; the
/// re-save was made on Linux, where the path stays as it is.
[[nodiscard]] std::string withThisPlatformsLookupFile(std::string resave)
{
    constexpr std::string_view kHarnessFile     = "/lookups/drag.csv";
    const std::string          harnessAttribute = std::format(R"(file="{}")", kHarnessFile);
    const std::string          localAttribute   = std::format(
        R"(file="{}")",
        QtRocket::pathToUtf8(
            std::filesystem::absolute(std::filesystem::path(kHarnessFile)).lexically_normal()));
    for (std::size_t at = resave.find(harnessAttribute); at != std::string::npos;
         at             = resave.find(harnessAttribute, at + localAttribute.size()))
    {
        resave.replace(at, harnessAttribute.size(), localAttribute);
    }
    return resave;
}

/// Loads the re-save of the coverage document @p input in an environment of its own, saves it
/// with or without its simulation data as the harness saved it and compares the save with the
/// re-save, the whole text, its lookup file as this platform writes it
/// (withThisPlatformsLookupFile()). Adds the comparison to @p saves; returns the problems.
[[nodiscard]] std::string coverageSaveProblems(const QtRocket::Test::ResaveInput& input,
                                               SavesCompared&                     saves)
{
    const DesignFileEnvironment  environment;
    const std::filesystem::path  file = QtRocket::Test::goldensDir() / input.resave;
    const GeneralRocketLoader    loader(environment.context());
    const Result<LoadedDocument> loaded = loader.load(file);
    const Result<std::string>    resave = QtRocket::readTextFile(file);
    if (!loaded || !resave)
    {
        return std::format("{}: no load or no re-save\n", input.name);
    }
    WarningSet        warnings;
    const std::string saved =
        saveOf(*loaded->document, environment.preferences(), warnings, input.saveSimulationData);
    compareSaves(saves, input.name, saved, withThisPlatformsLookupFile(*resave));
    if (!loaded->warnings.empty() || !warnings.empty())
    {
        return std::format("{}: {} load warnings, {} save warnings\n", input.name,
                           loaded->warnings.size(), warnings.size());
    }
    return {};
}

/// coverageSaveProblems() of each coverage document of the manifest.
[[nodiscard]] std::string saveTheCoverageDocuments(SavesCompared& saves)
{
    std::string problems;
    for (const QtRocket::Test::ResaveInput& input :
         QtRocket::Test::resaveInputsOf(QtRocket::Test::kCoverageKind, problems))
    {
        problems += coverageSaveProblems(input, saves);
    }
    return problems;
}

// The saver-coverage documents of the harness (CoverageDocuments.java: what no other re-save
// holds, with them every element and attribute name OpenRocket's saver writes), loaded from
// their re-saves without a warning and saved again without one, are their re-saves, every one of
// the 674 lines (20 components, 2 simulations, a stored warning).
TEST(OrkResaveDiffGolden, TheCoverageDocumentsAreSavedAsOpenRocketResavedThem)
{
    SavesCompared saves;
    EXPECT_EQ(saveTheCoverageDocuments(saves), "");
    EXPECT_EQ(saves.problems, "");
    EXPECT_EQ(saves.designs, 2);
    EXPECT_EQ(saves.lines, 674U);
    EXPECT_EQ(saves.byValue, 0U);
    EXPECT_EQ(saves.components, 20U);
    EXPECT_EQ(saves.simulations, 2U);
    EXPECT_EQ(saves.warnings, 1U);
}

/// A rocket element of a few lines, with the root around it.
constexpr std::string_view kRocket =
    "<openrocket version=\"1.11\" creator=\"QtRocket 0.1.0\">\n"
    "  <rocket>\n"
    "    <name>R</name>\n"
    "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n"
    "    <axialoffset method=\"absolute\">0.001</axialoffset>\n"
    "    <designtype>original</designtype>\n"
    "  </rocket>\n"
    "</openrocket>\n";

/// kRocket with the first @p from replaced by @p to.
[[nodiscard]] std::string changed(std::string_view from, std::string_view to)
{
    std::string text(kRocket);
    return text.replace(text.find(from), from.size(), to);
}

// The comparison of the texts: the creator is the only text set aside; a number of the same value
// written otherwise is counted, not reported (none is expected of a save); a changed digit, a
// carriage return, a line missing and two lines in another order are reported in their lines,
// and the line feed missing at the end of a text.
TEST(OrkResaveDiffGolden, TheComparisonReportsEveryChangeButTheCreator)
{
    const OrkTextComparison same =
        compareOrkTexts(kRocket, changed("QtRocket 0.1.0", "OpenRocket 26.xx"));
    EXPECT_EQ(same.lines, 8U);
    EXPECT_EQ(same.differing, 0U);
    EXPECT_EQ(same.byValue, 0U);
    EXPECT_EQ(same.differences, "");

    const OrkTextComparison byValue = compareOrkTexts(kRocket, changed("0.001", "1.0E-3"));
    EXPECT_EQ(byValue.differing, 0U);
    EXPECT_EQ(byValue.byValue, 1U);

    const OrkTextComparison digit = compareOrkTexts(kRocket, changed("0.001", "0.002"));
    EXPECT_EQ(digit.differing, 1U);
    EXPECT_EQ(digit.differences,
              "line 5:\n"
              "  QtRocket:       <axialoffset method=\"absolute\">0.001</axialoffset>\n"
              "  OpenRocket:     <axialoffset method=\"absolute\">0.002</axialoffset>\n");

    const OrkTextComparison cr =
        compareOrkTexts(kRocket, changed("<name>R</name>\n", "<name>R</name>\r\n"));
    EXPECT_EQ(cr.differing, 1U);
    EXPECT_EQ(cr.differences,
              "line 3:\n"
              "  QtRocket:       <name>R</name>\n"
              "  OpenRocket:     <name>R</name>\\r\n");

    const OrkTextComparison reordered = compareOrkTexts(
        kRocket,
        changed("    <name>R</name>\n    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n",
                "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n    <name>R</name>\n"));
    EXPECT_EQ(reordered.differing, 2U);

    const OrkTextComparison missing = compareOrkTexts(kRocket, changed("</openrocket>\n", ""));
    EXPECT_EQ(missing.differing, 1U);
    EXPECT_EQ(missing.differences, "line 8:\n  QtRocket:   </openrocket>\n  OpenRocket: (none)\n");

    const OrkTextComparison end =
        compareOrkTexts(kRocket, changed("</openrocket>\n", "</openrocket>"));
    EXPECT_EQ(end.differing, 1U);
    EXPECT_EQ(end.differences,
              "the end:\n  QtRocket:   a line feed at the end\n"
              "  OpenRocket: no line feed at the end\n");
}

// A report names at most kMaxReportedLines lines and counts the others.
TEST(OrkResaveDiffGolden, TheComparisonCountsWhatItDoesNotShow)
{
    std::string ours;
    std::string theirs;
    for (std::size_t i = 0; i < kMaxReportedLines + 3; ++i)
    {
        ours += std::format("<a>{}</a>\n", i);
        theirs += std::format("<b>{}</b>\n", i);
    }
    const OrkTextComparison comparison = compareOrkTexts(ours, theirs);
    EXPECT_EQ(comparison.differing, kMaxReportedLines + 3);
    EXPECT_TRUE(comparison.differences.ends_with("... and 3 more lines\n"))
        << comparison.differences;
}

// What is a number of the same value: the text around the numbers is the same, the numbers read
// as the same double (bit for bit: -0.0 is not 0.0), and one at least is written otherwise. The
// digits inside a word (an id) are no number of their own.
TEST(OrkResaveDiffGolden, TheSameValueIsTheSameDoubleInTheSameText)
{
    EXPECT_TRUE(sameByValue("<x>1.0E-3</x>", "<x>0.001</x>"));
    EXPECT_TRUE(sameByValue(R"(<p x="9.999999999999999E22" y="1"/>)", R"(<p x="1.0E23" y="1"/>)"));
    EXPECT_TRUE(sameByValue("<x>-1.5</x>", "<x>-1.50</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<x>0.001</x>")) << "nothing written otherwise";
    EXPECT_FALSE(sameByValue("<x>0.0</x>", "<x>-0.0</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<x>0.002</x>"));
    EXPECT_FALSE(sameByValue("<x>0.001</x>", "<y>1.0E-3</y>"));
    EXPECT_FALSE(sameByValue("<x>1</x><x>2</x>", "<x>1</x>"));
    EXPECT_FALSE(sameByValue("<id>a5b</id>", "<id>a05b</id>"));
    EXPECT_TRUE(sameByValue("<id>5b</id>", "<id>05b</id>")) << "a number before a letter";
    EXPECT_FALSE(sameByValue("<x>-0.5</x>", "<x>0.5</x>")) << "the sign is the number's";
    EXPECT_TRUE(sameByValue("<id>2-5</id>", "<id>2-5.0</id>")) << "a '-' after a digit is no sign";
}

// The rocket element of a text, from "  <rocket>" to "  </rocket>".
TEST(OrkResaveDiffGolden, TheRocketElementIsItsLines)
{
    EXPECT_EQ(rocketElementOf(kRocket),
              "  <rocket>\n"
              "    <name>R</name>\n"
              "    <id>5b3bdde2-5a1e-4223-87ba-cc52d1641209</id>\n"
              "    <axialoffset method=\"absolute\">0.001</axialoffset>\n"
              "    <designtype>original</designtype>\n"
              "  </rocket>\n");
    EXPECT_EQ(rocketElementOf("  <rocket>\n  </rocket>\n"), "  <rocket>\n  </rocket>\n");
    EXPECT_EQ(rocketElementOf("<openrocket>\n  <rocket>\n"), "");
    EXPECT_EQ(rocketElementOf("<openrocket/>\n"), "");
}

// The changes of two texts with the place of every line (placedLineChanges(), which
// kLegacySaveChanges is written in): the same lines added in another place, or indented
// otherwise, are other changes, where lineChanges() tells them apart by neither.
TEST(OrkResaveDiffGolden, ThePlacedChangesTellWhereALineStands)
{
    constexpr std::string_view kResave =
        "<a>\n  <s>\n    <x/>\n  </s>\n  <s>\n    <x/>\n  </s>\n</a>\n";
    constexpr std::string_view kInTheFirst =
        "<a>\n  <s>\n    <x/>\n    <y/>\n  </s>\n  <s>\n    <x/>\n  </s>\n</a>\n";
    constexpr std::string_view kInTheSecond =
        "<a>\n  <s>\n    <x/>\n  </s>\n  <s>\n    <x/>\n    <y/>\n  </s>\n</a>\n";
    constexpr std::string_view kIndented =
        "<a>\n  <s>\n    <x/>\n      <y/>\n  </s>\n  <s>\n    <x/>\n  </s>\n</a>\n";
    EXPECT_EQ(placedLineChanges(kResave, kInTheFirst), "+ 4:     <y/>\n");
    EXPECT_EQ(placedLineChanges(kResave, kInTheSecond), "+ 7:     <y/>\n");
    EXPECT_EQ(placedLineChanges(kResave, kIndented), "+ 4:       <y/>\n");
    EXPECT_EQ(QtRocket::Test::lineChanges(kResave, kInTheFirst), "+ <y/>\n");
    EXPECT_EQ(QtRocket::Test::lineChanges(kResave, kInTheSecond), "+ <y/>\n");
    EXPECT_EQ(QtRocket::Test::lineChanges(kResave, kIndented), "+ <y/>\n");
    EXPECT_EQ(placedLineChanges(kInTheFirst, kInTheSecond), "- 4:     <y/>\n+ 7:     <y/>\n");
    EXPECT_EQ(placedLineChanges(kResave, kResave), "");
}

}  // namespace
