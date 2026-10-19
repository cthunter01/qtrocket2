#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"

#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentConfigDialogTest = Test::ConfigDialogTest;
using Test::childNamed;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::ScriptedUserPrompts;
using Test::tabTitles;
using Test::typeInto;

/// The tabs this run's panels show for each kind (ConfigPanelFactory::createWithSharedPanels():
/// the shared panels of OpenRocket's panel of the kind before Override and Comment).
struct KindTabs
{
    ComponentKind kind;
    const char*   title;
    QStringList   tabs;
};

[[nodiscard]] std::vector<KindTabs> expectedTabsOfEveryKind()
{
    const QString     override = QStringLiteral("Override");
    const QString     comment  = QStringLiteral("Comment");
    const QString     general  = QStringLiteral("General");
    const QStringList generalTabs{general, override, comment};
    return {
        {.kind  = ComponentKind::ROCKET,
         .title = "Rocket configuration",
         .tabs  = {override, comment}},
        {.kind  = ComponentKind::AXIAL_STAGE,
         .title = "Stage configuration",
         .tabs  = {override, comment}},
        {.kind  = ComponentKind::PARALLEL_STAGE,
         .title = "Booster Set configuration",
         .tabs  = generalTabs},
        {.kind = ComponentKind::POD_SET, .title = "Pod Set configuration", .tabs = generalTabs},
        {.kind  = ComponentKind::BODY_TUBE,
         .title = "Body Tube configuration",
         .tabs  = {general, QStringLiteral("Motor"), override, comment}},
        {.kind  = ComponentKind::TRANSITION,
         .title = "Transition configuration",
         .tabs  = {general, QStringLiteral("Shoulder"), override, comment}},
        {.kind  = ComponentKind::NOSE_CONE,
         .title = "Nose Cone configuration",
         .tabs  = {general, QStringLiteral("Shoulder"), override, comment}},
        {.kind  = ComponentKind::TRAPEZOID_FIN_SET,
         .title = "Trapezoidal Fin Set configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::ELLIPTICAL_FIN_SET,
         .title = "Elliptical Fin Set configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::FREEFORM_FIN_SET,
         .title = "Freeform Fin Set configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::TUBE_FIN_SET,
         .title = "Tube Fin Set configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::LAUNCH_LUG,
         .title = "Launch Lug configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::RAIL_BUTTON,
         .title = "Rail Button configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::INNER_TUBE,
         .title = "Inner Tube configuration",
         .tabs  = {general, QStringLiteral("Motor"), override, comment}},
        {.kind  = ComponentKind::TUBE_COUPLER,
         .title = "Tube Coupler configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::ENGINE_BLOCK,
         .title = "Engine Block configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::CENTERING_RING,
         .title = "Centering Ring configuration",
         .tabs  = generalTabs},
        {.kind = ComponentKind::BULKHEAD, .title = "Bulkhead configuration", .tabs = generalTabs},
        {.kind  = ComponentKind::MASS_COMPONENT,
         .title = "Mass Component configuration",
         .tabs  = generalTabs},
        {.kind  = ComponentKind::SHOCK_CORD,
         .title = "Shock Cord configuration",
         .tabs  = generalTabs},
        {.kind = ComponentKind::PARACHUTE, .title = "Parachute configuration", .tabs = generalTabs},
        {.kind = ComponentKind::STREAMER, .title = "Streamer configuration", .tabs = generalTabs},
    };
}

/// The differences between the dialog of each kind of @p design and @p expected: its title, its
/// tabs, the first tab selected, its undo position.
[[nodiscard]] std::string everyKindDifferences(QWidget&                    window,
                                               const ConfigDialogServices& services,
                                               EveryKindDesign&            design)
{
    std::string differences;
    for (const KindTabs& expected : expectedTabsOfEveryKind())
    {
        RocketComponent& component = design.component(expected.kind);
        // Each opens on its first tab (from a stage to a stage the tab would be remembered).
        ComponentConfigDialog::showDialog(window, services.services(), *design.document, component,
                                          {}, false, false);
        const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
        if (dialog == nullptr || !dialog->isVisible())
        {
            differences += std::format("{}: no dialog\n", expected.title);
            continue;
        }
        if (dialog->windowTitle() != QString::fromLatin1(expected.title))
        {
            differences += std::format("{}: title '{}'\n", expected.title,
                                       QtStrings::toUtf8(dialog->windowTitle()));
        }
        if (tabTitles(dialog->configurator()) != expected.tabs)
        {
            differences +=
                std::format("{}: tabs '{}'\n", expected.title,
                            QtStrings::toUtf8(tabTitles(dialog->configurator()).join(u',')));
        }
        if (dialog->configurator().selectedTabIndex() != 0)
        {
            differences += std::format("{}: tab {} selected\n", expected.title,
                                       dialog->configurator().selectedTabIndex());
        }
        const std::optional<std::string> undo = design.document->getUndoDetail().nextDescription;
        const std::string modify              = "Modify " + std::string(displayName(expected.kind));
        if (undo != modify)
        {
            differences += std::format("{}: next undo description '{}'\n", expected.title,
                                       undo.value_or("(none)"));
        }
        ComponentConfigDialog::disposeDialog();
    }
    return differences;
}

TEST_F(ComponentConfigDialogTest, OpensForEveryKindWithItsTitleAndTabs)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    EXPECT_EQ(everyKindDifferences(window, services, design), "");
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
}

TEST_F(ComponentConfigDialogTest, SeveralClassesGetTheGenericConfiguration)
{
    // Java: getDialogContents() gives the plain RocketComponentConfig, titled
    // "Multi-component configuration", and selects tab 1 (Appearance, Milestone 5): here the
    // first.
    const ConfigDialogServices            services;
    EveryKindDesign                       design;
    QWidget                               window;
    const std::array<RocketComponent*, 1> others{&design.component(ComponentKind::NOSE_CONE)};
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      design.component(ComponentKind::BODY_TUBE), others);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Multi-component configuration"));
    EXPECT_EQ(tabTitles(dialog->configurator()),
              (QStringList{QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(dialog->configurator().selectedTabIndex(), 0);
    EXPECT_FALSE(dialog->configurator().isAllSameType());
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Modify components");
}

TEST_F(ComponentConfigDialogTest, SeveralComponentsOfOneClassGetTheirClasssConfiguration)
{
    const ConfigDialogServices            services;
    EveryKindDesign                       design;
    QWidget                               window;
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      design.component(ComponentKind::BODY_TUBE), others);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("(Multi-component) Body Tube configuration"));
    EXPECT_EQ(tabTitles(dialog->configurator()),
              (QStringList{QStringLiteral("General"), QStringLiteral("Motor"),
                           QStringLiteral("Override"), QStringLiteral("Comment")}));
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Modify components");
}

TEST_F(ComponentConfigDialogTest, AComponentOfAnotherRocketIsABug)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    EveryKindDesign            other;
    QWidget                    window;
    EXPECT_THROW(ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                                   other.component(ComponentKind::BODY_TUBE)),
                 BugError);
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
}

/// The dialog of the body tube "Body" of @p design, shown and active, or null.
[[nodiscard]] ComponentConfigDialog* showBodyTubeDialog(QWidget&                    window,
                                                        const ConfigDialogServices& services,
                                                        EveryKindDesign& design, bool isNew = false)
{
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      design.component(ComponentKind::BODY_TUBE), {}, isNew);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    if (dialog == nullptr)
    {
        ADD_FAILURE() << "no dialog";
        return nullptr;
    }
    dialog->activateWindow();
    if (!QTest::qWaitForWindowActive(dialog))
    {
        ADD_FAILURE() << "the dialog did not become active";
    }
    return dialog;
}

/// Switches the mass override of the open dialog on (its check box clicked) and types @p mass
/// (in the default unit, g) into its field, committed with Enter when @p commit.
void overrideMass(ComponentConfigDialog& dialog, const QString& mass, bool commit)
{
    dialog.configurator().setSelectedTab(QStringLiteral("Override"));
    auto* check = childNamed<BoolCheckBox>(dialog, QStringLiteral("MassOverridden"));
    ASSERT_NE(check, nullptr);
    check->click();
    auto* field = childNamed<UnitSpinBox>(dialog, QStringLiteral("OverrideMass"));
    ASSERT_NE(field, nullptr);
    ASSERT_TRUE(field->isEnabled());
    typeInto(*field, mass);
    if (commit)
    {
        QTest::keyClick(field, Qt::Key_Return);
    }
}

/// Two edits of the body tube's dialog: the finish chosen (polished) and the mass overridden
/// (50 g, committed with Enter).
void editBodyTube(ComponentConfigDialog& dialog)
{
    auto* finish = childNamed<EnumComboBox>(dialog, QStringLiteral("Finish"));
    ASSERT_NE(finish, nullptr);
    Q_EMIT finish->activated(static_cast<int>(Finish::POLISHED));
    overrideMass(dialog, QStringLiteral("50"), true);
}

TEST_F(ComponentConfigDialogTest, EverySessionIsOneUndoStep)
{
    // ComponentConfigDialog.showDialog(): one undo position per session, so that one undo
    // restores every edit (the config-dialogs scout's core probe: two edits, one undo).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    auto&        tube   = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const Uuid   id     = tube.getId();
    const Finish finish = tube.getFinish();
    const double mass   = tube.getOverrideMass();
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    editBodyTube(*dialog);
    EXPECT_TRUE(tube.isMassOverridden());
    EXPECT_DOUBLE_EQ(tube.getOverrideMass(), 0.05);
    EXPECT_EQ(tube.getFinish(), Finish::POLISHED);
    EXPECT_TRUE(dialog->isModified());
    EXPECT_TRUE(dialog->windowTitle().startsWith(QStringLiteral("*Body Tube configuration")));

    dialog->configurator().okButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");
    design.document->undo();
    const auto* restored =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_FALSE(restored->isMassOverridden());
    EXPECT_EQ(restored->getOverrideMass(), mass);
    EXPECT_EQ(restored->getFinish(), finish);
    EXPECT_FALSE(design.document->isUndoAvailable());
}

TEST_F(ComponentConfigDialogTest, OkCommitsAPendingEdit)
{
    // OK, a button that takes no focus on macOS: the edit is committed all the same
    // (BindingScope::commitAll()).
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    overrideMass(*dialog, QStringLiteral("20"), false);
    EXPECT_TRUE(tube.isMassOverridden());
    EXPECT_NE(tube.getOverrideMass(), 0.02);
    dialog->ok();
    EXPECT_DOUBLE_EQ(tube.getOverrideMass(), 0.02);
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
}

TEST_F(ComponentConfigDialogTest, EscAndTheCloseButtonKeepTheChanges)
{
    // Java: Esc and the window's close button dispose the dialog; the focus loss commits.
    const ConfigDialogServices   services;
    EveryKindDesign              design;
    QWidget                      window;
    const RocketComponent&       tube   = design.component(ComponentKind::BODY_TUBE);
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Main tube"));
    QTest::keyClick(&name, Qt::Key_Escape);
    EXPECT_EQ(tube.getName(), "Main tube");
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);

    ComponentConfigDialog* second = showBodyTubeDialog(window, services, design);
    ASSERT_NE(second, nullptr);
    second->configurator().commentArea().setPlainText(QStringLiteral("glued"));
    EXPECT_TRUE(second->close());
    EXPECT_EQ(tube.getComment(), "glued");
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
}

TEST_F(ComponentConfigDialogTest, CancelWithoutTheQuestionUndoesTheSession)
{
    const ConfigDialogServices services;
    EveryKindDesign            design;
    QWidget                    window;
    services.services().preferences().setShowDiscardConfirmation(false);
    const Uuid             id     = design.component(ComponentKind::BODY_TUBE).getId();
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    editBodyTube(*dialog);
    dialog->configurator().cancelButton().click();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    const auto* restored =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_FALSE(restored->isMassOverridden());
    EXPECT_EQ(restored->getFinish(), Finish::NORMAL);
    EXPECT_FALSE(design.document->isUndoAvailable());
    EXPECT_TRUE(design.document->isRedoAvailable());
}

TEST_F(ComponentConfigDialogTest, CancelAsksBeforeItDiscards)
{
    ConfigDialogServices   services;
    EveryKindDesign        design;
    QWidget                window;
    const Uuid             id     = design.component(ComponentKind::BODY_TUBE).getId();
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    editBodyTube(*dialog);

    // No: the dialog stays, nothing is undone.
    services.prompts().answerDiscardChanges({.discard = false, .doNotAskAgain = false});
    dialog->cancel();
    ASSERT_EQ(ComponentConfigDialog::dialog(), dialog);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");

    // Yes, and do not ask again: the preference is off, the dialog closes before the undo.
    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = true});
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_FALSE(services.services().preferences().isShowDiscardConfirmation());
    const auto* restored =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getFinish(), Finish::NORMAL);

    const std::vector<ScriptedUserPrompts::Prompt>& prompts = services.prompts().prompts();
    ASSERT_EQ(prompts.size(), 2U);
    EXPECT_EQ(prompts.at(0).kind, ScriptedUserPrompts::Kind::DISCARD_CHANGES);
    EXPECT_EQ(prompts.at(0).texts, QStringList{QStringLiteral("COMPONENT_CHANGES")});
}

TEST_F(ComponentConfigDialogTest, CancelOfANewComponentUndoesAddingIt)
{
    // The add button made the undo position "Add Body Tube" and added the component; the dialog
    // makes none, and Cancel asks to undo adding it.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    Rocket&              rocket = design.document->getRocket();
    design.document->addUndoPosition("Add Body Tube");
    RocketComponent&  added  = design.booster->addChild(std::make_unique<BodyTube>());
    const Uuid        id     = added.getId();
    const std::size_t states = design.document->getUndoDetail().descriptions.size();

    ComponentConfigDialog::showDialog(window, services.services(), *design.document, added, {},
                                      true);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isNewComponent());
    EXPECT_EQ(design.document->getUndoDetail().descriptions.size(), states);
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Add Body Tube");

    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(rocket.findComponent(id), nullptr);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_EQ(services.prompts().prompts().front().texts,
              QStringList{QStringLiteral("COMPONENT_ADDITION")});
    EXPECT_TRUE(services.services().preferences().isShowDiscardConfirmation());
}

TEST_F(ComponentConfigDialogTest, AnUnchangedCancelClosesWithoutAnUndo)
{
    ConfigDialogServices   services;
    EveryKindDesign        design;
    QWidget                window;
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    EXPECT_FALSE(dialog->isModified());
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(services.prompts().prompts().empty());
    EXPECT_FALSE(design.document->isUndoAvailable());
}

/// Changes the design's body tube while a configuration dialog is being shown, as run 3's panels
/// do while they are made (BodyTubeConfig switches the automatic radius off): an event filter of
/// the application on the dialog's Show event.
class ChangeWhileShown : public QObject
{
public:
    explicit ChangeWhileShown(BodyTube& tube) : m_tube(&tube)
    {
        QCoreApplication::instance()->installEventFilter(this);
    }
    ~ChangeWhileShown() override { QCoreApplication::instance()->removeEventFilter(this); }
    ChangeWhileShown(const ChangeWhileShown&)            = delete;
    ChangeWhileShown& operator=(const ChangeWhileShown&) = delete;
    ChangeWhileShown(ChangeWhileShown&&)                 = delete;
    ChangeWhileShown& operator=(ChangeWhileShown&&)      = delete;

    [[nodiscard]] int changes() const noexcept { return m_changes; }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Show && m_changes == 0 &&
            qobject_cast<ComponentConfigDialog*>(watched) != nullptr)
        {
            m_tube->setOuterRadiusAutomatic(!m_tube->isOuterRadiusAutomatic());
            ++m_changes;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    BodyTube* m_tube;
    int       m_changes{0};
};

TEST_F(ComponentConfigDialogTest, AChangeBeforeTheUndoPositionIsNoModification)
{
    // The user's decision U4: OpenRocket counts the changes its panels make while the dialog is
    // made, before the session's undo position, and its unchanged Cancel then undoes the step
    // before the dialog ("Add Body Tube": the new tube was gone). Here they do not count.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    services.services().preferences().setShowDiscardConfirmation(false);
    design.document->addUndoPosition("Add Body Tube");
    BodyTube&  added     = design.booster->addChild(std::make_unique<BodyTube>());
    const Uuid id        = added.getId();
    const bool automatic = added.isOuterRadiusAutomatic();
    {
        const ChangeWhileShown change(added);
        ComponentConfigDialog::showDialog(window, services.services(), *design.document, added);
        EXPECT_EQ(change.changes(), 1);
    }
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_FALSE(dialog->isModified());
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("Body Tube configuration"));

    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    const auto* kept =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(kept, nullptr);
    EXPECT_NE(kept->isOuterRadiusAutomatic(), automatic);
    EXPECT_EQ(design.document->getUndoDescription(), "Add Body Tube");
}

TEST_F(ComponentConfigDialogTest, AChangeOfTheRocketMarksTheDialogModifiedOnce)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    auto&                tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    tube.setLength(0.5);
    tube.setLength(0.6);
    EXPECT_TRUE(dialog->isModified());
    EXPECT_EQ(dialog->windowTitle(), QStringLiteral("*Body Tube configuration"));
    // The fields show the change (Java: updateFields()).
    EXPECT_TRUE(
        dialog->configurator().infoLabel().text().startsWith(QStringLiteral("Component mass:")));
}

TEST_F(ComponentConfigDialogTest, AnUndoWhileOpenClosesTheDialogInTheEvent)
{
    // The undo replaces every component object (Rocket::loadFrom()): the dialog closes inside the
    // event, and a pending edit, committed by the focus loss of the hidden dialog, reaches none of
    // the components that are gone (proved under the asan preset).
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Pending"));

    design.document->undo();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(dialog->isVisible());
    // The closed dialog reaches no component until it is deleted.
    EXPECT_FALSE(dialog->configurator().commitAll());
    dialog->configurator().updateFields();
    EXPECT_FALSE(ComponentConfigDialog::commitPendingEdits());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    const auto& tube = dynamic_cast<const BodyTube&>(
        *design.document->getRocket().getChildren().front()->getChildren().at(1));
    EXPECT_EQ(tube.getName(), "Body");
    EXPECT_EQ(design.document->getRocket().getChildren().front()->getChildren().front()->getName(),
              "Nose Cone");
}

TEST_F(ComponentConfigDialogTest, ATreeChangeClosesTheDialog)
{
    ConfigDialogServices         services;
    EveryKindDesign              design;
    QWidget                      window;
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    static_cast<void>(design.booster->addChild(std::make_unique<BodyTube>()));
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_FALSE(ComponentConfigDialog::isDialogVisible());
}

TEST_F(ComponentConfigDialogTest, RenamingAStageClosesTheDialog)
{
    // A stage's name is part of the tree (setName() fires a TREE change for a stage): the dialog
    // closes, as in OpenRocket, and the stage has its new name.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      *design.booster);
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Second"));
    QTest::keyClick(&name, Qt::Key_Return);
    EXPECT_EQ(design.booster->getName(), "Second");
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
}

TEST_F(ComponentConfigDialogTest, TheNextDialogOpensAfterThePendingEditsOfTheOpenOne)
{
    // A stage's new name, pending in the open dialog, is committed when the next dialog opens for
    // the same stage: a TREE change, after which the next dialog's components are still there.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      *design.booster);
    const ComponentConfigDialog* first = ComponentConfigDialog::dialog();
    ASSERT_NE(first, nullptr);
    QLineEdit& name = first->configurator().nameField();
    name.setText(QStringLiteral("Second"));
    Q_EMIT name.textEdited(QStringLiteral("Second"));
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      *design.booster);
    EXPECT_EQ(design.booster->getName(), "Second");
    const ComponentConfigDialog* second = ComponentConfigDialog::dialog();
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second, first);
    EXPECT_EQ(second->configurator().nameField().text(), QStringLiteral("Second"));
}

TEST_F(ComponentConfigDialogTest, OneDialogAtATime)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      design.component(ComponentKind::NOSE_CONE));
    const ComponentConfigDialog* first = ComponentConfigDialog::dialog();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(ComponentConfigDialog::isDialogVisible());
    ComponentConfigDialog::showDialog(window, services.services(), *design.document,
                                      design.component(ComponentKind::TRANSITION));
    const ComponentConfigDialog* second = ComponentConfigDialog::dialog();
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second, first);
    EXPECT_TRUE(first->isDisposed());
    EXPECT_EQ(second->windowTitle(), QStringLiteral("Transition configuration"));
    ComponentConfigDialog::disposeDialog();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    ComponentConfigDialog::disposeDialog();  // none open: nothing
}

/// The tab selected when @p component's dialog opens after the dialogs before it.
[[nodiscard]] QString openedTab(QWidget& window, const ConfigDialogServices& services,
                                EveryKindDesign& design, RocketComponent& component,
                                bool rememberPreviousTab = true)
{
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, component, {},
                                      false, rememberPreviousTab);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    return dialog != nullptr ? dialog->selectedTabName() : QStringLiteral("(no dialog)");
}

/// Selects the tab @p title of the open dialog.
void selectTab(const QString& title)
{
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    dialog->configurator().setSelectedTab(title);
    ASSERT_EQ(dialog->selectedTabName(), title);
}

TEST_F(ComponentConfigDialogTest, TheSelectedTabIsRememberedByItsTitle)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    RocketComponent&     body    = design.component(ComponentKind::BODY_TUBE);
    const QString        comment = QStringLiteral("Comment");

    EXPECT_EQ(openedTab(window, services, design, body), QStringLiteral("General"));
    selectTab(comment);
    EXPECT_EQ(openedTab(window, services, design, *design.boosterTube), comment);
    selectTab(QStringLiteral("Motor"));
    // A panel without that tab opens on its first.
    EXPECT_EQ(openedTab(window, services, design, design.component(ComponentKind::NOSE_CONE)),
              QStringLiteral("General"));
    selectTab(comment);
    // After the Rocket's dialog nothing is remembered.
    EXPECT_EQ(openedTab(window, services, design, design.document->getRocket()), comment);
    EXPECT_EQ(openedTab(window, services, design, body), QStringLiteral("General"));
    // From a stage to a stage, but not from a stage to another component.
    selectTab(comment);
    EXPECT_EQ(openedTab(window, services, design, *design.booster), comment);
    EXPECT_EQ(openedTab(window, services, design, design.component(ComponentKind::AXIAL_STAGE)),
              comment);
    EXPECT_EQ(openedTab(window, services, design, body), QStringLiteral("General"));
    // Not when asked not to.
    selectTab(comment);
    EXPECT_EQ(openedTab(window, services, design, *design.boosterTube, false),
              QStringLiteral("General"));
}

TEST_F(ComponentConfigDialogTest, TheLeftmostTabWhenThePreferencesSaySo)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    services.services().preferences().setAlwaysOpenLeftmostTab(true);
    RocketComponent& body = design.component(ComponentKind::BODY_TUBE);
    EXPECT_EQ(openedTab(window, services, design, body), QStringLiteral("General"));
    selectTab(QStringLiteral("Comment"));
    EXPECT_EQ(openedTab(window, services, design, *design.boosterTube), QStringLiteral("General"));
}

TEST_F(ComponentConfigDialogTest, ThePositionIsRememberedTheSizeIsTheContents)
{
    ConfigDialogServices   services;
    EveryKindDesign        design;
    QWidget                window;
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    const QSize packed = dialog->size();
    dialog->move(QPoint(123, 145));
    QCoreApplication::processEvents();
    const QPoint moved = dialog->pos();
    dialog->resize(packed * 2);
    QCoreApplication::processEvents();
    ComponentConfigDialog::disposeDialog();
    const Preferences* node =
        services.services().preferences().findNode(GuiPreferenceKeys::kNodeWindows);
    ASSERT_NE(node, nullptr);
    EXPECT_EQ(node->get(ComponentConfigDialog::kPositionKey),
              std::format("{},{}", moved.x(), moved.y()));

    const ComponentConfigDialog* reopened = showBodyTubeDialog(window, services, design);
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(reopened->pos(), moved);
    EXPECT_EQ(reopened->size(), packed);
}

TEST_F(ComponentConfigDialogTest, APositionOnNoScreenIsNotUsed)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    services.services()
        .preferences()
        .getNode(GuiPreferenceKeys::kNodeWindows)
        .put(ComponentConfigDialog::kPositionKey, "-30000,-30000");
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    EXPECT_NE(dialog->pos(), QPoint(-30000, -30000));
}

TEST_F(ComponentConfigDialogTest, CommitPendingEditsCommitsTheOpenDialog)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    EXPECT_FALSE(ComponentConfigDialog::commitPendingEdits());
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    overrideMass(*dialog, QStringLiteral("40"), false);
    EXPECT_TRUE(ComponentConfigDialog::commitPendingEdits());
    EXPECT_DOUBLE_EQ(tube.getOverrideMass(), 0.04);
    EXPECT_TRUE(ComponentConfigDialog::isDialogVisible());
}

TEST_F(ComponentConfigDialogTest, TheWindowTakesItsDialogAlong)
{
    ConfigDialogServices services;
    EveryKindDesign      design;
    auto                 window = std::make_unique<QWidget>();
    ComponentConfigDialog::showDialog(*window, services.services(), *design.document,
                                      design.component(ComponentKind::NOSE_CONE));
    ASSERT_NE(ComponentConfigDialog::dialog(), nullptr);
    window.reset();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    // The next dialog remembers the closed one's tab as after any other close.
    QWidget next;
    EXPECT_EQ(openedTab(next, services, design, design.component(ComponentKind::TRANSITION)),
              QStringLiteral("General"));
}

}  // namespace

}  // namespace QtRocket::Gui
