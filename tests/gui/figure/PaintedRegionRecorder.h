#pragma once

#include <QEvent>
#include <QObject>
#include <QRect>
#include <QRegion>

class QScrollBar;
class QWidget;

namespace QtRocket::Gui::Test
{

/// Collects the regions of the paint events a widget receives (an event filter on it), in the
/// widget's coordinates: what Qt had it paint again, for instance after a scroll.
class PaintedRegionRecorder final : public QObject
{
public:
    /// Adds the region of every paint event of @p widget to @p painted until the recorder is
    /// destroyed; both must outlive it.
    PaintedRegionRecorder(QWidget& widget, QRegion& painted);
    ~PaintedRegionRecorder() override;

    PaintedRegionRecorder(const PaintedRegionRecorder&)            = delete;
    PaintedRegionRecorder& operator=(const PaintedRegionRecorder&) = delete;
    PaintedRegionRecorder(PaintedRegionRecorder&&)                 = delete;
    PaintedRegionRecorder& operator=(PaintedRegionRecorder&&)      = delete;

    /// Adds the region of a paint event of the widget.
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QWidget* m_widget;
    QRegion* m_painted;
};

/// Whether @p region covers every pixel of @p rect (QRegion::contains(QRect) only tells whether
/// they overlap).
[[nodiscard]] bool coversAll(const QRegion& region, const QRect& rect);

/// Lets @p widget paint what it was asked to, moves @p bar, which scrolls it, by @p delta pixels
/// and lets it paint again: whether its paint events covered all of what can be seen of it
/// (its visible region's bounds) within @p timeoutMilliseconds. A paint that covers it comes at
/// the first pass of the event loop; one that does not never comes.
[[nodiscard]] bool paintsAllOfItAfterScrolling(QWidget& widget, QScrollBar& bar, int delta,
                                               int timeoutMilliseconds = 500);

}  // namespace QtRocket::Gui::Test
