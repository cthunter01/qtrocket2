// The drop rules of the component tree (ComponentDrop), compared with OpenRocket's: the transcript
// of the M2 component-tree scout's DndProbe (evidence/DndProbe.out: OpenRocket 5f164fd0e,
// ComponentTreeTransferHandler's createTransferable() and the loop of importData() run headless),
// then drops through the view as a platform delivers them (QDragEnterEvent, QDragMoveEvent and
// QDropEvent sent to the viewport run QAbstractItemView's own drop logic: Qt's drop positions as
// the scouting's drop_position_probe measured them), drops from another document, with decal
// images, and the bounds.

#include "QtRocket/gui/tree/ComponentDrop.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QModelIndex>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/file/openrocket/DocumentConfig.h"
#include "QtRocket/gui/tree/ComponentMimeData.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
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
using Test::savedWithoutWarnings;
using Test::scenarioName;
using Test::undoText;
using ::testing::IsEmpty;

/// The component tree of a design window: the model, the view and its selection of @p document.
class TreeWindow
{
public:
    explicit TreeWindow(OpenRocketDocument& document)
      : m_model(document, m_preferences),
        m_view(m_model),
        m_selection(makeChild<DocumentSelectionModel>(&m_view))
    {
    }

    [[nodiscard]] ComponentTreeModel&     model() noexcept { return m_model; }
    [[nodiscard]] ComponentTreeView&      view() noexcept { return m_view; }
    [[nodiscard]] DocumentSelectionModel& selection() const noexcept { return *m_selection; }

    /// What a drag of @p components carries (ComponentTreeView::startDrag() asks the model with
    /// every selected row); null when the model makes no drag.
    [[nodiscard]] std::unique_ptr<QMimeData> drag(const std::vector<RocketComponent*>& components)
    {
        QModelIndexList indexes;
        for (const RocketComponent* component : components)
        {
            indexes.append(m_model.indexOf(*component));
        }
        return std::unique_ptr<QMimeData>(m_model.mimeData(indexes));
    }

    /// Shows the view at a size that holds every row.
    void show()
    {
        m_view.resize(320, 480);
        m_view.show();
        QCoreApplication::processEvents();
    }

private:
    InMemoryPreferences     m_preferences;
    ComponentTreeModel      m_model;
    ComponentTreeView       m_view;
    DocumentSelectionModel* m_selection;
};

// =========================================================================== DndProbe's transcript

/// A scenario of DndProbe: the selection dragged, the destination's name and index (Java's -1
/// already made 0), the action; the transcript expected, OpenRocket's.
struct DndScenario
{
    std::string_view name;
    Names            selection;
    std::string_view destination;
    std::size_t      index  = 0;
    Qt::DropAction   action = Qt::MoveAction;
    std::string_view transcript;
};

std::ostream& operator<<(std::ostream& stream, const DndScenario& scenario)
{
    return stream << scenario.name;
}

/// DndProbe's scenarios: the transcripts of DndProbe.out, but where a comment says otherwise
/// (DndProbe prints the trees without marking copies).
[[nodiscard]] const std::vector<DndScenario>& dndScenarios()
{
    static const std::vector<DndScenario> kScenarios{
        {.name        = "MoveBT2ToSustainer0",
         .selection   = {"BT2"},
         .destination = "Sustainer",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [BT2] to (Sustainer, 0)
  dragged (transferable order): [BT2]
  canImport=true
  importData=true tree=Rocket[Sustainer[BT2[Chute], NC, BT1[IT[EB], CR1, Fins]], Booster] undo='Move component')"},
        {.name        = "MoveNCToSustainer3End",
         .selection   = {"NC"},
         .destination = "Sustainer",
         .index       = 3,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [NC] to (Sustainer, 3) (end)
  dragged (transferable order): [NC]
  canImport=true
  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], BT2[Chute], NC], Booster] undo='Move component')"},
        {.name        = "MoveNCToSustainer1SamePlace",
         .selection   = {"NC"},
         .destination = "Sustainer",
         .index       = 1,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [NC] to (Sustainer, 1) (same place)
  dragged (transferable order): [NC]
  canImport=true
  importData=false tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='null')"},
        {.name        = "MoveNCToSustainer0SamePlace",
         .selection   = {"NC"},
         .destination = "Sustainer",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [NC] to (Sustainer, 0) (same place)
  dragged (transferable order): [NC]
  canImport=true
  importData=false tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='null')"},
        // Decision U4: validated whole, then moved together, in tree order. Java drags [BT2, NC],
        // moves BT2 and then stops at NC, already at its place: "importData=false" with this tree.
        {.name        = "MoveNCBT2ToSustainer1",
         .selection   = {"NC", "BT2"},
         .destination = "Sustainer",
         .index       = 1,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [NC, BT2] to (Sustainer, 1)
  dragged (transferable order): [NC, BT2]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], Booster] undo='Move components')"},
        // Decision U4: Java drags [BT2, NC] and stops at once, BT2 being at its place
        // ("importData=false", nothing moved, undo 'null'); here both go to the end, next to each
        // other.
        {.name        = "MoveNCBT2ToSustainer3",
         .selection   = {"NC", "BT2"},
         .destination = "Sustainer",
         .index       = 3,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [NC, BT2] to (Sustainer, 3)
  dragged (transferable order): [NC, BT2]
  canImport=true
  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster] undo='Move components')"},
        // As the one before: the order of the selection does not matter.
        {.name        = "MoveBT2NCToSustainer3",
         .selection   = {"BT2", "NC"},
         .destination = "Sustainer",
         .index       = 3,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [BT2, NC] to (Sustainer, 3)
  dragged (transferable order): [NC, BT2]
  canImport=true
  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster] undo='Move components')"},
        {.name        = "MoveITOnBT2Index10",
         .selection   = {"IT"},
         .destination = "BT2",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [IT] on BT2 (index -1 -> 0)
  dragged (transferable order): [IT]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[CR1, Fins], BT2[IT[EB], Chute]], Booster] undo='Move component')"},
        // The dragged components in tree order (Java: descending child position, [Fins, CR1]); the
        // same result.
        {.name        = "MoveCR1FinsToBT21",
         .selection   = {"CR1", "Fins"},
         .destination = "BT2",
         .index       = 1,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [CR1, Fins] to (BT2, 1)
  dragged (transferable order): [CR1, Fins]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[IT[EB]], BT2[Chute, CR1, Fins]], Booster] undo='Move components')"},
        // The dragged components in tree order (Java: [Fins, CR1]).
        {.name        = "MoveCR1FinsToSustainer0FinsNotAllowedThere",
         .selection   = {"CR1", "Fins"},
         .destination = "Sustainer",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [CR1, Fins] to (Sustainer, 0) (fins not allowed there)
  dragged (transferable order): [CR1, Fins]
  canImport=false)"},
        {.name        = "MoveBT1IntoITNotCompatible",
         .selection   = {"BT1"},
         .destination = "IT",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [BT1] into IT (not compatible)
  dragged (transferable order): [BT1]
  canImport=false)"},
        {.name        = "MoveBT1IntoItsOwnChildIT",
         .selection   = {"IT"},
         .destination = "IT",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [BT1] into its own child IT
  dragged (transferable order): [IT]
  canImport=false)"},
        {.name        = "MoveBT1ITToSustainer0",
         .selection   = {"BT1", "IT"},
         .destination = "Sustainer",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [BT1, IT] to (Sustainer, 0)
  dragged (transferable order): [BT1]
  canImport=true
  importData=true tree=Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], Booster] undo='Move component')"},
        // Decision U4: Java moves IT, then stops at Chute with the source index it took before the
        // move ("importData=false" with this tree); here both move, with the same tree.
        {.name        = "MoveITChuteToBT20DifferentParents",
         .selection   = {"IT", "Chute"},
         .destination = "BT2",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [IT, Chute] to (BT2, 0) (different parents)
  dragged (transferable order): [IT, Chute]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[CR1, Fins], BT2[IT[EB], Chute]], Booster] undo='Move components')"},
        // Decision U4: Java drags [Chute, IT] (equal child positions: the selection's order), moves
        // Chute to BT1's front and stops at IT ("importData=false tree=Rocket[Sustainer[NC,
        // BT1[Chute, IT[EB], CR1, Fins], BT2], Booster]"); here both move in tree order.
        {.name        = "MoveChuteITToBT10DifferentParents",
         .selection   = {"Chute", "IT"},
         .destination = "BT1",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [Chute, IT] to (BT1, 0) (different parents)
  dragged (transferable order): [IT, Chute]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[IT[EB], Chute, CR1, Fins], BT2], Booster] undo='Move components')"},
        {.name        = "CopyBT1ToSustainer1",
         .selection   = {"BT1"},
         .destination = "Sustainer",
         .index       = 1,
         .action      = Qt::CopyAction,
         .transcript  = R"(== copy [BT1] to (Sustainer, 1)
  dragged (transferable order): [BT1]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='Copy component')"},
        // The dragged components in tree order (Java: [CR1, IT], copied in turn at index 1); the
        // same result.
        {.name        = "CopyITCR1ToBT21",
         .selection   = {"IT", "CR1"},
         .destination = "BT2",
         .index       = 1,
         .action      = Qt::CopyAction,
         .transcript  = R"(== copy [IT, CR1] to (BT2, 1)
  dragged (transferable order): [IT, CR1]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute, IT[EB], CR1]], Booster] undo='Copy components')"},
        {.name        = "CopyNCToSustainer0SamePlaceCopy",
         .selection   = {"NC"},
         .destination = "Sustainer",
         .index       = 0,
         .action      = Qt::CopyAction,
         .transcript  = R"(== copy [NC] to (Sustainer, 0) (same place, copy)
  dragged (transferable order): [NC]
  canImport=true
  importData=true tree=Rocket[Sustainer[NC, NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], Booster] undo='Copy component')"},
        {.name        = "MoveSustainerToRocket2",
         .selection   = {"Sustainer"},
         .destination = "Rocket",
         .index       = 2,
         .action      = Qt::MoveAction,
         .transcript  = R"(== move [Sustainer] to (Rocket, 2)
  dragged (transferable order): [Sustainer]
  canImport=true
  importData=true tree=Rocket[Booster, Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]] undo='Move component')"},
        {.name        = "DragRocket",
         .selection   = {"Rocket"},
         .destination = "Rocket",
         .index       = 0,
         .action      = Qt::MoveAction,
         .transcript  = R"(== drag [Rocket]
  createTransferable: null (no drag))"},
    };
    return kScenarios;
}

/// The components of @p data in @p rocket, in its order.
[[nodiscard]] std::vector<const RocketComponent*> componentsOf(const ComponentMimeData& data,
                                                               const Rocket&            rocket)
{
    std::vector<const RocketComponent*> components;
    components.reserve(data.componentIds().size());
    for (const Uuid& id : data.componentIds())
    {
        components.push_back(rocket.findComponent(id));
    }
    return components;
}

/// DndProbe's drop(): the transcript of @p scenario.
[[nodiscard]] std::vector<std::string> run(const DndScenario& scenario)
{
    const ProbeDesign             design(ProbeDesign::Booster::EMPTY);
    TreeWindow                    window(design.document());
    std::vector<std::string>      lines{std::string(linesOf(scenario.transcript).front())};
    std::vector<RocketComponent*> selected;
    selected.reserve(scenario.selection.size());
    for (const std::string_view name : scenario.selection)
    {
        selected.push_back(&design.find(name));
    }
    const std::unique_ptr<QMimeData> data = window.drag(selected);
    if (data == nullptr)
    {
        lines.emplace_back("  createTransferable: null (no drag)");
        return lines;
    }
    const ComponentMimeData* drag = ComponentMimeData::fromMimeData(data.get());
    lines.push_back("  dragged (transferable order): " +
                    design.names(componentsOf(*drag, design.rocket())));
    const ComponentDrop::Target target{.parent = &design.find(scenario.destination),
                                       .index  = scenario.index};
    const bool allowed = ComponentDrop::canDrop(design.document(), *drag, scenario.action, target);
    lines.push_back("  canImport=" + javaBool(allowed));
    if (allowed)
    {
        const ComponentDrop::Outcome outcome =
            ComponentDrop::drop(design.document(), *drag, scenario.action, target);
        EXPECT_FALSE(outcome.refusal.has_value());
        lines.push_back("  importData=" + javaBool(outcome.dropped) +
                        " tree=" + ProbeDesign::plainTree(design.rocket()) + " " +
                        undoText(design.document()));
    }
    return lines;
}

class DndProbeTranscript : public Test::GuiTest, public ::testing::WithParamInterface<DndScenario>
{ };

TEST_P(DndProbeTranscript, IsOpenRocketsTranscript)
{
    EXPECT_EQ(run(GetParam()), linesOf(GetParam().transcript));
}

INSTANTIATE_TEST_SUITE_P(ComponentDrop, DndProbeTranscript, ::testing::ValuesIn(dndScenarios()),
                         scenarioName<DndScenario>);

// =========================================================================== the rules, one by one

using ComponentDropTest = Test::GuiTest;

TEST_F(ComponentDropTest, RefusesWhatIsNotCompatibleOrWouldMakeACycle)
{
    const ProbeDesign                         design;
    const std::vector<const RocketComponent*> bt1{&design.find("BT1")};
    const std::vector<const RocketComponent*> fins{&design.find("Fins")};

    EXPECT_TRUE(ComponentDrop::isAllowed(bt1, {.parent = &design.find("Booster"), .index = 0}));
    EXPECT_FALSE(ComponentDrop::isAllowed(bt1, {.parent = &design.find("BT2"), .index = 0}));
    // Into itself, into a descendant.
    EXPECT_FALSE(ComponentDrop::isAllowed(bt1, {.parent = &design.find("BT1"), .index = 0}));
    EXPECT_FALSE(ComponentDrop::isAllowed(bt1, {.parent = &design.find("IT"), .index = 0}));
    EXPECT_FALSE(ComponentDrop::isAllowed(fins, {.parent = &design.find("Sustainer"), .index = 0}));
    EXPECT_FALSE(ComponentDrop::isAllowed({}, {.parent = &design.find("Sustainer"), .index = 0}));
}

TEST_F(ComponentDropTest, KnowsAMoveToTheSamePlace)
{
    const ProbeDesign                         design;
    RocketComponent*                          stage = &design.find("Sustainer");
    const std::vector<const RocketComponent*> nc{&design.find("NC")};
    const std::vector<const RocketComponent*> ncBt1{&design.find("NC"), &design.find("BT1")};
    const std::vector<const RocketComponent*> ncBt2{&design.find("NC"), &design.find("BT2")};

    // Java's checkImportAction(): the component's own index or the next one.
    EXPECT_TRUE(ComponentDrop::isNoOpMove(nc, {.parent = stage, .index = 0}));
    EXPECT_TRUE(ComponentDrop::isNoOpMove(nc, {.parent = stage, .index = 1}));
    EXPECT_FALSE(ComponentDrop::isNoOpMove(nc, {.parent = stage, .index = 2}));
    // Neighbours: any index from the first to after the last.
    EXPECT_TRUE(ComponentDrop::isNoOpMove(ncBt1, {.parent = stage, .index = 0}));
    EXPECT_TRUE(ComponentDrop::isNoOpMove(ncBt1, {.parent = stage, .index = 1}));
    EXPECT_TRUE(ComponentDrop::isNoOpMove(ncBt1, {.parent = stage, .index = 2}));
    EXPECT_FALSE(ComponentDrop::isNoOpMove(ncBt1, {.parent = stage, .index = 3}));
    // Not neighbours: they would come together.
    EXPECT_FALSE(ComponentDrop::isNoOpMove(ncBt2, {.parent = stage, .index = 0}));
    EXPECT_FALSE(ComponentDrop::isNoOpMove(nc, {.parent = &design.find("Booster"), .index = 0}));
}

/// @p target, or a test failure and no target when there is none. (A plain if is what clang-tidy's
/// unchecked-optional-access check follows; it cannot see through ASSERT_TRUE.)
[[nodiscard]] ComponentDrop::Target required(const std::optional<ComponentDrop::Target>& target)
{
    if (!target.has_value())
    {
        ADD_FAILURE() << "no drop target";
        return {};
    }
    return *target;
}

TEST_F(ComponentDropTest, MapsQtsDropPositions)
{
    const ProbeDesign         design;
    TreeWindow                window(design.document());
    const ComponentTreeModel& model = window.model();
    const QModelIndex         bt1   = model.indexOf(design.find("BT1"));

    const ComponentDrop::Target onRow   = required(ComponentDrop::target(model, -1, bt1));
    const ComponentDrop::Target atRow   = required(ComponentDrop::target(model, 2, bt1));
    const ComponentDrop::Target blank   = required(ComponentDrop::target(model, -1, QModelIndex()));
    const auto                  aboveIt = ComponentDrop::target(model, 0, QModelIndex());

    // On a row: its first child (Java: -1 becomes 0).
    EXPECT_EQ(onRow.parent, &design.find("BT1"));
    EXPECT_EQ(onRow.index, 0U);
    EXPECT_EQ(atRow.index, 2U);
    // Below the rows: the end of the Rocket.
    EXPECT_EQ(blank.parent, &design.rocket());
    EXPECT_EQ(blank.index, 2U);
    // Above or below the Rocket's row, beyond the children: nowhere.
    EXPECT_FALSE(aboveIt.has_value());
    EXPECT_FALSE(ComponentDrop::target(model, 4, bt1).has_value());
}

// ================================================================================ through the view

/// Where in a row a drop lands (the scouting's drop_position_probe: one pixel inside the top and
/// the bottom edge, and the middle).
enum class Part
{
    TOP,
    MIDDLE,
    BOTTOM,
};

/// The point of the viewport in @p part of @p index's row.
[[nodiscard]] QPoint pointIn(const ComponentTreeView& view, const QModelIndex& index, Part part)
{
    const QRect rect = view.visualRect(index);
    switch (part)
    {
        case Part::TOP:
            return {rect.center().x(), rect.top() + 1};
        case Part::MIDDLE:
            return rect.center();
        case Part::BOTTOM:
            return {rect.center().x(), rect.bottom() - 1};
    }
    return rect.center();
}

/// A point of the viewport below the last row.
[[nodiscard]] QPoint below(const ComponentTreeView& view)
{
    const QModelIndexList rows = view.visibleRows();
    return {20, view.visualRect(rows.last()).bottom() + 40};
}

/// What a drop through the view did.
struct ViewDrop
{
    /// Whether the view accepted the drag over the point (the cursor allows a drop).
    bool moveAccepted = false;
    /// The action it proposed there.
    Qt::DropAction moveAction = Qt::IgnoreAction;
    /// Whether the drop was accepted (QAbstractItemView: the model dropped).
    bool dropAccepted = false;
};

/// A drag of @p data entering @p view's viewport, moving to @p position and dropped there, the keys
/// @p modifiers held (Shift: a move, Ctrl: a copy; QPlatformDrag's rule, offscreen too); as a
/// platform delivers the events to a widget.
ViewDrop dropAt(ComponentTreeView& view, const QMimeData& data, QPoint position,
                Qt::KeyboardModifiers modifiers)
{
    QWidget*              viewport = view.viewport();
    const Qt::DropActions actions  = Qt::CopyAction | Qt::MoveAction;
    QDragEnterEvent       enter(position, actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &enter);
    QDragMoveEvent move(position, actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &move);
    ViewDrop result{.moveAccepted = move.isAccepted(), .moveAction = move.dropAction()};
    // Dropped also where the view refused the drag: the model must refuse it then too.
    QDropEvent drop(QPointF(position), actions, &data, Qt::LeftButton, modifiers);
    QApplication::sendEvent(viewport, &drop);
    result.dropAccepted = drop.isAccepted();
    return result;
}

/// The fixture of the drops through a view of the probes' design.
class ComponentDropViewTest : public Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_window = std::make_unique<TreeWindow>(m_design.document());
        m_window->model().setUserPrompts(&m_prompts, &m_window->view());
        m_window->show();
    }

    void TearDown() override
    {
        m_window.reset();
        Gui::Test::GuiTest::TearDown();
    }

    [[nodiscard]] QModelIndex row(std::string_view name) const
    {
        return m_window->model().indexOf(m_design.find(name));
    }

    /// Selects @p names and drags them; drops at @p part of the row of @p onto.
    ViewDrop dragAndDrop(const std::vector<std::string_view>& names, std::string_view onto,
                         Part part, Qt::KeyboardModifiers modifiers = Qt::ShiftModifier)
    {
        const QPoint position = pointIn(m_window->view(), row(onto), part);
        return dragAndDrop(names, position, modifiers);
    }

    ViewDrop dragAndDrop(const std::vector<std::string_view>& names, QPoint position,
                         Qt::KeyboardModifiers modifiers = Qt::ShiftModifier)
    {
        std::vector<RocketComponent*> components;
        components.reserve(names.size());
        for (const std::string_view name : names)
        {
            components.push_back(&m_design.find(name));
        }
        m_window->selection().setSelectedComponents(components);
        const std::unique_ptr<QMimeData> data = m_window->drag(components);
        EXPECT_NE(data, nullptr);
        return data == nullptr ? ViewDrop{} : dropAt(m_window->view(), *data, position, modifiers);
    }

    [[nodiscard]] std::string tree() const { return m_design.tree(); }
    [[nodiscard]] std::string selection() const
    {
        return m_design.names(m_window->selection().selectedComponents());
    }

    ProbeDesign                    m_design;
    Gui::Test::ScriptedUserPrompts m_prompts;
    std::unique_ptr<TreeWindow>    m_window;
};

// drop_position_probe "Stage middle -> row=-1 col=-1 parent=Stage": its first child; DndProbe "move
// [BT2] to (Sustainer, 0)". The moved component stays selected (Java: the tree finds it again).
TEST_F(ComponentDropViewTest, DropsOnTheMiddleOfARowAsItsFirstChild)
{
    const ViewDrop drop = dragAndDrop({"BT2"}, "Sustainer", Part::MIDDLE);

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_EQ(drop.moveAction, Qt::MoveAction);
    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[BT2[Chute], NC, BT1[IT[EB], CR1, Fins]], "
              "Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(undoText(m_design.document()), "undo='Move component'");
    EXPECT_EQ(selection(), "[BT2]");
}

// drop_position_probe "BT1 top -> row=0 col=0 parent=Stage": before the row in its parent.
TEST_F(ComponentDropViewTest, DropsInTheTopMarginBeforeTheRow)
{
    const ViewDrop drop = dragAndDrop({"BT2"}, "BT1", Part::TOP);

    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT2[Chute], BT1[IT[EB], CR1, Fins]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// drop_position_probe "BT1 bottom -> row=1 col=0 parent=Stage": after the row, also when it is
// expanded with children (JTree inserts as its first child there: OpenRocket would refuse the nose
// cone, which a body tube does not take).
TEST_F(ComponentDropViewTest, DropsInTheBottomMarginAfterTheRow)
{
    const ViewDrop drop = dragAndDrop({"NC"}, "BT1", Part::BOTTOM);

    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[BT1[IT[EB], CR1, Fins], NC, BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// drop_position_probe "Fins (leaf) middle -> row=3 col=0 parent=Stage": a row that takes no
// children has no middle; after it.
TEST_F(ComponentDropViewTest, DropsOnTheMiddleOfALeafAfterIt)
{
    const ViewDrop drop = dragAndDrop({"IT"}, "Fins", Part::MIDDLE);

    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1[CR1, Fins, IT[EB]], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// Below the last row: the end of the Rocket (JTree: the root's end), DndProbe "move [Sustainer] to
// (Rocket, 2)".
TEST_F(ComponentDropViewTest, DropsBelowTheRowsAtTheEndOfTheRocket)
{
    const ViewDrop drop = dragAndDrop({"Sustainer"}, below(m_window->view()));

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Booster[BBT[Side boosters[PBT]]], Sustainer[NC, BT1[IT[EB], CR1, "
              "Fins], BT2[Chute]]]");
}

TEST_F(ComponentDropViewTest, DropsNothingAboveOrBelowTheRocketsRow)
{
    const std::string before = tree();

    const ViewDrop above = dragAndDrop({"Booster"}, "Rocket", Part::TOP);
    const ViewDrop under = dragAndDrop({"Booster"}, "Rocket", Part::BOTTOM);

    EXPECT_FALSE(above.moveAccepted);
    EXPECT_FALSE(above.dropAccepted);
    EXPECT_FALSE(under.moveAccepted);
    EXPECT_FALSE(under.dropAccepted);
    EXPECT_EQ(tree(), before);
    EXPECT_EQ(undoText(m_design.document()), "undo='null'");
}

// DndProbe "copy [BT1] to (Sustainer, 1)", with the key of a copy.
TEST_F(ComponentDropViewTest, CopiesWithControl)
{
    const ViewDrop drop = dragAndDrop({"BT1"}, "NC", Part::BOTTOM, Qt::ControlModifier);

    EXPECT_EQ(drop.moveAction, Qt::CopyAction);
    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1*[IT*[EB*], CR1*, Fins*], "
              "BT1[IT[EB], CR1, Fins], "
              "BT2[Chute]], Booster[BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(undoText(m_design.document()), "undo='Copy component'");
    // Java selects nothing new.
    EXPECT_EQ(selection(), "[BT1]");
}

TEST_F(ComponentDropViewTest, RefusesWhatTheRulesRefuse)
{
    const std::string before = tree();

    // A body tube into a body tube; a body tube into its own inner tube.
    const ViewDrop incompatible = dragAndDrop({"BT2"}, "BT1", Part::MIDDLE);
    const ViewDrop cycle        = dragAndDrop({"BT1"}, "IT", Part::MIDDLE);

    EXPECT_FALSE(incompatible.moveAccepted);
    EXPECT_FALSE(incompatible.dropAccepted);
    EXPECT_FALSE(cycle.moveAccepted);
    EXPECT_FALSE(cycle.dropAccepted);
    EXPECT_EQ(tree(), before);
}

// DndProbe "move [NC] to (Sustainer, 0) (same place)": the drag may drop there (canImport), the
// drop changes nothing and makes no undo step.
TEST_F(ComponentDropViewTest, AMoveToTheSamePlaceChangesNothing)
{
    const std::string before = tree();

    const ViewDrop drop = dragAndDrop({"NC"}, "NC", Part::TOP);

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_FALSE(drop.dropAccepted);
    EXPECT_EQ(tree(), before);
    EXPECT_EQ(undoText(m_design.document()), "undo='null'");
}

TEST_F(ComponentDropViewTest, AMoveIsOneUndoStep)
{
    ASSERT_TRUE(dragAndDrop({"NC", "BT2"}, "Booster", Part::MIDDLE).dropAccepted);

    EXPECT_EQ(tree(),
              "Rocket[Sustainer[BT1[IT[EB], CR1, Fins]], Booster[NC, BT2[Chute], "
              "BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(selection(), "[NC, BT2]");
    m_design.document().undo();
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BBT[Side boosters[PBT]]]]");
}

// ================================================================================ another document

// A drag from another window: always a copy (the view proposes the copy action whatever the keys,
// as Java's canImportTarget() changes the action), the source unchanged.
TEST_F(ComponentDropViewTest, CopiesFromAnotherDocument)
{
    const ProbeDesign                source(ProbeDesign::Booster::NONE);
    TreeWindow                       sourceWindow(source.document());
    const std::unique_ptr<QMimeData> data = sourceWindow.drag({&source.find("BT1")});
    ASSERT_NE(data, nullptr);

    const ViewDrop drop =
        dropAt(m_window->view(), *data, pointIn(m_window->view(), row("Booster"), Part::MIDDLE),
               Qt::ShiftModifier);

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_EQ(drop.moveAction, Qt::CopyAction);
    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(tree(),
              "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]], "
              "Booster[BT1*[IT*[EB*], CR1*, Fins*], BBT[Side boosters[PBT]]]]");
    EXPECT_EQ(undoText(m_design.document()), "undo='Copy component'");
    EXPECT_EQ(source.tree(), "Rocket[Sustainer[NC, BT1[IT[EB], CR1, Fins], BT2[Chute]]]");
    EXPECT_EQ(undoText(source.document()), "undo='null'");
}

// Decision O27: a copy that would give the rocket more components than QtRocket loads is refused,
// with a message once the drop is over.
TEST_F(ComponentDropViewTest, RefusesACopyBeyondTheComponentBoundWithAMessage)
{
    fillUpTo(m_design.rocket(), m_design.find("BBT"), DocumentConfig::kMaxComponents - 4);
    const std::size_t components = DocumentConfig::componentCount(m_design.rocket());

    const ViewDrop drop = dragAndDrop({"BT1"}, "NC", Part::BOTTOM, Qt::ControlModifier);

    EXPECT_TRUE(drop.moveAccepted);
    EXPECT_FALSE(drop.dropAccepted);
    EXPECT_EQ(DocumentConfig::componentCount(m_design.rocket()), components);
    // Not inside the drop event.
    EXPECT_THAT(m_prompts.prompts(), IsEmpty());
    QCoreApplication::processEvents();
    ASSERT_EQ(m_prompts.prompts().size(), 1U);
    EXPECT_EQ(m_prompts.prompts()[0].title, QStringLiteral("Copy component"));
    EXPECT_EQ(m_prompts.prompts()[0].texts,
              QStringList({QStringLiteral("WARNING"),
                           QStringLiteral("The rocket would have 1001 components. QtRocket opens "
                                          "at most 1000 components of a design and would ignore "
                                          "the ones beyond when it opens this design again.")}));
}

// A move adds nothing: no bound.
TEST_F(ComponentDropViewTest, MovesAtTheComponentBound)
{
    fillUpTo(m_design.rocket(), m_design.find("BBT"), DocumentConfig::kMaxComponents);

    EXPECT_TRUE(dragAndDrop({"BT1"}, "NC", Part::TOP).dropAccepted);

    EXPECT_EQ(DocumentConfig::componentCount(m_design.rocket()), DocumentConfig::kMaxComponents);
}

// The M2 scout's decal probe as a drop: the first body component with a decal of "Parallel booster
// staging" (its body tube, with the booster set, the fins and the recovery) dropped from its window
// into a new design's stage, which then saves with all six images and loads them again (OpenRocket
// saves the six too; before ComponentCopy QtRocket's save failed).
TEST_F(ComponentDropTest, DropsDecalImagesIntoAnotherDocumentThatSavesThem)
{
    const QtRocket::Test::GoldenExample* example =
        QtRocket::Test::findGoldenExample("Parallel booster staging.ork");
    ASSERT_NE(example, nullptr);
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(*example);
    ASSERT_TRUE(loaded.has_value());
    TreeWindow             sourceWindow(*loaded->document);
    const RocketComponent* withDecal = firstBodyComponentWithDecal(*loaded->document);
    ASSERT_NE(withDecal, nullptr);
    const std::unique_ptr<QMimeData> data =
        sourceWindow.drag({loaded->document->getRocket().findComponent(withDecal->getId())});
    ASSERT_NE(data, nullptr);
    const std::unique_ptr<OpenRocketDocument> target = OpenRocketDocumentFactory::createNewRocket();
    TreeWindow                                targetWindow(*target);
    targetWindow.show();
    const QModelIndex stage = targetWindow.model().indexOf(target->getRocket().getChild(0));

    const ViewDrop drop =
        dropAt(targetWindow.view(), *data, pointIn(targetWindow.view(), stage, Part::MIDDLE),
               Qt::ShiftModifier);

    EXPECT_TRUE(drop.dropAccepted);
    EXPECT_EQ(decalListOf(*target).size(), 6U);
    std::vector<std::byte> bytes = savedWithoutWarnings(*target);
    EXPECT_EQ(archiveEntries(bytes), decalProbeArchiveEntries());
    Result<LoadedDocument> again = QtRocket::Test::loadGoldenExampleFromBytes(std::move(bytes));
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ(decalListOf(*again->document), decalListOf(*target));
}

}  // namespace

}  // namespace QtRocket::Gui
