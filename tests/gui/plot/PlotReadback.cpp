#include "plot/PlotReadback.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include <QChar>
#include <QColor>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QObject>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTransform>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <Qt>
#include <QtContainerFwd>
#include <QtTypes>

#include <gtest/gtest.h>
#include <rocketplot/Axis.h>
#include <rocketplot/LineSeries.h>
#include <rocketplot/PlotWidget.h>
#include <rocketplot/Series.h>

#include "QtRocket/gui/util/QtStrings.h"
#include "TestTempDir.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// How far (in pixels) a text may start outside a box and still count as in it, and how close two
/// texts' baselines are on one row: SVG coordinates are written with a few decimals.
constexpr double kTolerance = 0.5;

/// The transform an SVG "transform" attribute gives: QSvgGenerator writes
/// "matrix(a,b,c,d,e,f)"; none for an empty one; nullopt for another form.
[[nodiscard]] std::optional<QTransform> transformOf(QStringView attribute)
{
    if (attribute.isEmpty())
    {
        return QTransform();
    }
    const QStringView prefix = u"matrix(";
    if (!attribute.startsWith(prefix) || !attribute.endsWith(u')'))
    {
        return std::nullopt;
    }
    const QList<QStringView> values =
        attribute.sliced(prefix.size(), attribute.size() - prefix.size() - 1).split(u',');
    constexpr qsizetype kMatrixValues = 6;
    if (values.size() != kMatrixValues)
    {
        return std::nullopt;
    }
    std::vector<double> numbers;
    for (const QStringView value : values)
    {
        bool         ok     = false;
        const double number = value.trimmed().toDouble(&ok);
        if (!ok)
        {
            return std::nullopt;
        }
        numbers.push_back(number);
    }
    // SVG's matrix(a, b, c, d, e, f) maps (x, y) to (a x + c y + e, b x + d y + f), as
    // QTransform(m11 = a, m12 = b, m21 = c, m22 = d, dx = e, dy = f) does.
    return QTransform(numbers.at(0), numbers.at(1), numbers.at(2), numbers.at(3), numbers.at(4),
                      numbers.at(5));
}

/// The number an attribute holds, 0 for none.
[[nodiscard]] double numberOf(const QXmlStreamAttributes& attributes, QStringView name)
{
    return attributes.value(name).toDouble();
}

/// What readSvg() reads of an SVG document: its texts and paths, each in the document's order.
struct SvgContents
{
    std::vector<SvgText> texts;
    std::vector<SvgPath> paths;
};

/// A group of the SVG document around the element being read: its transform (with its parents')
/// and its fill (its own or the innermost parent's that has one).
struct SvgGroup
{
    QTransform transform;
    QString    fill;
};

/// The fill an element with @p attributes inside @p group paints with: its own, or the group's.
[[nodiscard]] QString fillOf(const QXmlStreamAttributes& attributes, const SvgGroup& group)
{
    return attributes.hasAttribute(u"fill") ? attributes.value(u"fill").toString() : group.fill;
}

/// The texts and paths of the SVG document @p file, in its order.
[[nodiscard]] SvgContents readSvg(const QString& file)
{
    QFile svg(file);
    if (!svg.open(QIODevice::ReadOnly))
    {
        ADD_FAILURE() << "cannot read the SVG export " << QtStrings::toUtf8(file);
        return {};
    }
    SvgContents           contents;
    std::vector<SvgGroup> groups{SvgGroup{}};
    QXmlStreamReader      reader(&svg);
    while (!reader.atEnd())
    {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == u"g")
        {
            const QXmlStreamAttributes      attributes = reader.attributes();
            const QStringView               attribute  = attributes.value(u"transform");
            const std::optional<QTransform> local      = transformOf(attribute);
            if (!local.has_value())
            {
                ADD_FAILURE() << "a transform the read-back does not know: "
                              << QtStrings::toUtf8(attribute.toString());
            }
            // The group's own transform first, then its parents'.
            groups.push_back({.transform = local.value_or(QTransform()) * groups.back().transform,
                              .fill      = fillOf(attributes, groups.back())});
        }
        else if (reader.isEndElement() && reader.name() == u"g" && groups.size() > 1)
        {
            groups.pop_back();
        }
        else if (reader.isStartElement() && reader.name() == u"text")
        {
            const QXmlStreamAttributes attributes = reader.attributes();
            const QPointF     anchor(numberOf(attributes, u"x"), numberOf(attributes, u"y"));
            const QTransform& transform = groups.back().transform;
            contents.texts.push_back(
                {.text     = reader.readElementText(QXmlStreamReader::IncludeChildElements),
                 .position = transform.map(anchor),
                 .rotated  = transform.isRotating()});
        }
        else if (reader.isStartElement() && reader.name() == u"path")
        {
            const QXmlStreamAttributes attributes = reader.attributes();
            contents.paths.push_back({.data   = attributes.value(u"d").toString().trimmed(),
                                      .fill   = fillOf(attributes, groups.back()),
                                      .origin = groups.back().transform.map(QPointF(0.0, 0.0))});
        }
    }
    if (reader.hasError())
    {
        ADD_FAILURE() << "the SVG export does not read: "
                      << QtStrings::toUtf8(reader.errorString());
        return {};
    }
    return contents;
}

/// @p plot's SVG export read (readSvg()); nothing, with a test failure, when it cannot be written.
[[nodiscard]] SvgContents exportedSvg(const rocketplot::PlotWidget& plot)
{
    const QtRocket::Test::TempDir directory;
    const QString                 file = QtStrings::fromPath(directory.resolve("plot.svg"));
    if (!plot.exportSvg(file))
    {
        ADD_FAILURE() << "cannot write the SVG export " << QtStrings::toUtf8(file);
        return {};
    }
    return readSvg(file);
}

/// The point of the path command @p command, @p letter and "x,y" ("M1.5,-2"); nullopt for
/// another command.
[[nodiscard]] std::optional<QPointF> commandPoint(QStringView command, QChar letter)
{
    if (command.size() < 2 || command.front() != letter)
    {
        return std::nullopt;
    }
    const QList<QStringView> coordinates = command.sliced(1).split(u',');
    if (coordinates.size() != 2)
    {
        return std::nullopt;
    }
    bool         xOk = false;
    bool         yOk = false;
    const double x   = coordinates.front().toDouble(&xOk);
    const double y   = coordinates.back().toDouble(&yOk);
    if (!xOk || !yOk)
    {
        return std::nullopt;
    }
    return QPointF(x, y);
}

/// The points of the path outline @p data if it is a move and straight lines only ("Mx,y Lx,y
/// ...", closed by a "Z" or not), as QSvgGenerator writes one; nullopt otherwise.
[[nodiscard]] std::optional<std::vector<QPointF>> straightOutline(QStringView data)
{
    QList<QStringView> commands = data.split(u' ', Qt::SkipEmptyParts);
    if (!commands.isEmpty() && commands.back() == u"Z")
    {
        commands.removeLast();
    }
    std::vector<QPointF> points;
    for (const QStringView command : std::as_const(commands))
    {
        const std::optional<QPointF> point = commandPoint(command, points.empty() ? u'M' : u'L');
        if (!point.has_value())
        {
            return std::nullopt;
        }
        points.push_back(*point);
    }
    return points;
}

/// Whether @p point is in @p box, or within kTolerance of it.
[[nodiscard]] bool isIn(const QRectF& box, QPointF point)
{
    return !box.isEmpty() &&
           box.adjusted(-kTolerance, -kTolerance, kTolerance, kTolerance).contains(point);
}

/// The y of the x tick labels' row among @p texts (see readPlotTexts()), nullopt for none.
[[nodiscard]] std::optional<double> xTickRow(const std::vector<SvgText>& texts,
                                             const QRectF& plotArea, const QRectF& legendArea)
{
    std::optional<double> row;
    for (const SvgText& text : texts)
    {
        const QPointF at = text.position;
        if (!text.rotated && !isIn(legendArea, at) && at.y() > plotArea.bottom() &&
            at.x() >= plotArea.left())
        {
            row = row.has_value() ? std::min(*row, at.y()) : at.y();
        }
    }
    return row;
}

/// Whether @p text is a y tick label of a plot whose area is @p plotArea, whose legend is in
/// @p legendArea and whose x tick labels are on @p row.
[[nodiscard]] bool isYTickLabel(const SvgText& text, const QRectF& plotArea,
                                const QRectF& legendArea, std::optional<double> row)
{
    const QPointF at = text.position;
    return !text.rotated && !isIn(legendArea, at) && at.x() < plotArea.left() &&
           at.y() > plotArea.top() && (!row.has_value() || at.y() < *row - kTolerance);
}

}  // namespace

void PrintTo(const SeriesReadback& series, std::ostream* out)
{
    *out << std::format(
        "{{name \"{}\", group \"{}\", visible {}, color {}, marker {}, size {}, ring {}, width {}, "
        "markers always {}, {} points}}",
        QtStrings::toUtf8(series.name), QtStrings::toUtf8(series.legendGroup), series.visible,
        QtStrings::toUtf8(series.color.name(QColor::HexArgb)), static_cast<int>(series.marker),
        series.markerSize, series.markerRingWidth, series.lineWidth, series.markersAlwaysVisible,
        series.x.size());
}

void PrintTo(const AxisReadback& axis, std::ostream* out)
{
    *out << std::format(
        "{{label \"{}\", [{:a}, {:a}], autoscale {}, margin {}, zero {}, grid {}, "
        "tick labels {}}}",
        QtStrings::toUtf8(axis.label), axis.min, axis.max, axis.autoscale, axis.autoscaleMargin,
        axis.autoscaleIncludesZero, axis.gridVisible, axis.tickLabelsVisible);
}

SeriesReadback readSeries(const rocketplot::Series& series)
{
    SeriesReadback result{.name                 = series.name(),
                          .legendGroup          = series.legendGroup(),
                          .visible              = series.isVisible(),
                          .color                = series.color(),
                          .marker               = series.marker(),
                          .markerSize           = series.markerSize(),
                          .markerRingWidth      = series.markerRingWidth(),
                          .lineWidth            = 0.0,
                          .markersAlwaysVisible = false,
                          .x                    = {},
                          .y                    = {}};
    if (const auto* const line = qobject_cast<const rocketplot::LineSeries*>(&series))
    {
        result.lineWidth            = line->lineWidth();
        result.markersAlwaysVisible = line->areMarkersAlwaysVisible();
    }
    result.x.reserve(series.size());
    result.y.reserve(series.size());
    for (std::size_t index = 0; index < series.size(); ++index)
    {
        result.x.push_back(series.x(index));
        result.y.push_back(series.y(index));
    }
    return result;
}

std::vector<SeriesReadback> readAllSeries(const rocketplot::PlotWidget& plot)
{
    std::vector<SeriesReadback>      result;
    const QList<rocketplot::Series*> all = plot.series();
    for (const rocketplot::Series* const series : all)
    {
        result.push_back(readSeries(*series));
    }
    return result;
}

QStringList seriesNames(const rocketplot::PlotWidget& plot)
{
    QStringList                      names;
    const QList<rocketplot::Series*> all = plot.series();
    for (const rocketplot::Series* const series : all)
    {
        names.append(series->name());
    }
    return names;
}

AxisReadback readAxis(const rocketplot::Axis& axis)
{
    return {.label                 = axis.label(),
            .min                   = axis.min(),
            .max                   = axis.max(),
            .autoscale             = axis.autoscale(),
            .autoscaleMargin       = axis.autoscaleMargin(),
            .autoscaleIncludesZero = axis.autoscaleIncludesZero(),
            .gridVisible           = axis.isGridVisible(),
            .tickLabelsVisible     = axis.areTickLabelsVisible()};
}

PlotReadback readPlot(const rocketplot::PlotWidget& plot)
{
    return {.title  = plot.title(),
            .xAxis  = readAxis(*plot.xAxis()),
            .yAxis  = readAxis(*plot.yAxis()),
            .series = readAllSeries(plot)};
}

std::vector<SvgText> svgTexts(const rocketplot::PlotWidget& plot)
{
    return exportedSvg(plot).texts;
}

std::vector<SvgPath> svgPaths(const rocketplot::PlotWidget& plot)
{
    return exportedSvg(plot).paths;
}

std::optional<double> centredSquareSide(const SvgPath& path)
{
    std::optional<std::vector<QPointF>> outline = straightOutline(path.data);
    if (!outline.has_value())
    {
        return std::nullopt;
    }
    std::vector<QPointF>& points = *outline;
    // A closed outline may end where it started.
    constexpr std::size_t kCorners = 4;
    if (points.size() == kCorners + 1 && points.back() == points.front())
    {
        points.pop_back();
    }
    if (points.size() != kCorners)
    {
        return std::nullopt;
    }
    const double half = std::abs(points.front().x());
    if (!(half > 0.0))
    {
        return std::nullopt;
    }
    const std::vector<QPointF> corners{{-half, -half}, {half, -half}, {half, half}, {-half, half}};
    for (const QPointF& corner : corners)
    {
        if (std::ranges::count(points, corner) != 1)
        {
            return std::nullopt;
        }
    }
    return 2.0 * half;
}

PlotTexts readPlotTexts(rocketplot::PlotWidget& plot)
{
    // Painted, so that the legend's area is where the legend is drawn at the plot's size.
    static_cast<void>(plot.grab());
    const std::vector<SvgText>  texts      = svgTexts(plot);
    const QRectF                plotArea   = plot.plotArea();
    const QRectF                legendArea = plot.legendArea();
    const std::optional<double> row =
        plot.xAxis()->areTickLabelsVisible() ? xTickRow(texts, plotArea, legendArea) : std::nullopt;
    const bool yLabeled = plot.yAxis()->areTickLabelsVisible();
    PlotTexts  sorted;
    for (const SvgText& text : texts)
    {
        sorted.all.append(text.text);
        if (!text.rotated && isIn(legendArea, text.position))
        {
            sorted.legendEntries.append(text.text);
        }
        else if (row.has_value() && !text.rotated && text.position.y() > plotArea.bottom() &&
                 std::abs(text.position.y() - *row) < kTolerance)
        {
            sorted.xTickLabels.append(text.text);
        }
        else if (yLabeled && isYTickLabel(text, plotArea, legendArea, row))
        {
            sorted.yTickLabels.append(text.text);
        }
    }
    return sorted;
}

}  // namespace QtRocket::Gui::Test
