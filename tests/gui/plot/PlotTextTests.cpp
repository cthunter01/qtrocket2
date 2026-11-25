#include "QtRocket/gui/plot/PlotText.h"

#include <vector>

#include <QString>
#include <QStringList>
#include <Qt>
#include <QtContainerFwd>

#include <gtest/gtest.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/TextAnnotation.h>
#include <rocketplot/enums.h>
#include <rocketplot/text.h>

#include "GuiTestSupport.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "plot/PlotReadback.h"

namespace QtRocket::Gui
{

namespace
{

using PlotTextTest = Gui::Test::GuiTest;

/// The texts of @p plot's SVG export, in its order.
[[nodiscard]] QStringList exportedTexts(const rocketplot::PlotWidget& plot)
{
    QStringList texts;
    for (const Gui::Test::SvgText& text : Gui::Test::svgTexts(plot))
    {
        texts.append(text.text);
    }
    return texts;
}

}  // namespace

TEST_F(PlotTextTest, ALineIsRocketplotsLiteralText)
{
    const QString plain = QStringLiteral("Simulation Abort");
    EXPECT_EQ(PlotText::literalText(plain), rocketplot::literalText(plain));
    const QString markup = QStringLiteral("Stage: <b>A & B</b>; Cost: $5");
    EXPECT_EQ(PlotText::literalText(markup), rocketplot::literalText(markup));
}

TEST_F(PlotTextTest, LinesAreAParagraphOfEscapedLines)
{
    EXPECT_EQ(PlotText::literalText(QStringLiteral("Simulation Abort\nStage: A<B & $5;  x")),
              QStringLiteral("<p style=\"white-space:pre\">Simulation Abort<br>"
                             "Stage: A&lt;B &amp; &#36;5;  x</p>"));
    EXPECT_EQ(PlotText::literalText(QStringLiteral("a\n\nb\n")),
              QStringLiteral("<p style=\"white-space:pre\">a<br><br>b<br></p>"));
}

// The workaround of rocketplot's N20: a text annotation of several lines draws each line as
// written, the characters that would be markup or math included.
TEST_F(PlotTextTest, AnAnnotationShowsEveryLineAsWritten)
{
    rocketplot::PlotWidget plot;
    plot.resize(600, 400);
    plot.addLine(std::vector<double>{0.0, 1.0}, std::vector<double>{0.0, 1.0});
    rocketplot::TextAnnotation* const text = plot.addText(
        0.01, 0.01,
        PlotText::literalText(QStringLiteral("Simulation Abort\nStage: <A> & $5$; Time: 1.00 s")));
    text->setCoordinates(rocketplot::AnnotationCoordinates::PLOT_AREA);
    text->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
    const QStringList texts = exportedTexts(plot);
    EXPECT_TRUE(texts.contains(QStringLiteral("Simulation Abort")))
        << QtStrings::toUtf8(texts.join(u'|'));
    EXPECT_TRUE(texts.contains(QStringLiteral("Stage: <A> & $5$; Time: 1.00 s")))
        << QtStrings::toUtf8(texts.join(u'|'));
}

}  // namespace QtRocket::Gui
