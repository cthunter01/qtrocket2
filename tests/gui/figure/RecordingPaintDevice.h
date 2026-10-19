#pragma once

#include <memory>
#include <vector>

#include <QBrush>
#include <QFont>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QTransform>

class QPaintEngine;

namespace QtRocket::Gui::Test
{

/// What a RecordingPaintDevice was asked to draw: one drawing call of a QPainter, with the
/// painter's state at that moment.
struct RecordedPaint
{
    /// The kind of call.
    enum class Kind
    {
        /// drawPath(), fillPath() (with no pen) and strokePath() (with no brush).
        PATH,
        /// drawRect(), drawRects() and fillRect(): each rectangle is a record of its own.
        RECT,
        /// drawEllipse().
        ELLIPSE,
        /// drawLine() and drawLines(): each line is a record of its own.
        LINE,
        /// drawPolygon(), drawPolyline() (the path is not closed), drawConvexPolygon().
        POLYGON,
        /// drawPoint() and drawPoints(): each point is a moveTo of the path.
        POINTS,
        /// drawText(): text(), position() and font() tell what.
        TEXT,
        /// drawPixmap(), drawImage(), drawTiledPixmap(): the path is the target rectangle.
        IMAGE,
    };

    Kind kind{Kind::PATH};
    /// The geometry in the painter's coordinates (before transform()).
    QPainterPath path;
    QPen         pen;
    QBrush       brush;
    /// The painter's combined transform (its world transform and its window and viewport).
    QTransform transform;
    /// Whether QPainter::Antialiasing was on.
    bool antialiased{false};
    /// The text and its baseline origin of a TEXT record.
    QString text;
    QPointF position;
    QFont   font;

    /// path() mapped by transform(): where it lands on the device.
    [[nodiscard]] QPainterPath devicePath() const { return transform.map(path); }
};

/// A paint device that draws nothing and records every call a QPainter makes on it, with the pen,
/// the brush, the transform and the antialiasing of the moment (RecordedPaint), for tests that
/// check what is painted (shapes, pens, dashes, colours, texts) without looking at pixels. Its
/// engine takes every QPainter feature, so that QPainter hands it the primitives as they are,
/// with the transform in the state, and never turns them into polygons or pixels first. It
/// reports 96 dots per inch, 32 bits per pixel and a device pixel ratio of 1.
///
/// A QWidget paints into it with QWidget::render(&device, QPoint(), QRegion(), {}).
class RecordingPaintDevice : public QPaintDevice
{
public:
    /// A device of @p size pixels.
    explicit RecordingPaintDevice(QSize size);
    ~RecordingPaintDevice() override;
    RecordingPaintDevice(const RecordingPaintDevice&)            = delete;
    RecordingPaintDevice& operator=(const RecordingPaintDevice&) = delete;
    RecordingPaintDevice(RecordingPaintDevice&&)                 = delete;
    RecordingPaintDevice& operator=(RecordingPaintDevice&&)      = delete;

    /// Everything recorded since it was made or cleared, in the order of the calls.
    [[nodiscard]] const std::vector<RecordedPaint>& records() const noexcept { return m_records; }
    /// Forgets the records.
    void clear() noexcept { m_records.clear(); }

    [[nodiscard]] QSize size() const noexcept { return m_size; }

    [[nodiscard]] QPaintEngine* paintEngine() const override;

protected:
    [[nodiscard]] int metric(PaintDeviceMetric metric) const override;

private:
    class Engine;

    QSize                      m_size;
    std::vector<RecordedPaint> m_records;
    std::unique_ptr<Engine>    m_engine;
};

}  // namespace QtRocket::Gui::Test
