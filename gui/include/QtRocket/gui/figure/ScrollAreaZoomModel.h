#pragma once

#include <QObject>
#include <QPointer>

#include "QtRocket/gui/figure/ZoomModel.h"

namespace QtRocket::Gui
{

class ScaleScrollArea;

/// The ZoomModel of a ScaleScrollArea: the zoom of the fin point editor (USER_SCALE, Java's
/// ScaleSelector(ScaleScrollPane)) or of the design view's 2D figure (RELATIVE_TO_FIT,
/// RocketPanel's model: 100 % is the fit). It emits changed() whenever the area's figure changes
/// (ScaleFigure::changed()). Once the area is gone, the scale is 1, nothing is fitted and the
/// requests do nothing.
class ScrollAreaZoomModel : public ZoomModel
{
    Q_OBJECT

public:
    /// Which scale the model shows and sets.
    enum class Kind
    {
        /// The figure's user scale (ScaleScrollArea::userScale(), setScaling()).
        USER_SCALE,
        /// The user scale relative to the fit (ScaleScrollArea::relativeScale(),
        /// setRelativeScaling()).
        RELATIVE_TO_FIT,
    };

    ScrollAreaZoomModel(ScaleScrollArea* area, Kind kind, QObject* parent);

    [[nodiscard]] Kind kind() const noexcept { return m_kind; }

    [[nodiscard]] double scale() override;
    [[nodiscard]] bool   isFit() const override;
    void                 setScale(double scale) override;
    /// ScaleScrollArea::setFitting(true).
    void setFit() override;

private:
    QPointer<ScaleScrollArea> m_area;
    Kind                      m_kind;
};

}  // namespace QtRocket::Gui
