// The extensions of a simulation as the simulation options tab lists them (OpenRocket's
// SimulationOptionsPanel's currentExtensions and SimulationExtensionPanel), over the simulations of
// the examples "Simulation extensions" and "Simulation scripting": what the probe
// tools/openrocket-probes/simulation-options shows and does (SimulationOptionsPins.h, its EXT,
// HELP and extension walks), in the same order. Where QtRocket differs on purpose, the pinned
// line is edited here, with the reason: the Configure button of an extension comes with
// Milestone 7 (the user's decision U-M4-3).

#include "QtRocket/gui/dialogs/simulation/SimulationExtensionList.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QObject>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/file/GeneralRocketLoader.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/ThemeColors.h"
#include "QtRocket/gui/widgets/DescriptionArea.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/simulation/Simulation.h"
#include "QtRocket/simulation/extension/SimulationExtension.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "TestPaths.h"
#include "dialogs/simulation/SimulationDialogWalk.h"
#include "dialogs/simulation/SimulationOptionsPins.h"
#include "file/DesignFileEnvironment.h"
#include "motor/TestMotorDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using SimulationExtensionListTest = Test::GuiTest;
namespace Pins                    = Test::SimulationOptionsPins;

/// An example design (data/examples) loaded as the probe loads it.
class Example
{
public:
    explicit Example(std::string_view name)
    {
        Result<LoadedDocument> loaded =
            GeneralRocketLoader(m_environment.context())
                .load(QtRocket::Test::dataDir() / "examples" / (std::string(name) + ".ork"));
        if (loaded.has_value())
        {
            m_document = std::move(loaded->document);
        }
        else
        {
            ADD_FAILURE() << loaded.error().message;
        }
    }

    /// The design's simulations; none, with a test failure, when it cannot be loaded.
    [[nodiscard]] std::vector<std::shared_ptr<Simulation>> simulations() const
    {
        std::vector<std::shared_ptr<Simulation>> simulations;
        if (m_document != nullptr)
        {
            for (std::size_t index = 0; index < m_document->getSimulationCount(); ++index)
            {
                simulations.push_back(m_document->getSimulation(index));
            }
        }
        return simulations;
    }

private:
    ThrustCurveMotorSetDatabase           m_motors = QtRocket::Test::estesA8Database();
    QtRocket::Test::DesignFileEnvironment m_environment{
        QtRocket::Test::DesignFileEnvironment::Presets::NONE, m_motors};
    std::unique_ptr<OpenRocketDocument> m_document;
};

/// @p text with its line breaks written as "\n" (the probe's clean()).
[[nodiscard]] std::string clean(const std::string& text)
{
    std::string cleaned;
    for (const char character : text)
    {
        cleaned += character == '\n' ? std::string("\\n") : std::string(1, character);
    }
    return cleaned;
}

/// The walk of @p list, each line with the probe's prefix "WALK ".
void appendWalk(std::vector<std::string>& lines, const SimulationExtensionList& list)
{
    for (const std::string& line : Gui::Test::dialogWalk(list))
    {
        lines.push_back("WALK " + line);
    }
}

/// The probe's EXT lines of @p list: each extension of the simulation, then each panel (or the
/// line shown without extensions).
void appendExtensions(std::vector<std::string>& lines, const SimulationExtensionList& list)
{
    for (const std::shared_ptr<SimulationExtension>& extension :
         list.simulation()->getSimulationExtensions())
    {
        lines.push_back("EXT extension name=[" + clean(extension->getName()) +
                        "] id=" + extension->getId() + " description=[" +
                        clean(extension->getDescription().value_or("")) + "]");
    }
    if (const StyledLabel* const empty = list.noExtensionsLabel(); empty != nullptr)
    {
        lines.push_back("EXT LABEL " + QtStrings::toUtf8(empty->text()));
    }
    for (std::size_t index = 0; index < list.extensionCount(); ++index)
    {
        std::string panel =
            "EXT panel name=[" + QtStrings::toUtf8(list.nameLabel(index).text()) + "]";
        if (list.helpButton(index) != nullptr)
        {
            panel += " button=HELP";
        }
        panel += " button=DELETE";
        lines.push_back(panel);
    }
}

/// What the probe's help() prints of an extension's help dialog, and its Close clicked.
struct HelpAnswer
{
    std::vector<std::string>* lines = nullptr;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<QDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        const auto* const area =
            dialog->findChild<DescriptionArea*>(QStringLiteral("descriptionArea"));
        auto* const close = dialog->findChild<QPushButton*>(QStringLiteral("closeButton"));
        ASSERT_NE(area, nullptr);
        ASSERT_NE(close, nullptr);
        const QString defaultButton = close->isDefault() ? close->text() : QStringLiteral("none");
        // A QDialog's Escape closes it (QDialog::reject()), as Java's installed action does.
        lines->push_back(
            "  HELP title=[" + QtStrings::toUtf8(dialog->windowTitle()) + "] modality=" +
            (dialog->windowModality() == Qt::ApplicationModal ? "APPLICATION_MODAL" : "other") +
            " text=[" + QtStrings::toUtf8(area->toPlainText().trimmed()) +
            "] close=true default=" + QtStrings::toUtf8(defaultButton) + " escape=true");
        close->click();
        lines->push_back(std::string("  HELP closed: visible=") +
                         (dialog->isVisible() ? "true" : "false"));
    }
};

/// The probe's help dialogs and deletions on @p list (the last simulation's).
void appendHelpAndDeletions(std::vector<std::string>& lines, const SimulationExtensionList& list)
{
    for (std::size_t index = 0; index < list.extensionCount(); ++index)
    {
        if (list.helpButton(index) == nullptr)
        {
            continue;
        }
        const Gui::Test::NextModalDialog answer(HelpAnswer{.lines = &lines});
        list.helpButton(index)->click();
        EXPECT_TRUE(answer.acted());
    }
    while (list.extensionCount() > 0)
    {
        list.deleteButton(0).click();
        std::string names;
        for (const std::shared_ptr<SimulationExtension>& extension :
             list.simulation()->getSimulationExtensions())
        {
            names += " [" + extension->getName() + "]";
        }
        lines.push_back("STEP deleted the first | extensions" + names);
        lines.emplace_back("== extensions after the deletion");
        appendWalk(lines, list);
    }
}

/// The probe's transcript of the extensions of the example @p name.
[[nodiscard]] std::vector<std::string> extensionsTranscript(std::string_view name)
{
    std::vector<std::string> lines;
    const Example            example(name);
    const auto               simulations = example.simulations();
    for (const std::shared_ptr<Simulation>& simulation : simulations)
    {
        const SimulationExtensionList list(simulation, nullptr);
        lines.push_back("== extensions: " + std::string(name) + ", " + simulation->getName());
        appendWalk(lines, list);
        appendExtensions(lines, list);
    }
    if (!simulations.empty())
    {
        const SimulationExtensionList list(simulations.back(), nullptr);
        appendHelpAndDeletions(lines, list);
    }
    return lines;
}

/// @p pins without Java's Configure button: the first of the three buttons after each
/// extension's name in a walk, and "button=CONFIGURE" of the panels (Milestone 7, U-M4-3).
[[nodiscard]] std::vector<std::string> withoutConfigure(std::span<const std::string_view> pins)
{
    std::vector<std::string> lines;
    bool                     afterName = false;
    for (const std::string_view pin : pins)
    {
        std::string line(pin);
        if (afterName && line == "WALK BUTTON  icon")
        {
            afterName = false;
            continue;
        }
        afterName = line.starts_with("WALK LABEL ") &&
                    line != "WALK LABEL No simulation extensions defined";
        if (const std::size_t at = line.find(" button=CONFIGURE"); at != std::string::npos)
        {
            line.erase(at, std::string_view(" button=CONFIGURE").size());
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

TEST_F(SimulationExtensionListTest, TheExtensionsOfTheExampleAreListedAsJavaListsThem)
{
    EXPECT_EQ(extensionsTranscript("Simulation extensions"),
              withoutConfigure(Pins::kExtensionsExample));
}

// The scripting extension, which QtRocket cannot run, is listed with its name, Help and Delete
// all the same.
TEST_F(SimulationExtensionListTest, TheScriptingExtensionsAreListedAsJavaListsThem)
{
    EXPECT_EQ(extensionsTranscript("Simulation scripting"),
              withoutConfigure(Pins::kScriptingExample));
}

TEST_F(SimulationExtensionListTest, DeletingAnExtensionTellsItsObservers)
{
    const Example example("Simulation extensions");
    const auto    simulations = example.simulations();
    ASSERT_EQ(simulations.size(), 3U);
    SimulationExtensionList list(simulations[2], nullptr);
    const QSignalSpy        changed(&list, &SimulationExtensionList::extensionsChanged);
    const std::shared_ptr<SimulationExtension> second =
        simulations[2]->getSimulationExtensions().at(1);
    list.deleteExtension(0);
    EXPECT_EQ(changed.count(), 1);
    // The other extension stays, the same object.
    ASSERT_EQ(simulations[2]->getSimulationExtensions().size(), 1U);
    EXPECT_EQ(simulations[2]->getSimulationExtensions().front(), second);
    ASSERT_EQ(list.extensionCount(), 1U);
    EXPECT_EQ(list.nameLabel(0).text(), QtStrings::fromUtf8(second->getName()));
    EXPECT_EQ(list.noExtensionsLabel(), nullptr);
    list.deleteExtension(0);
    EXPECT_EQ(changed.count(), 2);
    EXPECT_TRUE(simulations[2]->getSimulationExtensions().empty());
    ASSERT_NE(list.noExtensionsLabel(), nullptr);
    EXPECT_THROW(static_cast<void>(list.nameLabel(0)), BugError);
}

/// Escape pressed in a help dialog, and whether it closed it.
struct EscapeAnswer
{
    bool* closed = nullptr;

    void operator()(QWidget& dialog) const
    {
        QTest::keyClick(&dialog, Qt::Key_Escape);
        *closed = !dialog.isVisible();
    }
};

TEST_F(SimulationExtensionListTest, EscapeClosesTheHelpDialog)
{
    const Example example("Simulation extensions");
    const auto    simulations = example.simulations();
    ASSERT_EQ(simulations.size(), 3U);
    SimulationExtensionList          list(simulations[1], nullptr);
    bool                             closed = false;
    const Gui::Test::NextModalDialog answer(EscapeAnswer{.closed = &closed});
    list.showHelp(0);
    EXPECT_TRUE(answer.acted());
    EXPECT_TRUE(closed);
}

TEST_F(SimulationExtensionListTest, ThePanelsHaveJavasBorderAndHeight)
{
    const Example example("Simulation extensions");
    const auto    simulations = example.simulations();
    ASSERT_EQ(simulations.size(), 3U);
    const SimulationExtensionList list(simulations[2], nullptr);
    EXPECT_EQ(list.sizeHint().height(), SimulationExtensionList::kPreferredHeight);
    EXPECT_EQ(list.minimumHeight(), SimulationExtensionList::kMinimumHeight);
    ASSERT_EQ(list.extensionCount(), 2U);
    const QFrame& panel = list.extensionPanel(0);
    EXPECT_EQ(panel.frameShape(), QFrame::Box);
    EXPECT_EQ(panel.lineWidth(), 1);
    EXPECT_EQ(panel.palette().color(QPalette::WindowText),
              ThemeColors::color(ThemeColors::Key::TEXT_DIM, list.palette()));
    EXPECT_EQ(list.nameLabel(0).palette().color(QPalette::WindowText),
              list.palette().color(QPalette::WindowText));
}

TEST_F(SimulationExtensionListTest, TheListNeedsASimulation)
{
    EXPECT_THROW(SimulationExtensionList(nullptr, nullptr), BugError);
}

}  // namespace

}  // namespace QtRocket::Gui
