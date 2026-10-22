#include "QtRocket/gui/configdialog/ClusterSelectionPanel.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <span>
#include <string>

#include <QGridLayout>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "GuiTestSupport.h"
#include "QtRocket/gui/configdialog/ClusterButton.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ClusterConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "RingPanelTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ClusterSelectionPanelTest = Test::GuiTest;
using Test::PanelHost;
using Test::RingDesign;

/// The layout @p index of ClusterConfiguration::configurations().
[[nodiscard]] const ClusterConfiguration& layout(std::size_t index)
{
    return ClusterConfiguration::configurations()[index];
}

/// How @p panel differs from Java's: the buttons in the layouts' order, four to a row, each named
/// after its layout, and only the button of @p selected shown selected.
[[nodiscard]] std::string buttonDifferences(const ClusterSelectionPanel& panel,
                                            const ClusterConfiguration*  selected)
{
    const auto* grid = qobject_cast<const QGridLayout*>(panel.layout());
    if (grid == nullptr)
    {
        return "no grid\n";
    }
    std::string                                 differences;
    const std::span<const ClusterConfiguration> configurations =
        ClusterConfiguration::configurations();
    if (panel.buttonCount() != static_cast<int>(configurations.size()))
    {
        differences += std::format("{} buttons\n", panel.buttonCount());
    }
    for (int i = 0; i < panel.buttonCount(); ++i)
    {
        const ClusterButton& button  = panel.button(i);
        int                  row     = -1;
        int                  column  = -1;
        int                  rows    = 0;
        int                  columns = 0;
        grid->getItemPosition(grid->indexOf(&button), &row, &column, &rows, &columns);
        const ClusterConfiguration& expected = configurations[static_cast<std::size_t>(i)];
        if (&button.configuration() != &expected || row != i / 4 || column != i % 4 ||
            button.toolTip() != QtStrings::fromUtf8(expected.getXmlName()) ||
            button.isSelected() != (&expected == selected))
        {
            differences += std::format("button {} ({}) at {}, {}: tooltip '{}', selected {}\n", i,
                                       button.configuration().getXmlName(), row, column,
                                       QtStrings::toUtf8(button.toolTip()), button.isSelected());
        }
    }
    return differences;
}

TEST_F(ClusterSelectionPanelTest, FourteenButtonsFourToARowWithTheTubesLayoutSelected)
{
    // Java: a MigLayout of four 50-pixel columns that wraps after every fourth button: the 14
    // layouts make three rows of four and a row of two (the scout's "4 x 3 grid" of "12" missed
    // 9-grid and 9-star).
    const RingDesign design;
    PanelHost        host(*design.inner);
    const auto*      panel = &host.make<ClusterSelectionPanel>();
    EXPECT_EQ(panel->objectName(), QStringLiteral("ClusterConfiguration"));
    EXPECT_EQ(panel->buttonCount(), 14);
    EXPECT_EQ(panel->selectedConfiguration(), &ClusterConfiguration::single());
    EXPECT_EQ(buttonDifferences(*panel, &ClusterConfiguration::single()), "");
    const auto* grid = qobject_cast<const QGridLayout*>(panel->layout());
    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(grid->rowCount(), 4);
    EXPECT_EQ(grid->columnCount(), 4);
    EXPECT_EQ(grid->horizontalSpacing(), 0);
    EXPECT_EQ(grid->verticalSpacing(), 0);
    EXPECT_THROW(static_cast<void>(panel->button(14)), BugError);
    EXPECT_THROW(static_cast<void>(panel->button(-1)), BugError);
}

TEST_F(ClusterSelectionPanelTest, AClickGivesTheLayoutToEverySelectedTube)
{
    const RingDesign design;
    auto&            other = design.body->addChild(std::make_unique<InnerTube>());
    other.setClusterConfiguration(layout(1));
    const std::array<RocketComponent*, 1> others{&other};
    PanelHost                             host(*design.inner, others);
    auto*                                 panel = &host.make<ClusterSelectionPanel>();
    host.root().show();
    QTest::mouseClick(&panel->button(4), Qt::LeftButton, Qt::NoModifier, QPoint(25, 25));
    EXPECT_EQ(&design.inner->getClusterConfiguration(), &layout(4));
    EXPECT_EQ(&other.getClusterConfiguration(), &layout(4));
    EXPECT_EQ(panel->selectedConfiguration(), &layout(4));
    EXPECT_EQ(buttonDifferences(*panel, &layout(4)), "");

    panel->select(layout(12));
    EXPECT_EQ(&design.inner->getClusterConfiguration(), &layout(12));
    EXPECT_EQ(&other.getClusterConfiguration(), &layout(12));
    EXPECT_EQ(design.inner->getInstanceCount(), 9);
}

TEST_F(ClusterSelectionPanelTest, AChangeOfTheMainTubeShowsAfterItsEvent)
{
    const RingDesign design;
    PanelHost        host(*design.inner);
    const auto*      panel = &host.make<ClusterSelectionPanel>();
    design.inner->setClusterConfiguration(layout(5));
    EXPECT_EQ(panel->selectedConfiguration(), &layout(5));
    EXPECT_EQ(buttonDifferences(*panel, &layout(5)), "");
}

TEST_F(ClusterSelectionPanelTest, ATreeChangeEndsTheSelection)
{
    // The TREE change invalidates the selection: no layout shown, a click changes nothing.
    const RingDesign design;
    PanelHost        host(*design.inner);
    auto*            panel = &host.make<ClusterSelectionPanel>();
    static_cast<void>(design.body->addChild(std::make_unique<InnerTube>()));
    EXPECT_EQ(panel->selectedConfiguration(), nullptr);
    EXPECT_EQ(buttonDifferences(*panel, nullptr), "");
    panel->select(layout(3));
    EXPECT_EQ(&design.inner->getClusterConfiguration(), &ClusterConfiguration::single());
}

TEST_F(ClusterSelectionPanelTest, OnlyAnInnerTubeHasClusters)
{
    const RingDesign design;
    PanelHost        host(*design.coupler);
    EXPECT_THROW(static_cast<void>(&host.make<ClusterSelectionPanel>()), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
