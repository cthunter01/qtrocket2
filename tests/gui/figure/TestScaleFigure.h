#pragma once

#include <QPoint>
#include <QWidget>

#include "QtRocket/figure/FigureView.h"
#include "QtRocket/gui/figure/ScaleFigure.h"
#include "QtRocket/util/Geometry2D.h"

namespace QtRocket::Gui::Test
{

/// A ScaleFigure that shows a subject the test sets, as the design view shows a rocket
/// (RocketFigure's origin, ScaleFigure::viewOrigin(), and its auto-zoom point), at 96 dots per
/// inch, Java's resolution when headless: the scale figure's numbers can be compared with
/// OpenRocket's RocketFigure for the subject bounds it had. The subject is also the content, as
/// in RocketFigure.
class TestScaleFigure final : public ScaleFigure
{
public:
    /// GUIUtil.getDPI() when headless.
    static constexpr double kDotsPerInch = 96.0;

    TestScaleFigure(const Rectangle2D& subject, FigureView view, QWidget* parent);

    /// Shows @p subject: updateFigure() and changed(), as RocketFigure does when the rocket
    /// changes and it is told.
    void setSubject(const Rectangle2D& subject);
    /// Sets the subject the next updateSubjectDimensions() takes, without telling anyone (the
    /// rocket changed, nobody updated the figure yet).
    void setSubjectQuietly(const Rectangle2D& subject) noexcept { m_subject = subject; }
    /// Shows @p view: updateFigure() and changed() (RocketFigure.setType()).
    void                     setView(FigureView view);
    [[nodiscard]] FigureView view() const noexcept { return m_view; }

    /// RocketFigure.getAutoZoomPoint(): (max(0, origin x - 30), max(0, -20)).
    [[nodiscard]] QPoint autoZoomPoint() const override;

protected:
    void updateSubjectDimensions() override;
    void updateCanvasOrigin() override;

private:
    Rectangle2D m_subject;
    FigureView  m_view;
};

}  // namespace QtRocket::Gui::Test
