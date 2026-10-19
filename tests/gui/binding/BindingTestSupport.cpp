#include "BindingTestSupport.h"

#include <memory>

#include "QtRocket/gui/binding/PropertySource.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"

namespace QtRocket::Gui::Test
{

TwoTubeRocket::TwoTubeRocket()
{
    stage = &rocket->addChild(std::make_unique<AxialStage>());
    nose  = &stage->addChild(std::make_unique<NoseCone>());
    tubeA = &stage->addChild(std::make_unique<BodyTube>());
    tubeB = &stage->addChild(std::make_unique<BodyTube>());
    tubeA->setName("A");
    tubeB->setName("B");
    tubeA->setOuterRadiusAutomatic(false);
    tubeA->setOuterRadius(0.02);
    tubeA->setThickness(0.001);
    tubeA->setLength(0.3);
    tubeB->setOuterRadiusAutomatic(false);
    tubeB->setOuterRadius(0.03);
    tubeB->setThickness(0.002);
    tubeB->setLength(0.5);
    rocket->enableEvents();
}

RocketEventRecorder::RocketEventRecorder(Rocket& rocket)
  : m_connection(rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
        m_types.push_back(event.getType());
        m_sources.push_back(event.getSource()->getName());
    }))
{
}

void RocketEventRecorder::clear()
{
    m_types.clear();
    m_sources.clear();
}

PropertySource<double> BeanProperty::source()
{
    return PropertySource<double>::of<RocketComponent>({
        .get =
            [this](const RocketComponent&) {
                ++gets;
                return value;
            },
        .set =
            [this](RocketComponent& component, double newValue) {
                ++sets;
                value = newValue;
                component.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
            },
        .isAutomatic = [this](const RocketComponent&) { return automatic; },
        .setAutomatic =
            [this](RocketComponent& component, bool newAutomatic) {
                automatic = newAutomatic;
                component.fireComponentChangeEvent(ComponentChangeEvent::kNonFunctionalChange);
            },
    });
}

PropertySource<double> bodyTubeLength()
{
    return PropertySource<double>::of<BodyTube>({
        .get = [](const BodyTube& tube) { return tube.getLength(); },
        .set = [](BodyTube& tube, double length) { tube.setLength(length); },
    });
}

PropertySource<double> bodyTubeOuterRadius()
{
    return PropertySource<double>::of<BodyTube>({
        .get          = [](const BodyTube& tube) { return tube.getOuterRadius(); },
        .set          = [](BodyTube& tube, double radius) { tube.setOuterRadius(radius); },
        .isAutomatic  = [](const BodyTube& tube) { return tube.isOuterRadiusAutomatic(); },
        .setAutomatic = [](BodyTube& tube, bool on) { tube.setOuterRadiusAutomatic(on); },
    });
}

PropertySource<double> bodyTubeInnerRadius()
{
    return PropertySource<double>::of<BodyTube>({
        .get = [](const BodyTube& tube) { return tube.getInnerRadius(); },
        .set = [](BodyTube& tube, double radius) { tube.setInnerRadius(radius); },
    });
}

}  // namespace QtRocket::Gui::Test
