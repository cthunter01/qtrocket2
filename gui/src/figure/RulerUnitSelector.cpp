#include "QtRocket/gui/figure/RulerUnitSelector.h"

#include <memory>

#include <QFont>
#include <QObject>
#include <QString>

#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/figure/ScaleScrollArea.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket::Gui
{

RulerUnitSelector* RulerUnitSelector::install(ScaleScrollArea& area)
{
    auto* selector = makeChild<RulerUnitSelector>(&area);
    area.setRulerCornerWidget(selector);
    return selector;
}

RulerUnitSelector::RulerUnitSelector(ScaleScrollArea* area)
  : UnitSelector(std::make_shared<DoubleBinding>(0.0, unitGroup(UnitGroupId::LENGTH)), area),
    m_area(area)
{
    QTROCKET_ASSERT(area != nullptr);
    setObjectName(QStringLiteral("rulerUnitSelector"));
    // GUIUtil.createUIFont(UI_FONT_STYLE_REGULAR, 8.0f): the size of the area's unit label.
    QFont small = font();
    small.setPixelSize(ScaleScrollArea::kCornerFontPixelSize);
    setFont(small);
    binding()->setCurrentUnit(area->rulerUnit());
    m_connection = binding()->addChangeListener(
        [this] { Guard::run([this] { unitChosen(); }, QStringLiteral("Ruler unit")); });
    connect(area, &ScaleScrollArea::rulerUnitChanged, this,
            guarded([this] { rulerUnitChanged(); }, QStringLiteral("Ruler unit")));
}

RulerUnitSelector::~RulerUnitSelector() = default;

void RulerUnitSelector::unitChosen()
{
    m_area->setRulerUnit(binding()->getCurrentUnit());
}

void RulerUnitSelector::rulerUnitChanged()
{
    binding()->setCurrentUnit(m_area->rulerUnit());
}

}  // namespace QtRocket::Gui
