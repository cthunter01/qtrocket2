#include "QtRocket/figure/MassComponentShapes.h"

#include <cmath>
#include <random>
#include <utility>
#include <vector>

#include "MassObjectShapeSupport.h"
#include "ProviderComponent.h"
#include "QtRocket/figure/ComponentShape.h"
#include "QtRocket/figure/FigureArc.h"
#include "QtRocket/figure/FigureEllipse.h"
#include "QtRocket/figure/FigurePath.h"
#include "QtRocket/figure/FigureRect.h"
#include "QtRocket/figure/FigureRoundRect.h"
#include "QtRocket/figure/FigureShape.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Transformation.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::MassComponentShapes
{

namespace
{

using Detail::centerX;
using Detail::centerY;

void addAltimeterSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double vMargin        = bounds.height / 8.0;
    const double hMargin        = bounds.width / 2.25;
    const double halfArrowWidth = MathUtil::min(hMargin, vMargin);

    FigurePath symbol;
    symbol.moveTo(centerX(bounds), bounds.y + vMargin);
    symbol.lineTo(centerX(bounds), bounds.y + (7 * vMargin));
    symbol.lineTo(centerX(bounds) - halfArrowWidth, bounds.y + (6 * vMargin));
    symbol.lineTo(centerX(bounds) + halfArrowWidth, bounds.y + (6 * vMargin));
    symbol.lineTo(centerX(bounds), bounds.y + (7 * vMargin));

    shapes.emplace_back(std::move(symbol));
}

void addFlightComputerSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    constexpr int kPins = 12;

    const double vMargin    = bounds.height / 8.0;
    const double hMargin    = bounds.width / 6.0;
    const double pinHeight  = vMargin;
    const double pinSpacing = (bounds.width - (2 * hMargin)) / (kPins + 1);
    const double pinWidth   = pinSpacing / 2;

    shapes.emplace_back(
        FigureRect{bounds.x + hMargin, bounds.y + (2 * vMargin), 4 * hMargin, 4 * vMargin});
    // Java fills the upper row at offset + 1 + i and the lower row at offset + 1 + 6 + i.
    std::vector<FigureShape> lower;
    for (int i = 0; i < (kPins / 2); i++)
    {
        const double x = bounds.x + hMargin + (2 * i * pinSpacing) + pinSpacing;
        shapes.emplace_back(FigureRect{x, bounds.y + (6 * vMargin), pinWidth, pinHeight});
        lower.emplace_back(FigureRect{x, bounds.y + vMargin, pinWidth, pinHeight});
    }
    for (FigureShape& pin : lower)
    {
        shapes.push_back(std::move(pin));
    }
}

void addTrackerSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    double vMargin = bounds.width / 10;

    const double xCenter = centerX(bounds);
    const double yCenter = centerY(bounds);

    const double arcExtent = 60.0;
    const double arcStart1 = 360 - (arcExtent / 2);
    const double arcStart2 = 180 - (arcExtent / 2);

    if (3 * vMargin * std::sin(MathUtil::javaToRadians(arcExtent / 2)) > 0.9 * bounds.height / 2)
    {
        vMargin = 0.9 * bounds.height / (6 * std::sin(MathUtil::javaToRadians(arcExtent / 2)));
    }
    shapes.emplace_back(
        FigureEllipse{xCenter - (vMargin / 2), yCenter - (vMargin / 2), vMargin, vMargin});
    std::vector<FigureShape> left;
    for (int i = 1; i < 4; i++)
    {
        const double x    = xCenter - (i * vMargin);
        const double y    = yCenter - (i * vMargin);
        const double size = 2 * i * vMargin;
        shapes.emplace_back(
            FigureArc{x, y, size, size, arcStart1, arcExtent, FigureArc::Type::OPEN});
        left.emplace_back(FigureArc{x, y, size, size, arcStart2, arcExtent, FigureArc::Type::OPEN});
    }
    for (FigureShape& arc : left)
    {
        shapes.push_back(std::move(arc));
    }
}

void addPayloadSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double vMargin = bounds.height / 10;
    const double hMargin = bounds.width / 10;

    shapes.emplace_back(FigureEllipse{bounds.x + hMargin, bounds.y + vMargin,
                                      bounds.width - (2 * hMargin), bounds.height - (2 * vMargin)});
}

void addRecoveryHardwareSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double vMargin = bounds.height / 8;
    const double hMargin = bounds.width / 8;

    shapes.emplace_back(FigureRoundRect{bounds.x + hMargin, bounds.y + vMargin,
                                        bounds.width - (2 * hMargin), bounds.height - (2 * vMargin),
                                        15, 5});
    shapes.emplace_back(FigureRoundRect{bounds.x + hMargin + vMargin, bounds.y + (2 * vMargin),
                                        bounds.width - (2 * hMargin) - (2 * vMargin),
                                        bounds.height - (4 * vMargin), 15, 5});
    shapes.emplace_back(FigureRect{centerX(bounds) - (1.5 * hMargin),
                                   centerY(bounds) + (1.5 * vMargin), 3 * hMargin, 2 * vMargin});
}

/// The next draw of @p engine in [0, 1): its upper 53 bits times 2^-53.
[[nodiscard]] double nextUnitDouble(std::mt19937_64& engine)
{
    return static_cast<double>(engine() >> 11U) * 0x1.0p-53;
}

void addDeploymentChargeSymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds,
                               const RocketComponent& component)
{
    const double xCenter = centerX(bounds);
    const double yCenter = centerY(bounds);
    // Java: new Random(), unseeded; here seeded by the component's id (see the header).
    const Uuid&     id = component.getId();
    std::mt19937_64 generator{id.mostSignificantBits() ^ id.leastSignificantBits()};

    // Make arc size proportional to component size
    const double arcRadius = MathUtil::javaMin(bounds.width, bounds.height) / 4.0;
    shapes.emplace_back(FigureArc{xCenter - arcRadius, yCenter - arcRadius, 2 * arcRadius,
                                  2 * arcRadius, 55.0, 180.0, FigureArc::Type::CHORD});

    FigurePath explosion;

    // Scale rays proportionally to component size
    const double maxRayLength = MathUtil::javaMin(bounds.width, bounds.height) / 4.0;

    for (int i = 0; i < kDeploymentChargeRays; i++)
    {
        // Two statements: Java draws rx before ry.
        const double rx = nextUnitDouble(generator) * maxRayLength;
        const double ry = nextUnitDouble(generator) * maxRayLength;
        explosion.moveTo(xCenter, yCenter);
        explosion.lineTo(xCenter + rx, yCenter + ry);
    }
    shapes.emplace_back(std::move(explosion));
}

void addBatterySymbol(std::vector<FigureShape>& shapes, const Rectangle2D& bounds)
{
    const double vMargin    = bounds.height / 8.0;
    const double hMargin    = bounds.width / 3.0;
    const double cellWidth  = hMargin / 3.0;
    const double cellTop    = bounds.y + (7 * vMargin);
    const double cellBottom = bounds.y + vMargin;

    FigurePath symbol;
    symbol.moveTo(bounds.x + hMargin, centerY(bounds));
    symbol.lineTo(bounds.x + (2 * hMargin / 3), centerY(bounds));
    // Java: for (Double x = ...; x < ...; x += cellWidth), which never ends when the cell width
    // no longer moves x.
    double x = bounds.x + hMargin;
    while (x < bounds.x + (2 * hMargin))
    {
        symbol.moveTo(x, cellTop);
        symbol.lineTo(x, cellBottom);
        symbol.moveTo(x + (cellWidth / 2.0), cellBottom + vMargin);
        symbol.lineTo(x + (cellWidth / 2.0), cellTop - vMargin);
        const double next = x + cellWidth;
        if (!(next > x))
        {
            break;
        }
        x = next;
    }
    symbol.moveTo(bounds.x + bounds.width - (2 * hMargin / 3) - (cellWidth / 2), centerY(bounds));
    symbol.lineTo(bounds.x + (2 * hMargin) - (cellWidth / 2), centerY(bounds));

    shapes.emplace_back(std::move(symbol));
}

}  // namespace

std::vector<ComponentShape> getShapesSide(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj =
        Detail::providerComponent<MassComponent>(component, "MassComponentShapes");

    const FigureRoundRect    base   = Detail::massObjectSide(massObj, transformation);
    const Rectangle2D        bounds = base.bounds();
    std::vector<FigureShape> s{FigureShape{base}};

    switch (massObj.getMassComponentType())
    {
        case MassComponent::MassComponentType::ALTIMETER:
            addAltimeterSymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::FLIGHTCOMPUTER:
            addFlightComputerSymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::DEPLOYMENTCHARGE:
            addDeploymentChargeSymbol(s, bounds, component);
            break;
        case MassComponent::MassComponentType::RECOVERYHARDWARE:
            addRecoveryHardwareSymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::PAYLOAD:
            addPayloadSymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::TRACKER:
            addTrackerSymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::BATTERY:
            addBatterySymbol(s, bounds);
            break;
        case MassComponent::MassComponentType::MASSCOMPONENT:
            break;
    }
    return ComponentShape::fromShapes(std::move(s), component);
}

std::vector<ComponentShape> getShapesBack(const RocketComponent& component,
                                          const Transformation&  transformation)
{
    const auto& massObj = Detail::providerComponent<MassObject>(component, "MassComponentShapes");

    return ComponentShape::fromShapes(
        {FigureShape{Detail::massObjectBack(massObj, transformation)}}, component);
}

}  // namespace QtRocket::MassComponentShapes
