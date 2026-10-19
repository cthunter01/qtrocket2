#include "QtRocket/gui/figure/ScrollAreaZoomModel.h"

#include <QObject>

#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/figure/ZoomModel.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

ScrollAreaZoomModel::ScrollAreaZoomModel(ScaleScrollArea* area, Kind kind, QObject* parent)
  : ZoomModel(parent), m_area(area), m_kind(kind)
{
    QTROCKET_ASSERT(area != nullptr);
    connect(area->figure(), &ScaleFigure::changed, this, &ZoomModel::changed);
}

double ScrollAreaZoomModel::scale()
{
    ScaleScrollArea* const area = m_area.data();
    if (area == nullptr)
    {
        return 1.0;
    }
    return m_kind == Kind::USER_SCALE ? area->userScale() : area->relativeScale();
}

bool ScrollAreaZoomModel::isFit() const
{
    const ScaleScrollArea* const area = m_area.data();
    return area != nullptr && area->isFitting();
}

void ScrollAreaZoomModel::setScale(double scale)
{
    ScaleScrollArea* const area = m_area.data();
    if (area == nullptr)
    {
        return;
    }
    if (m_kind == Kind::USER_SCALE)
    {
        area->setScaling(scale);
    }
    else
    {
        area->setRelativeScaling(scale);
    }
}

void ScrollAreaZoomModel::setFit()
{
    ScaleScrollArea* const area = m_area.data();
    if (area != nullptr)
    {
        area->setFitting(true);
    }
}

}  // namespace QtRocket::Gui
