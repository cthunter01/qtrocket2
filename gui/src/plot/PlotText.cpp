#include "QtRocket/gui/plot/PlotText.h"

#include <QChar>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <rocketplot/text.h>

namespace QtRocket::Gui
{

QString PlotText::literalText(const QString& text)
{
    if (!text.contains(QLatin1Char('\n')))
    {
        return rocketplot::literalText(text);
    }
    QStringList lines = text.split(QLatin1Char('\n'));
    for (QString& line : lines)
    {
        line = line.toHtmlEscaped().replace(QLatin1Char('$'), QStringLiteral("&#36;"));
    }
    return QStringLiteral("<p style=\"white-space:pre\">") + lines.join(QStringLiteral("<br>")) +
           QStringLiteral("</p>");
}

}  // namespace QtRocket::Gui
