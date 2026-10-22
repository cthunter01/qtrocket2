#include "QtRocket/gui/tree/ComponentTreeModel.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QAbstractItemModel>
#include <QAbstractItemModelTester>
#include <QByteArray>
#include <QIcon>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QObject>
#include <QSignalSpy>
#include <QString>
#include <QStringList>
#include <QTreeView>
#include <QVariant>
#include <Qt>
#include <QtContainerFwd>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/file/LoadedDocument.h"
#include "QtRocket/gui/tree/ComponentMimeData.h"
#include "QtRocket/gui/tree/ComponentToolTip.h"
#include "QtRocket/gui/util/ComponentIcons.h"
#include "QtRocket/gui/util/Icons.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentChangeEvent.h"
#include "QtRocket/rocket/FlightConfiguration.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/NoseCone.h"
#include "QtRocket/rocket/ParallelStage.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/TrapezoidFinSet.h"
#include "QtRocket/util/Error.h"
#include "QtRocket/util/Uuid.h"
#include "goldens/GoldenExamples.h"
#include "rocket/TestRockets.h"

namespace QtRocket::Gui
{

namespace
{

using QtRocket::Test::GoldenExample;
using QtRocket::Test::TestRocketMaker;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ComponentTreeModelTest = Gui::Test::GuiTest;
using Id                     = Icons::Id;

// ============================================================================ the mirror

/// The differences between the rows of @p model below @p index and @p component's subtree, one
/// line each, added to @p differences.
void collectDifferences(const ComponentTreeModel& model, const QModelIndex& index,
                        const RocketComponent& component, std::vector<std::string>& differences)
{
    const std::string where = component.getName() + " " + component.getId().toString() + ": ";
    if (model.componentId(index) != component.getId())
    {
        differences.push_back(where + "another id");
    }
    if (model.component(index) != &component)
    {
        differences.push_back(where + "another component");
    }
    if (model.indexOf(component) != index || model.indexOf(component.getId()) != index)
    {
        differences.push_back(where + "indexOf() finds another row");
    }
    if (index.data().toString().toStdString() != component.toString())
    {
        differences.push_back(where + "the text " + index.data().toString().toStdString());
    }
    const Qt::ItemFlags flags = model.flags(index);
    if (flags.testFlag(Qt::ItemIsDropEnabled) != component.allowsChildren() ||
        flags.testFlag(Qt::ItemNeverHasChildren) == component.allowsChildren() ||
        flags.testFlag(Qt::ItemIsDragEnabled) != (component.getParent() != nullptr))
    {
        differences.push_back(where + "the flags");
    }
    if (model.rowCount(index) != static_cast<int>(component.getChildCount()))
    {
        differences.push_back(where + "the number of rows");
        return;
    }
    for (std::size_t i = 0; i < component.getChildCount(); ++i)
    {
        const QModelIndex child = model.index(static_cast<int>(i), 0, index);
        if (model.parent(child) != index)
        {
            differences.push_back(where + "the parent of row " + std::to_string(i));
        }
        collectDifferences(model, child, component.getChild(i), differences);
    }
}

/// The differences between the rows of @p model and the component tree of @p rocket.
std::vector<std::string> mirrorDifferences(const ComponentTreeModel& model, const Rocket& rocket)
{
    std::vector<std::string> differences;
    if (model.rowCount() != 1)
    {
        differences.emplace_back("not one top-level row");
        return differences;
    }
    collectDifferences(model, model.rocketIndex(), rocket, differences);
    return differences;
}

/// Whether @p type has the bit @p bit.
bool has(int type, ComponentChangeEvent::Type bit)
{
    return ComponentChangeEvent::matches(bit, type);
}

// ================================================================ add, remove, move, undo

/// A model of a document under QAbstractItemModelTester (in its Warning mode, whose complaints
/// are Qt warnings and so fail the test), shown by a QTreeView that, as the first prototype of
/// the scouting did, asks isExpanded() (which calls flags()) and data() of every row while the
/// model is about to be reset; then the steps: adds, a move, a removal, a frozen move, a frozen
/// removal whose component is destroyed before thaw() delivers its event, five undos and five
/// redos, the mirror compared with the component tree after every one.
class ModelSteps
{
public:
    explicit ModelSteps(OpenRocketDocument& document)
      : m_document(&document),
        m_model(document, m_preferences),
        m_tester(&m_model, QAbstractItemModelTester::FailureReportingMode::Warning)
    {
        m_view.setModel(&m_model);
        m_view.expandAll();
        QObject::connect(&m_model, &QAbstractItemModel::modelAboutToBeReset, &m_view,
                         [this] { walk(m_model.rocketIndex()); });
    }

    void run()
    {
        expectMirror("the design as it is");
        addComponents();
        moveComponent();
        removeComponent();
        frozenMove();
        frozenRemoveAndDestroy();
        undoAndRedo();
    }

private:
    [[nodiscard]] Rocket& rocket() { return m_document->getRocket(); }

    void expectMirror(std::string_view step)
    {
        EXPECT_THAT(mirrorDifferences(m_model, rocket()), IsEmpty()) << step;
    }

    /// Every row's expansion and data, as a view might ask them while the components may be
    /// gone (under the asan preset this proves that nothing reads a destroyed component).
    void walk(const QModelIndex& index)
    {
        static_cast<void>(m_view.isExpanded(index));
        static_cast<void>(index.data(Qt::DisplayRole));
        static_cast<void>(index.data(Qt::ToolTipRole));
        static_cast<void>(index.data(ComponentTreeModel::kMarkersRole));
        for (int row = 0; row < m_model.rowCount(index); ++row)
        {
            walk(m_model.index(row, 0, index));
        }
    }

    void addComponents()
    {
        RocketComponent& stage = rocket().getChild(0);
        m_document->addUndoPosition("Add Body Tube");
        RocketComponent& tube = stage.addChild(std::make_unique<BodyTube>());
        m_tube                = tube.getId();
        expectMirror("a body tube added to the first stage");
        EXPECT_EQ(m_model.lastResetSource(), stage.getId());
        EXPECT_TRUE(has(m_model.lastResetChangeType(), ComponentChangeEvent::Type::TREE));
        EXPECT_FALSE(has(m_model.lastResetChangeType(), ComponentChangeEvent::Type::UNDO));

        m_document->addUndoPosition("Add Inner Tube");
        tube.addChild(std::make_unique<InnerTube>());
        expectMirror("an inner tube added to it");
        EXPECT_EQ(m_model.lastResetSource(), m_tube);
    }

    void moveComponent()
    {
        m_document->addUndoPosition("Move Body Tube");
        rocket().getChild(0).moveChild(rocket().findComponent(m_tube), 0);
        expectMirror("the body tube moved to the front of the stage");
    }

    void removeComponent()
    {
        m_document->addUndoPosition("Delete Inner Tube");
        const std::unique_ptr<RocketComponent> removed =
            rocket().findComponent(m_tube)->removeChild(0);
        expectMirror("the inner tube removed");
    }

    void frozenMove()
    {
        m_document->addUndoPosition("Move Body Tube");
        Rocket&          rocket = this->rocket();
        RocketComponent& stage  = rocket.getChild(0);
        rocket.freeze();
        std::unique_ptr<RocketComponent> taken = stage.removeChild(rocket.findComponent(m_tube));
        stage.addChild(std::move(taken));
        rocket.thaw();
        expectMirror("the body tube moved to the end of the stage while frozen");
    }

    void frozenRemoveAndDestroy()
    {
        m_document->addUndoPosition("Delete Body Tube");
        Rocket&           rocket = this->rocket();
        const QModelIndex stale  = m_model.indexOf(m_tube);
        rocket.freeze();
        // Destroyed at once, before thaw() delivers the event of its removal.
        static_cast<void>(rocket.getChild(0).removeChild(rocket.findComponent(m_tube)));
        EXPECT_EQ(m_model.component(stale), nullptr);
        EXPECT_FALSE(stale.data(Qt::DisplayRole).isValid());
        EXPECT_FALSE(stale.data(Qt::ToolTipRole).isValid());
        EXPECT_EQ(m_model.componentId(stale), m_tube);
        EXPECT_TRUE(m_model.flags(stale).testFlag(Qt::ItemIsDragEnabled));
        walk(m_model.rocketIndex());
        rocket.thaw();
        expectMirror("the body tube removed and destroyed while frozen");
        EXPECT_FALSE(m_model.indexOf(m_tube).isValid());
    }

    void undoAndRedo()
    {
        for (int i = 0; i < 5; ++i)
        {
            m_document->undo();
            expectMirror("undo " + std::to_string(i + 1));
            EXPECT_TRUE(has(m_model.lastResetChangeType(), ComponentChangeEvent::Type::TREE));
            EXPECT_TRUE(has(m_model.lastResetChangeType(), ComponentChangeEvent::Type::UNDO));
            EXPECT_EQ(m_model.lastResetSource(), rocket().getId());
        }
        for (int i = 0; i < 5; ++i)
        {
            m_document->redo();
            expectMirror("redo " + std::to_string(i + 1));
        }
    }

    OpenRocketDocument*      m_document;
    InMemoryPreferences      m_preferences;
    ComponentTreeModel       m_model;
    QAbstractItemModelTester m_tester;
    QTreeView                m_view;
    Uuid                     m_tube;
};

/// The test rockets of OpenRocket's TestRockets.
class ComponentTreeModelTestRocket : public Gui::Test::GuiTest,
                                     public ::testing::WithParamInterface<TestRocketMaker>
{ };

TEST_P(ComponentTreeModelTestRocket, MirrorsTheTreeThroughEveryChange)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createDocumentFromRocket(GetParam().make());
    ModelSteps(*document).run();
}

std::string makerTestName(const ::testing::TestParamInfo<TestRocketMaker>& info)
{
    std::string name{info.param.input};
    std::ranges::replace(name, '-', '_');
    return name;
}

INSTANTIATE_TEST_SUITE_P(Makers, ComponentTreeModelTestRocket,
                         ::testing::ValuesIn(QtRocket::Test::testRocketMakers()), makerTestName);

/// OpenRocket's sixteen example designs.
class ComponentTreeModelExample : public Gui::Test::GuiTest,
                                  public ::testing::WithParamInterface<GoldenExample>
{ };

TEST_P(ComponentTreeModelExample, MirrorsTheTreeThroughEveryChange)
{
    Result<LoadedDocument> loaded = QtRocket::Test::loadGoldenExample(GetParam());
    ASSERT_TRUE(loaded.has_value()) << loaded.error().message;
    ModelSteps(*loaded->document).run();
}

INSTANTIATE_TEST_SUITE_P(Examples, ComponentTreeModelExample,
                         ::testing::ValuesIn(QtRocket::Test::goldenExamples()),
                         QtRocket::Test::goldenExampleTestName);

// ============================================================================ the design

/// A new design whose stage holds a nose cone and a body tube, which holds an inner tube and a
/// fin set; the inner tube holds a mass component.
struct Design
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    InMemoryPreferences                 preferences;
    RocketComponent*                    stage    = &document->getRocket().getChild(0);
    RocketComponent*                    noseCone = &stage->addChild(std::make_unique<NoseCone>());
    RocketComponent*                    bodyTube = &stage->addChild(std::make_unique<BodyTube>());
    RocketComponent* innerTube = &bodyTube->addChild(std::make_unique<InnerTube>());
    RocketComponent* fins      = &bodyTube->addChild(std::make_unique<TrapezoidFinSet>());
    MassComponent*   mass      = &innerTube->addChild(std::make_unique<MassComponent>());

    [[nodiscard]] Rocket& rocket() const { return document->getRocket(); }
};

TEST_F(ComponentTreeModelTest, HasTheRocketAsItsOnlyTopLevelRow)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);

    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), 1);
    EXPECT_EQ(model.component(model.rocketIndex()), &design.rocket());
    EXPECT_FALSE(model.parent(model.rocketIndex()).isValid());
    EXPECT_FALSE(model.index(1, 0).isValid());
    EXPECT_FALSE(model.index(0, 1).isValid());
    EXPECT_EQ(&model.document(), design.document.get());
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

TEST_F(ComponentTreeModelTest, LetsEveryComponentButTheRocketBeDragged)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);

    // The Rocket: no drag (Java: createTransferable() returns null for it); it takes stages.
    EXPECT_EQ(model.flags(model.rocketIndex()),
              Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDropEnabled);
    EXPECT_EQ(
        model.flags(model.indexOf(*design.bodyTube)),
        Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    // A fin set allows no children: a leaf (Java's isLeaf()).
    EXPECT_EQ(model.flags(model.indexOf(*design.fins)), Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                                                            Qt::ItemIsDragEnabled |
                                                            Qt::ItemNeverHasChildren);
    // The blank space below the rows takes drops: the end of the Rocket (ComponentDrop, as
    // JTree's drop below the last row).
    EXPECT_EQ(model.flags(QModelIndex()), Qt::ItemIsDropEnabled);
}

TEST_F(ComponentTreeModelTest, ShowsTheNameTheIconAndTheToolTipOfAComponent)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    const QModelIndex        bodyTube = model.indexOf(*design.bodyTube);
    design.noseCone->setName("My nose");

    EXPECT_EQ(model.data(model.indexOf(*design.noseCone)).toString(), QStringLiteral("My nose"));
    EXPECT_EQ(model.data(bodyTube).toString(), QStringLiteral("Body Tube"));
    EXPECT_EQ(model.data(bodyTube, Qt::DecorationRole).value<QIcon>().cacheKey(),
              ComponentIcons::smallIcon(*design.bodyTube).cacheKey());
    EXPECT_EQ(
        model.data(bodyTube, Qt::ToolTipRole).toString(),
        ComponentToolTip::singleComponent(*design.bodyTube, design.preferences.getDefaultMach()));
    EXPECT_EQ(model.data(bodyTube, ComponentTreeModel::kComponentIdRole).toString(),
              QString::fromStdString(design.bodyTube->getId().toString()));
    // The Rocket has no icon.
    EXPECT_FALSE(model.data(model.rocketIndex(), Qt::DecorationRole).isValid());
    EXPECT_FALSE(model.data(bodyTube, Qt::EditRole).isValid());
}

TEST_F(ComponentTreeModelTest, ShowsTheIconOfAMassComponentsType)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    design.mass->setMassComponentType(MassComponent::MassComponentType::ALTIMETER);

    EXPECT_EQ(
        model.data(model.indexOf(*design.mass), Qt::DecorationRole).value<QIcon>().cacheKey(),
        ComponentIcons::smallMassTypeIcon(MassComponent::MassComponentType::ALTIMETER).cacheKey());
}

TEST_F(ComponentTreeModelTest, AnswersAnotherChangeWithTheDataOfEveryRow)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    const QSignalSpy         resets(&model, &QAbstractItemModel::modelReset);
    const QSignalSpy         changes(&model, &QAbstractItemModel::dataChanged);

    design.innerTube->setName("Motor mount");

    EXPECT_EQ(resets.count(), 0);
    // The Rocket's row, and the rows below each of the Rocket, the stage, the body tube and the
    // inner tube.
    EXPECT_EQ(changes.count(), 5);
    EXPECT_EQ(model.data(model.indexOf(*design.innerTube)).toString(),
              QStringLiteral("Motor mount"));
}

TEST_F(ComponentTreeModelTest, AnswersATreeChangeWithAReset)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    const QSignalSpy         aboutToReset(&model, &QAbstractItemModel::modelAboutToBeReset);
    const QSignalSpy         resets(&model, &QAbstractItemModel::modelReset);
    const QSignalSpy         inserts(&model, &QAbstractItemModel::rowsInserted);

    design.noseCone->addChild(std::make_unique<InnerTube>());

    EXPECT_EQ(aboutToReset.count(), 1);
    EXPECT_EQ(resets.count(), 1);
    EXPECT_EQ(inserts.count(), 0);
    EXPECT_EQ(model.lastResetSource(), design.noseCone->getId());
    EXPECT_EQ(model.lastResetChangeType() & ComponentChangeEvent::kTreeChange,
              ComponentChangeEvent::kTreeChange);
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

TEST_F(ComponentTreeModelTest, KnowsNoResetBeforeTheFirst)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);

    EXPECT_EQ(model.lastResetSource(), std::nullopt);
    EXPECT_EQ(model.lastResetChangeType(), 0);
}

TEST_F(ComponentTreeModelTest, ImplementsNoRowInsertionOrRemoval)
{
    Design             design;
    ComponentTreeModel model(*design.document, design.preferences);
    const QModelIndex  bodyTube = model.indexOf(*design.bodyTube);

    // A drag's QAbstractItemView::clearOrRemove() calls removeRows() after a move: it must not
    // remove anything (the drop has moved the components already).
    EXPECT_FALSE(model.removeRows(0, 1, bodyTube));
    EXPECT_FALSE(model.insertRows(0, 1, bodyTube));
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
    EXPECT_EQ(design.bodyTube->getChildCount(), 2U);
}

TEST_F(ComponentTreeModelTest, AnswersNothingForAComponentThatLeftBeforeItsEventArrived)
{
    Design                              design;
    std::unique_ptr<ComponentTreeModel> model;
    QVariant                            seen = QStringLiteral("not asked");
    // A listener of the rocket that comes before the model's asks for a row of the component that
    // the change removed, before the model has heard of it.
    const ComponentChangeSignal::ScopedConnection before{
        design.rocket().addComponentChangeListener([&](const ComponentChangeEvent& event) {
            if (event.isTreeChange() && model != nullptr)
            {
                seen = model->data(model->index(0, 0, model->indexOf(*design.bodyTube)));
            }
        })};
    model = std::make_unique<ComponentTreeModel>(*design.document, design.preferences);
    const Uuid innerTube = design.innerTube->getId();

    const std::unique_ptr<RocketComponent> removed = design.bodyTube->removeChild(design.innerTube);

    EXPECT_FALSE(seen.isValid());
    EXPECT_FALSE(model->indexOf(innerTube).isValid());
    EXPECT_THAT(mirrorDifferences(*model, design.rocket()), IsEmpty());
}

TEST_F(ComponentTreeModelTest, ResetsForAChangeThatArrivesBeforeTheTreeChangeItFollows)
{
    Design                              design;
    std::unique_ptr<ComponentTreeModel> model;
    // A listener before the model's renames the added component as it hears of the addition:
    // the renaming's event reaches the model first, while its mirror lacks the component.
    const ComponentChangeSignal::ScopedConnection before{
        design.rocket().addComponentChangeListener([&](const ComponentChangeEvent& event) {
            if (event.isTreeChange() && design.noseCone->getChildCount() == 1 &&
                design.noseCone->getChild(0).getName() != "Renamed")
            {
                design.noseCone->getChild(0).setName("Renamed");
            }
        })};
    model = std::make_unique<ComponentTreeModel>(*design.document, design.preferences);
    const QAbstractItemModelTester tester(model.get(),
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(model.get(), &QAbstractItemModel::modelReset);

    design.noseCone->addChild(std::make_unique<InnerTube>());

    EXPECT_EQ(resets.count(), 2);
    EXPECT_THAT(mirrorDifferences(*model, design.rocket()), IsEmpty());
    EXPECT_EQ(model->data(model->indexOf(design.noseCone->getChild(0))).toString(),
              QStringLiteral("Renamed"));
}

TEST_F(ComponentTreeModelTest, ResetsAgainForAChangeMadeWhileTheViewsAnswerAReset)
{
    Design                         design;
    ComponentTreeModel             model(*design.document, design.preferences);
    const QAbstractItemModelTester tester(&model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&model, &QAbstractItemModel::modelReset);
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&design] {
        if (design.noseCone->getChildCount() == 1)
        {
            static_cast<void>(design.noseCone->removeChild(0));
        }
    });

    design.noseCone->addChild(std::make_unique<InnerTube>());

    EXPECT_EQ(resets.count(), 2);
    EXPECT_EQ(design.noseCone->getChildCount(), 0U);
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

// Changes made while the views answer a reset come in one more reset, with all their bits and the
// source of the last that changed the tree (which the views make visible), as Rocket::thaw()
// merges the events of a freeze; Java answers each tree change with
// fireTreeStructureChanged(source).
TEST_F(ComponentTreeModelTest, ResetsOnceMoreForAllTheChangesMadeWhileTheViewsAnswerAReset)
{
    Design                         design;
    ComponentTreeModel             model(*design.document, design.preferences);
    const QAbstractItemModelTester tester(&model,
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(&model, &QAbstractItemModel::modelReset);
    bool                           changed = false;
    QObject::connect(&model, &QAbstractItemModel::modelReset, &model, [&design, &changed] {
        if (!changed)
        {
            changed = true;
            design.bodyTube->addChild(std::make_unique<MassComponent>());
            design.noseCone->setName("Nose");
            design.noseCone->setName("Nose cone");
        }
    });

    design.innerTube->addChild(std::make_unique<MassComponent>());

    EXPECT_EQ(resets.count(), 2);
    EXPECT_EQ(model.lastResetSource(), design.bodyTube->getId());
    EXPECT_TRUE(has(model.lastResetChangeType(), ComponentChangeEvent::Type::TREE));
    EXPECT_TRUE(has(model.lastResetChangeType(), ComponentChangeEvent::Type::NON_FUNCTIONAL));
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

/// A listener of @p rocket, connected now (so before a model made later), that asks @p model, once
/// there is one, at every change event for the data of the rows below the second row of the
/// Rocket's first row (the design's body tube), into @p seen.
[[nodiscard]] ComponentChangeSignal::ScopedConnection askingForTheBodyTubesRows(
    Rocket& rocket, const std::unique_ptr<ComponentTreeModel>& model, std::vector<QVariant>& seen)
{
    return ComponentChangeSignal::ScopedConnection{
        rocket.addComponentChangeListener([&model, &seen](const ComponentChangeEvent& /*event*/) {
            if (model == nullptr)
            {
                return;
            }
            const QModelIndex bodyTube =
                model->index(1, 0, model->index(0, 0, model->rocketIndex()));
            for (int row = 0; row < model->rowCount(bodyTube); ++row)
            {
                seen.push_back(model->data(model->index(row, 0, bodyTube)));
            }
        })};
}

// A change made while the rocket's events are off changes no modification id; enableEvents(true)
// tells of it with an AEROMASS event only. The model finds the tree changed (and its mirror's
// pointer of the removed component dangling, which it never reads: ASan), also for a listener
// that asks before the model's turn.
TEST_F(ComponentTreeModelTest, FindsAChangeMadeWhileTheRocketsEventsWereOff)
{
    Design                                        design;
    std::unique_ptr<ComponentTreeModel>           model;
    std::vector<QVariant>                         seen;
    const ComponentChangeSignal::ScopedConnection before =
        askingForTheBodyTubesRows(design.rocket(), model, seen);
    model = std::make_unique<ComponentTreeModel>(*design.document, design.preferences);
    const QAbstractItemModelTester tester(model.get(),
                                          QAbstractItemModelTester::FailureReportingMode::Warning);
    const QSignalSpy               resets(model.get(), &QAbstractItemModel::modelReset);
    const Uuid                     innerTube = design.innerTube->getId();

    design.rocket().enableEvents(false);
    static_cast<void>(design.bodyTube->removeChild(design.innerTube));
    design.rocket().enableEvents(true);

    EXPECT_EQ(resets.count(), 1);
    EXPECT_FALSE(model->indexOf(innerTube).isValid());
    EXPECT_THAT(mirrorDifferences(*model, design.rocket()), IsEmpty());
    // The listener asked for the rows of the body tube as the mirror had them: the removed inner
    // tube's row answered nothing, the fin set's its name.
    ASSERT_EQ(seen.size(), 2U);
    EXPECT_FALSE(seen[0].isValid());
    EXPECT_EQ(seen[1].toString(), QtStrings::fromUtf8(design.fins->getName()));
}

// During an undo's reset the components are the new ones: component() looks them up by id while the
// views answer modelAboutToBeReset (Rocket::loadFrom() has replaced every object, and an undo of a
// change that kept the tree gives the tree's modification id back).
TEST_F(ComponentTreeModelTest, GivesTheNewComponentsWhileAnUndoResets)
{
    Design             design;
    ComponentTreeModel model(*design.document, design.preferences);
    const std::string  name = design.noseCone->getName();
    design.document->addUndoPosition("Rename");
    design.noseCone->setName("Nose");
    const Uuid             noseCone = design.noseCone->getId();
    const RocketComponent* old      = design.noseCone;
    const RocketComponent* answered = nullptr;
    const RocketComponent* found    = nullptr;
    QObject::connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
        answered = model.component(model.indexOf(noseCone));
        found    = design.rocket().findComponent(noseCone);
    });

    design.document->undo();

    ASSERT_NE(found, nullptr);
    EXPECT_NE(found, old);
    EXPECT_EQ(answered, found);
    EXPECT_EQ(found->getName(), name);
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

// ============================================================================== mime data

TEST_F(ComponentTreeModelTest, DragsTheComponentsWithoutAnAncestorAmongThemInTreeOrder)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    const QModelIndexList indexes{model.indexOf(*design.innerTube), model.indexOf(*design.fins),
                                  model.indexOf(*design.bodyTube), model.indexOf(*design.noseCone)};

    const std::unique_ptr<QMimeData> data{model.mimeData(indexes)};

    const ComponentMimeData* drag = ComponentMimeData::fromMimeData(data.get());
    ASSERT_NE(drag, nullptr);
    EXPECT_THAT(drag->componentIds(),
                ElementsAre(design.noseCone->getId(), design.bodyTube->getId()));
    EXPECT_TRUE(drag->isFrom(*design.document));
    EXPECT_EQ(&drag->sourceDocument(), design.document.get());
    EXPECT_EQ(model.mimeTypes(), QStringList{ComponentMimeData::mimeType()});
    EXPECT_TRUE(data->hasFormat(ComponentMimeData::mimeType()));
    EXPECT_EQ(data->data(ComponentMimeData::mimeType()),
              QByteArray::fromStdString(design.noseCone->getId().toString() + "\n" +
                                        design.bodyTube->getId().toString() + "\n"));
}

TEST_F(ComponentTreeModelTest, DragsNothingWithTheRocketOrWithoutComponents)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);

    const std::unique_ptr<QMimeData> withRocket{
        model.mimeData({model.indexOf(*design.noseCone), model.rocketIndex()})};
    const std::unique_ptr<QMimeData> none{model.mimeData({})};

    EXPECT_EQ(withRocket, nullptr);
    EXPECT_EQ(none, nullptr);
    EXPECT_EQ(model.supportedDragActions(), Qt::CopyAction | Qt::MoveAction);
    EXPECT_EQ(model.supportedDropActions(), Qt::CopyAction | Qt::MoveAction);
}

// The drop rules themselves are ComponentDrop's (ComponentDropTests.cpp); the model answers Qt
// with them.
TEST_F(ComponentTreeModelTest, TakesDropsByTheRules)
{
    Design                           design;
    ComponentTreeModel               model(*design.document, design.preferences);
    const std::unique_ptr<QMimeData> data{model.mimeData({model.indexOf(*design.innerTube)})};
    const QModelIndex                fins     = model.indexOf(*design.fins);
    const QModelIndex                stage    = model.indexOf(*design.stage);
    const QModelIndex                bodyTube = model.indexOf(*design.bodyTube);
    QMimeData                        foreign;
    foreign.setText(QStringLiteral("not components"));

    // A fin set takes no inner tube, a stage neither; nor does anything take another program's
    // drag; and a row of the invalid root is no place.
    EXPECT_FALSE(model.canDropMimeData(data.get(), Qt::MoveAction, -1, 0, fins));
    EXPECT_FALSE(model.canDropMimeData(data.get(), Qt::MoveAction, 0, 0, stage));
    EXPECT_FALSE(model.canDropMimeData(&foreign, Qt::MoveAction, 0, 0, bodyTube));
    EXPECT_FALSE(model.canDropMimeData(data.get(), Qt::MoveAction, 1, 0, QModelIndex()));
    EXPECT_FALSE(model.canDropMimeData(data.get(), Qt::LinkAction, 0, 0, bodyTube));
    EXPECT_FALSE(model.dropMimeData(data.get(), Qt::MoveAction, -1, 0, fins));
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());

    // On the body tube's row (row -1): its first child, where the inner tube is already.
    EXPECT_TRUE(model.canDropMimeData(data.get(), Qt::MoveAction, -1, 0, bodyTube));
    EXPECT_FALSE(model.dropMimeData(data.get(), Qt::MoveAction, -1, 0, bodyTube));
    // After the fin set.
    EXPECT_TRUE(model.dropMimeData(data.get(), Qt::MoveAction, 2, 0, bodyTube));
    EXPECT_EQ(design.bodyTube->getChild(1).getId(), design.innerTube->getId());
    EXPECT_THAT(mirrorDifferences(model, design.rocket()), IsEmpty());
}

// ===================================================================== markers and states

TEST_F(ComponentTreeModelTest, MarksMassCgAndCdOverrides)
{
    Design design;
    design.bodyTube->setMassOverridden(true);
    design.bodyTube->setCGOverridden(true);
    design.bodyTube->setCDOverridden(true);
    design.bodyTube->setSubcomponentsOverriddenCG(true);
    const ComponentTreeModel model(*design.document, design.preferences);

    EXPECT_THAT(ComponentTreeModel::markers(*design.bodyTube),
                ElementsAre(Id::MASS_OVERRIDE, Id::CG_OVERRIDE, Id::CD_OVERRIDE));
    // The children: the CG overridden by their ancestor.
    EXPECT_THAT(ComponentTreeModel::markers(*design.mass),
                ElementsAre(Id::CG_OVERRIDE_SUBCOMPONENT));
    EXPECT_THAT(ComponentTreeModel::markersOf(model.indexOf(*design.innerTube)),
                ElementsAre(Id::CG_OVERRIDE_SUBCOMPONENT));
    EXPECT_THAT(ComponentTreeModel::markersOf(model.indexOf(*design.noseCone)), IsEmpty());
}

// The renderer's markers were recorded with OpenRocket 5f164fd0e by a probe of this part
// (RendererProbe.java, which calls ComponentTreeRenderer.getTreeCellRendererComponent() headless
// and measures the combined marker icon: 15 px per marker, 11 px for a subcomponent marker, 3 px
// apart): with the body tube's mass and CG overridden, the body tube 33 px (two markers) and the
// inner tube none; with its mass and CD then overridden with the subcomponents and the inner
// tube's own mass and CD overridden, the body tube 51 px (three) and the inner tube two
// subcomponent markers (its own overrides not shown).
TEST_F(ComponentTreeModelTest, MarksAnOverriddenSubcomponentRatherThanItsOwnOverride)
{
    Design design;
    design.bodyTube->setMassOverridden(true);
    design.bodyTube->setCGOverridden(true);

    EXPECT_THAT(ComponentTreeModel::markers(*design.bodyTube),
                ElementsAre(Id::MASS_OVERRIDE, Id::CG_OVERRIDE));
    EXPECT_THAT(ComponentTreeModel::markers(*design.innerTube), IsEmpty());

    design.bodyTube->setSubcomponentsOverriddenMass(true);
    design.bodyTube->setCDOverridden(true);
    design.bodyTube->setSubcomponentsOverriddenCD(true);
    design.innerTube->setMassOverridden(true);
    design.innerTube->setCDOverridden(true);

    EXPECT_THAT(ComponentTreeModel::markers(*design.bodyTube),
                ElementsAre(Id::MASS_OVERRIDE, Id::CG_OVERRIDE, Id::CD_OVERRIDE));
    EXPECT_THAT(ComponentTreeModel::markers(*design.innerTube),
                ElementsAre(Id::MASS_OVERRIDE_SUBCOMPONENT, Id::CD_OVERRIDE_SUBCOMPONENT));
}

TEST_F(ComponentTreeModelTest, MarksAHiddenComponent)
{
    Design design;
    design.noseCone->setVisible(false);
    const ComponentTreeModel model(*design.document, design.preferences);

    EXPECT_THAT(ComponentTreeModel::markers(*design.noseCone), ElementsAre(Id::COMPONENT_HIDDEN));
    EXPECT_TRUE(
        model.data(model.indexOf(*design.noseCone), ComponentTreeModel::kHiddenRole).toBool());
    EXPECT_FALSE(
        model.data(model.indexOf(*design.bodyTube), ComponentTreeModel::kHiddenRole).toBool());
}

/// A rocket of two stages, a sustainer with a body tube and a booster stage with a body tube
/// that holds a booster set with a nose cone, and an empty third stage.
struct Stages
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    InMemoryPreferences                 preferences;
    RocketComponent*                    sustainer = &document->getRocket().getChild(0);
    RocketComponent* sustainerTube = &sustainer->addChild(std::make_unique<BodyTube>());
    RocketComponent* booster     = &document->getRocket().addChild(std::make_unique<AxialStage>());
    RocketComponent* boosterTube = &booster->addChild(std::make_unique<BodyTube>());
    RocketComponent* boosters    = &boosterTube->addChild(std::make_unique<ParallelStage>());
    RocketComponent* boosterNose = &boosters->addChild(std::make_unique<NoseCone>());
    RocketComponent* empty       = &document->getRocket().addChild(std::make_unique<AxialStage>());

    /// Makes @p stage inactive in the selected configuration and tells the tree, as OpenRocket's
    /// stage toggling does (AEROMASS | MOTOR | TREE_CHILDREN).
    void deactivate(const RocketComponent& stage) const
    {
        Rocket& rocket = document->getRocket();
        rocket.getSelectedConfiguration().setStageActive(stage.getStageNumber(), false);
        rocket.fireComponentChangeEvent(ComponentChangeEvent::kAeromassChange |
                                        ComponentChangeEvent::kMotorChange |
                                        ComponentChangeEvent::kTreeChangeChildren);
    }
};

/// The names of those of @p components that @p model does not show as in an inactive stage.
std::vector<std::string> notShownInactive(const ComponentTreeModel&                  model,
                                          const std::vector<const RocketComponent*>& components)
{
    std::vector<std::string> names;
    for (const RocketComponent* component : components)
    {
        if (!ComponentTreeModel::isInInactiveStage(*component) ||
            !model.data(model.indexOf(*component), ComponentTreeModel::kInactiveRole).toBool())
        {
            names.push_back(component->getName());
        }
    }
    return names;
}

// RendererProbe (see above), with the booster stage and the empty stage set inactive: the
// booster stage, its body tube, the booster set and its nose cone in italics, the booster stage
// and the booster set with one marker each (the disabled one), the empty stage plain without a
// marker, the sustainer's components plain; selected, no row in italics.
TEST_F(ComponentTreeModelTest, MarksTheComponentsOfAnInactiveStage)
{
    Stages                   stages;
    const ComponentTreeModel model(*stages.document, stages.preferences);
    const QSignalSpy         changes(&model, &QAbstractItemModel::dataChanged);

    stages.deactivate(*stages.booster);
    stages.deactivate(*stages.empty);

    EXPECT_GT(changes.count(), 0);
    EXPECT_THAT(notShownInactive(model, {stages.booster, stages.boosterTube, stages.boosters,
                                         stages.boosterNose}),
                IsEmpty());
    // The disabled marker on the stages alone: the booster stage and the booster set within it.
    EXPECT_THAT(ComponentTreeModel::markers(*stages.booster), ElementsAre(Id::COMPONENT_DISABLED));
    EXPECT_THAT(ComponentTreeModel::markers(*stages.boosters), ElementsAre(Id::COMPONENT_DISABLED));
    EXPECT_THAT(ComponentTreeModel::markers(*stages.boosterTube), IsEmpty());
    EXPECT_THAT(ComponentTreeModel::markers(*stages.boosterNose), IsEmpty());
    EXPECT_FALSE(ComponentTreeModel::isInInactiveStage(*stages.sustainerTube));
    EXPECT_FALSE(ComponentTreeModel::isInInactiveStage(*stages.empty));
    EXPECT_FALSE(ComponentTreeModel::isInInactiveStage(stages.document->getRocket()));
}

TEST_F(ComponentTreeModelTest, DoesNotMarkAnEmptyStageAsInactive)
{
    Stages stages;

    stages.deactivate(*stages.empty);

    // isStageActive() is false for a stage without components; the renderer leaves it alone.
    EXPECT_FALSE(
        stages.document->getRocket().getSelectedConfiguration().isComponentActive(*stages.empty));
    EXPECT_FALSE(ComponentTreeModel::isInInactiveStage(*stages.empty));
    EXPECT_THAT(ComponentTreeModel::markers(*stages.empty), IsEmpty());
}

TEST_F(ComponentTreeModelTest, MarksNothingOfAComponentOutsideARocket)
{
    BodyTube tube;
    auto&    inner = tube.addChild(std::make_unique<InnerTube>());

    EXPECT_FALSE(ComponentTreeModel::isInInactiveStage(inner));
    EXPECT_THAT(ComponentTreeModel::markers(inner), IsEmpty());
}

// ============================================================================== tooltips

TEST_F(ComponentTreeModelTest, GivesTheToolTipOfASelection)
{
    Design                   design;
    const ComponentTreeModel model(*design.document, design.preferences);
    const QModelIndexList    selection{model.indexOf(*design.noseCone),
                                       model.indexOf(*design.bodyTube)};

    EXPECT_EQ(model.toolTip(model.indexOf(*design.bodyTube), selection),
              ComponentToolTip::multipleComponents(
                  std::vector<const RocketComponent*>{design.noseCone, design.bodyTube}));
    EXPECT_EQ(model.toolTip(model.indexOf(*design.noseCone), {model.indexOf(*design.noseCone)}),
              model.data(model.indexOf(*design.noseCone), Qt::ToolTipRole).toString());
    EXPECT_TRUE(model.toolTip(QModelIndex(), selection).isEmpty());
}

}  // namespace

}  // namespace QtRocket::Gui
