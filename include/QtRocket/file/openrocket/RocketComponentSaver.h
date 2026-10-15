#pragma once

#include <format>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"

namespace QtRocket
{

class Material;
class MotorMount;
class OrkWriter;

/// The part of the .ork component savers that every component shares (OpenRocket's
/// file/openrocket/savers/RocketComponentSaver): the parameters of addParams(), the material and
/// motor-mount parameters, and the helpers that write one element.
///
/// A component's saver gives the lines of its element as Java's getElements() does (see
/// ComponentSaver): the open tag, the parameter lines and the close tag. A parameter line is the
/// text of the line without the indentation of the element, which OpenRocketSaver adds (one level
/// more than the component's tag); a line that belongs to a nested element carries its own
/// leading spaces, as in Java ("  <paint .../>", "    <center .../>").
///
/// addParams() writes, in Java's order and with Java's getter calls (the getters of automatic
/// dimensions store what they compute, so the order of the calls shows in the saved text, as it
/// does in OpenRocket's: tier 10 decision O3):
/// - <name> and <id>;
/// - <preset type manufacturer partno digest/> of getPresetComponent(), or of
///   getUnresolvedPreset() (see the deviations);
/// - <appearance> and, for an InsideColorComponent with an inside appearance, <insideappearance>
///   with <edgessameasinside> and <insidesameasoutside>: the paint, the shine,
///   <opacityaffectstexture> and the decal with its name, rotation, edge mode (the constant's
///   name), center, offset and scale;
/// - for a component that is no assembly, <color> and <linestyle> when it has them;
/// - for an Instanceable that is not Clusterable, <instancecount> (and <fincount> for the fin
///   sets and the tube fin set, which OpenRocket keeps for version 15.03); for a LineInstanceable
///   <instanceseparation>;
/// - for a RadiusPositionable <radiusoffset method>, for an AnglePositionable <angleoffset
///   method> in degrees (angle * 180.0 / PI, MathUtil::rad2deg()) and again as <rotation> (fin
///   sets) or <radialdirection> (all but the stages, pod sets and rail buttons);
/// - unless the axial method is AFTER, <axialoffset method> and the legacy <position type>;
/// - the mass, CG and CD overrides that are set, each with its subcomponents flag;
/// - <comment> when there is one.
///
/// Deviations from OpenRocket:
/// - A component without a preset that keeps the reference of an unresolved <preset>
///   (RocketComponent::getUnresolvedPreset(), the user's decision U8 of 2026-10-08) writes that
///   reference back: the type and the digest only when the reference has them. OpenRocket's
///   loader forgets such a reference, so its save has no <preset> there.
/// - Every text and attribute value of the model goes through OrkWriter::escape() (decision E3
///   of tier 10): also the digests of a preset and of a motor and the type of an unresolved
///   preset reference, which Java writes unescaped; for the texts these hold in a file OpenRocket
///   can read the result is the same. (The names of constants, numbers and booleans need no
///   escaping and are written as they are.)
/// - A material's name is written as it is: QtRocket's names are not translated, so Java's
///   Translator.getBaseText("material", name) is the name itself.
/// - The savers take the component as a constant (QtRocket's getters of automatic dimensions are
///   const and refresh mutable members) and the writer for its escape(); Java's static
///   TextUtil.escapeXML() and the protected helpers emitDouble(), emitInteger(), appendElement()
///   are the element helpers here.
class RocketComponentSaver final
{
public:
    RocketComponentSaver() = delete;

    /// The lines of a component's element (Java: the List<String> of getElements()).
    using Elements = std::vector<std::string>;

    /// Java's RocketComponentSaver.addParams(): the parameters every component writes, appended
    /// to @p elements.
    static void addParams(const RocketComponent& component, OrkWriter& writer, Elements& elements);

    /// Java's materialParam(mat): materialParam("material", @p material).
    [[nodiscard]] static std::string materialParam(const Material& material, OrkWriter& writer);

    /// Java's materialParam(tag, mat): `<tag type="bulk|surface|line" density="D"
    /// [shearModulus="S"] group="G">name</tag>`, with the shear modulus when it is not 0 or the
    /// material is user-defined.
    /// @throws BugError for a material of type CUSTOM (Java: BugException "Unknown material
    ///         type"), which no component can hold.
    [[nodiscard]] static std::string materialParam(std::string_view tag, const Material& material,
                                                   OrkWriter& writer);

    /// Java's motorMountParams(): nothing when @p mount does not act as a motor mount, else the
    /// lines of its <motormount> element: the default ignition event and delay, the overhang,
    /// then for each flight configuration of the rocket (Rocket::getIds()) that gives the mount
    /// a motor a <motor configid> (type unless UNKNOWN, the manufacturer's simple name and the
    /// digest of a ThrustCurveMotor, designation, diameter, length, the ejection delay, "none"
    /// for a plugged one, and the nozzle exit diameter when it is above 0) and an
    /// <ignitionconfiguration> with its ignition event and delay (written for every motor: a
    /// configuration with a motor is never the mount's default one).
    [[nodiscard]] static Elements motorMountParams(const MotorMount& mount, OrkWriter& writer);

    /// "<tag>" + @p text + "</tag>", @p text as it is (a number, a name of a constant).
    [[nodiscard]] static std::string element(std::string_view tag, std::string_view text);

    /// element() of @p text made safe by @p writer (OrkWriter::escape()).
    [[nodiscard]] static std::string textElement(std::string_view tag, std::string_view text,
                                                 OrkWriter& writer);

    /// element() of @p value as Double.toString() prints it (Java: emitDouble(), and the
    /// concatenation of a double).
    [[nodiscard]] static std::string doubleElement(std::string_view tag, double value);

    /// element() of "true" or "false" (Java: the concatenation of a boolean).
    [[nodiscard]] static std::string booleanElement(std::string_view tag, bool value);

    /// element() of @p value in decimal (Java: emitInteger(), and the concatenation of an int).
    [[nodiscard]] static std::string integerElement(std::string_view tag, int value);

    /// Java's getElements() of a saver: "<tag>", the parameters @p addParams appends to the
    /// lines it is given, "</tag>".
    template <typename AddParams>
    [[nodiscard]] static Elements elementOf(std::string_view tag, AddParams addParams)
    {
        Elements list;
        list.push_back(std::format("<{}>", tag));
        addParams(list);
        list.push_back(std::format("</{}>", tag));
        return list;
    }

    /// @p component as the class @p T a saver writes (Java's cast in each addParams()).
    /// @throws BugError when @p component is no @p T: a saver called for another kind.
    template <typename T>
    [[nodiscard]] static const T& as(const RocketComponent& component)
    {
        const auto* const cast = dynamic_cast<const T*>(&component);
        if (cast == nullptr)
        {
            bug(std::format("a saver called for a component of kind {}",
                            componentKindName(component.kind())));
        }
        return *cast;
    }
};

}  // namespace QtRocket
