#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/Setter.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "file/openrocket/SetterTestSupport.h"
#include "rocket/preset/ExamplePresets.h"

// The failure policy of the loader (decision D9): nothing a file can hold may make a setter
// throw. Every setter of the table is applied with texts that are numbers at and beyond the
// edge of what a component expects, the special words, counts, truth values and nonsense, to
// components in a document whose rocket has its events on, as while a file loads: every setter
// then fires its event, and the rocket and its flight configurations recompute on it.
//
// After every setter the rocket is asked for its length, because a setter may ask for it at any
// moment of a load: <cordlength>auto</cordlength> of a shock cord takes the cord's length from
// the rocket's, which is the bounds of every component as the file has left them so far.
//
// The setters that read attributes (the positions, the tab of a fin, a material, a preset) get
// each text with every method they know, with the text as a material's density and shear
// modulus, and with each preset of the example designs, whatever the kind of the component.
//
// What a component makes of such a value is OpenRocket's business and tested with the
// components; here only "no exception" counts, and that the rocket can still be used.

namespace
{

using QtRocket::ComponentKind;
using QtRocket::DocumentConfig;
using QtRocket::Result;
using QtRocket::Rocket;
using QtRocket::RocketComponent;
using QtRocket::Setter;
using QtRocket::WarningSet;
using QtRocket::Test::ApplicationSetterFixture;
using QtRocket::Test::attributesOf;
using QtRocket::Test::kExamplePresetReferences;

using Texts = std::vector<std::string>;

/// Texts for every kind of setter: each setter takes the ones that mean something to it and
/// warns of the others.
constexpr auto kHostileTexts = std::to_array<std::string_view>({
    // numbers
    "0",
    "-0.0",
    "1",
    "-1",
    "0.5",
    "-0.5",
    "1e-300",
    "-1e-300",
    "4.9e-324",
    "1e18",
    "-1e18",
    "1e300",
    "-1e300",
    "1.7976931348623157e308",
    "-1.7976931348623157e308",
    "90",
    "-90",
    "360",
    "-720",
    // the special words, alone and with a number
    "auto",
    "auto 0",
    "auto -1",
    "auto 1e300",
    "filled",
    // counts (the largest are in a test of their own)
    "2",
    "8",
    "64",
    "-2147483648",
    // truth values
    "true",
    "false",
    // an id
    "1-2-3-4-5",
    // cluster layouts
    "5-ring",
    "9-grid",
    // nothing a setter reads
    "",
    " ",
    "x",
    "NaN",
    "Infinity",
});

/// The attributes the position setters and the setter of a fin's tab are tried with: every
/// method the setter knows, the old attribute name, a method it does not know, and none. Empty
/// for the element of another setter.
[[nodiscard]] Texts methodsOf(std::string_view element)
{
    if (element == "axialoffset")
    {
        return {"method=absolute", "method=after", "method=top", "method=middle",
                "method=bottom",   "type=bottom",  "method=x",   ""};
    }
    if (element == "position")
    {
        // The old element with the old attribute, and with the new one.
        return {"type=absolute", "type=after", "type=bottom", "method=middle", "type=x", ""};
    }
    if (element == "radiusoffset")
    {
        return {"method=coaxial", "method=free", "method=relative",
                "method=surface", "method=x",    ""};
    }
    if (element == "angleoffset")
    {
        return {"method=relative", "method=fixed", "method=mirrorxy", "method=x", ""};
    }
    if (element == "tabposition")
    {
        return {"relativeto=absolute", "relativeto=after",
                "relativeto=top",      "relativeto=middle",
                "relativeto=bottom",   "relativeto=front",
                "relativeto=center",   "relativeto=end",
                "relativeto=x",        ""};
    }
    return {};
}

[[nodiscard]] bool isMaterialElement(std::string_view element)
{
    return element == "material" || element == "filletmaterial" || element == "linematerial";
}

/// A number that differs from text to text, to choose among attributes with: the sum of the
/// text's bytes and its length.
[[nodiscard]] std::size_t numberOf(std::string_view text) noexcept
{
    std::size_t number = text.size();
    for (const char c : text)
    {
        number += static_cast<unsigned char>(c);
    }
    return number;
}

/// How many of an element's sets of attributes a text is tried with.
enum class AttributeSets
{
    /// Every set: each method with each text.
    EVERY,
    /// Two of them, which two depending on the text, so that each set meets some of the texts
    /// and the methods a component has change as the texts go by.
    TWO_PER_TEXT,
};

/// Every set of attributes the element @p element is tried with for the text @p text: one empty
/// set for a setter that reads none.
[[nodiscard]] Texts everyAttributeSetOf(std::string_view element, std::string_view text)
{
    Texts sets = methodsOf(element);
    if (!sets.empty())
    {
        return sets;
    }
    if (isMaterialElement(element))
    {
        // The text as the density and as the shear modulus of a material of the document, and
        // as the density of one of the application's ('|' and '=' are the table's separators).
        sets.push_back(std::format("density={}", text));
        sets.push_back(std::format("density={}|shearModulus={}|group=Woods", text, text));
        sets.push_back(std::format("density=1|shearModulus={}|group=ThreadsLines", text));
        return sets;
    }
    if (element == "preset")
    {
        // The text plays no part for a preset: one of the six presets of the examples per text,
        // whatever the kind of the component.
        const QtRocket::Test::ExamplePresetReference& preset =
            kExamplePresetReferences.at(numberOf(text) % kExamplePresetReferences.size());
        sets.push_back(std::format("manufacturer={}|partno={}|digest={}", preset.manufacturer,
                                   preset.partNo, preset.digest));
        return sets;
    }
    sets.emplace_back();
    return sets;
}

/// The sets of attributes the element @p element is tried with for the text @p text.
[[nodiscard]] Texts attributeSetsOf(std::string_view element, std::string_view text,
                                    AttributeSets howMany)
{
    Texts sets = everyAttributeSetOf(element, text);
    if (howMany == AttributeSets::EVERY || sets.size() <= 2)
    {
        return sets;
    }
    // Two neighbours of the list, the first one chosen by the text.
    const std::size_t first = numberOf(text) % sets.size();
    return {sets.at(first), sets.at((first + 1) % sets.size())};
}

/// The order in which the setters of a component and the hostile texts are gone through.
struct Order
{
    /// Every setter for the first text, then every setter for the second, and so on; otherwise
    /// every text for the first setter first.
    bool textsFirst;
    /// The whole sequence from its end.
    bool backwards;
};

/// A setter of a component with the element it is the setter of.
struct ElementSetter
{
    std::string_view element;
    const Setter*    setter;
};

/// The elements for which the walk of @p kind finds a setter, with their setters.
[[nodiscard]] std::vector<ElementSetter> settersOf(ComponentKind kind)
{
    std::vector<ElementSetter> setters;
    for (const std::string_view key : DocumentConfig::setterKeys())
    {
        const std::string_view             element = key.substr(key.find(':') + 1);
        const DocumentConfig::SetterLookup lookup  = DocumentConfig::findSetter(kind, element);
        const bool                         isNew   = std::ranges::none_of(
            setters, [element](const ElementSetter& known) { return known.element == element; });
        if (lookup.setter != nullptr && isNew)
        {
            setters.push_back({.element = element, .setter = lookup.setter});
        }
    }
    return setters;
}

/// What applyHostileTexts() works with: the document, how many sets of attributes a text is
/// tried with, and the warnings so far.
class Trial
{
public:
    Trial(ApplicationSetterFixture& fixture, AttributeSets attributeSets) noexcept
      : m_fixture(&fixture), m_attributeSets(attributeSets)
    {
    }

    [[nodiscard]] ApplicationSetterFixture& fixture() noexcept { return *m_fixture; }
    [[nodiscard]] AttributeSets attributeSets() const noexcept { return m_attributeSets; }
    [[nodiscard]] WarningSet&   warnings() noexcept { return m_warnings; }

private:
    ApplicationSetterFixture* m_fixture;
    AttributeSets             m_attributeSets;
    WarningSet                m_warnings;
};

/// Applies @p text through @p chosen to @p component, once with each set of attributes the
/// element is tried with, and asks the rocket for its length after each. Returns the failure of
/// the setter, or "" when it succeeded. Only the setter of a component's id may fail (a text
/// that is no UUID fails the load, as in OpenRocket): its failure is an answer, not an error,
/// and gives "" too.
[[nodiscard]] std::string applyOne(Trial& trial, RocketComponent& component,
                                   const ElementSetter& chosen, std::string_view text)
{
    // A material's name: the text, or a name of the application's when the text is none.
    const bool             isMaterial = isMaterialElement(chosen.element);
    const std::string_view value = isMaterial && QtRocket::Strings::isEmpty(text) ? "Balsa" : text;
    for (const std::string& attributes :
         attributeSetsOf(chosen.element, text, trial.attributeSets()))
    {
        const Result<void> result = chosen.setter->set(component, value, attributesOf(attributes),
                                                       trial.warnings(), trial.fixture().context());
        static_cast<void>(trial.fixture().rocket().getLength());
        if (!result.has_value() && chosen.element != "id")
        {
            return std::format("the setter failed with [{}]: {}", attributes,
                               result.error().message);
        }
    }
    return {};
}

/// Applies every hostile text through every setter of @p component in the order @p order, and
/// asks the rocket for its length after each. Returns what was thrown, or which setter failed,
/// and where; "" when nothing went wrong.
[[nodiscard]] std::string applyHostileTexts(ApplicationSetterFixture& fixture,
                                            RocketComponent& component, Order order,
                                            AttributeSets attributeSets)
{
    const std::vector<ElementSetter> setters = settersOf(component.kind());
    const std::size_t                total   = setters.size() * kHostileTexts.size();
    Trial                            trial(fixture, attributeSets);
    std::string_view                 element;
    std::string_view                 text;
    std::string                      wrong;
    try
    {
        for (std::size_t step = 0; step < total && wrong.empty(); ++step)
        {
            const std::size_t    i = order.backwards ? total - 1 - step : step;
            const ElementSetter& chosen =
                setters.at(order.textsFirst ? i % setters.size() : i / kHostileTexts.size());
            element = chosen.element;
            text =
                kHostileTexts.at(order.textsFirst ? i / setters.size() : i % kHostileTexts.size());
            wrong = applyOne(trial, component, chosen, text);
        }
    }
    catch (const std::exception& error)
    {
        wrong = error.what();
    }
    if (wrong.empty())
    {
        return wrong;
    }
    return std::format("<{}>{}</{}> on a {}: {}", element, text, element,
                       className(component.kind()), wrong);
}

/// What the loader's last steps and a first look at the rocket throw, or "".
[[nodiscard]] std::string useTheRocket(Rocket& rocket)
{
    try
    {
        // GeneralRocketLoader ends a load with enableEvents(), which updates the whole rocket.
        rocket.enableEvents();
        rocket.update();
        static_cast<void>(rocket.getSelectedConfiguration().getActiveComponents());
        static_cast<void>(rocket.getLength());
    }
    catch (const std::exception& error)
    {
        return std::format("the rocket afterwards: {}", error.what());
    }
    return {};
}

/// What a component of each kind, alone in the tree of a SetterFixture, throws for the hostile
/// texts; empty when no kind throws.
[[nodiscard]] Texts thrownByEachKindAlone(Order order)
{
    Texts thrown;
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        ApplicationSetterFixture fixture;
        RocketComponent&         component = fixture.make(kind);
        std::string what = applyHostileTexts(fixture, component, order, AttributeSets::EVERY);
        if (what.empty())
        {
            what = useTheRocket(fixture.rocket());
        }
        if (!what.empty())
        {
            thrown.push_back(std::move(what));
        }
    }
    return thrown;
}

/// A rocket with one component of every kind a file can hold: the nose cone and the transition
/// in the stage, everything else in the body tube. Returns the components, the rocket first.
[[nodiscard]] std::vector<RocketComponent*> buildARocketOfEveryKind(
    ApplicationSetterFixture& fixture)
{
    std::vector<RocketComponent*> components{&fixture.rocket(), &fixture.setters().stage(),
                                             &fixture.setters().tube()};
    for (const ComponentKind kind : QtRocket::kAllComponentKinds)
    {
        if (kind == ComponentKind::ROCKET || kind == ComponentKind::AXIAL_STAGE ||
            kind == ComponentKind::BODY_TUBE)
        {
            continue;
        }
        std::unique_ptr<RocketComponent> component = DocumentConfig::createComponent(xmlName(kind));
        const bool inStage = kind == ComponentKind::NOSE_CONE || kind == ComponentKind::TRANSITION;
        RocketComponent& parent = inStage ? static_cast<RocketComponent&>(fixture.setters().stage())
                                          : fixture.setters().tube();
        components.push_back(&parent.addChild(std::move(component)));
    }
    return components;
}

/// What the components of one rocket of every kind throw for the hostile texts, each component
/// in turn, so that a value of one component meets the values of the others (a radius taken
/// from a neighbour, the body a fin set stands on, the length of the whole rocket). A setter
/// that reads attributes gets two of its sets of them per text here; thrownByEachKindAlone()
/// goes through every set with every text.
[[nodiscard]] Texts thrownByARocketOfEveryKind(Order order)
{
    ApplicationSetterFixture            fixture;
    const std::vector<RocketComponent*> components = buildARocketOfEveryKind(fixture);
    Texts                               thrown;
    for (RocketComponent* const component : components)
    {
        if (std::string what =
                applyHostileTexts(fixture, *component, order, AttributeSets::TWO_PER_TEXT);
            !what.empty())
        {
            thrown.push_back(std::move(what));
        }
    }
    if (std::string what = useTheRocket(fixture.rocket()); !what.empty())
    {
        thrown.push_back(std::move(what));
    }
    return thrown;
}

TEST(SetterRobustness, NoTextMakesASetterOfAnyKindThrow)
{
    EXPECT_EQ(thrownByEachKindAlone({.textsFirst = true, .backwards = false}), Texts{});
    EXPECT_EQ(thrownByEachKindAlone({.textsFirst = false, .backwards = false}), Texts{});
    EXPECT_EQ(thrownByEachKindAlone({.textsFirst = true, .backwards = true}), Texts{});
    EXPECT_EQ(thrownByEachKindAlone({.textsFirst = false, .backwards = true}), Texts{});
}

// Found by this test: an elliptical fin set with a negative root chord and a cant has no root
// point in OpenRocket, whose loader then dies of an IndexOutOfBoundsException at
// <cordlength>auto</cordlength> of a shock cord in the same rocket (FinSet has the fix).
TEST(SetterRobustness, NoTextMakesASetterThrowInARocketOfEveryKind)
{
    EXPECT_EQ(thrownByARocketOfEveryKind({.textsFirst = true, .backwards = false}), Texts{});
    EXPECT_EQ(thrownByARocketOfEveryKind({.textsFirst = false, .backwards = true}), Texts{});
}

/// What a component of @p kind is left with when @p element is set to the largest count the
/// loader lets through and another change is made: "<class> <instance count> <warnings>"; a
/// throw is told in its place.
[[nodiscard]] std::string countAtTheMaximum(ComponentKind kind, std::string_view element)
{
    ApplicationSetterFixture           fixture;
    RocketComponent&                   component = fixture.make(kind);
    WarningSet                         warnings;
    const DocumentConfig::SetterLookup lookup = DocumentConfig::findSetter(kind, element);
    if (lookup.setter == nullptr)
    {
        return "no setter";
    }
    try
    {
        if (!lookup.setter->set(component, "10000", {}, warnings, fixture.context()).has_value())
        {
            return "the setter failed";
        }
        // Another event, with the count in place.
        component.setName("counted");
        fixture.rocket().enableEvents();
    }
    catch (const std::exception& error)
    {
        return error.what();
    }
    return std::format("{} {} {}", className(kind), component.getInstanceCount(), warnings.size());
}

// DocumentConfig::kMaxCount instances are built at once.
TEST(SetterRobustness, ACountAtTheLoadersMaximumIsBuilt)
{
    EXPECT_EQ(countAtTheMaximum(ComponentKind::LAUNCH_LUG, "instancecount"), "LaunchLug 10000 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::RAIL_BUTTON, "instancecount"), "RailButton 10000 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::CENTERING_RING, "instancecount"),
              "CenteringRing 10000 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::BULKHEAD, "instancecount"), "Bulkhead 10000 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::POD_SET, "instancecount"), "PodSet 10000 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::PARALLEL_STAGE, "instancecount"),
              "ParallelStage 10000 0");
    // A fin set bounds its fins itself.
    EXPECT_EQ(countAtTheMaximum(ComponentKind::TRAPEZOID_FIN_SET, "fincount"),
              "TrapezoidFinSet 8 0");
    EXPECT_EQ(countAtTheMaximum(ComponentKind::TUBE_FIN_SET, "instancecount"), "TubeFinSet 8 0");
}

}  // namespace
