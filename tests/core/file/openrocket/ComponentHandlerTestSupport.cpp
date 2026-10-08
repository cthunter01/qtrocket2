#include "file/openrocket/ComponentHandlerTestSupport.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "QtRocket/file/GzipStream.h"
#include "QtRocket/file/ZipInputStream.h"
#include "QtRocket/file/openrocket/ComponentParameterHandler.h"
#include "QtRocket/logging/Warning.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/DesignType.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/FinSet.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/FreeformFinSet.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/LaunchLug.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorConfigurationSet.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/RadiusRingComponent.h"
#include "QtRocket/rocket/RailButton.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/ReferenceType.h"
#include "QtRocket/rocket/RingComponent.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/ShockCord.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/rocket/Streamer.h"
#include "QtRocket/rocket/StructuralComponent.h"
#include "QtRocket/rocket/Transition.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/TubeFinSet.h"
#include "QtRocket/rocket/position/AngleMethod.h"
#include "QtRocket/rocket/position/AnglePositionable.h"
#include "QtRocket/rocket/position/AxialMethod.h"
#include "QtRocket/rocket/position/RadiusMethod.h"
#include "QtRocket/rocket/position/RadiusPositionable.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/LineStyle.h"
#include "QtRocket/util/Signal.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "Sha256.h"
#include "file/openrocket/HandlerTestSupport.h"
#include "rocket/preset/ExamplePresets.h"

namespace QtRocket::Test
{

namespace
{

[[nodiscard]] std::string num(double value)
{
    return Strings::javaDoubleToString(value);
}

[[nodiscard]] std::string truth(bool value)
{
    return std::format("{}", value);
}

/// @p text in single quotes, a line break written as backslash n.
[[nodiscard]] std::string quote(std::string_view text)
{
    std::string quoted = "'";
    for (const char c : text)
    {
        if (c == '\n')
        {
            quoted += "\\n";
        }
        else
        {
            quoted += c;
        }
    }
    quoted += '\'';
    return quoted;
}

/// The texts joined by ':'. The elements of a braced list are computed in the order they are
/// written (the arguments of a function in any order, and GCC takes them from the last to the
/// first), which matters for getters that store what they compute: the probe calls its getters
/// from left to right.
[[nodiscard]] std::string inOrder(std::initializer_list<std::string> texts)
{
    std::string joined;
    for (const std::string& text : texts)
    {
        if (!joined.empty())
        {
            joined += ':';
        }
        joined += text;
    }
    return joined;
}

[[nodiscard]] std::string text(std::string_view value)
{
    return std::string(value);
}

[[nodiscard]] bool isIdChar(char c) noexcept
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F') || c == '-';
}

/// An id in full when the case's text spells it out or it is made of a hash (its upper half is
/// zero, as of the id OpenRocket makes of a text that is no UUID), else "random".
[[nodiscard]] std::string idText(const Uuid& id, const std::set<Uuid>& known)
{
    if (known.contains(id) || id.mostSignificantBits() == 0)
    {
        return id.toString();
    }
    return "random";
}

[[nodiscard]] std::string materialText(const Material& material)
{
    return "[" + material.toStorableString() + "]";
}

[[nodiscard]] std::string ignitionText(const MotorConfiguration& config)
{
    return inOrder({text(name(config.getIgnitionEvent())), num(config.getIgnitionDelay()),
                    truth(config.hasIgnitionOverride())});
}

[[nodiscard]] std::string deploymentText(const DeploymentConfiguration& config)
{
    return inOrder({text(deployEventName(config.getDeployEvent())), num(config.getDeployDelay()),
                    num(config.getDeployAltitude())});
}

[[nodiscard]] std::string separationText(const StageSeparationConfiguration& config)
{
    return inOrder({text(separationEventName(config.getSeparationEvent())),
                    num(config.getSeparationDelay()), num(config.getSeparationAltitude())});
}

// describeComponent() in parts. Each appends " key=value" for what a component of some class
// holds, in the order the probe calls the getters: a few of them store what they compute
// (MassObject::getRadius()), so the order is part of what is compared.

void describeBasics(const RocketComponent& c, const std::set<Uuid>& known, std::string& line)
{
    line += className(c.kind());
    line += ' ';
    line += quote(c.getName());
    if (known.contains(c.getId()))
    {
        line += " id=" + c.getId().toString();
    }
    line +=
        " axial=" + inOrder({text(axialMethodName(c.getAxialMethod())), num(c.getAxialOffset())});
    line += " x=" + num(c.getPosition().x);
    // Not of the rocket: its length is that of its bounds, and computing them reads every
    // component, in tree order here and in the order of a hash map in OpenRocket (see
    // InstanceMap). That order shows in what the automatic values store: a mass object with an
    // automatic radius adapts its stored length to the radius its parent has at that moment,
    // and a tube's automatic radius depends on which neighbour was asked before.
    if (c.kind() != ComponentKind::ROCKET)
    {
        line += " len=" + num(c.getLength());
    }
    if (!c.getComment().empty())
    {
        line += " comment=" + quote(c.getComment());
    }
    if (const std::optional<Color>& color = c.getColor(); color.has_value())
    {
        line += std::format(" color={},{},{},{}", color->red(), color->green(), color->blue(),
                            color->alpha());
    }
    if (const std::optional<LineStyle> style = c.getLineStyle(); style.has_value())
    {
        line += " linestyle=" + text(lineStyleName(*style));
    }
    if (const ComponentPreset* preset = c.getPresetComponent(); preset != nullptr)
    {
        line += " preset=" + preset->getPartNo();
    }
}

void describeOverrides(const RocketComponent& c, std::string& line)
{
    if (c.isMassOverridden())
    {
        line += " massovr=" + num(c.getOverrideMass());
    }
    if (c.isCGOverridden())
    {
        line += " cgovr=" + num(c.getOverrideCGX());
    }
    if (c.isCDOverridden())
    {
        line += " cdovr=" + num(c.getOverrideCD());
    }
    if (c.isSubcomponentsOverriddenMass() || c.isSubcomponentsOverriddenCG() ||
        c.isSubcomponentsOverriddenCD())
    {
        line += std::format(" subovr={},{},{}", c.isSubcomponentsOverriddenMass(),
                            c.isSubcomponentsOverriddenCG(), c.isSubcomponentsOverriddenCD());
    }
}

void describeRocketItself(const RocketComponent& c, std::string& line)
{
    const auto* const rocket = dynamic_cast<const Rocket*>(&c);
    if (rocket == nullptr)
    {
        return;
    }
    if (!rocket->getDesigner().empty())
    {
        line += " designer=" + quote(rocket->getDesigner());
    }
    if (!rocket->getRevision().empty())
    {
        line += " revision=" + quote(rocket->getRevision());
    }
    line += " ref=" + text(referenceTypeName(rocket->getReferenceType()));
    line += " customref=" + num(rocket->getCustomReferenceLength());
    line += " design=" + text(designTypeName(rocket->getDesignType()));
    if (!rocket->getKitName().empty())
    {
        line += " kit=" + quote(rocket->getKitName());
    }
}

void describeStage(const RocketComponent& c, const std::set<Uuid>& known, std::string& line)
{
    const auto* const stage = dynamic_cast<const AxialStage*>(&c);
    if (stage == nullptr)
    {
        return;
    }
    line += std::format(" stage={}", stage->getStageNumber());
    line += " sep=" + separationText(stage->getSeparationConfigurations().getDefault());
    for (const FlightConfigurationId& id : stage->getSeparationConfigurations().getIds())
    {
        line += std::format(" sep[{}]={}", idText(id.key(), known),
                            separationText(stage->getSeparationConfigurations().get(id)));
    }
}

void describePlacement(const RocketComponent& c, std::string& line)
{
    const ComponentKind kind = c.kind();
    if (kind == ComponentKind::PARALLEL_STAGE || kind == ComponentKind::POD_SET ||
        kind == ComponentKind::LAUNCH_LUG || kind == ComponentKind::RAIL_BUTTON ||
        dynamic_cast<const FinSet*>(&c) != nullptr || kind == ComponentKind::TUBE_FIN_SET ||
        dynamic_cast<const RadiusRingComponent*>(&c) != nullptr)
    {
        line += std::format(" inst={}", c.getInstanceCount());
    }
    if (const auto* const radius = dynamic_cast<const RadiusPositionable*>(&c))
    {
        line += " radius=" + inOrder({text(radiusMethodName(radius->getRadiusMethod())),
                                      num(radius->getRadiusOffset())});
    }
    if (const auto* const angle = dynamic_cast<const AnglePositionable*>(&c))
    {
        line += " angle=" + inOrder({text(angleMethodName(angle->getAngleMethod())),
                                     num(angle->getAngleOffset())});
    }
}

void describeMaterials(const RocketComponent& c, std::string& line)
{
    if (const auto* const external = dynamic_cast<const ExternalComponent*>(&c))
    {
        line += " finish=" + text(finishName(external->getFinish()));
        line += " mat=" + materialText(external->getMaterial());
    }
    if (const auto* const structural = dynamic_cast<const StructuralComponent*>(&c))
    {
        line += " mat=" + materialText(structural->getMaterial());
    }
}

void describeBody(const RocketComponent& c, std::string& line)
{
    if (const auto* const tube = dynamic_cast<const BodyTube*>(&c))
    {
        line +=
            " r=" + inOrder({num(tube->getOuterRadius()), truth(tube->isOuterRadiusAutomatic())});
        line += " thick=" + inOrder({num(tube->getThickness()), truth(tube->isFilled())});
    }
    if (const auto* const t = dynamic_cast<const Transition*>(&c))
    {
        line += " shape=" + inOrder({text(transitionShapeName(t->getShapeType())),
                                     num(t->getShapeParameter()), truth(t->isClipped())});
        line += " fore=" + inOrder({num(t->getForeRadius()), truth(t->isForeRadiusAutomatic())});
        line += " aft=" + inOrder({num(t->getAftRadius()), truth(t->isAftRadiusAutomatic())});
        line += " thick=" + inOrder({num(t->getThickness()), truth(t->isFilled())});
        line += " foresh=" +
                inOrder({num(t->getForeShoulderRadius()), num(t->getForeShoulderLength()),
                         num(t->getForeShoulderThickness()), truth(t->isForeShoulderCapped())});
        line += " aftsh=" +
                inOrder({num(t->getAftShoulderRadius()), num(t->getAftShoulderLength()),
                         num(t->getAftShoulderThickness()), truth(t->isAftShoulderCapped())});
    }
    if (const auto* const nose = dynamic_cast<const NoseCone*>(&c))
    {
        line += " flipped=" + truth(nose->isFlipped());
    }
}

void describeFins(const RocketComponent& c, std::string& line)
{
    if (const auto* const f = dynamic_cast<const FinSet*>(&c))
    {
        line += std::format(" fins={}", f->getFinCount());
        line += " cant=" + num(f->getCantAngle());
        line += " thick=" + num(f->getThickness());
        line += " cross=" + text(finCrossSectionName(f->getCrossSection()));
        line += " tab=" + inOrder({num(f->getTabHeight()), num(f->getTabLength()),
                                   text(axialMethodName(f->getTabOffsetMethod())),
                                   num(f->getTabOffset()), num(f->getTabFrontEdge())});
        line +=
            " fillet=" + inOrder({num(f->getFilletRadius()), materialText(f->getFilletMaterial())});
    }
    if (const auto* const f = dynamic_cast<const TrapezoidFinSet*>(&c))
    {
        line += " chord=" + inOrder({num(f->getRootChord()), num(f->getTipChord())});
        line += " sweep=" + num(f->getSweep());
        line += " height=" + num(f->getHeight());
    }
    if (const auto* const f = dynamic_cast<const EllipticalFinSet*>(&c))
    {
        line += " height=" + num(f->getHeight());
    }
    if (const auto* const f = dynamic_cast<const FreeformFinSet*>(&c))
    {
        line += " points=";
        bool first = true;
        for (const Coordinate& p : f->getFinPoints())
        {
            line += std::format("{}{},{}", first ? "" : ";", num(p.x), num(p.y));
            first = false;
        }
    }
    if (const auto* const t = dynamic_cast<const TubeFinSet*>(&c))
    {
        line += std::format(" fins={}", t->getFinCount());
        line += " autoradius=" + truth(t->isOuterRadiusAutomatic());
        line += " thick=" + num(t->getThickness());
    }
}

void describeRailGuides(const RocketComponent& c, std::string& line)
{
    if (const auto* const lug = dynamic_cast<const LaunchLug*>(&c))
    {
        line += " r=" + num(lug->getOuterRadius());
        line += " thick=" + num(lug->getThickness());
        line += " spacing=" + num(lug->getInstanceSeparation());
    }
    if (const auto* const b = dynamic_cast<const RailButton*>(&c))
    {
        line += " diameter=" + inOrder({num(b->getOuterDiameter()), num(b->getInnerDiameter())});
        line += " height=" + inOrder({num(b->getTotalHeight()), num(b->getBaseHeight()),
                                      num(b->getFlangeHeight()), num(b->getScrewHeight())});
        line += " spacing=" + num(b->getInstanceSeparation());
    }
}

void describeRings(const RocketComponent& c, std::string& line)
{
    if (const auto* const ring = dynamic_cast<const RingComponent*>(&c))
    {
        line += " outer=" +
                inOrder({num(ring->getOuterRadius()), truth(ring->isOuterRadiusAutomatic())});
        line += " inner=" +
                inOrder({num(ring->getInnerRadius()), truth(ring->isInnerRadiusAutomatic())});
        line +=
            " radial=" + inOrder({num(ring->getRadialPosition()), num(ring->getRadialDirection())});
    }
    if (const auto* const ring = dynamic_cast<const RadiusRingComponent*>(&c))
    {
        line += " spacing=" + num(ring->getInstanceSeparation());
    }
    if (const auto* const tube = dynamic_cast<const InnerTube*>(&c))
    {
        line +=
            " cluster=" + inOrder({text(tube->getClusterConfiguration().getXmlName()),
                                   num(tube->getClusterScale()), num(tube->getClusterRotation())});
    }
}

void describeMassObjects(const RocketComponent& c, std::string& line)
{
    if (const auto* const m = dynamic_cast<const MassObject*>(&c))
    {
        // In this order: getRadius() stores what getLength() reads.
        line += " packed=" +
                inOrder({num(m->getLength()), num(m->getRadius()), truth(m->isRadiusAutomatic())});
        line += " radial=" + inOrder({num(m->getRadialPosition()), num(m->getRadialDirection())});
    }
    if (const auto* const m = dynamic_cast<const MassComponent*>(&c))
    {
        line += " mass=" + num(m->getComponentMass());
        line += " type=" + text(massComponentTypeName(m->getMassComponentType()));
    }
    if (const auto* const cord = dynamic_cast<const ShockCord*>(&c))
    {
        line +=
            " cord=" + inOrder({num(cord->getCordLength()), truth(cord->isCordLengthAutomatic())});
        line += " mat=" + materialText(cord->getMaterial());
    }
}

void describeRecovery(const RocketComponent& c, const std::set<Uuid>& known, std::string& line)
{
    if (const auto* const d = dynamic_cast<const RecoveryDevice*>(&c))
    {
        line += " cd=" + inOrder({num(d->getCD()), truth(d->isCDAutomatic())});
        line += " drogue=" + truth(d->isDrogue());
        line += " mat=" + materialText(d->getMaterial());
        line += " deploy=" + deploymentText(d->getDeploymentConfigurations().getDefault());
        for (const FlightConfigurationId& id : d->getDeploymentConfigurations().getIds())
        {
            line += std::format(" deploy[{}]={}", idText(id.key(), known),
                                deploymentText(d->getDeploymentConfigurations().get(id)));
        }
    }
    if (const auto* const p = dynamic_cast<const Parachute*>(&c))
    {
        line += " diameter=" + num(p->getDiameter());
        line += " lines=" + inOrder({std::to_string(p->getLineCount()), num(p->getLineLength()),
                                     truth(p->isLineLengthAutomatic())});
        line += " linemat=" + materialText(p->getLineMaterial());
    }
    if (const auto* const s = dynamic_cast<const Streamer*>(&c))
    {
        line += " strip=" + inOrder({num(s->getStripLength()), num(s->getStripWidth())});
    }
}

void describeMount(const RocketComponent& c, const std::set<Uuid>& known, std::string& line)
{
    const auto* const mount = dynamic_cast<const MotorMount*>(&c);
    if (mount == nullptr)
    {
        return;
    }
    line += " mount=" + truth(mount->isMotorMount());
    line += " overhang=" + num(mount->getMotorOverhang());
    line += " ign=" + ignitionText(mount->getDefaultMotorConfig());
    for (const FlightConfigurationId& id : mount->getMotorConfigurationSet().getIds())
    {
        const MotorConfiguration& config = mount->getMotorConfig(id);
        line += std::format(" motor[{}]=", idText(id.key(), known));
        line += inOrder({config.getMotor() == nullptr ? std::string("none")
                                                      : config.getMotor()->getDesignation(),
                         num(config.getEjectionDelay()), num(config.getNozzleExitDiameter()),
                         ignitionText(config)});
    }
}

/// One line for a component: its Java class and its name, then "key=value" for what it holds.
[[nodiscard]] std::string describeComponent(const RocketComponent& c, const std::set<Uuid>& known)
{
    std::string line;
    describeBasics(c, known, line);
    describeOverrides(c, line);
    describeRocketItself(c, line);
    describeStage(c, known, line);
    describePlacement(c, line);
    describeMaterials(c, line);
    describeBody(c, line);
    describeFins(c, line);
    describeRailGuides(c, line);
    describeRings(c, line);
    describeMassObjects(c, line);
    describeRecovery(c, known, line);
    describeMount(c, known, line);
    return line;
}

void describeTree(const RocketComponent& c, std::size_t depth, const std::set<Uuid>& known,
                  std::vector<std::string>& lines)
{
    lines.push_back(std::string(2 * depth, ' ') + describeComponent(c, known));
    for (const RocketComponent* child : c.getChildren())
    {
        describeTree(*child, depth + 1, known, lines);
    }
}

/// The preloaded stage activeness as Java prints a sorted map: "{0=true, 1=false}", or "null".
[[nodiscard]] std::string preloadedText(const FlightConfiguration& config)
{
    const std::optional<std::map<int, bool>>& preloaded = config.getPreloadedStageActiveness();
    if (!preloaded.has_value())
    {
        return "null";
    }
    std::string text = "{";
    for (const auto& [stageNumber, active] : *preloaded)
    {
        text += std::format("{}{}={}", text.size() > 1 ? ", " : "", stageNumber, active);
    }
    return text + "}";
}

[[nodiscard]] std::string describeConfiguration(const Rocket&              rocket,
                                                const FlightConfiguration& config,
                                                std::string_view           label,
                                                const Preferences&         preferences)
{
    std::string line = std::format("config {} name={} preload={} active=[", label,
                                   quote(config.getNameRaw(preferences)), preloadedText(config));
    for (std::size_t stage = 0; stage < rocket.getStageCount(); ++stage)
    {
        line += std::format("{}{}", stage == 0 ? "" : ",",
                            config.isStageActive(static_cast<int>(stage)));
    }
    line += std::format("] motors={}", config.getAllMotors().size());
    return line;
}

/// "{name=value, name2=value2}": a map as Java prints it.
template <class Map>
[[nodiscard]] std::string mapText(const Map& map)
{
    std::string text = "{";
    for (const auto& [key, value] : map)
    {
        text += std::format("{}{}={}", text.size() > 1 ? ", " : "", key, value);
    }
    return text + "}";
}

/// @p text with every line break written as backslash n.
[[nodiscard]] std::string oneLine(std::string_view text)
{
    std::string line;
    for (const char c : text)
    {
        if (c == '\n')
        {
            line += "\\n";
        }
        else
        {
            line += c;
        }
    }
    return line;
}

/// Counts the change events of a rocket by kind, as the probe's listener does.
class EventCounter
{
public:
    explicit EventCounter(Rocket& rocket)
      : m_connection(rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
            ++m_count;
            const std::string text  = event.toString();
            const std::size_t open  = text.find('[');
            const std::string kinds = text.substr(open + 1, text.size() - open - 2);
            ++m_byKind[kinds];
        }))
    {
    }

    /// "<n> {<kinds>=<n>, ...}".
    [[nodiscard]] std::string text() const
    {
        return std::format("{} {}", m_count, mapText(m_byKind));
    }

private:
    int                                     m_count{0};
    std::map<std::string, int>              m_byKind;
    ComponentChangeSignal::ScopedConnection m_connection;
};

/// The XML of a case: the rocket element itself, or its content wrapped into one.
[[nodiscard]] std::string rocketElement(std::string_view xml)
{
    xml = Strings::trim(xml);
    if (xml.starts_with("<rocket"))
    {
        return std::string(xml);
    }
    return std::format("<rocket>{}</rocket>", xml);
}

}  // namespace

ByDesignationMotorFinder::ByDesignationMotorFinder()
  : m_motor(makeEmbeddedTestMotor("F12X", 12.0, "d"))
{
}

std::shared_ptr<const Motor> ByDesignationMotorFinder::findMotor(
    std::optional<Motor::Type> /*type*/, std::optional<std::string_view> /*manufacturer*/,
    std::optional<std::string_view> designation, double /*diameter*/, double /*length*/,
    std::optional<std::string_view> /*digest*/, WarningSet& warnings) const
{
    if (!designation.has_value())
    {
        return nullptr;
    }
    if (designation->starts_with('W'))
    {
        warnings.add(Warning::fromString(std::format("finder: {}", *designation)));
    }
    return m_motor;
}

std::set<Uuid> knownIds(std::string_view xml)
{
    std::set<Uuid> known;
    std::size_t    i = 0;
    while (i < xml.size())
    {
        std::size_t j = i;
        while (j < xml.size() && isIdChar(xml[j]))
        {
            ++j;
        }
        if (j == i)
        {
            ++i;
            continue;
        }
        const std::string_view run = xml.substr(i, j - i);
        if (run.contains('-'))
        {
            if (const Result<Uuid> id = Uuid::javaFromString(run))
            {
                known.insert(*id);
            }
        }
        i = j;
    }
    return known;
}

std::vector<std::string> describeRocket(const Rocket& rocket, const Preferences& preferences,
                                        const std::set<Uuid>& known)
{
    std::vector<std::string> lines;
    describeTree(rocket, 0, known, lines);
    const FlightConfigurationId& selected =
        rocket.getSelectedConfiguration().getFlightConfigurationId();
    lines.push_back("selected=" + (selected.isDefaultId() ? std::string("default")
                                                          : idText(selected.key(), known)));
    lines.push_back(describeConfiguration(rocket, rocket.getFlightConfigurations().getDefault(),
                                          "default", preferences));
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        lines.push_back(describeConfiguration(rocket, rocket.getFlightConfiguration(id),
                                              idText(id.key(), known), preferences));
    }
    return lines;
}

RocketLoadFixture::RocketLoadFixture(bool withPresets)
{
    addBuiltinMaterials(m_fixture.materials());
    m_fixture.context().setMotorFinder(&m_motorFinder);
    if (withPresets)
    {
        m_presets = makeExamplePresetDatabase();
        m_fixture.context().setComponentPresetDatabase(&m_presets);
    }
}

HandlerRun RocketLoadFixture::load(std::string_view xml)
{
    ComponentParameterHandler handler(rocket(), context());
    return runHandler(handler, rocketElement(xml));
}

std::string RocketLoadFixture::describeLoad(std::string_view xml, std::vector<std::string>& state)
{
    const std::string  element = rocketElement(xml);
    const EventCounter events(rocket());
    const HandlerRun   run = load(element);

    std::string text = run.result.has_value() ? std::string("RESULT ok\n")
                                              : std::format("RESULT FAILED {}: {}\n",
                                                            toString(run.result.error().code),
                                                            oneLine(run.result.error().message));
    for (const std::string& warning : run.texts())
    {
        text += std::format("W {}\n", oneLine(warning));
    }
    if (!run.element.empty())
    {
        text += std::format("ROOT {} {} [{}]\n", run.element, mapText(run.attributes),
                            Strings::trim(run.content));
    }
    text += std::format("EVENTS {}\n", events.text());
    state = describeRocket(rocket(), m_fixture.preferences(), knownIds(element));
    return text;
}

std::string RocketLoadFixture::loadAndDescribe(std::string_view xml)
{
    std::vector<std::string> state;
    std::string              text = describeLoad(xml, state);
    for (const std::string& line : state)
    {
        text += std::format("| {}\n", line);
    }
    // The last line has no line break, as the expectations are written.
    text.pop_back();
    return text;
}

std::string RocketLoadFixture::loadAndSummarize(std::string_view          xml,
                                                std::vector<std::string>* state)
{
    std::vector<std::string> lines;
    std::string              text = describeLoad(xml, lines);
    std::string              all;
    std::size_t              components = 0;
    for (const std::string& line : lines)
    {
        all += line;
        all += '\n';
        if (!line.starts_with("selected=") && !line.starts_with("config "))
        {
            ++components;
        }
    }
    text += std::format("COMPONENTS {}\nSTATE {}", components, sha256Hex(stringToBytes(all)));
    if (state != nullptr)
    {
        *state = std::move(lines);
    }
    return text;
}

std::string runRocketCase(std::string_view xml, bool withPresets)
{
    RocketLoadFixture fixture(withPresets);
    return fixture.loadAndDescribe(xml);
}

std::vector<std::string> failedRocketCases(std::span<const RocketCase> cases, bool withPresets)
{
    std::vector<std::string> failed;
    for (const RocketCase& one : cases)
    {
        const std::string found = runRocketCase(one.xml, withPresets);
        if (found != one.expected)
        {
            failed.push_back(std::format("case {}\n{}\n--- expected\n{}\n--- found\n{}\n", one.name,
                                         one.xml, one.expected, found));
        }
    }
    return failed;
}

std::string printedRocketCases(std::span<const RocketCase> cases, bool withPresets)
{
    std::string printed;
    for (const RocketCase& one : cases)
    {
        printed += std::format("=== {}\n{}\n", one.name, runRocketCase(one.xml, withPresets));
    }
    return printed;
}

std::string whatUsingTheRocketThrows(RocketLoadFixture& fixture)
{
    try
    {
        Rocket& rocket = fixture.rocket();
        rocket.getFlightConfigurations().getDefault().applyPreloadedStageActiveness();
        for (const FlightConfigurationId& id : rocket.getIds())
        {
            rocket.getFlightConfiguration(id).applyPreloadedStageActiveness();
        }
        fixture.document().clearUndo();
        rocket.enableEvents();
        rocket.update();
        static_cast<void>(rocket.getSelectedConfiguration().getActiveComponents());
        static_cast<void>(rocket.getLength());
        static_cast<void>(describeRocket(rocket, fixture.fixture().preferences(), {}));
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return {};
}

std::string whatReadingThrows(std::string_view xml, bool withPresets)
{
    try
    {
        RocketLoadFixture fixture(withPresets);
        static_cast<void>(fixture.load(xml));
        return whatUsingTheRocketThrows(fixture);
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
}

std::vector<std::string> casesThatThrowWhenCutOff(std::span<const RocketCase> cases,
                                                  bool                        withPresets)
{
    std::vector<std::string> thrown;
    for (const RocketCase& one : cases)
    {
        const std::string element = rocketElement(one.xml);
        for (std::size_t end = element.find('>'); end != std::string::npos;
             end             = element.find('>', end + 1))
        {
            const std::string_view cut   = std::string_view(element).substr(0, end + 1);
            const std::string      wrong = whatReadingThrows(cut, withPresets);
            if (!wrong.empty())
            {
                thrown.push_back(
                    std::format("case {}, cut off as\n{}\n{}\n", one.name, cut, wrong));
                break;
            }
        }
    }
    return thrown;
}

namespace
{

/// The XML document of a design file: the file itself, what its gzip holds, or the first entry
/// of its zip archive whose name ends in ".ork".
[[nodiscard]] std::string documentOfDesignFile(const std::filesystem::path& path)
{
    Result<std::vector<std::byte>> bytes = readFile(path);
    if (!bytes)
    {
        bug(std::format("cannot read {}: {}", pathToUtf8(path), bytes.error().message));
    }
    if (looksLikeGzip(*bytes))
    {
        Result<std::vector<std::byte>> inflated = gzipInflate(*bytes);
        if (!inflated)
        {
            bug(std::format("cannot inflate {}: {}", pathToUtf8(path), inflated.error().message));
        }
        return bytesToString(*inflated);
    }
    if (bytes->size() > 2 && (*bytes)[0] == std::byte{'P'} && (*bytes)[1] == std::byte{'K'})
    {
        ZipInputStream zip(*bytes);
        while (true)
        {
            const Result<std::optional<ZipInputStream::Entry>> entry = zip.nextEntry();
            if (!entry || !entry->has_value())
            {
                bug(std::format("no .ork entry in {}", pathToUtf8(path)));
            }
            if (Strings::toLower((*entry)->name).ends_with(".ork"))
            {
                const Result<std::vector<std::byte>> contents = zip.readEntry();
                if (!contents)
                {
                    bug(std::format("cannot read the entry of {}: {}", pathToUtf8(path),
                                    contents.error().message));
                }
                return bytesToString(*contents);
            }
        }
    }
    return bytesToString(*bytes);
}

/// @p text without the elements named @p name (none of them holds one of its own name).
[[nodiscard]] std::string withoutElements(std::string_view text, std::string_view name)
{
    const std::string open  = std::format("<{}", name);
    const std::string close = std::format("</{}>", name);
    std::string       out;
    std::size_t       pos = 0;
    while (true)
    {
        const std::size_t i = text.find(open, pos);
        if (i == std::string_view::npos)
        {
            break;
        }
        const char c = text.at(i + open.size());
        if (c != '>' && c != ' ' && c != '/')
        {
            out += text.substr(pos, i + open.size() - pos);
            pos = i + open.size();
            continue;
        }
        const std::size_t tagEnd = text.find('>', i);
        const std::size_t end =
            text.at(tagEnd - 1) == '/' ? tagEnd + 1 : text.find(close, tagEnd) + close.size();
        out += text.substr(pos, i - pos);
        pos = end;
    }
    out += text.substr(pos);
    return out;
}

}  // namespace

std::string rocketElementOfDesignFile(const std::filesystem::path& path)
{
    const std::string          document = documentOfDesignFile(path);
    const std::size_t          begin    = document.find("<rocket>");
    constexpr std::string_view kEnd     = "</rocket>";
    const std::size_t          end      = document.rfind(kEnd);
    if (begin == std::string::npos || end == std::string::npos || end < begin)
    {
        bug(std::format("no rocket element in {}", pathToUtf8(path)));
    }
    const std::string_view rocket =
        std::string_view(document).substr(begin, end + kEnd.size() - begin);
    return withoutElements(
        withoutElements(withoutElements(rocket, "appearance"), "insideappearance"),
        "inside-appearance");
}

std::vector<std::string> failedDesignFiles(const std::filesystem::path&    directory,
                                           std::span<const DesignFileCase> cases, bool withPresets)
{
    std::vector<std::string> failed;
    for (const DesignFileCase& one : cases)
    {
        RocketLoadFixture fixture(withPresets);
        const std::string found =
            fixture.loadAndSummarize(rocketElementOfDesignFile(directory / one.file));
        if (found != one.expected)
        {
            failed.push_back(std::format("file {}\n--- expected\n{}\n--- found\n{}\n", one.file,
                                         one.expected, found));
        }
    }
    return failed;
}

std::string printedDesignFiles(const std::filesystem::path&    directory,
                               std::span<const DesignFileCase> cases, bool withPresets,
                               bool withState)
{
    std::string printed;
    for (const DesignFileCase& one : cases)
    {
        RocketLoadFixture        fixture(withPresets);
        std::vector<std::string> state;
        printed += std::format(
            "=== {}\n{}\n", one.file,
            fixture.loadAndSummarize(rocketElementOfDesignFile(directory / one.file), &state));
        if (withState)
        {
            for (const std::string& line : state)
            {
                printed += std::format("{}\n", line);
            }
        }
    }
    return printed;
}

}  // namespace QtRocket::Test
