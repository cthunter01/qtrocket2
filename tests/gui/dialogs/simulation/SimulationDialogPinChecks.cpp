#include "dialogs/simulation/SimulationDialogPinChecks.h"

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QColor>
#include <QString>
#include <QTabWidget>

#include <gtest/gtest.h>

#include "QtRocket/gui/dialogs/simulation/SimulationConfigDialog.h"
#include "QtRocket/gui/util/ColorConversion.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/SimulationStatusColors.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/simulation/Simulation.h"
#include "dialogs/simulation/SimulationDialogPins.h"
#include "dialogs/simulation/SimulationDialogWalk.h"

namespace QtRocket::Gui::Test
{

namespace
{

/// The tabs whose pages the walk holds: Launch conditions, Simulation options and Warnings.
constexpr int kWalkedTabs = 3;

/// The walk's line of the status (the top rows' "LABEL Status:" is followed by it).
constexpr std::string_view kStatusLabelLine = "LABEL Status:";
/// The status line of OpenRocket's walk: kStatusPrefix, "#rrggbb", kStatusMiddle, the text,
/// kStatusSuffix.
constexpr std::string_view kStatusPrefix   = "LABEL <html><font color=\"";
constexpr std::string_view kStatusMiddle   = "\">";
constexpr std::string_view kStatusSuffix   = "</font></html>";
constexpr std::size_t      kHexColorLength = 7;

/// The line of the extensions' box, and its lines' indentation.
constexpr std::string_view kExtensionsBorder  = "  BORDER Simulation extensions";
constexpr std::string_view kExtensionIndent   = "    ";
constexpr std::string_view kNoExtensionsLine  = "    LABEL No simulation extensions defined";
constexpr std::string_view kExtensionNameLine = "    LABEL ";
constexpr std::string_view kIconButtonLine    = "    BUTTON  icon";

/// The name of @p key in the pins' colours ("text"), empty for a key the status does not take.
[[nodiscard]] std::string_view colorName(ThemeColors::Key key)
{
    switch (key)
    {
        case ThemeColors::Key::TEXT:
            return "text";
        case ThemeColors::Key::WARNING:
            return "warning";
        case ThemeColors::Key::ERROR:
            return "error";
        default:
            return {};
    }
}

/// OpenRocket's status line @p line in QtRocket's colour of @p status; @p line itself (with a test
/// failure) when it is not a status line, or when its colour is not the theme's of the status.
[[nodiscard]] std::string statusLineOf(const std::string& line, Simulation::Status status,
                                       const SimulationDialogPins& pins)
{
    const std::size_t textStart = kStatusPrefix.size() + kHexColorLength + kStatusMiddle.size();
    if (!line.starts_with(kStatusPrefix) || !line.ends_with(kStatusSuffix) ||
        line.size() < textStart + kStatusSuffix.size() ||
        std::string_view(line).substr(kStatusPrefix.size() + kHexColorLength,
                                      kStatusMiddle.size()) != kStatusMiddle)
    {
        ADD_FAILURE() << "not a status line: " << line;
        return line;
    }
    const std::string color = line.substr(kStatusPrefix.size(), kHexColorLength);
    const std::string key(colorName(SimulationStatusColors::key(status)));
    const auto        found = pins.colors.find(key);
    EXPECT_TRUE(found != pins.colors.end() && found->second == color)
        << "OpenRocket's status colour " << color << " is not its theme's " << key << " of "
        << name(status);
    const std::string text = line.substr(textStart, line.size() - textStart - kStatusSuffix.size());
    return "LABEL <html>" +
           QtStrings::toUtf8(ColorConversion::formatHtmlColor(SimulationStatusColors::color(status),
                                                              QtStrings::fromUtf8(text))) +
           "</html>";
}

/// @p lines with the status line (after "LABEL Status:") in QtRocket's colour of @p status.
[[nodiscard]] std::vector<std::string> withStatusColor(std::vector<std::string>    lines,
                                                       Simulation::Status          status,
                                                       const SimulationDialogPins& pins)
{
    for (std::size_t index = 0; index + 1 < lines.size(); ++index)
    {
        if (lines[index] == kStatusLabelLine)
        {
            lines[index + 1] = statusLineOf(lines[index + 1], status, pins);
            return lines;
        }
    }
    ADD_FAILURE() << "the walk has no status line";
    return lines;
}

/// @p lines with every @p from replaced by @p to (in any line, any number of times).
[[nodiscard]] std::vector<std::string> replacedEverywhere(std::vector<std::string> lines,
                                                          std::string_view         from,
                                                          std::string_view         to)
{
    for (std::string& line : lines)
    {
        if (const std::size_t at = line.find(from); at != std::string::npos)
        {
            line.replace(at, from.size(), to);
        }
    }
    return lines;
}

/// Whether @p line names an extension in the extensions' box (its panel's label).
[[nodiscard]] bool isExtensionName(std::string_view line)
{
    return line.starts_with(kExtensionNameLine) && line != kNoExtensionsLine;
}

/// The walk of the extensions' panels, which @p extensions describe in order (their buttons by
/// their icons), without the Configure buttons: the first icon button after an extension's name
/// where its panel's first button is CONFIGURE.
class ConfigureButtonsRemoval
{
public:
    explicit ConfigureButtonsRemoval(const std::vector<std::string>& extensions)
      : m_extensions(&extensions)
    {
    }

    /// Whether @p line stays.
    [[nodiscard]] bool keeps(const std::string& line)
    {
        if (line == kExtensionsBorder)
        {
            m_inExtensions = true;
            return true;
        }
        if (!line.starts_with(kExtensionIndent))
        {
            m_inExtensions = false;
        }
        if (!m_inExtensions)
        {
            return true;
        }
        if (m_dropNextButton && line == kIconButtonLine)
        {
            m_dropNextButton = false;
            return false;
        }
        if (isExtensionName(line))
        {
            m_dropNextButton =
                m_named < m_extensions->size() && (*m_extensions)[m_named].starts_with("CONFIGURE");
            ++m_named;
        }
        return true;
    }

    /// The number of extensions' names seen.
    [[nodiscard]] std::size_t named() const noexcept { return m_named; }

private:
    const std::vector<std::string>* m_extensions;
    bool                            m_inExtensions   = false;
    bool                            m_dropNextButton = false;
    std::size_t                     m_named          = 0;
};

/// @p lines without the extensions' Configure buttons (Milestone 7, U-M4-3).
[[nodiscard]] std::vector<std::string> withoutConfigureButtons(
    const std::vector<std::string>& lines, const std::vector<std::string>& extensions)
{
    std::vector<std::string> kept;
    ConfigureButtonsRemoval  removal(extensions);
    for (const std::string& line : lines)
    {
        if (removal.keeps(line))
        {
            kept.push_back(line);
        }
    }
    EXPECT_EQ(removal.named(), extensions.size()) << "extensions' names in the walk";
    return kept;
}

}  // namespace

std::vector<std::string> settingsDialogWalk(const SimulationConfigDialog& dialog)
{
    std::vector<std::string> lines = dialogWalk(dialog.topPanel());
    const QTabWidget&        tabs  = dialog.tabs();
    QString                  titles;
    for (int index = 0; index < tabs.count(); ++index)
    {
        titles += QStringLiteral(" [") + tabs.tabText(index) + QStringLiteral("]");
    }
    lines.push_back(QtStrings::toUtf8(QStringLiteral("TABS") + titles +
                                      QStringLiteral(" selected=%1").arg(tabs.currentIndex())));
    for (int index = 0; index < tabs.count(); ++index)
    {
        lines.push_back("TAB " + QtStrings::toUtf8(tabs.tabText(index)));
        if (index < kWalkedTabs)
        {
            for (const std::string& line : dialogWalk(*tabs.widget(index)))
            {
                lines.push_back("  " + line);
            }
        }
    }
    for (std::string& line : dialogWalk(dialog.bottomPanel()))
    {
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> settingsDialogTabStates(const QTabWidget& tabs)
{
    std::vector<std::string> states;
    for (int index = 0; index < tabs.count(); ++index)
    {
        QString state =
            tabs.tabText(index) +
            (tabs.isTabEnabled(index) ? QStringLiteral(" enabled") : QStringLiteral(" disabled"));
        if (!tabs.tabToolTip(index).isEmpty())
        {
            state += QStringLiteral(" tip=") + tabs.tabToolTip(index);
        }
        states.push_back(QtStrings::toUtf8(state));
    }
    return states;
}

std::vector<std::string> expectedSettingsDialogWalk(const SimulationDialogPins&          pins,
                                                    const SimulationDialogPinSimulation& simulation,
                                                    Simulation::Status                   status)
{
    std::vector<std::string> lines = withStatusColor(simulation.walk, status, pins);
    lines                          = without(std::move(lines), "    BUTTON Add extension");
    lines                          = withoutConfigureButtons(lines, simulation.extensions);
    lines = replacedEverywhere(std::move(lines), "BUTTON Single Deployment Warnings icon",
                               "BUTTON Single Deployment Warnings");
    lines = replacedEverywhere(std::move(lines), "BUTTON Dual Deployment Warnings icon",
                               "BUTTON Dual Deployment Warnings");
    return replacedEverywhere(std::move(lines),
                              "LABEL <html><body style=", "TEXTAREA <html><body style=");
}

std::vector<std::string> expectedSettingsDialogTabStates(
    const SimulationDialogPinSimulation& simulation, bool exportPlaceholder)
{
    constexpr auto           kExport = static_cast<std::size_t>(SimulationConfigDialog::kExportTab);
    std::vector<std::string> states  = simulation.tabs;
    if (exportPlaceholder && states.size() > kExport)
    {
        std::string& exportState = states[kExport];
        if (exportState == "Export data enabled")
        {
            exportState = "Export data disabled";
        }
    }
    return states;
}

}  // namespace QtRocket::Gui::Test
