#include "QtRocket/file/openrocket/OpenRocketSaver.h"

#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/file/openrocket/OrkWriter.h"
#include "QtRocket/file/openrocket/PhotoStudioSaver.h"
#include "QtRocket/logging/ErrorSet.h"
#include "QtRocket/logging/WarningSet.h"
#include "QtRocket/material/Material.h"
#include "QtRocket/material/MaterialDatabase.h"
#include "QtRocket/preferences/DocumentPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/simulation/FlightData.h"
#include "QtRocket/simulation/FlightDataBranch.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/customexpression/CustomExpression.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Config.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Version.h"

namespace QtRocket
{

namespace
{

/// "major.minor" as major * divisor + minor (the loader's reading of a version), or -1 for a text
/// of another form.
consteval int versionNumber(std::string_view text)
{
    const std::size_t point = text.find('.');
    if (point == std::string_view::npos || point == 0 || point + 1 == text.size())
    {
        return -1;
    }
    int major = 0;
    for (const char c : text.substr(0, point))
    {
        major = (major * 10) + (c - '0');
    }
    int minor = 0;
    for (const char c : text.substr(point + 1))
    {
        minor = (minor * 10) + (c - '0');
    }
    return (major * DocumentConfig::kFileVersionDivisor) + minor;
}

// Java's note in calculateNecessaryFileVersion(): "Remember to update the supported versions in
// DocumentConfig as well!" Here the compiler remembers it.
static_assert(versionNumber(DocumentConfig::kSupportedVersions.back()) ==
                  OpenRocketSaver::kFileVersion,
              "the saver writes the newest version the loader knows");

/// The lines OpenRocket's saver of @p component's class gives (Java: the getElements() that
/// findGetElementsMethod() finds): the open tag, the parameter lines and the close tag. Texts go
/// through the writer's escape(); the preferences decide whether a flight configuration's name is
/// its own (FlightConfiguration::isNameOverridden()).
///
/// SEAM(S2, component savers): until the component savers are ported, a component writes its
/// open and close tag only.
[[nodiscard]] std::vector<std::string> componentElements(const RocketComponent& component,
                                                         OrkWriter& /*writer*/,
                                                         const Preferences& /*preferences*/)
{
    const std::string_view name = xmlName(component.kind());
    return {std::format("<{}>", name), std::format("</{}>", name)};
}

/// A component whose subcomponents are being written: Java's saveComponent() between the
/// <subcomponents> of its children and its close tag.
struct OpenComponent
{
    std::vector<RocketComponent*> children;
    /// The child to write next.
    std::size_t next{0};
    /// The last line of the component's saver.
    std::string closeTag;
};

/// Java's saveComponent() up to the children: the open tag and the parameter lines of
/// @p component, then, when it has children, a blank line and <subcomponents> (the component
/// is returned to have them written); without children its close tag.
[[nodiscard]] std::optional<OpenComponent> openComponent(RocketComponent&   component,
                                                         OrkWriter&         writer,
                                                         const Preferences& preferences)
{
    std::vector<std::string> list = componentElements(component, writer, preferences);
    if (list.empty())
    {
        return std::nullopt;  // Nothing to do (Java)
    }
    // Java: RuntimeException "BUG, component data length less than two lines."
    QTROCKET_ASSERT(list.size() >= 2);

    writer.line(list.front());
    writer.indent();
    for (std::size_t i = 1; i + 1 < list.size(); ++i)
    {
        writer.line(list[i]);
    }
    if (component.getChildCount() > 0)
    {
        writer.blankLine();
        writer.line("<subcomponents>");
        writer.indent();
        return OpenComponent{
            .children = component.getChildren(), .next = 0, .closeTag = std::move(list.back())};
    }
    writer.outdent();
    writer.line(list.back());
    return std::nullopt;
}

/// Java's recursive saveComponent() of @p rocket and everything below it. The recursion is a
/// loop over a stack here, so that a tree of any depth that a caller built is written; the
/// order of the lines and of the savers' calls is Java's.
void saveComponents(RocketComponent& rocket, OrkWriter& writer, const Preferences& preferences)
{
    std::vector<OpenComponent> open;
    if (std::optional<OpenComponent> first = openComponent(rocket, writer, preferences))
    {
        open.push_back(std::move(*first));
    }
    while (!open.empty())
    {
        OpenComponent& top = open.back();
        if (top.next < top.children.size())
        {
            // A blank line between two children (Java: emptyline), whatever the first wrote.
            if (top.next > 0)
            {
                writer.blankLine();
            }
            RocketComponent& child = *top.children[top.next];
            ++top.next;
            if (std::optional<OpenComponent> opened = openComponent(child, writer, preferences))
            {
                open.push_back(std::move(*opened));  // top is not used after this
            }
            continue;
        }
        writer.outdent();
        writer.line("</subcomponents>");
        writer.outdent();
        writer.line(top.closeTag);
        open.pop_back();
    }
}

/// Java's saveCustomDatatypes() and saveCustomExpressionDatatype().
void saveCustomDatatypes(const OpenRocketDocument& document, OrkWriter& writer)
{
    const std::vector<CustomExpression>& expressions = document.getCustomExpressions();
    if (expressions.empty())
    {
        return;
    }
    writer.line("<datatypes>");
    writer.indent();
    for (const CustomExpression& expression : expressions)
    {
        writer.line("<type source=\"customexpression\">");
        writer.indent();
        writer.element("name", expression.getName());
        writer.element("symbol", expression.getSymbol());
        // "auto": the unit type is determined from the unit's text.
        writer.line(
            std::format("<unit unittype=\"auto\">{}</unit>", writer.escape(expression.getUnit())));
        writer.element("expression", expression.getExpressionString());
        writer.outdent();
        writer.line("</type>");
    }
    writer.outdent();
    writer.line("</datatypes>");
    writer.blankLine();
}

/// Java's saveSimulation(): the <simulation> element of @p simulation, with its data when
/// @p saveSimulationData. Save warnings go to @p warnings.
///
/// SEAM(S3, the simulation saver): until it is ported, a simulation writes nothing. It is to call
/// Simulation::getStatus() where Java does (OpenRocketSaver.java:331 and 534).
void saveSimulation(Simulation& /*simulation*/, bool /*saveSimulationData*/, OrkWriter& /*writer*/,
                    WarningSet& /*warnings*/)
{
}

/// The simulations: <simulations> with a blank line between two of them.
void saveSimulations(const OpenRocketDocument& document, bool saveSimulationData, OrkWriter& writer,
                     WarningSet& warnings)
{
    writer.line("<simulations>");
    writer.indent();
    bool first = true;
    for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
    {
        if (!first)
        {
            writer.blankLine();
        }
        first = false;
        saveSimulation(*simulation, saveSimulationData, writer, warnings);
    }
    writer.outdent();
    writer.line("</simulations>");
}

/// Java's savePhotoSettings(): written also when there are no settings.
void savePhotoSettings(const std::map<std::string, std::string>& settings, OrkWriter& writer)
{
    writer.line("<photostudio>");
    writer.indent();
    for (const std::string& element : PhotoStudioSaver::getElements(settings, writer))
    {
        writer.line(element);
    }
    writer.outdent();
    writer.line("</photostudio>");
}

/// The Config value of a document preference: the same Java type (Boolean, Integer, Double,
/// String), which writeEntry() prints alike.
[[nodiscard]] Config::Value configValueOf(const DocumentPreferences::Value& value)
{
    return std::visit([](const auto& held) { return Config::Value(held); }, value);
}

/// Java's saveDocumentPreferences(): the preferences, then the document materials.
void saveDocumentPreferences(const OpenRocketDocument& document, OrkWriter& writer)
{
    writer.line("<docprefs>");
    writer.indent();

    // Normal preferences
    for (const auto& [key, preference] : document.getDocumentPreferences().preferences())
    {
        writer.entry("pref", key, configValueOf(preference.value()), true);
    }

    // Document materials
    if (document.getDocumentMaterials().totalMaterialCount() > 0)
    {
        writer.line("<docmaterials>");
        writer.indent();
        const MaterialDatabase materials = document.getDocumentMaterials().allMaterials();
        for (const Material& material : materials)
        {
            writer.element("material", material.toStorableString());
        }
        writer.outdent();
        writer.line("</docmaterials>");
    }

    writer.outdent();
    writer.line("</docprefs>");
}

/// The save warnings of decisions E3 and O12 (see the header) for the text @p writer holds, which
/// is finished, of @p document.
void addSaveWarnings(const OpenRocketDocument& document, const OrkWriter& writer,
                     WarningSet& warnings)
{
    if (writer.droppedCharacters() > 0)
    {
        warnings.add(OpenRocketSaver::kDroppedCharacters);
    }
    if (writer.size() > GeneralRocketLoader::kMaxDocumentBytes)
    {
        warnings.add(OpenRocketSaver::documentBeyondLoaderBound(writer.size()));
    }
    const std::size_t components = DocumentConfig::componentCount(document.getRocket());
    if (components > DocumentConfig::kMaxComponents)
    {
        warnings.add(OpenRocketSaver::componentsBeyondLoaderBound(components));
    }
}

}  // namespace

std::string OpenRocketSaver::documentBeyondLoaderBound(std::size_t bytes)
{
    return std::format(
        "The saved design is {} bytes long; QtRocket loads a design of at most {} bytes and will "
        "not open this file.",
        bytes, GeneralRocketLoader::kMaxDocumentBytes);
}

std::string OpenRocketSaver::componentsBeyondLoaderBound(std::size_t components)
{
    return std::format(
        "The saved rocket has {} components; QtRocket loads at most {} and will ignore the ones "
        "beyond when it opens this file.",
        components, DocumentConfig::kMaxComponents);
}

std::string OpenRocketSaver::unsupportedFileType(StorageOptions::FileType fileType)
{
    return std::format(
        "OpenRocketSaver writes the OpenRocket format; the storage options ask for {}.",
        name(fileType));
}

Result<std::string> OpenRocketSaver::save(OpenRocketDocument&   document,
                                          const StorageOptions& options,
                                          const Preferences& preferences, WarningSet& warnings,
                                          ErrorSet& /*errors*/)
{
    if (options.getFileType() != StorageOptions::FileType::OPENROCKET)
    {
        return fail(ErrorCode::UNSUPPORTED_FORMAT, unsupportedFileType(options.getFileType()));
    }

    OrkWriter writer;

    // Select file version number
    const int fileVersion = calculateNecessaryFileVersion(document, options);

    writer.line("<?xml version='1.0' encoding='utf-8'?>");
    writer.line(std::format(R"(<openrocket version="{}" creator="{}">)",
                            fileVersionText(fileVersion), writer.escape(creatorString())));
    writer.indent();

    // Recursively save the rocket structure
    saveComponents(document.getRocket(), writer, preferences);

    writer.blankLine();

    // Save custom expressions
    saveCustomDatatypes(document, writer);

    // Save all simulations
    saveSimulations(document, options.getSaveSimulationData(), writer, warnings);

    // Save PhotoSettings
    savePhotoSettings(document.getPhotoSettings(), writer);

    // Save document preferences
    saveDocumentPreferences(document, writer);

    writer.outdent();
    writer.line("</openrocket>");

    addSaveWarnings(document, writer, warnings);
    return writer.release();
}

Result<std::string> OpenRocketSaver::save(OpenRocketDocument& document,
                                          const Preferences& preferences, WarningSet& warnings,
                                          ErrorSet& errors)
{
    // The options are read only; nothing a save does changes them.
    return save(document, document.getDefaultStorageOptions(), preferences, warnings, errors);
}

std::int64_t OpenRocketSaver::estimateFileSize(const OpenRocketDocument& document,
                                               const StorageOptions&     options)
{
    std::int64_t size = 0;

    // HOOK(decals): Java's "TODO - estimate decals".

    // Size per component (Java: rocket.iterator(true), the rocket included)
    std::int64_t componentCount = 0;
    for ([[maybe_unused]] const RocketComponent& component : document.getRocket().subtree(true))
    {
        ++componentCount;
    }
    size += componentCount * kBytesPerComponentCompressed;

    // Size per simulation
    size +=
        static_cast<std::int64_t>(document.getSimulationCount()) * kBytesPerSimulationCompressed;

    // Size per flight data point
    std::int64_t pointCount = 0;
    if (options.getSaveSimulationData())
    {
        for (const std::shared_ptr<Simulation>& simulation : document.getSimulations())
        {
            const std::shared_ptr<FlightData>& data = simulation->getSimulatedData();
            if (data == nullptr)
            {
                continue;
            }
            for (std::size_t i = 0; i < data->getBranchCount(); ++i)
            {
                // Java's countFlightDataBranchPoints(): the rows of the branch, 0 without types.
                pointCount += static_cast<std::int64_t>(data->getBranch(i).getLength());
            }
        }
    }
    size += pointCount * kBytesPerDatapointCompressed;

    return size;
}

int OpenRocketSaver::calculateNecessaryFileVersion(const OpenRocketDocument& /*document*/,
                                                   const StorageOptions& /*options*/)
{
    // NOTE: Remember to update the supported versions in DocumentConfig as well! (Java; the
    // static_assert above does.)
    return kFileVersion;
}

std::string OpenRocketSaver::fileVersionText(int fileVersion)
{
    return std::format("{}.{}", fileVersion / kFileVersionDivisor,
                       fileVersion % kFileVersionDivisor);
}

}  // namespace QtRocket
