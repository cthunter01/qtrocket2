// The design view (RocketPanel) in its design window, as the WindowManager puts one below the
// tabs of every window: its clicks select in the component tree's selection and the tree's
// changes show in its figure (the design panel's highlight), its double-click asks the window for
// the configuration dialog, its right-click shows the design panel's popup menu, and every one of
// OpenRocket's example designs opens in a window with it without a Qt warning.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QMenu>
#include <QObject>
#include <QPainter>
#include <QPoint>
#include <QSignalSpy>
#include <QSplitter>
#include <QString>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "ExpectedQtMessage.h"
#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/RocketActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "TestGuiServices.h"
#include "figure/FigureMouse.h"
#include "goldens/GoldenExamples.h"
#include "tree/ProbeDesign.h"

namespace QtRocket::Gui
{

namespace
{

using LastWindowBehaviour = WindowManager::LastWindowBehaviour;
using Gui::Test::ProbeDesign;

/// Delivers the posted events until the window, its layouts and its figure are done.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

/// The ids of @p components, in their order.
[[nodiscard]] std::vector<Uuid> idsOf(const std::vector<RocketComponent*>& components)
{
    std::vector<Uuid> ids;
    ids.reserve(components.size());
    for (const RocketComponent* component : components)
    {
        ids.push_back(component->getId());
    }
    return ids;
}

/// The windows of a window manager over test services and a clipboard of the test's own; the
/// configuration dialog requests of the test's window recorded as "dialog <name> [<others>]".
class RocketPanelWindowTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    /// Opens a window on @p document and makes it the test's.
    MainWindow& open(std::unique_ptr<OpenRocketDocument> document)
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                        LastWindowBehaviour::EXIT);
        }
        m_window = &m_manager->addDocument(std::move(document));
        QObject::connect(m_window, &MainWindow::configDialogRequested, m_window,
                         [this](RocketComponent*                     component,
                                const std::vector<RocketComponent*>& others, bool, bool) {
                             std::string text = "dialog " + component->getName() + " [";
                             for (const RocketComponent* other : others)
                             {
                                 text += (text.back() == '[' ? "" : ", ") + other->getName();
                             }
                             m_requests.push_back(text + "]");
                         });
        settle();
        return *m_window;
    }

    /// Opens a window on the probes' design (ProbeDesign, one stage).
    MainWindow& openProbeDesign()
    {
        return open(ProbeDesign::makeDocument(ProbeDesign::Booster::NONE));
    }

    [[nodiscard]] MainWindow&   window() const { return *m_window; }
    [[nodiscard]] DesignPanel&  design() const { return *window().componentDesignPanel(); }
    [[nodiscard]] RocketPanel&  panel() const { return *window().componentRocketPanel(); }
    [[nodiscard]] RocketFigure& figure() const { return panel().figure(); }
    [[nodiscard]] Rocket&       rocket() const { return window().document().getRocket(); }
    [[nodiscard]] DocumentSelectionModel&   selection() const { return design().selection(); }
    [[nodiscard]] std::vector<std::string>& requests() noexcept { return m_requests; }

    [[nodiscard]] RocketComponent& find(std::string_view name) const
    {
        return ProbeDesign::find(rocket(), name);
    }
    /// A pixel of the figure where @p name is the topmost component.
    [[nodiscard]] QPoint pixelOf(std::string_view name) const
    {
        const std::optional<QPoint> pixel = Gui::Test::figurePixelOf(figure(), find(name));
        if (!pixel.has_value())
        {
            ADD_FAILURE() << "no pixel of " << name;
            return {};
        }
        return *pixel;
    }

private:
    Gui::Test::TestGuiServices     m_services;
    OpenRocketClipboard            m_clipboard;
    std::unique_ptr<WindowManager> m_manager;
    MainWindow*                    m_window = nullptr;
    std::vector<std::string>       m_requests;
};

TEST_F(RocketPanelWindowTest, EveryWindowHasADesignViewBelowItsTabs)
{
    openProbeDesign();
    RocketPanel* const panel = window().componentRocketPanel();
    ASSERT_NE(panel, nullptr);
    EXPECT_EQ(window().rocketPanel(), panel);
    EXPECT_EQ(window().splitter()->indexOf(panel), 1);
    EXPECT_EQ(&panel->document(), &window().document());
    EXPECT_EQ(panel->selectionModel(), &design().selection());
    EXPECT_TRUE(panel->isVisible());
}

TEST_F(RocketPanelWindowTest, TheFigureDrawsTheDesignPanelsHighlight)
{
    openProbeDesign();
    // BasicFrame's start: the first stage alone.
    EXPECT_EQ(figure().selection(), (std::vector<Uuid>{find("Sustainer").getId()}));

    // The tree's selection, then a stage with everything below it.
    selection().setSelectedComponent(find("BT1"));
    EXPECT_EQ(figure().selection(), (std::vector<Uuid>{find("BT1").getId()}));
    selection().setSelectedComponent(find("Sustainer"));
    EXPECT_EQ(figure().selection(), idsOf(design().highlightedComponents()));
    EXPECT_GT(figure().selection().size(), 1U);
    selection().clearComponentSelection();
    EXPECT_TRUE(figure().selection().empty());
}

TEST_F(RocketPanelWindowTest, AClickInTheFigureSelectsInTheTree)
{
    openProbeDesign();
    Gui::Test::clickAt(figure(), pixelOf("BT2"));
    ASSERT_EQ(selection().selectedComponents().size(), 1U);
    EXPECT_EQ(selection().selectedComponent(), &find("BT2"));
    // And the figure shows it.
    EXPECT_EQ(figure().selection(), (std::vector<Uuid>{find("BT2").getId()}));

    Gui::Test::clickAt(figure(), pixelOf("NC"), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_EQ(idsOf(selection().selectedComponents()),
              (std::vector<Uuid>{find("BT2").getId(), find("NC").getId()}));
    Gui::Test::clickAt(figure(), QPoint(2, 2));
    EXPECT_TRUE(selection().selectedComponents().empty());
    EXPECT_TRUE(requests().empty());
}

TEST_F(RocketPanelWindowTest, ADoubleClickAsksTheWindowForTheDialog)
{
    openProbeDesign();
    const QPoint pixel = pixelOf("BT1");
    Gui::Test::clickAt(figure(), pixel);
    Gui::Test::secondClickAt(figure(), pixel);
    EXPECT_EQ(requests(), (std::vector<std::string>{"dialog BT1 []"}));

    // A multi-edit with Shift.
    requests().clear();
    Gui::Test::secondClickAt(figure(), pixelOf("NC"), Qt::LeftButton, Qt::ShiftModifier);
    EXPECT_EQ(requests(), (std::vector<std::string>{"dialog NC [BT1]"}));
}

TEST_F(RocketPanelWindowTest, ARightClickShowsTheComponentPopupMenu)
{
    openProbeDesign();
    QMenu&           menu = *design().rocketActions().contextMenu();
    const QSignalSpy shown(&menu, &QMenu::aboutToShow);
    {
        // The offscreen platform cannot raise the menu or grab the keyboard for it.
        const Gui::Test::ExpectedQtMessage noRaise(QStringLiteral("does not support raise"));
        const Gui::Test::ExpectedQtMessage noGrab(QStringLiteral("grabbing the keyboard"));
        Gui::Test::clickAt(figure(), pixelOf("IT"), Qt::RightButton);
        EXPECT_EQ(shown.count(), 1);
        EXPECT_TRUE(menu.isVisible());
        menu.hide();
    }
    // The topmost clicked component was selected first (decision U4).
    EXPECT_EQ(selection().selectedComponent(), &find("IT"));
    EXPECT_EQ(selection().selectedComponents().size(), 1U);
}

/// OpenRocket's sixteen example designs.
class RocketPanelWindowExample : public RocketPanelWindowTest,
                                 public ::testing::WithParamInterface<QtRocket::Test::GoldenExample>
{ };

TEST_P(RocketPanelWindowExample, OpensWithItsDesignViewWithoutAQtWarning)
{
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(GetParam());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    open(std::move(loaded->document));
    // The figure shows the design, the overlay its numbers; every view is painted (any Qt
    // warning on the way fails the test: GuiTestMain.cpp).
    EXPECT_FALSE(figure().figureGeometry().shapes().empty());
    QImage image(panel().size(), QImage::Format_ARGB32_Premultiplied);
    for (const FigureView view : RocketPanel::kViewTypes)
    {
        panel().setViewType(view);
        settle();
        QPainter painter(&image);
        panel().render(&painter);
    }
    // A click on the first stage's first component selects it in the tree.
    panel().setViewType(FigureView::SIDE);
    settle();
    ASSERT_GT(rocket().getChild(0).getChildCount(), 0U);
    const RocketComponent&      first = rocket().getChild(0).getChild(0);
    const std::optional<QPoint> pixel = Gui::Test::figurePixelOf(figure(), first);
    ASSERT_TRUE(pixel.has_value());
    Gui::Test::clickAt(figure(), pixel.value_or(QPoint()));
    EXPECT_EQ(selection().selectedComponent(), &first);
}

INSTANTIATE_TEST_SUITE_P(Examples, RocketPanelWindowExample,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

}  // namespace

}  // namespace QtRocket::Gui
