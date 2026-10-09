#include "goldens/GoldenExamples.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/AutomaticDimensions.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenSimulations.h"

namespace QtRocket::Test
{

namespace
{

/// What the manifest calls the examples, and where their source is said to be.
constexpr std::string_view kExampleKind      = "example";
constexpr std::string_view kExampleDirectory = "data/examples/";
constexpr std::string_view kExamplePrefix    = "example-";

/// The examples the manifest lists (goldenExamples()).
[[nodiscard]] std::vector<GoldenExample> readGoldenExamples()
{
    std::vector<GoldenExample> examples;
    if (!manifest().has_value())
    {
        return examples;
    }
    for (const GoldenInput& input : manifest()->inputs)
    {
        if (input.kind != kExampleKind || !input.source.starts_with(kExampleDirectory))
        {
            continue;
        }
        const std::string file = input.source.substr(kExampleDirectory.size());
        examples.push_back({.name     = input.name,
                            .file     = file,
                            .original = dataDir() / "examples" / file,
                            .resave   = goldensDir() / input.resave});
    }
    return examples;
}

}  // namespace

const std::vector<GoldenExample>& goldenExamples()
{
    static const std::vector<GoldenExample> kExamples = readGoldenExamples();
    return kExamples;
}

const GoldenExample* findGoldenExample(std::string_view name)
{
    const std::vector<GoldenExample>& examples = goldenExamples();
    const auto found = std::ranges::find_if(examples, [name](const GoldenExample& example) {
        return example.file == name || example.name == name;
    });
    return found == examples.end() ? nullptr : &*found;
}

std::string goldenExampleTestName(const ::testing::TestParamInfo<GoldenExample>& info)
{
    std::string name = info.param.name;
    if (name.starts_with(kExamplePrefix))
    {
        name.erase(0, kExamplePrefix.size());
    }
    std::ranges::replace(name, '-', '_');
    return name;
}

const DesignFileEnvironment& goldenExampleEnvironment(DesignFileEnvironment::Presets presets)
{
    // Made on first use and never destroyed before the process ends: they outlive every
    // document a test loads (the simulations of a document keep the preference store). The
    // objects are no constants themselves: the loads write to their preference stores,
    // through the pointer the context holds.
    if (presets == DesignFileEnvironment::Presets::NONE)
    {
        // NOLINTNEXTLINE(misc-const-correctness): written to through its context, see above
        static DesignFileEnvironment s_withoutPresets(DesignFileEnvironment::Presets::NONE);
        return s_withoutPresets;
    }
    // NOLINTNEXTLINE(misc-const-correctness): written to through its context, see above
    static DesignFileEnvironment s_withPresets(DesignFileEnvironment::Presets::EXAMPLES);
    return s_withPresets;
}

Result<LoadedDocument> loadGoldenExample(const GoldenExample& example, ExampleSource source,
                                         DesignFileEnvironment::Presets presets,
                                         GeneralRocketLoader::Options   options)
{
    const GeneralRocketLoader loader(goldenExampleEnvironment(presets).context(),
                                     std::move(options));
    return loader.load(example.path(source));
}

Result<int> settleGoldenExample(const LoadedDocument& loaded)
{
    return settleAutomaticDimensions(loaded.document->getRocket());
}

Result<LoadedDocument> loadSettledGoldenExample(const GoldenExample& example, ExampleSource source,
                                                DesignFileEnvironment::Presets presets)
{
    Result<LoadedDocument> loaded = loadGoldenExample(example, source, presets);
    if (!loaded)
    {
        return loaded;
    }
    const Result<int> settled = settleGoldenExample(*loaded);
    if (!settled)
    {
        return fail(settled.error().code, settled.error().message);
    }
    return loaded;
}

}  // namespace QtRocket::Test
