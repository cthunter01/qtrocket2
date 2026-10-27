// The component actions of a design window (RocketActions): the Edit menu's actions connected and
// enabled by OpenRocket's rules, its own actions (Move up, Move down, the popup's Hide/Show, the
// stages' Disable/Enable), the popup menu, the requests for the configuration dialog, and what the
// visibility and stage actions do, compared with OpenRocket's transcripts: the enables of the M2
// component-tree scout's ActionsProbe (tools/openrocket-gui-pins/component-editing) and the
// visibility and stage sections of T4Probe (Milestone 2 run 2, part T4: OpenRocket 5f164fd0e run
// headless with JDK 17, RocketActions' inner actions made by their own constructors), on the
// probes' design (tree/ProbeDesign.h). What the editing actions do is ComponentEditing's
// (main/ComponentEditingTests.cpp); here a few of them run through their QActions.

#include "QtRocket/gui/main/RocketActions.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAction>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QMenu>
#include <QMenuBar>
#include <QObject>
#include <QPointer>
#include <QSignalSpy>
#include <QSize>
#include <QString>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/aero/ComponentDrag.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/main/ComponentEditing.h"
#include "QtRocket/gui/main/MainMenus.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/main/UndoRedoActions.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "RecordingGuardHook.h"
#include "ScriptedUserPrompts.h"
#include "rocket/TestRockets.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Names = std::vector<std::string_view>;
using Lines = std::vector<std::string>;
using Test::javaBool;
using Test::lineDifferences;
using Test::linesOf;
using Test::ProbeDesign;
using Test::RecordingGuardHook;
using Test::ScriptedUserPrompts;
using Test::undoText;

using RocketActionsTest = Gui::Test::GuiTest;

// ============================================================================== the support

/// @p component and every component below it, in pre-order (Java's iteration of a component).
[[nodiscard]] std::vector<RocketComponent*> preOrder(RocketComponent& component)
{
    std::vector<RocketComponent*> components{&component};
    for (RocketComponent* child : component.getChildren())
    {
        const std::vector<RocketComponent*> below = preOrder(*child);
        components.insert(components.end(), below.begin(), below.end());
    }
    return components;
}

/// The first component of @p rocket in pre-order named @p name; a failure and the Rocket when
/// there is none.
[[nodiscard]] RocketComponent& named(Rocket& rocket, std::string_view name)
{
    for (RocketComponent* component : preOrder(rocket))
    {
        if (component->getName() == name)
        {
            return *component;
        }
    }
    ADD_FAILURE() << "no component named " << name;
    return rocket;
}

/// "[Name, Name]" of @p components, as Java prints a list of names.
[[nodiscard]] std::string namesOf(const std::vector<RocketComponent*>& components)
{
    std::string text = "[";
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        text += (i > 0 ? ", " : "") + components[i]->getName();
    }
    return text + "]";
}

/// @p icon drawn at 16 pixels (two icons are the same when they draw the same).
[[nodiscard]] QImage drawnIcon(const QIcon& icon)
{
    return icon.pixmap(QSize(16, 16)).toImage();
}

/// The name of @p action's icon among the visibility and stage icons, as T4Probe prints it.
[[nodiscard]] std::string iconName(const QAction& action)
{
    if (action.icon().isNull())
    {
        return "null";
    }
    const QImage                                         drawn = drawnIcon(action.icon());
    const std::vector<std::pair<Icons::Id, std::string>> icons{
        {Icons::Id::COMPONENT_HIDDEN, "COMPONENT_HIDDEN"},
        {Icons::Id::COMPONENT_SHOWING, "COMPONENT_SHOWING"},
        {Icons::Id::COMPONENT_DISABLED, "COMPONENT_DISABLED"},
        {Icons::Id::COMPONENT_ENABLED, "COMPONENT_ENABLED"},
    };
    for (const auto& [id, name] : icons)
    {
        if (drawn == drawnIcon(Icons::icon(id)))
        {
            return name;
        }
    }
    return "other";
}

/// T4Probe's state(): "'<text>' ttip='<tooltip>' icon=<icon> enabled=<enabled>".
[[nodiscard]] std::string state(const QAction& action)
{
    return "'" + QtStrings::toUtf8(action.text()) + "' ttip='" +
           QtStrings::toUtf8(action.toolTip()) + "' icon=" + iconName(action) +
           " enabled=" + javaBool(action.isEnabled());
}

/// @p line without the field starting with @p field (" events=3"): up to the next space or the
/// end.
[[nodiscard]] std::string withoutField(std::string line, std::string_view field)
{
    const std::size_t start = line.find(field);
    if (start == std::string::npos)
    {
        return line;
    }
    const std::size_t end = line.find(' ', start + field.size());
    line.erase(start, (end == std::string::npos ? line.size() : end) - start);
    return line;
}

/// The lines of T4Probe's visibility transcript @p text without what QtRocket's test does not
/// compare: the number of events (one here: the rocket is frozen meanwhile, a documented
/// deviation; VisibilityChangesFireOneEvent) and Deselect all's state, which the probe does not
/// refresh with the others.
[[nodiscard]] Lines comparedVisibilityLines(std::string_view text)
{
    Lines lines = linesOf(text);
    for (std::string& line : lines)
    {
        line = withoutField(withoutField(line, " events="), " deselect enabled=");
    }
    return lines;
}

/// A design window's component actions over the tree of a document: the tree with its
/// selection, the Edit menu of a menu bar, the clipboard (the test's own), the questions, and the
/// requests of the configuration dialog recorded.
class Window
{
public:
    explicit Window(OpenRocketDocument& document)
      : m_document(&document),
        m_model(document, m_preferences),
        m_view(std::make_unique<ComponentTreeView>(m_model)),
        m_selection(makeChild<DocumentSelectionModel>(m_view.get())),
        m_undoRedo(document, nullptr),
        m_menuBar(std::make_unique<QMenuBar>()),
        m_menus(makeChild<MainMenus>(m_menuBar.get(), m_undoRedo)),
        m_actions(makeChild<RocketActions>(m_selection, *m_menus, m_clipboard, m_prompts))
    {
        QObject::connect(m_actions, &RocketActions::closeConfigDialogRequested, m_actions,
                         [this] { ++m_closeRequests; });
        // The analyzer sees no owner of the actions, which the selection (their Qt parent) has.
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks): the Qt parent deletes them
        QObject::connect(
            m_actions, &RocketActions::editRequested, m_actions,
            [this](RocketComponent* component, bool withSelection) {
                m_edits.push_back(component->getName() + " " + javaBool(withSelection));
            });
    }

    /// Selects the components @p names, in this order.
    void select(const Names& names) const
    {
        std::vector<RocketComponent*> components;
        components.reserve(names.size());
        for (const std::string_view name : names)
        {
            components.push_back(&named(m_document->getRocket(), name));
        }
        m_selection->setSelectedComponents(components);
    }

    /// The names of the selected components.
    [[nodiscard]] std::string selectionNames() const
    {
        return namesOf(m_selection->selectedComponents());
    }

    [[nodiscard]] OpenRocketDocument&        document() const noexcept { return *m_document; }
    [[nodiscard]] Rocket&                    rocket() const { return m_document->getRocket(); }
    [[nodiscard]] RocketActions&             actions() const { return *m_actions; }
    [[nodiscard]] DocumentSelectionModel&    selection() const { return *m_selection; }
    [[nodiscard]] MainMenus&                 menus() const { return *m_menus; }
    [[nodiscard]] const OpenRocketClipboard& clipboard() const noexcept { return m_clipboard; }
    [[nodiscard]] OpenRocketClipboard&       clipboard() noexcept { return m_clipboard; }
    [[nodiscard]] ScriptedUserPrompts&       prompts() noexcept { return m_prompts; }
    [[nodiscard]] int          closeRequests() const noexcept { return m_closeRequests; }
    [[nodiscard]] const Lines& edits() const noexcept { return m_edits; }

    /// Destroys the tree view, with its selection and the actions.
    void destroyView() { m_view = nullptr; }
    /// Destroys the menu bar, with the menus and their actions.
    void destroyMenus() { m_menuBar = nullptr; }

private:
    OpenRocketDocument*                m_document;
    InMemoryPreferences                m_preferences;
    OpenRocketClipboard                m_clipboard;
    ScriptedUserPrompts                m_prompts;
    ComponentTreeModel                 m_model;
    std::unique_ptr<ComponentTreeView> m_view;
    DocumentSelectionModel*            m_selection;
    UndoRedoActions                    m_undoRedo;
    std::unique_ptr<QMenuBar>          m_menuBar;
    MainMenus*                         m_menus;
    RocketActions*                     m_actions;
    int                                m_closeRequests = 0;
    Lines                              m_edits;
};

/// The change events of a rocket, as their texts, while it lives.
class EventRecorder
{
public:
    explicit EventRecorder(Rocket& rocket)
      : m_connection(rocket.addComponentChangeListener([this](const ComponentChangeEvent& event) {
            m_events.push_back(event.toString());
            m_types.push_back(event.getType());
        }))
    {
    }

    /// The types of the events since the last call.
    [[nodiscard]] std::vector<int> takeTypes()
    {
        std::vector<int> types = std::move(m_types);
        m_types.clear();
        m_events.clear();
        return types;
    }

    /// The events since the last call, as Java prints a list ("[ComponentChangeEvent[...]]").
    [[nodiscard]] std::string take()
    {
        std::string text = "[";
        for (std::size_t i = 0; i < m_events.size(); ++i)
        {
            text += (i > 0 ? ", " : "") + m_events[i];
        }
        m_events.clear();
        m_types.clear();
        return text + "]";
    }

private:
    Lines                                   m_events;
    std::vector<int>                        m_types;
    ComponentChangeSignal::ScopedConnection m_connection;
};

// =========================================================================== the actions

TEST_F(RocketActionsTest, DrivesTheEditMenusActionsAndHasItsOwn)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    const MainMenus&     menus   = window.menus();
    EXPECT_EQ(actions.editAction(), menus.editAction());
    EXPECT_EQ(actions.cutAction(), menus.cutAction());
    EXPECT_EQ(actions.copyAction(), menus.copyAction());
    EXPECT_EQ(actions.pasteAction(), menus.pasteAction());
    EXPECT_EQ(actions.duplicateAction(), menus.duplicateAction());
    EXPECT_EQ(actions.deleteAction(), menus.deleteAction());
    EXPECT_EQ(actions.deselectAllAction(), menus.deselectAllAction());
    EXPECT_EQ(actions.toggleVisibilityAction(), menus.toggleVisibilityAction());
    EXPECT_EQ(actions.showAllComponentsAction(), menus.showAllComponentsAction());
    EXPECT_EQ(&actions.selection(), &window.selection());
    // It belongs to the selection, so that it never outlives it.
    EXPECT_EQ(actions.parent(), &window.selection());
}

TEST_F(RocketActionsTest, OwnActionsHaveOpenRocketsTextsTooltipsAndIcons)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    const QAction&       up      = *actions.moveUpAction();
    const QAction&       down    = *actions.moveDownAction();
    EXPECT_EQ(up.objectName(), QStringLiteral("moveUpAction"));
    EXPECT_EQ(up.text(), QStringLiteral("Move up"));
    EXPECT_EQ(up.toolTip(), QStringLiteral("Move this component upwards."));
    EXPECT_EQ(drawnIcon(up.icon()), drawnIcon(Icons::icon(Icons::Id::UP)));
    EXPECT_TRUE(up.shortcuts().isEmpty());
    EXPECT_EQ(down.objectName(), QStringLiteral("moveDownAction"));
    EXPECT_EQ(down.text(), QStringLiteral("Move down"));
    EXPECT_EQ(down.toolTip(), QStringLiteral("Move this component downwards."));
    EXPECT_EQ(drawnIcon(down.icon()), drawnIcon(Icons::icon(Icons::Id::DOWN)));
    EXPECT_TRUE(down.shortcuts().isEmpty());
    // With nothing selected: ToggleVisibilityAction's first texts, ToggleActiveAction's.
    EXPECT_EQ(actions.toggleVisibilityContextMenuAction()->objectName(),
              QStringLiteral("toggleVisibilityContextMenuAction"));
    EXPECT_EQ(state(*actions.toggleVisibilityContextMenuAction()),
              "'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN "
              "enabled=false");
    EXPECT_EQ(actions.toggleActiveAction()->objectName(), QStringLiteral("toggleActiveAction"));
    EXPECT_EQ(state(*actions.toggleActiveAction()),
              "'Disable' ttip='Disable selected component(s) in the current flight "
              "configuration.' icon=COMPONENT_DISABLED enabled=false");
}

/// The menus of no window, in their own menu bar.
struct NoWindowMenuBar
{
    QMenuBar   menuBar;
    MainMenus* menus = makeChild<MainMenus>(&menuBar);
};

TEST_F(RocketActionsTest, ARocketActionsNeedsADesignWindowsMenusAndASelection)
{
    const ProbeDesign     design;
    const Window          window(design.document());
    const NoWindowMenuBar noWindow;
    OpenRocketClipboard   clipboard;
    ScriptedUserPrompts   prompts;
    EXPECT_THROW(
        static_cast<void>(RocketActions(*noWindow.menus, clipboard, prompts, &window.selection())),
        BugError);
    EXPECT_THROW(static_cast<void>(RocketActions(window.menus(), clipboard, prompts, nullptr)),
                 BugError);
}

// =============================================================================== the rules

/// ActionsProbe's enables of a selection (its enabled()): the line it prints.
struct EnablesScenario
{
    ProbeDesign::Booster booster = ProbeDesign::Booster::WITH_SIDE_BOOSTERS;
    Names                selection;
    std::string_view     expected;
};

/// The scenario of @p selection in the probes' two-stage design.
[[nodiscard]] EnablesScenario twoStages(Names selection, std::string_view expected)
{
    return {.booster   = ProbeDesign::Booster::WITH_SIDE_BOOSTERS,
            .selection = std::move(selection),
            .expected  = expected};
}

std::ostream& operator<<(std::ostream& stream, const EnablesScenario& scenario)
{
    return stream << scenario.expected;
}

/// ActionsProbe's enables() through the actions: "enabled with [..]: cut=.. edit=..".
[[nodiscard]] std::string enables(const Window& window)
{
    const RocketActions& a = window.actions();
    return "  enabled with " + window.selectionNames() +
           ": cut=" + javaBool(a.cutAction()->isEnabled()) +
           " copy=" + javaBool(a.copyAction()->isEnabled()) +
           " paste=" + javaBool(a.pasteAction()->isEnabled()) +
           " dup=" + javaBool(a.duplicateAction()->isEnabled()) +
           " del=" + javaBool(a.deleteAction()->isEnabled()) +
           " up=" + javaBool(a.moveUpAction()->isEnabled()) +
           " down=" + javaBool(a.moveDownAction()->isEnabled()) +
           " edit=" + javaBool(a.editAction()->isEnabled());
}

class RocketActionsEnables : public Gui::Test::GuiTest,
                             public ::testing::WithParamInterface<EnablesScenario>
{ };

TEST_P(RocketActionsEnables, AreOpenRocketsWithAnEmptyClipboard)
{
    const ProbeDesign design(GetParam().booster);
    const Window      window(design.document());
    window.select(GetParam().selection);
    EXPECT_EQ(enables(window), GetParam().expected);
}

// Every distinct line "enabled with" of ActionsProbe.out (the single-stage design for the
// Sustainer that cannot be deleted).
INSTANTIATE_TEST_SUITE_P(
    ActionsProbe, RocketActionsEnables,
    ::testing::Values(twoStages({"Booster"},
                                "  enabled with [Booster]: cut=true copy=true paste=false "
                                "dup=true del=true up=true down=false edit=true"),
                      twoStages({"BT1", "Chute"},
                                "  enabled with [BT1, Chute]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"BT1"},
                                "  enabled with [BT1]: cut=true copy=true paste=false "
                                "dup=true del=true up=true down=true edit=true"),
                      twoStages({"BT1", "IT"},
                                "  enabled with [BT1, IT]: cut=true copy=true paste=false "
                                "dup=true del=true up=true down=true edit=true"),
                      twoStages({"BT2"},
                                "  enabled with [BT2]: cut=true copy=true paste=false "
                                "dup=true del=true up=true down=false edit=true"),
                      twoStages({"Chute"},
                                "  enabled with [Chute]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"CR1", "Fins"},
                                "  enabled with [CR1, Fins]: cut=true copy=true paste=false "
                                "dup=true del=true up=true down=false edit=true"),
                      twoStages({"Fins", "IT"},
                                "  enabled with [Fins, IT]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"IT", "BT2"},
                                "  enabled with [IT, BT2]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"IT"},
                                "  enabled with [IT]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true"),
                      twoStages({"IT", "Fins"},
                                "  enabled with [IT, Fins]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"IT", "NC"},
                                "  enabled with [IT, NC]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true"),
                      twoStages({"NC", "BT1"},
                                "  enabled with [NC, BT1]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true"),
                      twoStages({"NC", "BT2"},
                                "  enabled with [NC, BT2]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"NC", "BT2", "IT"},
                                "  enabled with [NC, BT2, IT]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"NC", "Chute"},
                                "  enabled with [NC, Chute]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=false edit=true"),
                      twoStages({"NC"},
                                "  enabled with [NC]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true"),
                      twoStages({"NC", "IT"},
                                "  enabled with [NC, IT]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true"),
                      twoStages({"Rocket"},
                                "  enabled with [Rocket]: cut=false copy=false paste=false "
                                "dup=false del=false up=false down=false edit=true"),
                      twoStages({"Rocket", "NC"},
                                "  enabled with [Rocket, NC]: cut=false copy=false "
                                "paste=false dup=false del=false up=false down=false "
                                "edit=true"),
                      twoStages({"Side boosters"},
                                "  enabled with [Side boosters]: cut=true copy=true "
                                "paste=false dup=true del=true up=false down=false edit=true"),
                      twoStages({"Sustainer", "Booster"},
                                "  enabled with [Sustainer, Booster]: cut=true copy=true "
                                "paste=false dup=true del=true up=false down=false edit=true"),
                      EnablesScenario{
                          .booster   = ProbeDesign::Booster::NONE,
                          .selection = {"Sustainer"},
                          .expected = "  enabled with [Sustainer]: cut=false copy=true paste=false "
                                      "dup=true del=false up=false down=false edit=true"},
                      twoStages({"Sustainer"},
                                "  enabled with [Sustainer]: cut=true copy=true paste=false "
                                "dup=true del=true up=false down=true edit=true")));

/// What of the actions' enabled states differs from their rules (RocketActions.java's
/// clipboardChanged() of each action) for the selection of @p window, one text each.
[[nodiscard]] Lines ruleDifferences(const Window& window)
{
    const RocketActions&                      a        = window.actions();
    const std::vector<RocketComponent*>       selected = window.selection().selectedComponents();
    const std::vector<const RocketComponent*> constSelected(selected.begin(), selected.end());
    const bool                                any   = !selected.empty();
    const RocketComponent*                    first = any ? selected.front() : nullptr;
    const bool                                cut =
        any && std::ranges::all_of(selected, [](const RocketComponent* c) {
            return ComponentEditing::isDeletable(*c) && ComponentEditing::isCopyable(*c);
        });
    const bool copy      = first != nullptr && ComponentEditing::isCopyable(*first);
    const bool deletable = first != nullptr && ComponentEditing::isDeletable(*first);
    const std::vector<RocketComponent*> below = preOrder(window.rocket());
    const bool                          anyHidden =
        std::ranges::any_of(std::next(below.begin()), below.end(),
                            [](const RocketComponent* c) { return !c->isVisible(); });
    const std::vector<std::pair<std::string, std::pair<const QAction*, bool>>> rules{
        {"edit", {a.editAction(), any}},
        {"cut", {a.cutAction(), cut}},
        {"copy", {a.copyAction(), copy}},
        {"paste", {a.pasteAction(), false}},
        {"duplicate", {a.duplicateAction(), copy}},
        {"delete", {a.deleteAction(), deletable}},
        {"deselect all", {a.deselectAllAction(), any}},
        {"move up", {a.moveUpAction(), ComponentEditing::canMoveUp(constSelected)}},
        {"move down", {a.moveDownAction(), ComponentEditing::canMoveDown(constSelected)}},
        {"hide/show", {a.toggleVisibilityAction(), any}},
        {"popup hide/show", {a.toggleVisibilityContextMenuAction(), any}},
        {"show all", {a.showAllComponentsAction(), anyHidden}},
    };
    Lines differences;
    for (const auto& [name, rule] : rules)
    {
        if (rule.first->isEnabled() != rule.second)
        {
            differences.push_back(window.selectionNames() + " " + name);
        }
    }
    return differences;
}

/// ruleDifferences() for each component of @p window's rocket selected alone, and with the one
/// before it in pre-order (a hidden component among them).
[[nodiscard]] Lines ruleDifferencesOverTheRocket(const Window& window)
{
    const std::vector<RocketComponent*> components = preOrder(window.rocket());
    components.back()->setVisible(false);
    Lines differences;
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        const std::vector<RocketComponent*> alone{components[i]};
        window.selection().setSelectedComponents(alone);
        Lines more = ruleDifferences(window);
        if (i > 0)
        {
            const std::vector<RocketComponent*> pair{components[i], components[i - 1]};
            window.selection().setSelectedComponents(pair);
            const Lines pairs = ruleDifferences(window);
            more.insert(more.end(), pairs.begin(), pairs.end());
        }
        differences.insert(differences.end(), more.begin(), more.end());
    }
    window.selection().clearComponentSelection();
    const Lines none = ruleDifferences(window);
    differences.insert(differences.end(), none.begin(), none.end());
    return differences;
}

class RocketActionsTestRocket
  : public Gui::Test::GuiTest,
    public ::testing::WithParamInterface<QtRocket::Test::TestRocketMaker>
{ };

TEST_P(RocketActionsTestRocket, FollowOpenRocketsRulesForEverySelection)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(GetParam().make());
    const Window window(*document);
    EXPECT_EQ(ruleDifferencesOverTheRocket(window), Lines{});
}

std::string makerTestName(const ::testing::TestParamInfo<QtRocket::Test::TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, RocketActionsTestRocket,
                         ::testing::ValuesIn(QtRocket::Test::testRocketMakers()), makerTestName);

/// A transcript of ActionsRulesProbe (tools/openrocket-gui-pins/actions-rules; Milestone 2 run 2,
/// the fixer of group T, for the review's finding that the rules above are QtRocket's own:
/// OpenRocket 5f164fd0e run headless with JDK 17, RocketActions' inner actions as in the scouting's
/// ActionsProbe, each action's clipboardChanged() asked, the clipboard empty) for one of
/// OpenRocket's test rockets: the components in pre-order with their indexes (0 the Rocket; the
/// last one hidden first, "(hidden)"), then for nothing selected, each component alone and each
/// with the one before it (the component first), the selection as indexes and the enabled states, 1
/// or 0, of Edit, Cut, Copy, Paste, Duplicate, Delete, Deselect all, Move up, Move down, Hide/Show
/// (the menu's), Hide/Show (the popup's) and Show all. (The probe's simulation panel is null: a
/// rule whose component part is false and that then asks it throws there; that state is the
/// component part, false, as the design window's is with no simulation selected.)
struct RulesTranscript
{
    std::string_view method;
    std::string_view transcript;
};

/// ActionsRulesProbe.out, maker by maker.
[[nodiscard]] std::span<const RulesTranscript> rulesTranscripts()
{
    static constexpr std::array<RulesTranscript, 13> kTranscripts{{
        {.method     = "makeEstesAlphaIII",
         .transcript = R"(0 Estes Alpha III / Code Verification Rocket
1 Stage
2 Nose Cone
3 Body Tube
4 3 Fin Set
5 Launch Lugs
6 Motor Mount Tube
7 Engine Block
8 Parachute
9 Centering Rings (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011111111
[5, 4] 111011101111
[6] 111011111111
[6, 5] 111011111111
[7] 111011100111
[7, 6] 111011111111
[8] 111011111111
[8, 7] 111011100111
[9] 111011110111
[9, 8] 111011110111
[] 000000000001)"},
        {.method = "makeBeta", .transcript = R"(0 Kit-bash Beta
1 Sustainer Stage
2 Nose Cone
3 Sustainer Body Tube
4 3 Fin Set
5 Launch Lugs
6 Motor Mount Tube
7 Engine Block
8 Parachute
9 Centering Rings
10 Booster Stage
11 Booster Body
12 Coupler
13 Booster Fins
14 Booster MMT
15 Launch Lugs
16 Booster Tail Cone (hidden)
[0] 100000100111
[1] 111011101111
[1, 0] 101011100111
[2] 111011101111
[2, 1] 111011101111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011111111
[5, 4] 111011101111
[6] 111011111111
[6, 5] 111011111111
[7] 111011100111
[7, 6] 111011111111
[8] 111011111111
[8, 7] 111011100111
[9] 111011110111
[9, 8] 111011110111
[10] 111011110111
[10, 9] 111011110111
[11] 111011101111
[11, 10] 111011110111
[12] 111011101111
[12, 11] 111011101111
[13] 111011111111
[13, 12] 111011101111
[14] 111011111111
[14, 13] 111011111111
[15] 111011110111
[15, 14] 111011110111
[16] 111011110111
[16, 15] 111011110111
[] 000000000001)"},
        {.method = "makeSimple2Stage", .transcript = R"(0 Simple 2-Stage Rocket
1 Sustainer Stage
2 Sustainer Body Tube
3 Booster Stage
4 Booster Body Tube (hidden)
[0] 100000100111
[1] 111011101111
[1, 0] 101011100111
[2] 111011100111
[2, 1] 111011101111
[3] 111011110111
[3, 2] 111011100111
[4] 111011100111
[4, 3] 111011110111
[] 000000000001)"},
        {.method = "makeBigBlue", .transcript = R"(0 Rocket
1 Stage1
2 Nose Cone
3 Body Tube
4 Freeform Fin Set
5 Mass Component (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011110111
[5, 4] 111011100111
[] 000000000001)"},
        {.method = "makeIsoHaisu", .transcript = R"(0 Rocket
1 Stage1
2 Nose Cone
3 Body Tube
4 Launch Lug
5 Tube Coupler
6 Mass Component
7 Mass Component
8 Mass Component
9 CONTROL
10 Body Tube
11 Tube Coupler
12 Mass Component
13 Bulkhead
14 Mass Component
15 Body Tube
16 Inner Tube
17 Centering Ring
18 Centering Ring
19 Centering Ring
20 Trapezoidal Fin Set (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011111111
[3, 2] 111011101111
[4] 111011101111
[4, 3] 111011111111
[5] 111011111111
[5, 4] 111011101111
[6] 111011111111
[6, 5] 111011111111
[7] 111011111111
[7, 6] 111011111111
[8] 111011111111
[8, 7] 111011111111
[9] 111011110111
[9, 8] 111011110111
[10] 111011111111
[10, 9] 111011110111
[11] 111011101111
[11, 10] 111011111111
[12] 111011111111
[12, 11] 111011101111
[13] 111011111111
[13, 12] 111011111111
[14] 111011110111
[14, 13] 111011110111
[15] 111011110111
[15, 14] 111011110111
[16] 111011101111
[16, 15] 111011110111
[17] 111011111111
[17, 16] 111011101111
[18] 111011111111
[18, 17] 111011111111
[19] 111011111111
[19, 18] 111011111111
[20] 111011110111
[20, 19] 111011110111
[] 000000000001)"},
        {.method = "makeFalcon9Heavy", .transcript = R"(0 Falcon9H Scale Rocket
1 Payload Fairing Stage
2 PL Fairing Nose
3 PL Fairing Body
4 PL Fairing Transition
5 Upper Stage Body
6 Parachute
7 Shock Cord
8 Interstage
9 Core Stage
10 Core Stage Body
11 Booster Stage
12 Booster Nose
13 Booster Body
14 Booster Motor Tubes
15 Booster Fins (hidden)
[0] 100000100111
[1] 111011101111
[1, 0] 101011100111
[2] 111011101111
[2, 1] 111011101111
[3] 111011111111
[3, 2] 111011101111
[4] 111011111111
[4, 3] 111011111111
[5] 111011111111
[5, 4] 111011111111
[6] 111011101111
[6, 5] 111011111111
[7] 111011110111
[7, 6] 111011100111
[8] 111011110111
[8, 7] 111011110111
[9] 111011110111
[9, 8] 111011110111
[10] 111011100111
[10, 9] 111011110111
[11] 111011100111
[11, 10] 111011100111
[12] 111011101111
[12, 11] 111011100111
[13] 111011110111
[13, 12] 111011100111
[14] 111011101111
[14, 13] 111011110111
[15] 111011110111
[15, 14] 111011100111
[] 000000000001)"},
        {.method = "makeMultiStageEventTestRocket", .transcript = R"(0 Rocket
1 Sustainer
2 Sustainer Nose Cone
3 Sustainer Body Tube
4 Sustainer Parachute
5 Center Booster
6 Center Booster Body Tube
7 Center Booster Fin Set
8 Side boosters
9 Side Booster Nose Cones
10 Side Booster Body Tubes
11 Side Chutes (hidden)
[0] 100000100111
[1] 111011101111
[1, 0] 101011100111
[2] 111011101111
[2, 1] 111011101111
[3] 111011110111
[3, 2] 111011100111
[4] 111011100111
[4, 3] 111011110111
[5] 111011110111
[5, 4] 111011100111
[6] 111011100111
[6, 5] 111011110111
[7] 111011101111
[7, 6] 111011100111
[8] 111011110111
[8, 7] 111011100111
[9] 111011101111
[9, 8] 111011110111
[10] 111011110111
[10, 9] 111011100111
[11] 111011100111
[11, 10] 111011110111
[] 000000000001)"},
        {.method = "makeEndPlateRocket", .transcript = R"(0 End Plate Test
1 Sustainer
2 Nose Cone
3 Body tube
4 Body Tube FinSet
5 Pod Set
6 Phantom
7 End plates (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011110111
[5, 4] 111011100111
[6] 111011100111
[6, 5] 111011110111
[7] 111011100111
[7, 6] 111011100111
[] 000000000001)"},
        {.method     = "makeEstesAlphaIIIWithPods",
         .transcript = R"(0 Estes Alpha III / Code Verification Rocket
1 Stage
2 Nose Cone
3 Body Tube
4 Launch Lugs
5 Motor Mount Tube
6 Engine Block
7 Parachute
8 Centering Rings
9 Pod Set
10 Pod Body
11 3 Fin Set (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011111111
[5, 4] 111011101111
[6] 111011100111
[6, 5] 111011111111
[7] 111011111111
[7, 6] 111011100111
[8] 111011111111
[8, 7] 111011111111
[9] 111011110111
[9, 8] 111011110111
[10] 111011100111
[10, 9] 111011110111
[11] 111011100111
[11, 10] 111011100111
[] 000000000001)"},
        {.method = "makeEstesAlphaIIIWithMotorPods", .transcript = R"(0 Alpha III with motor pods
1 Stage
2 Nose Cone
3 Body Tube
4 3 Fin Set
5 Launch Lugs
6 Motor Mount Tube
7 Engine Block
8 Parachute
9 Centering Rings
10 Motor Pods
11 Pod Body Tube
12 Pod Motor Mount (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011111111
[5, 4] 111011101111
[6] 111011111111
[6, 5] 111011111111
[7] 111011100111
[7, 6] 111011111111
[8] 111011111111
[8, 7] 111011100111
[9] 111011111111
[9, 8] 111011111111
[10] 111011110111
[10, 9] 111011110111
[11] 111011100111
[11, 10] 111011110111
[12] 111011100111
[12, 11] 111011100111
[] 000000000001)"},
        {.method     = "makeEstesAlphaIIIWithSecondMotor",
         .transcript = R"(0 Alpha III with a second motor
1 Stage
2 Nose Cone
3 Body Tube
4 3 Fin Set
5 Launch Lugs
6 Motor Mount Tube
7 Engine Block
8 Parachute
9 Centering Rings
10 Second Motor Mount Tube (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011101111
[4, 3] 111011110111
[5] 111011111111
[5, 4] 111011101111
[6] 111011111111
[6, 5] 111011111111
[7] 111011100111
[7, 6] 111011111111
[8] 111011111111
[8, 7] 111011100111
[9] 111011111111
[9, 8] 111011111111
[10] 111011110111
[10, 9] 111011110111
[] 000000000001)"},
        {.method     = "makeEstesAlphaIIIwithInlinePod",
         .transcript = R"(0 Estes Alpha III / Code Verification Rocket
1 Stage
2 Nose Cone
3 Front Body Tube
4 Pod Set
5 Middle Body Tube
6 Aft body tube
7 3 Fin Set
8 Launch Lugs
9 Motor Mount Tube
10 Engine Block
11 Parachute
12 Centering Rings (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011101111
[2, 1] 101011100111
[3] 111011110111
[3, 2] 111011100111
[4] 111011100111
[4, 3] 111011110111
[5] 111011101111
[5, 4] 111011100111
[6] 111011110111
[6, 5] 111011100111
[7] 111011101111
[7, 6] 111011110111
[8] 111011111111
[8, 7] 111011101111
[9] 111011111111
[9, 8] 111011111111
[10] 111011100111
[10, 9] 111011111111
[11] 111011111111
[11, 10] 111011100111
[12] 111011110111
[12, 11] 111011110111
[] 000000000001)"},
        {.method = "makeClusterPods", .transcript = R"(0 Rocket
1 Sustainer
2 Sustainer Body Tube
3 Inner Tube
4 Booster Set
5 Side Booster Body Tubes
6 Inner Tube (hidden)
[0] 100000100111
[1] 101010100111
[1, 0] 101010100111
[2] 111011100111
[2, 1] 101011100111
[3] 111011101111
[3, 2] 111011100111
[4] 111011110111
[4, 3] 111011100111
[5] 111011100111
[5, 4] 111011110111
[6] 111011100111
[6, 5] 111011100111
[] 000000000001)"},

    }};
    return kTranscripts;
}

/// The enabled states of @p window's actions as ActionsRulesProbe prints them.
[[nodiscard]] std::string enabledStates(const Window& window)
{
    const RocketActions&                 actions = window.actions();
    const std::array<const QAction*, 12> inProbeOrder{actions.editAction(),
                                                      actions.cutAction(),
                                                      actions.copyAction(),
                                                      actions.pasteAction(),
                                                      actions.duplicateAction(),
                                                      actions.deleteAction(),
                                                      actions.deselectAllAction(),
                                                      actions.moveUpAction(),
                                                      actions.moveDownAction(),
                                                      actions.toggleVisibilityAction(),
                                                      actions.toggleVisibilityContextMenuAction(),
                                                      actions.showAllComponentsAction()};
    std::string                          states;
    for (const QAction* action : inProbeOrder)
    {
        states += action->isEnabled() ? '1' : '0';
    }
    return states;
}

/// The line of ActionsRulesProbe for @p window with the components @p indexes of @p components
/// selected, in this order.
[[nodiscard]] std::string rulesLine(const Window&                        window,
                                    const std::vector<RocketComponent*>& components,
                                    const std::vector<std::size_t>&      indexes)
{
    std::vector<RocketComponent*> selected;
    std::string                   line = "[";
    for (const std::size_t index : indexes)
    {
        selected.push_back(components.at(index));
        line += (selected.size() > 1 ? ", " : "") + std::to_string(index);
    }
    window.selection().setSelectedComponents(selected);
    return line + "] " + enabledStates(window);
}

/// ActionsRulesProbe's transcript of @p window's rocket.
[[nodiscard]] Lines rulesTranscriptOf(const Window& window)
{
    const std::vector<RocketComponent*> components = preOrder(window.rocket());
    components.back()->setVisible(false);
    Lines lines;
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        lines.push_back(std::to_string(i) + " " + components[i]->getName() +
                        (components[i]->isVisible() ? "" : " (hidden)"));
    }
    for (std::size_t i = 0; i < components.size(); ++i)
    {
        lines.push_back(rulesLine(window, components, {i}));
        if (i > 0)
        {
            lines.push_back(rulesLine(window, components, {i, i - 1}));
        }
    }
    lines.push_back(rulesLine(window, components, {}));
    return lines;
}

/// The transcript of the maker @p method; a failure and an empty one when there is none.
[[nodiscard]] std::string_view rulesTranscriptFor(std::string_view method)
{
    for (const RulesTranscript& transcript : rulesTranscripts())
    {
        if (transcript.method == method)
        {
            return transcript.transcript;
        }
    }
    ADD_FAILURE() << "no transcript of " << method;
    return {};
}

TEST_P(RocketActionsTestRocket, AreEnabledAsInOpenRocketsTranscript)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(GetParam().make());
    const Window window(*document);
    const Lines  expected = linesOf(rulesTranscriptFor(GetParam().method));
    EXPECT_EQ(lineDifferences(rulesTranscriptOf(window), expected), Lines{});
}

TEST_F(RocketActionsTest, AreEvaluatedAgainWhenTheSelectionChanges)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    window.select({"BT2"});
    EXPECT_TRUE(actions.moveUpAction()->isEnabled());
    EXPECT_FALSE(actions.moveDownAction()->isEnabled());
    window.select({"NC"});
    EXPECT_FALSE(actions.moveUpAction()->isEnabled());
    EXPECT_TRUE(actions.moveDownAction()->isEnabled());
    window.selection().clearComponentSelection();
    EXPECT_FALSE(actions.editAction()->isEnabled());
    EXPECT_FALSE(actions.deselectAllAction()->isEnabled());
}

TEST_F(RocketActionsTest, AreEvaluatedAgainWhenTheClipboardChanges)
{
    const ProbeDesign    design;
    Window               window(design.document());
    const RocketActions& actions = window.actions();
    window.select({"BT1"});
    EXPECT_FALSE(actions.pasteAction()->isEnabled());
    actions.copyAction()->trigger();
    EXPECT_FALSE(window.clipboard().isEmpty());
    EXPECT_TRUE(actions.pasteAction()->isEnabled());
    window.clipboard().clear();
    EXPECT_FALSE(actions.pasteAction()->isEnabled());
}

TEST_F(RocketActionsTest, AreEvaluatedAgainAtEveryChangeOfTheRocket)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    window.select({"Booster"});
    EXPECT_FALSE(actions.showAllComponentsAction()->isEnabled());
    EXPECT_EQ(actions.toggleActiveAction()->text(), QStringLiteral("Disable"));
    // Changes of the rocket that leave the selection as it is.
    design.find("PBT").setVisible(false);
    EXPECT_TRUE(actions.showAllComponentsAction()->isEnabled());
    FlightConfiguration& configuration = design.rocket().getSelectedConfiguration();
    configuration.toggleStage(1);
    design.rocket().fireComponentChangeEvent(ComponentChangeEvent::kTreeChangeChildren,
                                             configuration.getId());
    EXPECT_EQ(actions.toggleActiveAction()->text(), QStringLiteral("Enable"));
    EXPECT_EQ(window.selectionNames(), "[Booster]");
}

// ======================================================================= what they do

/// What the design, the clipboard and the undo history are after an action:
/// "tree=... clipboard=[...] undo='...'".
[[nodiscard]] std::string outcome(const ProbeDesign& design, const Window& window)
{
    return "tree=" + design.tree() +
           " clipboard=" + design.clipboard(window.clipboard().components()) + " " +
           undoText(design.document());
}

/// A scenario of ActionsProbe run through the actions: the selection, the action and its outcome
/// as ActionsProbe.out has it; with a second selection and action (a paste) when there is one.
struct OperationScenario
{
    /// An action's accessor.
    using Accessor = QAction* (RocketActions::*)() const noexcept;

    std::string_view name;
    Names            selection;
    Accessor         action = nullptr;
    std::string_view expected;
    Names            thenSelection;
    Accessor         then = nullptr;
    std::string_view thenExpected;
};

/// The scenario @p name of one action.
[[nodiscard]] OperationScenario operation(std::string_view name, Names selection,
                                          OperationScenario::Accessor action,
                                          std::string_view            expected)
{
    return {.name          = name,
            .selection     = std::move(selection),
            .action        = action,
            .expected      = expected,
            .thenSelection = {},
            .then          = nullptr,
            .thenExpected  = {}};
}

std::ostream& operator<<(std::ostream& stream, const OperationScenario& scenario)
{
    return stream << scenario.name;
}

class RocketActionsOperation : public Gui::Test::GuiTest,
                               public ::testing::WithParamInterface<OperationScenario>
{ };

TEST_P(RocketActionsOperation, DoesWhatOpenRocketsActionDoes)
{
    const OperationScenario& scenario = GetParam();
    const ProbeDesign        design;
    const Window             window(design.document());
    window.select(scenario.selection);
    (window.actions().*scenario.action)()->trigger();
    EXPECT_EQ(outcome(design, window), scenario.expected);
    if (scenario.then != nullptr)
    {
        window.select(scenario.thenSelection);
        (window.actions().*scenario.then)()->trigger();
        EXPECT_EQ(outcome(design, window), scenario.thenExpected);
    }
}

// ActionsProbe.out's trees, clipboards and undo texts (its clipboard of a paste or a duplicate
// shows what Copy, Cut or Duplicate put there).
INSTANTIATE_TEST_SUITE_P(
    ActionsProbe, RocketActionsOperation,
    ::testing::Values(
        OperationScenario{
            .name          = "CopyBT1PasteOnBT2",
            .selection     = {"BT1"},
            .action        = &RocketActions::copyAction,
            .expected      = "tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
                             "Booster[BBT[Side boosters[PBT]]]] clipboard=[BT1*[IT*[EB*], CR1*, "
                             "Fins*]] undo='null'",
            .thenSelection = {"BT2"},
            .then          = &RocketActions::pasteAction,
            .thenExpected =
                "tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], "
                "CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]] clipboard=[BT1*[IT*[EB*], CR1*, "
                "Fins*]] undo='Paste Body Tube'"},
        operation("CutBT1", {"BT1"}, &RocketActions::cutAction,
                  "tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] "
                  "clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] undo='Cut components'"),
        operation("DuplicateIT", {"IT"}, &RocketActions::duplicateAction,
                  "tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT*[EB*]], BT2[Chute]], "
                  "Booster[BBT[Side boosters[PBT]]]] clipboard=[IT*[EB*]] "
                  "undo='Duplicate Inner Tube'"),
        operation("DeleteNCBT2IT", {"NC", "BT2", "IT"}, &RocketActions::deleteAction,
                  "tree=Rocket[Sustainer[BT1[CR1, Fins]], Booster[BBT[Side boosters[PBT]]]] "
                  "clipboard=[] undo='Delete components'"),
        operation("MoveUpCR1Fins", {"CR1", "Fins"}, &RocketActions::moveUpAction,
                  "tree=Rocket[Sustainer[NC, BT1[CR1, Fins, IT[EB]], BT2[Chute]], "
                  "Booster[BBT[Side boosters[PBT]]]] clipboard=[] undo='Move components'"),
        operation("MoveDownNCBT1", {"NC", "BT1"}, &RocketActions::moveDownAction,
                  "tree=Rocket[Sustainer[BT2[Chute], NC, BT1[IT[EB], CR1, Fins]], "
                  "Booster[BBT[Side boosters[PBT]]]] clipboard=[] undo='Move components'")),
    Test::scenarioName<OperationScenario>);

/// The number of requests to close the configuration dialog that @p action makes when it is
/// triggered with @p selection selected in @p window.
[[nodiscard]] int closeRequestsOf(const Window& window, QAction* action, const Names& selection)
{
    window.select(selection);
    const int before = window.closeRequests();
    action->trigger();
    return window.closeRequests() - before;
}

/// Whether the popup shows Disable/Enable with @p selection selected in @p window.
[[nodiscard]] bool popupShowsTheStageItemFor(const Window& window, const Names& selection)
{
    window.select(selection);
    window.actions().prepareContextMenu();
    return window.actions().contextMenu()->actions().contains(
        window.actions().toggleActiveAction());
}

TEST_F(RocketActionsTest, AsksToCloseTheDialogWhereJavasActionDisposesIt)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    EXPECT_EQ(closeRequestsOf(window, actions.copyAction(), {"BT1"}), 0);
    EXPECT_EQ(closeRequestsOf(window, actions.pasteAction(), {"BT2"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.duplicateAction(), {"NC"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.moveUpAction(), {"BT2"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.moveDownAction(), {"NC"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.deleteAction(), {"Chute"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.cutAction(), {"CR1"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.editAction(), {"BT1"}), 1);
    EXPECT_EQ(closeRequestsOf(window, actions.toggleVisibilityAction(), {"BT1"}), 0);
    EXPECT_EQ(closeRequestsOf(window, actions.toggleVisibilityContextMenuAction(), {"BT1"}), 0);
    EXPECT_EQ(closeRequestsOf(window, actions.showAllComponentsAction(), {"BT1"}), 0);
    EXPECT_EQ(closeRequestsOf(window, actions.toggleActiveAction(), {"Booster"}), 0);
    EXPECT_EQ(closeRequestsOf(window, actions.deselectAllAction(), {"BT1"}), 0);
    // A disabled action does nothing: Move up of the first stage.
    EXPECT_EQ(closeRequestsOf(window, actions.moveUpAction(), {"Sustainer"}), 0);
}

/// The number of requests for the design tab that @p action makes when it is triggered with
/// @p selection selected in @p window.
[[nodiscard]] int designTabRequestsOf(const Window& window, QAction* action, const Names& selection)
{
    window.select(selection);
    const QSignalSpy requests(&window.actions(), &RocketActions::designTabRequested);
    action->trigger();
    return static_cast<int>(requests.count());
}

TEST_F(RocketActionsTest, AsksForTheDesignTabWhereJavasActionSelectsIt)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    // Paste with an empty clipboard is disabled: nothing done, no tab.
    EXPECT_EQ(designTabRequestsOf(window, actions.pasteAction(), {"BT2"}), 0);
    // parentFrame.selectTab(DESIGN_TAB) at the end of Cut, Copy, Paste, Duplicate and Delete.
    EXPECT_EQ(designTabRequestsOf(window, actions.copyAction(), {"BT1"}), 1);
    EXPECT_EQ(designTabRequestsOf(window, actions.pasteAction(), {"BT2"}), 1);
    // A paste that fits nowhere (a body tube into the engine block's inner tube) goes on to the
    // end in Java, selecting what it pasted (nothing) and the design tab.
    EXPECT_EQ(designTabRequestsOf(window, actions.pasteAction(), {"EB"}), 1);
    EXPECT_EQ(window.selectionNames(), "[]");
    EXPECT_EQ(designTabRequestsOf(window, actions.duplicateAction(), {"NC"}), 1);
    EXPECT_EQ(designTabRequestsOf(window, actions.deleteAction(), {"Chute"}), 1);
    EXPECT_EQ(designTabRequestsOf(window, actions.cutAction(), {"CR1"}), 1);
    // The other actions leave the tab alone.
    EXPECT_EQ(designTabRequestsOf(window, actions.moveUpAction(), {"BT2"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.moveDownAction(), {"NC"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.editAction(), {"BT1"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.toggleVisibilityAction(), {"BT1"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.toggleVisibilityContextMenuAction(), {"BT1"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.showAllComponentsAction(), {"BT1"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.toggleActiveAction(), {"Booster"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.deselectAllAction(), {"BT1"}), 0);
    // An action whose rule says no: Copy and Delete with the Rocket selected.
    EXPECT_EQ(designTabRequestsOf(window, actions.copyAction(), {"Rocket"}), 0);
    EXPECT_EQ(designTabRequestsOf(window, actions.deleteAction(), {"Rocket"}), 0);
}

TEST_F(RocketActionsTest, EditAsksForTheDialogOfTheFirstSelectedComponentWithTheSelection)
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& actions = window.actions();
    window.select({"BT2", "NC", "IT"});
    actions.editAction()->trigger();
    EXPECT_EQ(window.edits(), Lines{"BT2 true"});
    EXPECT_EQ(window.closeRequests(), 1);
    window.select({"Rocket"});
    actions.editAction()->trigger();
    EXPECT_EQ(window.edits(), (Lines{"BT2 true", "Rocket true"}));
    window.selection().clearComponentSelection();
    actions.editAction()->trigger();
    EXPECT_EQ(window.edits().size(), 2U);
}

TEST_F(RocketActionsTest, DeselectAllSelectsNothing)
{
    const ProbeDesign design;
    const Window      window(design.document());
    window.select({"BT2", "NC"});
    ASSERT_TRUE(window.actions().deselectAllAction()->isEnabled());
    window.actions().deselectAllAction()->trigger();
    EXPECT_EQ(window.selectionNames(), "[]");
    EXPECT_FALSE(window.actions().deselectAllAction()->isEnabled());
}

// ============================================================================= Hide / Show

/// T4Probe's visibility sections run through the actions: the states of Hide/Show selected, the
/// popup's Hide/Show and Show all, and the hidden components after each toggle.
class VisibilityTranscript
{
public:
    explicit VisibilityTranscript(const Window& window) : m_window(&window) { }

    void line(std::string text) { m_lines.push_back(std::move(text)); }

    /// visibilityStates(): the three actions' states for the selection.
    void states(std::string_view label)
    {
        const RocketActions& a = m_window->actions();
        line("  " + std::string(label) + " " + m_window->selectionNames() + ": menu " +
             state(*a.toggleVisibilityAction()));
        line("    context " + state(*a.toggleVisibilityContextMenuAction()));
        line("    show all " + state(*a.showAllComponentsAction()));
    }

    /// toggle(): @p action triggered with @p selection selected, the states before and after.
    void toggle(std::string_view label, QAction& action, const Names& selection)
    {
        m_window->select(selection);
        states("before");
        action.trigger();
        line("  " + std::string(label) + " " + m_window->selectionNames() + ": hidden=" + hidden() +
             " " + undoText(m_window->document()));
        states("after");
    }

    /// Show all triggered.
    void showAll()
    {
        m_window->actions().showAllComponentsAction()->trigger();
        line("  show all: hidden=" + hidden());
    }

    /// The hidden components, the Rocket first, in pre-order.
    [[nodiscard]] std::string hidden() const
    {
        std::vector<RocketComponent*> hiddenOnes;
        std::ranges::copy_if(preOrder(m_window->rocket()), std::back_inserter(hiddenOnes),
                             [](const RocketComponent* c) { return !c->isVisible(); });
        return namesOf(hiddenOnes);
    }

    [[nodiscard]] const Lines& lines() const noexcept { return m_lines; }

private:
    const Window* m_window;
    Lines         m_lines;
};

/// T4Probe's "== visibility": components that are not stages.
[[nodiscard]] Lines visibilityOfComponents()
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& a = window.actions();
    VisibilityTranscript t(window);
    t.line("== visibility");
    t.line("-- initial");
    t.states("initial");
    window.select({"BT1"});
    t.states("selected");
    t.line("-- hide [BT1] with the menu action, then show it");
    t.toggle("menu", *a.toggleVisibilityAction(), {"BT1"});
    t.toggle("menu", *a.toggleVisibilityAction(), {"BT1"});
    t.line("-- hide [IT, EB] with the context action");
    t.toggle("context", *a.toggleVisibilityContextMenuAction(), {"IT", "EB"});
    t.line("-- [IT, NC]: one hidden, one visible");
    t.toggle("menu", *a.toggleVisibilityAction(), {"IT", "NC"});
    t.line("-- [IT, NC]: both hidden");
    t.toggle("menu", *a.toggleVisibilityAction(), {"IT", "NC"});
    t.line("-- show all");
    window.select({});
    t.states("nothing selected");
    t.showAll();
    t.states("after");
    return t.lines();
}

/// The second half of T4Probe's "== visibility of stages", from "-- [Rocket]".
void visibilityOfTheRocket(const Window& window, VisibilityTranscript& t)
{
    const RocketActions& a = window.actions();
    t.line("-- [Rocket]");
    t.toggle("menu", *a.toggleVisibilityAction(), {"Rocket"});
    t.toggle("context", *a.toggleVisibilityContextMenuAction(), {"Rocket"});
    t.line("-- [Rocket, BT1] with BT1 hidden");
    t.toggle("menu", *a.toggleVisibilityAction(), {"BT1"});
    t.toggle("menu", *a.toggleVisibilityAction(), {"Rocket", "BT1"});
    t.toggle("menu", *a.toggleVisibilityAction(), {"Rocket", "BT1"});
    t.line("-- the Rocket hidden alone");
    // Java's probe hides the Rocket with [Rocket, BT1] selected and refreshes the actions only
    // once nothing is selected; OpenRocket's actions (and these) are refreshed at the change of
    // the rocket, while [Rocket, BT1] would make them "Show all". Selecting nothing first gives
    // the probe's state the way the application comes to it.
    window.select({});
    window.rocket().setVisible(false);
    t.states("nothing selected");
    window.select({"NC"});
    t.states("selected");
    t.showAll();
}

/// T4Probe's "== visibility of stages": stages, booster sets and the Rocket.
[[nodiscard]] Lines visibilityOfStages()
{
    const ProbeDesign    design;
    const Window         window(design.document());
    const RocketActions& a    = window.actions();
    QAction&             menu = *a.toggleVisibilityAction();
    VisibilityTranscript t(window);
    t.line("== visibility of stages");
    t.line("-- [Sustainer]");
    t.toggle("menu", menu, {"Sustainer"});
    t.toggle("menu", menu, {"Sustainer"});
    t.line("-- [Side boosters] (a booster set is a stage)");
    t.toggle("menu", menu, {"Side boosters"});
    t.toggle("menu", menu, {"Side boosters"});
    t.line("-- [PBT] (the only component of its booster set)");
    t.toggle("menu", menu, {"PBT"});
    t.toggle("menu", menu, {"PBT"});
    t.line("-- every component of the Sustainer, one by one");
    t.toggle("menu", menu, {"NC", "BT1", "IT", "EB", "CR1", "Fins", "BT2", "Chute"});
    t.line("-- [Sustainer, NC]: a stage and a component");
    t.toggle("menu", menu, {"Sustainer", "NC"});
    t.line("-- every component but the Rocket, one by one");
    t.toggle(
        "menu", menu,
        {"NC", "BT1", "IT", "EB", "CR1", "Fins", "BT2", "Chute", "BBT", "Side boosters", "PBT"});
    t.line("-- every component but the Rocket");
    const Names all{"Sustainer", "NC",  "BT1",           "IT", "EB", "CR1", "Fins", "BT2", "Chute",
                    "Booster",   "BBT", "Side boosters", "PBT"};
    t.toggle("menu", menu, all);
    t.toggle("menu", menu, all);
    visibilityOfTheRocket(window, t);
    return t.lines();
}

// T4Probe.out, "== visibility".
constexpr std::string_view kVisibilityTranscript = R"(== visibility
-- initial
  initial []: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    context 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  selected [BT1]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=true
-- hide [BT1] with the menu action, then show it
  before [BT1]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=true
  menu [BT1]: hidden=[BT1] events=3 undo='null'
  after [BT1]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
  before [BT1]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
  menu [BT1]: hidden=[] events=3 undo='null'
  after [BT1]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=true
-- hide [IT, EB] with the context action
  before [IT, EB]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=true
  context [IT, EB]: hidden=[IT, EB] events=6 undo='null'
  after [IT, EB]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
-- [IT, NC]: one hidden, one visible
  before [IT, NC]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
  menu [IT, NC]: hidden=[NC, IT, EB] events=6 undo='null'
  after [IT, NC]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
-- [IT, NC]: both hidden
  before [IT, NC]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
  menu [IT, NC]: hidden=[EB] events=6 undo='null'
  after [IT, NC]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
-- show all
  nothing selected []: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=true
  show all: hidden=[]
  after []: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=false
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=true)";

// T4Probe.out, "== visibility of stages", but its last line: the probe calls Show all directly
// while it is disabled (only the Rocket is hidden, and the action asks for a hidden component
// below it), and Java's action shows the Rocket; a disabled QAction does nothing, as Java's
// action in OpenRocket's menu cannot be chosen either (the probe printed "hidden=[]").
constexpr std::string_view kStageVisibilityTranscript = R"(== visibility of stages
-- [Sustainer]
  before [Sustainer]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [Sustainer]: hidden=[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute] events=9 undo='null'
  after [Sustainer]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Sustainer]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Sustainer]: hidden=[] events=9 undo='null'
  after [Sustainer]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- [Side boosters] (a booster set is a stage)
  before [Side boosters]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [Side boosters]: hidden=[Side boosters, PBT] events=2 undo='null'
  after [Side boosters]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Side boosters]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Side boosters]: hidden=[] events=2 undo='null'
  after [Side boosters]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- [PBT] (the only component of its booster set)
  before [PBT]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [PBT]: hidden=[Side boosters, PBT] events=3 undo='null'
  after [PBT]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [PBT]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [PBT]: hidden=[] events=3 undo='null'
  after [PBT]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- every component of the Sustainer, one by one
  before [NC, BT1, IT, EB, CR1, Fins, BT2, Chute]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [NC, BT1, IT, EB, CR1, Fins, BT2, Chute]: hidden=[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute] events=24 undo='null'
  after [NC, BT1, IT, EB, CR1, Fins, BT2, Chute]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
-- [Sustainer, NC]: a stage and a component
  before [Sustainer, NC]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Sustainer, NC]: hidden=[] events=12 undo='null'
  after [Sustainer, NC]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- every component but the Rocket, one by one
  before [NC, BT1, IT, EB, CR1, Fins, BT2, Chute, BBT, Side boosters, PBT]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [NC, BT1, IT, EB, CR1, Fins, BT2, Chute, BBT, Side boosters, PBT]: hidden=[Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, BBT, Side boosters, PBT] events=32 undo='null'
  after [NC, BT1, IT, EB, CR1, Fins, BT2, Chute, BBT, Side boosters, PBT]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
-- every component but the Rocket
  before [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: hidden=[Rocket, Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT] events=14 undo='null'
  after [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: hidden=[] events=14 undo='null'
  after [Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- [Rocket]
  before [Rocket]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [Rocket]: hidden=[Rocket, Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT] events=14 undo='null'
  after [Rocket]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Rocket]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  context [Rocket]: hidden=[] events=14 undo='null'
  after [Rocket]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- [Rocket, BT1] with BT1 hidden
  before [BT1]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  menu [BT1]: hidden=[BT1] events=3 undo='null'
  after [BT1]: menu 'Show selected' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show selected component(s).' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Rocket, BT1]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Rocket, BT1]: hidden=[Rocket, Sustainer, NC, BT1, IT, EB, CR1, Fins, BT2, Chute, Booster, BBT, Side boosters, PBT] events=14 undo='null'
  after [Rocket, BT1]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  before [Rocket, BT1]: menu 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    context 'Show' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=true deselect enabled=false
  menu [Rocket, BT1]: hidden=[] events=14 undo='null'
  after [Rocket, BT1]: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
-- the Rocket hidden alone
  nothing selected []: menu 'Hide all' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=false
    context 'Hide' ttip='Hide all components.' icon=COMPONENT_HIDDEN enabled=false
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  selected [NC]: menu 'Hide selected' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    context 'Hide' ttip='Hide selected component(s).' icon=COMPONENT_HIDDEN enabled=true
    show all 'Show all' ttip='Show all components.' icon=COMPONENT_SHOWING enabled=false deselect enabled=false
  show all: hidden=[Rocket])";

TEST_F(RocketActionsTest, HideAndShowComponentsAsOpenRocketDoes)
{
    EXPECT_EQ(
        lineDifferences(visibilityOfComponents(), comparedVisibilityLines(kVisibilityTranscript)),
        Lines{});
}

TEST_F(RocketActionsTest, HideAndShowStagesAndTheRocketAsOpenRocketDoes)
{
    EXPECT_EQ(
        lineDifferences(visibilityOfStages(), comparedVisibilityLines(kStageVisibilityTranscript)),
        Lines{});
}

TEST_F(RocketActionsTest, VisibilityChangesFireOneEventAndAddNoUndoPosition)
{
    const ProbeDesign design;
    const Window      window(design.document());
    EventRecorder     events(design.rocket());
    const std::size_t states = design.document().getUndoDetail().descriptions.size();
    window.select({"IT", "NC"});
    const std::vector<int> oneGraphicChange{ComponentChangeEvent::kGraphicChange};
    window.actions().toggleVisibilityAction()->trigger();
    // Java fires one event for each setVisible(): six here.
    EXPECT_EQ(events.takeTypes(), oneGraphicChange);
    window.actions().showAllComponentsAction()->trigger();
    EXPECT_EQ(events.takeTypes(), oneGraphicChange);
    window.select({"Sustainer"});
    window.actions().toggleVisibilityContextMenuAction()->trigger();
    EXPECT_EQ(events.takeTypes(), oneGraphicChange);
    EXPECT_EQ(design.document().getUndoDetail().descriptions.size(), states);
    EXPECT_FALSE(design.rocket().isFrozen());
}

TEST_F(RocketActionsTest, VisibilityActionsLeaveAFrozenRocketFrozen)
{
    const ProbeDesign design;
    const Window      window(design.document());
    EventRecorder     events(design.rocket());
    window.select({"BT1"});
    design.rocket().freeze();
    window.actions().toggleVisibilityAction()->trigger();
    EXPECT_TRUE(design.rocket().isFrozen());
    EXPECT_EQ(events.takeTypes(), std::vector<int>{});
    design.rocket().thaw();
    EXPECT_EQ(events.takeTypes(), std::vector<int>{ComponentChangeEvent::kGraphicChange});
    EXPECT_FALSE(design.find("BT1").isVisible());
}

// ========================================================================== Disable / Enable

/// T4Probe's stage sections run through the actions: Disable/Enable's state for selections, and
/// the stages, events and undo history after each toggle.
class StageTranscript
{
public:
    explicit StageTranscript(const Window& window)
      : m_window(&window), m_events(window.rocket()) { }

    void line(std::string text) { m_lines.push_back(std::move(text)); }

    /// The active stages of the selected configuration, in stage number order.
    [[nodiscard]] std::string activeStages() const
    {
        const FlightConfiguration& configuration = m_window->rocket().getSelectedConfiguration();
        std::string                text          = "[";
        bool                       firstStage    = true;
        for (const AxialStage* stage : configuration.getAllStages())
        {
            text += (firstStage ? "" : ", ") + stage->getName() + "=" +
                    javaBool(configuration.isStageActive(stage->getStageNumber()));
            firstStage = false;
        }
        return text + "]";
    }

    /// stageState(): the action's state for @p selection and whether the popup shows it.
    void stageState(const Names& selection)
    {
        m_window->select(selection);
        m_window->actions().prepareContextMenu();
        const QAction* const action = m_window->actions().toggleActiveAction();
        line("  " + m_window->selectionNames() + ": " + state(*action) + " popup visible=" +
             javaBool(m_window->actions().contextMenu()->actions().contains(action)));
    }

    /// toggleStages(): the state, then the action triggered.
    void toggleStages(const Names& selection)
    {
        stageState(selection);
        static_cast<void>(m_events.take());
        const OpenRocketDocument& document  = m_window->document();
        const std::string         before    = document.getUndoDescription().value_or("null");
        const bool                available = document.isUndoAvailable();
        m_window->actions().toggleActiveAction()->trigger();
        line("  toggled " + m_window->selectionNames() + ": " + activeStages() +
             " events=" + m_events.take() + " " + undoText(document) + " (before '" + before +
             "', available before " + (available ? "1" : "0") + " after " +
             (document.isUndoAvailable() ? "1" : "0") + ")");
    }

    [[nodiscard]] const Lines& lines() const noexcept { return m_lines; }

private:
    const Window* m_window;
    EventRecorder m_events;
    Lines         m_lines;
};

/// T4Probe's "== stages" up to the error case.
[[nodiscard]] Lines stageToggles()
{
    const ProbeDesign design;
    auto              empty = std::make_unique<AxialStage>();
    empty->setName("Empty");
    design.rocket().addChild(std::move(empty));
    const Window    window(design.document());
    StageTranscript t(window);
    t.line("== stages");
    t.line("-- states: " + t.activeStages());
    for (const Names& selection :
         {Names{}, Names{"NC"}, Names{"Sustainer"}, Names{"Booster"}, Names{"Side boosters"},
          Names{"Empty"}, Names{"Sustainer", "Booster"}, Names{"Sustainer", "Empty"},
          Names{"Sustainer", "NC"}, Names{"Rocket"},
          Names{"Sustainer", "Booster", "Side boosters"}})
    {
        t.stageState(selection);
    }
    t.line("-- disable [Booster]");
    t.toggleStages({"Booster"});
    t.stageState({"Sustainer"});
    t.stageState({"Booster"});
    t.stageState({"Side boosters"});
    t.stageState({"Booster", "Sustainer"});
    t.line("-- enable [Side boosters]");
    t.toggleStages({"Side boosters"});
    t.stageState({"Booster"});
    t.stageState({"Sustainer"});
    t.line("-- disable [Side boosters]");
    t.toggleStages({"Side boosters"});
    t.line("-- enable [Booster, Side boosters]");
    t.toggleStages({"Booster", "Side boosters"});
    t.line("-- disable [Sustainer, Booster] (the last active stage)");
    t.stageState({"Sustainer", "Booster"});
    t.line("-- disable [Sustainer, NC]");
    t.toggleStages({"Sustainer", "NC"});
    t.stageState({"Sustainer"});
    t.stageState({"Booster"});
    t.line("-- enable [Sustainer]");
    t.toggleStages({"Sustainer"});
    return t.lines();
}

/// A new design whose stage holds BT, a body tube holding Outer, a booster set with OBT, a body
/// tube holding Inner, a booster set with IBT (T4Probe's errorCase()).
[[nodiscard]] std::unique_ptr<OpenRocketDocument> nestedBoosterSets()
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    RocketComponent*                    parent   = &document->getRocket().getChild(0);
    for (const std::string_view name : {"BT", "Outer", "OBT", "Inner", "IBT"})
    {
        std::unique_ptr<RocketComponent> component;
        if (name == "Outer" || name == "Inner")
        {
            component = std::make_unique<ParallelStage>();
        }
        else
        {
            component = std::make_unique<BodyTube>();
        }
        component->setName(name);
        parent = &parent->addChild(std::move(component));
    }
    return document;
}

/// T4Probe's "== stage toggles to an active stage below two inactive ones".
[[nodiscard]] Lines stageTogglesToAnUnsupportedSelection(OpenRocketDocument& document)
{
    const Window    window(document);
    StageTranscript t(window);
    t.line("== stage toggles to an active stage below two inactive ones");
    t.line("-- states: " + t.activeStages());
    t.toggleStages({"Sustainer"});
    t.toggleStages({"Inner"});
    t.stageState({"Sustainer"});
    t.stageState({"Outer"});
    t.stageState({"Inner"});
    return t.lines();
}

// T4Probe.out, "== stages".
constexpr std::string_view kStageTranscript = R"(== stages
-- states: [Sustainer=true, Booster=true, Side boosters=true, Empty=false]
  []: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=false popup visible=false
  [NC]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=false popup visible=false
  [Sustainer]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  [Booster]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  [Side boosters]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  [Empty]: 'Disable' ttip='Cannot disable a stage with no components.' icon=COMPONENT_DISABLED enabled=false popup visible=true
  [Sustainer, Booster]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  [Sustainer, Empty]: 'Disable' ttip='Cannot disable a stage with no components.' icon=COMPONENT_DISABLED enabled=false popup visible=true
  [Sustainer, NC]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=false
  [Rocket]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=false popup visible=false
  [Sustainer, Booster, Side boosters]: 'Disable' ttip='Cannot disable the only active stage.' icon=COMPONENT_DISABLED enabled=false popup visible=true
-- disable [Booster]
  [Booster]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  toggled [Booster]: [Sustainer=true, Booster=false, Side boosters=false, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
  [Sustainer]: 'Disable' ttip='Cannot disable the only active stage.' icon=COMPONENT_DISABLED enabled=false popup visible=true
  [Booster]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Side boosters]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Booster, Sustainer]: 'Disable' ttip='Cannot disable the only active stage.' icon=COMPONENT_DISABLED enabled=false popup visible=true
-- enable [Side boosters]
  [Side boosters]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  toggled [Side boosters]: [Sustainer=true, Booster=false, Side boosters=true, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
  [Booster]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Sustainer]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
-- disable [Side boosters]
  [Side boosters]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  toggled [Side boosters]: [Sustainer=true, Booster=false, Side boosters=false, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
-- enable [Booster, Side boosters]
  [Booster, Side boosters]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  toggled [Booster, Side boosters]: [Sustainer=true, Booster=true, Side boosters=true, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
-- disable [Sustainer, Booster] (the last active stage)
  [Sustainer, Booster]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
-- disable [Sustainer, NC]
  [Sustainer, NC]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=false
  toggled [Sustainer, NC]: [Sustainer=false, Booster=true, Side boosters=true, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
  [Sustainer]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Booster]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
-- enable [Sustainer]
  [Sustainer]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  toggled [Sustainer]: [Sustainer=true, Booster=true, Side boosters=true, Empty=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1))";

// T4Probe.out, "== stage toggles to an active stage below two inactive ones".
constexpr std::string_view kUnsupportedStagesTranscript =
    R"(== stage toggles to an active stage below two inactive ones
-- states: [Sustainer=true, Outer=true, Inner=true]
  [Sustainer]: 'Disable' ttip='Disable selected component(s) in the current flight configuration.' icon=COMPONENT_DISABLED enabled=true popup visible=true
  toggled [Sustainer]: [Sustainer=false, Outer=false, Inner=false] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
  [Inner]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  toggled [Inner]: [Sustainer=false, Outer=false, Inner=true] events=[ComponentChangeEvent[mass,aero,treechild,motor]] undo='null' (before 'null', available before 1 after 1)
  [Sustainer]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Outer]: 'Enable' ttip='Enable selected stage(s) in the current flight configuration.' icon=COMPONENT_ENABLED enabled=true popup visible=true
  [Inner]: 'Disable' ttip='Cannot disable the only active stage.' icon=COMPONENT_DISABLED enabled=false popup visible=true)";

TEST_F(RocketActionsTest, DisableAndEnableStagesAsOpenRocketDoes)
{
    EXPECT_EQ(lineDifferences(stageToggles(), linesOf(kStageTranscript)), Lines{});
}

TEST_F(RocketActionsTest, StageTogglesReachASelectionTheForceAnalysisRefusesWithoutAnException)
{
    const RecordingGuardHook                  hook;
    const std::unique_ptr<OpenRocketDocument> document = nestedBoosterSets();
    EXPECT_EQ(lineDifferences(stageTogglesToAnUnsupportedSelection(*document),
                              linesOf(kUnsupportedStagesTranscript)),
              Lines{});
    // Whoever shows the force analysis now shows its Error (ComponentDrag, the tree's tooltips).
    const InMemoryPreferences preferences;
    const Result<double>      cd = ComponentDrag::getOverrideCD(named(document->getRocket(), "IBT"),
                                                                preferences.getDefaultMach());
    ASSERT_FALSE(cd.has_value());
    EXPECT_EQ(cd.error().code, ErrorCode::INVALID_ARGUMENT);
    EXPECT_TRUE(hook.failures().empty());
}

TEST_F(RocketActionsTest, AStageToggleFiresOneEventAndAddsNoUndoPosition)
{
    const ProbeDesign design;
    const Window      window(design.document());
    EventRecorder     events(design.rocket());
    design.document().addUndoPosition("Before");
    const OpenRocketDocument::UndoDetail before = design.document().getUndoDetail();
    window.select({"Booster"});
    window.actions().toggleActiveAction()->trigger();
    EXPECT_EQ(events.take(), "[ComponentChangeEvent[mass,aero,treechild,motor]]");
    const OpenRocketDocument::UndoDetail after = design.document().getUndoDetail();
    EXPECT_EQ(after.descriptions.size(), before.descriptions.size());
    EXPECT_EQ(after.nextDescription, before.nextDescription);
    EXPECT_EQ(design.document().getUndoDescription(), std::optional<std::string>("Before"));
}

// ============================================================================== the popup

/// The names of @p menu's items ("---" for a separator), a submenu by its object name.
[[nodiscard]] Lines itemNames(const QMenu& menu)
{
    Lines names;
    for (const QAction* action : menu.actions())
    {
        if (action->isSeparator())
        {
            names.emplace_back("---");
            continue;
        }
        const QMenu* submenu = action->menu();
        names.push_back(
            QtStrings::toUtf8(submenu != nullptr ? submenu->objectName() : action->objectName()));
    }
    return names;
}

TEST_F(RocketActionsTest, ThePopupMenuHasOpenRocketsItemsOfMilestoneTwoInItsOrder)
{
    const ProbeDesign design;
    const Window      window(design.document());
    RocketActions&    actions = window.actions();
    const QMenu&      popup   = *actions.contextMenu();
    window.select({"Booster"});
    actions.prepareContextMenu();
    EXPECT_EQ(popup.objectName(), QStringLiteral("componentTreePopupMenu"));
    EXPECT_EQ(itemNames(popup),
              (Lines{"editAction", "cutAction", "copyAction", "pasteAction", "duplicateAction",
                     "deleteAction", "---", "componentTreePopupSelectMenu", "---",
                     "toggleVisibilityContextMenuAction", "toggleActiveAction"}));
    const QMenu* select = popup.findChild<QMenu*>(QStringLiteral("componentTreePopupSelectMenu"));
    ASSERT_NE(select, nullptr);
    EXPECT_EQ(select->title(), QStringLiteral("Select"));
    EXPECT_EQ(itemNames(*select), Lines{"deselectAllAction"});
    EXPECT_TRUE(popup.toolTipsVisible());
    EXPECT_EQ(popup.parent(), &window.selection().view());
}

TEST_F(RocketActionsTest, ThePopupShowsDisableAndEnableForStagesOnly)
{
    const ProbeDesign design;
    const Window      window(design.document());
    EXPECT_FALSE(popupShowsTheStageItemFor(window, {}));
    EXPECT_TRUE(popupShowsTheStageItemFor(window, {"Sustainer"}));
    EXPECT_TRUE(popupShowsTheStageItemFor(window, {"Sustainer", "Side boosters"}));
    EXPECT_FALSE(popupShowsTheStageItemFor(window, {"Sustainer", "NC"}));
    EXPECT_FALSE(popupShowsTheStageItemFor(window, {"Rocket"}));
    EXPECT_FALSE(popupShowsTheStageItemFor(window, {"BT1"}));
}

// ============================================================================== lifetimes

TEST_F(RocketActionsTest, TheActionsGoWithTheSelectionAndTheMenusItemsStayHarmless)
{
    const ProbeDesign design;
    Window            window(design.document());
    window.select({"BT1"});
    QAction* const                cut = window.actions().cutAction();
    const QPointer<RocketActions> actions(&window.actions());
    window.destroyView();
    EXPECT_TRUE(actions.isNull());
    cut->setEnabled(true);
    cut->trigger();
    design.find("BT2").setName("Second tube");
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], Second tube[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

TEST_F(RocketActionsTest, TheActionsLeaveTheMenusItemsAloneOnceTheMenusAreGone)
{
    const RecordingGuardHook hook;
    const ProbeDesign        design;
    Window                   window(design.document());
    window.destroyMenus();
    window.select({"BT1"});
    design.find("BT2").setName("Second tube");
    EXPECT_TRUE(window.actions().moveUpAction()->isEnabled());
    EXPECT_EQ(window.actions().cutAction(), nullptr);
    EXPECT_TRUE(hook.failures().empty());
}

}  // namespace

}  // namespace QtRocket::Gui
