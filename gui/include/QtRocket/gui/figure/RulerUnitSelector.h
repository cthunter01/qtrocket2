#pragma once

#include <QObject>

#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/util/Signal.h"

namespace QtRocket::Gui
{

class ScaleScrollArea;

/// The unit selector in the corner between the rulers of a ScaleScrollArea (OpenRocket's
/// ScaleScrollPane: a UnitSelector of a DoubleModel(0, UNITS_LENGTH), whose current unit is the
/// rulers' unit, in the UPPER_LEFT_CORNER, font size 8). Choosing a length unit sets the
/// rulers' unit (ScaleScrollArea::setRulerUnit()); a new unit of the rulers (updateRulerUnit()
/// after the preferences changed) shows in the selector.
///
/// It belongs to its scroll area: install() makes it and puts it in the area's corner
/// (ScaleScrollArea::setRulerCornerWidget(), in place of the area's unit label).
class RulerUnitSelector final : public UnitSelector
{
    Q_OBJECT

public:
    /// Makes the selector of @p area's rulers and puts it in the area's corner.
    static RulerUnitSelector* install(ScaleScrollArea& area);

    /// The selector of @p area's rulers, a child of @p area (install() puts it in the corner).
    explicit RulerUnitSelector(ScaleScrollArea* area);
    ~RulerUnitSelector() override;
    RulerUnitSelector(const RulerUnitSelector&)            = delete;
    RulerUnitSelector& operator=(const RulerUnitSelector&) = delete;
    RulerUnitSelector(RulerUnitSelector&&)                 = delete;
    RulerUnitSelector& operator=(RulerUnitSelector&&)      = delete;

private:
    /// The binding's unit changed (the user's choice): the rulers take it.
    void unitChosen();
    /// The rulers' unit changed: the selector shows it.
    void rulerUnitChanged();

    ScaleScrollArea*           m_area;
    Signal<>::ScopedConnection m_connection;
};

}  // namespace QtRocket::Gui
