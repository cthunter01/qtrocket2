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
//   is the first save with data but for one known difference, the sixteen stored warnings of the
//   type RecoveryHighSpeedDeployment (in six examples), which the loader reads back as warnings of
//   the type Other without their parameter, as OpenRocket's does, until the user's decision E2 of
//   2026-10-14 (group 'decisions') makes it read them as they were written. OpenRocket's own save
//   with data of the same sixteen loads (the scouts' SaverProbe, saver-simulations out/java/) is
//   QtRocket's line for line; those files are not part of the repository.
// - The three examples with a <preset> again with no presets (Presets::NONE): the six
//   references that cannot be resolved are kept (the user's decision U8 of 2026-10-08) and
//   written back, so the file is the re-save's all the same.
// - The thirteen test rockets as save(load(re-save)) (loadResave() of GoldenResaves.h, with the
//   seven motors of TestRockets and OpenRocket's test preferences): the re-save but for one line,
//   the wall of the pod body of the Alpha III with pods, 2 mm in the file and none in the loaded
//   tube (a tube without a radius keeps no wall), as OpenRocket's own reload has it.
//
// The same loop pins OpenRocket's estimateFileSize() of each loaded example (one load per
// example serves everything).

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
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
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Sha256.h"
#include "QtRocket/util/Strings.h"
#include "file/DesignFileEnvironment.h"
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
using QtRocket::Test::LoadedResave;
using QtRocket::Test::loadGoldenExample;
using QtRocket::Test::loadResave;
using QtRocket::Test::OrkTextComparison;
using QtRocket::Test::rocketElementOf;
using QtRocket::Test::sameByValue;
using QtRocket::Test::testRocketInputs;
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

/// The lines of @p text, each without its '\n'.
[[nodiscard]] std::vector<std::string_view> linesOf(std::string_view text)
{
    std::vector<std::string_view> lines;
    while (!text.empty())
    {
        const std::size_t end = text.find('\n');
        lines.push_back(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    return lines;
}

/// The lines that differ between @p before and @p after, two texts of the same number of lines,
/// each as "<line of before> -> <line of after>" without the indentation, one per line; a text
/// of another number of lines is named instead.
[[nodiscard]] std::string changedLines(std::string_view before, std::string_view after)
{
    const std::vector<std::string_view> a = linesOf(before);
    const std::vector<std::string_view> b = linesOf(after);
    if (a.size() != b.size())
    {
        return std::format("{} lines -> {} lines\n", a.size(), b.size());
    }
    std::string changes;
    for (std::size_t i = 0; i < a.size(); ++i)
    {
        if (a[i] != b[i])
        {
            const std::size_t from = a[i].find_first_not_of(' ');
            const std::size_t to   = b[i].find_first_not_of(' ');
            changes +=
                std::format("{} -> {}\n", a[i].substr(from == std::string_view::npos ? 0 : from),
                            b[i].substr(to == std::string_view::npos ? 0 : to));
        }
    }
    return changes;
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
    for (const std::string_view line : linesOf(rocketElementOf(saved)))
    {
        compared.components += isIdLine(line) ? 1U : 0U;
        compared.presets += line.contains("<preset ") ? 1U : 0U;
        compared.motors += line.contains("<motor configid=") ? 1U : 0U;
    }
    for (const std::string_view line : linesOf(saved))
    {
        compared.simulations += line.contains("<simulation status=") ? 1U : 0U;
        compared.warnings += line.contains("<warning type=") ? 1U : 0U;
    }
}

/// Compares @p saved, QtRocket's save of the design @p name, with @p resave, the whole text, and
/// adds the result to @p compared; a difference is a problem unless it is @p expected
/// (changedLines() from the re-save to the save).
void compareSaves(SavesCompared& compared, std::string_view name, std::string_view saved,
                  std::string_view resave, std::string_view expected = "")
{
    const OrkTextComparison comparison = compareOrkTexts(saved, resave);
    compared.designs++;
    compared.lines += comparison.lines;
    compared.byValue += comparison.byValue;
    countParts(compared, saved);
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

/// @p text as it is saved again after a load before the user's decision E2 of 2026-10-14 (group
/// 'decisions') is merged: every stored warning of the type RecoveryHighSpeedDeployment (what the
/// saver writes, Java's class name) of the type Other and without its <parameter> line, as the
/// loader reads it back (it knows the type by the name HighSpeedDeployment only, as OpenRocket's
/// loader). Counts those warnings into @p count.
[[nodiscard]] std::string asReadBackBeforeE2(std::string_view text, std::size_t& count)
{
    std::string result;
    result.reserve(text.size());
    bool inHighSpeedWarning = false;
    for (const std::string_view line : linesOf(text))
    {
        const std::string_view trimmed = QtRocket::Strings::trim(line);
        if (trimmed == kHighSpeedWarning)
        {
            ++count;
            inHighSpeedWarning = true;
            result += std::format("{}<warning type=\"Other\">\n",
                                  line.substr(0, line.size() - trimmed.size()));
            continue;
        }
        if (inHighSpeedWarning && trimmed.starts_with("<parameter>"))
        {
            continue;
        }
        if (trimmed == "</warning>")
        {
            inHighSpeedWarning = false;
        }
        result += std::format("{}\n", line);
    }
    return result;
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
    /// The stored warnings of the type RecoveryHighSpeedDeployment that came back as Other
    /// (asReadBackBeforeE2()).
    std::size_t highSpeedWarnings{0};
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

/// Saves @p document, an example, with its simulation data, then as an .ork archive (archiveOf(),
/// which compares the archive with OpenRocket's), loads the archive back from memory without a
/// directory (decision O5 of tier 10), with no warning, and compares the flight data and the save
/// of the reload with the first save (asReadBackBeforeE2()), all but the rocket: the first save of
/// a load writes the automatic dimensions the dual parachute deployment's first save writes
/// (kDualParachuteSaves), not those of a design saved before. Adds the result to @p trips.
void roundTripTheData(const GoldenExample& example, OpenRocketDocument& document,
                      DataRoundTrips& trips)
{
    const Preferences&           preferences = goldenExampleEnvironment().preferences();
    WarningSet                   warnings;
    const std::string            first   = saveOf(document, preferences, warnings, true);
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
    const std::string expected = asReadBackBeforeE2(withoutRocket(first), trips.highSpeedWarnings);
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
// than OpenRocket writes it. With it: OpenRocket's estimate of each file's size; the saves of one
// loaded document that follow the first change as OpenRocket's do; the sixteen .ork archives are
// OpenRocket's, entry for entry (23 decal images, 41 thrust curves); and loaded back from them,
// the flights of the 54 simulations (69 branches) come back bit for bit, and a second save is the
// first but for the sixteen warnings of decision E2.
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
    EXPECT_EQ(trips.simulations, 54U);
    EXPECT_EQ(trips.branches, 69U);
    EXPECT_EQ(trips.archives, 16U);
    EXPECT_EQ(trips.decals, 23U);
    EXPECT_EQ(trips.curves, 41U);
    // Decision E2 of 2026-10-14 (group 'decisions'): the loader is to read the type
    // RecoveryHighSpeedDeployment back. Once that is merged, the second save equals the first and
    // asReadBackBeforeE2() turns this known difference into a reported one: then compare the two
    // saves as they are and drop this pin.
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

// What a second save after a load makes of a stored warning of the type
// RecoveryHighSpeedDeployment until decision E2 is merged: a warning of the type Other without
// its parameter; every other line stays.
TEST(OrkResaveDiffGolden, AHighSpeedWarningIsReadBackAsOtherBeforeE2)
{
    constexpr std::string_view kText =
        "        <warning type=\"RecoveryHighSpeedDeployment\">\n"
        "          <id>9e26b796-6349-4902-ba33-14816cf9d3bd</id>\n"
        "          <parameter>31.94803510503947</parameter>\n"
        "          Text\n"
        "        </warning>\n"
        "        <warning type=\"LargeAOA\">\n"
        "          <parameter>0.5</parameter>\n"
        "        </warning>\n";
    std::size_t count = 0;
    EXPECT_EQ(asReadBackBeforeE2(kText, count),
              "        <warning type=\"Other\">\n"
              "          <id>9e26b796-6349-4902-ba33-14816cf9d3bd</id>\n"
              "          Text\n"
              "        </warning>\n"
              "        <warning type=\"LargeAOA\">\n"
              "          <parameter>0.5</parameter>\n"
              "        </warning>\n");
    EXPECT_EQ(count, 1U);
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
// carriage return, a line missing and two lines in another order are reported in their lines.
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

}  // namespace
