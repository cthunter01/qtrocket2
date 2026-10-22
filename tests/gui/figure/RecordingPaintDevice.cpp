#include "RecordingPaintDevice.h"

#include <cstddef>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#include <QBrush>
#include <QImage>
#include <QLineF>
#include <QPaintDevice>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSize>
#include <QTextItem>
#include <QTransform>
#include <Qt>

namespace QtRocket::Gui::Test
{

namespace
{

/// RecordingPaintDevice's resolution.
constexpr int kDotsPerInch = 96;
constexpr int kDepth       = 32;

/// @p pixels at kDotsPerInch, in millimetres.
[[nodiscard]] int millimetres(int pixels) noexcept
{
    constexpr double kMillimetresPerInch = 25.4;
    return static_cast<int>(pixels * kMillimetresPerInch / kDotsPerInch);
}

/// The size of a span from a Qt count, never negative.
[[nodiscard]] std::size_t countOf(int count) noexcept
{
    return count < 0 ? 0 : static_cast<std::size_t>(count);
}

}  // namespace

/// The engine: every QPainter feature, so that QPainter hands over every primitive unchanged.
class RecordingPaintDevice::Engine final : public QPaintEngine
{
public:
    explicit Engine(std::vector<RecordedPaint>& records)
      : QPaintEngine(QPaintEngine::AllFeatures), m_records(&records)
    {
    }

    bool begin(QPaintDevice* /*device*/) override { return true; }
    bool end() override { return true; }

    void updateState(const QPaintEngineState& engineState) override
    {
        const QPaintEngine::DirtyFlags flags = engineState.state();
        if (flags.testFlag(QPaintEngine::DirtyPen))
        {
            m_pen = engineState.pen();
        }
        if (flags.testFlag(QPaintEngine::DirtyBrush))
        {
            m_brush = engineState.brush();
        }
        if (flags.testFlag(QPaintEngine::DirtyTransform))
        {
            m_transform = engineState.transform();
        }
        if (flags.testFlag(QPaintEngine::DirtyHints))
        {
            m_antialiased = engineState.renderHints().testFlag(QPainter::Antialiasing);
        }
    }

    void drawPath(const QPainterPath& path) override { record(RecordedPaint::Kind::PATH, path); }

    void drawRects(const QRectF* rects, int rectCount) override
    {
        for (const QRectF& rect : std::span(rects, countOf(rectCount)))
        {
            QPainterPath path;
            path.addRect(rect);
            record(RecordedPaint::Kind::RECT, std::move(path));
        }
    }

    void drawLines(const QLineF* lines, int lineCount) override
    {
        for (const QLineF& line : std::span(lines, countOf(lineCount)))
        {
            QPainterPath path(line.p1());
            path.lineTo(line.p2());
            record(RecordedPaint::Kind::LINE, std::move(path));
        }
    }

    void drawEllipse(const QRectF& rect) override
    {
        QPainterPath path;
        path.addEllipse(rect);
        record(RecordedPaint::Kind::ELLIPSE, std::move(path));
    }

    void drawPoints(const QPointF* points, int pointCount) override
    {
        QPainterPath path;
        for (const QPointF& point : std::span(points, countOf(pointCount)))
        {
            path.moveTo(point);
        }
        record(RecordedPaint::Kind::POINTS, std::move(path));
    }

    void drawPolygon(const QPointF* points, int pointCount, PolygonDrawMode mode) override
    {
        QPainterPath path;
        bool         first = true;
        for (const QPointF& point : std::span(points, countOf(pointCount)))
        {
            if (first)
            {
                path.moveTo(point);
                first = false;
            }
            else
            {
                path.lineTo(point);
            }
        }
        if (mode != PolylineMode)
        {
            path.closeSubpath();
        }
        path.setFillRule(mode == WindingMode ? Qt::WindingFill : Qt::OddEvenFill);
        record(RecordedPaint::Kind::POLYGON, std::move(path));
    }

    void drawTextItem(const QPointF& position, const QTextItem& textItem) override
    {
        RecordedPaint& paint = record(RecordedPaint::Kind::TEXT, QPainterPath{});
        paint.text           = textItem.text();
        paint.position       = position;
        paint.font           = textItem.font();
    }

    void drawPixmap(const QRectF& rect, const QPixmap& /*pixmap*/,
                    const QRectF& /*source*/) override
    {
        recordImage(rect);
    }

    void drawTiledPixmap(const QRectF& rect, const QPixmap& /*pixmap*/,
                         const QPointF& /*offset*/) override
    {
        recordImage(rect);
    }

    void drawImage(const QRectF& rect, const QImage& /*image*/, const QRectF& /*source*/,
                   Qt::ImageConversionFlags /*flags*/) override
    {
        recordImage(rect);
    }

    [[nodiscard]] Type type() const override { return QPaintEngine::User; }

private:
    RecordedPaint& record(RecordedPaint::Kind kind, QPainterPath path)
    {
        RecordedPaint paint;
        paint.kind        = kind;
        paint.path        = std::move(path);
        paint.pen         = m_pen;
        paint.brush       = m_brush;
        paint.transform   = m_transform;
        paint.antialiased = m_antialiased;
        m_records->push_back(std::move(paint));
        return m_records->back();
    }

    void recordImage(const QRectF& rect)
    {
        QPainterPath path;
        path.addRect(rect);
        record(RecordedPaint::Kind::IMAGE, std::move(path));
    }

    std::vector<RecordedPaint>* m_records;
    QPen                        m_pen;
    QBrush                      m_brush;
    QTransform                  m_transform;
    bool                        m_antialiased{false};
};

RecordingPaintDevice::RecordingPaintDevice(QSize size)
  : m_size(size), m_engine(std::make_unique<Engine>(m_records))
{
}

RecordingPaintDevice::~RecordingPaintDevice() = default;

QPaintEngine* RecordingPaintDevice::paintEngine() const
{
    return m_engine.get();
}

int RecordingPaintDevice::metric(PaintDeviceMetric metric) const
{
    switch (metric)
    {
        case PdmWidth:
            return m_size.width();
        case PdmHeight:
            return m_size.height();
        case PdmWidthMM:
            return millimetres(m_size.width());
        case PdmHeightMM:
            return millimetres(m_size.height());
        case PdmDepth:
            return kDepth;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return kDotsPerInch;
        default:
            // The device pixel ratio (1), its scaled and encoded forms, the number of colours.
            return QPaintDevice::metric(metric);
    }
}

std::vector<RecordedPaint> joinTextRuns(std::span<const RecordedPaint> records)
{
    std::vector<RecordedPaint> joined;
    for (const RecordedPaint& record : records)
    {
        if (!joined.empty() && record.kind == RecordedPaint::Kind::TEXT &&
            joined.back().kind == RecordedPaint::Kind::TEXT &&
            record.transform.map(record.position).y() ==
                joined.back().transform.map(joined.back().position).y() &&
            record.pen == joined.back().pen && record.font == joined.back().font)
        {
            joined.back().text += record.text;
            continue;
        }
        joined.push_back(record);
    }
    return joined;
}

}  // namespace QtRocket::Gui::Test
