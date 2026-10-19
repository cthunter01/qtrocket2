#include "QtRocket/gui/main/MainWindow.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QSignalSpy>
#include <QSize>
#include <QSplitter>
#include <QString>
#include <QStyle>
#include <QTabWidget>
#include <QWidget>
#include <QWindow>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/settings/WindowGeometry.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/FileIo.h"
#include "TestGuiServices.h"

namespace QtRocket::Gui
{

namespace
{

/// The requests of a window for the configuration dialog, as texts: "<name> <number of others>
/// new|existing tab|first tab", or "close".
class DialogRequests
{
public:
    explicit DialogRequests(MainWindow& window)
    {
        QObject::connect(
            &window, &MainWindow::configDialogRequested, &window,
            [this](RocketComponent* component, const std::vector<RocketComponent*>& others,
                   bool isNewComponent, bool rememberPreviousTab) {
                add(*component, others.size(), isNewComponent, rememberPreviousTab);
            });
        QObject::connect(&window, &MainWindow::closeConfigDialogRequested, &window,
                         [this] { m_texts.emplace_back("close"); });
    }

    [[nodiscard]] const std::vector<std::string>& texts() const noexcept { return m_texts; }

private:
    void add(const RocketComponent& component, std::size_t others, bool isNewComponent,
             bool rememberPreviousTab)
    {
        m_texts.push_back(component.getName() + " " + std::to_string(others) + " " +
                          (isNewComponent ? "new" : "existing") + " " +
                          (rememberPreviousTab ? "tab" : "first tab"));
    }

    std::vector<std::string> m_texts;
};

/// A window of a new design.
class MainWindowTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        m_document = OpenRocketDocumentFactory::createNewRocket();
        m_window   = std::make_unique<MainWindow>(*m_document, m_services.services());
    }

    void TearDown() override
    {
        // The window before its document, both before the fixture looks for windows left.
        m_window.reset();
        m_document.reset();
        GuiTest::TearDown();
    }

    [[nodiscard]] OpenRocketDocument&         document() { return *m_document; }
    [[nodiscard]] Rocket&                     rocket() { return m_document->getRocket(); }
    [[nodiscard]] MainWindow&                 window() { return *m_window; }
    [[nodiscard]] Gui::Test::TestGuiServices& services() { return m_services; }

    /// The title as the window shows it, with Qt's modified mark ("*" where the style shows it).
    [[nodiscard]] std::string shownTitle()
    {
        static_cast<void>(m_window->winId());
        return QtStrings::toUtf8(m_window->windowHandle()->title());
    }

    /// The modified mark the style shows in a title.
    [[nodiscard]] std::string modifiedMark() const
    {
        return m_window->style()->styleHint(QStyle::SH_TitleBar_ModifyNotification, nullptr,
                                            m_window.get()) != 0
                   ? "*"
                   : "";
    }

private:
    Gui::Test::TestGuiServices          m_services;
    std::unique_ptr<OpenRocketDocument> m_document;
    std::unique_ptr<MainWindow>         m_window;
};

TEST_F(MainWindowTest, HoldsTheDesignTabAboveTheRocketPanel)
{
    const QSplitter& splitter = *window().splitter();
    EXPECT_EQ(window().centralWidget(), &splitter);
    EXPECT_EQ(splitter.objectName(), QStringLiteral("splitPane"));
    EXPECT_EQ(splitter.orientation(), Qt::Vertical);
    ASSERT_EQ(splitter.count(), 2);
    EXPECT_EQ(splitter.widget(0), window().tabs());
    EXPECT_EQ(splitter.widget(1), window().rocketPanel());
    EXPECT_FALSE(splitter.childrenCollapsible());

    const QTabWidget& tabs = *window().tabs();
    EXPECT_EQ(tabs.objectName(), QStringLiteral("tabbedPane"));
    ASSERT_EQ(tabs.count(), 1);
    EXPECT_EQ(tabs.tabText(0), QStringLiteral("Rocket design"));
    EXPECT_EQ(tabs.widget(0), window().designPanel());
    EXPECT_EQ(window().designPanel()->objectName(), QStringLiteral("designPanel"));
    EXPECT_EQ(window().rocketPanel()->objectName(), QStringLiteral("rocketPanel"));

    EXPECT_EQ(&window().document(), &document());
    EXPECT_TRUE(window().menus().isDesignWindowMenus());
    EXPECT_EQ(window().menus().undoAction(), window().undoRedoActions().undoAction());
    EXPECT_FALSE(window().windowIcon().isNull());
}

TEST_F(MainWindowTest, TitleIsTheRocketsNameAndTheFilesName)
{
    EXPECT_EQ(QtStrings::toUtf8(window().windowTitle()), "[*]Rocket");
    EXPECT_FALSE(window().isWindowModified());
    EXPECT_TRUE(window().windowFilePath().isEmpty());
    EXPECT_EQ(shownTitle(), "Rocket");

    const std::filesystem::path file = pathFromUtf8("designs/My rocket.ork");
    document().setFile(file);
    // setFile() tells nothing: whoever calls it renews the title.
    EXPECT_EQ(QtStrings::toUtf8(window().windowTitle()), "[*]Rocket");
    window().refreshTitle();
    EXPECT_EQ(QtStrings::toUtf8(window().windowTitle()), "[*]Rocket (My rocket.ork)");
    EXPECT_EQ(window().windowFilePath(), QtStrings::fromPath(file));
    EXPECT_EQ(shownTitle(), "Rocket (My rocket.ork)");
    EXPECT_EQ(MainWindow::titleFor(document()), QStringLiteral("[*]Rocket (My rocket.ork)"));
}

TEST_F(MainWindowTest, TitleFollowsTheRocketsEditsUndoAndSave)
{
    document().addUndoPosition("Rename");
    rocket().setName("Mars");
    EXPECT_EQ(QtStrings::toUtf8(window().windowTitle()), "[*]Mars");
    EXPECT_TRUE(window().isWindowModified());
    EXPECT_EQ(shownTitle(), modifiedMark() + "Mars");

    // Undo never makes a design saved again.
    window().menus().undoAction()->trigger();
    EXPECT_EQ(QtStrings::toUtf8(window().windowTitle()), "[*]Rocket");
    EXPECT_TRUE(window().isWindowModified());

    // setSaved() tells nothing either.
    document().setSaved(true);
    EXPECT_TRUE(window().isWindowModified());
    window().refreshTitle();
    EXPECT_FALSE(window().isWindowModified());
    EXPECT_EQ(shownTitle(), "Rocket");
}

TEST_F(MainWindowTest, TitleShowsAModifiedMarkOfTheNameAsItIs)
{
    rocket().setName("A[*]B");
    EXPECT_EQ(MainWindow::titleFor(document()), QStringLiteral(u"[*]A[*\u200B]B"));
    EXPECT_EQ(shownTitle(), modifiedMark() + "A[*\u200B]B");
    document().setSaved(true);
    window().refreshTitle();
    EXPECT_EQ(shownTitle(), "A[*\u200B]B");
}

TEST_F(MainWindowTest, PanelsTakeThePlacesOfThePlaceholders)
{
    const QPointer<QWidget> designPlaceholder = window().designPanel();
    const QPointer<QWidget> rocketPlaceholder = window().rocketPanel();
    auto* const             design            = makeChild<QWidget>(&window());
    window().setDesignPanel(design);
    EXPECT_EQ(window().designPanel(), design);
    ASSERT_EQ(window().tabs()->count(), 1);
    EXPECT_EQ(window().tabs()->widget(0), design);
    EXPECT_EQ(window().tabs()->tabText(0), QStringLiteral("Rocket design"));

    auto* const rocketPanel = makeChild<QWidget>(&window());
    window().setRocketPanel(rocketPanel);
    EXPECT_EQ(window().rocketPanel(), rocketPanel);
    EXPECT_EQ(window().splitter()->widget(1), rocketPanel);
    EXPECT_EQ(window().splitter()->widget(0), window().tabs());

    // The placeholders go when the event loop deletes them.
    EXPECT_FALSE(designPlaceholder.isNull());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(designPlaceholder.isNull());
    EXPECT_TRUE(rocketPlaceholder.isNull());
}

TEST_F(MainWindowTest, OpensAt90PercentOfTheScreenWithTheDividerAt40Percent)
{
    const QSize screen = QGuiApplication::primaryScreen()->size();
    EXPECT_EQ(window().size(), QSize(screen.width() * 9 / 10, screen.height() * 9 / 10));

    window().show();
    ASSERT_TRUE(window().isVisible());
    const QList<int> sizes  = window().splitter()->sizes();
    const int        length = window().splitter()->height() - window().splitter()->handleWidth();
    ASSERT_EQ(sizes.size(), 2);
    EXPECT_EQ(sizes.at(0), static_cast<int>(length * MainWindow::kDividerLocation));
    EXPECT_EQ(sizes.at(0) + sizes.at(1), length);
}

TEST_F(MainWindowTest, EachSideOfTheDividerTakesHalfOfAChangeOfTheHeight)
{
    // JSplitPane.setResizeWeight(0.5).
    window().show();
    ASSERT_TRUE(window().isVisible());
    const QList<int> before       = window().splitter()->sizes();
    const int        heightBefore = window().splitter()->height();
    window().resize(window().width(), window().height() - 100);
    const QList<int> after  = window().splitter()->sizes();
    const int        change = window().splitter()->height() - heightBefore;
    ASSERT_EQ(before.size(), 2);
    ASSERT_EQ(after.size(), 2);
    ASSERT_LT(change, -50);
    EXPECT_NEAR(after.at(0) - before.at(0), change / 2.0, 1.0);
    EXPECT_NEAR(after.at(1) - before.at(1), change / 2.0, 1.0);
}

TEST_F(MainWindowTest, ASideKeepsItsMinimumHeightAndTheOtherTakesTheRestOfAChange)
{
    // BasicSplitPaneUI.distributeSpace(): the side that would go below its minimum takes what it
    // lacks from the other one.
    window().show();
    ASSERT_TRUE(window().isVisible());
    const QList<int> before = window().splitter()->sizes();
    ASSERT_EQ(before.size(), 2);
    window().rocketPanel()->setMinimumHeight(before.at(1) - 20);
    const int heightBefore = window().splitter()->height();
    window().resize(window().width(), window().height() - 100);
    const int        change = window().splitter()->height() - heightBefore;
    const QList<int> after  = window().splitter()->sizes();
    ASSERT_EQ(after.size(), 2);
    ASSERT_LT(change, -50);
    EXPECT_EQ(after.at(1), before.at(1) - 20);
    EXPECT_EQ(after.at(0), before.at(0) + change + 20);
}

TEST_F(MainWindowTest, RemembersItsGeometryAsMainWindow)
{
    window().show();
    ASSERT_TRUE(window().isVisible());
    window().move(40, 50);
    const Preferences* const windows =
        services().preferences().findNode(GuiPreferenceKeys::kNodeWindows);
    ASSERT_NE(windows, nullptr);
    EXPECT_EQ(windows->keys(),
              std::vector<std::string>{WindowGeometry::key(QStringLiteral("MainWindow"))});
}

TEST_F(MainWindowTest, ClosingAsksInsteadOfClosing)
{
    window().show();
    ASSERT_TRUE(window().isVisible());
    const QSignalSpy closeRequested(&window(), &MainWindow::closeRequested);
    EXPECT_FALSE(window().close());
    EXPECT_EQ(closeRequested.count(), 1);
    EXPECT_TRUE(window().isVisible());
}

TEST_F(MainWindowTest, RequestsForTheConfigurationDialogComeOutAsSignals)
{
    const DialogRequests                requests(window());
    RocketComponent&                    stage = rocket().getChild(0);
    const std::vector<RocketComponent*> both{&stage, &rocket()};
    window().requestConfigDialog(rocket());
    window().requestConfigDialog(stage, both, true, false);
    window().requestConfigDialogClose();
    EXPECT_EQ(requests.texts(), (std::vector<std::string>{"Rocket 0 existing tab",
                                                          "Sustainer 2 new first tab", "close"}));
}

TEST_F(MainWindowTest, RequestsTheConfigurationDialogOfItsOwnComponentsOnly)
{
    const DialogRequests                      requests(window());
    RocketComponent&                          stage = rocket().getChild(0);
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    EXPECT_THROW(window().requestConfigDialog(other->getRocket()), BugError);
    const std::vector<RocketComponent*> foreign{&other->getRocket().getChild(0)};
    EXPECT_THROW(window().requestConfigDialog(stage, foreign), BugError);
    const std::vector<RocketComponent*> none{nullptr};
    EXPECT_THROW(window().requestConfigDialog(stage, none), BugError);
    EXPECT_TRUE(requests.texts().empty());
}

TEST_F(MainWindowTest, HasNoComponentDesignPanelOfItsOwn)
{
    // The WindowManager puts a DesignPanel in (DesignPanelTests); a placeholder or another
    // widget is none.
    EXPECT_EQ(window().componentDesignPanel(), nullptr);
    window().setDesignPanel(makeChild<QWidget>(&window()));
    EXPECT_EQ(window().componentDesignPanel(), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
