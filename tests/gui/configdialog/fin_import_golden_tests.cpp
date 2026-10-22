// "Import from image" of the freeform fin editor (FreeformFinSetConfig::importImage()) compared
// with OpenRocket's FreeformFinSetConfig.importImage(): the report's "import" blocks
// (tests/data/fin-images/report.txt, tools/openrocket-fin-import), which give the fin points
// after the import, after the document's undo and after its redo, with the undo descriptions.

#include <array>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QAction>

#include <gtest/gtest.h>

#include "BodyPanelTestSupport.h"
#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/configdialog/FreeformFinSetConfig.h"
#include "QtRocket/gui/util/CustomFinImporter.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "ScriptedUserPrompts.h"
#include "figure/FinPointTestSupport.h"
#include "util/FinImportReport.h"

namespace QtRocket::Gui
{

namespace
{

using Test::ConfigDialogServices;
using Test::FactoryPanel;
using Test::finImage;
using Test::finImageTestName;
using Test::FinImportReport;
using Test::FreeformDesign;
using Test::outlineText;

/// The report's import blocks: the image (without ".png"), " multi" when a second fin set is
/// selected (Java: the first one's config listener).
constexpr std::array<std::string_view, 10> kImports{
    "trapezoid", "rectangle", "swept",        "full-black",     "staircase",
    "overhang",  "edges",     "single-pixel", "one-pixel-wide", "trapezoid multi",
};

/// The second fin set of the multi-component imports (FinImportDumper's).
[[nodiscard]] FreeformDesign::Outline otherOutline()
{
    return {0, 0, 0.115, 0.072, 0.255, 0.072, 0.255, 0.037, 0.150, 0};
}

/// The state line of the report: "points N: POINTS length=L [other N: POINTS] undo=U redo=R".
[[nodiscard]] std::string stateLine(const FreeformDesign& design, bool multi)
{
    const FreeformFinSet& fins = design.fins(0);
    std::string line = std::format("points {} length={}", outlineText(fins.getFinPoints()),
                                   FloatingDecimal::toJavaFormatString(fins.getLength()));
    if (multi)
    {
        line += " other " + outlineText(design.fins(1).getFinPoints());
    }
    const OpenRocketDocument& document = design.document();
    line += " undo=" + (document.isUndoAvailable() ? document.getUndoDescription().value_or("null")
                                                   : std::string("-"));
    line += " redo=" + (document.isRedoAvailable() ? document.getRedoDescription().value_or("null")
                                                   : std::string("-"));
    return line;
}

using FinImportPanelGoldenReport = Test::GuiTest;

TEST_F(FinImportPanelGoldenReport, HasTheImportsOfTheTests)
{
    std::vector<std::string> imports;
    for (const std::string& header : FinImportReport::get().headers())
    {
        if (header.starts_with("import "))
        {
            imports.push_back(header.substr(7));
        }
    }
    EXPECT_EQ(imports, std::vector<std::string>(kImports.begin(), kImports.end()));
}

class FinImportPanelGolden : public Test::ConfigDialogTest,
                             public ::testing::WithParamInterface<std::string_view>
{ };

TEST_P(FinImportPanelGolden, ImportsAsOpenRocket)
{
    const std::string_view name   = GetParam();
    const bool             multi  = name.ends_with(" multi");
    const std::string      image  = std::string(name.substr(0, name.find(' '))) + ".png";
    const std::string      block  = "import " + std::string(name);
    const FinImportReport& report = FinImportReport::get();
    ConfigDialogServices   services;
    const FreeformDesign   design(multi ? std::vector<FreeformDesign::Outline>{{}, otherOutline()}
                                        : std::vector<FreeformDesign::Outline>{{}});
    std::vector<RocketComponent*> others;
    if (multi)
    {
        others.push_back(&design.fins(1));
    }
    const FactoryPanel<FreeformFinSetConfig> panel(design.document(), services, design.fins(0),
                                                   others);
    const Result<std::vector<Coordinate>> imported = CustomFinImporter::getPoints(finImage(image));
    ASSERT_TRUE(imported.has_value()) << imported.error().message;
    EXPECT_EQ(outlineText(*imported), report.line(block, "imported"));

    services.prompts().answerOpenFiles(UserPrompts::OpenFileAnswer{
        .files = {finImage(image)}, .directory = finImage("").parent_path()});
    panel->importImageAction().trigger();
    EXPECT_EQ(stateLine(design, multi), report.line(block, "after"));
    design.document().undo();
    EXPECT_EQ(stateLine(design, multi), report.line(block, "undo"));
    design.document().redo();
    EXPECT_EQ(stateLine(design, multi), report.line(block, "redo"));
}

INSTANTIATE_TEST_SUITE_P(Images, FinImportPanelGolden, ::testing::ValuesIn(kImports),
                         [](const ::testing::TestParamInfo<std::string_view>& param) {
                             return finImageTestName(std::string(param.param) + ".png");
                         });

}  // namespace

}  // namespace QtRocket::Gui
