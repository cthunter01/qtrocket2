#include "QtRocket/gui/widgets/HtmlLabel.h"

#include <QLabel>
#include <QSize>
#include <QString>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using HtmlLabelTest = Test::GuiTest;

TEST_F(HtmlLabelTest, HoldsItsHeightAtTheHeightOfItsFirstText)
{
    const QString text = QStringLiteral("<html>Line one<br>Line two</html>");
    HtmlLabel     label(text, nullptr);
    const QLabel  plain(text, nullptr);
    const int     height = plain.sizeHint().height();
    EXPECT_EQ(label.minimumHeight(), height);
    EXPECT_EQ(label.maximumHeight(), height);
    EXPECT_EQ(label.minimumSizeHint(), QSize(0, height));
    // A later text keeps the height (OpenRocket limits the size once).
    label.setText(QStringLiteral("<html>One<br>two<br>three<br>four</html>"));
    EXPECT_EQ(label.maximumHeight(), height);
}

TEST_F(HtmlLabelTest, TakesAnAlignment)
{
    const HtmlLabel label(QStringLiteral("<b>Centred</b>"), Qt::AlignHCenter, nullptr);
    EXPECT_TRUE(label.alignment().testFlag(Qt::AlignHCenter));
    EXPECT_EQ(label.minimumHeight(), label.maximumHeight());
}

}  // namespace

}  // namespace QtRocket::Gui
