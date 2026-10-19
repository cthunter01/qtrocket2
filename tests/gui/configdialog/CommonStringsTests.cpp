#include "QtRocket/gui/configdialog/CommonStrings.h"

#include <QChar>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using CommonStringsTest = Test::GuiTest;

TEST_F(CommonStringsTest, TheDaggerAndItsNote)
{
    // CommonStrings.dagger = "\u2020"; override_description = dagger + " " +
    // Configuration.lbl.override.
    EXPECT_EQ(CommonStrings::dagger(), QString(QChar(0x2020)));
    EXPECT_EQ(
        CommonStrings::overrideDescription(),
        QString(QChar(0x2020)) +
            QStringLiteral(" This parameter can be overridden in each flight configuration."));
}

}  // namespace

}  // namespace QtRocket::Gui
