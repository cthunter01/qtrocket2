#include "file/openrocket/DesignFileState.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/DocumentLoadingContext.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/OpenRocketHandler.h"
#include "QtRocket/file/simplesax/SimpleSax.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/mass/RigidBody.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/motor/ThrustCurveMotor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Strings.h"
#include "Sha256.h"
#include "file/DesignFileEnvironment.h"
#include "file/RocketLoaderTestSupport.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "file/openrocket/SimulationTestSupport.h"
#include "unit/DefaultUnitsGuard.h"

namespace QtRocket::Test
{

namespace
{

/// A motor mount of a rocket and its place among the lines of describeRocket() (the rocket is
/// 0).
struct PlacedMount
{
    int               place;
    const MotorMount* mount;
};

/// The components of @p rocket that are motor mounts (MotorMount::isMotorMount()), in the order
/// of the tree.
[[nodiscard]] std::vector<PlacedMount> motorMounts(const Rocket& rocket)
{
    std::vector<PlacedMount> mounts;
    int                      place = 0;
    rocket.forEach([&mounts, &place](const RocketComponent& component) {
        const auto* const mount = dynamic_cast<const MotorMount*>(&component);
        if (mount != nullptr && mount->isMotorMount())
        {
            mounts.push_back({.place = place, .mount = mount});
        }
        place++;
    });
    return mounts;
}

/// What @p motorConfig holds, as a motor line ends: "<manufacturer>|<designation>|<digest>|
/// delay=<ejection delay>", or "none" without a motor.
[[nodiscard]] std::string motorText(const MotorConfiguration& motorConfig)
{
    const auto* const motor = dynamic_cast<const ThrustCurveMotor*>(motorConfig.getMotor().get());
    if (motor == nullptr)
    {
        return "none";
    }
    return std::format("{}|{}|{}|delay={}", motor->getManufacturer().getDisplayName(),
                       motor->getDesignation(), motor->getDigest(),
                       Strings::javaDoubleToString(motorConfig.getEjectionDelay()));
}

}  // namespace

std::string versionLine(const DesignFileEnvironment& environment, std::string_view text)
{
    DocumentLoadingContext                    context = environment.context();
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createEmptyRocket();
    context.setOpenRocketDocument(document.get());
    context.setFileVersion(0);
    OpenRocketHandler            handler(context);
    WarningSet                   warnings;
    const std::vector<std::byte> bytes = stringToBytes(text);
    const Result<void>           read  = SimpleSax::readXml(bytes, handler, warnings);
    return (read ? std::string() : failureLine(read.error())) +
           std::format("version={}\n", context.getFileVersion());
}

std::string nameLines(const OpenRocketDocument& document, const Preferences& preferences)
{
    const Rocket& rocket = document.getRocket();
    std::string lines = std::format("name default '{}'\n",
                                    onOneLine(rocket.getEmptyConfiguration().getName(preferences)));
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    for (std::size_t index = 0; index < ids.size(); index++)
    {
        lines +=
            std::format("name {} '{}'\n", index,
                        onOneLine(rocket.getFlightConfiguration(ids[index]).getName(preferences)));
    }
    std::size_t number = 0;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        const auto at = std::ranges::find(ids, simulation->getFlightConfigurationId());
        lines += std::format("simulation {} config={}\n", number++,
                             at == ids.end() ? -1 : std::ranges::distance(ids.begin(), at));
    }
    return lines;
}

std::string motorLines(const Rocket& rocket)
{
    const std::vector<PlacedMount> mounts = motorMounts(rocket);
    std::string                    lines;
    int                            config = 0;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        for (const PlacedMount& placed : mounts)
        {
            lines += std::format("motor config={} mount=#{} {}\n", config, placed.place,
                                 motorText(placed.mount->getMotorConfig(id)));
        }
        config++;
    }
    return lines;
}

std::string decalLines(const OpenRocketDocument& document)
{
    std::vector<std::string> lines;
    for (const std::shared_ptr<DecalImage>& image : document.getDecalList())
    {
        const Result<std::vector<std::byte>> bytes = image->getBytes();
        lines.push_back(std::format(
            "decal '{}' {}\n", image->getName(),
            bytes ? std::format("bytes={}", bytes->size()) : std::string("unreadable")));
    }
    std::ranges::sort(lines);
    std::string text;
    for (const std::string& line : lines)
    {
        text += line;
    }
    return text;
}

std::string simulationLines(const OpenRocketDocument& document)
{
    std::string text;
    std::size_t index = 0;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        text += describeSimulation(index++, *simulation, document.getRocket(),
                                   {.allIds = false, .digest = true});
    }
    return text;
}

std::vector<std::string> linesOf(std::string_view text)
{
    std::vector<std::string> lines = Strings::split(text, '\n');
    if (!lines.empty() && lines.back().empty())
    {
        lines.pop_back();
    }
    return lines;
}

std::string documentPreferenceLines(const OpenRocketDocument& document)
{
    std::string lines = std::format("docprefs={}\n", document.getDocumentPreferences().size());
    for (const Material& material : document.getDocumentMaterials().allMaterials())
    {
        lines += std::format("docmaterial {}\n", onOneLine(material.toStorableString()));
    }
    return lines;
}

std::string loadedDocumentState(const Result<LoadedDocument>& loaded, const LoadEvents& events,
                                const Preferences& preferences, std::string_view document,
                                StateDetail detail)
{
    if (!loaded)
    {
        return failureLine(loaded.error());
    }
    const Rocket& rocket = loaded->document->getRocket();
    std::string   text   = warningLines(loaded->warnings);
    text += documentLines(*loaded->document, nullptr);
    text += std::format("events rocket={}\n", events.rocket);
    for (const std::string& line : describeRocket(rocket, preferences, knownIds(document)))
    {
        text += "| " + line + "\n";
    }
    text += nameLines(*loaded->document, preferences);
    text += motorLines(rocket);
    text += decalLines(*loaded->document);
    text += simulationLines(*loaded->document);
    if (detail == StateDetail::WITH_DOCUMENT_PREFERENCES)
    {
        text += documentPreferenceLines(*loaded->document);
    }
    return text;
}

std::vector<std::string> designFileState(DesignFileEnvironment&       environment,
                                         const std::filesystem::path& file, StateDetail detail)
{
    const DefaultUnitsGuard           units;
    const std::string                 document = documentOfDesignFile(file);
    const std::string                 version  = versionLine(environment, document);
    const std::shared_ptr<LoadEvents> events   = std::make_shared<LoadEvents>();
    const GeneralRocketLoader         loader(environment.context(), countingOptions(events));
    const Result<LoadedDocument>      loaded = loader.load(file);
    return linesOf(version + loadedDocumentState(loaded, *events, environment.preferences(),
                                                 document, detail));
}

namespace
{

/// The marks of the long lines of a stored branch, behind their indentation.
constexpr std::string_view kTypesMark  = "types=";
constexpr std::string_view kColumnMark = "col ";
/// What separates the types of a "types=" line.
constexpr std::string_view kTypeSeparator = " | ";

/// The number of blanks @p line starts with.
[[nodiscard]] std::size_t indentationOf(std::string_view line)
{
    const std::size_t first = line.find_first_not_of(' ');
    return first == std::string_view::npos ? line.size() : first;
}

/// The SHA-256 of @p text.
[[nodiscard]] std::string digestOf(std::string_view text)
{
    const std::vector<std::byte> bytes = stringToBytes(text);
    return sha256Hex(bytes);
}

/// How often @p part occurs in @p text.
[[nodiscard]] std::size_t occurrences(std::string_view text, std::string_view part)
{
    std::size_t count = 0;
    for (std::size_t at = text.find(part); at != std::string_view::npos;
         at             = text.find(part, at + part.size()))
    {
        count++;
    }
    return count;
}

}  // namespace

std::vector<CompactLine> compactState(const std::vector<std::string>& state)
{
    std::vector<CompactLine> compact;
    std::size_t              index = 0;
    while (index < state.size())
    {
        const std::string&     line   = state[index];
        const std::size_t      indent = indentationOf(line);
        const std::string_view body   = std::string_view(line).substr(indent);
        if (body.starts_with(kTypesMark))
        {
            compact.push_back(
                {.text   = std::format("{}types={} sha256={}", line.substr(0, indent),
                                       occurrences(body, kTypeSeparator) + 1, digestOf(body)),
                 .detail = line + "\n"});
            index++;
            continue;
        }
        if (!body.starts_with(kColumnMark))
        {
            compact.push_back({.text = line, .detail = {}});
            index++;
            continue;
        }
        std::string run;
        std::size_t columns = 0;
        while (index < state.size() && std::string_view(state[index])
                                           .substr(indentationOf(state[index]))
                                           .starts_with(kColumnMark))
        {
            run += state[index] + "\n";
            columns++;
            index++;
        }
        compact.push_back({.text   = std::format("{}cols={} sha256={}", line.substr(0, indent),
                                                 columns, digestOf(run)),
                           .detail = run});
    }
    return compact;
}

std::vector<std::string> textsOf(const std::vector<CompactLine>& lines)
{
    std::vector<std::string> texts;
    texts.reserve(lines.size());
    for (const CompactLine& line : lines)
    {
        texts.push_back(line.text);
    }
    return texts;
}

std::vector<DesignNumber> massAndLength(const Rocket& rocket)
{
    FlightConfiguration all = rocket.getEmptyConfiguration().clone();
    all.setAllStages();
    const RigidBody           structure = MassCalculator::calculateStructure(all);
    std::vector<DesignNumber> numbers{
        {.name = "structure.mass", .value = structure.getMass()},
        {.name = "structure.cm.x", .value = structure.getCM().x},
        {.name = "structure.cm.y", .value = structure.getCM().y},
        {.name = "structure.cm.z", .value = structure.getCM().z},
    };
    int index = 0;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        FlightConfiguration config = rocket.getFlightConfiguration(id).clone();
        config.setAllStages();
        const RigidBody launch = MassCalculator::calculateLaunch(config);
        numbers.push_back(
            {.name = std::format("launch[{}].mass", index), .value = launch.getMass()});
        numbers.push_back(
            {.name = std::format("launch[{}].cm.x", index), .value = launch.getCM().x});
        index++;
    }
    numbers.push_back({.name = "length", .value = all.getLength()});
    return numbers;
}

std::string firstDifference(std::span<const std::string_view> expected,
                            const std::vector<std::string>&   found)
{
    const std::size_t common = std::min(expected.size(), found.size());
    for (std::size_t i = 0; i < common; i++)
    {
        if (expected[i] != found[i])
        {
            return std::format("line {} of {} (found {}):\nexpected: {}\nfound:    {}", i + 1,
                               expected.size(), found.size(), expected[i], found[i]);
        }
    }
    if (expected.size() == found.size())
    {
        return "";
    }
    return std::format("{} lines expected, {} found; the first one more: {}", expected.size(),
                       found.size(),
                       expected.size() > common ? std::string(expected[common]) : found[common]);
}

std::string firstDifference(std::span<const std::string_view> expected,
                            const std::vector<CompactLine>&   found)
{
    std::string difference = firstDifference(expected, textsOf(found));
    for (std::size_t i = 0; i < std::min(expected.size(), found.size()); i++)
    {
        if (expected[i] != found[i].text)
        {
            if (!found[i].detail.empty())
            {
                difference += "\nthe lines found there:\n" + found[i].detail;
            }
            break;
        }
    }
    return difference;
}

bool isCloseTo(double expected, double found) noexcept
{
    return std::abs(found - expected) <= (1e-9 * std::abs(expected)) + 1e-15;
}

std::string wrongNumbers(std::span<const std::string_view> expected,
                         const std::vector<DesignNumber>&  found)
{
    if (expected.size() != found.size())
    {
        return std::format("{} numbers expected, {} found", expected.size(), found.size());
    }
    std::string report;
    for (std::size_t i = 0; i < found.size(); i++)
    {
        const std::size_t           equals = expected[i].find('=');
        const std::optional<double> value =
            Strings::javaParseDouble(expected[i].substr(equals + 1));
        if (expected[i].substr(0, equals) != found[i].name || !value.has_value() ||
            !isCloseTo(*value, found[i].value))
        {
            report += std::format("expected {}, found {}={}\n", expected[i], found[i].name,
                                  Strings::javaDoubleToString(found[i].value));
        }
    }
    return report;
}

}  // namespace QtRocket::Test
