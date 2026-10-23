#include "GuiTestSupport.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QMimeData>
#include <QPalette>
#include <QString>
#include <QWidget>

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/Error.h"
#include "ScopedApplicationPalette.h"
#include "TestTempDir.h"

// The checks of the fixture GuiTest, each made to fail once with GoogleTest's own failure capture
// (EXPECT_NONFATAL_FAILURE): a check that could never fail would let every leaking test pass.

namespace QtRocket::Gui::Test
{

namespace
{

/// A GuiTest run by hand, its SetUp() and TearDown() around one scenario.
class GuiTestProbe : public GuiTest
{
public:
    void runSetUp() { SetUp(); }
    void runTearDown() { TearDown(); }

private:
    void TestBody() override { }
};

TEST(GuiTest, PassesATestThatLeavesNothingBehind)
{
    GuiTestProbe probe;
    probe.runSetUp();
    {
        const QWidget window;
    }
    probe.runTearDown();
}

TEST(GuiTest, FailsATestThatLeavesATopLevelWidgetBehind)
{
    EXPECT_NONFATAL_FAILURE(
        {
            GuiTestProbe probe;
            probe.runSetUp();
            QWidget window;
            window.setObjectName(QStringLiteral("leftOver"));
            window.setWindowTitle(QStringLiteral("Rocket"));
            probe.runTearDown();
        },
        "the test left a top-level widget behind: QWidget 'leftOver' titled 'Rocket'");
}

TEST(GuiTest, FailsATestThatLeavesDataOnTheClipboardAndClearsIt)
{
    EXPECT_NONFATAL_FAILURE(
        {
            GuiTestProbe probe;
            probe.runSetUp();
            QGuiApplication::clipboard()->setText(QStringLiteral("Nose cone"));
            probe.runTearDown();
        },
        "the test left data on the clipboard: text/plain");
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    EXPECT_TRUE(data == nullptr || data->formats().isEmpty());
}

TEST(GuiTest, FailsATestThatLeavesComponentsOnTheApplicationsComponentClipboardAndClearsIt)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    EXPECT_NONFATAL_FAILURE(
        {
            GuiTestProbe probe;
            probe.runSetUp();
            Result<ComponentCopy> copy = ComponentCopy::copyOf(
                *document, std::vector<const RocketComponent*>{&document->getRocket().getChild(0)});
            if (copy.has_value())
            {
                OpenRocketClipboard::instance().setComponents(std::move(*copy));
            }
            probe.runTearDown();
        },
        "the test left components on the application's component clipboard");
    EXPECT_TRUE(OpenRocketClipboard::instance().isEmpty());
}

TEST(GuiTest, FailsATestThatChangesADefaultUnit)
{
    EXPECT_NONFATAL_FAILURE(
        {
            GuiTestProbe probe;
            probe.runSetUp();
            UnitGroup& length = unitGroup(UnitGroupId::LENGTH);
            length.setDefaultUnit(length.getDefaultUnitIndex() == 0 ? 1 : 0);
            probe.runTearDown();
        },
        "the test changed the default unit of LENGTH");
}

TEST(GuiTest, PutsTheDefaultUnitsBack)
{
    int defaultLength = 0;
    {
        // The fixture's DefaultUnitsGuard puts the defaults in place now and when the fixture ends.
        const GuiTestProbe probe;
        defaultLength = unitGroup(UnitGroupId::LENGTH).getDefaultUnitIndex();
        unitGroup(UnitGroupId::LENGTH).setDefaultUnit(defaultLength == 0 ? 1 : 0);
    }
    EXPECT_EQ(unitGroup(UnitGroupId::LENGTH).getDefaultUnitIndex(), defaultLength);
}

TEST(GuiTest, FailsATestThatChangesTheCurrentDirectoryAndChangesItBack)
{
    const std::filesystem::path current = std::filesystem::current_path();
    EXPECT_NONFATAL_FAILURE(
        {
            const QtRocket::Test::TempDir directory;
            GuiTestProbe                  probe;
            probe.runSetUp();
            std::filesystem::current_path(directory.path());
            probe.runTearDown();
        },
        "the test changed the current directory to ");
    EXPECT_EQ(std::filesystem::current_path(), current);
}

TEST(GuiTest, FailsATestThatChangesTheApplicationPaletteAndPutsItBack)
{
    const QPalette palette = QGuiApplication::palette();
    EXPECT_NONFATAL_FAILURE(
        {
            GuiTestProbe probe;
            probe.runSetUp();
            QApplication::setPalette(ScopedApplicationPalette::dark());
            probe.runTearDown();
        },
        "the test changed the application's palette");
    EXPECT_EQ(QGuiApplication::palette(), palette);
}

TEST(GuiTest, PassesATestWhoseScopedPaletteEndsInIt)
{
    GuiTestProbe probe;
    probe.runSetUp();
    {
        const ScopedApplicationPalette dark(ScopedApplicationPalette::dark());
        EXPECT_EQ(QGuiApplication::palette().color(QPalette::Window),
                  ScopedApplicationPalette::dark().color(QPalette::Window));
    }
    probe.runTearDown();
}

}  // namespace

}  // namespace QtRocket::Gui::Test
