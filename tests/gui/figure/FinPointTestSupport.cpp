#include "FinPointTestSupport.h"

#include <cstddef>
#include <format>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/figure/FinPointSelection.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/FloatingDecimal.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui::Test
{

FreeformDesign::FreeformDesign(const std::vector<Outline>& outlines)
{
    auto        rocket = std::make_unique<Rocket>();
    AxialStage& stage  = rocket->addChild(std::make_unique<AxialStage>());
    auto        tube   = std::make_unique<BodyTube>();
    tube->setOuterRadiusAutomatic(false);
    tube->setOuterRadius(0.021);
    tube->setLength(0.3);
    BodyTube& body = stage.addChild(std::move(tube));
    for (const Outline& outline : outlines)
    {
        auto fins = std::make_unique<FreeformFinSet>();
        if (!outline.empty())
        {
            std::vector<Coordinate> points;
            for (std::size_t i = 0; i + 1 < outline.size(); i += 2)
            {
                points.emplace_back(outline[i], outline[i + 1]);
            }
            fins->setPoints(points);
        }
        m_ids.push_back(body.addChild(std::move(fins)).getId());
    }
    rocket->enableEvents();
    m_document = OpenRocketDocumentFactory::createDocumentFromRocket(std::move(rocket));
    m_document->clearUndo();
}

FreeformDesign::~FreeformDesign() = default;

Rocket& FreeformDesign::rocket() const
{
    return m_document->getRocket();
}

FreeformFinSet& FreeformDesign::fins(std::size_t index) const
{
    auto* fins = dynamic_cast<FreeformFinSet*>(rocket().findComponent(m_ids.at(index)));
    if (fins == nullptr)
    {
        bug("The fin set is no longer in the design");
    }
    return *fins;
}

FinPointSelection FreeformDesign::selection(std::size_t                        main,
                                            std::initializer_list<std::size_t> others) const
{
    std::vector<Uuid> otherIds;
    for (const std::size_t other : others)
    {
        otherIds.push_back(m_ids.at(other));
    }
    return {rocket(), m_ids.at(main), std::move(otherIds)};
}

std::string pointsText(std::span<const Coordinate> points)
{
    std::string text;
    for (const Coordinate& c : points)
    {
        if (!text.empty())
        {
            text += ' ';
        }
        text += std::format("({},{})", FloatingDecimal::toJavaFormatString(c.x),
                            FloatingDecimal::toJavaFormatString(c.y));
    }
    return text;
}

std::string finPointsText(const FreeformFinSet& fins)
{
    return pointsText(fins.getFinPoints());
}

RocketEvents::RocketEvents(Rocket& rocket)
  : m_types(std::make_shared<std::vector<int>>()),
    m_connection(
        rocket.addComponentChangeListener([types = m_types](const ComponentChangeEvent& event) {
            types->push_back(event.getType());
        }))
{
}

}  // namespace QtRocket::Gui::Test
