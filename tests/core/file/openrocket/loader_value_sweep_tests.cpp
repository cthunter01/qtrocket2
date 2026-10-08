#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/PhotoStudioHandler.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/DocumentHandlerTestSupport.h"

// The exit check of the loader's rocket side (tier 9b), second half: a sweep over every value
// the loader reads. For every entry of the setter table, on every kind of component the entry
// is reached from, and for every value the handlers of this side read (a motor mount, the
// flight configurations, deployment, separation, fin points, appearances, the document's
// preferences, materials, photo settings and custom expressions), a design that holds nothing
// but that value is read through the test root with each of the texts below in its place:
// numbers that are none, numbers beyond every range, counts of two thousand million, nothing,
// nonsense, and a text of 100000 characters.
//
// Each time the load has to end as the failure policy says (decision D9): with a warning, with
// an Error or with a value taken, never with an exception (a BugError is one), and never with
// something built once per unit of a number a file gives. The rocket is then used as the
// top-level loader and a first look at it use it (whatUsingTheRocketThrows()). What the value
// becomes is not this test's business: the case tables of the handlers and the tests of the
// setters pin that against OpenRocket.
//
// The sweep is one test per text, so that the texts run side by side; the asan preset is where
// it counts (a read past a buffer, an allocation as large as a number).

namespace
{

using QtRocket::ComponentKind;
using QtRocket::DocumentConfig;
using QtRocket::Parachute;
using QtRocket::PhotoStudioHandler;
using QtRocket::RocketComponent;
using QtRocket::Test::RocketLoadFixture;
using QtRocket::Test::whatReadingTheElementThrows;
using QtRocket::Test::whatUsingTheRocketThrows;

using Texts = std::vector<std::string>;

/// The length of the long texts of the sweep.
constexpr std::size_t kLongTextLength = 100000;

/// The texts of the sweep but the two long ones.
constexpr auto kShortTexts = std::to_array<std::string_view>({
    "NaN",
    "Infinity",
    "-Infinity",
    "",
    " ",
    "abc",
    "1e999",
    "-1",
    "0",
    "2147483647",
    "2147483648",
});

/// The number of texts of the sweep: the short ones, then 100000 digits (a number beyond
/// every range) and 100000 letters.
constexpr int kTextCount = static_cast<int>(kShortTexts.size()) + 2;

/// The text number @p index of the sweep.
[[nodiscard]] std::string sweepText(int index)
{
    const auto at = static_cast<std::size_t>(index);
    if (at < kShortTexts.size())
    {
        return std::string(kShortTexts.at(at));
    }
    // Not a braced list: that would be a text of two characters.
    std::string text(kLongTextLength, at == kShortTexts.size() ? '9' : 'a');
    return text;
}

/// A name for the test of the text number @p index.
[[nodiscard]] std::string sweepTextName(int index)
{
    constexpr auto kNames = std::to_array<std::string_view>({
        "NaN",
        "Infinity",
        "NegativeInfinity",
        "Empty",
        "Blank",
        "Letters",
        "TooLargeForADouble",
        "MinusOne",
        "Zero",
        "TheLargestInt",
        "BeyondAnInt",
        "AHundredThousandDigits",
        "AHundredThousandLetters",
    });
    std::string    name(kNames.at(static_cast<std::size_t>(index)));
    return name;
}

/// The element names of the setter table: what stands behind the colon of its keys, the
/// refused ones included, each once.
[[nodiscard]] std::vector<std::string_view> parameterElements()
{
    std::set<std::string_view> names;
    for (const std::vector<std::string_view>& keys :
         {DocumentConfig::setterKeys(), DocumentConfig::refusedKeys()})
    {
        for (const std::string_view key : keys)
        {
            names.insert(key.substr(key.find(':') + 1));
        }
    }
    return {names.begin(), names.end()};
}

/// Every kind of component a design holds, the rocket itself included.
[[nodiscard]] std::vector<ComponentKind> everyKind()
{
    std::vector<ComponentKind> kinds(QtRocket::kAllComponentKinds.begin(),
                                     QtRocket::kAllComponentKinds.end());
    if (std::ranges::find(kinds, ComponentKind::ROCKET) == kinds.end())
    {
        kinds.push_back(ComponentKind::ROCKET);
    }
    return kinds;
}

/// The attributes the setter of @p element reads, each with @p value: the method or type of
/// a position, the reference of a fin tab, the fields of a material, of a preset and of a
/// colour; "" for an element whose setter reads its text only.
[[nodiscard]] std::string attributesOf(std::string_view element, std::string_view value)
{
    std::vector<std::string_view> names;
    if (element == "angleoffset" || element == "radiusoffset" || element == "axialoffset")
    {
        names = {"method"};
    }
    else if (element == "position")
    {
        names = {"type"};
    }
    else if (element == "tabposition")
    {
        names = {"relativeto"};
    }
    else if (element == "material" || element == "linematerial" || element == "filletmaterial")
    {
        names = {"type", "density", "shearModulus", "group"};
    }
    else if (element == "preset")
    {
        names = {"type", "manufacturer", "partno", "digest"};
    }
    else if (element == "color")
    {
        names = {"red", "green", "blue", "alpha"};
    }
    std::string attributes;
    for (const std::string_view name : names)
    {
        attributes += std::format(" {}=\"{}\"", name, value);
    }
    return attributes;
}

/// The content of a rocket element in which a component of @p kind, where a design can have
/// one, holds @p content; for the rocket itself, @p content.
[[nodiscard]] std::string designWith(ComponentKind kind, std::string_view content)
{
    if (kind == ComponentKind::ROCKET)
    {
        return std::string(content);
    }
    const std::string_view name = xmlName(kind);
    const std::string      one  = std::format("<{0}>{1}</{0}>", name, content);
    if (kind == ComponentKind::AXIAL_STAGE)
    {
        return std::format("<subcomponents>{}</subcomponents>", one);
    }
    if (kind == ComponentKind::NOSE_CONE || kind == ComponentKind::BODY_TUBE ||
        kind == ComponentKind::TRANSITION)
    {
        return std::format(
            "<subcomponents><stage><subcomponents>{}</subcomponents></stage>"
            "</subcomponents>",
            one);
    }
    return std::format(
        "<subcomponents><stage><subcomponents><bodytube><length>0.3</length>"
        "<radius>0.02</radius><subcomponents>{}</subcomponents></bodytube>"
        "</subcomponents></stage></subcomponents>",
        one);
}

/// Something of @p rocket that is as large as a number of a file should never make it: a
/// component with more instances than the loader's bound, a parachute with more lines, or more
/// instances of all components over all flight configurations than the loader's budget
/// (DocumentConfig::kMaxInstances: the counts of nested components multiply); "" when there is
/// none.
[[nodiscard]] std::string whatIsBuiltByTheNumber(const QtRocket::Rocket& rocket)
{
    if (const std::uint64_t load = DocumentConfig::instanceLoad(rocket);
        load > DocumentConfig::kMaxInstances)
    {
        return std::format("{} component instances over all flight configurations", load);
    }
    for (const RocketComponent& component : rocket.subtree())
    {
        if (component.getInstanceCount() > DocumentConfig::kMaxCount)
        {
            return std::format("{} instances of {}", component.getInstanceCount(),
                               component.getComponentName());
        }
        const auto* const parachute = dynamic_cast<const Parachute*>(&component);
        if (parachute != nullptr && parachute->getLineCount() > DocumentConfig::kMaxCount)
        {
            return std::format("{} lines of a parachute", parachute->getLineCount());
        }
    }
    return {};
}

/// What goes wrong when the content @p xml of a rocket element is read and the rocket used:
/// what was thrown, or what was built by a number; "" when nothing did. The attachments are
/// those of an archive without entries, so that no text is looked up as a file.
[[nodiscard]] std::string whatGoesWrong(std::string_view xml)
{
    try
    {
        RocketLoadFixture fixture(false, RocketLoadFixture::Attachments::ARCHIVE);
        static_cast<void>(fixture.load(xml));
        std::string wrong = whatUsingTheRocketThrows(fixture);
        if (wrong.empty())
        {
            wrong = whatIsBuiltByTheNumber(fixture.rocket());
        }
        return wrong;
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
}

/// A text for a failure message: @p text, shortened when it is one of the long ones.
[[nodiscard]] std::string shown(std::string_view text)
{
    return text.size() > 40 ? std::format("{}... ({} characters)", text.substr(0, 20), text.size())
                            : std::string(text);
}

/// What goes wrong when a component of @p kind holds @p element with @p text, and
/// @p attributes when there are any (see attributesOf()), as a line for a failure message;
/// "" when nothing does.
[[nodiscard]] std::string wrongForOneParameter(ComponentKind kind, std::string_view element,
                                               std::string_view attributes, std::string_view text)
{
    const std::string found = whatGoesWrong(
        designWith(kind, std::format("<{0}{1}>{2}</{0}>", element, attributes, text)));
    if (found.empty())
    {
        return {};
    }
    return std::format("{} <{}{}>{}: {}", className(kind), element,
                       attributes.empty() ? "" : " with attributes", shown(text), found);
}

/// What goes wrong for the entries of the setter table when their element holds @p text.
///
/// The text is read alone, and once more with the attributes its setter reads set to the text
/// as well, when it reads any. A short text is read so for every kind of component and every
/// element that the kind has a setter or a refusal for (544 pairs); a long text, which takes
/// its time to parse, once for every entry of the table (135), on the first kind that reaches
/// the entry.
[[nodiscard]] Texts wrongForTheSetterTable(std::string_view text)
{
    Texts                               wrong;
    const std::vector<std::string_view> elements = parameterElements();
    const bool                          isLong   = text.size() > 1000;
    std::set<std::string>               entriesRead;
    for (const ComponentKind kind : everyKind())
    {
        for (const std::string_view element : elements)
        {
            const DocumentConfig::SetterLookup found = DocumentConfig::findSetter(kind, element);
            if (found.owner.empty())
            {
                // No class of the kind has an entry: an unknown parameter, as any other name.
                continue;
            }
            if (isLong && !entriesRead.insert(std::format("{}:{}", found.owner, element)).second)
            {
                continue;
            }
            std::string line = wrongForOneParameter(kind, element, "", text);
            if (!line.empty())
            {
                wrong.push_back(std::move(line));
            }
            const std::string attributes = attributesOf(element, text);
            if (attributes.empty())
            {
                continue;
            }
            line = wrongForOneParameter(kind, element, attributes, text);
            if (!line.empty())
            {
                wrong.push_back(std::move(line));
            }
        }
    }
    return wrong;
}

/// The designs of the sweep over the values of the component handlers: the content of a rocket
/// element each, with "{}" where the value stands.
constexpr auto kHandlerValues =
    std::to_array<std::string_view>({
        // The flight configurations of the rocket.
        R"(<motorconfiguration configid="{}" default="true"><name>N</name><stage number="0" active="true"/></motorconfiguration>)",
        R"(<motorconfiguration configid="a" default="{}"/>)",
        R"(<motorconfiguration configid="a"><name>{}</name></motorconfiguration>)",
        R"(<subcomponents><stage/></subcomponents><motorconfiguration configid="a"><stage number="{}" active="true"/></motorconfiguration>)",
        R"(<subcomponents><stage/></subcomponents><motorconfiguration configid="a"><stage number="0" active="{}"/></motorconfiguration>)",
        R"(<flightconfiguration configid="{}"><name>N</name></flightconfiguration>)",
        // A motor mount, its motors and its ignition.
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionevent>{}</ignitionevent></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><ignitiondelay>{}</ignitiondelay></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><overhang>{}</overhang></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="{}"><designation>C6</designation><delay>3</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><type>{}</type><designation>C6</designation></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><manufacturer>{}</manufacturer><designation>C6</designation></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><designation>{}</designation></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><digest>{}</digest><designation>C6</designation></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><designation>C6</designation><diameter>{}</diameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><designation>C6</designation><length>{}</length></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><designation>C6</designation><delay>{}</delay></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><motor configid="a"><designation>C6</designation><nozzleexitdiameter>{}</nozzleexitdiameter></motor></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="{}"><ignitionevent>burnout</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="a"><ignitionevent>{}</ignitionevent><ignitiondelay>1</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><motormount><ignitionconfiguration configid="a"><ignitionevent>burnout</ignitionevent><ignitiondelay>{}</ignitiondelay></ignitionconfiguration></motormount></bodytube></subcomponents></stage></subcomponents>)",
        // The deployment of a recovery device and the separation of a stage.
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><parachute><deploymentconfiguration configid="{}"><deployevent>apogee</deployevent></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><parachute><deploymentconfiguration configid="a"><deployevent>{}</deployevent></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><streamer><deploymentconfiguration configid="a"><deploydelay>{}</deploydelay></deploymentconfiguration></streamer></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><parachute><deploymentconfiguration configid="a"><deployaltitude>{}</deployaltitude></deploymentconfiguration></parachute></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><separationconfiguration configid="{}"><separationevent>burnout</separationevent></separationconfiguration></stage></subcomponents>)",
        R"(<subcomponents><stage><separationconfiguration configid="a"><separationevent>{}</separationevent></separationconfiguration></stage></subcomponents>)",
        R"(<subcomponents><stage><separationconfiguration configid="a"><separationdelay>{}</separationdelay></separationconfiguration></stage></subcomponents>)",
        R"(<subcomponents><stage><separationconfiguration configid="a"><separationaltitude>{}</separationaltitude></separationconfiguration></stage></subcomponents>)",
        // The outline of a freeform fin set.
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="{}" y="0.05"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><freeformfinset><finpoints><point x="0" y="0"/><point x="0.05" y="{}"/><point x="0.1" y="0"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><freeformfinset><finpoints><point x="{}" y="{}"/></finpoints></freeformfinset></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        // An appearance and its decal.
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="{}" green="2" blue="3" alpha="4"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="{}" blue="3"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="{}"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><paint red="1" green="2" blue="3" alpha="{}"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><shine>{}</shine></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><opacityaffectstexture>{}</opacityaffectstexture></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="{}" rotation="0" edgemode="REPEAT"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="{}" edgemode="REPEAT"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="{}"/></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><center x="{}" y="0"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><center x="0" y="{}"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><offset x="{}" y="0"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><offset x="0" y="{}"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><scale x="{}" y="1"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="a.png" rotation="0" edgemode="REPEAT"><scale x="1" y="{}"/></decal></appearance></nosecone></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><nosecone><appearance><decal name="{}" rotation="{}" edgemode="{}"><center x="{}" y="{}"/><offset x="{}" y="{}"/><scale x="{}" y="{}"/></decal><shine>{}</shine></appearance></nosecone></subcomponents></stage></subcomponents>)",
        // An inside appearance: its own two flags, and the values it shares with an appearance.
        R"(<subcomponents><stage><subcomponents><bodytube><insideappearance><edgessameasinside>{}</edgessameasinside></insideappearance></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><insideappearance><insidesameasoutside>{}</insidesameasoutside></insideappearance></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><insideappearance><paint red="{}" green="{}" blue="{}" alpha="{}"/><shine>{}</shine></insideappearance></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><inside-appearance><decal name="{}" rotation="0" edgemode="REPEAT"><center x="{}" y="1"/></decal></inside-appearance></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><insideappearance><decal name="a.png" rotation="{}" edgemode="REPEAT"/></insideappearance></stage></subcomponents>)",
        // What the review of the group found: counts that multiply down the tree and with the
        // flight configurations, and an id that another component has.
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><podset><instancecount>{}</instancecount><subcomponents><bodytube><subcomponents><launchlug><instancecount>{}</instancecount></launchlug><trapezoidfinset><fincount>{}</fincount></trapezoidfinset><innertube><clusterconfiguration>9-grid</clusterconfiguration><subcomponents><innertube><clusterconfiguration>{}</clusterconfiguration></innertube></subcomponents></innertube></subcomponents></bodytube></subcomponents></podset></subcomponents></bodytube></subcomponents></stage></subcomponents>)",
        R"(<subcomponents><stage><subcomponents><bodytube><subcomponents><launchlug><instancecount>{}</instancecount></launchlug></subcomponents><motormount><motor configid="1-1-1-1-1"/><motor configid="{}"/></motormount></bodytube></subcomponents></stage></subcomponents><motorconfiguration configid="2-2-2-2-2"/><motorconfiguration configid="{}"/><motorconfiguration/>)",
        R"(<id>1-2-3-4-5</id><subcomponents><stage><id>1-2-3-4-5</id><name>{}</name><subcomponents><bodytube><id>1-2-3-4-5</id><name>{}</name></bodytube></subcomponents></stage><stage><id>1-2-3-4-5</id></stage></subcomponents>)",
        R"(<subcomponents><stage><id>{}</id></stage><stage><id>{}</id></stage></subcomponents>)",
    });

/// The elements of the sweep over the values of the handlers of a document's own elements: a
/// <docprefs>, <docmaterials> or <datatypes> each, with "{}" where the value stands. (The
/// settings of the photo studio are made from the handler's own lists.)
constexpr auto kDocumentValues = std::to_array<std::string_view>({
    R"(<docprefs><pref key="{}" type="string">v</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="{}">v</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="boolean">{}</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="string">{}</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="integer">{}</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="double">{}</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="number">{}</pref></docprefs>)",
    R"(<docprefs><pref type="number">{}</pref></docprefs>)",
    R"(<docprefs><pref key="k" type="list"><entry type="number">{}</entry><entry type="{}">1</entry></pref></docprefs>)",
    R"(<docprefs><pref type="list"><entry key="{}" type="string">{}</entry></pref></docprefs>)",
    R"(<docprefs><docmaterials><material>{}</material></docmaterials></docprefs>)",
    R"(<docmaterials><material>{}</material></docmaterials>)",
    R"(<docmaterials><material>{}|A|1.0</material></docmaterials>)",
    R"(<docmaterials><material>BULK|{}|1.0</material></docmaterials>)",
    R"(<docmaterials><material>BULK|A|{}</material></docmaterials>)",
    R"(<docmaterials><material>BULK|A|1.0|{}</material></docmaterials>)",
    R"(<docmaterials><material>BULK|A|1.0|2.0|{}</material></docmaterials>)",
    R"(<docmaterials><material>LINE|A|{}|ThreadsLines</material></docmaterials>)",
    R"(<docmaterials><material>BULK|A|{}|{}|{}</material><material>BULK|A|{}|{}|{}</material></docmaterials>)",
    R"(<datatypes><type source="{}"><name>n</name></type></datatypes>)",
    R"(<datatypes><type source="customexpression"><name>{}</name><symbol>qr4sweep</symbol></type></datatypes>)",
    R"(<datatypes><type source="customexpression"><name>Sweep r4</name><symbol>{}</symbol></type></datatypes>)",
    R"(<datatypes><type source="customexpression"><unit unittype="auto">{}</unit></type></datatypes>)",
    R"(<datatypes><type source="customexpression"><unit unittype="{}">m</unit></type></datatypes>)",
    R"(<datatypes><type source="customexpression"><expression>{}</expression></type></datatypes>)",
    R"(<datatypes><{}/></datatypes>)",
});

/// @p form with every "{}" replaced by @p text.
[[nodiscard]] std::string filled(std::string_view form, std::string_view text)
{
    std::string out;
    std::size_t pos = 0;
    for (std::size_t at = form.find("{}"); at != std::string_view::npos; at = form.find("{}", pos))
    {
        out += form.substr(pos, at - pos);
        out += text;
        pos = at + 2;
    }
    out += form.substr(pos);
    return out;
}

/// What goes wrong for the values of the component handlers when each is @p text.
[[nodiscard]] Texts wrongForTheHandlerValues(std::string_view text)
{
    Texts wrong;
    for (const std::string_view form : kHandlerValues)
    {
        const std::string found = whatGoesWrong(filled(form, text));
        if (!found.empty())
        {
            wrong.push_back(std::format("{} with {}: {}", form, shown(text), found));
        }
    }
    return wrong;
}

/// The elements of the sweep over the photo settings: per setting kept as text an element with
/// "{}" as its text, and per colour one for each of its four attributes.
[[nodiscard]] Texts photoSettingForms()
{
    Texts forms;
    for (const std::string_view name : PhotoStudioHandler::kTextSettings)
    {
        forms.push_back(std::format("<photostudio><{0}>{{}}</{0}></photostudio>", name));
    }
    constexpr auto kParts = std::to_array<std::string_view>({"red", "green", "blue", "alpha"});
    for (const std::string_view name : PhotoStudioHandler::kColorSettings)
    {
        for (const std::string_view hostile : kParts)
        {
            std::string attributes;
            for (const std::string_view part : kParts)
            {
                attributes += std::format(" {}=\"{}\"", part, part == hostile ? "{}" : "1");
            }
            forms.push_back(std::format("<photostudio><{}{}/></photostudio>", name, attributes));
        }
    }
    return forms;
}

/// What goes wrong for the values of the handlers of a document's own elements when each is
/// @p text.
[[nodiscard]] Texts wrongForTheDocumentValues(std::string_view text)
{
    Texts forms = photoSettingForms();
    forms.insert(forms.end(), kDocumentValues.begin(), kDocumentValues.end());
    Texts wrong;
    for (const std::string& form : forms)
    {
        // An element whose name the text is has to be a name: "<abc/>" is one, "<NaN/>" too,
        // "<-1/>" and "</>" are no XML, which fails before a handler is asked. Both are fine.
        const std::string found = whatReadingTheElementThrows(filled(form, text));
        if (!found.empty())
        {
            wrong.push_back(std::format("{} with {}: {}", form, shown(text), found));
        }
    }
    return wrong;
}

class LoaderValueSweep : public ::testing::TestWithParam<int>
{ };

TEST_P(LoaderValueSweep, NoEntryOfTheSetterTableMakesTheLoaderThrow)
{
    EXPECT_EQ(wrongForTheSetterTable(sweepText(GetParam())), Texts{});
}

TEST_P(LoaderValueSweep, NoValueOfTheComponentHandlersMakesTheLoaderThrow)
{
    EXPECT_EQ(wrongForTheHandlerValues(sweepText(GetParam())), Texts{});
}

TEST_P(LoaderValueSweep, NoValueOfTheDocumentHandlersMakesTheLoaderThrow)
{
    EXPECT_EQ(wrongForTheDocumentValues(sweepText(GetParam())), Texts{});
}

INSTANTIATE_TEST_SUITE_P(Texts, LoaderValueSweep, ::testing::Range(0, kTextCount),
                         [](const ::testing::TestParamInfo<int>& paramInfo) {
                             return sweepTextName(paramInfo.param);
                         });

/// The texts of the sweep, each as a failure message shows it.
[[nodiscard]] Texts shownTexts()
{
    Texts texts;
    for (int index = 0; index < kTextCount; ++index)
    {
        texts.push_back(shown(sweepText(index)));
    }
    return texts;
}

// The sweep sweeps what it says: every text, every element of the setter table on every kind
// that reaches it, and every setting of the photo studio.
TEST(LoaderValueSweepCoverage, HasTheTextsOfTheExitCheck)
{
    EXPECT_EQ(shownTexts(),
              (Texts{"NaN", "Infinity", "-Infinity", "", " ", "abc", "1e999", "-1", "0",
                     "2147483647", "2147483648", "99999999999999999999... (100000 characters)",
                     "aaaaaaaaaaaaaaaaaaaa... (100000 characters)"}));
}

/// How many pairs of a kind of component and an element the sweep over the setter table
/// reads, and how many of the table's keys are reached by none of them.
struct Coverage
{
    std::size_t pairs{0};
    Texts       keysNotReached;
};

[[nodiscard]] Coverage coverageOfTheSetterTable()
{
    Coverage                            coverage;
    std::set<std::string>               reached;
    const std::vector<std::string_view> elements = parameterElements();
    for (const ComponentKind kind : everyKind())
    {
        for (const std::string_view element : elements)
        {
            const DocumentConfig::SetterLookup found = DocumentConfig::findSetter(kind, element);
            if (!found.owner.empty())
            {
                ++coverage.pairs;
                reached.insert(std::format("{}:{}", found.owner, element));
            }
        }
    }
    for (const std::vector<std::string_view>& keys :
         {DocumentConfig::setterKeys(), DocumentConfig::refusedKeys()})
    {
        for (const std::string_view key : keys)
        {
            if (!reached.contains(std::string(key)))
            {
                coverage.keysNotReached.emplace_back(key);
            }
        }
    }
    return coverage;
}

TEST(LoaderValueSweepCoverage, ReachesEveryEntryOfTheSetterTable)
{
    const Coverage coverage = coverageOfTheSetterTable();
    // All 135 entries of OpenRocket's table, the five refusals included.
    EXPECT_EQ(DocumentConfig::setterKeys().size() + DocumentConfig::refusedKeys().size(), 135U);
    EXPECT_EQ(coverage.keysNotReached, Texts{});
    // The pairs of a kind and an element, each read per short text (see
    // wrongForTheSetterTable()).
    EXPECT_EQ(coverage.pairs, 544U);
}

TEST(LoaderValueSweepCoverage, HasAFormForEverySettingOfThePhotoStudio)
{
    EXPECT_EQ(photoSettingForms().size(), 24U + (6U * 4U));
}

/// What whatIsBuiltByTheNumber() says of a rocket whose launch lug was given @p count
/// instances in each of 400 pods, past the loader.
[[nodiscard]] std::string whatIsSaidOfLaunchLugsInPods(int count)
{
    RocketLoadFixture fixture;
    static_cast<void>(fixture.load(
        "<subcomponents><stage><subcomponents><bodytube><subcomponents><podset>"
        "<instancecount>400</instancecount><subcomponents><bodytube><subcomponents><launchlug/>"
        "</subcomponents></bodytube></subcomponents></podset></subcomponents></bodytube>"
        "</subcomponents></stage></subcomponents>"));
    for (RocketComponent& component : fixture.rocket().subtree())
    {
        if (component.kind() == ComponentKind::LAUNCH_LUG)
        {
            component.setInstanceCount(count);
        }
    }
    return whatIsBuiltByTheNumber(fixture.rocket());
}

/// What whatIsBuiltByTheNumber() says of a rocket whose launch lug was given more instances
/// than the loader lets a file give.
[[nodiscard]] std::string whatIsSaidOfTooManyLaunchLugs()
{
    RocketLoadFixture fixture;
    static_cast<void>(fixture.load(designWith(ComponentKind::LAUNCH_LUG, "")));
    for (RocketComponent& component : fixture.rocket().subtree())
    {
        if (component.kind() == ComponentKind::LAUNCH_LUG)
        {
            component.setInstanceCount(DocumentConfig::kMaxCount + 1);
        }
    }
    return whatIsBuiltByTheNumber(fixture.rocket());
}

// The check checks: a design that is fine is said to be, and a count beyond the bound is told
// of.
TEST(LoaderValueSweepCoverage, TellsOfWhatGoesWrong)
{
    EXPECT_EQ(whatGoesWrong("<name>fine</name>"), "");
    EXPECT_EQ(whatIsSaidOfTooManyLaunchLugs(), "10001 instances of Launch Lug");
    // Counts that are each within the bound and multiply beyond the budget.
    EXPECT_EQ(whatIsSaidOfLaunchLugsInPods(247), "");
    EXPECT_EQ(whatIsSaidOfLaunchLugsInPods(248),
              "100003 component instances over all flight configurations");
}

}  // namespace
