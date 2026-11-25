#pragma once

#include <QString>

namespace QtRocket::Gui
{

/// Text for a plot that rocketplot shows exactly as written, line breaks included: the workaround
/// of rocketplot's N20 (handoff/rocketplot-m4-points.md). rocketplot::literalText() promises to
/// keep the line breaks of the text it is given, but returns a plain text with line breaks as it
/// is, which rocketplot lays out as one line (a TextAnnotation one line tall, its lines spilling
/// out of its box). OpenRocket's abort box of the simulation plots has several lines
/// (SimulationPlot.ErrorAnnotationSet). Once rocketplot keeps the line breaks itself, literalText()
/// here becomes rocketplot::literalText() and this class can go.
class PlotText
{
public:
    PlotText() = delete;

    /// @p text as a plot shows it exactly as written (the text of a title, a label, a legend name
    /// or an annotation): rocketplot::literalText(@p text) for a text without a line break ('\n');
    /// for one with line breaks, rich text of one line per line: each line escaped for HTML
    /// (QString::toHtmlEscaped()) with every '$' (which starts math in rocketplot) as "&#36;", the
    /// lines joined by "<br>" in a paragraph that keeps its spaces (<p style="white-space:pre">).
    [[nodiscard]] static QString literalText(const QString& text);
};

}  // namespace QtRocket::Gui
