#include "QtRocket/file/motor/RockSimMotorLoader.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <expected>
#include <format>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <pugixml.hpp>

#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/motor/MotorDigest.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"

namespace QtRocket
{

namespace
{

/// An element's attributes by local name (Java's HashMap<String, String>).
using Attributes = std::map<std::string, std::string, std::less<>>;

/// A handler of one XML element as OpenRocket's SimpleSAX calls it (ElementHandler). The
/// handlers' warnings are not modelled: RockSimMotorLoader collects them in a WarningSet it
/// discards, and endHandler() is a no-op for every handler it uses.
class ElementHandler
{
public:
    ElementHandler()                                 = default;
    ElementHandler(const ElementHandler&)            = delete;
    ElementHandler(ElementHandler&&)                 = delete;
    ElementHandler& operator=(const ElementHandler&) = delete;
    ElementHandler& operator=(ElementHandler&&)      = delete;
    virtual ~ElementHandler()                        = default;

    /// The handler of the child element @p element, nullptr to ignore it and everything in it,
    /// or a failure (Java's SAXException).
    [[nodiscard]] virtual Result<ElementHandler*> openElement(std::string_view  element,
                                                              const Attributes& attributes) = 0;

    /// Called when a child element this handler did not ignore closes, with the attributes and
    /// text content DelegatorHandler pops for it.
    [[nodiscard]] virtual Result<void> closeElement(std::string_view  element,
                                                    const Attributes& attributes,
                                                    std::string_view  content) = 0;
};

/// PlainTextHandler and NullElementHandler as the loader sees them: every child element is
/// ignored (with a warning OpenRocket discards), and there is nothing to close.
class IgnoringHandler final : public ElementHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view /*element*/,
                                                      const Attributes& /*attributes*/) override
    {
        return nullptr;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view /*element*/,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/) override
    {
        return {};
    }
};

/// OpenRocket's DelegatorHandler: hands SAX events to the handler stack. Kept exactly, including
/// its bookkeeping slip: an ignored element pushes a text buffer and its attributes but never pops
/// them, so the enclosing element is later closed with those (see RockSimMotorLoader).
class Delegator
{
public:
    explicit Delegator(ElementHandler& initialHandler) : m_handlers{&initialHandler}
    {
        m_elementData.emplace_back();  // Just in case
    }

    [[nodiscard]] Result<void> startElement(std::string_view name, Attributes attributes)
    {
        // Check for ignore
        if (m_ignore > 0)
        {
            m_ignore++;
            return {};
        }

        // Add layer to data stacks
        m_elementData.emplace_back();
        m_elementAttributes.push_back(std::move(attributes));

        // Call the handler
        Result<ElementHandler*> handler =
            m_handlers.back()->openElement(name, m_elementAttributes.back());
        if (!handler)
        {
            return std::unexpected(std::move(handler.error()));
        }
        if (*handler != nullptr)
        {
            m_handlers.push_back(*handler);
        }
        else
        {
            // Start ignoring elements
            m_ignore++;
        }
        return {};
    }

    /// Stores encountered characters in the element data stack.
    void characters(std::string_view text)
    {
        if (m_ignore > 0)
        {
            return;
        }
        m_elementData.back() += text;
    }

    /// Removes the last layer from the stack.
    [[nodiscard]] Result<void> endElement(std::string_view name)
    {
        // Check for ignore
        if (m_ignore > 0)
        {
            m_ignore--;
            return {};
        }
        QTROCKET_ASSERT(m_handlers.size() > 1 && !m_elementAttributes.empty());

        // Remove data from stack
        const std::string content = std::move(m_elementData.back());
        m_elementData.pop_back();
        const Attributes attributes = std::move(m_elementAttributes.back());
        m_elementAttributes.pop_back();

        // Remove last handler (its endHandler() is a no-op) and call the next one
        m_handlers.pop_back();
        return m_handlers.back()->closeElement(name, attributes, content);
    }

private:
    std::vector<ElementHandler*> m_handlers;
    std::vector<std::string>     m_elementData;
    std::vector<Attributes>      m_elementAttributes;
    // Ignore all elements as long as m_ignore > 0
    int m_ignore{0};
};

/// The local name of a qualified XML name ("r:engine" gives "engine"), which a namespace-aware
/// SAX parser reports.
[[nodiscard]] std::string_view localName(std::string_view name) noexcept
{
    const std::size_t colon = name.find(':');
    return colon == std::string_view::npos ? name : name.substr(colon + 1);
}

[[nodiscard]] bool isXmlWhitespace(std::string_view text) noexcept
{
    return text.find_first_not_of(" \t\r\n") == std::string_view::npos;
}

/// The attributes of @p element as the SAX parser reports them: by local name, without the
/// namespace declarations. A repeated attribute is an error, as XML requires.
[[nodiscard]] Result<Attributes> attributesOf(const pugi::xml_node& element)
{
    Attributes attributes;
    Attributes seen;
    for (const pugi::xml_attribute& attribute : element.attributes())
    {
        const std::string_view name = attribute.name();
        if (!seen.try_emplace(std::string(name)).second)
        {
            return fail(ErrorCode::PARSE,
                        std::format(R"(Attribute "{}" was already specified for element "{}".)",
                                    name, element.name()));
        }
        if (name == "xmlns" || name.starts_with("xmlns:"))
        {
            continue;
        }
        attributes.insert_or_assign(std::string(localName(name)), std::string(attribute.value()));
    }
    return attributes;
}

[[nodiscard]] Result<void> startElement(Delegator& sax, const pugi::xml_node& element)
{
    Result<Attributes> attributes = attributesOf(element);
    if (!attributes)
    {
        return std::unexpected(std::move(attributes.error()));
    }
    return sax.startElement(localName(element.name()), std::move(*attributes));
}

/// Feeds the SAX events of the element @p root and its content to @p sax, in document order and
/// without recursion, so that deep nesting cannot exhaust the stack.
[[nodiscard]] Result<void> readElement(const pugi::xml_node& root, Delegator& sax)
{
    if (Result<void> started = startElement(sax, root); !started)
    {
        return started;
    }
    pugi::xml_node parent = root;
    pugi::xml_node node   = root.first_child();
    while (true)
    {
        if (node.empty())
        {
            if (Result<void> ended = sax.endElement(localName(parent.name())); !ended)
            {
                return ended;
            }
            if (parent == root)
            {
                return {};
            }
            node   = parent.next_sibling();
            parent = parent.parent();
            continue;
        }
        switch (node.type())
        {
            case pugi::node_element:
                if (Result<void> started = startElement(sax, node); !started)
                {
                    return started;
                }
                parent = node;
                node   = node.first_child();
                continue;
            case pugi::node_pcdata:
            case pugi::node_cdata:
                sax.characters(node.value());
                break;
            default:  // comments and processing instructions are not character data
                break;
        }
        node = node.next_sibling();
    }
}

/// Parses @p text and feeds it to @p sax, with the checks of Java's parser that pugixml does not
/// make: a byte-order mark or text outside the root element, no root element, a second one.
[[nodiscard]] Result<void> readXml(std::string_view text, Delegator& sax)
{
    // InputStreamReader keeps a byte-order mark as a character, which the parser then rejects.
    if (text.starts_with("\xEF\xBB\xBF"))
    {
        return fail(ErrorCode::PARSE, "Content is not allowed in prolog.");
    }
    pugi::xml_document document;
    // parse_fragment keeps text outside the root and accepts a document without one, so that the
    // loop below can reject both with Java's messages; parse_ws_pcdata keeps whitespace-only
    // text, which SAX reports as characters too.
    const pugi::xml_parse_result parsed = document.load_buffer(
        text.data(), text.size(),
        pugi::parse_default | pugi::parse_ws_pcdata | pugi::parse_fragment, pugi::encoding_utf8);
    if (!parsed)
    {
        return fail(ErrorCode::PARSE,
                    std::format("{} (at offset {})", parsed.description(), parsed.offset));
    }

    pugi::xml_node root;
    for (const pugi::xml_node& node : document.children())
    {
        if (node.type() == pugi::node_element)
        {
            if (!root.empty())
            {
                return fail(ErrorCode::PARSE,
                            "The markup in the document following the root "
                            "element must be well-formed.");
            }
            root = node;
        }
        else if ((node.type() == pugi::node_pcdata || node.type() == pugi::node_cdata) &&
                 !isXmlWhitespace(node.value()))
        {
            return fail(ErrorCode::PARSE, !root.empty()
                                              ? "Content is not allowed in trailing section."
                                              : "Content is not allowed in prolog.");
        }
    }
    if (root.empty())
    {
        return fail(ErrorCode::PARSE, "Premature end of file.");
    }
    return readElement(root, sax);
}

/// The attribute @p name of @p attributes, when present.
[[nodiscard]] std::optional<std::string_view> attribute(const Attributes& attributes,
                                                        std::string_view  name)
{
    const auto it = attributes.find(name);
    if (it == attributes.end())
    {
        return std::nullopt;
    }
    return it->second;
}

/// RSEMotorDataHandler.parseDouble(): the number, or NaN when missing or not a number.
[[nodiscard]] double parseOrNaN(std::optional<std::string_view> text)
{
    if (!text.has_value())
    {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return Strings::javaParseDouble(*text).value_or(std::numeric_limits<double>::quiet_NaN());
}

/// RockSimMotorLoader.hasIllegalValue(): a NaN or infinite value.
[[nodiscard]] bool hasIllegalValue(const std::vector<double>& list)
{
    return std::ranges::any_of(list, [](double d) { return std::isnan(d) || std::isinf(d); });
}

/// "0" or "false" (any case) turns an automatic calculation off; anything else, absence included,
/// leaves it on.
[[nodiscard]] bool isAutomatic(std::optional<std::string_view> value)
{
    return !(value.has_value() &&
             (*value == "0" || Strings::javaEqualsIgnoreCase(*value, "false")));
}

/// The engine's delays attribute: comma-separated numbers, those from
/// RockSimMotorLoader::kDelayLimit up plugged; "P" or "plugged" as the whole attribute is plugged.
[[nodiscard]] std::vector<double> parseDelays(std::optional<std::string_view> delays)
{
    std::vector<double> parsed;
    if (!delays.has_value())
    {
        return parsed;
    }
    for (const std::string& delay : Strings::splitJava(*delays, ','))
    {
        if (const std::optional<double> value = Strings::javaParseDouble(delay))
        {
            parsed.push_back(*value >= RockSimMotorLoader::kDelayLimit ? Motor::kPluggedDelay
                                                                       : *value);
        }
        // OpenRocket tests the whole attribute here, not the piece that failed to parse, so a "P"
        // among other delays is dropped.
        else if (Strings::javaEqualsIgnoreCase(*delays, "P") ||
                 Strings::javaEqualsIgnoreCase(*delays, "plugged"))
        {
            parsed.push_back(Motor::kPluggedDelay);
        }
    }
    return parsed;
}

/// The attribute @p name, a length in mm or a mass in g, in m or kg; OpenRocket's @p missing
/// message when it is absent, "Invalid <what> <value>" when it is no number.
[[nodiscard]] Result<double> readNumber(const Attributes& attributes, std::string_view name,
                                        std::string_view what, std::string_view missing)
{
    const std::optional<std::string_view> text = attribute(attributes, name);
    if (!text.has_value())
    {
        return fail(ErrorCode::PARSE, std::string(missing));
    }
    const std::optional<double> value = Strings::javaParseDouble(*text);
    if (!value.has_value())
    {
        return fail(ErrorCode::PARSE, std::format("Invalid {} {}", what, *text));
    }
    return *value / 1000.0;
}

/// The engine's Type attribute: the type, and whether it is explicitly "unknown".
[[nodiscard]] std::pair<Motor::Type, bool> parseType(std::optional<std::string_view> type)
{
    const auto is = [&type](std::string_view name) {
        return type.has_value() && Strings::javaEqualsIgnoreCase(*type, name);
    };
    const bool explicitlyUnknown = is("unknown");
    if (is("single-use"))
    {
        return {Motor::Type::SINGLE, explicitlyUnknown};
    }
    if (is("hybrid"))
    {
        return {Motor::Type::HYBRID, explicitlyUnknown};
    }
    if (is("reloadable"))
    {
        return {Motor::Type::RELOAD, explicitlyUnknown};
    }
    return {Motor::Type::UNKNOWN, explicitlyUnknown};
}

}  // namespace

/// Handler for the <data> element in a RockSim engine file motor definition.
class RockSimMotorLoader::RseMotorDataHandler final : public ElementHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view element,
                                                      const Attributes& /*attributes*/) override
    {
        if (element == "eng-data")
        {
            return &m_nullHandler;
        }
        return nullptr;  // Unknown element, ignoring
    }

    [[nodiscard]] Result<void> closeElement(std::string_view /*element*/,
                                            const Attributes& attributes,
                                            std::string_view /*content*/) override
    {
        const double t = parseOrNaN(attribute(attributes, "t"));
        const double f = parseOrNaN(attribute(attributes, "f"));
        const double m = parseOrNaN(attribute(attributes, "m")) / 1000.0;
        const double g = parseOrNaN(attribute(attributes, "cg")) / 1000.0;

        if (std::isnan(t) || std::isnan(f))
        {
            return fail(ErrorCode::PARSE, "Illegal motor data point encountered");
        }

        m_time.push_back(t);
        m_force.push_back(f);
        m_mass.push_back(m);
        m_cg.push_back(g);
        return {};
    }

    std::vector<double> takeTime() { return std::exchange(m_time, {}); }
    std::vector<double> takeForce() { return std::exchange(m_force, {}); }
    std::vector<double> takeMass() { return std::exchange(m_mass, {}); }
    std::vector<double> takeCg() { return std::exchange(m_cg, {}); }

private:
    IgnoringHandler     m_nullHandler;
    std::vector<double> m_time;
    std::vector<double> m_force;
    std::vector<double> m_mass;
    std::vector<double> m_cg;
};

/// Handler for a RockSim engine file <engine> element.
class RockSimMotorLoader::RseMotorHandler final : public ElementHandler
{
public:
    /// An engine without attributes; create() reads them.
    RseMotorHandler() = default;

    /// The handler of an engine with @p attributes, or OpenRocket's SAXException for a missing or
    /// invalid one.
    [[nodiscard]] static Result<std::unique_ptr<RseMotorHandler>> create(
        const Attributes& attributes)
    {
        auto handler = std::make_unique<RseMotorHandler>();
        if (Result<void> read = handler->readAttributes(attributes); !read)
        {
            return std::unexpected(std::move(read.error()));
        }
        return handler;
    }

    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view element,
                                                      const Attributes& /*attributes*/) override
    {
        if (element == "comments")
        {
            return &m_plainTextHandler;
        }

        if (element == "data")
        {
            if (m_dataHandler != nullptr)
            {
                return fail(ErrorCode::PARSE,
                            "Multiple data elements encountered in motor definition");
            }
            m_dataHandler = std::make_unique<RseMotorDataHandler>();
            return m_dataHandler.get();
        }

        return nullptr;  // Unknown element, ignoring
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element,
                                            const Attributes& /*attributes*/,
                                            std::string_view content) override
    {
        if (element == "comments")
        {
            if (!m_description.empty())
            {
                m_description += "\n\n";
            }
            m_description += Strings::trim(content);
            return {};
        }

        if (element == "data")
        {
            QTROCKET_ASSERT(m_dataHandler != nullptr);
            m_time    = m_dataHandler->takeTime();
            m_force   = m_dataHandler->takeForce();
            m_mass    = m_dataHandler->takeMass();
            m_cg      = m_dataHandler->takeCg();
            m_hasData = true;

            sortLists(m_time, {m_force, m_mass, m_cg});

            if (std::ranges::any_of(m_mass, [](double d) { return std::isnan(d); }))
            {
                m_calculateMass = true;
            }
            if (std::ranges::any_of(m_cg, [](double d) { return std::isnan(d); }))
            {
                m_calculateCg = true;
            }
        }
        return {};
    }

    /// The motor (getMotor()), or OpenRocket's failure for missing or unusable data.
    [[nodiscard]] Result<ThrustCurveMotor::Builder> getMotor()
    {
        if (!m_hasData || m_time.empty())
        {
            return fail(ErrorCode::PARSE, "Illegal motor data");
        }

        if (Result<void> finished = finalizeThrustCurve(m_time, m_force, {m_mass, m_cg}); !finished)
        {
            return std::unexpected(std::move(finished.error()));
        }

        // The thrusts are removed with the times, so they stay as many.
        const std::size_t n = m_time.size();
        QTROCKET_ASSERT(m_force.size() == n);

        if (hasIllegalValue(m_mass))
        {
            m_calculateMass = true;
        }
        if (hasIllegalValue(m_cg))
        {
            m_calculateCg = true;
        }

        if (m_calculateMass)
        {
            m_mass = calculateMass(m_time, m_force, m_initMass, m_propMass);
        }

        if (m_calculateCg)
        {
            for (std::size_t i = 0; i < n; i++)
            {
                m_cg[i] = m_length / 2;
            }
        }

        std::vector<Coordinate> cgArray;
        cgArray.reserve(n);
        for (std::size_t i = 0; i < n; i++)
        {
            cgArray.emplace_back(m_cg[i], 0, 0, m_mass[i]);
        }

        // Create the motor digest from all data available in the file. The mass and CG lists can
        // be one longer than the curve (see finalizeThrustCurve) and are digested whole, as in
        // OpenRocket.
        MotorDigest motorDigest;
        motorDigest.update(MotorDigest::DataType::TIME_ARRAY, m_time);
        if (!m_calculateMass)
        {
            motorDigest.update(MotorDigest::DataType::MASS_PER_TIME, m_mass);
        }
        else
        {
            motorDigest.update(MotorDigest::DataType::MASS_SPECIFIC,
                               {m_initMass, m_initMass - m_propMass});
        }
        if (!m_calculateCg)
        {
            motorDigest.update(MotorDigest::DataType::CG_PER_TIME, m_cg);
        }
        motorDigest.update(MotorDigest::DataType::FORCE_PER_TIME, m_force);

        const Manufacturer& m = Manufacturer::getManufacturer(m_manufacturer);
        Motor::Type         t = m_type;
        // Preserve an explicit unknown type. Manufacturer inference remains useful for older RSE
        // files that omit the type or contain an unrecognized value. (OpenRocket logs a warning
        // when the type contradicts the manufacturer's; that log is not ported.)
        if (t == Motor::Type::UNKNOWN && !m_explicitlyUnknownType)
        {
            t = m.getMotorType();
        }

        ThrustCurveMotor::Builder builder;
        builder.setManufacturer(m)
            .setDesignation(m_designation)
            .setDescription(m_description)
            .setMotorType(t)
            .setStandardDelays(m_delays)
            .setDiameter(m_diameter)
            .setLength(m_length)
            .setTimePoints(m_time)
            .setThrustPoints(m_force)
            .setCGPoints(std::move(cgArray))
            .setDigest(motorDigest.getDigest());
        return builder;
    }

private:
    /// The constructor of RSEMotorHandler: reads the engine's attributes in OpenRocket's order.
    [[nodiscard]] Result<void> readAttributes(const Attributes& attributes)
    {
        // Manufacturer
        const std::optional<std::string_view> mfg = attribute(attributes, "mfg");
        if (!mfg.has_value())
        {
            return fail(ErrorCode::PARSE, "Manufacturer missing");
        }
        m_manufacturer = *mfg;

        // Designation
        const std::optional<std::string_view> code = attribute(attributes, "code");
        if (!code.has_value())
        {
            return fail(ErrorCode::PARSE, "Designation missing");
        }
        m_designation = removeDelay(*code);

        // Delays
        m_delays = parseDelays(attribute(attributes, "delays"));

        // Diameter, length, initial mass and propellant mass
        const Result<double> diameter =
            readNumber(attributes, "dia", "diameter", "Diameter missing");
        if (!diameter)
        {
            return std::unexpected(diameter.error());
        }
        m_diameter                  = *diameter;
        const Result<double> length = readNumber(attributes, "len", "length", "Length missing");
        if (!length)
        {
            return std::unexpected(length.error());
        }
        m_length = *length;
        const Result<double> initMass =
            readNumber(attributes, "initWt", "initial mass", "Initial mass missing");
        if (!initMass)
        {
            return std::unexpected(initMass.error());
        }
        m_initMass = *initMass;
        const Result<double> propMass =
            readNumber(attributes, "propWt", "propellant mass", "Propellant mass missing");
        if (!propMass)
        {
            return std::unexpected(propMass.error());
        }
        m_propMass = *propMass;

        if (m_propMass > m_initMass)
        {
            return fail(ErrorCode::PARSE,
                        "Propellant weight exceeds total weight in RockSim engine format");
        }

        // Motor type
        std::tie(m_type, m_explicitlyUnknownType) = parseType(attribute(attributes, "Type"));

        // Calculate mass and CG
        m_calculateMass = isAutomatic(attribute(attributes, "auto-calc-mass"));
        m_calculateCg   = isAutomatic(attribute(attributes, "auto-calc-cg"));
        return {};
    }

    std::string         m_manufacturer;
    std::string         m_designation;
    std::vector<double> m_delays;
    double              m_diameter{0};
    double              m_length{0};
    double              m_initMass{0};
    double              m_propMass{0};
    Motor::Type         m_type{Motor::Type::UNKNOWN};
    bool                m_explicitlyUnknownType{false};
    bool                m_calculateMass{false};
    bool                m_calculateCg{false};

    std::string m_description;

    bool                m_hasData{false};
    std::vector<double> m_time;
    std::vector<double> m_force;
    std::vector<double> m_mass;
    std::vector<double> m_cg;

    IgnoringHandler                      m_plainTextHandler;
    std::unique_ptr<RseMotorDataHandler> m_dataHandler;
};

/// Initial handler for the RockSim engine files.
class RockSimMotorLoader::RseHandler final : public ElementHandler
{
public:
    [[nodiscard]] Result<ElementHandler*> openElement(std::string_view  element,
                                                      const Attributes& attributes) override
    {
        if (element == "engine-database" || element == "engine-list")
        {
            // Ignore <engine-database> and <engine-list> elements
            return this;
        }

        if (element == "version")
        {
            // Ignore <version> elements completely
            return nullptr;
        }

        if (element == "engine")
        {
            Result<std::unique_ptr<RseMotorHandler>> handler = RseMotorHandler::create(attributes);
            if (!handler)
            {
                return std::unexpected(std::move(handler.error()));
            }
            m_motorHandler = std::move(*handler);
            return m_motorHandler.get();
        }

        return nullptr;
    }

    [[nodiscard]] Result<void> closeElement(std::string_view element,
                                            const Attributes& /*attributes*/,
                                            std::string_view /*content*/) override
    {
        if (element == "engine")
        {
            QTROCKET_ASSERT(m_motorHandler != nullptr);
            Result<ThrustCurveMotor::Builder> motor = m_motorHandler->getMotor();
            if (!motor)
            {
                return std::unexpected(std::move(motor.error()));
            }
            m_motors.push_back(std::move(*motor));
        }
        return {};
    }

    [[nodiscard]] std::vector<ThrustCurveMotor::Builder> takeMotors()
    {
        return std::exchange(m_motors, {});
    }

private:
    std::vector<ThrustCurveMotor::Builder> m_motors;
    std::unique_ptr<RseMotorHandler>       m_motorHandler;
};

Result<std::vector<ThrustCurveMotor::Builder>> RockSimMotorLoader::loadText(
    std::string_view text, std::string_view /*filename*/) const
{
    RseHandler handler;
    Delegator  sax(handler);
    if (Result<void> read = readXml(text, sax); !read)
    {
        return std::unexpected(std::move(read.error()));
    }
    return handler.takeMotors();
}

}  // namespace QtRocket
