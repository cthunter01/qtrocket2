#include "QtRocket/file/openrocket/ExternalComponentSaver.h"

#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/RocketComponentSaver.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Elements = ExternalComponentSaver::Elements;
using Saver    = RocketComponentSaver;

/// "auto " followed by @p value when @p automatic, else the value (Java: "auto " + value).
[[nodiscard]] std::string autoValue(bool automatic, double value)
{
    return (automatic ? "auto " : "") + Strings::javaDoubleToString(value);
}

/// Java's BodyComponentSaver.addParams() and SymmetricComponentSaver.addParams().
void addSymmetricParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    ExternalComponentSaver::addParams(c, writer, elements);

    // Body components have a natural length, store it now
    elements.push_back(Saver::doubleElement("length", c.getLength()));

    const auto& comp = Saver::as<SymmetricComponent>(c);
    if (comp.isFilled())
    {
        elements.push_back(Saver::element("thickness", "filled"));
    }
    else
    {
        elements.push_back(Saver::doubleElement("thickness", comp.getThickness()));
    }
}

/// Java's TransitionSaver.addParams(), which handles nose cones as well.
void addTransitionParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    addSymmetricParams(c, writer, elements);
    const auto& trans = Saver::as<Transition>(c);

    const TransitionShape shape = trans.getShapeType();
    elements.push_back(Saver::element("shape", orkName(shape)));
    if (isClippable(shape))
    {
        elements.push_back(Saver::booleanElement("shapeclipped", trans.isClipped()));
    }
    if (usesParameter(shape))
    {
        elements.push_back(Saver::doubleElement("shapeparameter", trans.getShapeParameter()));
    }

    // Nose cones need other parameter saving, due to the isFlipped() parameter
    if (c.kind() == ComponentKind::NOSE_CONE)
    {
        return;
    }

    elements.push_back(Saver::element(
        "foreradius", autoValue(trans.isForeRadiusAutomatic(), trans.getForeRadius())));
    elements.push_back(
        Saver::element("aftradius", autoValue(trans.isAftRadiusAutomatic(), trans.getAftRadius())));

    elements.push_back(Saver::doubleElement("foreshoulderradius", trans.getForeShoulderRadius()));
    elements.push_back(Saver::doubleElement("foreshoulderlength", trans.getForeShoulderLength()));
    elements.push_back(
        Saver::doubleElement("foreshoulderthickness", trans.getForeShoulderThickness()));
    elements.push_back(Saver::booleanElement("foreshouldercapped", trans.isForeShoulderCapped()));

    elements.push_back(Saver::doubleElement("aftshoulderradius", trans.getAftShoulderRadius()));
    elements.push_back(Saver::doubleElement("aftshoulderlength", trans.getAftShoulderLength()));
    elements.push_back(
        Saver::doubleElement("aftshoulderthickness", trans.getAftShoulderThickness()));
    elements.push_back(Saver::booleanElement("aftshouldercapped", trans.isAftShoulderCapped()));
}

/// Java's NoseConeSaver.addParams().
void addNoseConeParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    const auto& noseCone = Saver::as<NoseCone>(c);
    addTransitionParams(c, writer, elements);

    elements.push_back(Saver::element(
        "aftradius", autoValue(noseCone.isBaseRadiusAutomatic(), noseCone.getBaseRadius())));

    elements.push_back(Saver::doubleElement("aftshoulderradius", noseCone.getShoulderRadius()));
    elements.push_back(Saver::doubleElement("aftshoulderlength", noseCone.getShoulderLength()));
    elements.push_back(
        Saver::doubleElement("aftshoulderthickness", noseCone.getShoulderThickness()));
    elements.push_back(Saver::booleanElement("aftshouldercapped", noseCone.isShoulderCapped()));

    elements.push_back(Saver::booleanElement("isflipped", noseCone.isFlipped()));
}

/// The legacy relativeto of a tab position (OpenRocket 15.03): "front" for TOP, "end" for
/// BOTTOM, else "center" (Java's switch gives MIDDLE and its default "center").
[[nodiscard]] std::string_view legacyTabPosition(AxialMethod method) noexcept
{
    if (method == AxialMethod::TOP)
    {
        return "front";
    }
    if (method == AxialMethod::BOTTOM)
    {
        return "end";
    }
    return "center";
}

/// Java's FinSetSaver.addParams() for the fin tab: written only when it has a height and a
/// length (compatibility with file version < 1.1).
void addFinTabParams(const FinSet& fins, Elements& elements)
{
    if (MathUtil::equals(fins.getTabHeight(), 0) || MathUtil::equals(fins.getTabLength(), 0))
    {
        return;
    }
    elements.push_back(Saver::doubleElement("tabheight", fins.getTabHeight()));
    elements.push_back(Saver::doubleElement("tablength", fins.getTabLength()));
    // TODO: delete this when no backward compatibility with OR 15.03 is needed anymore
    const double           offsetVal = fins.getTabOffset();
    const std::string_view offset    = legacyTabPosition(fins.getTabOffsetMethod());
    elements.push_back(std::format(R"(<tabposition relativeto="{}">{}</tabposition>)", offset,
                                   Strings::javaDoubleToString(offsetVal)));
    elements.push_back(std::format(R"(<tabposition relativeto="{}">{}</tabposition>)",
                                   orkName(fins.getTabOffsetMethod()),
                                   Strings::javaDoubleToString(fins.getTabOffset())));
}

/// Java's FinSetSaver.addParams().
void addFinSetParams(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    ExternalComponentSaver::addParams(c, writer, elements);

    const auto& fins = Saver::as<FinSet>(c);

    // (Java's comment: the fin count and the rotation are already saved as 'RingInstanceable'
    // in RocketComponentSaver.)

    elements.push_back(Saver::doubleElement("thickness", fins.getThickness()));
    elements.push_back(Saver::element("crosssection", orkName(fins.getCrossSection())));
    // Java: Math.toDegrees(), not angle * 180.0 / Math.PI as everywhere else.
    elements.push_back(Saver::doubleElement("cant", MathUtil::javaToDegrees(fins.getCantAngle())));

    // Save fin tabs only if they exist (compatibility with file version < 1.1)
    addFinTabParams(fins, elements);

    elements.push_back(Saver::doubleElement("filletradius", fins.getFilletRadius()));
    elements.push_back(Saver::materialParam("filletmaterial", fins.getFilletMaterial(), writer));
}

}  // namespace

void ExternalComponentSaver::addParams(const RocketComponent& component, OrkWriter& writer,
                                       Elements& elements)
{
    RocketComponentSaver::addParams(component, writer, elements);

    const auto& ext = Saver::as<ExternalComponent>(component);

    // Finish enum names are currently the same except for case
    elements.push_back(Saver::element("finish", orkName(ext.getFinish())));

    // Material
    elements.push_back(Saver::materialParam(ext.getMaterial(), writer));
}

ExternalComponentSaver::Elements ExternalComponentSaver::bodyTubeElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("bodytube", [&](Elements& elements) {
        addSymmetricParams(component, writer, elements);
        const auto& tube = Saver::as<BodyTube>(component);

        elements.push_back(Saver::element(
            "radius", autoValue(tube.isOuterRadiusAutomatic(), tube.getOuterRadius())));

        if (tube.isMotorMount())
        {
            for (std::string& line : Saver::motorMountParams(tube, writer))
            {
                elements.push_back(std::move(line));
            }
        }
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::transitionElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("transition", [&](Elements& elements) {
        addTransitionParams(component, writer, elements);
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::noseConeElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf(
        "nosecone", [&](Elements& elements) { addNoseConeParams(component, writer, elements); });
}

ExternalComponentSaver::Elements ExternalComponentSaver::trapezoidFinSetElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("trapezoidfinset", [&](Elements& elements) {
        addFinSetParams(component, writer, elements);

        const auto& fins = Saver::as<TrapezoidFinSet>(component);
        elements.push_back(Saver::doubleElement("rootchord", fins.getRootChord()));
        elements.push_back(Saver::doubleElement("tipchord", fins.getTipChord()));
        elements.push_back(Saver::doubleElement("sweeplength", fins.getSweep()));
        elements.push_back(Saver::doubleElement("height", fins.getHeight()));
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::ellipticalFinSetElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("ellipticalfinset", [&](Elements& elements) {
        addFinSetParams(component, writer, elements);

        const auto& fins = Saver::as<EllipticalFinSet>(component);
        elements.push_back(Saver::doubleElement("rootchord", fins.getLength()));
        elements.push_back(Saver::doubleElement("height", fins.getHeight()));
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::freeformFinSetElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("freeformfinset", [&](Elements& elements) {
        addFinSetParams(component, writer, elements);

        const auto& fins = Saver::as<FreeformFinSet>(component);
        elements.emplace_back("<finpoints>");
        for (const Coordinate& p : fins.getFinPoints())
        {
            elements.push_back(std::format(R"(  <point x="{}" y="{}"/>)",
                                           Strings::javaDoubleToString(p.x),
                                           Strings::javaDoubleToString(p.y)));
        }
        elements.emplace_back("</finpoints>");
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::tubeFinSetElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("tubefinset", [&](Elements& elements) {
        addParams(component, writer, elements);
        const auto& fins = Saver::as<TubeFinSet>(component);

        elements.push_back(Saver::integerElement("fincount", fins.getFinCount()));
        // Java: fins.getBaseRotation() * 180.0 / Math.PI
        elements.push_back(
            Saver::doubleElement("rotation", MathUtil::rad2deg(fins.getBaseRotation())));
        if (fins.isOuterRadiusAutomatic())
        {
            elements.push_back(Saver::element("radius", "auto"));
        }
        else
        {
            elements.push_back(Saver::doubleElement("radius", fins.getOuterRadius()));
        }
        elements.push_back(Saver::doubleElement("length", fins.getLength()));
        elements.push_back(Saver::doubleElement("thickness", fins.getThickness()));
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::launchLugElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("launchlug", [&](Elements& elements) {
        addParams(component, writer, elements);
        const auto& lug = Saver::as<LaunchLug>(component);

        elements.push_back(Saver::doubleElement("radius", lug.getOuterRadius()));
        elements.push_back(Saver::doubleElement("length", lug.getLength()));
        elements.push_back(Saver::doubleElement("thickness", lug.getThickness()));
    });
}

ExternalComponentSaver::Elements ExternalComponentSaver::railButtonElements(
    const RocketComponent& component, OrkWriter& writer, const Preferences& /*preferences*/)
{
    return Saver::elementOf("railbutton", [&](Elements& elements) {
        addParams(component, writer, elements);
        const auto& rb = Saver::as<RailButton>(component);

        elements.push_back(Saver::doubleElement("outerdiameter", rb.getOuterDiameter()));
        elements.push_back(Saver::doubleElement("innerdiameter", rb.getInnerDiameter()));
        elements.push_back(Saver::doubleElement("height", rb.getTotalHeight()));
        elements.push_back(Saver::doubleElement("baseheight", rb.getBaseHeight()));
        elements.push_back(Saver::doubleElement("flangeheight", rb.getFlangeHeight()));
        elements.push_back(Saver::doubleElement("screwheight", rb.getScrewHeight()));
    });
}

}  // namespace QtRocket
