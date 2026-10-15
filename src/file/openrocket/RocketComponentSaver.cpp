#include "QtRocket/file/openrocket/RocketComponentSaver.h"

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialGroup.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/Clusterable.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/InsideColorComponent.h"
#include "QtRocket/rocket/InsideColorComponentHandler.h"
#include "QtRocket/rocket/Instanceable.h"
#include "QtRocket/rocket/LineInstanceable.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetReference.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

using Elements = RocketComponentSaver::Elements;

/// Java's OpenRocketSaver.INDENT.
constexpr std::string_view kIndent = "  ";

/// The <preset> element of @p component: its preset's (Java), or the reference of an unresolved
/// one it keeps (QtRocket, decision U8), or none.
[[nodiscard]] std::optional<std::string> presetElement(const RocketComponent& component,
                                                       OrkWriter&             writer)
{
    if (const ComponentPreset* const preset = component.getPresetComponent())
    {
        return std::format(R"(<preset type="{}" manufacturer="{}" partno="{}" digest="{}"/>)",
                           componentPresetTypeName(preset->getType()),
                           writer.escape(preset->getManufacturer().getSimpleName()),
                           writer.escape(preset->getPartNo()), writer.escape(preset->getDigest()));
    }
    const std::optional<ComponentPresetReference>& reference = component.getUnresolvedPreset();
    if (!reference.has_value())
    {
        return std::nullopt;
    }
    std::string element = "<preset";
    if (reference->type.has_value())
    {
        element += std::format(R"( type="{}")", writer.escape(*reference->type));
    }
    element +=
        std::format(R"( manufacturer="{}" partno="{}")", writer.escape(reference->manufacturer),
                    writer.escape(reference->partNo));
    if (reference->digest.has_value())
    {
        element += std::format(R"( digest="{}")", writer.escape(*reference->digest));
    }
    return element + "/>";
}

/// Java's emitColor(): the colour as attributes of an empty element, indented @p indents
/// levels; nothing without a colour.
void emitColor(std::string_view elementName, Elements& elements, const std::optional<Color>& color,
               int indents)
{
    if (color.has_value())
    {
        std::string line;
        for (int i = 0; i < indents; ++i)
        {
            line += kIndent;
        }
        elements.push_back(line + std::format("<{} {}/>", elementName, color->toXmlAttributes()));
    }
}

/// The attributes of a decal's point: x="X" y="Y" (Java: "x=\"" + getX() + "\" y=\"" + getY()).
[[nodiscard]] std::string pointAttributes(const Coordinate& point)
{
    return std::format(R"(x="{}" y="{}")", Strings::javaDoubleToString(point.x),
                       Strings::javaDoubleToString(point.y));
}

/// Java's buildAppearanceElements(): the lines inside an <appearance> or <insideappearance>.
void buildAppearanceElements(Elements& elements, const Appearance& appearance, OrkWriter& writer)
{
    emitColor("paint", elements, appearance.getPaint(), 1);
    elements.push_back(std::string(kIndent) +
                       RocketComponentSaver::doubleElement("shine", appearance.getShine()));
    elements.push_back(std::string(kIndent) +
                       RocketComponentSaver::booleanElement("opacityaffectstexture",
                                                            appearance.isOpacityAffectsTexture()));
    const std::optional<Decal>& decal = appearance.getTexture();
    if (!decal.has_value())
    {
        return;
    }
    const std::string name     = writer.decalName(decal->getImageName());
    const double      rotation = decal->getRotation();
    elements.push_back(std::format(R"({}<decal name="{}" rotation="{}" edgemode="{}">)", kIndent,
                                   name, Strings::javaDoubleToString(rotation),
                                   edgeModeName(decal->getEdgeMode())));
    elements.push_back(
        std::format("{0}{0}<center {1}/>", kIndent, pointAttributes(decal->getCenter())));
    elements.push_back(
        std::format("{0}{0}<offset {1}/>", kIndent, pointAttributes(decal->getOffset())));
    elements.push_back(
        std::format("{0}{0}<scale {1}/>", kIndent, pointAttributes(decal->getScale())));
    elements.push_back(std::format("{}</decal>", kIndent));
}

/// The name, the id, the preset and the appearances (outside and inside).
void addIdentityAndAppearance(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    elements.push_back(RocketComponentSaver::textElement("name", c.getName(), writer));
    elements.push_back(RocketComponentSaver::textElement("id", c.getId().toString(), writer));

    if (std::optional<std::string> preset = presetElement(c, writer))
    {
        elements.push_back(std::move(*preset));
    }

    // Save outside appearance
    if (const std::optional<Appearance>& ap = c.getAppearance())
    {
        elements.emplace_back("<appearance>");
        buildAppearanceElements(elements, *ap, writer);
        elements.emplace_back("</appearance>");
    }

    // Save inside appearance
    if (const auto* const inside = dynamic_cast<const InsideColorComponent*>(&c))
    {
        const InsideColorComponentHandler& handler = inside->getInsideColorComponentHandler();
        if (const std::optional<Appearance>& apIn = handler.getInsideAppearance())
        {
            elements.emplace_back("<insideappearance>");
            elements.push_back(std::string(kIndent) +
                               RocketComponentSaver::booleanElement("edgessameasinside",
                                                                    handler.isEdgesSameAsInside()));
            elements.push_back(std::string(kIndent) +
                               RocketComponentSaver::booleanElement(
                                   "insidesameasoutside", handler.isSeparateInsideOutside()));
            buildAppearanceElements(elements, *apIn, writer);
            elements.emplace_back("</insideappearance>");
        }
    }
}

/// The colour and the line style, of a component that is no assembly; the instance counts.
void addColorAndInstances(const RocketComponent& c, Elements& elements)
{
    // Save color and line style if significant
    if (!isAssembly(c.kind()))
    {
        emitColor("color", elements, c.getColor(), 0);

        if (const std::optional<LineStyle> style = c.getLineStyle())
        {
            // Type names currently equivalent to the enum names except for case.
            elements.push_back(RocketComponentSaver::element("linestyle", toString(*style)));
        }
    }

    // Java first reads getInstanceCount() into a local it never uses; the getter is pure.
    if (dynamic_cast<const Instanceable*>(&c) != nullptr)
    {
        if (dynamic_cast<const Clusterable*>(&c) == nullptr)
        {
            // (A Clusterable sets its instance count through its cluster configuration.)
            elements.push_back(
                RocketComponentSaver::integerElement("instancecount", c.getInstanceCount()));
            // TODO: delete this when no backward compatibility with OR 15.03 is needed anymore
            if (isFinSet(c.kind()) || c.kind() == ComponentKind::TUBE_FIN_SET)
            {
                elements.push_back(
                    RocketComponentSaver::integerElement("fincount", c.getInstanceCount()));
            }
        }

        if (const auto* const line = dynamic_cast<const LineInstanceable*>(&c))
        {
            elements.push_back(RocketComponentSaver::doubleElement("instanceseparation",
                                                                   line->getInstanceSeparation()));
        }
    }
}

/// The radial, angular and axial positions.
void addPositions(const RocketComponent& c, Elements& elements)
{
    if (const auto* const radPos = dynamic_cast<const RadiusPositionable*>(&c))
    {
        // The type names are currently equivalent to the enum names except for case.
        const std::string_view radiusMethod = orkName(radPos->getRadiusMethod());
        const double           radiusOffset = radPos->getRadiusOffset();
        elements.push_back(std::format(R"(<radiusoffset method="{}">{}</radiusoffset>)",
                                       radiusMethod, Strings::javaDoubleToString(radiusOffset)));
    }
    if (const auto* const anglePos = dynamic_cast<const AnglePositionable*>(&c))
    {
        // The type names are currently equivalent to the enum names except for case.
        const std::string_view angleMethod = orkName(anglePos->getAngleMethod());
        // Java: angleOffset * 180.0 / Math.PI
        const std::string angleOffset =
            Strings::javaDoubleToString(MathUtil::rad2deg(anglePos->getAngleOffset()));
        elements.push_back(
            std::format(R"(<angleoffset method="{}">{}</angleoffset>)", angleMethod, angleOffset));
        // TODO: delete this when no backward compatibility with OR 15.03 is needed anymore
        const ComponentKind kind = c.kind();
        if (isFinSet(kind) || kind == ComponentKind::TUBE_FIN_SET)
        {
            elements.push_back(RocketComponentSaver::element("rotation", angleOffset));
        }
        else if (kind != ComponentKind::PARALLEL_STAGE && kind != ComponentKind::POD_SET &&
                 kind != ComponentKind::RAIL_BUTTON)
        {
            elements.push_back(RocketComponentSaver::element("radialdirection", angleOffset));
        }
    }

    // Save position unless "AFTER"
    if (c.getAxialMethod() != AxialMethod::AFTER)
    {
        // The type names are currently equivalent to the enum names except for case.
        const std::string_view axialMethod = orkName(c.getAxialMethod());
        elements.push_back(std::format(R"(<axialoffset method="{}">{}</axialoffset>)", axialMethod,
                                       Strings::javaDoubleToString(c.getAxialOffset())));
        // TODO: delete this when no backward compatibility with OR 15.03 is needed anymore
        elements.push_back(std::format(R"(<position type="{}">{}</position>)", axialMethod,
                                       Strings::javaDoubleToString(c.getAxialOffset())));
    }
}

/// The overrides and the comment.
void addOverridesAndComment(const RocketComponent& c, OrkWriter& writer, Elements& elements)
{
    // Overrides
    if (c.isMassOverridden())
    {
        elements.push_back(
            RocketComponentSaver::doubleElement("overridemass", c.getOverrideMass()));
        elements.push_back(RocketComponentSaver::booleanElement("overridesubcomponentsmass",
                                                                c.isSubcomponentsOverriddenMass()));
    }
    if (c.isCGOverridden())
    {
        elements.push_back(RocketComponentSaver::doubleElement("overridecg", c.getOverrideCGX()));
        elements.push_back(RocketComponentSaver::booleanElement("overridesubcomponentscg",
                                                                c.isSubcomponentsOverriddenCG()));
    }
    if (c.isCDOverridden())
    {
        elements.push_back(RocketComponentSaver::doubleElement("overridecd", c.getOverrideCD()));
        elements.push_back(RocketComponentSaver::booleanElement("overridesubcomponentscd",
                                                                c.isSubcomponentsOverriddenCD()));
    }

    // Comment
    if (!c.getComment().empty())
    {
        elements.push_back(RocketComponentSaver::textElement("comment", c.getComment(), writer));
    }
}

/// The lines of the <motor> of @p motorInstance, the mount's configuration for @p fcid.
void addMotor(const FlightConfigurationId& fcid, const MotorConfiguration& motorInstance,
              OrkWriter& writer, Elements& elements)
{
    const Motor& motor = *motorInstance.getMotor();

    elements.push_back(std::format(R"(  <motor configid="{}">)", fcid.toString()));
    if (motor.getMotorType() != Motor::Type::UNKNOWN)
    {
        elements.push_back(std::format("    <type>{}</type>", orkName(motor.getMotorType())));
    }
    if (const auto* const m = dynamic_cast<const ThrustCurveMotor*>(&motor))
    {
        elements.push_back(std::format("    <manufacturer>{}</manufacturer>",
                                       writer.escape(m->getManufacturer().getSimpleName())));
        elements.push_back(std::format("    <digest>{}</digest>", writer.escape(m->getDigest())));
    }
    elements.push_back(
        std::format("    <designation>{}</designation>", writer.escape(motor.getDesignation())));
    elements.push_back(std::format("    <diameter>{}</diameter>",
                                   Strings::javaDoubleToString(motor.getDiameter())));
    elements.push_back(
        std::format("    <length>{}</length>", Strings::javaDoubleToString(motor.getLength())));

    // Motor delay
    if (motorInstance.getEjectionDelay() == Motor::kPluggedDelay)
    {
        elements.emplace_back("    <delay>none</delay>");
    }
    else
    {
        elements.push_back(
            std::format("    <delay>{}</delay>",
                        Strings::javaDoubleToString(motorInstance.getEjectionDelay())));
    }
    if (motorInstance.getNozzleExitDiameter() > 0)
    {
        elements.push_back(
            std::format("    <nozzleexitdiameter>{}</nozzleexitdiameter>",
                        Strings::javaDoubleToString(motorInstance.getNozzleExitDiameter())));
    }

    elements.emplace_back("  </motor>");
}

}  // namespace

void RocketComponentSaver::addParams(const RocketComponent& component, OrkWriter& writer,
                                     Elements& elements)
{
    addIdentityAndAppearance(component, writer, elements);
    addColorAndInstances(component, elements);
    addPositions(component, elements);
    addOverridesAndComment(component, writer, elements);
}

std::string RocketComponentSaver::materialParam(const Material& material, OrkWriter& writer)
{
    return materialParam("material", material, writer);
}

std::string RocketComponentSaver::materialParam(std::string_view tag, const Material& material,
                                                OrkWriter& writer)
{
    std::string str = std::format("<{}", tag);

    switch (material.getType())
    {
        case Material::Type::LINE:
            str += R"( type="line")";
            break;
        case Material::Type::SURFACE:
            str += R"( type="surface")";
            break;
        case Material::Type::BULK:
            str += R"( type="bulk")";
            break;
        case Material::Type::CUSTOM:
            bug(std::format("Unknown material type: {}", toString(material.getType())));
    }

    // Java: Translator.getBaseText("material", name); QtRocket's names are not translated.
    const std::string_view baseName = material.getName();

    std::string result =
        str + std::format(R"( density="{}")", Strings::javaDoubleToString(material.getDensity()));

    // Add shear modulus when defined or explicitly set for a user-defined material.
    const double shearModulus = material.getInPlaneShearModulus();
    if (shearModulus != 0.0 || material.isUserDefined())
    {
        result += std::format(R"( shearModulus="{}")", Strings::javaDoubleToString(shearModulus));
    }

    result += std::format(R"( group="{}">{}</{}>)", databaseString(material.getGroup()),
                          writer.escape(baseName), tag);
    return result;
}

RocketComponentSaver::Elements RocketComponentSaver::motorMountParams(const MotorMount& mount,
                                                                      OrkWriter&        writer)
{
    if (!mount.isMotorMount())
    {
        return {};
    }

    const Rocket& rkt = asComponent(mount).getRocket();

    Elements elements;

    const MotorConfiguration& defaultInstance = mount.getDefaultMotorConfig();

    elements.emplace_back("<motormount>");

    // NOTE:  Default config must be BEFORE overridden config for proper backward compatibility
    // later on
    elements.push_back(std::format("  <ignitionevent>{}</ignitionevent>",
                                   orkName(defaultInstance.getIgnitionEvent())));
    elements.push_back(
        std::format("  <ignitiondelay>{}</ignitiondelay>",
                    Strings::javaDoubleToString(defaultInstance.getIgnitionDelay())));
    elements.push_back(std::format("  <overhang>{}</overhang>",
                                   Strings::javaDoubleToString(mount.getMotorOverhang())));

    for (const FlightConfigurationId& fcid : rkt.getIds())
    {
        const MotorConfiguration& motorInstance = mount.getMotorConfig(fcid);
        // Nothing is stored if no motor loaded
        if (motorInstance.isEmpty())
        {
            continue;
        }
        addMotor(fcid, motorInstance, writer, elements);

        // i.e. if this has overridden parameters....
        if (!(motorInstance == defaultInstance))
        {
            elements.push_back(
                std::format(R"(  <ignitionconfiguration configid="{}">)", fcid.toString()));
            elements.push_back(std::format("    <ignitionevent>{}</ignitionevent>",
                                           orkName(motorInstance.getIgnitionEvent())));
            elements.push_back(
                std::format("    <ignitiondelay>{}</ignitiondelay>",
                            Strings::javaDoubleToString(motorInstance.getIgnitionDelay())));
            elements.emplace_back("  </ignitionconfiguration>");
        }
    }

    elements.emplace_back("</motormount>");

    return elements;
}

std::string RocketComponentSaver::element(std::string_view tag, std::string_view text)
{
    return std::format("<{}>{}</{}>", tag, text, tag);
}

std::string RocketComponentSaver::textElement(std::string_view tag, std::string_view text,
                                              OrkWriter& writer)
{
    return element(tag, writer.escape(text));
}

std::string RocketComponentSaver::doubleElement(std::string_view tag, double value)
{
    return element(tag, Strings::javaDoubleToString(value));
}

std::string RocketComponentSaver::booleanElement(std::string_view tag, bool value)
{
    return element(tag, value ? "true" : "false");
}

std::string RocketComponentSaver::integerElement(std::string_view tag, int value)
{
    return element(tag, std::format("{}", value));
}

}  // namespace QtRocket
