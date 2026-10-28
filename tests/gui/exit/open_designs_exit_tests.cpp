// The exit suite of Milestone 2 in the direction "a design made in OpenRocket opens in QtRocket"
// (plan section 9.5, OpenDesignsExitGolden; the tests-exit scout's section 2.3): OpenRocket's
// sixteen example designs (data/examples), the 49 re-saves of its golden harness
// (tests/data/goldens/*/resave/rocket.ork: the sixteen examples, the thirteen test rockets, the
// eighteen legacy files and the two saver-coverage documents, written by OpenRocket 5f164fd0e) and
// the design OpenRocket's GUI built from scratch (tests/data/goldens/gui/from-scratch/rocket.ork,
// tools/openrocket-gui-reference), each opened through File > Open of a New design's window (the
// application's DocumentFileOperations, the file chooser's answer scripted), as a user opens it:
// - the window takes the place of the New design, with the title "<rocket> (<file>)", saved, and
//   the load's warnings shown are those the core's tests record (kLoadWarnings);
// - the component tree has a row for every component, in the order of describeRocket() (the
//   core's description of a loaded design), each with the component's name and icon, every row
//   in view, under QAbstractItemModelTester (Warning mode, whose complaints fail the test);
// - every component's configuration dialog opens (the component selected in the tree, Edit) with
//   its title, the component's name and its values in the main fields (length, radii and
//   diameters, height, chords, the mass of a mass component, the mass and CG overrides with their
//   check boxes): the component's own values, in the field's unit, and for the designs with
//   goldens OpenRocket's (geometry.json, within the golden tolerance). Its Cancel leaves the
//   design as the dialog found it (saved, the undo history, no question asked), also after an
//   edit made first (the rocket's comment, whose step of the history every Cancel keeps);
//   opening a dialog changes nothing either, but for the dialogs of kOpeningChanges, which
//   OpenRocket's change too;
// - the figure paints every view of the view selector without a Qt warning, and its subject
//   bounds hold every shape it draws, but for what OpenRocket draws past them too (outsideOf():
//   the aft end of a motor, the marker of a pod set or a booster set, the tubes of a tube fin set
//   by less than 0.1 mm);
// - the information overlay shows, as opened and for every flight configuration chosen in the
//   configuration selector, OpenRocket's launch CG and worst CP at the default Mach number with
//   their texts and that of the stability margin, painted in the figure (GoldenDesign's mass.json
//   and aero.json, within the golden tolerance; the GUI reference's state.json for its CG); for
//   the designs without goldens, what the command-line tool's 'info' prints, which computes as
//   the core;
// - the "Motors & Configuration" tab (Milestone 3, plan section 10.5), when it is chosen, shows
//   OpenRocket's tab of the loaded design (OpenDesignsTabPins.h, the open-designs family of
//   tools/openrocket-gui-pins: OpenRocket's real FlightConfigurationPanel over the same file):
//   the rows and their configurations, the three sub-tabs, every button, the help text of a
//   design without a motor mount, the three tables cell by cell through the panels' models (the
//   texts, the shading, the selection the tables make), the motor mount table, and the design's
//   selection that the Motors table sets; opening it changes nothing, and a click on a row
//   chooses its configuration, in the design view's configuration selector too.
//
// The environment of the windows is that of the core's golden tests (DesignFileEnvironment with
// the six component presets of the example designs, Presets::EXAMPLES: the bundled motor
// database, OpenRocket's test preferences and materials, the bundled extensions), so that the
// goldens apply; the thirteen test rockets are loaded against the seven motors of TestRockets, as
// resave_golden_tests.cpp loads them (their motors are synthetic). The six presets are the only
// ones the designs name, and the bundled presets give the same six
// (OpenDesignsExitGoldenPresets): with them the three examples that name presets, and their
// re-saves, load without the six "No matching ComponentPreset" warnings an application without
// presets gives, as with the application's bundled presets, at no cost (the bundled presets take a
// second and a half to read in a debug build). The windows are those of the application without
// its start-up: a WindowManager with ConfigDialogRequests and DocumentFileOperations, as
// ApplicationSession connects them (the application's own start is tested in tests/gui/app/).
//
// Each of the six tests of a design opens it anew, so that ctest runs them as separate processes
// in parallel: 399 tests. Milestone 2's 331 took about 20 s on 8 jobs in a debug build (145 s of
// processes, none above 1.2 s; the examples, whose archives hold flight data, take the longest)
// and about 65 s under the asan preset (500 s of processes, none above 6 s); the tab's test of a
// design costs about what each of the other five costs (most of it is the opening), and the test
// of the presets reads the bundled ones once (a few seconds).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QLatin1Char>
#include <QLineEdit>
#include <QList>
#include <QModelIndex>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QRegion>
#include <QString>
#include <QTabWidget>
#include <QTest>
#include <QTextCursor>
#include <QVariant>
#include <QWidget>
#include <Qt>
#include <QtTypes>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include "GuiTestSupport.h"
#include "OpenDesignsTabPins.h"
#include "QtRocket/cli/CliOutput.h"
#include "QtRocket/cli/InfoCommand.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/figure/FigureView.h"
#include "QtRocket/figure/RocketFigureGeometry.h"
#include "QtRocket/gui/binding/DoubleBinding.h"
#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/figure/DesignPhysics.h"
#include "QtRocket/gui/figure/RocketFigure.h"
#include "QtRocket/gui/figure/RocketInfo.h"
#include "QtRocket/gui/figure/RocketPanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurablePanel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurableTableModel.h"
#include "QtRocket/gui/flightconfig/FlightConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/MotorMountTableModel.h"
#include "QtRocket/gui/flightconfig/RecoveryConfigurationPanel.h"
#include "QtRocket/gui/flightconfig/SeparationConfigurationPanel.h"
#include "QtRocket/gui/main/ConfigDialogRequests.h"
#include "QtRocket/gui/main/DesignPanel.h"
#include "QtRocket/gui/main/DocumentFileOperations.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/MainWindow.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/main/WindowManager.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/ConfigurationComboBox.h"
#include "QtRocket/gui/widgets/IconButton.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/mass/MassCalculator.h"
#include "QtRocket/material/BuiltinMaterials.h"
#include "QtRocket/material/MaterialStorage.h"
#include "QtRocket/motor/Manufacturer.h"
#include "QtRocket/motor/ThrustCurveMotorSetDatabase.h"
#include "QtRocket/rocket/Coaxial.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/EllipticalFinSet.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MassObject.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/StabilityUnits.h"
#include "QtRocket/rocket/SymmetricComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/unit/Unit.h"
#include "QtRocket/unit/UnitGroup.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/FileIo.h"
#include "QtRocket/util/Geometry2D.h"
#include "QtRocket/util/MathUtil.h"
#include "QtRocket/util/Strings.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "dialogs/PreferencesPageTranscript.h"
#include "figure/RecordingPaintDevice.h"
#include "file/DesignFileEnvironment.h"
#include "file/openrocket/ComponentHandlerTestSupport.h"
#include "flightconfig/FlightConfigurationTabSupport.h"
#include "flightconfig/FlightConfigurationTablePins.h"
#include "goldens/GoldenData.h"
#include "goldens/GoldenDesign.h"
#include "goldens/GoldenExamples.h"
#include "goldens/GoldenGuiReference.h"
#include "goldens/GoldenMismatches.h"
#include "goldens/GoldenResaves.h"
#include "motor/TestMotorDatabase.h"
#include "rocket/preset/ExamplePresets.h"
#include "rocket/preset/TestPresetDatabase.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::DesignFileEnvironment;
using QtRocket::Test::DesignGoldens;
using QtRocket::Test::kGoldenAbsolute;
using QtRocket::Test::kGoldenRelative;
using PromptKind = Gui::Test::ScriptedUserPrompts::Kind;

// ============================================================================ the designs

/// Where a design of the suite comes from.
enum class DesignSource
{
    EXAMPLE,         ///< a file of data/examples (an archive)
    EXAMPLE_RESAVE,  ///< OpenRocket's re-save of an example
    TEST_ROCKET,     ///< OpenRocket's re-save of a rocket of TestRockets.java
    LEGACY,          ///< OpenRocket's re-save of a design file of tests/data/ork
    COVERAGE,        ///< OpenRocket's re-save of a saver-coverage document of the harness
    GUI_REFERENCE,   ///< the design OpenRocket's GUI built from scratch
};

/// One design the suite opens.
struct OpenDesign
{
    /// The name of its tests ("example_a_simple_model_rocket", "legacy_v1_6_apocd").
    std::string  name;
    DesignSource source{DesignSource::EXAMPLE};
    /// The file File > Open is given.
    std::filesystem::path file;
    /// The golden input whose geometry.json, mass.json and aero.json describe the design; empty
    /// for a design without goldens (the legacy and coverage re-saves, the GUI reference).
    std::string goldenInput;
};

/// What GoogleTest prints for the design of a test (its name, which ctest's test names show too:
/// gtest_discover_tests() puts the printed parameter in place of the parameter's name).
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const OpenDesign& design, std::ostream* out)
{
    *out << design.name;
}

/// @p text with '_' for '-' (a test's name).
[[nodiscard]] std::string testNameOf(std::string_view text)
{
    std::string name(text);
    std::ranges::replace(name, '-', '_');
    return name;
}

/// The 66 designs, in the order of the plan: the examples, then the re-saves of the manifest
/// (examples, test rockets, legacy files, coverage documents), then the GUI reference. What went
/// wrong reading the manifest goes to @p problems.
[[nodiscard]] std::vector<OpenDesign> makeOpenDesigns(std::string& problems)
{
    constexpr std::size_t   kDesigns = 66;
    std::vector<OpenDesign> designs;
    designs.reserve(kDesigns);
    const auto& examples = QtRocket::Test::goldenExamples();
    for (const QtRocket::Test::GoldenExample& example : examples)
    {
        designs.push_back({.name        = testNameOf(example.name),
                           .source      = DesignSource::EXAMPLE,
                           .file        = example.original,
                           .goldenInput = example.name});
    }
    for (const QtRocket::Test::GoldenExample& example : examples)
    {
        designs.push_back({.name        = "resave_" + testNameOf(example.name),
                           .source      = DesignSource::EXAMPLE_RESAVE,
                           .file        = example.resave,
                           .goldenInput = example.name});
    }
    for (const QtRocket::Test::GoldenInput& input : QtRocket::Test::testRocketInputs(problems))
    {
        designs.push_back({.name        = testNameOf(input.name),
                           .source      = DesignSource::TEST_ROCKET,
                           .file        = QtRocket::Test::goldensDir() / input.resave,
                           .goldenInput = input.name});
    }
    for (const auto& [kind, source] :
         {std::pair(QtRocket::Test::kLegacyKind, DesignSource::LEGACY),
          std::pair(QtRocket::Test::kCoverageKind, DesignSource::COVERAGE)})
    {
        for (const QtRocket::Test::ResaveInput& input :
             QtRocket::Test::resaveInputsOf(kind, problems))
        {
            designs.push_back({.name        = testNameOf(input.name),
                               .source      = source,
                               .file        = QtRocket::Test::goldensDir() / input.resave,
                               .goldenInput = {}});
        }
    }
    designs.push_back({.name        = "gui_from_scratch",
                       .source      = DesignSource::GUI_REFERENCE,
                       .file        = QtRocket::Test::guiFromScratchDir() / "rocket.ork",
                       .goldenInput = {}});
    return designs;
}

/// The designs and what went wrong making the list, once per test process.
struct DesignList
{
    std::string             problems;
    std::vector<OpenDesign> designs;
};

[[nodiscard]] const DesignList& designList()
{
    static const DesignList kList = [] {
        DesignList list;
        list.designs = makeOpenDesigns(list.problems);
        return list;
    }();
    return kList;
}

[[nodiscard]] const std::vector<OpenDesign>& openDesigns()
{
    return designList().designs;
}

[[nodiscard]] std::string openDesignTestName(const ::testing::TestParamInfo<OpenDesign>& info)
{
    return info.param.name;
}

// ===================================================================== the load warnings

/// The warnings a design's load gives, by the name of its tests, as the core's tests record them;
/// every other design loads without a warning.
struct LoadWarnings
{
    std::string_view                  design;
    std::span<const std::string_view> warnings;
};

// (The three examples that name component presets, and their re-saves, find them all in the six
// presets of the environment, and give no warning: OpenDesignsExitGoldenPresets.)
// OpenRocket's re-save of the parallel boosters names its six textures by the absolute paths
// "/datafiles/textures/*", which a design may not read (decision D8 of 2026-10-14; GoldenExamples.h
// and LoaderWarningsTests.cpp, decalFileOutsideDesign()).
constexpr std::array<std::string_view, 6> kTextureWarnings{
    "The decal image file '/datafiles/textures/wadding.png' was not read: a design may name such "
    "a file only by a relative path inside its own directory; ignoring the decal.",
    "The decal image file '/datafiles/textures/spiral-wound-alpha.png' was not read: a design may "
    "name such a file only by a relative path inside its own directory; ignoring the decal.",
    "The decal image file '/datafiles/textures/balsa.jpg' was not read: a design may name such a "
    "file only by a relative path inside its own directory; ignoring the decal.",
    "The decal image file '/datafiles/textures/wood.jpg' was not read: a design may name such a "
    "file only by a relative path inside its own directory; ignoring the decal.",
    "The decal image file '/datafiles/textures/hardboard.jpg' was not read: a design may name such "
    "a file only by a relative path inside its own directory; ignoring the decal.",
    "The decal image file '/datafiles/textures/chute.jpg' was not read: a design may name such a "
    "file only by a relative path inside its own directory; ignoring the decal."};
// OpenRocket's re-save of v1.7-simulation-extensions-and-scripting.ork keeps its roll control
// scripts enabled, which QtRocket disables on load (decisions D1 and E4;
// legacy_ork_roundtrip_tests.cpp, kOwnStateChanges, and ScriptingExtensionTests.cpp).
constexpr std::array<std::string_view, 1> kScriptWarnings{
    "Scripts that OpenRocket trusts have been disabled: QtRocket runs no scripts."};

constexpr std::array kLoadWarnings{
    LoadWarnings{.design = "resave_example_parallel_booster_staging", .warnings = kTextureWarnings},
    LoadWarnings{.design   = "legacy_v1_7_simulation_extensions_and_scripting",
                 .warnings = kScriptWarnings},
};

/// The warnings the load of @p design gives (kLoadWarnings).
[[nodiscard]] std::vector<std::string> expectedLoadWarnings(const OpenDesign& design)
{
    for (const LoadWarnings& row : kLoadWarnings)
    {
        if (row.design == design.name)
        {
            return {row.warnings.begin(), row.warnings.end()};
        }
    }
    return {};
}

// ============================================================= the dialogs that change

/// The components whose dialog changes the design when it opens, by the name of the design's
/// tests; no other dialog does. OpenRocket's BodyTubeConfig switches the automatic outer radius
/// of a body tube off when no neighbour gives one (updateCheckboxAutoAftRadius(), and so
/// BodyTubeConfig::updateAutomaticCheckBox()): the two body tubes of makeClusterPods(), each the
/// only component of its stage or booster set, have automatic radii. The change comes before the
/// dialog's undo position, so the undo history labels it with the description the last dialog
/// left, and a Cancel does not take it back (the user's decision U4: what a panel changes while
/// it is made does not count as the dialog's modification), as in OpenRocket.
struct OpeningChanges
{
    std::string_view                  design;
    std::span<const std::string_view> components;
};

constexpr std::array<std::string_view, 2> kClusterPodsOpening{"Sustainer Body Tube",
                                                              "Side Booster Body Tubes"};
constexpr std::array                      kOpeningChanges{
    OpeningChanges{.design = "testrocket_cluster_pods", .components = kClusterPodsOpening}};

/// The components whose dialog changes @p design when it opens (kOpeningChanges).
[[nodiscard]] std::vector<std::string> expectedOpeningChanges(const OpenDesign& design)
{
    for (const OpeningChanges& row : kOpeningChanges)
    {
        if (row.design == design.name)
        {
            return {row.components.begin(), row.components.end()};
        }
    }
    return {};
}

// ===================================================================== the environment

/// The services of the windows (see the top of the file): a DesignFileEnvironment with the six
/// component presets of the example designs over @p motors, OpenRocket's built-in materials as
/// the application's materials, the environment's presets as the windows' presets too (the parts
/// library's), and the scripted questions.
class DesignServices
{
public:
    explicit DesignServices(const ThrustCurveMotorSetDatabase& motors)
      : m_environment(DesignFileEnvironment::Presets::EXAMPLES, motors),
        m_services(m_environment.preferences(), m_materials, motors, m_environment.presets(),
                   m_environment.context(), m_prompts)
    {
        addBuiltinMaterials(m_materials);
    }

    [[nodiscard]] const GuiServices&              services() const noexcept { return m_services; }
    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() noexcept { return m_prompts; }
    [[nodiscard]] const Preferences&              preferences() const noexcept
    {
        return m_environment.preferences();
    }

private:
    DesignFileEnvironment          m_environment;
    MaterialStorage                m_materials;
    Gui::Test::ScriptedUserPrompts m_prompts;
    GuiServices                    m_services;
};

/// Delivers the posted events until the windows, their layouts and their figures are done.
void settle()
{
    constexpr int kPasses = 4;
    for (int pass = 0; pass < kPasses; ++pass)
    {
        QApplication::processEvents();
    }
}

/// The components of @p rocket in the order of its tree (a component before its children), the
/// rocket first.
void collectInTreeOrder(const RocketComponent&               component,
                        std::vector<const RocketComponent*>& components)
{
    components.push_back(&component);
    for (const RocketComponent* const child : component.getChildren())
    {
        collectInTreeOrder(*child, components);
    }
}

[[nodiscard]] std::vector<const RocketComponent*> componentsInTreeOrder(const Rocket& rocket)
{
    std::vector<const RocketComponent*> components;
    collectInTreeOrder(rocket, components);
    return components;
}

/// The design panel of @p window (every design window has one).
[[nodiscard]] DesignPanel& designPanelOf(const MainWindow& window)
{
    DesignPanel* const panel = window.componentDesignPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The design view of @p window (every design window has one).
[[nodiscard]] RocketPanel& rocketPanelOf(const MainWindow& window)
{
    RocketPanel* const panel = window.componentRocketPanel();
    QTROCKET_ASSERT(panel != nullptr);
    return *panel;
}

/// The fixture: the windows of the application over DesignServices, the design of the test's
/// parameter opened through File > Open of a New design's window (open()).
class OpenDesignsExitGolden : public Gui::Test::GuiTest,
                              public ::testing::WithParamInterface<OpenDesign>
{
protected:
    void SetUp() override
    {
        GuiTest::SetUp();
        ComponentConfigDialog::forgetPreviousDialogs();
    }

    void TearDown() override
    {
        // The file operations and the dialog requests go before the windows, the windows (with
        // the documents and the dialog) before the services.
        m_fileOperations = nullptr;
        m_requests       = nullptr;
        m_windows        = nullptr;
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        m_services = nullptr;
        ComponentConfigDialog::forgetPreviousDialogs();
        GuiTest::TearDown();
    }

    /// Opens the design of the test through File > Open of a New design's window (the file
    /// chooser answered with the design's file) and returns the window it opened in; null (with
    /// a test failure) when none opened.
    MainWindow* open()
    {
        const OpenDesign& design = GetParam();
        m_services = std::make_unique<DesignServices>(design.source == DesignSource::TEST_ROCKET
                                                          ? QtRocket::Test::testRocketMotors()
                                                          : QtRocket::Test::bundledMotorDatabase());
        m_windows  = std::make_unique<WindowManager>(m_services->services(), m_clipboard,
                                                     WindowManager::LastWindowBehaviour::EXIT);
        m_requests = std::make_unique<ConfigDialogRequests>(*m_windows);
        m_fileOperations =
            std::make_unique<DocumentFileOperations>(*m_windows, m_services->services());
        const QPointer<MainWindow> start = &m_windows->newDesign();
        settle();
        prompts().answerOpenFiles(UserPrompts::OpenFileAnswer{
            .files = {design.file}, .directory = design.file.parent_path()});
        start->menus().openAction()->trigger();
        settle();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        // The design took the place of the New design, whose window is gone.
        if (m_windows->windowCount() != 1U || !start.isNull())
        {
            ADD_FAILURE() << m_windows->windowCount()
                          << " windows after File > Open, the New design's closed: "
                          << start.isNull();
            return nullptr;
        }
        MainWindow* const                          window = m_windows->windows().front();
        const std::optional<std::filesystem::path> file   = window->document().getFile();
        std::error_code                            error;
        if (!file.has_value() || !std::filesystem::equivalent(*file, design.file, error))
        {
            ADD_FAILURE() << "the window's design has another file";
            return nullptr;
        }
        return window;
    }

    [[nodiscard]] Gui::Test::ScriptedUserPrompts& prompts() const { return m_services->prompts(); }
    [[nodiscard]] const Preferences& preferences() const { return m_services->preferences(); }

private:
    std::unique_ptr<DesignServices>         m_services;
    OpenRocketClipboard                     m_clipboard;
    std::unique_ptr<WindowManager>          m_windows;
    std::unique_ptr<ConfigDialogRequests>   m_requests;
    std::unique_ptr<DocumentFileOperations> m_fileOperations;
};

/// How many of @p designs come from @p source.
[[nodiscard]] std::ptrdiff_t countOf(const std::vector<OpenDesign>& designs, DesignSource source)
{
    return std::ranges::count(designs, source, &OpenDesign::source);
}

/// The designs of @p designs whose file does not exist, by name.
[[nodiscard]] std::vector<std::string> missingFiles(const std::vector<OpenDesign>& designs)
{
    std::vector<std::string> missing;
    for (const OpenDesign& design : designs)
    {
        if (!std::filesystem::is_regular_file(design.file))
        {
            missing.push_back(design.name);
        }
    }
    return missing;
}

/// Whether one of @p designs is named @p name.
[[nodiscard]] bool isDesignName(const std::vector<OpenDesign>& designs, std::string_view name)
{
    return std::ranges::find(designs, name, &OpenDesign::name) != designs.end();
}

/// The designs kLoadWarnings and kOpeningChanges name that are none of @p designs.
[[nodiscard]] std::vector<std::string> unknownDesigns(const std::vector<OpenDesign>& designs)
{
    std::vector<std::string> unknown;
    for (const LoadWarnings& row : kLoadWarnings)
    {
        if (!isDesignName(designs, row.design))
        {
            unknown.emplace_back(row.design);
        }
    }
    for (const OpeningChanges& row : kOpeningChanges)
    {
        if (!isDesignName(designs, row.design))
        {
            unknown.emplace_back(row.design);
        }
    }
    return unknown;
}

// The suite opens the 66 designs of the plan: the 16 examples, the 49 re-saves of the manifest
// (16 examples, 13 test rockets, 18 legacy files, 2 coverage documents) and the GUI reference,
// each a file that exists; the tables of the expectations name none but these.
TEST(OpenDesignsExitGoldenDesigns, AreTheSixtySixOfThePlan)
{
    EXPECT_EQ(designList().problems, "");
    const std::vector<OpenDesign>& designs = openDesigns();
    EXPECT_EQ(designs.size(), 66U);
    EXPECT_EQ(countOf(designs, DesignSource::EXAMPLE), 16);
    EXPECT_EQ(countOf(designs, DesignSource::EXAMPLE_RESAVE), 16);
    EXPECT_EQ(countOf(designs, DesignSource::TEST_ROCKET), 13);
    EXPECT_EQ(countOf(designs, DesignSource::LEGACY), 18);
    EXPECT_EQ(countOf(designs, DesignSource::COVERAGE), 2);
    EXPECT_EQ(countOf(designs, DesignSource::GUI_REFERENCE), 1);
    EXPECT_EQ(missingFiles(designs), std::vector<std::string>{});
    EXPECT_EQ(unknownDesigns(designs), std::vector<std::string>{});
}

// ================================================================ the window and the tree

/// The rows of @p model below @p index, @p index's own first, each as describeRocket() begins
/// the line of its component: the indentation of its depth, its Java class and its name in quotes.
void collectRows(const ComponentTreeModel& model, const QModelIndex& index, int depth,
                 std::vector<std::string>& rows)
{
    const RocketComponent* const component = model.component(index);
    const std::string name = QtStrings::toUtf8(model.data(index, Qt::DisplayRole).toString());
    std::string       quoted;
    for (const char c : name)
    {
        quoted += c == '\n' ? std::string("\\n") : std::string(1, c);
    }
    rows.push_back(std::string(static_cast<std::size_t>(2 * depth), ' ') +
                   std::string(component != nullptr ? className(component->kind()) : "?") + " '" +
                   quoted + "'");
    for (int row = 0; row < model.rowCount(index); ++row)
    {
        collectRows(model, model.index(row, 0, index), depth + 1, rows);
    }
}

/// The lines of describeRocket() of @p rocket for its components, each up to the name.
[[nodiscard]] std::vector<std::string> describedComponents(const Rocket&      rocket,
                                                           const Preferences& preferences)
{
    const std::vector<std::string> lines = QtRocket::Test::describeRocket(rocket, preferences, {});
    const auto count = static_cast<std::size_t>(QtRocket::Test::componentCount(rocket));
    std::vector<std::string> components;
    for (std::size_t i = 0; i < count && i < lines.size(); ++i)
    {
        components.push_back(lines[i].substr(0, lines[i].find(" axial=")));
    }
    return components;
}

/// The components below @p index whose rows @p view does not show: a row hidden, or below a
/// collapsed one ("<name>").
[[nodiscard]] std::vector<std::string> unseenRows(const ComponentTreeView& view,
                                                  const QModelIndex&       index)
{
    std::vector<std::string>  unseen;
    const QAbstractItemModel& model = *view.model();
    for (int row = 0; row < model.rowCount(index); ++row)
    {
        const QModelIndex child = model.index(row, 0, index);
        if (view.isRowHidden(row, index) || !view.isExpanded(index))
        {
            unseen.push_back(QtStrings::toUtf8(child.data(Qt::DisplayRole).toString()));
        }
        const std::vector<std::string> below = unseenRows(view, child);
        unseen.insert(unseen.end(), below.begin(), below.end());
    }
    return unseen;
}

/// The rows below @p index of @p model that have no icon: every component's row has its
/// component's icon, the Rocket's none (ComponentTreeModel).
[[nodiscard]] std::vector<std::string> rowsWithoutIcon(const ComponentTreeModel& model,
                                                       const QModelIndex&        index)
{
    std::vector<std::string> without;
    for (int row = 0; row < model.rowCount(index); ++row)
    {
        const QModelIndex child = model.index(row, 0, index);
        if (model.data(child, Qt::DecorationRole).value<QIcon>().isNull())
        {
            without.push_back(QtStrings::toUtf8(model.data(child, Qt::DisplayRole).toString()));
        }
        const std::vector<std::string> below = rowsWithoutIcon(model, child);
        without.insert(without.end(), below.begin(), below.end());
    }
    return without;
}

/// Expects @p message to be BasicFrame's WarningDialog after the load of @p fileName: "Warnings
/// while opening file" with its two lines, then the warnings.
void expectLoadWarningsMessage(const Gui::Test::ScriptedUserPrompts::Prompt& message,
                               const QString&                                fileName)
{
    EXPECT_EQ(message.kind, PromptKind::WARNINGS);
    EXPECT_EQ(message.title, QStringLiteral("Warnings while opening file"));
    EXPECT_GE(message.texts.size(), 3);
    EXPECT_EQ(message.texts.value(0),
              QStringLiteral("The following problems were encountered while opening ") + fileName +
                  QLatin1Char('.'));
    EXPECT_EQ(message.texts.value(1),
              QStringLiteral("Some design features may not have been loaded correctly."));
}

/// The warnings of the load shown among @p asked, the questions and messages after File > Open's
/// file question: the message "Warnings while opening file" (BasicFrame's WarningDialog) with
/// its two lines about @p fileName, then the warnings.
[[nodiscard]] std::vector<std::string> shownLoadWarnings(
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& asked, const QString& fileName)
{
    std::vector<std::string> shown;
    for (std::size_t i = 1; i < asked.size(); ++i)
    {
        const Gui::Test::ScriptedUserPrompts::Prompt& message = asked[i];
        expectLoadWarningsMessage(message, fileName);
        for (qsizetype line = 2; line < message.texts.size(); ++line)
        {
            shown.push_back(QtStrings::toUtf8(message.texts.at(line)));
        }
    }
    return shown;
}

TEST_P(OpenDesignsExitGolden, OpensWithItsTitleTreeAndLoadWarnings)
{
    const OpenDesign&       design = GetParam();
    const MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    const OpenRocketDocument& document = window->document();
    const Rocket&             rocket   = document.getRocket();

    // File > Open asked for the file, then showed the load's warnings, if any (BasicFrame).
    const std::vector<Gui::Test::ScriptedUserPrompts::Prompt>& asked = prompts().prompts();
    ASSERT_FALSE(asked.empty());
    EXPECT_EQ(asked.front().kind, PromptKind::OPEN_FILES);
    EXPECT_LE(asked.size(), 2U);
    EXPECT_EQ(shownLoadWarnings(asked, QtStrings::fromPath(design.file.filename())),
              expectedLoadWarnings(design));

    // The title: the rocket's name and the file's, saved.
    EXPECT_EQ(window->windowTitle(),
              QStringLiteral("[*]") + QtStrings::fromUtf8(rocket.getName()) + QStringLiteral(" (") +
                  QtStrings::fromPath(design.file.filename()) + QLatin1Char(')'));
    EXPECT_FALSE(window->isWindowModified());
    EXPECT_TRUE(document.isSaved());
    EXPECT_FALSE(document.isUndoAvailable());

    // The component tree: a row for every component, in describeRocket()'s order, every one in
    // view (the tree is expanded when it is made: ComponentTree's expandTree()).
    const DesignPanel&             panel = designPanelOf(*window);
    ComponentTreeModel&            model = panel.treeModel();
    const QAbstractItemModelTester tester(&model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    std::vector<std::string>       rows;
    collectRows(model, model.rocketIndex(), 0, rows);
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(rows, describedComponents(rocket, preferences()));
    EXPECT_EQ(rows.size(), static_cast<std::size_t>(QtRocket::Test::componentCount(rocket)));
    EXPECT_EQ(panel.tree().model(), &model);
    EXPECT_EQ(unseenRows(panel.tree(), model.rocketIndex()), std::vector<std::string>{});
    EXPECT_TRUE(model.data(model.rocketIndex(), Qt::DecorationRole).value<QIcon>().isNull());
    EXPECT_EQ(rowsWithoutIcon(model, model.rocketIndex()), std::vector<std::string>{});
}

// ======================================================================== the dialogs

/// A main field of a configuration dialog, a spin box named after its Java property, and what
/// it shows: a value of the component and its place in a component of geometry.json.
struct MainField
{
    std::string_view property;
    /// The component's value, or none when the component has no such value (a test failure: the
    /// field shows something unknown).
    std::optional<double> (*value)(const RocketComponent& component);
    /// The JSON pointer of the value in the component's golden entry.
    std::string_view (*golden)(const RocketComponent& component);
    /// The field's multiplier (Java's DoubleModel: 2 for a radius shown as a diameter) and its
    /// units.
    double      multiplier{1.0};
    UnitGroupId units{UnitGroupId::LENGTH};
    /// Whether the value is a position (compared within kGoldenAbsolute; else relatively).
    bool position{false};
};

template <class T>
[[nodiscard]] const T* as(const RocketComponent& component)
{
    return dynamic_cast<const T*>(&component);
}

[[nodiscard]] std::optional<double> lengthOf(const RocketComponent& component)
{
    return component.getLength();
}

[[nodiscard]] std::optional<double> outerRadiusOf(const RocketComponent& component)
{
    const auto* const coaxial = as<Coaxial>(component);
    return coaxial != nullptr ? std::optional(coaxial->getOuterRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> innerRadiusOf(const RocketComponent& component)
{
    const auto* const coaxial = as<Coaxial>(component);
    return coaxial != nullptr ? std::optional(coaxial->getInnerRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> foreRadiusOf(const RocketComponent& component)
{
    const auto* const body = as<SymmetricComponent>(component);
    return body != nullptr ? std::optional(body->getForeRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> aftRadiusOf(const RocketComponent& component)
{
    const auto* const body = as<SymmetricComponent>(component);
    return body != nullptr ? std::optional(body->getAftRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> baseRadiusOf(const RocketComponent& component)
{
    const auto* const nose = as<NoseCone>(component);
    return nose != nullptr ? std::optional(nose->getBaseRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> massObjectRadiusOf(const RocketComponent& component)
{
    const auto* const object = as<MassObject>(component);
    return object != nullptr ? std::optional(object->getRadius()) : std::nullopt;
}

[[nodiscard]] std::optional<double> finHeightOf(const RocketComponent& component)
{
    if (const auto* const trapezoid = as<TrapezoidFinSet>(component))
    {
        return trapezoid->getHeight();
    }
    const auto* const elliptical = as<EllipticalFinSet>(component);
    return elliptical != nullptr ? std::optional(elliptical->getHeight()) : std::nullopt;
}

[[nodiscard]] std::optional<double> rootChordOf(const RocketComponent& component)
{
    const auto* const fins = as<TrapezoidFinSet>(component);
    return fins != nullptr ? std::optional(fins->getRootChord()) : std::nullopt;
}

[[nodiscard]] std::optional<double> tipChordOf(const RocketComponent& component)
{
    const auto* const fins = as<TrapezoidFinSet>(component);
    return fins != nullptr ? std::optional(fins->getTipChord()) : std::nullopt;
}

[[nodiscard]] std::optional<double> canopyDiameterOf(const RocketComponent& component)
{
    const auto* const chute = as<Parachute>(component);
    return chute != nullptr ? std::optional(chute->getDiameter()) : std::nullopt;
}

[[nodiscard]] std::optional<double> massComponentMassOf(const RocketComponent& component)
{
    const auto* const mass = as<MassComponent>(component);
    return mass != nullptr ? std::optional(mass->getComponentMass()) : std::nullopt;
}

[[nodiscard]] std::optional<double> overrideMassOf(const RocketComponent& component)
{
    return component.getOverrideMass();
}

[[nodiscard]] std::optional<double> overrideCgxOf(const RocketComponent& component)
{
    return component.getOverrideCGX();
}

/// The golden pointers: fixed ones, and the base of a nose cone (its fore end when flipped).
[[nodiscard]] std::string_view goldenLength(const RocketComponent& /*component*/)
{
    return "/length";
}
[[nodiscard]] std::string_view goldenOuterRadius(const RocketComponent& /*component*/)
{
    return "/details/outerRadius";
}
[[nodiscard]] std::string_view goldenInnerRadius(const RocketComponent& /*component*/)
{
    return "/details/innerRadius";
}
[[nodiscard]] std::string_view goldenForeRadius(const RocketComponent& /*component*/)
{
    return "/details/foreRadius";
}
[[nodiscard]] std::string_view goldenAftRadius(const RocketComponent& /*component*/)
{
    return "/details/aftRadius";
}
[[nodiscard]] std::string_view goldenBaseRadius(const RocketComponent& component)
{
    const auto* const nose = as<NoseCone>(component);
    return nose != nullptr && nose->isFlipped() ? "/details/foreRadius" : "/details/aftRadius";
}
[[nodiscard]] std::string_view goldenRadius(const RocketComponent& /*component*/)
{
    return "/details/radius";
}
[[nodiscard]] std::string_view goldenHeight(const RocketComponent& /*component*/)
{
    return "/details/height";
}
[[nodiscard]] std::string_view goldenRootChord(const RocketComponent& /*component*/)
{
    return "/details/rootChord";
}
[[nodiscard]] std::string_view goldenTipChord(const RocketComponent& /*component*/)
{
    return "/details/tipChord";
}
[[nodiscard]] std::string_view goldenDiameter(const RocketComponent& /*component*/)
{
    return "/details/diameter";
}
[[nodiscard]] std::string_view goldenComponentMass(const RocketComponent& /*component*/)
{
    return "/componentMass";
}
[[nodiscard]] std::string_view goldenOverrideMass(const RocketComponent& /*component*/)
{
    return "/overrides/overrideMass";
}
[[nodiscard]] std::string_view goldenOverrideCgx(const RocketComponent& /*component*/)
{
    return "/overrides/overrideCGX";
}

/// The main fields the suite looks at in every dialog that has them.
const std::array<MainField, 14> kMainFields{{
    {.property = "Length", .value = lengthOf, .golden = goldenLength},
    {.property   = "OuterRadius",
     .value      = outerRadiusOf,
     .golden     = goldenOuterRadius,
     .multiplier = 2.0},
    {.property   = "InnerRadius",
     .value      = innerRadiusOf,
     .golden     = goldenInnerRadius,
     .multiplier = 2.0},
    {.property   = "ForeRadius",
     .value      = foreRadiusOf,
     .golden     = goldenForeRadius,
     .multiplier = 2.0},
    {.property = "AftRadius", .value = aftRadiusOf, .golden = goldenAftRadius, .multiplier = 2.0},
    {.property   = "BaseRadius",
     .value      = baseRadiusOf,
     .golden     = goldenBaseRadius,
     .multiplier = 2.0},
    {.property = "Radius", .value = massObjectRadiusOf, .golden = goldenRadius, .multiplier = 2.0},
    {.property = "Height", .value = finHeightOf, .golden = goldenHeight},
    {.property = "RootChord", .value = rootChordOf, .golden = goldenRootChord},
    {.property = "TipChord", .value = tipChordOf, .golden = goldenTipChord},
    {.property = "Diameter", .value = canopyDiameterOf, .golden = goldenDiameter},
    {.property = "ComponentMass",
     .value    = massComponentMassOf,
     .golden   = goldenComponentMass,
     .units    = UnitGroupId::MASS},
    {.property = "OverrideMass",
     .value    = overrideMassOf,
     .golden   = goldenOverrideMass,
     .units    = UnitGroupId::MASS},
    {.property = "OverrideCGX",
     .value    = overrideCgxOf,
     .golden   = goldenOverrideCgx,
     .position = true},
}};

/// What the main fields of the dialogs showed, counted.
struct FieldCounts
{
    int fields{0};         ///< the spin boxes compared with the component
    int goldenValues{0};   ///< and with OpenRocket's value
    int overrideFlags{0};  ///< the override check boxes compared
};

/// Compares @p shown, the value the field @p field shows of @p component (without its
/// multiplier), with the component's value in @p golden, its entry of geometry.json, when that has
/// one (not null).
void expectGoldenValue(double shown, const MainField& field, const RocketComponent& component,
                       const nlohmann::json& golden, FieldCounts& counts)
{
    const nlohmann::json::json_pointer pointer{std::string(field.golden(component))};
    if (!golden.contains(pointer) || golden.at(pointer).is_null())
    {
        return;
    }
    const double expected  = golden.at(pointer).get<double>();
    const double tolerance = field.position
                                 ? kGoldenAbsolute
                                 : kGoldenRelative * std::max(std::abs(expected), std::abs(shown));
    EXPECT_NEAR(shown, expected, tolerance) << "OpenRocket's value";
    ++counts.goldenValues;
}

/// Compares the spin box @p spin of the field @p field with the value of @p component, and with
/// its value in @p golden (its entry of geometry.json) when there is one: the value times the
/// field's multiplier, written in the default unit of the field's units.
void expectField(const UnitSpinBox& spin, const MainField& field, const RocketComponent& component,
                 const nlohmann::json* golden, FieldCounts& counts)
{
    SCOPED_TRACE(field.property);
    const std::shared_ptr<DoubleBinding>& binding = spin.binding();
    ASSERT_NE(binding, nullptr);
    const std::optional<double> value = field.value(component);
    ASSERT_TRUE(value.has_value()) << "no value for the field";
    EXPECT_EQ(binding->getMultiplier(), field.multiplier);
    const Unit&  unit  = unitGroup(field.units).getDefaultUnit();
    const double shown = value.value_or(0.0) * field.multiplier;
    EXPECT_EQ(binding->value(), shown);
    EXPECT_EQ(binding->getCurrentUnit(), unit);
    EXPECT_EQ(spin.text(), QtStrings::fromUtf8(unit.toString(shown)));
    ++counts.fields;
    if (golden != nullptr)
    {
        expectGoldenValue(binding->value() / field.multiplier, field, component, *golden, counts);
    }
}

/// Compares the override check box named @p property of @p dialog with @p overridden.
void expectOverrideFlag(const ComponentConfigDialog& dialog, const QString& property,
                        bool overridden, FieldCounts& counts)
{
    for (const BoolCheckBox* const box : dialog.findChildren<BoolCheckBox*>(property))
    {
        EXPECT_EQ(box->isChecked(), overridden) << QtStrings::toUtf8(property);
        ++counts.overrideFlags;
    }
}

/// Compares the main fields of @p dialog with @p component and its golden entry @p golden (null
/// for none).
void expectMainFields(const ComponentConfigDialog& dialog, const RocketComponent& component,
                      const nlohmann::json* golden, FieldCounts& counts)
{
    for (const MainField& field : kMainFields)
    {
        for (const UnitSpinBox* const spin :
             dialog.findChildren<UnitSpinBox*>(QtStrings::fromUtf8(field.property)))
        {
            expectField(*spin, field, component, golden, counts);
        }
    }
    expectOverrideFlag(dialog, QStringLiteral("MassOverridden"), component.isMassOverridden(),
                       counts);
    expectOverrideFlag(dialog, QStringLiteral("CGOverridden"), component.isCGOverridden(), counts);
}

/// What undo and redo would do: the history, its position, and the Edit menu's two actions.
struct UndoState
{
    std::size_t                             position{0};
    std::vector<std::optional<std::string>> descriptions;
    bool                                    clean{true};
    QString                                 undoText;
    bool                                    undoEnabled{false};
    QString                                 redoText;
    bool                                    redoEnabled{false};

    [[nodiscard]] bool operator==(const UndoState&) const = default;
};

/// @p state for a test's message.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const UndoState& state, std::ostream* out)
{
    *out << std::format("position {} of {}, clean {}, '{}' {}, '{}' {}", state.position,
                        state.descriptions.size(), state.clean, QtStrings::toUtf8(state.undoText),
                        state.undoEnabled, QtStrings::toUtf8(state.redoText), state.redoEnabled);
}

/// The undo state of @p window's design. (The description the next change gets is left out: the
/// dialog's undo position sets it, and a Cancel leaves it, as in OpenRocket, for no state of the
/// history.)
[[nodiscard]] UndoState undoStateOf(const MainWindow& window)
{
    const OpenRocketDocument::UndoDetail detail = window.document().getUndoDetail();
    const QAction&                       undo   = *window.undoRedoActions().undoAction();
    const QAction&                       redo   = *window.undoRedoActions().redoAction();
    return {.position     = detail.position,
            .descriptions = detail.descriptions,
            .clean        = detail.clean,
            .undoText     = undo.text(),
            .undoEnabled  = undo.isEnabled(),
            .redoText     = redo.text(),
            .redoEnabled  = redo.isEnabled()};
}

/// The golden entries of the components of a design with goldens, in tree order; empty for one
/// without.
[[nodiscard]] std::vector<nlohmann::json> goldenComponents(const OpenDesign& design)
{
    if (design.goldenInput.empty())
    {
        return {};
    }
    const Result<DesignGoldens> goldens = QtRocket::Test::loadDesignGoldens(design.goldenInput);
    if (!goldens.has_value())
    {
        ADD_FAILURE() << goldens.error().toString();
        return {};
    }
    return goldens->geometry.at("components").get<std::vector<nlohmann::json>>();
}

/// What the dialogs are to leave alone: whether the design is saved (and the window's title
/// says so), its undo state, and the questions asked so far.
struct DesignState
{
    bool        saved{true};
    bool        windowModified{false};
    UndoState   undo;
    std::size_t questions{0};

    [[nodiscard]] bool operator==(const DesignState&) const = default;
};

/// @p state for a test's message.
// NOLINTNEXTLINE(readability-identifier-naming): the name GoogleTest looks for
void PrintTo(const DesignState& state, std::ostream* out)
{
    *out << std::format("saved {}, modified {}, {} questions, ", state.saved, state.windowModified,
                        state.questions);
    PrintTo(state.undo, out);
}

[[nodiscard]] DesignState stateOf(const MainWindow&                     window,
                                  const Gui::Test::ScriptedUserPrompts& prompts)
{
    return {.saved          = window.document().isSaved(),
            .windowModified = window.isWindowModified(),
            .undo           = undoStateOf(window),
            .questions      = prompts.prompts().size()};
}

/// What the dialogs of a design showed, and the components whose dialog changed the design when
/// it opened.
struct DialogRun
{
    FieldCounts              counts;
    std::vector<std::string> changedOnOpening;
};

/// Expects @p dialog to show @p component alone: its title, the selection, the name and the main
/// fields (with the component's golden entry @p golden, null for none).
void expectDialogOf(const ComponentConfigDialog& dialog, const RocketComponent& component,
                    const nlohmann::json* golden, FieldCounts& counts)
{
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(dialog.windowTitle(), QtStrings::fromUtf8(displayName(component.kind())) +
                                        QStringLiteral(" configuration"));
    const RocketComponentConfig& configurator = dialog.configurator();
    EXPECT_EQ(&configurator.selection()->getMain(), &component);
    EXPECT_TRUE(configurator.selection()->getOthers().empty());
    EXPECT_EQ(configurator.nameField().text(), QtStrings::fromUtf8(component.getName()));
    expectMainFields(dialog, component, golden, counts);
}

/// Opens the dialog of @p component in @p window as the user does (the component selected in
/// the tree, Edit), compares what it shows (expectDialogOf()) and cancels it, which must leave
/// the design as the dialog found it. Counted into @p run.
void showAndCancel(const MainWindow& window, const RocketComponent& component,
                   const nlohmann::json* golden, const Gui::Test::ScriptedUserPrompts& prompts,
                   DialogRun& run)
{
    const DesignState before = stateOf(window, prompts);
    designPanelOf(window).selection().setSelectedComponent(component);
    window.menus().editAction()->trigger();
    const ComponentConfigDialog* const dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    expectDialogOf(*dialog, component, golden, run.counts);
    const DesignState shown = stateOf(window, prompts);
    if (shown != before)
    {
        run.changedOnOpening.push_back(component.getName());
    }
    // The Cancel of a dialog in which nothing was changed changes nothing and asks nothing (the
    // user's decision U4: it no longer undoes the step before the dialog).
    dialog->configurator().cancelButton().click();
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
    EXPECT_EQ(stateOf(window, prompts), shown);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// showAndCancel() of every component of the design of @p window, in the order of its tree,
/// with its entry of @p golden (geometry.json's components, in the same order; empty for a
/// design without goldens).
[[nodiscard]] DialogRun showEveryDialog(const MainWindow&                     window,
                                        const std::vector<nlohmann::json>&    golden,
                                        const Gui::Test::ScriptedUserPrompts& prompts)
{
    // The components by id: a dialog may change what a component holds (an automatic value),
    // never which components there are.
    const Rocket&                             rocket     = window.document().getRocket();
    const std::vector<const RocketComponent*> components = componentsInTreeOrder(rocket);
    std::vector<Uuid>                         ids;
    ids.reserve(components.size());
    std::ranges::transform(components, std::back_inserter(ids),
                           [](const RocketComponent* component) { return component->getId(); });
    DialogRun run;
    if (!golden.empty() && golden.size() != ids.size())
    {
        ADD_FAILURE() << ids.size() << " components, " << golden.size() << " in geometry.json";
        return run;
    }
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        const RocketComponent* const component = rocket.findComponent(ids[i]);
        if (component == nullptr)
        {
            ADD_FAILURE() << "a dialog removed the component " << i;
            return run;
        }
        SCOPED_TRACE(std::format("component {}: {}", i, component->getName()));
        const nlohmann::json* const entry = golden.empty() ? nullptr : &golden[i];
        if (entry != nullptr)
        {
            EXPECT_EQ(entry->at("name").get<std::string>(), component->getName());
        }
        showAndCancel(window, *component, entry, prompts, run);
    }
    return run;
}

TEST_P(OpenDesignsExitGolden, ShowsEveryComponentInItsDialogAndCancels)
{
    const OpenDesign&       design = GetParam();
    const MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    ASSERT_TRUE(window->document().isSaved());
    const std::vector<nlohmann::json> golden = goldenComponents(design);
    const DialogRun                   run    = showEveryDialog(*window, golden, prompts());
    EXPECT_EQ(run.changedOnOpening, expectedOpeningChanges(design));
    EXPECT_EQ(window->document().isSaved(), run.changedOnOpening.empty());
    EXPECT_GT(run.counts.fields, 0);
    EXPECT_TRUE(golden.empty() || run.counts.goldenValues > 0);
}

/// Gives the design of @p window a step of history through its GUI: the rocket's dialog (File >
/// Properties), @p text typed at the end of its comment, OK. The step is stored by the next undo
/// position (OpenRocketDocument::addUndoPosition() after a change): the rocket's dialog is opened
/// again for that and cancelled, which keeps the step (nothing changed in that dialog).
void appendToTheRocketsComment(const MainWindow& window, const QString& text)
{
    window.menus().propertiesAction()->trigger();
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    QPlainTextEdit& comment = dialog->configurator().commentArea();
    comment.moveCursor(QTextCursor::End);
    QTest::keyClicks(&comment, text);
    dialog->configurator().okButton().click();
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    window.menus().propertiesAction()->trigger();
    dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    dialog->configurator().cancelButton().click();
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// Expects the history of @p window's design to be @p edited after the dialogs of @p run, unless
/// a dialog changed the design when it opened (kOpeningChanges: that change is history too).
void expectHistoryKept(const MainWindow& window, const UndoState& edited, const DialogRun& run)
{
    if (run.changedOnOpening.empty())
    {
        EXPECT_EQ(undoStateOf(window), edited);
    }
}

TEST_P(OpenDesignsExitGolden, EveryCancelKeepsTheEditBeforeIt)
{
    // A Cancel takes back what its own dialog changed, never an earlier step of the history (the
    // user's decision U4): with the rocket's comment edited first, through its dialog, every
    // dialog is opened and cancelled, and the edit stays in the design and in the history.
    const OpenDesign&       design = GetParam();
    const MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    const std::string comment = window->document().getRocket().getComment() + "Checked.";
    appendToTheRocketsComment(*window, QStringLiteral("Checked."));
    ASSERT_EQ(window->document().getRocket().getComment(), comment);
    const UndoState edited = undoStateOf(*window);
    ASSERT_TRUE(edited.undoEnabled);
    const DialogRun run = showEveryDialog(*window, goldenComponents(design), prompts());
    EXPECT_EQ(run.changedOnOpening, expectedOpeningChanges(design));
    EXPECT_EQ(window->document().getRocket().getComment(), comment);
    EXPECT_FALSE(window->document().isSaved());
    expectHistoryKept(*window, edited, run);
}

// ========================================================================= the figure

/// Whether @p inner lies in @p outer, within @p tolerance on every side.
[[nodiscard]] bool contains(const Rectangle2D& outer, const Rectangle2D& inner, double tolerance)
{
    return inner.x >= outer.x - tolerance && inner.y >= outer.y - tolerance &&
           inner.x + inner.width <= outer.x + outer.width + tolerance &&
           inner.y + inner.height <= outer.y + outer.height + tolerance;
}

/// @p bounds for a test's message.
[[nodiscard]] std::string boundsText(const Rectangle2D& bounds)
{
    return std::format("({}, {}) {} x {}", bounds.x, bounds.y, bounds.width, bounds.height);
}

/// The centre of @p bounds as a rectangle without extent.
[[nodiscard]] Rectangle2D centreOf(const Rectangle2D& bounds)
{
    return {.x      = bounds.x + (bounds.width / 2.0),
            .y      = bounds.y + (bounds.height / 2.0),
            .width  = 0.0,
            .height = 0.0};
}

/// The union of @p a and @p b.
[[nodiscard]] Rectangle2D unionOf(const Rectangle2D& a, const Rectangle2D& b)
{
    const double left   = std::min(a.x, b.x);
    const double bottom = std::min(a.y, b.y);
    return {.x      = left,
            .y      = bottom,
            .width  = std::max(a.x + a.width, b.x + b.width) - left,
            .height = std::max(a.y + a.height, b.y + b.height) - bottom};
}

/// What of @p geometry lies outside @p subject, each as "<component>: <bounds>". Every shape is
/// to lie in it, but for three that OpenRocket draws past its subject too (the subject is the
/// configuration's bounding box of its components, FlightConfiguration.calculateBounds(), and the
/// subject bounds and shapes of OpenRocket's figure.json are QtRocket's): a motor, which is no
/// component, stands out behind its mount (its front is to lie in the subject; in the back view
/// all of it); the marker of a pod set or a booster set (ComponentAssemblyShapes) sits at the
/// assembly's reference point, at its aft end for a bottom placement, with a radius of its own
/// (the point is to lie in the subject); and the tubes of a tube fin set reach a little past the
/// subject's radius (by 0.04 mm in the tube fin rocket's figure.json; each tube is to lie in the
/// subject but for kTubeFinOvershoot).
[[nodiscard]] std::vector<std::string> outsideOf(const RocketFigureGeometry& geometry,
                                                 const Rectangle2D&          subject)
{
    // How far a tube of a tube fin set may stand out of the subject: just above OpenRocket's 0.04
    // mm.
    constexpr double kTubeFinOvershoot = 1e-4;
    // The shapes are made in single precision, as OpenRocket's (Path2D.Float, a float
    // Rectangle2D), the subject in double: an edge may stand out by the rounding of a float.
    const double tolerance =
        4.0 * static_cast<double>(std::numeric_limits<float>::epsilon()) *
        std::max({1.0, std::abs(subject.x), std::abs(subject.x + subject.width),
                  std::abs(subject.y), std::abs(subject.y + subject.height)});
    std::vector<std::string> outside;
    // The markers, by component and instance, in the order they are drawn.
    using MarkerKey = std::tuple<const RocketComponent*, int, bool>;
    std::vector<std::pair<MarkerKey, Rectangle2D>> markers;
    for (const RocketFigureGeometry::InstanceShape& shape : geometry.shapes())
    {
        const RocketComponent& component = shape.shape.component();
        const Rectangle2D      bounds    = shape.shape.shape().bounds();
        const ComponentKind    kind      = component.kind();
        if (kind == ComponentKind::POD_SET || kind == ComponentKind::PARALLEL_STAGE)
        {
            const MarkerKey key(&component, shape.contextIndex, shape.extraRender);
            const auto      found =
                std::ranges::find(markers, key, &std::pair<MarkerKey, Rectangle2D>::first);
            if (found == markers.end())
            {
                markers.emplace_back(key, bounds);
            }
            else
            {
                found->second = unionOf(found->second, bounds);
            }
            continue;
        }
        const double allowed = kind == ComponentKind::TUBE_FIN_SET
                                   ? std::max(tolerance, kTubeFinOvershoot)
                                   : tolerance;
        if (!contains(subject, bounds, allowed))
        {
            outside.push_back(component.getName() + ": " + boundsText(bounds));
        }
    }
    for (const auto& [key, bounds] : markers)
    {
        if (!contains(subject, centreOf(bounds), tolerance))
        {
            outside.push_back("marker of " + std::get<0>(key)->getName() + ": " +
                              boundsText(bounds));
        }
    }
    for (const RocketFigureGeometry::MotorShape& motor : geometry.motors())
    {
        Rectangle2D front = motor.shape.bounds();
        if (geometry.view() != FigureView::BACK)
        {
            front.width = 0.0;
        }
        if (!contains(subject, front, tolerance))
        {
            outside.push_back("motor of " + motor.mount->getName() + ": " + boundsText(front));
        }
    }
    return outside;
}

/// Chooses the item @p item of the view selector of @p panel and expects the figure to show its
/// view: painted into @p image (a Qt warning on the way fails the test: GuiTestMain.cpp), with
/// shapes, and every shape in the subject bounds (outsideOf()).
void expectView(RocketPanel& panel, int item, QImage& image)
{
    Gui::Test::chooseItem(panel.viewSelector(), item);
    settle();
    const FigureView view = RocketPanel::kViewTypes.at(static_cast<std::size_t>(item));
    SCOPED_TRACE(std::format("view {}", item));
    EXPECT_EQ(panel.viewType(), view);
    {
        QPainter painter(&image);
        panel.render(&painter);
    }
    RocketFigure&               figure   = panel.figure();
    const RocketFigureGeometry& geometry = figure.figureGeometry();
    EXPECT_EQ(geometry.view(), view);
    EXPECT_FALSE(geometry.shapes().empty());
    EXPECT_EQ(figure.subjectBounds(), geometry.subjectBounds());
    EXPECT_EQ(outsideOf(geometry, figure.subjectBounds()), std::vector<std::string>{})
        << "subject " << boundsText(figure.subjectBounds());
}

TEST_P(OpenDesignsExitGolden, PaintsEveryViewAroundEveryComponent)
{
    const MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    RocketPanel& panel = rocketPanelOf(*window);
    ASSERT_EQ(panel.viewSelector().count(), static_cast<int>(RocketPanel::kViewTypes.size()));
    QImage image(panel.size(), QImage::Format_ARGB32_Premultiplied);
    for (int item = 0; item < panel.viewSelector().count(); ++item)
    {
        expectView(panel, item, image);
    }
}

// ======================================================================= the overlay

/// What the overlay of a configuration is to show: its CG and CP as the overlay shows them (0
/// without mass or lift: RocketInfoContextHelper.applyCgAndCp()).
struct OverlayNumbers
{
    double cg{0.0};
    double cp{0.0};
};

/// The overlay's CG of a CG @p x with the mass @p mass, and its CP of a CP @p x with the CNa
/// @p cna (DesignPhysics::applyCgAndCp()).
[[nodiscard]] double shownCg(double x, double mass)
{
    return mass > MassCalculator::kMinMass ? x : 0.0;
}
[[nodiscard]] double shownCp(double x, double cna)
{
    return cna > MathUtil::kEpsilon ? x : 0.0;
}

/// OpenRocket's numbers of the configuration of index @p index of the goldens: the launch CG of
/// mass.json and the worst CP at @p mach of aero.json; none (a test failure) when the goldens
/// lack them.
[[nodiscard]] std::optional<OverlayNumbers> goldenNumbers(const DesignGoldens& goldens, int index,
                                                          double mach)
{
    const nlohmann::json* launch = nullptr;
    for (const nlohmann::json& configuration : goldens.mass.at("configurations"))
    {
        if (configuration.at("index").get<int>() == index)
        {
            launch = &configuration.at("launch");
        }
    }
    const nlohmann::json* const results =
        QtRocket::Test::aeroResults(goldens.aero, static_cast<std::size_t>(index));
    if (launch == nullptr || results == nullptr)
    {
        ADD_FAILURE() << "no golden configuration " << index;
        return std::nullopt;
    }
    for (const nlohmann::json& worst : results->at("worstCP"))
    {
        if (worst.at("mach").get<double>() == mach)
        {
            const nlohmann::json& cp = worst.at("cp");
            return OverlayNumbers{.cg = shownCg(launch->at("cm").at(0).get<double>(),
                                                launch->at("mass").get<double>()),
                                  .cp = shownCp(cp.at(0).get<double>(), cp.at(3).get<double>())};
        }
    }
    ADD_FAILURE() << "no golden worst CP at Mach " << mach;
    return std::nullopt;
}

/// The numbers 'info' prints of the configuration of index @p index of @p rocket at @p mach
/// ("CG: 0.237 m", "worst CP at Mach 0.3: 0.259 m"); none (a test failure) when it prints none.
[[nodiscard]] std::optional<OverlayNumbers> printedNumbers(Rocket& rocket, int index, double mach,
                                                           const Preferences& preferences)
{
    CliOutput output;
    InfoCommand::printConfiguration(rocket, index, mach, preferences, output);
    std::map<std::string, std::string, std::less<>> printed;
    std::string_view                                text = output.out();
    while (!text.empty())
    {
        const std::size_t      end  = text.find('\n');
        const std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        if (const std::size_t colon = line.find(": "); colon != std::string_view::npos)
        {
            printed.emplace(line.substr(0, colon), line.substr(colon + 2));
        }
    }
    const auto number = [&printed](std::string_view label) {
        const auto found = printed.find(std::format("  {}", label));
        if (found == printed.end())
        {
            return std::numeric_limits<double>::quiet_NaN();
        }
        // "0.237 m", or "none (no mass)": shown as 0.
        return Strings::parseDouble(
                   std::string_view(found->second).substr(0, found->second.find(' ')))
            .value_or(0.0);
    };
    const double cg = number("CG");
    const double cp = number(std::format("worst CP at Mach {}", mach));
    if (std::isnan(cg) || std::isnan(cp))
    {
        ADD_FAILURE() << "'info' printed no CG or CP:\n" << output.out();
        return std::nullopt;
    }
    return OverlayNumbers{.cg = cg, .cp = cp};
}

/// The number "<name>=<value>" of state.json's loaded numbers of the GUI reference (OpenRocket's
/// load of the saved file); NaN (a test failure) when there is none.
[[nodiscard]] double guiReferenceNumber(std::string_view name)
{
    const Result<std::string> text =
        readTextFile(QtRocket::Test::guiFromScratchDir() / "state.json");
    if (!text.has_value())
    {
        ADD_FAILURE() << text.error().toString();
        return std::numeric_limits<double>::quiet_NaN();
    }
    const Result<QtRocket::Test::GuiReference> reference = QtRocket::Test::parseGuiReference(*text);
    if (!reference.has_value())
    {
        ADD_FAILURE() << reference.error().toString();
        return std::numeric_limits<double>::quiet_NaN();
    }
    const std::string prefix = std::string(name) + "=";
    for (const std::string& number : reference->loadedNumbers)
    {
        if (number.starts_with(prefix))
        {
            const std::optional<double> value =
                Strings::parseDouble(std::string_view(number).substr(prefix.size()));
            return value.value_or(std::numeric_limits<double>::quiet_NaN());
        }
    }
    ADD_FAILURE() << "no " << name << " in state.json";
    return std::numeric_limits<double>::quiet_NaN();
}

/// The numbers the overlay is to show for the configuration of index @p index of @p rocket: the
/// goldens' for a design with goldens, the GUI reference's CG with the CP 'info' prints, and
/// 'info''s for the others.
[[nodiscard]] std::optional<OverlayNumbers> expectedNumbers(const OpenDesign&    design,
                                                            const DesignGoldens* goldens,
                                                            Rocket& rocket, int index,
                                                            const Preferences& preferences)
{
    const double mach = preferences.getDefaultMach();
    if (goldens != nullptr)
    {
        return goldenNumbers(*goldens, index, mach);
    }
    std::optional<OverlayNumbers> numbers = printedNumbers(rocket, index, mach, preferences);
    if (design.source == DesignSource::GUI_REFERENCE && numbers.has_value())
    {
        // state.json: the saved design as OpenRocket loads it, which has no motor (its launch CG
        // is the structure's).
        numbers->cg =
            shownCg(guiReferenceNumber("structure.cm.x"), guiReferenceNumber("structure.mass"));
    }
    return numbers;
}

/// The texts @p figure paints (its overlay's: the rocket draws none).
[[nodiscard]] std::vector<QString> paintedTexts(RocketFigure& figure)
{
    Gui::Test::RecordingPaintDevice device(figure.size());
    figure.render(&device, QPoint(), QRegion(figure.rect()), QWidget::RenderFlags());
    std::vector<QString> texts;
    for (const Gui::Test::RecordedPaint& record : Gui::Test::joinTextRuns(device.records()))
    {
        if (record.kind == Gui::Test::RecordedPaint::Kind::TEXT)
        {
            texts.push_back(record.text);
        }
    }
    return texts;
}

/// The texts the overlay writes for a configuration whose CG and CP are @p expected
/// (RocketInfo.getCg(), getCp() and getStabilityCombined()): the CG and the CP in the default
/// length unit; the stability margin CP - CG in the configuration's stability unit, and " / "
/// with its secondary unit unless the margin is NaN, the two units are the same or the
/// preference DisplaySecondaryStability is off.
struct OverlayTexts
{
    QString cg;
    QString cp;
    QString stability;
};

[[nodiscard]] OverlayTexts overlayTexts(const FlightConfiguration& configuration,
                                        const OverlayNumbers&      expected,
                                        const Preferences&         preferences)
{
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> primary =
        StabilityUnits::stabilityUnits(configuration);
    const std::unique_ptr<UnitGroup::StabilityUnitGroup> secondary =
        StabilityUnits::secondaryStabilityUnits(configuration);
    const double margin    = expected.cp - expected.cg;
    QString      stability = QtStrings::unitTextWithUnit(primary->getDefaultUnit(), margin);
    if (!std::isnan(margin) && !secondary->getDefaultUnit().equals(primary->getDefaultUnit()) &&
        preferences.isDisplaySecondaryStability())
    {
        stability += QStringLiteral(" / ") +
                     QtStrings::unitTextWithUnit(secondary->getDefaultUnit(), margin);
    }
    return {.cg        = QtStrings::defaultUnitText(UnitGroupId::LENGTH, expected.cg),
            .cp        = QtStrings::defaultUnitText(UnitGroupId::LENGTH, expected.cp),
            .stability = stability};
}

/// Expects @p figure to paint each of @p texts.
void expectPainted(RocketFigure& figure, const OverlayTexts& texts)
{
    const std::vector<QString> painted = paintedTexts(figure);
    for (const QString& shown : {texts.cg, texts.cp, texts.stability})
    {
        EXPECT_TRUE(std::ranges::find(painted, shown) != painted.end())
            << "not painted: " << QtStrings::toUtf8(shown);
    }
}

/// Expects the overlay of @p panel to show @p expected for the rocket's selected configuration:
/// the CG and the CP within kGoldenAbsolute, and their texts and that of the stability margin
/// (overlayTexts()), painted in the figure.
void expectOverlay(const RocketPanel& panel, const OverlayNumbers& expected,
                   const Preferences& preferences)
{
    const RocketInfo& info = panel.physics().info();
    EXPECT_NEAR(info.cg(), expected.cg, kGoldenAbsolute) << "CG";
    EXPECT_NEAR(info.cp(), expected.cp, kGoldenAbsolute) << "CP";
    const OverlayTexts texts = overlayTexts(panel.document().getRocket().getSelectedConfiguration(),
                                            expected, preferences);
    EXPECT_EQ(info.getCg(), texts.cg);
    EXPECT_EQ(info.getCp(), texts.cp);
    EXPECT_EQ(info.getStabilityCombined(), texts.stability);
    expectPainted(panel.figure(), texts);
}

/// A design's window and what its overlay is compared with (observers).
struct OverlaySubject
{
    const OpenDesign*    design;
    const DesignGoldens* goldens;  ///< null for a design without goldens
    const RocketPanel*   panel;
    const Preferences*   preferences;
};

/// expectOverlay() for the rocket's selected configuration, the numbers of expectedNumbers().
void expectOverlayOf(const OverlaySubject& subject)
{
    Rocket&                             rocket = subject.panel->document().getRocket();
    const int                           index  = QtRocket::Test::selectedIndex(rocket);
    const std::optional<OverlayNumbers> expected =
        expectedNumbers(*subject.design, subject.goldens, rocket, index, *subject.preferences);
    if (expected.has_value())
    {
        expectOverlay(*subject.panel, *expected, *subject.preferences);
    }
}

/// The design documents of @p design's goldens; none for a design without (or with a failure
/// when they cannot be read).
[[nodiscard]] std::optional<DesignGoldens> designGoldensOf(const OpenDesign& design)
{
    if (design.goldenInput.empty())
    {
        return std::nullopt;
    }
    Result<DesignGoldens> loaded = QtRocket::Test::loadDesignGoldens(design.goldenInput);
    if (!loaded.has_value())
    {
        ADD_FAILURE() << loaded.error().toString();
        return std::nullopt;
    }
    return std::move(*loaded);
}

TEST_P(OpenDesignsExitGolden, ShowsTheGoldenNumbersInItsOverlay)
{
    const OpenDesign&       design = GetParam();
    const MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    const Rocket&                      rocket  = window->document().getRocket();
    const RocketPanel&                 panel   = rocketPanelOf(*window);
    const std::optional<DesignGoldens> goldens = designGoldensOf(design);
    const OverlaySubject               subject{.design  = &design,
                                               .goldens = goldens.has_value() ? &*goldens : nullptr,
                                               .panel   = &panel,
                                               .preferences = &preferences()};
    // As opened: the file's selected configuration.
    if (goldens.has_value())
    {
        EXPECT_EQ(QtRocket::Test::selectedIndex(rocket),
                  QtRocket::Test::goldenSelectedIndex(goldens->geometry));
    }
    expectOverlayOf(subject);

    // Every configuration chosen in the configuration selector: the default first, then the
    // others in the rocket's order (the goldens' indices).
    ConfigurationComboBox& configurations = panel.configurationSelector();
    ASSERT_EQ(configurations.count(), rocket.getConfigurationCount() + 1);
    for (int item = 0; item < configurations.count(); ++item)
    {
        SCOPED_TRACE(std::format("configuration {}", item));
        Gui::Test::chooseItem(configurations, item);
        settle();
        EXPECT_EQ(QtRocket::Test::selectedIndex(rocket), item);
        expectOverlayOf(subject);
    }
}

// ============================================================ the Motors & Configuration tab

using Gui::Test::CellStylePin;
using Gui::Test::ConfigurationRowPin;
using Gui::Test::EnabledPin;
using Gui::Test::MotorCellPin;
using Gui::Test::MotorMountRowPin;
using Gui::Test::OpenDesignTabPin;
using Gui::Test::ValueCellPin;

/// The pins of OpenRocket's tab for the design named @p name; null when there are none.
[[nodiscard]] const OpenDesignTabPin* tabPinOf(std::string_view name)
{
    for (const OpenDesignTabPin& pin : Gui::Test::kOpenDesignTabs)
    {
        if (pin.design == name)
        {
            return &pin;
        }
    }
    return nullptr;
}

/// The names of the designs the pins of OpenRocket's tab are of, in their order.
[[nodiscard]] std::vector<std::string_view> tabPinDesigns()
{
    std::vector<std::string_view> names;
    std::ranges::transform(Gui::Test::kOpenDesignTabs, std::back_inserter(names),
                           &OpenDesignTabPin::design);
    return names;
}

/// The names of @p designs, in their order.
[[nodiscard]] std::vector<std::string_view> namesOf(const std::vector<OpenDesign>& designs)
{
    std::vector<std::string_view> names;
    std::ranges::transform(designs, std::back_inserter(names),
                           [](const OpenDesign& design) { return std::string_view(design.name); });
    return names;
}

// The pins of OpenRocket's tab (OpenDesignsTabPins.h) come from the goldens' OpenRocket, are of
// the 66 designs of the suite in its order, and their two styles' colours are the run-2 pins'
// (FlightConfigurationTablePins.h, which tests them against ThemeColors' light ones).
TEST(OpenDesignsExitGoldenDesigns, HaveOpenRocketsTabEach)
{
    EXPECT_EQ(Gui::Test::kOpenDesignsTabPinsCommit, "5f164fd0e05d6e188857b0185601293b7a6e9eaf");
    EXPECT_EQ(tabPinDesigns(), namesOf(openDesigns()));
    EXPECT_EQ(Gui::Test::kOpenDesignsTabTextColour, Gui::Test::kJavaTextColour);
    EXPECT_EQ(Gui::Test::kOpenDesignsTabDimTextColour, Gui::Test::kJavaDimTextColour);
}

/// The text of @p value (a model's data), as UTF-8, as the pins hold their texts (and a failure
/// prints them).
[[nodiscard]] std::string textOf(const QVariant& value)
{
    return QtStrings::toUtf8(value.toString());
}

/// The configuration of the row @p row of @p model as Java's @p pin: its key (unless the loader
/// draws it: compared by row only), raw name and override of the default name (column 0:
/// expectNamesAsJava()).
void expectRowAsJava(const FlightConfigurableTableModel& model, std::size_t row,
                     const ConfigurationRowPin& pin, const Preferences& preferences)
{
    SCOPED_TRACE(std::format("row {}", row));
    const FlightConfigurationId& id = model.configurationIds()[row];
    if (!pin.drawn)
    {
        EXPECT_EQ(id.key().toString(), pin.key);
    }
    const Rocket&              rocket        = model.rocket();
    const FlightConfiguration& configuration = rocket.getFlightConfiguration(id);
    EXPECT_EQ(configuration.getNameRaw(preferences), pin.rawName);
    EXPECT_EQ(configuration.isNameOverridden(preferences), pin.nameOverridden);
}

/// The rows of @p model as Java's @p rows (expectRowAsJava()).
void expectRowsAsJava(const FlightConfigurableTableModel&  model,
                      std::span<const ConfigurationRowPin> rows, const Preferences& preferences)
{
    ASSERT_EQ(model.configurationIds().size(), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        expectRowAsJava(model, row, rows[row], preferences);
    }
}

/// The headers of @p model as Java's @p headers, column 0 ("Configuration") included.
void expectHeadersAsJava(const FlightConfigurableTableModel& model,
                         std::span<const std::string_view>   headers)
{
    ASSERT_EQ(static_cast<std::size_t>(model.columnCount()), headers.size());
    for (std::size_t column = 0; column < headers.size(); ++column)
    {
        EXPECT_EQ(textOf(model.headerData(static_cast<int>(column), Qt::Horizontal)),
                  headers[column])
            << "column " << column;
    }
}

/// Column 0 of @p model: the rows' names as Java's renderer writes them (the same in the three
/// tables), regular.
void expectNamesAsJava(const FlightConfigurableTableModel&  model,
                       std::span<const ConfigurationRowPin> rows)
{
    ASSERT_EQ(static_cast<std::size_t>(model.rowCount()), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const QModelIndex index = model.index(static_cast<int>(row), 0);
        EXPECT_EQ(textOf(model.data(index)), rows[row].name) << "row " << row;
        EXPECT_FALSE(model.data(index, FlightConfigurableTableModel::kDefaultRole).toBool())
            << "row " << row;
        EXPECT_FALSE(model.data(index, Qt::FontRole).isValid()) << "row " << row;
    }
}

/// The motor cell of @p model at @p pin as Java's: the motor label always regular, the ignition
/// shaded where Java shades it (the cell's configuration has no ignition override).
void expectMotorCellAsJava(const FlightConfigurableTableModel& model, const MotorCellPin& pin)
{
    SCOPED_TRACE(std::format("row {}, column {}", pin.row, pin.column));
    const QModelIndex index = model.index(pin.row, pin.column);
    EXPECT_EQ(textOf(model.data(index)), pin.motor);
    EXPECT_EQ(textOf(model.data(index, FlightConfigurableTableModel::kIgnitionTextRole)),
              pin.ignition);
    const bool shaded = pin.ignitionStyle == CellStylePin::SHADED;
    EXPECT_EQ(shaded, !pin.ignitionOverride);
    EXPECT_EQ(model.data(index, FlightConfigurableTableModel::kDefaultRole).toBool(), shaded);
    EXPECT_FALSE(model.data(index, Qt::FontRole).isValid());
}

/// The recovery or stage cell of @p model at @p pin as Java's. QtRocket shades a cell whose
/// configuration has no value of its own (!containsId(): OpenRocket's bug 1, fixed by the user's
/// decision U-M3-5, FlightConfigurableTableModel), where Java shades by isDefault(), which is
/// false for every cell of a loaded design.
void expectValueCellAsJava(const FlightConfigurableTableModel& model, const ValueCellPin& pin)
{
    SCOPED_TRACE(std::format("row {}, column {}", pin.row, pin.column));
    const QModelIndex index = model.index(pin.row, pin.column);
    EXPECT_EQ(textOf(model.data(index)), pin.text);
    EXPECT_EQ(pin.style == CellStylePin::SHADED, pin.isDefault);
    const bool shaded = !pin.containsId;
    EXPECT_EQ(model.data(index, FlightConfigurableTableModel::kDefaultRole).toBool(), shaded);
    EXPECT_EQ(qvariant_cast<QFont>(model.data(index, Qt::FontRole)).italic(), shaded);
    EXPECT_EQ(model.data(index, Qt::ForegroundRole).isValid(), shaded);
}

/// The cells of @p model, every component's column of every row, as Java's @p cells.
template <class Pin>
void expectCellsAsJava(const FlightConfigurableTableModel& model, std::span<const Pin> cells,
                       void (*expectCell)(const FlightConfigurableTableModel&, const Pin&))
{
    ASSERT_EQ(cells.size(), static_cast<std::size_t>(model.rowCount()) *
                                static_cast<std::size_t>(std::max(model.columnCount() - 1, 0)));
    for (const Pin& pin : cells)
    {
        expectCell(model, pin);
    }
}

/// The table of @p panel as Java's: its headers, column 0, its cells and the rows and columns it
/// selected (made with the window: the rocket's selected configuration in the last column).
template <class Pin>
void expectTableAsJava(const FlightConfigurablePanel&    panel,
                       std::span<const std::string_view> headers, std::span<const Pin> cells,
                       std::string_view selection, std::span<const ConfigurationRowPin> rows,
                       void (*expectCell)(const FlightConfigurableTableModel&, const Pin&))
{
    const FlightConfigurableTableModel& model = panel.tableModel();
    expectHeadersAsJava(model, headers);
    expectNamesAsJava(model, rows);
    expectCellsAsJava(model, cells, expectCell);
    EXPECT_EQ(Gui::Test::FlightConfigurationTab::lines(panel), selection);
}

/// The sub-tabs of @p tab as Java's: their titles, which are enabled, and the current one.
void expectSubTabsAsJava(const FlightConfigurationPanel& tab, const OpenDesignTabPin& pin)
{
    const QTabWidget& tabs = *tab.tabs();
    ASSERT_EQ(static_cast<std::size_t>(tabs.count()), pin.tabs.size());
    for (std::size_t i = 0; i < pin.tabs.size(); ++i)
    {
        const int index = static_cast<int>(i);
        EXPECT_EQ(QtStrings::toUtf8(tabs.tabText(index)), pin.tabs[i].name);
        EXPECT_EQ(tabs.isTabEnabled(index), pin.tabs[i].enabled) << pin.tabs[i].name;
    }
    EXPECT_EQ(tabs.currentIndex(), pin.currentTab);
}

/// A button of the tab, by the name of the Java field that holds it ("<panel>.<field>":
/// OpenDesignsTabPins.h).
struct TabButton
{
    std::string_view name;
    const QPushButton* (*button)(const FlightConfigurationPanel& tab);
};

const std::array<TabButton, 12> kTabButtons{{
    {.name   = "tab.newConfButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.newConfigurationButton();
     }},
    {.name   = "tab.renameConfButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.renameConfigurationButton();
     }},
    {.name   = "tab.removeConfButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.deleteConfigurationButton();
     }},
    {.name   = "tab.duplicateConfButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.duplicateConfigurationButton();
     }},
    {.name   = "motors.selectMotorButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.motorConfigurationPanel().selectMotorButton();
     }},
    {.name   = "motors.deleteMotorButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.motorConfigurationPanel().deleteMotorButton();
     }},
    {.name   = "motors.selectIgnitionButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.motorConfigurationPanel().selectIgnitionButton();
     }},
    {.name   = "motors.resetIgnitionButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.motorConfigurationPanel().resetIgnitionButton();
     }},
    {.name   = "recovery.selectDeploymentButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.recoveryConfigurationPanel().selectDeploymentButton();
     }},
    {.name   = "recovery.resetDeploymentButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.recoveryConfigurationPanel().resetDeploymentButton();
     }},
    {.name   = "stages.selectSeparationButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.separationConfigurationPanel().selectSeparationButton();
     }},
    // Java's field of the Stages panel's Reset button is named resetDeploymentButton.
    {.name   = "stages.resetDeploymentButton",
     .button = [](const FlightConfigurationPanel& tab) -> const QPushButton* {
         return tab.separationConfigurationPanel().resetSeparationButton();
     }},
}};

/// Whether @p button itself is enabled, as Java's isEnabled() tells: a disabled sub-tab disables
/// its page and so every widget on it (QTabWidget::setTabEnabled()), which a JTabbedPane does
/// not.
[[nodiscard]] bool isEnabledItself(const QPushButton& button)
{
    return !button.testAttribute(Qt::WA_ForceDisabled);
}

/// The buttons of @p tab as Java's @p buttons (kTabButtons' order, the probe's).
void expectButtonsAsJava(const FlightConfigurationPanel& tab, std::span<const EnabledPin> buttons)
{
    ASSERT_EQ(buttons.size(), kTabButtons.size());
    for (std::size_t i = 0; i < buttons.size(); ++i)
    {
        EXPECT_EQ(kTabButtons.at(i).name, buttons[i].name);
        const QPushButton* const button = kTabButtons.at(i).button(tab);
        ASSERT_NE(button, nullptr) << buttons[i].name;
        EXPECT_EQ(isEnabledItself(*button), buttons[i].enabled) << buttons[i].name;
    }
}

/// The motor mount table @p model as Java's @p rows: every MotorMount, its check box and name.
void expectMotorMountsAsJava(const MotorMountTableModel&       model,
                             std::span<const MotorMountRowPin> rows)
{
    ASSERT_EQ(static_cast<std::size_t>(model.rowCount()), rows.size());
    for (std::size_t row = 0; row < rows.size(); ++row)
    {
        const int r = static_cast<int>(row);
        EXPECT_EQ(
            model.data(model.index(r, MotorMountTableModel::kMotorMountColumn), Qt::CheckStateRole)
                .toInt(),
            static_cast<int>(rows[row].motorMount ? Qt::Checked : Qt::Unchecked))
            << "row " << row;
        EXPECT_EQ(textOf(model.data(model.index(r, MotorMountTableModel::kNameColumn))),
                  rows[row].name)
            << "row " << row;
    }
}

/// The names of the components of @p selection, "[A, B]".
[[nodiscard]] std::string selectedNames(const DocumentSelectionModel& selection)
{
    std::string text = "[";
    for (const RocketComponent* const component : selection.selectedComponents())
    {
        text += (text.size() > 1 ? ", " : "") + component->getName();
    }
    return text + "]";
}

/// The Motors panel of @p tab as Java's: its table (expectTableAsJava()), its help text instead of
/// the table for a design without an acting motor mount, and its motor mount table.
void expectMotorsAsJava(const FlightConfigurationPanel& tab, const OpenDesignTabPin& pin)
{
    const MotorConfigurationPanel& motors = tab.motorConfigurationPanel();
    expectTableAsJava(motors, pin.motorHeaders, pin.motorCells, pin.motorSelection,
                      pin.configurations, &expectMotorCellAsJava);
    EXPECT_EQ(motors.isShowingHelp(), pin.motorHelpShown);
    expectMotorMountsAsJava(motors.motorMountConfigurationPanel().model(), pin.motorMounts);
}

/// Clicks each row of the Motors table of @p tab, a tab of @p window, in its last column, as the
/// user chooses a configuration there, and expects the rocket's selected configuration to follow,
/// and with it the configuration selector of the design view (OpenRocket's table listener sets
/// Rocket.setSelectedConfiguration(), which the ribbon's ConfigurationComboBox shows: its item 0 is
/// the default configuration).
void expectRowsChooseTheConfiguration(const MainWindow& window, const FlightConfigurationPanel& tab)
{
    const MotorConfigurationPanel&      motors   = tab.motorConfigurationPanel();
    const FlightConfigurableTableModel& model    = motors.tableModel();
    const Rocket&                       rocket   = window.document().getRocket();
    const ConfigurationComboBox&        selector = rocketPanelOf(window).configurationSelector();
    for (int row = 0; row < model.rowCount(); ++row)
    {
        const int column = model.columnCount() - 1;
        motors.table().scrollTo(model.index(row, column));
        Gui::Test::FlightConfigurationTab::click(motors, row, column);
        settle();
        EXPECT_EQ(rocket.getSelectedConfiguration().getId(),
                  model.configurationIds()[static_cast<std::size_t>(row)])
            << "row " << row;
        EXPECT_EQ(selector.currentIndex(), row + 1) << "row " << row;
    }
}

/// The questions and messages File > Open of @p design gives: the file question, and the message
/// of the load's warnings when it has any.
[[nodiscard]] std::size_t questionsOfTheOpening(const OpenDesign& design)
{
    return expectedLoadWarnings(design).empty() ? 1U : 2U;
}

TEST_P(OpenDesignsExitGolden, TheConfigurationTabShowsOpenRocketsTables)
{
    // The tab as the user chooses it after File > Open, against OpenRocket's (OpenDesignsTabProbe:
    // its FlightConfigurationPanel made with the window, then takeTheSpotlight()).
    const OpenDesignTabPin* const pin = tabPinOf(GetParam().name);
    ASSERT_NE(pin, nullptr) << "no pins of OpenRocket's tab";
    MainWindow* const window = open();
    ASSERT_NE(window, nullptr);
    const Rocket&               rocket   = window->document().getRocket();
    const FlightConfigurationId selected = rocket.getSelectedConfiguration().getId();
    window->selectTab(MainWindow::kFlightConfigurationTab);
    settle();
    EXPECT_EQ(window->selectedTab(), MainWindow::kFlightConfigurationTab);
    const FlightConfigurationPanel* const tab = window->componentFlightConfigurationPanel();
    ASSERT_NE(tab, nullptr);

    // The rows: the rocket's configurations, its selected one, its stages.
    const FlightConfigurableTableModel& rows = tab->motorConfigurationPanel().tableModel();
    expectRowsAsJava(rows, pin->configurations, preferences());
    EXPECT_EQ(rows.rowOf(selected), pin->selectedRow);
    EXPECT_EQ(static_cast<int>(rocket.getStageCount()), pin->stageCount);
    // The sub-tabs and every button.
    expectSubTabsAsJava(*tab, *pin);
    expectButtonsAsJava(*tab, pin->buttons);
    // The three tables, cell by cell, and the motor mounts.
    {
        SCOPED_TRACE("motors");
        expectMotorsAsJava(*tab, *pin);
    }
    {
        SCOPED_TRACE("recovery");
        expectTableAsJava(tab->recoveryConfigurationPanel(), pin->recoveryHeaders,
                          pin->recoveryCells, pin->recoverySelection, pin->configurations,
                          &expectValueCellAsJava);
    }
    {
        SCOPED_TRACE("stages");
        expectTableAsJava(tab->separationConfigurationPanel(), pin->stageHeaders, pin->stageCells,
                          pin->stageSelection, pin->configurations, &expectValueCellAsJava);
    }
    // The design's selection, which the Motors table's selected columns set when the tab took the
    // spotlight (the top stage the window selected at its making, when no column is selected).
    EXPECT_EQ(selectedNames(designPanelOf(*window).selection()), pin->designSelection);
    // Choosing the tab changed nothing (as in OpenRocket: the probe's AFTER line).
    EXPECT_TRUE(window->document().isSaved());
    EXPECT_FALSE(window->isWindowModified());
    EXPECT_FALSE(window->document().isUndoAvailable());
    EXPECT_EQ(rocket.getSelectedConfiguration().getId(), selected);
    EXPECT_EQ(prompts().prompts().size(), questionsOfTheOpening(GetParam()));

    // Each row, clicked, chooses its configuration, in the design view too.
    expectRowsChooseTheConfiguration(*window, *tab);
}

// ======================================================================= the presets

/// What a preset of the database is to the suite: "<type>|<manufacturer>|<part number>|<digest>".
[[nodiscard]] std::string presetIdentity(const ComponentPreset& preset)
{
    return std::format("{}|{}|{}|{}", componentPresetTypeName(preset.getType()),
                       preset.getManufacturer().getSimpleName(), preset.getPartNo(),
                       preset.getDigest());
}

/// What @p database gives the .ork loader for each of the six <preset> elements of the example
/// designs (ComponentPresetSetter's lookup: the first preset of find(manufacturer, part number)
/// with the element's digest), as presetIdentity(); "none" for an element it finds nothing for.
[[nodiscard]] std::vector<std::string> presetsFoundIn(const ComponentPresetDatabase& database)
{
    std::vector<std::string> found;
    found.reserve(QtRocket::Test::kExamplePresetReferences.size());
    for (const QtRocket::Test::ExamplePresetReference& reference :
         QtRocket::Test::kExamplePresetReferences)
    {
        std::string identity = "none";
        for (const std::shared_ptr<const ComponentPreset>& preset :
             database.find(reference.manufacturer, reference.partNo))
        {
            if (preset->getDigest() == reference.digest)
            {
                identity = presetIdentity(*preset);
                break;
            }
        }
        found.push_back(std::move(identity));
    }
    return found;
}

/// The six <preset> elements of the example designs, as presetIdentity() writes a preset.
[[nodiscard]] std::vector<std::string> referencedPresets()
{
    std::vector<std::string> references;
    references.reserve(QtRocket::Test::kExamplePresetReferences.size());
    for (const QtRocket::Test::ExamplePresetReference& reference :
         QtRocket::Test::kExamplePresetReferences)
    {
        references.push_back(std::format("{}|{}|{}|{}", componentPresetTypeName(reference.type),
                                         reference.manufacturer, reference.partNo,
                                         reference.digest));
    }
    return references;
}

// The windows of the suite load with the six presets of the example designs
// (DesignFileEnvironment::Presets::EXAMPLES) where the application loads its bundled presets
// (data/components), which give the same loads: every <preset> element of the 66 designs finds one
// of the six (OpensWithItsTitleTreeAndLoadWarnings: no "No matching ComponentPreset" warning; the
// core's ExampleFilesWithTheBundledPresets for the examples with the bundled presets), and for
// each of the six elements the bundled presets give the loader the same preset as the six: the
// same type, manufacturer, part number and digest, the MD5 of all its properties. One test: the
// bundled presets are read once per process.
TEST(OpenDesignsExitGoldenPresets, TheBundledDatabaseGivesTheSixOfTheExamples)
{
    const ComponentPresetDatabase examples =
        DesignFileEnvironment::presetDatabase(DesignFileEnvironment::Presets::EXAMPLES);
    EXPECT_EQ(examples.size(), QtRocket::Test::kExamplePresetReferences.size());
    EXPECT_EQ(presetsFoundIn(examples), referencedPresets());
    EXPECT_EQ(presetsFoundIn(QtRocket::Test::bundledPresetDatabase()), referencedPresets());
}

INSTANTIATE_TEST_SUITE_P(Designs, OpenDesignsExitGolden, ::testing::ValuesIn(openDesigns()),
                         openDesignTestName);

}  // namespace

}  // namespace QtRocket::Gui
