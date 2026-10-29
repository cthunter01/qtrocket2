// A motor mount, and a stage with its recovery device and mount, pasted into a design whose flight
// configurations have other ids than the design they were copied from (the M3 tests-exit scout's
// open point C2, plan section 10.5): the pasted components keep their parameter sets with the
// source's configuration ids, which the target's rocket does not have. What OpenRocket does with
// them is pinned by hand from the probe tools/openrocket-probes/pasted-mount (PastedMountProbe,
// OpenRocket 5f164fd0e; its expected.txt, whose lines the expectations below repeat):
// - the copies keep every id of their sets (S1 and S2 of the source), which no row of the
//   target's tables shows: the target's configurations hold no motor of the pasted mount, the
//   pasted mount's cells say "None", the pasted chute and stage show their defaults;
// - the save writes the motors of a mount only for the rocket's configurations
//   (RocketComponentSaver.motorMountParams(): none of the pasted mount's), but the deployment and
//   separation overrides for every id of their sets (RecoveryDeviceSaver, AxialStageSaver), the
//   source's included; the pasted component's element is the source's element in the source's
//   save but for those motors and their ignitions (and its new component ids);
// - the reload of that save has the target's configurations only (a <deploymentconfiguration> or
//   <separationconfiguration> makes no configuration, only a <motor> does) and keeps the pasted
//   stage's and chute's overrides of the source's ids, still unseen; its save is the save;
// - the mount pasted from the target back into the source shows its motors there again.
// QtRocket matches it, through its own Copy and Paste between two windows (RocketActions,
// ComponentEditing) and its tab's tables (FlightConfigurableTableModel), but where it fixes
// OpenRocket's bug 6 (plan 10.4, U-M3-5): a copied mount keeps its default ignition, here the one
// its load marked overridden (MotorMountHandler.java:101-115, as QtRocket's loader), so a cell of a
// target's configuration, which shows the pasted mount's default, reads "Automatic" where
// OpenRocket's new default reads "Default (Automatic)" (shaded): kBug6Cell.
//
// The designs are loaded as the probe loads them: the examples "Two stage high power rocket" (the
// source, configurations S1 and S2) and "A simple model rocket" (the target, T1 to T5) from
// data/examples, with the bundled motors and no component preset. The tables take the default
// preferences and units, as in the tables' pins (tests/gui/flightconfig/
// flight_configuration_tables_golden_tests.cpp). The saves are .ork archives without the
// simulation data, as Save as writes them. The suite compares with values pinned by hand, not
// with golden data: no "Golden" in its name.

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QModelIndex>
#include <QString>
#include <QVariant>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/StorageOptions.h"
#include "QtRocket/file/GeneralRocketSaver.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/ZipArchive.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/motor/IgnitionEvent.h"
#include "QtRocket/motor/Motor.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/DeploymentConfiguration.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MotorConfiguration.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/RecoveryDevice.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StageSeparationConfiguration.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Strings.h"
#include "TestGuiServices.h"
#include "file/DesignFileEnvironment.h"
#include "goldens/GoldenExamples.h"

namespace QtRocket::Gui
{

namespace
{

using Presets = QtRocket::Test::DesignFileEnvironment::Presets;

/// The probe's source and target designs, files of data/examples.
constexpr std::string_view kSource = "Two stage high power rocket.ork";
constexpr std::string_view kTarget = "A simple model rocket.ork";

/// QtRocket's text of a motor cell of a target's configuration in the column of a pasted mount
/// (bug 6, see the top of this file); OpenRocket's is "None / ~Default (Automatic)".
constexpr std::string_view kBug6Cell = "None / Automatic";

/// The names of the configuration ids, by their text: the source's S1, S2, the target's T1 to T5
/// (by their rows), as the probe names them.
using ConfigurationNames = std::map<std::string, std::string, std::less<>>;

/// Names the configurations of @p rocket @p prefix + row + 1 in @p names.
void nameConfigurations(const Rocket& rocket, std::string_view prefix, ConfigurationNames& names)
{
    const std::vector<FlightConfigurationId> ids = rocket.getIds();
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        names.emplace(ids[i].toString(), std::format("{}{}", prefix, i + 1));
    }
}

/// The name of @p id: "default" for the default id, "?<id>" for one of neither design.
[[nodiscard]] std::string nameOf(const ConfigurationNames& names, const FlightConfigurationId& id)
{
    if (id.isDefaultId())
    {
        return "default";
    }
    const auto found = names.find(id.toString());
    return found == names.end() ? "?" + id.toString() : found->second;
}

/// "ids [S1, S2]".
[[nodiscard]] std::string idsText(const ConfigurationNames&              names,
                                  std::span<const FlightConfigurationId> ids)
{
    std::string text;
    for (const FlightConfigurationId& id : ids)
    {
        text += (text.empty() ? "" : ", ") + nameOf(names, id);
    }
    return "ids [" + text + "]";
}

/// The path of @p component from its stage, "Booster/Booster fin section/Booster motor mount".
[[nodiscard]] std::string pathOf(const RocketComponent& component)
{
    std::string            text   = component.getName();
    const RocketComponent* parent = component.getParent();
    while (parent != nullptr && dynamic_cast<const Rocket*>(parent) == nullptr)
    {
        text.insert(0, "/");
        text.insert(0, parent->getName());
        parent = parent->getParent();
    }
    return text;
}

/// The components of the tree of @p root in pre-order, @p root first (Java's iterator(true)).
void collect(const RocketComponent& root, std::vector<const RocketComponent*>& components)
{
    components.push_back(&root);
    for (const RocketComponent* child : root.getChildren())
    {
        collect(*child, components);
    }
}

/// The first component of @p rocket whose path or name is @p name (the probe's find()).
[[nodiscard]] const RocketComponent& find(const Rocket& rocket, std::string_view name)
{
    std::vector<const RocketComponent*> components;
    collect(rocket, components);
    for (const RocketComponent* component : components)
    {
        if (pathOf(*component) == name || component->getName() == name)
        {
            return *component;
        }
    }
    QtRocket::bug(std::format("no component {}", name));
}

/// The probe's motor(): "H148R-0 AUTOMATIC+0.0 (overridden)".
[[nodiscard]] std::string motorText(const MotorConfiguration& config)
{
    const std::string motor = config.getMotor() == nullptr
                                  ? std::string("no motor")
                                  : config.getMotor()->getDesignation(config.getEjectionDelay());
    return std::format("{} {}+{}{}", motor, QtRocket::name(config.getIgnitionEvent()),
                       Strings::javaDoubleToString(config.getIgnitionDelay()),
                       config.hasIgnitionOverride() ? " (overridden)" : "");
}

/// The probe's ignition(): "AUTOMATIC+0.0".
[[nodiscard]] std::string ignitionText(const MotorConfiguration& config)
{
    return std::format("{}+{}", QtRocket::name(config.getIgnitionEvent()),
                       Strings::javaDoubleToString(config.getIgnitionDelay()));
}

/// The probe's deployment(): "APOGEE/200.0/0.0".
[[nodiscard]] std::string deploymentText(const DeploymentConfiguration& config)
{
    return std::format("{}/{}/{}", deployEventName(config.getDeployEvent()),
                       Strings::javaDoubleToString(config.getDeployAltitude()),
                       Strings::javaDoubleToString(config.getDeployDelay()));
}

/// The probe's separation(): "BURNOUT/200.0/0.0".
[[nodiscard]] std::string separationText(const StageSeparationConfiguration& config)
{
    return std::format("{}/{}/{}", separationEventName(config.getSeparationEvent()),
                       Strings::javaDoubleToString(config.getSeparationAltitude()),
                       Strings::javaDoubleToString(config.getSeparationDelay()));
}

/// The probe's line of @p component when it is an acting motor mount, or "".
[[nodiscard]] std::string motorSetLine(const ConfigurationNames& names,
                                       const RocketComponent&    component)
{
    const auto* mount = dynamic_cast<const MotorMount*>(&component);
    if (mount == nullptr || !mount->isMotorMount())
    {
        return {};
    }
    std::string entries;
    for (const FlightConfigurationId& id : mount->getMotorConfigurationSet().getIds())
    {
        entries += (entries.empty() ? "" : ", ") + nameOf(names, id) + " " +
                   motorText(mount->getMotorConfig(id));
    }
    return std::format("motors {}: default {}, [{}]", pathOf(component),
                       ignitionText(mount->getDefaultMotorConfig()), entries);
}

/// The probe's line of the recovery device @p component, or "".
[[nodiscard]] std::string deploymentSetLine(const ConfigurationNames& names,
                                            const RocketComponent&    component)
{
    const auto* device = dynamic_cast<const RecoveryDevice*>(&component);
    if (device == nullptr)
    {
        return {};
    }
    const auto& set = device->getDeploymentConfigurations();
    std::string entries;
    for (const FlightConfigurationId& id : set.getIds())
    {
        entries +=
            (entries.empty() ? "" : ", ") + nameOf(names, id) + " " + deploymentText(set.get(id));
    }
    return std::format("deployments {}: default {}, [{}]", pathOf(component),
                       deploymentText(set.getDefault()), entries);
}

/// The probe's line of the stage @p component, or "".
[[nodiscard]] std::string separationSetLine(const ConfigurationNames& names,
                                            const RocketComponent&    component)
{
    const auto* stage = dynamic_cast<const AxialStage*>(&component);
    if (stage == nullptr)
    {
        return {};
    }
    const auto& set = stage->getSeparationConfigurations();
    std::string entries;
    for (const FlightConfigurationId& id : set.getIds())
    {
        entries +=
            (entries.empty() ? "" : ", ") + nameOf(names, id) + " " + separationText(set.get(id));
    }
    return std::format("separations {} (stage {}): default {}, [{}]", pathOf(component),
                       stage->getStageNumber(), separationText(set.getDefault()), entries);
}

/// The probe's sets(): the parameter sets of the motor mounts, recovery devices and stages of the
/// tree of @p root, a line each.
[[nodiscard]] std::string setLines(const ConfigurationNames& names, const RocketComponent& root)
{
    std::vector<const RocketComponent*> components;
    collect(root, components);
    std::string text;
    for (const RocketComponent* component : components)
    {
        for (const std::string& line :
             {motorSetLine(names, *component), deploymentSetLine(names, *component),
              separationSetLine(names, *component)})
        {
            if (!line.empty())
            {
                text += line + "\n";
            }
        }
    }
    return text;
}

/// The probe's configurations(): what each configuration of @p rocket holds, its motors by mount
/// and its active stages.
[[nodiscard]] std::string configurationLines(const ConfigurationNames& names, const Rocket& rocket,
                                             const Preferences& preferences)
{
    std::string text;
    for (const FlightConfigurationId& id : rocket.getIds())
    {
        const FlightConfiguration& config = rocket.getFlightConfiguration(id);
        std::string                motors;
        for (const MotorConfiguration& motor : config.getAllMotors())
        {
            motors += (motors.empty() ? "" : ", ") + pathOf(asComponent(motor.getMount())) + " " +
                      motor.getMotor()->getDesignation(motor.getEjectionDelay());
        }
        std::string stages;
        for (const AxialStage* stage : config.getActiveStages())
        {
            stages += (stages.empty() ? "" : ", ") + std::to_string(stage->getStageNumber());
        }
        text += std::format("configuration {} '{}': motors [{}], active stages [{}]\n",
                            nameOf(names, id), config.getName(preferences), motors, stages);
    }
    return text;
}

/// The probe's print() of @p panel's table @p name: its header, then a row per configuration,
/// "row: cell | cell" (a motor cell "motor / ignition", the ignition after "~" when it is the
/// default's).
[[nodiscard]] std::string tableLines(std::string_view name, const FlightConfigurablePanel& panel,
                                     bool motorCells)
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    std::string                         header;
    for (int column = 0; column < model.columnCount(); ++column)
    {
        header += (column > 0 ? " | " : "") +
                  QtStrings::toUtf8(model.headerData(column, Qt::Horizontal).toString());
    }
    std::string text = std::format("{}: {}\n", name, header);
    for (int row = 0; row < model.rowCount(); ++row)
    {
        std::string cells = QtStrings::toUtf8(model.index(row, 0).data().toString());
        for (int column = 1; column < model.columnCount(); ++column)
        {
            const QModelIndex index = model.index(row, column);
            cells += " | " + QtStrings::toUtf8(index.data().toString());
            if (motorCells)
            {
                cells +=
                    std::string(" / ") +
                    (index.data(FlightConfigurableTableModel::kDefaultRole).toBool() ? "~" : "") +
                    QtStrings::toUtf8(
                        index.data(FlightConfigurableTableModel::kIgnitionTextRole).toString());
            }
        }
        text += std::format("  {}: {}\n", row, cells);
    }
    return text;
}

/// The probe's tables(): the motors, recovery and stages tables of @p window's tab.
[[nodiscard]] std::string tablesOf(const MainWindow& window)
{
    const FlightConfigurationPanel* const tab = window.componentFlightConfigurationPanel();
    QTROCKET_ASSERT(tab != nullptr);
    return tableLines("motors", tab->motorConfigurationPanel(), true) +
           tableLines("recovery", tab->recoveryConfigurationPanel(), false) +
           tableLines("stages", tab->separationConfigurationPanel(), false);
}

/// @p text with every configuration id of @p names replaced by its name.
[[nodiscard]] std::string withNamedIds(const ConfigurationNames& names, std::string text)
{
    for (const auto& [id, name] : names)
    {
        for (std::size_t at = text.find(id); at != std::string::npos; at = text.find(id, at))
        {
            text.replace(at, id.size(), name);
            at += name.size();
        }
    }
    return text;
}

/// The .ork archive of @p document (GeneralRocketSaver, as Save as writes it, without the
/// simulation data), or no bytes with a test failure.
[[nodiscard]] std::vector<std::byte> saveArchive(OpenRocketDocument& document,
                                                 const Preferences&  preferences)
{
    StorageOptions options;
    options.setSaveSimulationData(false);
    GeneralRocketSaver                   saver(preferences);
    const Result<std::vector<std::byte>> bytes = saver.saveToBytes(document, options);
    if (!bytes || !saver.getWarnings().empty() || !saver.getErrors().empty())
    {
        ADD_FAILURE() << "the save " << (bytes ? "warned" : "failed: " + bytes.error().toString());
        return {};
    }
    return *bytes;
}

/// What the probe prints of an archive: the names of its entries, "[rocket.ork, decals/...]", and
/// the text of its design document.
struct ArchiveText
{
    std::string entries;
    std::string document;
};

/// The ArchiveText of @p bytes, an .ork archive; empty with a test failure when it is none.
[[nodiscard]] ArchiveText archiveText(std::span<const std::byte> bytes)
{
    const Result<ZipArchive> archive = ZipArchive::fromBytes(bytes);
    if (!archive)
    {
        ADD_FAILURE() << archive.error().toString();
        return {};
    }
    ArchiveText text;
    for (const ZipArchive::Entry& entry : archive->entries())
    {
        text.entries += (text.entries.empty() ? "" : ", ") + entry.name;
    }
    text.entries = "[" + text.entries + "]";
    if (const std::vector<std::byte>* const document = archive->find("rocket.ork");
        document != nullptr)
    {
        for (const std::byte character : *document)
        {
            text.document.push_back(static_cast<char>(character));
        }
    }
    return text;
}

/// The archive of the target designs' saves: the document, the target's decal images and the
/// thrust curves of the target's configurations (A8-3, B4-4, the C6s), none of the pasted mount's
/// (GeneralRocketSaver.java:292-311 takes the motors of the rocket's configurations only).
constexpr std::string_view kTargetArchive =
    "[rocket.ork, decals/BodyStripe.png, decals/TailStripe.png, "
    "thrustcurves/c049499ca03fd69115352e5d4be72de7.rse, "
    "thrustcurves/be11a726b56813c4b1aea0574c8302b2.rse, "
    "thrustcurves/2967cd7a160b396ef96f09695429d8e9.rse]";

/// The probe's lines(): the lines of @p text, a design document, that name a configuration of
/// @p names or a stage of a configuration, stripped, the ids by name, each after the text of the
/// last <name> element before it.
[[nodiscard]] std::string saveLines(const ConfigurationNames& names, std::string_view text)
{
    std::string lines;
    std::string context;
    for (std::size_t start = 0; start <= text.size();)
    {
        std::size_t end = text.find('\n', start);
        end             = end == std::string_view::npos ? text.size() : end;
        // The probe's String.strip(): the lines hold spaces at most.
        const std::string_view line = Strings::trim(text.substr(start, end - start));
        start                       = end + 1;
        if (line.starts_with("<name>") && line.ends_with("</name>"))
        {
            context = std::string(line.substr(6, line.size() - 13));
            continue;
        }
        const std::string named = withNamedIds(names, std::string(line));
        if (named != line || line.starts_with("<stage number="))
        {
            lines += std::format("{}: {}\n", context, named);
        }
    }
    return lines;
}

/// The probe's element(): the lines of the element of the component named @p name in @p text, a
/// design document, from its opening tag (the line before its <name>) to its closing tag (the next
/// line of the opening tag's indentation), stripped, the configuration ids by name, every
/// component id (new in a pasted copy) as "<id>(component)</id>"; a test failure and none when
/// the document has no such element or two.
[[nodiscard]] std::vector<std::string> elementLines(const ConfigurationNames& names,
                                                    std::string_view text, std::string_view name)
{
    std::vector<std::string_view> raw;
    for (std::size_t start = 0; start <= text.size();)
    {
        std::size_t end = text.find('\n', start);
        end             = end == std::string_view::npos ? text.size() : end;
        raw.push_back(text.substr(start, end - start));
        start = end + 1;
    }
    const std::string          nameLine = std::format("<name>{}</name>", name);
    std::optional<std::size_t> opening;
    for (std::size_t i = 1; i < raw.size(); ++i)
    {
        if (Strings::trim(raw[i]) == nameLine && !Strings::trim(raw[i - 1]).starts_with("</"))
        {
            if (opening.has_value())
            {
                ADD_FAILURE() << "two elements named " << name;
                return {};
            }
            opening = i - 1;
        }
    }
    if (!opening.has_value())
    {
        ADD_FAILURE() << "no element named " << name;
        return {};
    }
    const std::string_view   first       = raw[*opening];
    const std::string        indentation = std::string(first.substr(0, first.find('<'))) + "</";
    std::vector<std::string> lines;
    for (std::size_t i = *opening; i < raw.size(); ++i)
    {
        const std::string_view line = Strings::trim(raw[i]);
        lines.push_back(line.starts_with("<id>") ? std::string("<id>(component)</id>")
                                                 : withNamedIds(names, std::string(line)));
        if (i > *opening && raw[i].starts_with(indentation))
        {
            return lines;
        }
    }
    ADD_FAILURE() << "the element named " << name << " does not end";
    return {};
}

/// The probe's difference(): the lines in which @p before and @p after differ, "- " a line of
/// @p before only, "+ " one of @p after only, in their order, each ended: the complement of the
/// longest common subsequence (a table of the common lengths of the suffixes; at a choice the line
/// of @p before goes first), as the probe computes it.
[[nodiscard]] std::string differenceLines(const std::vector<std::string>& before,
                                          const std::vector<std::string>& after)
{
    const std::size_t                     rows    = before.size();
    const std::size_t                     columns = after.size();
    std::vector<std::vector<std::size_t>> common(rows + 1, std::vector<std::size_t>(columns + 1));
    for (std::size_t i = rows; i-- > 0;)
    {
        for (std::size_t j = columns; j-- > 0;)
        {
            common[i][j] = before[i] == after[j] ? common[i + 1][j + 1] + 1
                                                 : std::max(common[i + 1][j], common[i][j + 1]);
        }
    }
    std::string lines;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < rows || j < columns)
    {
        if (i < rows && j < columns && before[i] == after[j])
        {
            ++i;
            ++j;
        }
        else if (j == columns || (i < rows && common[i + 1][j] >= common[i][j + 1]))
        {
            lines += "- " + before[i++] + "\n";
        }
        else
        {
            lines += "+ " + after[j++] + "\n";
        }
    }
    return lines;
}

/// What the probe prints of the pasted mount's element, and of the pasted stage's (whose only
/// motor mount it is): the source's save has the motors and ignitions of the source's
/// configurations, the target's has none of them (RocketComponentSaver writes a mount's motors
/// for the rocket's configurations only); every other line of the element is the source's.
constexpr std::string_view kPastedElementDifference =
    "- <motor configid=\"S1\">\n"
    "- <type>reload</type>\n"
    "- <manufacturer>AeroTech</manufacturer>\n"
    "- <digest>a06234b6049c1079e394cb9ecd4607a9</digest>\n"
    "- <designation>H148R</designation>\n"
    "- <diameter>0.038</diameter>\n"
    "- <length>0.152</length>\n"
    "- <delay>0.0</delay>\n"
    "- </motor>\n"
    "- <ignitionconfiguration configid=\"S1\">\n"
    "- <ignitionevent>automatic</ignitionevent>\n"
    "- <ignitiondelay>0.0</ignitiondelay>\n"
    "- </ignitionconfiguration>\n"
    "- <motor configid=\"S2\">\n"
    "- <type>reload</type>\n"
    "- <manufacturer>AeroTech</manufacturer>\n"
    "- <digest>1a3327383625336706131b2a9d198139</digest>\n"
    "- <designation>I357T</designation>\n"
    "- <diameter>0.038</diameter>\n"
    "- <length>0.203</length>\n"
    "- <delay>14.0</delay>\n"
    "- </motor>\n"
    "- <ignitionconfiguration configid=\"S2\">\n"
    "- <ignitionevent>automatic</ignitionevent>\n"
    "- <ignitiondelay>0.0</ignitiondelay>\n"
    "- </ignitionconfiguration>\n";

/// The windows of a window manager over test services and a component clipboard of the test's
/// own, on the probe's designs.
class PastedMotorMountTest : public Gui::Test::GuiTest
{
protected:
    void TearDown() override
    {
        // The windows, and the documents and panels they leave to deleteLater(), go first.
        m_manager = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        GuiTest::TearDown();
    }

    /// A window on a new load of the example @p file (data/examples).
    MainWindow& open(std::string_view file)
    {
        const QtRocket::Test::GoldenExample* example = QtRocket::Test::findGoldenExample(file);
        QTROCKET_ASSERT(example != nullptr);
        Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(
            *example, QtRocket::Test::ExampleSource::ORIGINAL, Presets::NONE);
        QTROCKET_ASSERT(loaded.has_value() && loaded->warnings.empty());
        return manager().addDocument(std::move(loaded->document));
    }

    /// A window on the reload of @p bytes, a save.
    MainWindow& reopen(std::vector<std::byte> bytes)
    {
        Result<LoadedDocument> loaded =
            QtRocket::Test::loadGoldenExampleFromBytes(std::move(bytes), Presets::NONE);
        QTROCKET_ASSERT(loaded.has_value());
        m_reloadWarnings = loaded->warnings.size();
        return manager().addDocument(std::move(loaded->document));
    }

    /// The number of warnings of the last reopen().
    [[nodiscard]] std::size_t reloadWarnings() const noexcept { return m_reloadWarnings; }

    /// RocketActions' Copy of the component @p name of @p from, and its Paste into @p to with the
    /// component @p selected selected: the pasted component (the paste selects it).
    static const RocketComponent& paste(const MainWindow& from, std::string_view name,
                                        const MainWindow& to, std::string_view selected)
    {
        select(from, name);
        QAction* const copy = from.menus().copyAction();
        QTROCKET_ASSERT(copy->isEnabled());
        copy->trigger();
        select(to, selected);
        QAction* const paste = to.menus().pasteAction();
        QTROCKET_ASSERT(paste->isEnabled());
        paste->trigger();
        const RocketComponent* const pasted = designOf(to).selection().selectedComponent();
        QTROCKET_ASSERT(pasted != nullptr);
        return *pasted;
    }

    /// The preferences of the probe's saves: those of the loads.
    [[nodiscard]] static const Preferences& savePreferences()
    {
        return QtRocket::Test::goldenExampleEnvironment(Presets::NONE).preferences();
    }

private:
    [[nodiscard]] WindowManager& manager()
    {
        if (m_manager == nullptr)
        {
            m_manager = std::make_unique<WindowManager>(m_services.services(), m_clipboard,
                                                        WindowManager::LastWindowBehaviour::EXIT);
        }
        return *m_manager;
    }

    [[nodiscard]] static DesignPanel& designOf(const MainWindow& window)
    {
        DesignPanel* const panel = window.componentDesignPanel();
        QTROCKET_ASSERT(panel != nullptr);
        return *panel;
    }

    static void select(const MainWindow& window, std::string_view name)
    {
        designOf(window).selection().setSelectedComponent(
            find(window.document().getRocket(), name));
    }

    Gui::Test::TestGuiServices     m_services;
    OpenRocketClipboard            m_clipboard;
    std::unique_ptr<WindowManager> m_manager;
    std::size_t                    m_reloadWarnings{0};
};

/// The probe's lines of the target's five configurations in its tables, a motor mount's cell
/// @p pasted for each.
[[nodiscard]] std::string targetMotorRows(std::string_view names, std::string_view pasted)
{
    constexpr std::array<std::string_view, 5> kMotors{"A8-3", "B4-4", "C6-3", "C6-5", "C6-7"};
    std::string                               text;
    std::size_t                               row = 0;
    for (const std::string_view motor : kMotors)
    {
        text +=
            std::format("  {}: [{}{}] | {} / Automatic{}\n", row++, motor, names, motor, pasted);
    }
    return text;
}

// ===================================================================================== the mount

TEST_F(PastedMotorMountTest, AMountKeepsTheSourcesIdsWhichTheTargetsTablesAndSaveLeaveOut)
{
    const MainWindow&  source = open(kSource);
    const MainWindow&  target = open(kTarget);
    ConfigurationNames names;
    nameConfigurations(source.document().getRocket(), "S", names);
    nameConfigurations(target.document().getRocket(), "T", names);
    EXPECT_EQ(idsText(names, source.document().getRocket().getIds()), "ids [S1, S2]");
    EXPECT_EQ(idsText(names, target.document().getRocket().getIds()), "ids [T1, T2, T3, T4, T5]");

    const RocketComponent& mount = paste(source, "Booster motor mount", target, "Body tube");
    ASSERT_NE(mount.getParent(), nullptr);
    EXPECT_EQ(std::format("{} into {} at {}", pathOf(mount), pathOf(*mount.getParent()),
                          mount.getParent()->getChildPosition(&mount).value_or(99)),
              "Sustainer/Body tube/Booster motor mount into Sustainer/Body tube at 8");
    // The source's ids, with their motors.
    EXPECT_EQ(setLines(names, mount),
              "motors Sustainer/Body tube/Booster motor mount: default AUTOMATIC+0.0, [S1 H148R-0 "
              "AUTOMATIC+0.0 (overridden), S2 I357T-14 AUTOMATIC+0.0 (overridden)]\n");
    // No configuration of the target has a motor of it.
    const Rocket& rocket = target.document().getRocket();
    EXPECT_EQ(configurationLines(names, rocket, savePreferences()),
              "configuration T1 '[A8-3]': motors [Sustainer/Body tube/Inner Tube A8-3], active "
              "stages [0]\n"
              "configuration T2 '[B4-4]': motors [Sustainer/Body tube/Inner Tube B4-4], active "
              "stages [0]\n"
              "configuration T3 '[C6-3]': motors [Sustainer/Body tube/Inner Tube C6-3], active "
              "stages [0]\n"
              "configuration T4 '[C6-5]': motors [Sustainer/Body tube/Inner Tube C6-5], active "
              "stages [0]\n"
              "configuration T5 '[C6-7]': motors [Sustainer/Body tube/Inner Tube C6-7], active "
              "stages [0]\n");
    // The tables show the target's configurations, the pasted mount's cells without a motor
    // (OpenRocket: "None / ~Default (Automatic)", bug 6).
    const std::string recoveryAndStages =
        "recovery: Configuration | Parachute\n"
        "  0: [A8-3] | Ejection charge\n"
        "  1: [B4-4] | Ejection charge\n"
        "  2: [C6-3] | Ejection charge\n"
        "  3: [C6-5] | Ejection charge\n"
        "  4: [C6-7] | Ejection charge\n"
        "stages: Configuration\n"
        "  0: [A8-3]\n"
        "  1: [B4-4]\n"
        "  2: [C6-3]\n"
        "  3: [C6-5]\n"
        "  4: [C6-7]\n";
    EXPECT_EQ(tablesOf(target), "motors: Configuration | Inner Tube | Booster motor mount\n" +
                                    targetMotorRows("", std::format(" | {}", kBug6Cell)) +
                                    recoveryAndStages);

    // The save names the target's configurations only: no motor of the pasted mount, and none of
    // its thrust curves.
    const std::vector<std::byte> save    = saveArchive(target.document(), savePreferences());
    const ArchiveText            archive = archiveText(save);
    EXPECT_EQ(archive.entries, kTargetArchive);
    const std::string expectedSave =
        "A simple model rocket: <motorconfiguration configid=\"T1\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T2\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T3\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T4\" default=\"true\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T5\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "Inner Tube: <motor configid=\"T1\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T1\">\n"
        "Inner Tube: <motor configid=\"T2\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T2\">\n"
        "Inner Tube: <motor configid=\"T3\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T3\">\n"
        "Inner Tube: <motor configid=\"T4\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T4\">\n"
        "Inner Tube: <motor configid=\"T5\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T5\">\n"
        "Simulation 1: <configid>T1</configid>\n"
        "Simulation 2: <configid>T2</configid>\n"
        "Simulation 3 - too short delay: <configid>T3</configid>\n"
        "Simulation 4: <configid>T4</configid>\n"
        "Simulation 5: <configid>T5</configid>\n";
    EXPECT_EQ(saveLines(names, archive.document), expectedSave);
    // The pasted mount's element is the source's but for the source's motors.
    const ArchiveText sourceSave = archiveText(saveArchive(source.document(), savePreferences()));
    EXPECT_EQ(differenceLines(elementLines(names, sourceSave.document, "Booster motor mount"),
                              elementLines(names, archive.document, "Booster motor mount")),
              kPastedElementDifference);

    // Its reload: the mount without a motor configuration, every ignition marked overridden by
    // the loader (the default's too), so that neither program shades a cell; the same save.
    const MainWindow& reloaded = reopen(save);
    EXPECT_EQ(reloadWarnings(), 0U);
    const Rocket& again = reloaded.document().getRocket();
    EXPECT_EQ(idsText(names, again.getIds()), "ids [T1, T2, T3, T4, T5]");
    EXPECT_EQ(setLines(names, find(again, "Sustainer/Body tube/Booster motor mount")),
              "motors Sustainer/Body tube/Booster motor mount: default AUTOMATIC+0.0, []\n");
    EXPECT_EQ(tablesOf(reloaded), "motors: Configuration | Inner Tube | Booster motor mount\n" +
                                      targetMotorRows("", " | None / Automatic") +
                                      recoveryAndStages);
    const ArchiveText resave = archiveText(saveArchive(reloaded.document(), savePreferences()));
    EXPECT_EQ(resave.entries, kTargetArchive);
    EXPECT_EQ(saveLines(names, resave.document), expectedSave);
}

// ===================================================================================== the stage

TEST_F(PastedMotorMountTest, AStageAndItsChuteSaveTheirOverridesOfTheSourcesIds)
{
    const MainWindow&  source = open(kSource);
    const MainWindow&  target = open(kTarget);
    ConfigurationNames names;
    nameConfigurations(source.document().getRocket(), "S", names);
    nameConfigurations(target.document().getRocket(), "T", names);

    const RocketComponent& stage = paste(source, "Booster", target, "Sustainer");
    ASSERT_NE(stage.getParent(), nullptr);
    EXPECT_EQ(std::format("{} into {} at {}", pathOf(stage), stage.getParent()->getName(),
                          stage.getParent()->getChildPosition(&stage).value_or(99)),
              "Booster into A simple model rocket at 1");
    const std::string sets =
        "separations Booster (stage 1): default EJECTION/200.0/0.0, [S1 BURNOUT/200.0/0.0, S2 "
        "UPPER_IGNITION/200.0/0.0]\n"
        "deployments Booster/Booster airframe/Booster Chute: default EJECTION/200.0/0.0, [S1 "
        "APOGEE/200.0/0.0, S2 APOGEE/200.0/0.0]\n";
    EXPECT_EQ(setLines(names, stage),
              sets +
                  "motors Booster/Booster fin section/Booster motor mount: default "
                  "AUTOMATIC+0.0, [S1 H148R-0 AUTOMATIC+0.0 (overridden), S2 I357T-14 "
                  "AUTOMATIC+0.0 (overridden)]\n");
    const Rocket& rocket = target.document().getRocket();
    EXPECT_EQ(configurationLines(names, rocket, savePreferences()),
              "configuration T1 '[A8-3; None]': motors [Sustainer/Body tube/Inner Tube A8-3], "
              "active stages [0, 1]\n"
              "configuration T2 '[B4-4; None]': motors [Sustainer/Body tube/Inner Tube B4-4], "
              "active stages [0, 1]\n"
              "configuration T3 '[C6-3; None]': motors [Sustainer/Body tube/Inner Tube C6-3], "
              "active stages [0, 1]\n"
              "configuration T4 '[C6-5; None]': motors [Sustainer/Body tube/Inner Tube C6-5], "
              "active stages [0, 1]\n"
              "configuration T5 '[C6-7; None]': motors [Sustainer/Body tube/Inner Tube C6-7], "
              "active stages [0, 1]\n");
    // The chute and the stage show their defaults; the mount's cells as in the first test.
    const std::string recoveryAndStages =
        "recovery: Configuration | Parachute | Booster Chute\n"
        "  0: [A8-3; None] | Ejection charge | Ejection charge\n"
        "  1: [B4-4; None] | Ejection charge | Ejection charge\n"
        "  2: [C6-3; None] | Ejection charge | Ejection charge\n"
        "  3: [C6-5; None] | Ejection charge | Ejection charge\n"
        "  4: [C6-7; None] | Ejection charge | Ejection charge\n"
        "stages: Configuration | Booster\n"
        "  0: [A8-3; None] | Current stage ejection charge\n"
        "  1: [B4-4; None] | Current stage ejection charge\n"
        "  2: [C6-3; None] | Current stage ejection charge\n"
        "  3: [C6-5; None] | Current stage ejection charge\n"
        "  4: [C6-7; None] | Current stage ejection charge\n";
    EXPECT_EQ(tablesOf(target), "motors: Configuration | Inner Tube | Booster motor mount\n" +
                                    targetMotorRows("; None", std::format(" | {}", kBug6Cell)) +
                                    recoveryAndStages);

    // The save: the stage's separations and the chute's deployments of the source's ids; no
    // motor of the pasted mount.
    const std::vector<std::byte> save    = saveArchive(target.document(), savePreferences());
    const ArchiveText            archive = archiveText(save);
    EXPECT_EQ(archive.entries, kTargetArchive);
    const std::string expectedSave =
        "A simple model rocket: <motorconfiguration configid=\"T1\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <stage number=\"1\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T2\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <stage number=\"1\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T3\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <stage number=\"1\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T4\" default=\"true\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <stage number=\"1\" active=\"true\"/>\n"
        "A simple model rocket: <motorconfiguration configid=\"T5\">\n"
        "A simple model rocket: <stage number=\"0\" active=\"true\"/>\n"
        "A simple model rocket: <stage number=\"1\" active=\"true\"/>\n"
        "Inner Tube: <motor configid=\"T1\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T1\">\n"
        "Inner Tube: <motor configid=\"T2\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T2\">\n"
        "Inner Tube: <motor configid=\"T3\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T3\">\n"
        "Inner Tube: <motor configid=\"T4\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T4\">\n"
        "Inner Tube: <motor configid=\"T5\">\n"
        "Inner Tube: <ignitionconfiguration configid=\"T5\">\n"
        "Booster: <separationconfiguration configid=\"S1\">\n"
        "Booster: <separationconfiguration configid=\"S2\">\n"
        "Booster Chute: <deploymentconfiguration configid=\"S1\">\n"
        "Booster Chute: <deploymentconfiguration configid=\"S2\">\n"
        "Simulation 1: <configid>T1</configid>\n"
        "Simulation 2: <configid>T2</configid>\n"
        "Simulation 3 - too short delay: <configid>T3</configid>\n"
        "Simulation 4: <configid>T4</configid>\n"
        "Simulation 5: <configid>T5</configid>\n";
    EXPECT_EQ(saveLines(names, archive.document), expectedSave);
    // The pasted stage's element is the source's but for the motors of its mount: its separations
    // and its chute's deployments of the source's ids are the source's.
    const ArchiveText sourceSave = archiveText(saveArchive(source.document(), savePreferences()));
    EXPECT_EQ(differenceLines(elementLines(names, sourceSave.document, "Booster"),
                              elementLines(names, archive.document, "Booster")),
              kPastedElementDifference);

    // The reload keeps them, still unseen, and makes no configuration of their ids.
    const MainWindow& reloaded = reopen(save);
    EXPECT_EQ(reloadWarnings(), 0U);
    const Rocket& again = reloaded.document().getRocket();
    EXPECT_EQ(idsText(names, again.getIds()), "ids [T1, T2, T3, T4, T5]");
    EXPECT_EQ(setLines(names, find(again, "Booster")),
              sets +
                  "motors Booster/Booster fin section/Booster motor mount: default "
                  "AUTOMATIC+0.0, []\n");
    EXPECT_EQ(tablesOf(reloaded), "motors: Configuration | Inner Tube | Booster motor mount\n" +
                                      targetMotorRows("; None", " | None / Automatic") +
                                      recoveryAndStages);
    const ArchiveText resave = archiveText(saveArchive(reloaded.document(), savePreferences()));
    EXPECT_EQ(resave.entries, kTargetArchive);
    EXPECT_EQ(saveLines(names, resave.document), expectedSave);
}

// ====================================================================================== the back

TEST_F(PastedMotorMountTest, TheMountPastedBackIntoTheSourceShowsItsMotorsAgain)
{
    const MainWindow&  source = open(kSource);
    const MainWindow&  target = open(kTarget);
    ConfigurationNames names;
    nameConfigurations(source.document().getRocket(), "S", names);
    nameConfigurations(target.document().getRocket(), "T", names);
    const RocketComponent& mount = paste(source, "Booster motor mount", target, "Body tube");

    const RocketComponent& back = paste(target, pathOf(mount), source, "Booster fin section");
    ASSERT_NE(back.getParent(), nullptr);
    EXPECT_EQ(std::format("{} at {}", pathOf(back),
                          back.getParent()->getChildPosition(&back).value_or(99)),
              "Booster/Booster fin section/Booster motor mount at 5");
    EXPECT_EQ(setLines(names, back),
              "motors Booster/Booster fin section/Booster motor mount: default AUTOMATIC+0.0, [S1 "
              "H148R-0 AUTOMATIC+0.0 (overridden), S2 I357T-14 AUTOMATIC+0.0 (overridden)]\n");
    EXPECT_EQ(tablesOf(source),
              "motors: Configuration | Sustainer Motor Mount | Booster motor mount | Booster motor "
              "mount\n"
              "  0: [H148R-0; H148R-0] | H148R-0 / Burnout | H148R-0 / Automatic | H148R-0 / "
              "Automatic\n"
              "  1: [I59WN-P; I357T-14] | I59WN-P / Burnout | I357T-14 / Automatic | I357T-14 / "
              "Automatic\n"
              "recovery: Configuration | Sustainer Main | Sustainer Drogue | Booster Chute\n"
              "  0: [H148R-0; H148R-0] | Altitude 152 m | Apogee | Apogee\n"
              "  1: [I59WN-P; I357T-14] | Altitude 152 m | Apogee | Apogee\n"
              "stages: Configuration | Booster\n"
              "  0: [H148R-0; H148R-0] | Current stage motor burnout\n"
              "  1: [I59WN-P; I357T-14] | Upper stage motor ignition\n");
}

}  // namespace

}  // namespace QtRocket::Gui
