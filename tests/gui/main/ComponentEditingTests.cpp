// The logic of the component actions (ComponentEditing): the rules and operations of OpenRocket's
// RocketActions (Cut, Copy, Paste, Duplicate, Delete, Move up and Move down), compared with what
// OpenRocket does, line for line: the transcripts of the M2 component-tree scout's ActionsProbe
// (evidence/ActionsProbe.out) and of ClipboardProbe (Milestone 2 run 2, part T3: more selections,
// orders, paste positions, duplicates, cuts, deletes and moves), both OpenRocket 5f164fd0e run
// headless with JDK 17, on the same design (tree/ProbeDesign.h). The probes call each action's
// method directly, also when the action is disabled; the exception each records at its end
// (parentFrame.selectTab() on the frame the probe has not) is left out of the lines here.

#include "QtRocket/gui/main/ComponentEditing.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QtContainerFwd>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/ComponentCopy.h"
#include "QtRocket/document/DecalImage.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/main/OpenRocketClipboard.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Appearance.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/Decal.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TransitionShape.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/Color.h"
#include "QtRocket/util/Coordinate.h"
#include "QtRocket/util/Error.h"
#include "ScriptedUserPrompts.h"
#include "document/TestAttachments.h"
#include "goldens/GoldenExamples.h"
#include "tree/ProbeDesign.h"
#include "tree/TreeEditingSupport.h"

namespace QtRocket::Gui
{

namespace
{

using Names = std::vector<std::string_view>;
using Test::archiveEntries;
using Test::decalListOf;
using Test::decalProbeArchiveEntries;
using Test::fillUpTo;
using Test::firstBodyComponentWithDecal;
using Test::javaBool;
using Test::linesOf;
using Test::ProbeDesign;
using Test::redoText;
using Test::savedWithoutWarnings;
using Test::scenarioName;
using Test::ScriptedUserPrompts;
using Test::undoText;
using ::testing::IsEmpty;

/// The actions of the probes.
enum class Action
{
    CUT,
    COPY,
    DUPLICATE,
    DELETE,
    MOVE_UP,
    MOVE_DOWN,
};

/// The name of @p action's class in RocketActions, as the probes print it.
[[nodiscard]] std::string actionName(Action action)
{
    switch (action)
    {
        case Action::CUT:
            return "CutAction";
        case Action::COPY:
            return "CopyAction";
        case Action::DUPLICATE:
            return "DuplicateAction";
        case Action::DELETE:
            return "DeleteAction";
        case Action::MOVE_UP:
            return "MoveUpAction";
        case Action::MOVE_DOWN:
            return "MoveDownAction";
    }
    return {};
}

/// The first line of @p text.
[[nodiscard]] std::string firstLine(std::string_view text)
{
    return std::string(text.substr(0, text.find('\n')));
}

/// A design window's component editing (what RocketActions holds): the tree of @p design's document
/// with its selection, the clipboard and the questions.
class Window
{
public:
    Window(const ProbeDesign& design, OpenRocketClipboard& clipboard, ScriptedUserPrompts& prompts)
      : m_design(&design),
        m_model(design.document(), m_preferences),
        m_view(m_model),
        m_selection(makeChild<DocumentSelectionModel>(&m_view)),
        m_editing(*m_selection, clipboard, prompts)
    {
    }

    /// Selects the components @p names in this order (the probes' select()).
    void select(const Names& names) const
    {
        std::vector<RocketComponent*> components;
        components.reserve(names.size());
        for (const std::string_view name : names)
        {
            components.push_back(&m_design->find(name));
        }
        m_selection->setSelectedComponents(components);
    }

    /// The probes' selection().
    [[nodiscard]] std::string selection() const
    {
        return m_design->names(m_selection->selectedComponents());
    }

    /// The probes' enables() (ClipboardProbe's; ActionsProbe adds the Edit action's).
    [[nodiscard]] std::string enables() const
    {
        return "cut=" + javaBool(m_editing.canCut()) + " copy=" + javaBool(m_editing.canCopy()) +
               " paste=" + javaBool(m_editing.canPaste()) +
               " dup=" + javaBool(m_editing.canDuplicate()) +
               " del=" + javaBool(m_editing.canDelete()) +
               " up=" + javaBool(m_editing.canMoveUp()) +
               " down=" + javaBool(m_editing.canMoveDown());
    }

    /// ActionsProbe's enables(): with RocketActions' EditAction, enabled when a component is
    /// selected (EditAction.clipboardChanged()).
    [[nodiscard]] std::string enablesWithEdit() const
    {
        return enables() + " edit=" + javaBool(m_selection->selectedComponent() != nullptr);
    }

    /// Performs @p action.
    bool run(Action action)
    {
        switch (action)
        {
            case Action::CUT:
                return m_editing.cut();
            case Action::COPY:
                return m_editing.copy();
            case Action::DUPLICATE:
                return m_editing.duplicate();
            case Action::DELETE:
                return m_editing.deleteSelection();
            case Action::MOVE_UP:
                return m_editing.moveUp();
            case Action::MOVE_DOWN:
                return m_editing.moveDown();
        }
        return false;
    }

    [[nodiscard]] ComponentEditing&       editing() noexcept { return m_editing; }
    [[nodiscard]] DocumentSelectionModel& selectionModel() const noexcept { return *m_selection; }

private:
    const ProbeDesign*      m_design;
    InMemoryPreferences     m_preferences;
    ComponentTreeModel      m_model;
    ComponentTreeView       m_view;
    DocumentSelectionModel* m_selection;
    ComponentEditing        m_editing;
};

// ======================================================================= ActionsProbe's transcript

/// A scenario of ActionsProbe: the selection, the action, and the selection of the two pastes that
/// follow (none for no paste); the transcript expected, OpenRocket's.
struct ActionsScenario
{
    std::string_view     name;
    Names                selection;
    Action               action = Action::COPY;
    std::optional<Names> pasteOn;
    std::string_view     transcript;
};

std::ostream& operator<<(std::ostream& stream, const ActionsScenario& scenario)
{
    return stream << scenario.name;
}

/// ActionsProbe's scenarios: the transcripts of ActionsProbe.out, but where a comment says
/// otherwise.
///
/// After Cut and Delete the probe's selection model, which belongs to no tree, still names the
/// removed components ("selection=[BT1]"); OpenRocket's tree selects each selected component again,
/// or the nearest of its ancestors that remains (ComponentTreeModel.fireTreeStructureChanged(),
/// UndoRedoSelectionTest), and so does the view here (ComponentTreeView): the selections after a
/// Cut or a Delete are the view's.
[[nodiscard]] const std::vector<ActionsScenario>& actionsScenarios()
{
    static const std::vector<ActionsScenario>
        kScenarios{
            {.name       = "CopyBT1PasteOnBT2",
             .selection  = {"BT1"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT2"},
             .transcript = R"(== copy [BT1], paste on BT2
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1] undo='null'
  paste enabled with [BT2]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[BT1*] undo='Paste Body Tube'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Body Tube')"},
            {.name       = "CopyBT1PasteOnSustainer",
             .selection  = {"BT1"},
             .action     = Action::COPY,
             .pasteOn    = Names{"Sustainer"},
             .transcript = R"(== copy [BT1], paste on Sustainer
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1] undo='null'
  paste enabled with [Sustainer]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[BT1*] undo='Paste Body Tube'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Body Tube')"},
            {.name       = "CopyBT1PasteOnITNowhereFits",
             .selection  = {"BT1"},
             .action     = Action::COPY,
             .pasteOn    = Names{"IT"},
             .transcript = R"(== copy [BT1], paste on IT (nowhere fits)
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1] undo='null'
  paste enabled with [IT]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[] undo='null'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "CopyBT1PasteWithNothingSelected",
             .selection  = {"BT1"},
             .action     = Action::COPY,
             .pasteOn    = Names{},
             .transcript = R"(== copy [BT1], paste with nothing selected
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1] undo='null'
  paste enabled with []: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[] undo='null'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "CopyBT1ITParentAndOneChild",
             .selection  = {"BT1", "IT"},
             .action     = Action::COPY,
             .pasteOn    = Names{"Booster"},
             .transcript = R"(== copy [BT1, IT] (parent and one child)
  enabled with [BT1, IT]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*]]] selection=[BT1, IT] undo='null'
  paste enabled with [Booster]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]], BT1*[IT*[EB*]]]]
  selection=[BT1*] undo='Paste Body Tube'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]], BT1*[IT*[EB*]], BT1*[IT*[EB*]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]], BT1*[IT*[EB*]]]] redo='Paste Body Tube')"},
            {.name       = "CopyITFinsTwoSiblings",
             .selection  = {"IT", "Fins"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT2"},
             .transcript = R"(== copy [IT, Fins] (two siblings)
  enabled with [IT, Fins]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Fins*, IT*[EB*]] selection=[IT, Fins] undo='null'
  paste enabled with [BT2]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Fins*, IT*] undo='Paste components'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]] redo='Paste components')"},
            {.name       = "CopyFinsITTwoSiblingsReverseOrder",
             .selection  = {"Fins", "IT"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT2"},
             .transcript = R"(== copy [Fins, IT] (two siblings, reverse order)
  enabled with [Fins, IT]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Fins*, IT*[EB*]] selection=[Fins, IT] undo='null'
  paste enabled with [BT2]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Fins*, IT*] undo='Paste components'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], Fins*]], Booster[BBT[Side boosters[PBT]]]] redo='Paste components')"},
            {.name       = "CopyNCBT2PasteOnBT1",
             .selection  = {"NC", "BT2"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT1"},
             .transcript = R"(== copy [NC, BT2], paste on BT1
  enabled with [NC, BT2]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT2*[Chute*], NC*] selection=[NC, BT2] undo='null'
  paste enabled with [BT1]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], NC*, BT2*[Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[BT2*, NC*] undo='Paste components'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], NC*, BT2*[Chute*], NC*, BT2*[Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], NC*, BT2*[Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Paste components')"},
            {.name       = "CopyNCChuteDifferentParentsPasteOnBT1",
             .selection  = {"NC", "Chute"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT1"},
             .transcript = R"(== copy [NC, Chute] (different parents), paste on BT1
  enabled with [NC, Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[NC*, Chute*] selection=[NC, Chute] undo='null'
  paste enabled with [BT1]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*], NC*, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[NC*, Chute*] undo='Paste components'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*], NC*[Chute*], NC*, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*], NC*, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Paste components')"},
            {.name       = "CopySustainerPasteOnRocket",
             .selection  = {"Sustainer"},
             .action     = Action::COPY,
             .pasteOn    = Names{"Rocket"},
             .transcript = R"(== copy [Sustainer], paste on Rocket
  enabled with [Sustainer]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]] selection=[Sustainer] undo='null'
  paste enabled with [Rocket]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]]
  selection=[Sustainer*] undo='Paste Stage'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]] redo='Paste Stage')"},
            {.name       = "CopySustainerPasteOnBT1",
             .selection  = {"Sustainer"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT1"},
             .transcript = R"(== copy [Sustainer], paste on BT1
  enabled with [Sustainer]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]] selection=[Sustainer] undo='null'
  paste enabled with [BT1]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[] undo='null'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "CopyRocket",
             .selection  = {"Rocket"},
             .action     = Action::COPY,
             .pasteOn    = Names{"Sustainer"},
             .transcript = R"(== copy [Rocket]
  enabled with [Rocket]: cut=false copy=false paste=false dup=false del=false up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Rocket] undo='null'
  paste enabled with [Sustainer]: false
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Sustainer] undo='null'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "CopyChutePasteOnBT1NCFirstSelectedDecides",
             .selection  = {"Chute"},
             .action     = Action::COPY,
             .pasteOn    = Names{"BT1", "NC"},
             .transcript = R"(== copy [Chute], paste on [BT1, NC] (first selected decides)
  enabled with [Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Chute*] selection=[Chute] undo='null'
  paste enabled with [BT1, NC]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Chute*] undo='Paste Parachute'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*, Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, Chute*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Parachute')"},
            {.name       = "CopyChutePasteOnNCBT1FirstSelectedDecides",
             .selection  = {"Chute"},
             .action     = Action::COPY,
             .pasteOn    = Names{"NC", "BT1"},
             .transcript = R"(== copy [Chute], paste on [NC, BT1] (first selected decides)
  enabled with [Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CopyAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Chute*] selection=[Chute] undo='null'
  paste enabled with [NC, BT1]: true
  paste: tree=Rocket[Sustainer[NC[Chute*], BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Chute*] undo='Paste Parachute'
  paste again: tree=Rocket[Sustainer[NC[Chute*, Chute*], BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC[Chute*], BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Parachute')"},
            {.name       = "CutBT1",
             .selection  = {"BT1"},
             .action     = Action::CUT,
             .pasteOn    = Names{"BT2"},
             .transcript = R"(== cut [BT1]
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CutAction: tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[Sustainer] undo='Cut components'
  paste enabled with [BT2]: true
  paste: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[BT1*] undo='Paste Body Tube'
  paste again: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Body Tube')"},
            {.name       = "CutBT1IT",
             .selection  = {"BT1", "IT"},
             .action     = Action::CUT,
             .pasteOn    = Names{"BT2"},
             .transcript = R"(== cut [BT1, IT]
  enabled with [BT1, IT]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  CutAction: tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*]]] selection=[Sustainer] undo='Cut components'
  paste enabled with [BT2]: true
  paste: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*]]], Booster[BBT[Side boosters[PBT]]]]
  selection=[BT1*] undo='Paste Body Tube'
  paste again: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*]], BT1*[IT*[EB*]]], Booster[BBT[Side boosters[PBT]]]]
  after undo: tree=Rocket[Sustainer[NC, BT2[Chute], BT1*[IT*[EB*]]], Booster[BBT[Side boosters[PBT]]]] redo='Paste Body Tube')"},
            {.name       = "CutBooster",
             .selection  = {"Booster"},
             .action     = Action::CUT,
             .pasteOn    = Names{"Rocket"},
             .transcript = R"(== cut [Booster]
  enabled with [Booster]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  CutAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]
  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]]] selection=[Rocket] undo='Cut components'
  paste enabled with [Rocket]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster*[BBT*[Side boosters*[PBT*]]]]
  selection=[Booster*] undo='Paste Stage'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster*[BBT*[Side boosters*[PBT*]]], Booster*[BBT*[Side boosters*[PBT*]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster*[BBT*[Side boosters*[PBT*]]]] redo='Paste Stage')"},
            // Decision U4: Java deletes the completed selection one by one and throws (Report bug!
            // Component Stage not deletable, IllegalArgumentException) with
            // "tree=Rocket[Sustainer[NC]]"; here the tops go by Delete's rule and the last stage
            // stays, as Delete keeps it.
            {.name       = "CutSustainerBoosterAllStages",
             .selection  = {"Sustainer", "Booster"},
             .action     = Action::CUT,
             .pasteOn    = std::nullopt,
             .transcript = R"(== cut [Sustainer, Booster] (all stages)
  enabled with [Sustainer, Booster]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CutAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Booster*[BBT*[Side boosters*[PBT*]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]] selection=[Rocket, Booster] undo='Cut components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Cut components')"},
            {.name       = "CutPodsBoosterParallelStage",
             .selection  = {"Side boosters"},
             .action     = Action::CUT,
             .pasteOn    = Names{"BBT"},
             .transcript = R"(== cut [Pods-Booster] (parallel stage)
  enabled with [Side boosters]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  CutAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT]]
  clipboard=[Side boosters*[PBT*]] selection=[BBT] undo='Cut components'
  paste enabled with [BBT]: true
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters*[PBT*]]]]
  selection=[Side boosters*] undo='Paste Booster Set'
  paste again: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters*[PBT*], Side boosters*[PBT*]]]]
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters*[PBT*]]]] redo='Paste Booster Set')"},
            {.name       = "DuplicateBT1",
             .selection  = {"BT1"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [BT1]
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1*] undo='Duplicate Body Tube'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate Body Tube')"},
            {.name       = "DuplicateIT",
             .selection  = {"IT"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [IT]
  enabled with [IT]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT*[EB*]], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[IT*[EB*]] selection=[IT*] undo='Duplicate Inner Tube'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate Inner Tube')"},
            {.name       = "DuplicateNCBT2",
             .selection  = {"NC", "BT2"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [NC, BT2]
  enabled with [NC, BT2]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], NC*, BT2*[Chute*]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[NC*, BT2*[Chute*]] selection=[BT2*, NC*] undo='Duplicate components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate components')"},
            {.name       = "DuplicateBT1IT",
             .selection  = {"BT1", "IT"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [BT1, IT]
  enabled with [BT1, IT]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], BT1*[IT*[EB*]]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[BT1*[IT*[EB*]]] selection=[BT1*] undo='Duplicate Body Tube'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate Body Tube')"},
            {.name       = "DuplicateNCITDifferentParents",
             .selection  = {"NC", "IT"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [NC, IT] (different parents)
  enabled with [NC, IT]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT*[EB*]], BT2[Chute], NC*], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[NC*, IT*[EB*]] selection=[IT*, NC*] undo='Duplicate components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate components')"},
            {.name       = "DuplicateITNCDifferentParentsReverse",
             .selection  = {"IT", "NC"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [IT, NC] (different parents, reverse)
  enabled with [IT, NC]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT*[EB*]], BT2[Chute], NC*], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[IT*[EB*], NC*] selection=[NC*, IT*] undo='Duplicate components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate components')"},
            {.name       = "DuplicateBT1ChuteDifferentParents",
             .selection  = {"BT1", "Chute"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [BT1, Chute] (different parents)
  enabled with [BT1, Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, Chute*], BT1*[IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Chute*, BT1*[IT*[EB*], CR1*, Fins*]] selection=[BT1*, Chute*] undo='Duplicate components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate components')"},
            {.name       = "DuplicateSustainer",
             .selection  = {"Sustainer"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [Sustainer]
  enabled with [Sustainer]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]]
  clipboard=[Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]]] selection=[Sustainer*] undo='Duplicate Stage'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Duplicate Stage')"},
            {.name       = "DuplicateRocket",
             .selection  = {"Rocket"},
             .action     = Action::DUPLICATE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== duplicate [Rocket]
  enabled with [Rocket]: cut=false copy=false paste=false dup=false del=false up=false down=false edit=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Rocket] undo='null'
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "DeleteBT1",
             .selection  = {"BT1"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [BT1]
  enabled with [BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  DeleteAction: tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Sustainer] undo='Delete Body Tube'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Delete Body Tube')"},
            {.name       = "DeleteNCBT2IT",
             .selection  = {"NC", "BT2", "IT"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [NC, BT2, IT]
  enabled with [NC, BT2, IT]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  DeleteAction: tree=Rocket[Sustainer[BT1[CR1, Fins]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Sustainer, BT1] undo='Delete components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Delete components')"},
            {.name       = "DeleteBT1ITParentAndChild",
             .selection  = {"BT1", "IT"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [BT1, IT] (parent and child)
  enabled with [BT1, IT]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  DeleteAction: tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Sustainer] undo='Delete components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Delete components')"},
            {.name       = "DeleteBooster",
             .selection  = {"Booster"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [Booster]
  enabled with [Booster]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  DeleteAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]
  clipboard=[] selection=[Rocket] undo='Delete Stage'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Delete Stage')"},
            {.name       = "DeleteSustainerBoosterAllStages",
             .selection  = {"Sustainer", "Booster"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [Sustainer, Booster] (all stages)
  enabled with [Sustainer, Booster]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  DeleteAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Rocket, Booster] undo='Delete components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Delete components')"},
            {.name       = "DeleteRocketNC",
             .selection  = {"Rocket", "NC"},
             .action     = Action::DELETE,
             .pasteOn    = std::nullopt,
             .transcript = R"(== delete [Rocket, NC]
  enabled with [Rocket, NC]: cut=false copy=false paste=false dup=false del=false up=false down=false edit=true
  DeleteAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Rocket, NC] undo='null'
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "MoveUpBT2",
             .selection  = {"BT2"},
             .action     = Action::MOVE_UP,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move up [BT2]
  enabled with [BT2]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  MoveUpAction: tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[BT2] undo='Move Body Tube'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Move Body Tube')"},
            {.name       = "MoveUpCR1Fins",
             .selection  = {"CR1", "Fins"},
             .action     = Action::MOVE_UP,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move up [CR1, Fins]
  enabled with [CR1, Fins]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[CR1, Fins, IT[EB]], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[CR1, Fins] undo='Move components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Move components')"},
            // Move up is disabled here, and the operation does nothing. The probe calls Java's
            // action all the same, which adds the undo position "Move Nose Cone" and fires its
            // TREE_CHANGE: an undo step that changes nothing.
            {.name       = "MoveUpNCFirstChild",
             .selection  = {"NC"},
             .action     = Action::MOVE_UP,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move up [NC] (first child)
  enabled with [NC]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[NC] undo='null'
  after undo: tree=Rocket[Sustainer] redo='null')"},
            // Move up is disabled (IT cannot move), and the operation does nothing; Java's action,
            // called all the same, moves BT2 ("tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB],
            // CR1, Fins]], ...", undo 'Move components').
            {.name       = "MoveUpITBT2ITIsFirstInBT1",
             .selection  = {"IT", "BT2"},
             .action     = Action::MOVE_UP,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move up [IT, BT2] (IT is first in BT1)
  enabled with [IT, BT2]: cut=true copy=true paste=false dup=true del=true up=false down=false edit=true
  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[IT, BT2] undo='null'
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name       = "MoveUpBT1ITParentAndChild",
             .selection  = {"BT1", "IT"},
             .action     = Action::MOVE_UP,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move up [BT1, IT] (parent and child)
  enabled with [BT1, IT]: cut=true copy=true paste=false dup=true del=true up=true down=true edit=true
  MoveUpAction: tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[IT, BT1] undo='Move components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Move components')"},
            {.name       = "MoveDownNCBT1",
             .selection  = {"NC", "BT1"},
             .action     = Action::MOVE_DOWN,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move down [NC, BT1]
  enabled with [NC, BT1]: cut=true copy=true paste=false dup=true del=true up=false down=true edit=true
  MoveDownAction: tree=Rocket[Sustainer[BT2[Chute], NC, BT1[IT[EB], CR1, Fins]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[BT1, NC] undo='Move components'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Move components')"},
            // Move down is disabled here, and the operation does nothing; Java's action, called all
            // the same, makes the empty undo step "Move Body Tube".
            {.name       = "MoveDownBT2LastChild",
             .selection  = {"BT2"},
             .action     = Action::MOVE_DOWN,
             .pasteOn    = std::nullopt,
             .transcript = R"(== move down [BT2] (last child)
  enabled with [BT2]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  MoveDownAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[BT2] undo='null'
  after undo: tree=Rocket[Sustainer] redo='null')"},
            {.name = "MoveUpBooster", .selection = {"Booster"}, .action = Action::MOVE_UP, .pasteOn = std::nullopt, .transcript = R"(== move up [Booster]
  enabled with [Booster]: cut=true copy=true paste=false dup=true del=true up=true down=false edit=true
  MoveUpAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]], Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]
  clipboard=[] selection=[Booster] undo='Move Stage'
  after undo: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]] redo='Move Stage')"},
        };
    return kScenarios;
}

/// The lines of the pastes of an ActionsProbe scenario, appended to @p lines.
void pasteTwice(const ProbeDesign& design, Window& window, const Names& pasteOn,
                std::vector<std::string>& lines)
{
    window.select(pasteOn);
    lines.push_back("  paste enabled with " + window.selection() + ": " +
                    javaBool(window.editing().canPaste()));
    window.editing().paste();
    lines.push_back("  paste: tree=" + design.tree());
    lines.push_back("  selection=" + window.selection() + " " + undoText(design.document()));
    window.editing().paste();
    lines.push_back("  paste again: tree=" + design.tree());
}

/// ActionsProbe's scenario(): the transcript of @p scenario.
[[nodiscard]] std::vector<std::string> run(const ActionsScenario& scenario)
{
    const ProbeDesign        design;
    OpenRocketClipboard      clipboard;
    ScriptedUserPrompts      prompts;
    Window                   window(design, clipboard, prompts);
    std::vector<std::string> lines{firstLine(scenario.transcript)};
    window.select(scenario.selection);
    lines.push_back("  enabled with " + window.selection() + ": " + window.enablesWithEdit());
    window.run(scenario.action);
    lines.push_back("  " + actionName(scenario.action) + ": tree=" + design.tree());
    lines.push_back("  clipboard=" + design.clipboard(clipboard.components()) +
                    " selection=" + window.selection() + " " + undoText(design.document()));
    if (scenario.pasteOn.has_value())
    {
        pasteTwice(design, window, *scenario.pasteOn, lines);
    }
    if (design.document().isUndoAvailable())
    {
        design.document().undo();
        lines.push_back("  after undo: tree=" + design.tree() + " " + redoText(design.document()));
    }
    // No message: nothing here is beyond a bound.
    EXPECT_THAT(prompts.prompts(), IsEmpty());
    return lines;
}

class ActionsProbeTranscript : public Test::GuiTest,
                               public ::testing::WithParamInterface<ActionsScenario>
{ };

TEST_P(ActionsProbeTranscript, IsOpenRocketsTranscript)
{
    EXPECT_EQ(run(GetParam()), linesOf(GetParam().transcript));
}

INSTANTIATE_TEST_SUITE_P(ComponentEditing, ActionsProbeTranscript,
                         ::testing::ValuesIn(actionsScenarios()), scenarioName<ActionsScenario>);

// ===================================================================== ClipboardProbe's transcript

/// A scenario of ClipboardProbe: copy() (a Copy alone, with what it enables), copyPaste() (a Copy,
/// then a Paste on the component pasteOn) or action() (the action alone); the transcript expected,
/// OpenRocket's.
struct ClipboardScenario
{
    std::string_view                name;
    Names                           selection;
    Action                          action = Action::COPY;
    std::optional<std::string_view> pasteOn;
    std::string_view                transcript;
};

std::ostream& operator<<(std::ostream& stream, const ClipboardScenario& scenario)
{
    return stream << scenario.name;
}

/// ClipboardProbe's scenarios: the transcripts of ClipboardProbe.out
/// (probes/m2r2-T3/java/ClipboardProbe.out), but where a comment says otherwise.
///
/// After Cut and Delete the probe's selection model, which belongs to no tree, still names the
/// removed components ("selection=[BT1]"); OpenRocket's tree selects each selected component again,
/// or the nearest of its ancestors that remains (ComponentTreeModel.fireTreeStructureChanged(),
/// UndoRedoSelectionTest), and so does the view here (ComponentTreeView): the selections after a
/// Cut or a Delete are the view's.
[[nodiscard]] const std::vector<ClipboardScenario>& clipboardScenarios()
{
    static const std::vector<ClipboardScenario> kScenarios{
        {.name       = "CopySustainerITAGrandchild",
         .selection  = {"Sustainer", "IT"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Sustainer, IT] (a grandchild)
  enabled with [Sustainer, IT]: cut=true copy=true paste=false dup=true del=true up=false down=true
  copy: clipboard=[Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]])"},
        {.name       = "CopySustainerEBAGreatGrandchild",
         .selection  = {"Sustainer", "EB"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Sustainer, EB] (a great-grandchild)
  enabled with [Sustainer, EB]: cut=true copy=true paste=false dup=true del=true up=false down=true
  copy: clipboard=[Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]])"},
        {.name       = "CopyBT1EBAGrandchild",
         .selection  = {"BT1", "EB"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [BT1, EB] (a grandchild)
  enabled with [BT1, EB]: cut=true copy=true paste=false dup=true del=true up=true down=true
  copy: clipboard=[BT1*[IT*[EB*], CR1*, Fins*]])"},
        {.name       = "CopyITBT1ChildFirst",
         .selection  = {"IT", "BT1"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [IT, BT1] (child first)
  enabled with [IT, BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true
  copy: clipboard=[BT1*[IT*[EB*]]])"},
        {.name       = "CopyBT1ITCR1FinsAllChildren",
         .selection  = {"BT1", "IT", "CR1", "Fins"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [BT1, IT, CR1, Fins] (all children)
  enabled with [BT1, IT, CR1, Fins]: cut=true copy=true paste=false dup=true del=true up=true down=true
  copy: clipboard=[BT1*[IT*[EB*], CR1*, Fins*]])"},
        {.name       = "CopyBT1ITCR1FinsEBEverythingBelow",
         .selection  = {"BT1", "IT", "CR1", "Fins", "EB"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [BT1, IT, CR1, Fins, EB] (everything below)
  enabled with [BT1, IT, CR1, Fins, EB]: cut=true copy=true paste=false dup=true del=true up=true down=true
  copy: clipboard=[BT1*[IT*[EB*], CR1*, Fins*]])"},
        {.name       = "CopySustainerBT1EBAChildAndAGrandchildOfIt",
         .selection  = {"Sustainer", "BT1", "EB"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Sustainer, BT1, EB] (a child and a grandchild of it)
  enabled with [Sustainer, BT1, EB]: cut=true copy=true paste=false dup=true del=true up=false down=true
  copy: clipboard=[Sustainer*[BT1*[IT*[EB*], CR1*, Fins*]]])"},
        {.name       = "CopyBoosterPBTThroughABoosterSet",
         .selection  = {"Booster", "PBT"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Booster, PBT] (through a booster set)
  enabled with [Booster, PBT]: cut=true copy=true paste=false dup=true del=true up=true down=false
  copy: clipboard=[Booster*[BBT*[Side boosters*[PBT*]]]])"},
        {.name       = "CopyChuteBT1NC",
         .selection  = {"Chute", "BT1", "NC"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Chute, BT1, NC]
  enabled with [Chute, BT1, NC]: cut=true copy=true paste=false dup=true del=true up=false down=false
  copy: clipboard=[BT1*[IT*[EB*], CR1*, Fins*], Chute*, NC*])"},
        {.name       = "CopyFinsCR1IT",
         .selection  = {"Fins", "CR1", "IT"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [Fins, CR1, IT]
  enabled with [Fins, CR1, IT]: cut=true copy=true paste=false dup=true del=true up=false down=false
  copy: clipboard=[Fins*, CR1*, IT*[EB*]])"},
        {.name       = "CopyEBBT2DifferentDepths",
         .selection  = {"EB", "BT2"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [EB, BT2] (different depths)
  enabled with [EB, BT2]: cut=true copy=true paste=false dup=true del=true up=false down=false
  copy: clipboard=[BT2*[Chute*], EB*])"},
        {.name       = "CopyBT2BoosterNC",
         .selection  = {"BT2", "Booster", "NC"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [BT2, Booster, NC]
  enabled with [BT2, Booster, NC]: cut=true copy=true paste=false dup=true del=true up=false down=false
  copy: clipboard=[BT2*[Chute*], Booster*[BBT*[Side boosters*[PBT*]]], NC*])"},
        {.name       = "CopyNCRocketTheRocketSecond",
         .selection  = {"NC", "Rocket"},
         .action     = Action::COPY,
         .pasteOn    = std::nullopt,
         .transcript = R"(== copy [NC, Rocket] (the Rocket second)
  enabled with [NC, Rocket]: cut=false copy=true paste=false dup=true del=true up=false down=false
  copy: clipboard=[])"},
        {.name       = "CopyFinsCR1ITPasteOnBT2",
         .selection  = {"Fins", "CR1", "IT"},
         .action     = Action::COPY,
         .pasteOn    = "BT2",
         .transcript = R"(== copy [Fins, CR1, IT], paste on BT2
  copy: clipboard=[Fins*, CR1*, IT*[EB*]]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT*[EB*], CR1*, Fins*]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Fins*, CR1*, IT*] undo='Paste components')"},
        {.name       = "CopyFinsITPasteOnNC",
         .selection  = {"Fins", "IT"},
         .action     = Action::COPY,
         .pasteOn    = "NC",
         .transcript = R"(== copy [Fins, IT], paste on NC
  copy: clipboard=[Fins*, IT*[EB*]]
  paste: tree=Rocket[Sustainer[NC[IT*[EB*]], BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[IT*] undo='Paste components')"},
        {.name       = "CopyChutePasteOnBooster",
         .selection  = {"Chute"},
         .action     = Action::COPY,
         .pasteOn    = "Booster",
         .transcript = R"(== copy [Chute], paste on Booster
  copy: clipboard=[Chute*]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[] undo='null')"},
        {.name       = "CopyBT2PasteOnBooster",
         .selection  = {"BT2"},
         .action     = Action::COPY,
         .pasteOn    = "Booster",
         .transcript = R"(== copy [BT2], paste on Booster
  copy: clipboard=[BT2*[Chute*]]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]], BT2*[Chute*]]]
  selection=[BT2*] undo='Paste Body Tube')"},
        {.name       = "CopyBT2PasteOnPBT",
         .selection  = {"BT2"},
         .action     = Action::COPY,
         .pasteOn    = "PBT",
         .transcript = R"(== copy [BT2], paste on PBT
  copy: clipboard=[BT2*[Chute*]]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT, BT2*[Chute*]]]]]
  selection=[BT2*] undo='Paste Body Tube')"},
        {.name       = "CopyNCChutePasteOnEB",
         .selection  = {"NC", "Chute"},
         .action     = Action::COPY,
         .pasteOn    = "EB",
         .transcript = R"(== copy [NC, Chute], paste on EB
  copy: clipboard=[NC*, Chute*]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB, Chute*], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Chute*] undo='Paste components')"},
        {.name       = "CopySideBoostersPasteOnBT2",
         .selection  = {"Side boosters"},
         .action     = Action::COPY,
         .pasteOn    = "BT2",
         .transcript = R"(== copy [Side boosters], paste on BT2
  copy: clipboard=[Side boosters*[PBT*]]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, Side boosters*[PBT*]]], Booster[BBT[Side boosters[PBT]]]]
  selection=[Side boosters*] undo='Paste Booster Set')"},
        {.name       = "CopySustainerITPasteOnRocket",
         .selection  = {"Sustainer", "IT"},
         .action     = Action::COPY,
         .pasteOn    = "Rocket",
         .transcript = R"(== copy [Sustainer, IT], paste on Rocket
  copy: clipboard=[Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]]
  paste: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]]
  selection=[Sustainer*] undo='Paste Stage')"},
        {.name       = "DuplicateFinsIT",
         .selection  = {"Fins", "IT"},
         .action     = Action::DUPLICATE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== duplicate [Fins, IT]
  enabled with [Fins, IT]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins, IT*[EB*], Fins*], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[IT*[EB*], Fins*] selection=[Fins*, IT*] undo='Duplicate components')"},
        {.name       = "DuplicateChuteNC",
         .selection  = {"Chute", "NC"},
         .action     = Action::DUPLICATE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== duplicate [Chute, NC]
  enabled with [Chute, NC]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, Chute*], NC*], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Chute*, NC*] selection=[NC*, Chute*] undo='Duplicate components')"},
        {.name       = "DuplicateSustainerIT",
         .selection  = {"Sustainer", "IT"},
         .action     = Action::DUPLICATE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== duplicate [Sustainer, IT]
  enabled with [Sustainer, IT]: cut=true copy=true paste=false dup=true del=true up=false down=true
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]], Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]]
  clipboard=[Sustainer*[NC*, BT1*[IT*[EB*]], BT2*[Chute*]]] selection=[Sustainer*] undo='Duplicate Stage')"},
        {.name       = "DuplicateNCRocket",
         .selection  = {"NC", "Rocket"},
         .action     = Action::DUPLICATE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== duplicate [NC, Rocket]
  enabled with [NC, Rocket]: cut=false copy=true paste=false dup=true del=true up=false down=false
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[NC, Rocket] undo='null')"},
        {.name       = "DuplicateSideBoosters",
         .selection  = {"Side boosters"},
         .action     = Action::DUPLICATE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== duplicate [Side boosters]
  enabled with [Side boosters]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DuplicateAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT], Side boosters*[PBT*]]]]
  clipboard=[Side boosters*[PBT*]] selection=[Side boosters*] undo='Duplicate Booster Set')"},
        {.name       = "CutCR1",
         .selection  = {"CR1"},
         .action     = Action::CUT,
         .pasteOn    = std::nullopt,
         .transcript = R"(== cut [CR1]
  enabled with [CR1]: cut=true copy=true paste=false dup=true del=true up=true down=true
  CutAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[CR1*] selection=[BT1] undo='Cut Centering Ring')"},
        {.name       = "CutITChute",
         .selection  = {"IT", "Chute"},
         .action     = Action::CUT,
         .pasteOn    = std::nullopt,
         .transcript = R"(== cut [IT, Chute]
  enabled with [IT, Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false
  CutAction: tree=Rocket[Sustainer[NC, BT1[CR1, Fins], BT2], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[IT*[EB*], Chute*] selection=[BT1, BT2] undo='Cut components')"},
        {.name       = "CutFinsNC",
         .selection  = {"Fins", "NC"},
         .action     = Action::CUT,
         .pasteOn    = std::nullopt,
         .transcript = R"(== cut [Fins, NC]
  enabled with [Fins, NC]: cut=true copy=true paste=false dup=true del=true up=false down=false
  CutAction: tree=Rocket[Sustainer[BT1[IT[EB], CR1], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[Fins*, NC*] selection=[BT1, Sustainer] undo='Cut components')"},
        {.name       = "CutNCRocket",
         .selection  = {"NC", "Rocket"},
         .action     = Action::CUT,
         .pasteOn    = std::nullopt,
         .transcript = R"(== cut [NC, Rocket]
  enabled with [NC, Rocket]: cut=false copy=true paste=false dup=true del=true up=false down=false
  CutAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[NC, Rocket] undo='null')"},
        {.name       = "DeleteITBT1ChildFirst",
         .selection  = {"IT", "BT1"},
         .action     = Action::DELETE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== delete [IT, BT1] (child first)
  enabled with [IT, BT1]: cut=true copy=true paste=false dup=true del=true up=true down=true
  DeleteAction: tree=Rocket[Sustainer[NC, BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Sustainer] undo='Delete components')"},
        {.name       = "DeleteFinsNCChute",
         .selection  = {"Fins", "NC", "Chute"},
         .action     = Action::DELETE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== delete [Fins, NC, Chute]
  enabled with [Fins, NC, Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DeleteAction: tree=Rocket[Sustainer[BT1[IT[EB], CR1], BT2], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[BT1, Sustainer, BT2] undo='Delete components')"},
        {.name       = "DeleteBoosterSustainerAllStagesReverse",
         .selection  = {"Booster", "Sustainer"},
         .action     = Action::DELETE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== delete [Booster, Sustainer] (all stages, reverse)
  enabled with [Booster, Sustainer]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DeleteAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Booster, Rocket] undo='Delete components')"},
        {.name       = "DeleteSideBoosters",
         .selection  = {"Side boosters"},
         .action     = Action::DELETE,
         .pasteOn    = std::nullopt,
         .transcript = R"(== delete [Side boosters]
  enabled with [Side boosters]: cut=true copy=true paste=false dup=true del=true up=false down=false
  DeleteAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT]]
  clipboard=[] selection=[BBT] undo='Delete Booster Set')"},
        {.name       = "MoveDownCR1IT",
         .selection  = {"CR1", "IT"},
         .action     = Action::MOVE_DOWN,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move down [CR1, IT]
  enabled with [CR1, IT]: cut=true copy=true paste=false dup=true del=true up=false down=true
  MoveDownAction: tree=Rocket[Sustainer[NC, BT1[Fins, IT[EB], CR1], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[CR1, IT] undo='Move components')"},
        {.name       = "MoveUpFinsCR1",
         .selection  = {"Fins", "CR1"},
         .action     = Action::MOVE_UP,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move up [Fins, CR1]
  enabled with [Fins, CR1]: cut=true copy=true paste=false dup=true del=true up=true down=false
  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[CR1, Fins, IT[EB]], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[CR1, Fins] undo='Move components')"},
        {.name       = "MoveDownSustainer",
         .selection  = {"Sustainer"},
         .action     = Action::MOVE_DOWN,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move down [Sustainer]
  enabled with [Sustainer]: cut=true copy=true paste=false dup=true del=true up=false down=true
  MoveDownAction: tree=Rocket[Booster[BBT[Side boosters[PBT]]], Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]
  clipboard=[] selection=[Sustainer] undo='Move Stage')"},
        // Move up is disabled (IT cannot move), and the operation does nothing; Java's action,
        // called all the same, moves Fins ("tree=Rocket[Sustainer[NC, BT1[IT[EB], Fins, CR1],
        // BT2[Chute]], ...", selection=[IT, Fins] undo='Move components').
        {.name       = "MoveUpFinsITITFirstInBT1",
         .selection  = {"Fins", "IT"},
         .action     = Action::MOVE_UP,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move up [Fins, IT] (IT first in BT1)
  enabled with [Fins, IT]: cut=true copy=true paste=false dup=true del=true up=false down=false
  MoveUpAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Fins, IT] undo='null')"},
        // Move down is disabled (Chute cannot move), and the operation does nothing; Java's action,
        // called all the same, moves NC ("tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC,
        // BT2[Chute]], ...", undo 'Move components').
        {.name       = "MoveDownNCChute",
         .selection  = {"NC", "Chute"},
         .action     = Action::MOVE_DOWN,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move down [NC, Chute]
  enabled with [NC, Chute]: cut=true copy=true paste=false dup=true del=true up=false down=false
  MoveDownAction: tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[NC, Chute] undo='null')"},
        {.name       = "MoveUpBT2ChuteAParentAndItsOnlyChild",
         .selection  = {"BT2", "Chute"},
         .action     = Action::MOVE_UP,
         .pasteOn    = std::nullopt,
         .transcript = R"(== move up [BT2, Chute] (a parent and its only child)
  enabled with [BT2, Chute]: cut=true copy=true paste=false dup=true del=true up=true down=false
  MoveUpAction: tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], Booster[BBT[Side boosters[PBT]]]]
  clipboard=[] selection=[Chute, BT2] undo='Move components')"},
    };
    return kScenarios;
}

/// ClipboardProbe's copyPaste(): the lines after the label.
void copyAndPaste(const ProbeDesign& design, Window& window, const OpenRocketClipboard& clipboard,
                  std::string_view pasteOn, std::vector<std::string>& lines)
{
    window.editing().copy();
    lines.push_back("  copy: clipboard=" + design.clipboard(clipboard.components()));
    window.select({pasteOn});
    window.editing().paste();
    lines.push_back("  paste: tree=" + design.tree());
    lines.push_back("  selection=" + window.selection() + " " + undoText(design.document()));
}

/// ClipboardProbe's copy() and action(): the lines after the label.
void actAlone(const ProbeDesign& design, Window& window, const OpenRocketClipboard& clipboard,
              Action action, std::vector<std::string>& lines)
{
    lines.push_back("  enabled with " + window.selection() + ": " + window.enables());
    window.run(action);
    if (action == Action::COPY)
    {
        lines.push_back("  copy: clipboard=" + design.clipboard(clipboard.components()));
        return;
    }
    lines.push_back("  " + actionName(action) + ": tree=" + design.tree());
    lines.push_back("  clipboard=" + design.clipboard(clipboard.components()) +
                    " selection=" + window.selection() + " " + undoText(design.document()));
}

/// The transcript of @p scenario.
[[nodiscard]] std::vector<std::string> run(const ClipboardScenario& scenario)
{
    const ProbeDesign        design;
    OpenRocketClipboard      clipboard;
    ScriptedUserPrompts      prompts;
    Window                   window(design, clipboard, prompts);
    std::vector<std::string> lines{firstLine(scenario.transcript)};
    window.select(scenario.selection);
    if (scenario.pasteOn.has_value())
    {
        copyAndPaste(design, window, clipboard, *scenario.pasteOn, lines);
    }
    else
    {
        actAlone(design, window, clipboard, scenario.action, lines);
    }
    EXPECT_THAT(prompts.prompts(), IsEmpty());
    return lines;
}

class ClipboardProbeTranscript : public Test::GuiTest,
                                 public ::testing::WithParamInterface<ClipboardScenario>
{ };

TEST_P(ClipboardProbeTranscript, IsOpenRocketsTranscript)
{
    EXPECT_EQ(run(GetParam()), linesOf(GetParam().transcript));
}

INSTANTIATE_TEST_SUITE_P(ComponentEditing, ClipboardProbeTranscript,
                         ::testing::ValuesIn(clipboardScenarios()),
                         scenarioName<ClipboardScenario>);

// =========================================================== ActionsProbe's scenarios of their own

using ComponentEditingTest = Test::GuiTest;

// ActionsProbe "== delete the only stage": Delete and Cut are disabled for the only stage; the
// probe's direct calls change nothing.
TEST_F(ComponentEditingTest, KeepsTheOnlyStage)
{
    const ProbeDesign   design(ProbeDesign::Booster::NONE);
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"Sustainer"});

    EXPECT_EQ(window.enablesWithEdit(),
              "cut=false copy=true paste=false dup=true del=false up=false "
              "down=false edit=true");
    EXPECT_FALSE(window.editing().deleteSelection());
    EXPECT_EQ(design.tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(undoText(design.document()), "undo='null'");
    window.select({"Sustainer"});
    EXPECT_FALSE(window.editing().cut());
    EXPECT_EQ(design.tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(design.clipboard(clipboard.components()), "[]");
}

// ActionsProbe "== copy [BT1] in one document, paste into another": the clipboard is the
// application's, so a copy pastes into any document.
TEST_F(ComponentEditingTest, PastesIntoAnotherDocument)
{
    const ProbeDesign   first;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              firstWindow(first, clipboard, prompts);
    firstWindow.select({"BT1"});
    ASSERT_TRUE(firstWindow.editing().copy());
    const ProbeDesign second(ProbeDesign::Booster::NONE);
    Window            secondWindow(second, clipboard, prompts);
    secondWindow.select({"Sustainer"});

    EXPECT_TRUE(secondWindow.editing().paste());

    EXPECT_EQ(second.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute], "
              "BT1*[IT*[EB*], CR1*, Fins*]]]");
    // Java prints the first document with the ids of the second as the original ones.
    EXPECT_EQ(second.tree(first.rocket()),
              "Rocket*[Sustainer*[NC*, BT1*[IT*[EB*], CR1*, Fins*], BT2*[Chute*]], "
              "Booster*[BBT*[Side boosters*[PBT*]]]]");
    EXPECT_EQ(first.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// ActionsProbe "== the clipboard holds copies: changing the original after copy does not change the
// paste".
TEST_F(ComponentEditingTest, TheClipboardHoldsCopies)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"BT1"});
    ASSERT_TRUE(window.editing().copy());
    design.find("BT1").setName("BT1 renamed");
    RocketComponent& inner = design.find("IT");
    static_cast<void>(inner.getParent()->removeChild(&inner));
    window.select({"BT2"});

    EXPECT_TRUE(window.editing().paste());

    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1 renamed[CR1, Fins], "
              "BT2[Chute], BT1*[IT*[EB*], CR1*, "
              "Fins*]], Booster[BBT[Side boosters[PBT]]]]");
}

// ======================================================================================== the port

/// The first of @p components that is a T, or null (Java's stream().filter(c -> c instanceof
/// T).findFirst()).
template <class T>
[[nodiscard]] const T* firstOfType(const std::vector<std::unique_ptr<RocketComponent>>& components)
{
    for (const std::unique_ptr<RocketComponent>& component : components)
    {
        if (const auto* found = dynamic_cast<const T*>(component.get()))
        {
            return found;
        }
    }
    return nullptr;
}

// Port of RocketActionsTest.testCopyComponentsMaintainParent (swing
// gui/main/RocketActionsTest.java): four components without a parent come back as four copies with
// the same properties.
TEST_F(ComponentEditingTest, CopyComponentsMaintainParent)
{
    // Create a list of RocketComponent objects
    const auto bodyTube = std::make_unique<BodyTube>(0.5, 0.05);
    const auto noseCone = std::make_unique<NoseCone>(
        TransitionShape::CONICAL, 6 * NoseCone::kDefaultRadius, NoseCone::kDefaultRadius);
    const auto trapezoidFinSet = std::make_unique<TrapezoidFinSet>(3, 0.05, 0.05, 0.025, 0.03);
    const auto parachute       = std::make_unique<Parachute>();
    const std::vector<const RocketComponent*> components{bodyTube.get(), noseCone.get(),
                                                         trapezoidFinSet.get(), parachute.get()};

    // Copy the components
    const std::vector<std::unique_ptr<RocketComponent>> copiedComponents =
        ComponentEditing::copyComponentsMaintainParent(components);

    // Retrieve the copied components by their type
    const auto* copiedBodyTube        = firstOfType<BodyTube>(copiedComponents);
    const auto* copiedNoseCone        = firstOfType<NoseCone>(copiedComponents);
    const auto* copiedTrapezoidFinSet = firstOfType<TrapezoidFinSet>(copiedComponents);
    const auto* copiedParachute       = firstOfType<Parachute>(copiedComponents);

    // Verify that the copied components are not null
    ASSERT_NE(copiedBodyTube, nullptr);
    ASSERT_NE(copiedNoseCone, nullptr);
    ASSERT_NE(copiedTrapezoidFinSet, nullptr);
    ASSERT_NE(copiedParachute, nullptr);

    // Verify that the copied components have the same properties as the original components
    EXPECT_EQ(bodyTube->getLength(), copiedBodyTube->getLength());
    EXPECT_EQ(bodyTube->getOuterRadius(), copiedBodyTube->getOuterRadius());
    EXPECT_EQ(bodyTube->getThickness(), copiedBodyTube->getThickness());
    EXPECT_EQ(bodyTube->isFilled(), copiedBodyTube->isFilled());

    EXPECT_EQ(noseCone->getLength(), copiedNoseCone->getLength());
    EXPECT_EQ(noseCone->getBaseRadius(), copiedNoseCone->getBaseRadius());
    EXPECT_EQ(noseCone->getThickness(), copiedNoseCone->getThickness());
    EXPECT_EQ(noseCone->getShapeType(), copiedNoseCone->getShapeType());

    EXPECT_EQ(trapezoidFinSet->getFinCount(), copiedTrapezoidFinSet->getFinCount());
    EXPECT_EQ(trapezoidFinSet->getRootChord(), copiedTrapezoidFinSet->getRootChord());
    EXPECT_EQ(trapezoidFinSet->getTipChord(), copiedTrapezoidFinSet->getTipChord());
    EXPECT_EQ(trapezoidFinSet->getSweep(), copiedTrapezoidFinSet->getSweep());
    EXPECT_EQ(trapezoidFinSet->getLength(), copiedTrapezoidFinSet->getLength());
    EXPECT_EQ(trapezoidFinSet->getHeight(), copiedTrapezoidFinSet->getHeight());

    EXPECT_EQ(parachute->getDiameter(), copiedParachute->getDiameter());
    EXPECT_EQ(parachute->getMaterial(), copiedParachute->getMaterial());
    EXPECT_EQ(parachute->getLineCount(), copiedParachute->getLineCount());
    EXPECT_EQ(parachute->getLineLength(), copiedParachute->getLineLength());

    // Copies should be distinct objects from originals
    EXPECT_NE(bodyTube.get(), copiedBodyTube);
    EXPECT_NE(noseCone.get(), copiedNoseCone);
    EXPECT_NE(trapezoidFinSet.get(), copiedTrapezoidFinSet);
    EXPECT_NE(parachute.get(), copiedParachute);
}

// The rule copyComponentsMaintainParent() gives, alone: a listed component with listed children
// keeps only those; the others keep everything.
TEST_F(ComponentEditingTest, CopyComponentsMaintainParentKeepsTheListedChildren)
{
    const ProbeDesign design;

    // A completed selection (fillInMissingSelections() leaves no gap between a component and a
    // listed ancestor); BBT's stage is not listed.
    const std::vector<std::unique_ptr<RocketComponent>> copies =
        ComponentEditing::copyComponentsMaintainParent(std::vector<const RocketComponent*>{
            &design.find("Sustainer"), &design.find("BT1"), &design.find("IT"), &design.find("BT2"),
            &design.find("Chute"), &design.find("BBT")});

    ASSERT_EQ(copies.size(), 2U);
    EXPECT_EQ(design.tree(*copies[0]), "Sustainer*[BT1*[IT*[EB*]], BT2*[Chute*]]");
    EXPECT_EQ(design.tree(*copies[1]), "BBT*[Side boosters*[PBT*]]");
}

// fillInMissingSelections(): the selection first, then what Java adds, in Java's order.
TEST_F(ComponentEditingTest, FillsInTheSelectionAsOpenRocket)
{
    const ProbeDesign design;

    EXPECT_EQ(design.names(ComponentEditing::fillInMissingSelections(
                  std::vector<const RocketComponent*>{&design.find("BT1")})),
              "[BT1, IT, CR1, Fins]");
    EXPECT_EQ(
        design.names(ComponentEditing::fillInMissingSelections(
            std::vector<const RocketComponent*>{&design.find("Sustainer"), &design.find("EB")})),
        "[Sustainer, EB, NC, BT1, BT2, IT]");
    EXPECT_EQ(
        design.names(ComponentEditing::fillInMissingSelections(std::vector<const RocketComponent*>{
            &design.find("Sustainer"), &design.find("BT1"), &design.find("EB")})),
        "[Sustainer, BT1, EB, IT, CR1, Fins]");
}

TEST_F(ComponentEditingTest, KnowsWhatIsDeletableAndCopyable)
{
    const ProbeDesign design;
    const ProbeDesign single(ProbeDesign::Booster::NONE);

    EXPECT_FALSE(ComponentEditing::isDeletable(design.rocket()));
    EXPECT_FALSE(ComponentEditing::isCopyable(design.rocket()));
    EXPECT_TRUE(ComponentEditing::isDeletable(design.find("Sustainer")));
    EXPECT_FALSE(ComponentEditing::isDeletable(single.find("Sustainer")));
    EXPECT_TRUE(ComponentEditing::isCopyable(single.find("Sustainer")));
    // A booster set may go even when it is its parent's only child.
    EXPECT_TRUE(ComponentEditing::isDeletable(design.find("Side boosters")));
    // A detached component has no parent to leave.
    const BodyTube detached;
    EXPECT_FALSE(ComponentEditing::isDeletable(detached));
}

/// @p position, or a test failure and none when there is none. (A plain if is what clang-tidy's
/// unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.)
[[nodiscard]] ComponentEditing::PastePosition required(
    const std::optional<ComponentEditing::PastePosition>& position)
{
    if (!position.has_value())
    {
        ADD_FAILURE() << "no paste position";
        return {};
    }
    return *position;
}

TEST_F(ComponentEditingTest, PastesWhereOpenRocketDoes)
{
    const ProbeDesign design;
    const Parachute   chute;
    const BodyTube    tube;

    const ComponentEditing::PastePosition intoTube =
        required(ComponentEditing::pastePosition(chute, &design.find("BT2")));
    const ComponentEditing::PastePosition afterTube =
        required(ComponentEditing::pastePosition(tube, &design.find("BT1")));

    EXPECT_EQ(intoTube.parent, &design.find("BT2"));
    EXPECT_EQ(intoTube.index, 1U);
    EXPECT_EQ(afterTube.parent, &design.find("Sustainer"));
    EXPECT_EQ(afterTube.index, 2U);
    EXPECT_FALSE(ComponentEditing::pastePosition(tube, &design.find("EB")).has_value());
    EXPECT_FALSE(ComponentEditing::pastePosition(tube, nullptr).has_value());
}

// ===================================================================== the clipboard and selection

TEST_F(ComponentEditingTest, TellsTheClipboardsListeners)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    const QSignalSpy    changed(&clipboard, &OpenRocketClipboard::changed);
    window.select({"NC"});

    EXPECT_TRUE(window.editing().copy());
    EXPECT_EQ(changed.count(), 1);
    window.select({"BT2"});
    EXPECT_TRUE(window.editing().duplicate());
    EXPECT_EQ(changed.count(), 2);
    window.select({"Fins"});
    EXPECT_TRUE(window.editing().cut());
    EXPECT_EQ(changed.count(), 3);
    EXPECT_TRUE(window.editing().paste());
    EXPECT_EQ(changed.count(), 3);
    EXPECT_EQ(design.clipboard(clipboard.components()), "[Fins*]");
}

TEST_F(ComponentEditingTest, OperationsThatAreNotAvailableDoNothing)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    const std::string   before = design.tree();

    // Nothing selected, an empty clipboard.
    EXPECT_FALSE(window.editing().cut());
    EXPECT_FALSE(window.editing().copy());
    EXPECT_FALSE(window.editing().paste());
    EXPECT_FALSE(window.editing().duplicate());
    EXPECT_FALSE(window.editing().deleteSelection());
    EXPECT_FALSE(window.editing().moveUp());
    EXPECT_FALSE(window.editing().moveDown());
    // The Rocket first.
    window.select({"Rocket", "NC"});
    EXPECT_FALSE(window.editing().copy());
    EXPECT_FALSE(window.editing().deleteSelection());

    EXPECT_EQ(design.tree(), before);
    EXPECT_EQ(undoText(design.document()), "undo='null'");
    EXPECT_TRUE(clipboard.isEmpty());
}

// No component pointer is kept across a change of the rocket: a listener that deletes the first
// pasted component when the second goes in leaves the second one selected. (A pointer kept from
// the insertion would be read after it was freed: the asan preset reports that.)
TEST_F(ComponentEditingTest, KeepsNoComponentAcrossAChange)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"NC", "BT2"});
    ASSERT_TRUE(window.editing().copy());
    window.select({"BT1"});
    int        insertions = 0;
    const auto connection = design.rocket().addComponentChangeListener(
        [&design, &insertions](const ComponentChangeEvent& event) {
            if (!event.isTreeChange() || ++insertions != 2)
            {
                return;
            }
            RocketComponent& stage = design.find("Sustainer");
            for (std::size_t i = 0; i < stage.getChildCount(); ++i)
            {
                if (!design.isOriginal(stage.getChild(i)) && stage.getChild(i).getName() == "BT2")
                {
                    static_cast<void>(stage.removeChild(i));
                    return;
                }
            }
        });

    EXPECT_TRUE(window.editing().paste());

    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], NC*, BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(window.selection(), "[NC*]");
}

// ======================================================================= decision U4: no exception

// Java's Cut deletes every component of the completed selection one by one: with all stages
// selected it deletes the Booster and the Sustainer's children, then throws "Report bug! Component
// Stage not deletable." (IllegalArgumentException). Here the tops go by Delete's rule: the last
// stage stays, as Delete keeps it; the clipboard has both (Java's too).
TEST_F(ComponentEditingTest, CutOfAllStagesKeepsTheLastAsDelete)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"Sustainer", "Booster"});

    EXPECT_TRUE(window.editing().cut());

    EXPECT_EQ(design.tree(), "Rocket[Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(design.clipboard(clipboard.components()),
              "[Booster*[BBT*[Side boosters*[PBT*]]], Sustainer*[NC*, "
              "BT1*[IT*[EB*], CR1*, "
              "Fins*], BT2*[Chute*]]]");
    EXPECT_EQ(undoText(design.document()), "undo='Cut components'");
}

// Java's Cut of a stage holding a booster set removes the stage, then throws an
// IllegalStateException removing the booster set from the detached stage. Here the stage goes with
// everything in it.
TEST_F(ComponentEditingTest, CutOfAStageHoldingABoosterSet)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"Booster"});

    EXPECT_TRUE(window.editing().cut());

    EXPECT_EQ(design.tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(design.clipboard(clipboard.components()), "[Booster*[BBT*[Side boosters*[PBT*]]]]");
    design.document().undo();
    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// Java's Delete sorts the selection by the position in the parent and dies on the Rocket's null
// parent (NullPointerException) before deleting anything. Here the Rocket is left out.
TEST_F(ComponentEditingTest, DeleteLeavesASelectedRocketOut)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"NC", "Rocket"});
    EXPECT_TRUE(window.editing().canDelete());
    EXPECT_FALSE(window.editing().canCut());

    EXPECT_TRUE(window.editing().deleteSelection());

    EXPECT_EQ(design.tree(),
              "Rocket[Sustainer[BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(undoText(design.document()), "undo='Delete Nose Cone'");
    // The tree selects what is left of the selection (ComponentTreeView).
    EXPECT_EQ(window.selection(), "[Sustainer, Rocket]");
}

// ======================================================================== decision O27: the bounds

TEST_F(ComponentEditingTest, RefusesAPasteBeyondTheComponentBound)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"BT1"});
    ASSERT_TRUE(window.editing().copy());
    // BT1 brings five components.
    fillUpTo(design.rocket(), design.find("BT2"), DocumentConfig::kMaxComponents - 4);
    design.document().addUndoPosition("Rename");
    window.select({"Sustainer"});

    EXPECT_FALSE(window.editing().paste());

    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts()[0].title, QStringLiteral("Paste Body Tube"));
    EXPECT_EQ(prompts.prompts()[0].texts,
              QStringList({QStringLiteral("WARNING"),
                           QStringLiteral("The rocket would have 1001 components. QtRocket opens "
                                          "at most 1000 components of a design and would ignore "
                                          "the ones beyond when it opens this design again.")}));
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), 996U);
    EXPECT_EQ(window.selection(), "[Sustainer]");
    // No undo position either: the next change is the one named before the paste.
    design.find("NC").setName("Nose");
    EXPECT_EQ(undoText(design.document()), "undo='Rename'");
}

TEST_F(ComponentEditingTest, PastesUpToTheComponentBound)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"BT1"});
    ASSERT_TRUE(window.editing().copy());
    fillUpTo(design.rocket(), design.find("BT2"), DocumentConfig::kMaxComponents - 5);
    window.select({"Sustainer"});

    EXPECT_TRUE(window.editing().paste());

    EXPECT_THAT(prompts.prompts(), IsEmpty());
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), DocumentConfig::kMaxComponents);
    EXPECT_EQ(window.selection(), "[BT1*]");
}

TEST_F(ComponentEditingTest, RefusesADuplicateBeyondTheComponentBound)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    Window              window(design, clipboard, prompts);
    window.select({"NC"});
    ASSERT_TRUE(window.editing().copy());
    fillUpTo(design.rocket(), design.find("BT2"), DocumentConfig::kMaxComponents - 1);
    window.select({"BT1", "NC"});

    EXPECT_FALSE(window.editing().duplicate());

    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts()[0].title, QStringLiteral("Duplicate components"));
    EXPECT_EQ(DocumentConfig::componentCount(design.rocket()), DocumentConfig::kMaxComponents - 1);
    // The clipboard is as it was (Java sets it before it duplicates).
    EXPECT_EQ(design.clipboard(clipboard.components()), "[NC*]");
}

/// An appearance with a decal of the image named @p image.
[[nodiscard]] Appearance decalAppearance(const std::string& image)
{
    return {Color(10, 20, 30, 200), 0.25,
            Decal(Coordinate(0.125, 0.25), Coordinate(0.375, 0.5), Coordinate(1.5, 2.5), 0.75,
                  image, Decal::EdgeMode::MIRROR),
            true};
}

/// Registers @p count images of their own in @p document ("decals/fill<k>.png").
void registerImages(OpenRocketDocument& document, std::size_t count)
{
    for (std::size_t k = 0; k < count; ++k)
    {
        const std::string name = "decals/fill" + std::to_string(k) + ".png";
        static_cast<void>(document.getDecalImage(
            std::make_shared<QtRocket::Test::MemoryAttachment>(name, "fill " + name)));
    }
}

/// Gives @p component of @p document a decal of a new image named @p name.
void giveDecal(OpenRocketDocument& document, RocketComponent& component, const std::string& name)
{
    const std::shared_ptr<DecalImage> image = document.getDecalImage(
        std::make_shared<QtRocket::Test::MemoryAttachment>(name, "bytes of " + name));
    component.setAppearance(decalAppearance(image->getName()));
}

TEST_F(ComponentEditingTest, RefusesAPasteBeyondTheDecalImageBound)
{
    const ProbeDesign   source;
    const ProbeDesign   target;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    giveDecal(source.document(), source.find("NC"), "decals/nose.png");
    giveDecal(source.document(), source.find("BT2"), "decals/tube.png");
    Window sourceWindow(source, clipboard, prompts);
    sourceWindow.select({"NC", "BT2"});
    ASSERT_TRUE(sourceWindow.editing().copy());
    registerImages(target.document(), DocumentConfig::kMaxDecalImages - 1);
    Window targetWindow(target, clipboard, prompts);
    targetWindow.select({"Sustainer"});

    EXPECT_FALSE(targetWindow.editing().paste());

    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts()[0].title, QStringLiteral("Paste components"));
    EXPECT_EQ(prompts.prompts()[0].texts,
              QStringList({QStringLiteral("WARNING"),
                           QStringLiteral("The design would have 1001 decal images. QtRocket "
                                          "opens at most 1000 decal images of a design and would "
                                          "ignore the decals of the ones beyond when it opens "
                                          "this design again.")}));
    EXPECT_EQ(target.document().getDecalImageCount(), DocumentConfig::kMaxDecalImages - 1);
    EXPECT_EQ(target.tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    // One image fits.
    sourceWindow.select({"NC"});
    ASSERT_TRUE(sourceWindow.editing().copy());
    EXPECT_TRUE(targetWindow.editing().paste());
    EXPECT_EQ(target.document().getDecalImageCount(), DocumentConfig::kMaxDecalImages);
}

// A decal whose image its document lacks cannot be copied (ComponentCopy: NOT_FOUND); no part of
// QtRocket makes one, and the user is told rather than the clipboard changed.
TEST_F(ComponentEditingTest, TellsWhyACopyCannotBeMade)
{
    const ProbeDesign   design;
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    design.find("NC").setAppearance(decalAppearance("decals/missing.png"));
    Window window(design, clipboard, prompts);
    window.select({"NC"});

    EXPECT_FALSE(window.editing().copy());

    ASSERT_EQ(prompts.prompts().size(), 1U);
    EXPECT_EQ(prompts.prompts()[0].title, QStringLiteral("Copy"));
    ASSERT_EQ(prompts.prompts()[0].texts.size(), 2);
    EXPECT_EQ(prompts.prompts()[0].texts[0], QStringLiteral("ERROR"));
    EXPECT_TRUE(clipboard.isEmpty());
}

// ============================================================== decal images into another document

// The M2 scout's decal probe through the GUI: the body tube of "Parallel booster staging" is copied
// in its window and pasted into a new design in another, which saves with all six images and loads
// them again. Before ComponentCopy the save failed ("Could not find decal source file
// '/datafiles/textures/balsa.jpg'").
TEST_F(ComponentEditingTest, PastesDecalImagesIntoAnotherDocumentThatSavesThem)
{
    const QtRocket::Test::GoldenExample* example =
        QtRocket::Test::findGoldenExample("Parallel booster staging.ork");
    ASSERT_NE(example, nullptr);
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value());
    OpenRocketClipboard clipboard;
    ScriptedUserPrompts prompts;
    {
        const InMemoryPreferences preferences;
        ComponentTreeModel        model(*loaded->document, preferences);
        ComponentTreeView         view(model);
        auto*                     selection = makeChild<DocumentSelectionModel>(&view);
        ComponentEditing          editing(*selection, clipboard, prompts);
        const RocketComponent*    withDecal = firstBodyComponentWithDecal(*loaded->document);
        ASSERT_NE(withDecal, nullptr);
        EXPECT_EQ(withDecal->getName(), "Body Tube");
        selection->setSelectedComponent(*withDecal);
        ASSERT_TRUE(editing.copy());
    }
    // The source closed: the clipboard has what it needs.
    loaded->document.reset();
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences                 preferences;
    ComponentTreeModel                        model(*target, preferences);
    ComponentTreeView                         view(model);
    auto*                                     selection = makeChild<DocumentSelectionModel>(&view);
    ComponentEditing                          editing(*selection, clipboard, prompts);
    selection->setSelectedComponent(target->getRocket().getChild(0));

    ASSERT_TRUE(editing.paste());

    EXPECT_EQ(decalListOf(*target).size(), 6U);
    std::vector<std::byte> bytes = savedWithoutWarnings(*target);
    EXPECT_EQ(archiveEntries(bytes), decalProbeArchiveEntries());
    Result<LoadedDocument> again = QtRocket::Test::loadGoldenExampleFromBytes(std::move(bytes));
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(decalListOf(*again->document), decalListOf(*target));
    EXPECT_THAT(prompts.prompts(), IsEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
