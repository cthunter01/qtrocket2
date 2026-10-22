#include "TestScaleFigure.h"

#include <algorithm>

#include <QPoint>
#include <QWidget>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Gui::Test
{

TestScaleFigure::TestScaleFigure(const Rectangle2D& subject, FigureView view, QWidget* parent)
  : ScaleFigure(kDotsPerInch, parent), m_subject(subject), m_view(view)
{
    updateFigure();
}

void TestScaleFigure::setSubject(const Rectangle2D& subject)
{
    m_subject = subject;
    updateFigure();
    fireChangeEvent();
}

void TestScaleFigure::setView(FigureView view)
{
    m_view = view;
    updateFigure();
    fireChangeEvent();
}

QPoint TestScaleFigure::autoZoomPoint() const
{
    return {std::max(0, subjectOrigin().x() - kBorderWidth), std::max(0, -kBorderHeight)};
}

void TestScaleFigure::updateSubjectDimensions()
{
    setSubjectBounds(m_subject, m_subject);
}

void TestScaleFigure::updateCanvasOrigin()
{
    setSubjectOrigin(viewOrigin(m_view, size(), subjectBounds(), absoluteScale()));
}

}  // namespace QtRocket::Gui::Test
