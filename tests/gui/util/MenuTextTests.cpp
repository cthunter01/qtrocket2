#include "QtRocket/gui/util/MenuText.h"

#include <QChar>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using MenuTextTest = Test::GuiTest;

TEST_F(MenuTextTest, EscapesEveryAmpersand)
{
    EXPECT_EQ(MenuText::escaped(QStringLiteral("Modify A&B")), QStringLiteral("Modify A&&B"));
    EXPECT_EQ(MenuText::escaped(QStringLiteral("&&")), QStringLiteral("&&&&"));
    EXPECT_EQ(MenuText::escaped(QStringLiteral("Undo")), QStringLiteral("Undo"));
    EXPECT_EQ(MenuText::escaped(QString()), QString());
}

TEST_F(MenuTextTest, MarksTheFirstOccurrenceOfTheMnemonicInEitherCase)
{
    // SwingUtilities.findDisplayedMnemonicIndex(): the first of the upper and the lower case.
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("Undo"), QChar(u'U')), QStringLiteral("&Undo"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("Cut"), QChar(u'T')), QStringLiteral("Cu&t"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("Save as…"), QChar(u'A')),
              QStringLiteral("S&ave as…"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("Properties"), QChar(u'I')),
              QStringLiteral("Propert&ies"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("abA"), QChar(u'A')), QStringLiteral("&abA"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("bAa"), QChar(u'a')), QStringLiteral("b&Aa"));
}

TEST_F(MenuTextTest, LeavesATextWithoutTheMnemonicUnmarked)
{
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("Paste"), QChar(u'V')),
              QStringLiteral("Paste"));
    EXPECT_EQ(MenuText::withMnemonic(QString(), QChar(u'V')), QString());
}

TEST_F(MenuTextTest, EscapesAroundTheMnemonic)
{
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("A&B"), QChar(u'B')), QStringLiteral("A&&&B"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("&Rock & roll"), QChar(u'R')),
              QStringLiteral("&&&Rock && roll"));
    EXPECT_EQ(MenuText::withMnemonic(QStringLiteral("x&y"), QChar(u'Z')), QStringLiteral("x&&y"));
}

}  // namespace

}  // namespace QtRocket::Gui
