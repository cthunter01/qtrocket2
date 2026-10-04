#pragma once

// What the tests of the internal components share: a rocket whose events they record (the ring
// component layers: InternalComponentTests.cpp, StructuralComponentTests.cpp,
// RingComponentTests.cpp and RadiusRingComponentTests.cpp), a rocket with one stage to put the
// parents in, and presets made by the factory.

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetFactory.h"
#include "QtRocket/rocket/preset/TypedPropertyMap.h"

namespace QtRocket::Test
{

/// A rocket with a stage holding a body tube (0.3 m long, outer radius 0.025 m, inner radius
/// 0.024 m); events enabled and recorded. Each test file derives a fixture of its own name from
/// it.
class RingEventsFixture : public ::testing::Test
{
protected:
    RingEventsFixture()
    {
        m_stage = &m_rocket.addChild(std::make_unique<AxialStage>());
        m_body  = &m_stage->addChild(std::make_unique<BodyTube>(0.3, 0.025, 0.001));
        m_rocket.enableEvents();
        m_connection = m_rocket.addComponentChangeListener(
            [this](const ComponentChangeEvent& e) { m_types.push_back(e.getType()); });
    }

    Rocket                                  m_rocket;
    AxialStage*                             m_stage{nullptr};
    BodyTube*                               m_body{nullptr};
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// A rocket with one stage; the test adds the bodies and enables the events.
struct OneStage
{
    Rocket      rocket;
    AxialStage* stage{&rocket.addChild(std::make_unique<AxialStage>())};
};

/// A preset made by the factory (with an empty material storage, as the Java tests' Databases
/// hold no material they would find).
[[nodiscard]] inline ComponentPreset makeFactoryPreset(const TypedPropertyMap& props)
{
    const MaterialStorage materials;
    return ComponentPresetFactory::create(props, materials).value();
}

}  // namespace QtRocket::Test
