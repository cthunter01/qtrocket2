#include "QtRocket/gui/widgets/DescriptionArea.h"

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QSignalSpy>
#include <QString>
#include <QTextBrowser>
#include <QTextDocument>
#include <QUrl>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using DescriptionAreaTest = Test::GuiTest;

TEST_F(DescriptionAreaTest, ShowsItsTextAsHtmlInAFontOnePointSmaller)
{
    const DescriptionArea area(QStringLiteral("<b>Nose cone</b> shapes"), 5, nullptr);
    EXPECT_EQ(area.description(), QStringLiteral("<b>Nose cone</b> shapes"));
    EXPECT_EQ(area.toPlainText(), QStringLiteral("Nose cone shapes"));
    EXPECT_TRUE(area.isReadOnly());
    EXPECT_DOUBLE_EQ(area.textSize(), DescriptionArea::kDefaultSize);
    EXPECT_DOUBLE_EQ(area.document()->defaultFont().pointSizeF(), area.font().pointSizeF() - 1.0);
    EXPECT_EQ(area.horizontalScrollBarPolicy(), Qt::ScrollBarAlwaysOff);
    EXPECT_EQ(area.verticalScrollBarPolicy(), Qt::ScrollBarAsNeeded);
}

TEST_F(DescriptionAreaTest, ReadsPlainTextAsHtml)
{
    DescriptionArea area(3, 0.0, nullptr);
    area.setDescription(QStringLiteral("one\ntwo"));
    // Java's text/html pane joins the lines too.
    EXPECT_EQ(area.toPlainText(), QStringLiteral("one two"));
    EXPECT_DOUBLE_EQ(area.document()->defaultFont().pointSizeF(), area.font().pointSizeF());
}

TEST_F(DescriptionAreaTest, IsAsHighAsItsRows)
{
    const DescriptionArea one(1, nullptr);
    const DescriptionArea two(2, nullptr);
    const DescriptionArea five(5, nullptr);
    const int             line = two.sizeHint().height() - one.sizeHint().height();
    EXPECT_GT(line, 0);
    EXPECT_NEAR(five.sizeHint().height() - one.sizeHint().height(), 4 * line, 2);
}

TEST_F(DescriptionAreaTest, ATransparentAreaHasTheWindowsBackground)
{
    const DescriptionArea area(QString(), 2, -1.5, false, nullptr);
    EXPECT_EQ(area.palette().color(QPalette::Base), area.palette().color(QPalette::Window));
    EXPECT_DOUBLE_EQ(area.textSize(), -1.5);
}

TEST_F(DescriptionAreaTest, TakesItsColoursAndFont)
{
    DescriptionArea area(QStringLiteral("text"), 2, nullptr);
    area.setForegroundColor(QColor(10, 20, 30));
    area.setBackgroundColor(QColor(200, 210, 220));
    EXPECT_EQ(area.palette().color(QPalette::Text), QColor(10, 20, 30));
    EXPECT_EQ(area.palette().color(QPalette::Base), QColor(200, 210, 220));
    QFont font = area.font();
    font.setPointSizeF(20.0);
    area.setTextFont(font);
    EXPECT_DOUBLE_EQ(area.document()->defaultFont().pointSizeF(), 19.0);
    EXPECT_EQ(area.toPlainText(), QStringLiteral("text"));
}

TEST_F(DescriptionAreaTest, ReportsAClickedLink)
{
    DescriptionArea area(QStringLiteral("<a href=\"https://openrocket.info/\">OpenRocket</a>"), 2,
                         nullptr);
    area.setOpensLinks(false);
    EXPECT_FALSE(area.opensLinks());
    const QSignalSpy spy(&area, &DescriptionArea::linkActivated);
    Q_EMIT area.anchorClicked(QUrl(QStringLiteral("https://openrocket.info/")));
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toUrl(), QUrl(QStringLiteral("https://openrocket.info/")));
}

}  // namespace

}  // namespace QtRocket::Gui
