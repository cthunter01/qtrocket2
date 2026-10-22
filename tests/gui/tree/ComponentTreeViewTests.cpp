#include "QtRocket/gui/tree/ComponentTreeView.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDrag>
#include <QEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QModelIndex>
#include <QObject>
#include <QPoint>
#include <QRect>
#include <QScrollBar>
#include <QString>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/tree/ComponentMimeData.h"
#include "QtRocket/gui/tree/ComponentTreeDelegate.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/CenteringRing.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/Parachute.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"
#include "tree/ComponentTreeTranscript.h"
#include "tree/TreeViewMouse.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenExample;
using QtRocket::Test::TestRocketMaker;
using Test::treeTranscript;
using ::testing::ElementsAre;
using ::testing::IsEmpty;

/// A request of ComponentTreeView::editRequested().
struct EditRequest
{
    RocketComponent* component     = nullptr;
    bool             withSelection = false;
};

/// A document, its model, a view of it and the view's selection. The design is that of the
/// scouting's ViewProbe: the stage holds a nose cone and a body tube; the body tube an inner tube
/// (which holds a mass component) and a fin set.
class ComponentTreeViewTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_document = OpenRocketDocumentFactory::createNewRocket();
        m_stage    = &rocket().getChild(0);
        m_noseCone = &m_stage->addChild(std::make_unique<NoseCone>());
        m_bodyTube = &m_stage->addChild(std::make_unique<BodyTube>());
        m_inner    = &m_bodyTube->addChild(std::make_unique<InnerTube>());
        m_mass     = &m_inner->addChild(std::make_unique<MassComponent>());
        m_fins     = &m_bodyTube->addChild(std::make_unique<TrapezoidFinSet>());
        makeView();
    }

    void TearDown() override
    {
        m_view = nullptr;
        m_model.reset();
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    /// The model, the view and its selection of the document.
    void makeView()
    {
        m_view      = nullptr;
        m_model     = std::make_unique<ComponentTreeModel>(*m_document, m_preferences);
        m_view      = std::make_unique<ComponentTreeView>(*m_model);
        m_selection = makeChild<DocumentSelectionModel>(m_view.get());
        QObject::connect(
            m_view.get(), &ComponentTreeView::editRequested, m_view.get(),
            [this](RocketComponent* component, bool withSelection) {
                m_edits.push_back({.component = component, .withSelection = withSelection});
            });
        QObject::connect(m_view.get(), &ComponentTreeView::contextMenuRequested, m_view.get(),
                         [this](const QPoint& position) { m_menus.push_back(position); });
    }

    /// Shows the view at a size that holds every row.
    void show()
    {
        m_view->resize(320, 480);
        m_view->show();
        QCoreApplication::processEvents();
    }

    [[nodiscard]] Rocket& rocket() const { return m_document->getRocket(); }

    [[nodiscard]] QModelIndex index(const RocketComponent& component) const
    {
        return m_model->indexOf(component);
    }

    [[nodiscard]] std::string transcript(std::string_view label) const
    {
        return treeTranscript(label, *m_view);
    }

    void collapse(const RocketComponent& component) { m_view->collapse(index(component)); }
    void expand(const RocketComponent& component) { m_view->expand(index(component)); }

    void select(const std::vector<RocketComponent*>& components)
    {
        m_selection->setSelectedComponents(components);
    }

    /// The center of the row of @p component, in the viewport.
    [[nodiscard]] QPoint center(const RocketComponent& component) const
    {
        return m_view->visualRect(index(component)).center();
    }

    /// A point of the viewport below the last row.
    [[nodiscard]] QPoint blank() const
    {
        const QModelIndexList rows = m_view->visibleRows();
        return {10, m_view->visualRect(rows.last()).bottom() + 40};
    }

    /// A double-click at @p position of the viewport (Test::doubleClickAt()).
    void doubleClick(const QPoint& position, Qt::KeyboardModifiers modifiers = {},
                     Qt::MouseButton button = Qt::LeftButton) const
    {
        Gui::Test::doubleClickAt(*m_view->viewport(), position, modifiers, button);
    }

    /// A click of the right button and the context menu event at @p position of the viewport
    /// (Test::rightClickAt()).
    void rightClick(const QPoint& position, Qt::KeyboardModifiers modifiers = {}) const
    {
        Gui::Test::rightClickAt(*m_view->viewport(), position, modifiers);
    }

    [[nodiscard]] std::vector<std::string> selectedNames() const
    {
        std::vector<std::string> names;
        for (const RocketComponent* component : m_selection->selectedComponents())
        {
            names.push_back(component->getName());
        }
        return names;
    }

    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    RocketComponent*                    m_stage    = nullptr;
    RocketComponent*                    m_noseCone = nullptr;
    RocketComponent*                    m_bodyTube = nullptr;
    RocketComponent*                    m_inner    = nullptr;
    RocketComponent*                    m_mass     = nullptr;
    RocketComponent*                    m_fins     = nullptr;
    std::unique_ptr<ComponentTreeModel> m_model;
    std::unique_ptr<ComponentTreeView>  m_view;
    DocumentSelectionModel*             m_selection = nullptr;
    std::vector<EditRequest>            m_edits;
    std::vector<QPoint>                 m_menus;
};

// =========================================================================== the settings

TEST_F(ComponentTreeViewTest, HasBasicTreesLook)
{
    EXPECT_EQ(m_view->objectName(), QStringLiteral("componentTree"));
    // BasicTreeUI's leftChildIndent 15 and FlatLaf's rightChildIndent 11.
    EXPECT_EQ(m_view->indentation(), 26);
    EXPECT_FALSE(m_view->rootIsDecorated());
    EXPECT_FALSE(m_view->expandsOnDoubleClick());
    EXPECT_TRUE(m_view->isHeaderHidden());
    EXPECT_EQ(m_view->selectionMode(), QAbstractItemView::ExtendedSelection);
    EXPECT_EQ(m_view->selectionBehavior(), QAbstractItemView::SelectRows);
    EXPECT_EQ(m_view->editTriggers(), QAbstractItemView::NoEditTriggers);
    EXPECT_NE(dynamic_cast<ComponentTreeDelegate*>(m_view->itemDelegate()), nullptr);
    EXPECT_EQ(&m_view->componentModel(), m_model.get());
    EXPECT_EQ(m_view->model(), m_model.get());
}

// The M2 run 2 review's TreeIndentProbe (OpenRocket's BasicTree with its ComponentTreeModel and
// ComponentTreeRenderer under FlatLightLaf, headless): the rows of the depths 0 to 4 at x 0, 26,
// 52, 78 and 104.
TEST_F(ComponentTreeViewTest, IndentsTheRowsAsOpenRocketDoes)
{
    show();

    EXPECT_EQ(m_view->visualRect(m_model->rocketIndex()).left(), 0);
    EXPECT_EQ(m_view->visualRect(index(*m_stage)).left(), 26);
    EXPECT_EQ(m_view->visualRect(index(*m_bodyTube)).left(), 52);
    EXPECT_EQ(m_view->visualRect(index(*m_inner)).left(), 78);
    EXPECT_EQ(m_view->visualRect(index(*m_mass)).left(), 104);
}

TEST_F(ComponentTreeViewTest, DragsAndTakesDrops)
{
    EXPECT_TRUE(m_view->dragEnabled());
    EXPECT_TRUE(m_view->acceptDrops());
    EXPECT_TRUE(m_view->showDropIndicator());
    EXPECT_EQ(m_view->dragDropMode(), QAbstractItemView::DragDrop);
    EXPECT_EQ(m_view->defaultDropAction(), Qt::MoveAction);
    EXPECT_EQ(m_view->autoExpandDelay(), 2000);
}

TEST_F(ComponentTreeViewTest, ShowsNoOtherModel)
{
    ComponentTreeModel other(*m_document, m_preferences);
    EXPECT_THROW(m_view->setModel(&other), BugError);
    EXPECT_THROW(m_view->setModel(m_model.get()), BugError);
    EXPECT_EQ(m_view->model(), m_model.get());
}

TEST_F(ComponentTreeViewTest, IsFullyExpandedAtCreation)
{
    EXPECT_EQ(transcript("new"),
              "new: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner Tube+, "
              "Mass Component+, Trapezoidal Fin Set-, } selected=[]");
    EXPECT_EQ(m_view->visibleRows().size(), 7);
    EXPECT_EQ(m_view->visibleRows().first(), m_model->rocketIndex());
}

// ===================================================== the state across a change of the tree

// The expansion part of the scouting's TreeProbe (OpenRocket 5f164fd0e, evidence/TreeProbe.out),
// up to the redo (its last step collapsed a stage object that the undo had replaced: see
// FrozenMoveExpandsTheSourceStage for that step done right).
using ComponentTreeViewTreeProbe = Gui::Test::GuiTest;

TEST_F(ComponentTreeViewTreeProbe, KeepsTheExpansionAsOpenRocket)
{
    const InMemoryPreferences                 preferences;
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    ComponentTreeModel model(*document, preferences);
    ComponentTreeView  view(model);
    auto*              selection = makeChild<DocumentSelectionModel>(&view);
    RocketComponent&   stage     = document->getRocket().getChild(0);
    EXPECT_EQ(treeTranscript("new", view), "new: rows=2 {Rocket+, Sustainer+, } selected=[]");

    document->addUndoPosition("Add Nose cone");
    stage.addChild(std::make_unique<NoseCone>());
    EXPECT_EQ(treeTranscript("nose cone added", view),
              "nose cone added: rows=3 {Rocket+, Sustainer+, Nose Cone+, } selected=[]");
    document->addUndoPosition("Add Body tube");
    RocketComponent& bodyTube = stage.addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(treeTranscript("body tube added", view),
              "body tube added: rows=4 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, } "
              "selected=[]");
    document->addUndoPosition("Add Inner tube");
    RocketComponent& innerTube = bodyTube.addChild(std::make_unique<InnerTube>());
    EXPECT_EQ(treeTranscript("inner tube added", view),
              "inner tube added: rows=5 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner "
              "Tube+, } selected=[]");
    document->addUndoPosition("Add Centering ring");
    innerTube.addChild(std::make_unique<MassComponent>());
    const RocketComponent& ring = bodyTube.addChild(std::make_unique<CenteringRing>());
    EXPECT_EQ(treeTranscript("ring and mass added", view),
              "ring and mass added: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner "
              "Tube+, Mass Component+, Centering Ring-, } selected=[]");

    view.collapse(model.indexOf(innerTube));
    EXPECT_EQ(treeTranscript("inner tube collapsed", view),
              "inner tube collapsed: rows=6 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner "
              "Tube-, Mass Component(hidden), Centering Ring-, } selected=[]");
    selection->setSelectedComponent(ring);
    document->addUndoPosition("Add Parachute");
    bodyTube.addChild(std::make_unique<Parachute>());
    EXPECT_EQ(treeTranscript("parachute added to body tube (inner tube collapsed before)", view),
              "parachute added to body tube (inner tube collapsed before): rows=7 {Rocket+, "
              "Sustainer+, Nose Cone+, Body Tube+, Inner Tube-, Mass Component(hidden), Centering "
              "Ring-, Parachute-, } selected=[Centering Ring]");

    view.collapse(model.indexOf(bodyTube));
    EXPECT_EQ(treeTranscript("body tube collapsed", view),
              "body tube collapsed: rows=4 {Rocket+, Sustainer+, Nose Cone+, Body Tube-, Inner "
              "Tube(hidden), Mass Component(hidden), Centering Ring(hidden), Parachute(hidden), } "
              "selected=[Body Tube]");
    document->addUndoPosition("Add Fins");
    stage.addChild(std::make_unique<BodyTube>(), 1);
    EXPECT_EQ(
        treeTranscript("second body tube inserted at 1 (first body tube collapsed before)", view),
        "second body tube inserted at 1 (first body tube collapsed before): rows=5 "
        "{Rocket+, Sustainer+, Nose Cone+, Body Tube+, Body Tube-, Inner Tube(hidden), Mass "
        "Component(hidden), Centering Ring(hidden), Parachute(hidden), } selected=[Body "
        "Tube]");

    document->addUndoPosition("Paste Body tube");
    stage.addChild(bodyTube.copyWithNewIds());
    EXPECT_EQ(treeTranscript("copy of body tube pasted", view),
              "copy of body tube pasted: rows=6 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Body "
              "Tube-, Inner Tube(hidden), Mass Component(hidden), Centering Ring(hidden), "
              "Parachute(hidden), Body Tube-, Inner Tube(hidden), Mass Component(hidden), "
              "Centering Ring(hidden), Parachute(hidden), } selected=[Body Tube]");

    view.collapse(model.indexOf(bodyTube));
    selection->setSelectedComponent(innerTube);
    EXPECT_EQ(treeTranscript("before undo", view),
              "before undo: rows=9 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Body Tube+, Inner "
              "Tube-, Mass Component(hidden), Centering Ring-, Parachute-, Body Tube-, Inner "
              "Tube(hidden), Mass Component(hidden), Centering Ring(hidden), Parachute(hidden), } "
              "selected=[Inner Tube]");
    document->undo();
    EXPECT_EQ(treeTranscript("after undo", view),
              "after undo: rows=9 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Body Tube+, Inner "
              "Tube+, Mass Component+, Centering Ring-, Parachute-, } selected=[Inner Tube]");
    document->redo();
    EXPECT_EQ(treeTranscript("after redo", view),
              "after redo: rows=14 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Body Tube+, Inner "
              "Tube+, Mass Component+, Centering Ring-, Parachute-, Body Tube+, Inner Tube+, Mass "
              "Component+, Centering Ring-, Parachute-, } selected=[Inner Tube]");
}

// The expectations below are the transcript of the Java probe ViewProbe of this part (OpenRocket
// 5f164fd0e with DesignPanel's selection listener; probes/m2r2-T2/java/ViewProbe.out).

TEST_F(ComponentTreeViewTest, CollapsingMovesTheSelectionOffHiddenRows)
{
    select({m_noseCone, m_inner});
    EXPECT_EQ(transcript("nose cone and inner tube selected"),
              "nose cone and inner tube selected: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body "
              "Tube+, Inner Tube+, Mass Component+, Trapezoidal Fin Set-, } selected=[Nose Cone, "
              "Inner Tube]");
    collapse(*m_bodyTube);
    EXPECT_EQ(transcript("body tube collapsed"),
              "body tube collapsed: rows=4 {Rocket+, Sustainer+, Nose Cone+, Body Tube-, Inner "
              "Tube(hidden), Mass Component(hidden), Trapezoidal Fin Set(hidden), } "
              "selected=[Nose Cone, Body Tube]");
    expand(*m_bodyTube);
    EXPECT_EQ(transcript("body tube expanded again"),
              "body tube expanded again: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, "
              "Inner Tube+, Mass Component+, Trapezoidal Fin Set-, } selected=[Nose Cone, Body "
              "Tube]");
    select({m_bodyTube, m_mass});
    collapse(*m_inner);
    EXPECT_EQ(transcript("body tube and mass selected, inner tube collapsed"),
              "body tube and mass selected, inner tube collapsed: rows=6 {Rocket+, Sustainer+, "
              "Nose Cone+, Body Tube+, Inner Tube-, Mass Component(hidden), Trapezoidal Fin Set-, "
              "} selected=[Body Tube, Inner Tube]");
    select({m_noseCone});
    collapse(*m_bodyTube);
    EXPECT_EQ(transcript("nose cone selected, body tube collapsed"),
              "nose cone selected, body tube collapsed: rows=4 {Rocket+, Sustainer+, Nose Cone+, "
              "Body Tube-, Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin "
              "Set(hidden), } selected=[Nose Cone]");
}

TEST_F(ComponentTreeViewTest, CollapsingMovesTheCurrentRowToo)
{
    m_selection->setSelectedComponent(*m_mass);
    EXPECT_EQ(m_view->currentIndex(), index(*m_mass));
    collapse(*m_bodyTube);
    EXPECT_EQ(m_view->currentIndex(), index(*m_bodyTube));
    EXPECT_THAT(selectedNames(), ElementsAre("Body Tube"));
}

TEST_F(ComponentTreeViewTest, RemovalSelectsTheNearestRemainingAncestor)
{
    select({m_inner});
    std::unique_ptr<RocketComponent> inner = m_bodyTube->removeChild(m_inner);
    EXPECT_EQ(transcript("selected inner tube removed"),
              "selected inner tube removed: rows=5 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, "
              "Trapezoidal Fin Set-, } selected=[Body Tube]");

    m_bodyTube->addChild(std::move(inner), 0);
    select({m_noseCone, m_mass});
    std::unique_ptr<RocketComponent> bodyTube = m_stage->removeChild(m_bodyTube);
    EXPECT_EQ(transcript("nose cone and mass selected, body tube removed"),
              "nose cone and mass selected, body tube removed: rows=3 {Rocket+, Sustainer+, Nose "
              "Cone+, } selected=[Nose Cone, Sustainer]");

    m_stage->addChild(std::move(bodyTube));
    select({m_inner, m_mass});
    inner = m_bodyTube->removeChild(m_inner);
    EXPECT_EQ(transcript("inner tube and its mass selected, inner tube removed"),
              "inner tube and its mass selected, inner tube removed: rows=5 {Rocket+, Sustainer+, "
              "Nose Cone+, Body Tube+, Trapezoidal Fin Set-, } selected=[Body Tube]");
}

TEST_F(ComponentTreeViewTest, RemovalMovesTheCurrentRowToTheNearestAncestor)
{
    m_selection->setSelectedComponent(*m_mass);
    const std::unique_ptr<RocketComponent> inner = m_bodyTube->removeChild(m_inner);
    EXPECT_EQ(m_view->currentIndex(), index(*m_bodyTube));
}

TEST_F(ComponentTreeViewTest, MoveKeepsTheSelectionAndTheExpansion)
{
    collapse(*m_inner);
    select({m_fins, m_noseCone});
    m_stage->moveChild(m_bodyTube, 0);
    EXPECT_EQ(transcript("fins and nose cone selected, body tube moved to the front"),
              "fins and nose cone selected, body tube moved to the front: rows=6 {Rocket+, "
              "Sustainer+, Body Tube+, Inner Tube-, Mass Component(hidden), Trapezoidal Fin Set-, "
              "Nose Cone+, } selected=[Trapezoidal Fin Set, Nose Cone]");
}

TEST_F(ComponentTreeViewTest, ACollapsedStageStaysCollapsedWhenAStageIsAdded)
{
    collapse(*m_stage);
    EXPECT_EQ(transcript("stage collapsed"),
              "stage collapsed: rows=2 {Rocket+, Sustainer-, Nose Cone(hidden), Body Tube(hidden), "
              "Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin Set(hidden), } "
              "selected=[]");
    rocket().addChild(std::make_unique<AxialStage>());
    EXPECT_EQ(transcript("second stage added"),
              "second stage added: rows=3 {Rocket+, Sustainer-, Nose Cone(hidden), Body "
              "Tube(hidden), Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin "
              "Set(hidden), Stage+, } selected=[]");
    // The rows that were hidden lost their expansion (JTree keeps the visible ones only).
    expand(*m_stage);
    EXPECT_EQ(transcript("first stage expanded again"),
              "first stage expanded again: rows=5 {Rocket+, Sustainer+, Nose Cone+, Body Tube-, "
              "Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin Set(hidden), Stage+, } "
              "selected=[]");
}

TEST_F(ComponentTreeViewTest, FrozenMoveExpandsTheSourceStage)
{
    collapse(*m_inner);
    collapse(*m_stage);
    EXPECT_EQ(transcript("inner tube and stage collapsed"),
              "inner tube and stage collapsed: rows=2 {Rocket+, Sustainer-, Nose Cone(hidden), "
              "Body Tube(hidden), Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin "
              "Set(hidden), } selected=[]");
    rocket().freeze();
    std::unique_ptr<RocketComponent> bodyTube = m_stage->removeChild(m_bodyTube);
    m_stage->addChild(std::move(bodyTube), 0);
    rocket().thaw();
    EXPECT_EQ(transcript("body tube moved to the front while frozen"),
              "body tube moved to the front while frozen: rows=4 {Rocket+, Sustainer+, Body Tube-, "
              "Inner Tube(hidden), Mass Component(hidden), Trapezoidal Fin Set(hidden), Nose "
              "Cone+, } selected=[]");
}

TEST_F(ComponentTreeViewTest, AChildAddedToACollapsedRowShowsIt)
{
    collapse(*m_bodyTube);
    EXPECT_EQ(transcript("body tube collapsed"),
              "body tube collapsed: rows=4 {Rocket+, Sustainer+, Nose Cone+, Body Tube-, Inner "
              "Tube(hidden), Mass Component(hidden), Trapezoidal Fin Set(hidden), } selected=[]");
    m_inner->addChild(std::make_unique<Parachute>());
    EXPECT_EQ(transcript("parachute added to the inner tube"),
              "parachute added to the inner tube: rows=8 {Rocket+, Sustainer+, Nose Cone+, Body "
              "Tube+, Inner Tube+, Mass Component+, Parachute-, Trapezoidal Fin Set-, } "
              "selected=[]");
    collapse(*m_bodyTube);
    m_bodyTube->addChild(std::make_unique<Parachute>());
    EXPECT_EQ(transcript("body tube collapsed again, parachute added to it"),
              "body tube collapsed again, parachute added to it: rows=7 {Rocket+, Sustainer+, Nose "
              "Cone+, Body Tube+, Inner Tube-, Mass Component(hidden), Parachute(hidden), "
              "Trapezoidal Fin Set-, Parachute-, } selected=[]");
}

// Changes made while the view answers a reset (a slot of modelReset) come in one more reset: a
// child added to a collapsed row there shows too, also when other changes follow it (the model
// keeps the source of the tree change, ComponentTreeModel).
TEST_F(ComponentTreeViewTest, AChildAddedToACollapsedRowDuringAResetShowsIt)
{
    bool changed = false;
    QObject::connect(m_model.get(), &QAbstractItemModel::modelReset, m_view.get(), [&] {
        if (!changed)
        {
            changed = true;
            collapse(*m_bodyTube);
            m_bodyTube->addChild(std::make_unique<Parachute>());
            m_noseCone->setName("Nose");
            m_noseCone->setName("Nose Cone");
        }
    });

    m_noseCone->addChild(std::make_unique<MassComponent>());

    EXPECT_TRUE(m_view->isExpanded(index(*m_bodyTube)));
    EXPECT_EQ(transcript("parachute added during the reset"),
              "parachute added during the reset: rows=8 {Rocket+, Sustainer+, Nose Cone+, Mass "
              "Component+, Body Tube+, Inner Tube-, Mass Component(hidden), Trapezoidal Fin Set-, "
              "Parachute-, } selected=[]");
}

TEST_F(ComponentTreeViewTest, SelectionOrderSurvivesAChange)
{
    select({m_fins, m_noseCone, m_inner});
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(transcript("fins, nose cone, inner tube selected, body tube added"),
              "fins, nose cone, inner tube selected, body tube added: rows=8 {Rocket+, Sustainer+, "
              "Nose Cone+, Body Tube+, Inner Tube+, Mass Component+, Trapezoidal Fin Set-, Body "
              "Tube+, } selected=[Trapezoidal Fin Set, Nose Cone, Inner Tube]");
}

TEST_F(ComponentTreeViewTest, ASelectedRocketStaysSelected)
{
    select({&rocket(), m_noseCone});
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(transcript("rocket and nose cone selected, body tube added"),
              "rocket and nose cone selected, body tube added: rows=8 {Rocket+, Sustainer+, Nose "
              "Cone+, Body Tube+, Inner Tube+, Mass Component+, Trapezoidal Fin Set-, Body Tube+, "
              "} selected=[Rocket, Nose Cone]");
}

TEST_F(ComponentTreeViewTest, UndoAndRedoExpandEverythingAndKeepTheSelection)
{
    collapse(*m_inner);
    m_document->addUndoPosition("Add Parachute");
    const RocketComponent& parachute = m_inner->addChild(std::make_unique<Parachute>());
    m_selection->setSelectedComponent(parachute);
    collapse(*m_bodyTube);
    EXPECT_EQ(transcript("parachute added to the inner tube and selected, body tube collapsed"),
              "parachute added to the inner tube and selected, body tube collapsed: rows=4 "
              "{Rocket+, Sustainer+, Nose Cone+, Body Tube-, Inner Tube(hidden), Mass "
              "Component(hidden), Parachute(hidden), Trapezoidal Fin Set(hidden), } "
              "selected=[Body Tube]");
    m_document->undo();
    EXPECT_EQ(transcript("undo"),
              "undo: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner Tube+, Mass "
              "Component+, Trapezoidal Fin Set-, } selected=[Body Tube]");
    // The stage as it is now: the undo replaced every component object.
    collapse(rocket().getChild(0));
    m_document->redo();
    EXPECT_EQ(transcript("stage collapsed, redo"),
              "stage collapsed, redo: rows=8 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner "
              "Tube+, Mass Component+, Parachute-, Trapezoidal Fin Set-, } selected=[Sustainer]");
}

TEST_F(ComponentTreeViewTest, AChangeOfNoTreeKeepsTheState)
{
    collapse(*m_inner);
    select({m_fins, m_noseCone});
    int resets = 0;
    QObject::connect(m_model.get(), &ComponentTreeModel::modelReset, m_view.get(),
                     [&resets] { ++resets; });
    m_noseCone->setName("Pointy");
    EXPECT_EQ(resets, 0);
    EXPECT_EQ(transcript("state"),
              "state: rows=6 {Rocket+, Sustainer+, Pointy+, Body Tube+, Inner Tube-, Mass "
              "Component(hidden), Trapezoidal Fin Set-, } selected=[Trapezoidal Fin Set, Pointy]");
}

TEST_F(ComponentTreeViewTest, RestoresTheScrollBarsAndShowsTheSelection)
{
    for (int i = 0; i < 40; ++i)
    {
        m_stage->addChild(std::make_unique<BodyTube>());
    }
    m_view->resize(200, 120);
    m_view->show();
    QCoreApplication::processEvents();
    QScrollBar* bar = m_view->verticalScrollBar();
    ASSERT_GT(bar->maximum(), 0);

    bar->setValue(bar->maximum() / 2);
    const int middle = bar->value();
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(bar->value(), middle);

    // A selected row out of sight is scrolled into view after the change.
    m_selection->setSelectedComponent(*m_noseCone);
    bar->setValue(bar->maximum());
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_TRUE(m_view->viewport()->rect().intersects(m_view->visualRect(index(*m_noseCone))));
    EXPECT_LT(bar->value(), bar->maximum());
}

// ======================================================================= the selection

TEST_F(ComponentTreeViewTest, ASelectionScrollsIntoViewAndExpandsItsAncestors)
{
    collapse(*m_bodyTube);
    m_selection->setSelectedComponent(*m_mass);
    EXPECT_TRUE(m_view->isExpanded(index(*m_bodyTube)));
    EXPECT_TRUE(m_view->isExpanded(index(*m_inner)));
    EXPECT_TRUE(m_view->isRowVisible(index(*m_mass)));
}

TEST_F(ComponentTreeViewTest, CtrlASelectsTheVisibleRowsButTheRocket)
{
    collapse(*m_inner);
    QTest::keyClick(m_view.get(), Qt::Key_A, Qt::ControlModifier);
    EXPECT_EQ(transcript("inner tube collapsed, Ctrl+A"),
              "inner tube collapsed, Ctrl+A: rows=6 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, "
              "Inner Tube-, Mass Component(hidden), Trapezoidal Fin Set-, } selected=[Sustainer, "
              "Nose Cone, Body Tube, Inner Tube, Trapezoidal Fin Set]");
    m_selection->setSelectedComponent(*m_noseCone);
    m_view->selectAll();
    EXPECT_THAT(selectedNames(), ElementsAre("Sustainer", "Nose Cone", "Body Tube", "Inner Tube",
                                             "Trapezoidal Fin Set"));
}

TEST_F(ComponentTreeViewTest, LeavesTheWindowsKeysToTheWindow)
{
    m_selection->setSelectedComponent(*m_noseCone);
    QTest::keyClick(m_view.get(), Qt::Key_C, Qt::ControlModifier);
    QTest::keyClick(m_view.get(), Qt::Key_X, Qt::ControlModifier);
    QTest::keyClick(m_view.get(), Qt::Key_V, Qt::ControlModifier);
    QTest::keyClick(m_view.get(), Qt::Key_O, Qt::ControlModifier);
    EXPECT_TRUE(QGuiApplication::clipboard()->text().isEmpty());
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone"));
    EXPECT_EQ(m_stage->getChildCount(), 2U);
}

// ============================================================================== the mouse

TEST_F(ComponentTreeViewTest, ClickOnBlankSpaceClearsTheSelection)
{
    show();
    select({m_noseCone, m_inner});
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {}, blank());
    EXPECT_THAT(selectedNames(), IsEmpty());

    select({m_noseCone, m_inner});
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::ControlModifier, blank());
    EXPECT_THAT(selectedNames(), IsEmpty());

    select({m_noseCone});
    QTest::mouseClick(m_view->viewport(), Qt::RightButton, {}, blank());
    EXPECT_THAT(selectedNames(), IsEmpty());
    EXPECT_THAT(m_menus, IsEmpty());
}

TEST_F(ComponentTreeViewTest, ClickSelectsTheRow)
{
    show();
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {}, center(*m_bodyTube));
    EXPECT_THAT(selectedNames(), ElementsAre("Body Tube"));
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::ControlModifier, center(*m_noseCone));
    EXPECT_THAT(selectedNames(), ElementsAre("Body Tube", "Nose Cone"));
    EXPECT_THAT(m_edits, IsEmpty());
}

// JTree's getPathBetweenRows(): a range is in the order of the rows, whichever way it was
// picked (QTreeView would put the inner tube, a child, before the rows of the stage).
TEST_F(ComponentTreeViewTest, ARangeIsSelectedInTheOrderOfTheRows)
{
    show();
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {}, center(*m_inner));
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::ShiftModifier, center(*m_noseCone));
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone", "Body Tube", "Inner Tube"));
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, Qt::ShiftModifier, center(*m_fins));
    EXPECT_THAT(selectedNames(),
                ElementsAre("Inner Tube", "Mass Component", "Trapezoidal Fin Set"));
    // Dragging from the blank space below the rows up to the stage: every row in between, from
    // the top (a drag that starts on a row drags its components).
    QWidget* viewport = m_view->viewport();
    QTest::mousePress(viewport, Qt::LeftButton, {}, blank());
    QTest::mouseMove(viewport, center(*m_inner));
    QTest::mouseMove(viewport, center(*m_stage));
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, center(*m_stage));
    EXPECT_THAT(selectedNames(), ElementsAre("Sustainer", "Nose Cone", "Body Tube", "Inner Tube",
                                             "Mass Component", "Trapezoidal Fin Set"));
}

TEST_F(ComponentTreeViewTest, RightClickSelectsAnUnselectedRowThenAsksForTheMenu)
{
    show();
    select({m_noseCone});
    rightClick(center(*m_bodyTube));
    EXPECT_THAT(selectedNames(), ElementsAre("Body Tube"));
    ASSERT_EQ(m_menus.size(), 1U);
    EXPECT_EQ(m_menus.front(), m_view->viewport()->mapToGlobal(center(*m_bodyTube)));

    // With Ctrl held, Qt leaves the selection alone on the press; OpenRocket selects the row.
    select({m_noseCone});
    rightClick(center(*m_inner), Qt::ControlModifier);
    EXPECT_THAT(selectedNames(), ElementsAre("Inner Tube"));
    EXPECT_EQ(m_menus.size(), 2U);
}

TEST_F(ComponentTreeViewTest, RightClickOnASelectedRowKeepsTheSelection)
{
    show();
    select({m_noseCone, m_bodyTube, m_fins});
    rightClick(center(*m_bodyTube));
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone", "Body Tube", "Trapezoidal Fin Set"));
    EXPECT_EQ(m_menus.size(), 1U);
}

TEST_F(ComponentTreeViewTest, ContextMenuOnBlankSpaceClearsTheSelection)
{
    show();
    select({m_noseCone});
    const QPoint      position = blank();
    QContextMenuEvent event(QContextMenuEvent::Mouse, position,
                            m_view->viewport()->mapToGlobal(position));
    QApplication::sendEvent(m_view->viewport(), &event);
    EXPECT_THAT(selectedNames(), IsEmpty());
    EXPECT_THAT(m_menus, IsEmpty());
}

TEST_F(ComponentTreeViewTest, ContextMenuKeyAsksForTheMenuOfTheSelection)
{
    show();
    QContextMenuEvent none(QContextMenuEvent::Keyboard, QPoint(), QPoint());
    QApplication::sendEvent(m_view->viewport(), &none);
    EXPECT_THAT(m_menus, IsEmpty());

    select({m_noseCone, m_bodyTube});
    QContextMenuEvent event(QContextMenuEvent::Keyboard, QPoint(), QPoint());
    QApplication::sendEvent(m_view->viewport(), &event);
    ASSERT_EQ(m_menus.size(), 1U);
    EXPECT_EQ(m_menus.front(), m_view->viewport()->mapToGlobal(center(*m_noseCone)));
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone", "Body Tube"));
}

TEST_F(ComponentTreeViewTest, DoubleClickAsksToEditTheRow)
{
    show();
    select({m_noseCone, m_fins});
    doubleClick(center(*m_bodyTube));
    ASSERT_EQ(m_edits.size(), 1U);
    EXPECT_EQ(m_edits.front().component, m_bodyTube);
    EXPECT_FALSE(m_edits.front().withSelection);
    EXPECT_THAT(selectedNames(), ElementsAre("Body Tube"));
    // BasicTree's toggleClickCount 0: the double-click neither collapses nor expands.
    EXPECT_TRUE(m_view->isExpanded(index(*m_bodyTube)));
}

TEST_F(ComponentTreeViewTest, CtrlDoubleClickAddsTheRowAndAsksToEditTheSelection)
{
    show();
    select({m_noseCone, m_fins});
    doubleClick(center(*m_bodyTube), Qt::ControlModifier);
    ASSERT_EQ(m_edits.size(), 1U);
    EXPECT_EQ(m_edits.front().component, m_bodyTube);
    EXPECT_TRUE(m_edits.front().withSelection);
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone", "Trapezoidal Fin Set", "Body Tube"));

    // A selected row: the first click deselects it, the double-click selects it again.
    m_edits.clear();
    doubleClick(center(*m_noseCone), Qt::ControlModifier);
    ASSERT_EQ(m_edits.size(), 1U);
    EXPECT_EQ(m_edits.front().component, m_noseCone);
    EXPECT_TRUE(m_edits.front().withSelection);
    EXPECT_THAT(selectedNames(), ElementsAre("Trapezoidal Fin Set", "Body Tube", "Nose Cone"));
}

TEST_F(ComponentTreeViewTest, ShiftDoubleClickAsksToEditTheSelection)
{
    show();
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {}, center(*m_noseCone));
    doubleClick(center(*m_inner), Qt::ShiftModifier);
    ASSERT_EQ(m_edits.size(), 1U);
    EXPECT_EQ(m_edits.front().component, m_inner);
    EXPECT_TRUE(m_edits.front().withSelection);
    // The press with Shift selects the range from the nose cone.
    EXPECT_THAT(selectedNames(), ElementsAre("Nose Cone", "Body Tube", "Inner Tube"));
}

TEST_F(ComponentTreeViewTest, DoubleClickElsewhereAsksNothing)
{
    show();
    select({m_noseCone});
    doubleClick(blank());
    EXPECT_THAT(selectedNames(), IsEmpty());
    doubleClick(center(*m_bodyTube), {}, Qt::RightButton);
    // The first click of the left button only; the middle and right do not edit.
    EXPECT_THAT(m_edits, IsEmpty());
    // On the expand handle of the body tube, left of its row.
    const QRect row = m_view->visualRect(index(*m_bodyTube));
    doubleClick(QPoint(row.left() - (ComponentTreeView::kIndentation / 2), row.center().y()));
    EXPECT_THAT(m_edits, IsEmpty());
}

TEST_F(ComponentTreeViewTest, DoubleClickAsksNothingWhileEditingIsNotAllowed)
{
    show();
    bool allowed = false;
    m_view->setEditAllowed([&allowed] { return allowed; });
    doubleClick(center(*m_bodyTube));
    EXPECT_THAT(m_edits, IsEmpty());
    allowed = true;
    doubleClick(center(*m_bodyTube));
    EXPECT_EQ(m_edits.size(), 1U);
    m_view->setEditAllowed({});
    doubleClick(center(*m_bodyTube));
    EXPECT_EQ(m_edits.size(), 2U);
}

TEST_F(ComponentTreeViewTest, DoubleClickOnTheRocketAsksToEditIt)
{
    show();
    doubleClick(m_view->visualRect(m_model->rocketIndex()).center());
    ASSERT_EQ(m_edits.size(), 1U);
    EXPECT_EQ(m_edits.front().component, &rocket());
}

TEST_F(ComponentTreeViewTest, MouseSelectionDoesNotScrollAtOnce)
{
    for (int i = 0; i < 40; ++i)
    {
        m_stage->addChild(std::make_unique<BodyTube>());
    }
    m_view->resize(200, 120);
    m_view->show();
    QCoreApplication::processEvents();
    // The first row that is cut by the bottom of the viewport.
    const QModelIndexList rows   = m_view->visibleRows();
    const int             bottom = m_view->viewport()->rect().bottom();
    const auto            cut = std::ranges::find_if(rows, [this, bottom](const QModelIndex& row) {
        return m_view->visualRect(row).bottom() > bottom;
    });
    ASSERT_NE(cut, rows.end());
    const int value = m_view->verticalScrollBar()->value();
    QTest::mouseClick(m_view->viewport(), Qt::LeftButton, {},
                      QPoint(20, m_view->visualRect(*cut).top() + 1));
    EXPECT_EQ(m_view->verticalScrollBar()->value(), value);
    EXPECT_EQ(m_view->selectionModel()->selectedIndexes(), QModelIndexList{*cut});
}

// ============================================================================== dragging

TEST_F(ComponentTreeViewTest, DragsTheSelectionsComponents)
{
    show();
    select({m_bodyTube, m_noseCone, m_inner});
    QWidget* viewport = m_view->viewport();
    QTest::mousePress(viewport, Qt::LeftButton, {}, center(*m_bodyTube));
    QTest::mouseMove(viewport, center(*m_bodyTube) + QPoint(0, 40));
    QTest::mouseMove(viewport, center(*m_bodyTube) + QPoint(0, 80));
    const QDrag* drag = m_view->findChild<QDrag*>();
    ASSERT_NE(drag, nullptr);
    const ComponentMimeData* data = ComponentMimeData::fromMimeData(drag->mimeData());
    ASSERT_NE(data, nullptr);
    EXPECT_THAT(data->componentIds(), ElementsAre(m_noseCone->getId(), m_bodyTube->getId()));
    EXPECT_FALSE(drag->pixmap().isNull());
    // A platform's drag takes the release; the offscreen one ends at once, and the view gets it.
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, center(*m_bodyTube));
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(m_view->findChild<QDrag*>(), nullptr);
    // Nothing removed: the offscreen platform's drag ends without a drop.
    EXPECT_EQ(transcript("dragged"),
              "dragged: rows=7 {Rocket+, Sustainer+, Nose Cone+, Body Tube+, Inner Tube+, Mass "
              "Component+, Trapezoidal Fin Set-, } selected=[Body Tube, Nose Cone, Inner Tube]");
}

TEST_F(ComponentTreeViewTest, DragsNothingWithTheRocketSelected)
{
    show();
    select({&rocket(), m_noseCone});
    QWidget* viewport = m_view->viewport();
    QTest::mousePress(viewport, Qt::LeftButton, {}, center(*m_noseCone));
    QTest::mouseMove(viewport, center(*m_noseCone) + QPoint(0, 40));
    QTest::mouseMove(viewport, center(*m_noseCone) + QPoint(0, 80));
    EXPECT_EQ(m_view->findChild<QDrag*>(), nullptr);
    QTest::mouseRelease(viewport, Qt::LeftButton, {}, center(*m_noseCone) + QPoint(0, 80));
}

// ================================================= the test rockets and the example designs

/// Every component of @p document selected in a view, then the changes of ComponentTreeModel's
/// tests (adds, a move, a removal, a frozen move, a frozen removal whose component is destroyed
/// before thaw(), five undos and five redos): after each, the selection holds the components
/// of the rocket as it is now (under the asan preset, reading them proves that no pointer of
/// before the change survives) and DocumentSelectionModel::changed() came once.
class ViewSteps
{
public:
    explicit ViewSteps(OpenRocketDocument& document)
      : m_document(&document),
        m_model(document, m_preferences),
        m_view(m_model),
        m_selection(makeChild<DocumentSelectionModel>(&m_view))
    {
        QObject::connect(m_selection, &DocumentSelectionModel::changed, &m_view,
                         [this] { ++m_changes; });
        selectEverything();
    }

    void run()
    {
        expectLiveSelection("everything selected", 1);
        RocketComponent& stage = rocket().getChild(0);
        m_document->addUndoPosition("Add Body Tube");
        RocketComponent& tube = stage.addChild(std::make_unique<BodyTube>());
        expectLiveSelection("a body tube added", 1);
        m_tube = tube.getId();
        m_document->addUndoPosition("Add Inner Tube");
        tube.addChild(std::make_unique<InnerTube>());
        expectLiveSelection("an inner tube added", 1);
        // The added components selected too, so that the removals below remove selected ones.
        selectEverything();
        expectLiveSelection("everything selected again", 1);
        m_document->addUndoPosition("Move Body Tube");
        stage.moveChild(&tube, 0);
        expectLiveSelection("the body tube moved", 1);
        m_document->addUndoPosition("Delete Inner Tube");
        static_cast<void>(tube.removeChild(0));
        expectLiveSelection("the inner tube removed", 1);
        frozenSteps();
        for (int i = 0; i < 5; ++i)
        {
            m_document->undo();
            expectLiveSelection("undo " + std::to_string(i + 1), 1);
            expectFullyExpanded();
        }
        for (int i = 0; i < 5; ++i)
        {
            m_document->redo();
            expectLiveSelection("redo " + std::to_string(i + 1), 1);
        }
    }

private:
    [[nodiscard]] Rocket& rocket() const { return m_document->getRocket(); }

    void selectEverything()
    {
        std::vector<RocketComponent*> all = rocket().getAllChildren();
        all.insert(all.begin(), &rocket());
        m_selection->setSelectedComponents(all);
    }

    void frozenSteps()
    {
        Rocket&          rocket = this->rocket();
        RocketComponent& stage  = rocket.getChild(0);
        m_document->addUndoPosition("Move Body Tube");
        rocket.freeze();
        std::unique_ptr<RocketComponent> taken = stage.removeChild(rocket.findComponent(m_tube));
        stage.addChild(std::move(taken));
        rocket.thaw();
        expectLiveSelection("the body tube moved while frozen", 1);
        m_document->addUndoPosition("Delete Body Tube");
        rocket.freeze();
        static_cast<void>(stage.removeChild(rocket.findComponent(m_tube)));
        // Before thaw() tells the tree: the selection holds the destroyed body tube's row, and
        // hands out the components that are still there.
        for (const RocketComponent* component : m_selection->selectedComponents())
        {
            EXPECT_EQ(rocket.findComponent(component->getId()), component);
        }
        rocket.thaw();
        expectLiveSelection("the body tube removed and destroyed while frozen", 1);
    }

    void expectLiveSelection(const std::string& step, int changes)
    {
        EXPECT_EQ(m_changes, changes) << step;
        m_changes                                    = 0;
        const std::vector<RocketComponent*> selected = m_selection->selectedComponents();
        const std::vector<Uuid>             ids      = m_selection->selectedComponentIds();
        ASSERT_EQ(selected.size(), ids.size()) << step;
        EXPECT_FALSE(selected.empty()) << step;
        for (std::size_t i = 0; i < selected.size(); ++i)
        {
            // Read through the pointer: a stale one is a heap-use-after-free under asan.
            EXPECT_EQ(selected[i]->getId(), ids[i]) << step;
            EXPECT_EQ(rocket().findComponent(ids[i]), selected[i]) << step;
        }
    }

    void expectFullyExpanded()
    {
        for (const RocketComponent& component : rocket().subtree())
        {
            if (component.getChildCount() > 0)
            {
                EXPECT_TRUE(m_view.isExpanded(m_model.indexOf(component)))
                    << component.getName() << " collapsed after an undo";
            }
        }
    }

    OpenRocketDocument*     m_document;
    InMemoryPreferences     m_preferences;
    ComponentTreeModel      m_model;
    ComponentTreeView       m_view;
    DocumentSelectionModel* m_selection;
    Uuid                    m_tube;
    int                     m_changes = 0;
};

class ComponentTreeViewTestRocket : public Gui::Test::GuiTest,
                                    public ::testing::WithParamInterface<TestRocketMaker>
{ };

TEST_P(ComponentTreeViewTestRocket, KeepsALiveSelectionThroughEveryChange)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(GetParam().make());
    ViewSteps(*document).run();
}

std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, ComponentTreeViewTestRocket,
                         ::testing::ValuesIn(QtRocket::Test::testRocketMakers()), makerTestName);

class ComponentTreeViewExample : public Gui::Test::GuiTest,
                                 public ::testing::WithParamInterface<GoldenExample>
{ };

TEST_P(ComponentTreeViewExample, KeepsALiveSelectionThroughEveryChange)
{
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(GetParam());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ViewSteps(*loaded->document).run();
}

INSTANTIATE_TEST_SUITE_P(Examples, ComponentTreeViewExample,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

}  // namespace

}  // namespace QtRocket::Gui
