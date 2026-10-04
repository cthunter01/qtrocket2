#include "goldens/GoldenBodies.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "QtRocket/material/Material.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "goldens/GoldenGeometry.h"

namespace QtRocket::Test
{

namespace
{

using nlohmann::json;

/// Sets a Transition's shoulders, wall and radii to the golden @p details. The shoulder setters
/// compare with MathUtil::equals, so each value is first moved away from the target.
void applyTransitionDetails(Transition& transition, const json& details)
{
    const std::optional<TransitionShape> shape =
        transitionShapeFromName(details.at("shapeType").get<std::string>());
    if (!shape)
    {
        ADD_FAILURE() << "unknown shape " << details.at("shapeType");
        return;
    }
    transition.setShapeType(*shape);
    transition.setShapeParameter(goldenValue(details.at("shapeParameter")));
    transition.setClipped(details.at("clipped").get<bool>());
    transition.setForeRadius(goldenValue(details.at("foreRadius")), false);
    transition.setAftRadius(goldenValue(details.at("aftRadius")), false);
    transition.setThickness(goldenValue(details.at("thickness")), false);
    transition.setFilled(details.at("filled").get<bool>());

    const double foreLength = goldenValue(details.at("foreShoulderLength"));
    if (foreLength != 0)
    {
        transition.setForeShoulderLength(foreLength);
    }
    transition.setForeShoulderRadius(-1, false);
    transition.setForeShoulderRadius(goldenValue(details.at("foreShoulderRadius")), false);
    transition.setForeShoulderThickness(-1);
    transition.setForeShoulderThickness(goldenValue(details.at("foreShoulderThickness")));
    transition.setForeShoulderCapped(details.at("foreShoulderCapped").get<bool>());

    const double aftLength = goldenValue(details.at("aftShoulderLength"));
    if (aftLength != 0)
    {
        transition.setAftShoulderLength(aftLength);
    }
    transition.setAftShoulderRadius(-1, false);
    transition.setAftShoulderRadius(goldenValue(details.at("aftShoulderRadius")), false);
    transition.setAftShoulderThickness(-1);
    transition.setAftShoulderThickness(goldenValue(details.at("aftShoulderThickness")));
    transition.setAftShoulderCapped(details.at("aftShoulderCapped").get<bool>());
}

}  // namespace

std::unique_ptr<SymmetricComponent> rebuildGoldenBody(const nlohmann::json& component)
{
    const std::string type    = component.at("type").get<std::string>();
    const json&       details = component.at("details");
    const double      length  = goldenValue(component.at("length"));

    std::unique_ptr<SymmetricComponent> result;
    if (type == "BodyTube")
    {
        const double radius = goldenValue(details.at("outerRadius"));
        if (details.at("filled").get<bool>())
        {
            result = std::make_unique<BodyTube>(length, radius, true);
        }
        else
        {
            result =
                std::make_unique<BodyTube>(length, radius, goldenValue(details.at("thickness")));
        }
    }
    else if (type == "NoseCone")
    {
        // A flipped nose cone (a tail cone) has its base at the front.
        const double fore    = goldenValue(details.at("foreRadius"));
        const double aft     = goldenValue(details.at("aftRadius"));
        const bool   flipped = fore > 0 && aft == 0;
        auto         nose =
            std::make_unique<NoseCone>(TransitionShape::CONICAL, length, flipped ? fore : aft);
        if (flipped)
        {
            nose->setFlipped(true, false);
        }
        applyTransitionDetails(*nose, details);
        result = std::move(nose);
    }
    else if (type == "Transition")
    {
        auto transition = std::make_unique<Transition>();
        transition->setLength(length);
        applyTransitionDetails(*transition, details);
        result = std::move(transition);
    }
    else
    {
        return nullptr;
    }

    const json& material = details.at("material");
    result->setMaterial(Material::newMaterial(Material::Type::BULK,
                                              material.at("name").get<std::string>(),
                                              goldenValue(material.at("density")), true));
    return result;
}

}  // namespace QtRocket::Test
