#include "QtRocket/gui/tree/DocumentSelectionModel.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QItemSelectionModel>
#include <QObject>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/PodSet.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using ::testing::ElementsAre;
using ::testing::IsEmpty;

/// A design (the stage holds a nose cone and a body tube; the body tube an inner tube with a
/// mass component, a fin set, a pod set with a body tube and a booster set with a body tube),
/// its model, a view and the view's selection, whose changed() signals are counted.
class DocumentSelectionModelTest : public Gui::Test::GuiTest
{
protected:
    void SetUp() override
    {
        Gui::Test::GuiTest::SetUp();
        m_document    = OpenRocketDocumentFactory::createNewRocket();
        m_stage       = &rocket().getChild(0);
        m_noseCone    = &m_stage->addChild(std::make_unique<NoseCone>());
        m_bodyTube    = &m_stage->addChild(std::make_unique<BodyTube>());
        m_inner       = &m_bodyTube->addChild(std::make_unique<InnerTube>());
        m_mass        = &m_inner->addChild(std::make_unique<MassComponent>());
        m_fins        = &m_bodyTube->addChild(std::make_unique<TrapezoidFinSet>());
        m_pods        = &m_bodyTube->addChild(std::make_unique<PodSet>());
        m_podTube     = &m_pods->addChild(std::make_unique<BodyTube>());
        m_boosters    = &m_bodyTube->addChild(std::make_unique<ParallelStage>());
        m_boosterTube = &m_boosters->addChild(std::make_unique<BodyTube>());
        m_pods->setName("Pods");
        m_boosters->setName("Boosters");
        m_podTube->setName("Pod tube");
        m_boosterTube->setName("Booster tube");
        m_model     = std::make_unique<ComponentTreeModel>(*m_document, m_preferences);
        m_view      = std::make_unique<ComponentTreeView>(*m_model);
        m_selection = makeChild<DocumentSelectionModel>(m_view.get());
        QObject::connect(m_selection, &DocumentSelectionModel::changed, m_view.get(),
                         [this] { ++m_changes; });
    }

    void TearDown() override
    {
        m_view = nullptr;
        m_model.reset();
        m_document.reset();
        Gui::Test::GuiTest::TearDown();
    }

    [[nodiscard]] Rocket& rocket() const { return m_document->getRocket(); }

    /// The names of @p components.
    [[nodiscard]] static std::vector<std::string> names(
        const std::vector<RocketComponent*>& components)
    {
        std::vector<std::string> found;
        found.reserve(components.size());
        for (const RocketComponent* component : components)
        {
            found.push_back(component->getName());
        }
        return found;
    }

    [[nodiscard]] std::vector<std::string> selected() const
    {
        return names(m_selection->selectedComponents());
    }

    [[nodiscard]] std::vector<std::string> highlighted() const
    {
        return names(m_selection->highlightedComponents());
    }

    void select(const std::vector<RocketComponent*>& components)
    {
        m_selection->setSelectedComponents(components);
    }

    /// The selected components as a slot of changed() reads them, renewed at every changed().
    [[nodiscard]] const std::vector<RocketComponent*>& recordSelectionOnChange()
    {
        QObject::connect(m_selection, &DocumentSelectionModel::changed, m_view.get(),
                         [this] { m_seen = m_selection->selectedComponents(); });
        return m_seen;
    }

    /// The changed() signals since the last call.
    [[nodiscard]] int changes() { return std::exchange(m_changes, 0); }

    InMemoryPreferences                 m_preferences;
    std::unique_ptr<OpenRocketDocument> m_document;
    RocketComponent*                    m_stage       = nullptr;
    RocketComponent*                    m_noseCone    = nullptr;
    RocketComponent*                    m_bodyTube    = nullptr;
    RocketComponent*                    m_inner       = nullptr;
    RocketComponent*                    m_mass        = nullptr;
    RocketComponent*                    m_fins        = nullptr;
    RocketComponent*                    m_pods        = nullptr;
    RocketComponent*                    m_podTube     = nullptr;
    RocketComponent*                    m_boosters    = nullptr;
    RocketComponent*                    m_boosterTube = nullptr;
    std::unique_ptr<ComponentTreeModel> m_model;
    std::unique_ptr<ComponentTreeView>  m_view;
    DocumentSelectionModel*             m_selection = nullptr;
    int                                 m_changes   = 0;
    std::vector<RocketComponent*>       m_seen;
};

TEST_F(DocumentSelectionModelTest, BelongsToItsView)
{
    EXPECT_EQ(m_selection->parent(), m_view.get());
    EXPECT_EQ(&m_selection->view(), m_view.get());
    EXPECT_EQ(&m_selection->model(), m_model.get());
    EXPECT_EQ(&m_selection->document(), m_document.get());
    EXPECT_THROW(DocumentSelectionModel{nullptr}, BugError);
}

TEST_F(DocumentSelectionModelTest, SelectsNothingAtFirst)
{
    EXPECT_EQ(m_selection->selectedComponent(), nullptr);
    EXPECT_THAT(m_selection->selectedComponents(), IsEmpty());
    EXPECT_THAT(m_selection->selectedComponentIds(), IsEmpty());
    EXPECT_THAT(m_selection->highlightedComponents(), IsEmpty());
}

// The scouting's TreeProbe (OpenRocket 5f164fd0e): "document selection after
// setSelectionPaths(bt,nc): first=Body Tube all=[Body Tube, Nose Cone]".
TEST_F(DocumentSelectionModelTest, TheFirstSelectedIsTheSelectedComponent)
{
    select({m_bodyTube, m_noseCone});
    EXPECT_EQ(m_selection->selectedComponent(), m_bodyTube);
    EXPECT_THAT(selected(), ElementsAre("Body Tube", "Nose Cone"));
    EXPECT_THAT(m_selection->selectedComponentIds(),
                ElementsAre(m_bodyTube->getId(), m_noseCone->getId()));
    EXPECT_TRUE(m_selection->isSelected(*m_bodyTube));
    EXPECT_FALSE(m_selection->isSelected(*m_inner));
    EXPECT_EQ(m_view->currentIndex(), m_model->indexOf(*m_bodyTube));
}

// Qt keeps the order of the selection when a component leaves it (Java's DefaultTreeSelectionModel
// takes the order of a hash table then: TreeProbe printed [Body Tube, Nose Cone] and [Nose Cone,
// Body Tube] in two runs).
TEST_F(DocumentSelectionModelTest, KeepsTheOrderOfTheSelection)
{
    select({m_fins, m_noseCone, m_bodyTube});
    QItemSelectionModel& qt = *m_view->selectionModel();
    qt.select(m_model->indexOf(*m_fins), QItemSelectionModel::Deselect);
    EXPECT_THAT(selected(), ElementsAre("Nose Cone", "Body Tube"));
    qt.select(m_model->indexOf(*m_fins), QItemSelectionModel::Select);
    EXPECT_THAT(selected(), ElementsAre("Nose Cone", "Body Tube", "Trapezoidal Fin Set"));
}

TEST_F(DocumentSelectionModelTest, SetsTheSelection)
{
    m_selection->setSelectedComponent(*m_inner);
    EXPECT_THAT(selected(), ElementsAre("Inner Tube"));
    EXPECT_EQ(changes(), 1);
    m_selection->setSelectedComponent(*m_inner);
    EXPECT_EQ(changes(), 0);
    select({m_mass, nullptr, m_noseCone, m_mass});
    EXPECT_THAT(selected(), ElementsAre("Mass Component", "Nose Cone"));
    EXPECT_EQ(changes(), 1);
    m_selection->clearComponentSelection();
    EXPECT_THAT(selected(), IsEmpty());
    EXPECT_EQ(changes(), 1);
    m_selection->clearComponentSelection();
    EXPECT_EQ(changes(), 0);
}

// The first selected component decides OpenRocket's actions and add buttons, so a new order of
// the same components is a change, which Qt's selection model does not tell.
TEST_F(DocumentSelectionModelTest, TellsANewOrderOfTheSameComponents)
{
    select({m_inner, m_stage});
    EXPECT_EQ(changes(), 1);
    select({m_stage, m_inner});
    EXPECT_THAT(selected(), ElementsAre("Sustainer", "Inner Tube"));
    EXPECT_EQ(changes(), 1);
    select({m_stage, m_inner});
    EXPECT_EQ(changes(), 0);
    select({m_inner, m_noseCone});
    EXPECT_EQ(changes(), 1);
}

TEST_F(DocumentSelectionModelTest, LeavesOutComponentsNotInTheTree)
{
    const std::unique_ptr<OpenRocketDocument> other = OpenRocketDocumentFactory::createNewRocket();
    RocketComponent&                          foreign = other->getRocket().getChild(0);
    select({m_noseCone});
    m_selection->setSelectedComponent(foreign);
    EXPECT_THAT(selected(), IsEmpty());
    select({m_noseCone, &foreign, m_bodyTube});
    EXPECT_THAT(selected(), ElementsAre("Nose Cone", "Body Tube"));
    EXPECT_FALSE(m_selection->isSelected(foreign));
}

TEST_F(DocumentSelectionModelTest, TellsOnceAfterEveryChangeOfTheTree)
{
    // Nothing selected: the tree changed all the same.
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(changes(), 1);
    select({m_noseCone, m_inner});
    EXPECT_EQ(changes(), 1);
    m_stage->addChild(std::make_unique<BodyTube>());
    EXPECT_EQ(changes(), 1);
    EXPECT_THAT(selected(), ElementsAre("Nose Cone", "Inner Tube"));
    // A change that is not of the tree changes no row.
    m_noseCone->setName("Pointy");
    EXPECT_EQ(changes(), 0);
    // A removed selected component: its parent instead.
    const std::unique_ptr<RocketComponent> removed = m_bodyTube->removeChild(m_inner);
    EXPECT_EQ(changes(), 1);
    EXPECT_THAT(selected(), ElementsAre("Pointy", "Body Tube"));
}

TEST_F(DocumentSelectionModelTest, HandsOutTheRocketsComponentsAfterAnUndo)
{
    select({m_inner, m_fins});
    EXPECT_EQ(changes(), 1);
    m_document->addUndoPosition("Add body tube");
    m_stage->addChild(std::make_unique<BodyTube>());
    const Uuid inner = m_inner->getId();
    const Uuid fins  = m_fins->getId();
    EXPECT_EQ(changes(), 1);

    // What a listener reads while the undo is delivered: the restored selection, of the new
    // objects (every one the undo made).
    const std::vector<RocketComponent*>& seen = recordSelectionOnChange();
    m_document->undo();
    EXPECT_EQ(changes(), 1);
    ASSERT_EQ(seen.size(), 2U);
    EXPECT_EQ(seen[0], rocket().findComponent(inner));
    EXPECT_EQ(seen[1], rocket().findComponent(fins));
    EXPECT_EQ(m_selection->selectedComponents(), seen);
}

TEST_F(DocumentSelectionModelTest, HighlightsTheDescendantsOfAssemblies)
{
    select({m_bodyTube});
    EXPECT_THAT(highlighted(), ElementsAre("Body Tube"));
    select({m_stage});
    EXPECT_THAT(highlighted(),
                ElementsAre("Sustainer", "Nose Cone", "Body Tube", "Inner Tube", "Mass Component",
                            "Trapezoidal Fin Set", "Pods", "Pod tube", "Boosters", "Booster tube"));
    select({m_pods});
    EXPECT_THAT(highlighted(), ElementsAre("Pods", "Pod tube"));
    select({m_boosters, m_noseCone});
    EXPECT_THAT(highlighted(), ElementsAre("Boosters", "Nose Cone", "Booster tube"));
    // Each once: the selection first, then the descendants of each assembly in tree order.
    select({m_inner, m_stage});
    EXPECT_THAT(highlighted(),
                ElementsAre("Inner Tube", "Sustainer", "Nose Cone", "Body Tube", "Mass Component",
                            "Trapezoidal Fin Set", "Pods", "Pod tube", "Boosters", "Booster tube"));
    select({&rocket()});
    EXPECT_EQ(m_selection->highlightedComponents().size(), 11U);
    EXPECT_EQ(m_selection->highlightedComponents().front(), &rocket());
}

TEST_F(DocumentSelectionModelTest, AddsAssemblyDescendantsToAnyList)
{
    const std::vector<RocketComponent*> list{m_podTube, nullptr, m_pods, m_podTube};
    EXPECT_THAT(names(DocumentSelectionModel::withAssemblyDescendants(list)),
                ElementsAre("Pod tube", "Pods"));
    EXPECT_THAT(DocumentSelectionModel::withAssemblyDescendants({}), IsEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
