// The selection after an undo: the component tree view puts back the selected components by id,
// or the nearest of their ancestors that the undo left (ComponentTreeView,
// DocumentSelectionModel).

#include <memory>

#include <gtest/gtest.h>

#include "GuiTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/tree/ComponentTreeModel.h"
#include "QtRocket/gui/tree/ComponentTreeView.h"
#include "QtRocket/gui/tree/DocumentSelectionModel.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/InnerTube.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/Uuid.h"

namespace QtRocket::Gui
{

namespace
{

using UndoRedoSelectionTest = Gui::Test::GuiTest;

// Port of UndoRedoSelectionTest.undoAddedComponentSelectsParent (swing
// gui/main/UndoRedoSelectionTest.java). Java's JTree with the ComponentTreeModel is the
// ComponentTreeView here: the view puts the selection back (Java's model does).
TEST_F(UndoRedoSelectionTest, UndoAddedComponentSelectsParent)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    ComponentTreeModel        model(*document, preferences);
    ComponentTreeView         tree(model);
    auto*                     selectionModel = makeChild<DocumentSelectionModel>(&tree);

    RocketComponent& sustainer   = document->getRocket().getChild(0);
    const Uuid       sustainerId = sustainer.getId();

    document->addUndoPosition("Add body tube");
    const BodyTube& bodyTube = sustainer.addChild(std::make_unique<BodyTube>());

    selectionModel->setSelectedComponent(bodyTube);

    document->undo();

    ASSERT_NE(selectionModel->selectedComponent(), nullptr);
    EXPECT_EQ(sustainerId, selectionModel->selectedComponent()->getId());
}

// Port of UndoRedoSelectionTest.undoAddedChildComponentSelectsParent.
TEST_F(UndoRedoSelectionTest, UndoAddedChildComponentSelectsParent)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    ComponentTreeModel        model(*document, preferences);
    ComponentTreeView         tree(model);
    auto*                     selectionModel = makeChild<DocumentSelectionModel>(&tree);

    RocketComponent& sustainer = document->getRocket().getChild(0);

    document->addUndoPosition("Add body tube");
    BodyTube&  bodyTube   = sustainer.addChild(std::make_unique<BodyTube>());
    const Uuid bodyTubeId = bodyTube.getId();

    document->addUndoPosition("Add inner tube");
    const InnerTube& innerTube = bodyTube.addChild(std::make_unique<InnerTube>());

    selectionModel->setSelectedComponent(innerTube);

    document->undo();

    ASSERT_NE(selectionModel->selectedComponent(), nullptr);
    EXPECT_EQ(bodyTubeId, selectionModel->selectedComponent()->getId());
}

// Beyond the Java tests: the selected component comes back after the redo, and the
// components are the rocket's new objects.
TEST_F(UndoRedoSelectionTest, RedoSelectsTheComponentAgain)
{
    const std::unique_ptr<OpenRocketDocument> document =
        OpenRocketDocumentFactory::createNewRocket();
    const InMemoryPreferences preferences;
    ComponentTreeModel        model(*document, preferences);
    ComponentTreeView         tree(model);
    auto*                     selectionModel = makeChild<DocumentSelectionModel>(&tree);

    RocketComponent& sustainer = document->getRocket().getChild(0);
    document->addUndoPosition("Add body tube");
    const BodyTube& bodyTube   = sustainer.addChild(std::make_unique<BodyTube>());
    const Uuid      bodyTubeId = bodyTube.getId();
    const Uuid      stageId    = sustainer.getId();
    selectionModel->setSelectedComponent(bodyTube);

    document->undo();
    document->redo();

    // Undo selected the stage, which the redo keeps.
    ASSERT_NE(selectionModel->selectedComponent(), nullptr);
    EXPECT_EQ(stageId, selectionModel->selectedComponent()->getId());
    EXPECT_EQ(selectionModel->selectedComponent(), &document->getRocket().getChild(0));
    EXPECT_NE(document->getRocket().findComponent(bodyTubeId), nullptr);
}

}  // namespace

}  // namespace QtRocket::Gui
