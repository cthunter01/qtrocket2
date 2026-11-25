#include "exit/OpenDesigns.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/util/Error.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GoldenResaves.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// @p text with '_' for '-' (a test's name).
[[nodiscard]] std::string testNameOf(std::string_view text)
{
    std::string name(text);
    std::ranges::replace(name, '-', '_');
    return name;
}

}  // namespace

std::string_view openDesignKind(OpenDesignSource source) noexcept
{
    switch (source)
    {
        case OpenDesignSource::EXAMPLE:
            return "example";
        case OpenDesignSource::EXAMPLE_RESAVE:
            return "example-resave";
        case OpenDesignSource::TEST_ROCKET:
            return "testrocket";
        case OpenDesignSource::LEGACY:
            return "legacy";
        case OpenDesignSource::COVERAGE:
            return "coverage";
        case OpenDesignSource::GUI_REFERENCE:
            return "gui";
    }
    return {};
}

void PrintTo(const OpenDesign& design, std::ostream* out)
{
    *out << design.name;
}

std::vector<OpenDesign> makeOpenDesigns(std::string& problems)
{
    constexpr std::size_t   kDesigns = 66;
    std::vector<OpenDesign> designs;
    designs.reserve(kDesigns);
    const auto& examples = QtRocket::Test::goldenExamples();
    for (const QtRocket::Test::GoldenExample& example : examples)
    {
        designs.push_back({.name        = testNameOf(example.name),
                           .source      = OpenDesignSource::EXAMPLE,
                           .file        = example.original,
                           .goldenInput = example.name});
    }
    for (const QtRocket::Test::GoldenExample& example : examples)
    {
        designs.push_back({.name        = "resave_" + testNameOf(example.name),
                           .source      = OpenDesignSource::EXAMPLE_RESAVE,
                           .file        = example.resave,
                           .goldenInput = example.name});
    }
    for (const QtRocket::Test::GoldenInput& input : QtRocket::Test::testRocketInputs(problems))
    {
        designs.push_back({.name        = testNameOf(input.name),
                           .source      = OpenDesignSource::TEST_ROCKET,
                           .file        = QtRocket::Test::goldensDir() / input.resave,
                           .goldenInput = input.name});
    }
    for (const auto& [kind, source] :
         {std::pair(QtRocket::Test::kLegacyKind, OpenDesignSource::LEGACY),
          std::pair(QtRocket::Test::kCoverageKind, OpenDesignSource::COVERAGE)})
    {
        for (const QtRocket::Test::ResaveInput& input :
             QtRocket::Test::resaveInputsOf(kind, problems))
        {
            designs.push_back({.name        = testNameOf(input.name),
                               .source      = source,
                               .file        = QtRocket::Test::goldensDir() / input.resave,
                               .goldenInput = {}});
        }
    }
    designs.push_back({.name        = "gui_from_scratch",
                       .source      = OpenDesignSource::GUI_REFERENCE,
                       .file        = QtRocket::Test::guiFromScratchDir() / "rocket.ork",
                       .goldenInput = {}});
    return designs;
}

const OpenDesignList& openDesignList()
{
    static const OpenDesignList kList = [] {
        OpenDesignList list;
        list.designs = makeOpenDesigns(list.problems);
        return list;
    }();
    return kList;
}

const std::vector<OpenDesign>& openDesigns()
{
    return openDesignList().designs;
}

const OpenDesign* findOpenDesign(std::string_view name)
{
    const std::vector<OpenDesign>& designs = openDesigns();
    const auto                     found   = std::ranges::find(designs, name, &OpenDesign::name);
    return found == designs.end() ? nullptr : &*found;
}

std::string openDesignTestName(const ::testing::TestParamInfo<OpenDesign>& info)
{
    return info.param.name;
}

const ThrustCurveMotorSetDatabase& openDesignMotors(const OpenDesign& design)
{
    return design.source == OpenDesignSource::TEST_ROCKET ? QtRocket::Test::testRocketMotors()
                                                          : QtRocket::Test::bundledMotorDatabase();
}

Result<LoadedOpenDesign> loadOpenDesign(const OpenDesign& design)
{
    LoadedOpenDesign loaded;
    loaded.environment = std::make_unique<QtRocket::Test::DesignFileEnvironment>(
        QtRocket::Test::DesignFileEnvironment::Presets::EXAMPLES, openDesignMotors(design));
    const GeneralRocketLoader loader(loaded.environment->context());
    Result<LoadedDocument>    document = loader.load(design.file);
    if (!document.has_value())
    {
        return std::unexpected(std::move(document.error()));
    }
    loaded.document = std::move(document->document);
    return loaded;
}

}  // namespace QtRocket::Gui::Test
