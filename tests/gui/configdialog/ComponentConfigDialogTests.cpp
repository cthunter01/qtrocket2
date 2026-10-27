#include "QtRocket/gui/configdialog/ComponentConfigDialog.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QEvent>
#include <QEventLoop>
#include <QLayout>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointer>
#include <QPushButton>
#include <QSignalSpy>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <QWidget>
#include <Qt>
#include <QtContainerFwd>
#include <QtCore/qtestsupport_core.h>
#include <QtWidgets/qtestsupport_widgets.h>

#include <gtest/gtest.h>

#include "ConfigDialogTestSupport.h"
#include "ForwardingUserPrompts.h"
#include "NextModalDialog.h"
#include "QtRocket/document/OpenRocketDocument.h"
#include "QtRocket/document/OpenRocketDocumentFactory.h"
#include "QtRocket/gui/binding/ComponentSelection.h"
#include "QtRocket/gui/binding/PresetBinding.h"
#include "QtRocket/gui/binding/PresetDiameterFilters.h"
#include "QtRocket/gui/configdialog/RocketComponentConfig.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetChooserDialog.h"
#include "QtRocket/gui/dialogs/preset/ComponentPresetTable.h"
#include "QtRocket/gui/main/WidgetUserPrompts.h"
#include "QtRocket/gui/settings/GuiPreferenceKeys.h"
#include "QtRocket/gui/util/Guard.h"
#include "QtRocket/gui/util/GuiServices.h"
#include "QtRocket/gui/util/MakeChild.h"
#include "QtRocket/gui/util/QtStrings.h"
#include "QtRocket/gui/util/UserPrompts.h"
#include "QtRocket/gui/widgets/BoolCheckBox.h"
#include "QtRocket/gui/widgets/EnumComboBox.h"
#include "QtRocket/gui/widgets/PresetComboBox.h"
#include "QtRocket/gui/widgets/StyledLabel.h"
#include "QtRocket/gui/widgets/UnitSelector.h"
#include "QtRocket/gui/widgets/UnitSpinBox.h"
#include "QtRocket/preferences/InMemoryPreferences.h"
#include "QtRocket/preferences/Preferences.h"
#include "QtRocket/rocket/AxialStage.h"
#include "QtRocket/rocket/BodyTube.h"
#include "QtRocket/rocket/ComponentKind.h"
#include "QtRocket/rocket/ExternalComponent.h"
#include "QtRocket/rocket/Finish.h"
#include "QtRocket/rocket/FlightConfigurationId.h"
#include "QtRocket/rocket/MassComponent.h"
#include "QtRocket/rocket/MotorMount.h"
#include "QtRocket/rocket/Rocket.h"
#include "QtRocket/rocket/RocketComponent.h"
#include "QtRocket/rocket/preset/ComponentPreset.h"
#include "QtRocket/rocket/preset/ComponentPresetDatabase.h"
#include "QtRocket/rocket/preset/ComponentPresetType.h"
#include "QtRocket/util/BugError.h"
#include "QtRocket/util/Uuid.h"
#include "ScriptedUserPrompts.h"
#include "rocket/preset/TestPresetDatabase.h"
#include "widgets/WidgetTestSupport.h"

namespace QtRocket::Gui
{

namespace
{

using ComponentConfigDialogTest = Test::ConfigDialogTest;
using Test::childNamed;
using Test::ConfigDialogServices;
using Test::EveryKindDesign;
using Test::OffscreenPopupWarnings;
using Test::ScriptedUserPrompts;
using Test::tabTitles;
using Test::typeInto;

/// The title of the dialog of each kind and the tabs of its panel (ConfigPanelFactory: the
/// kind's own panel), OpenRocket's.
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
        // RocketConfig hides its base's tab widget, which keeps its tabs (Java's removeAll()).
        {.kind  = ComponentKind::ROCKET,
         .title = "Rocket configuration",
         .tabs  = {override, comment}},
        {.kind  = ComponentKind::AXIAL_STAGE,
         .title = "Stage configuration",
         .tabs  = {override, comment, QStringLiteral("Recovery")}},
        {.kind  = ComponentKind::PARALLEL_STAGE,
         .title = "Booster Set configuration",
         .tabs  = {QStringLiteral("Separation"), general, override, comment,
                   QStringLiteral("Recovery")}},
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
         .tabs  = {general, QStringLiteral("Fin tabs"), override, comment}},
        {.kind  = ComponentKind::ELLIPTICAL_FIN_SET,
         .title = "Elliptical Fin Set configuration",
         .tabs  = {general, QStringLiteral("Fin tabs"), override, comment}},
        {.kind  = ComponentKind::FREEFORM_FIN_SET,
         .title = "Freeform Fin Set configuration",
         .tabs = {general, QStringLiteral("Shape"), QStringLiteral("Fin tabs"), override, comment}},
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
         .tabs  = {general, QStringLiteral("Motor"), QStringLiteral("Cluster"),
                   QStringLiteral("Radial position"), override, comment}},
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
         .tabs  = {general, QStringLiteral("Radial position"), override, comment}},
        {.kind  = ComponentKind::SHOCK_CORD,
         .title = "Shock Cord configuration",
         .tabs  = {general, QStringLiteral("Radial position"), override, comment}},
        {.kind  = ComponentKind::PARACHUTE,
         .title = "Parachute configuration",
         .tabs  = {general, QStringLiteral("Radial position"), override, comment}},
        {.kind  = ComponentKind::STREAMER,
         .title = "Streamer configuration",
         .tabs  = {general, QStringLiteral("Radial position"), override, comment}},
    };
}

/// The default and auto-default buttons of @p dialog, titled @p title, one line each: none, as
/// Java's dialog has no default button (Enter in a combo box clicks nothing).
[[nodiscard]] std::string defaultButtons(const QWidget& dialog, const char* title)
{
    std::string buttons;
    for (const QPushButton* button : dialog.findChildren<QPushButton*>())
    {
        if (button->isDefault() || button->autoDefault())
        {
            buttons += std::format("{}: the button '{}' is a default button\n", title,
                                   QtStrings::toUtf8(button->text()));
        }
    }
    return buttons;
}

/// The differences between the dialog of each kind of @p design and @p expected: its title, its
/// tabs, the first tab selected, its undo position, no default button.
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
        differences += defaultButtons(*dialog, expected.title);
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
void overrideMass(ComponentConfigDialog const& dialog, const QString& mass, bool commit)
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
void editBodyTube(ComponentConfigDialog const& dialog)
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
    auto const&  tube   = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const Uuid   id     = tube.getId();
    const Finish finish = tube.getFinish();
    const double mass   = tube.getOverrideMass();
    ComponentConfigDialog const* dialog = showBodyTubeDialog(window, services, design);
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
    auto const& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
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
    const Uuid                   id     = design.component(ComponentKind::BODY_TUBE).getId();
    ComponentConfigDialog const* dialog = showBodyTubeDialog(window, services, design);
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
    auto const& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    ComponentConfigDialog const* dialog = showBodyTubeDialog(window, services, design);
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

/// The questions of a test, answered by @p delegate (ScriptedUserPrompts), but for the discard
/// question of Cancel, which runs an event loop as a modal message box does: in it a 0 ms timer
/// does what the test gives (an undo, the window going), and the deferred deletions posted
/// meanwhile are carried out, as Qt 6 does in the loop that posted them; then the question gives
/// its answer.
class NestedLoopPrompts final : public UserPrompts
{
public:
    NestedLoopPrompts(ScriptedUserPrompts& delegate, std::function<void()> whileAsked,
                      DiscardAnswer answer)
      : m_delegate(&delegate), m_whileAsked(std::move(whileAsked)), m_answer(answer)
    {
    }

    [[nodiscard]] int            asked() const noexcept { return m_asked; }
    [[nodiscard]] const QWidget* parent() const noexcept { return m_parent; }

    [[nodiscard]] DiscardAnswer askDiscardChanges(QWidget* parent,
                                                  DiscardQuestion /*question*/) override
    {
        ++m_asked;
        m_parent = parent;
        QEventLoop loop;
        QTimer::singleShot(0, &loop, [this, &loop] {
            m_whileAsked();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            loop.quit();
        });
        loop.exec();
        return m_answer;
    }

    [[nodiscard]] InsertPositionAnswer askInsertPosition(QWidget*               parent,
                                                         InsertPositionQuestion question) override
    {
        return m_delegate->askInsertPosition(parent, question);
    }
    [[nodiscard]] SaveChangesAnswer askSaveChanges(QWidget*       parent,
                                                   const QString& designName) override
    {
        return m_delegate->askSaveChanges(parent, designName);
    }
    [[nodiscard]] bool askOverwrite(QWidget* parent, const QString& fileName) override
    {
        return m_delegate->askOverwrite(parent, fileName);
    }
    [[nodiscard]] std::optional<bool> askStorageOptions(QWidget* parent,
                                                        bool     saveSimulationData) override
    {
        return m_delegate->askStorageOptions(parent, saveSimulationData);
    }
    [[nodiscard]] bool askYesNo(QWidget* parent, const Question& question) override
    {
        return m_delegate->askYesNo(parent, question);
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askDecalFile(
        QWidget* parent, const QString& message, const std::filesystem::path& directory) override
    {
        return m_delegate->askDecalFile(parent, message, directory);
    }
    [[nodiscard]] std::optional<OpenFileAnswer> askOpenFiles(
        QWidget* parent, const OpenFileRequest& request) override
    {
        return m_delegate->askOpenFiles(parent, request);
    }
    [[nodiscard]] std::optional<SaveFileAnswer> askSaveFile(QWidget*               parent,
                                                            const SaveFileRequest& request) override
    {
        return m_delegate->askSaveFile(parent, request);
    }
    void editDesignInformation(QWidget* parent, const GuiServices& services,
                               OpenRocketDocument& document) override
    {
        m_delegate->editDesignInformation(parent, services, document);
    }
    [[nodiscard]] std::optional<IgnitionChoice> askIgnition(QWidget*               parent,
                                                            const IgnitionRequest& request) override
    {
        return m_delegate->askIgnition(parent, request);
    }
    [[nodiscard]] std::optional<DeploymentChoice> askDeployment(
        QWidget* parent, const DeploymentRequest& request) override
    {
        return m_delegate->askDeployment(parent, request);
    }
    [[nodiscard]] std::optional<SeparationChoice> askSeparation(
        QWidget* parent, const SeparationRequest& request) override
    {
        return m_delegate->askSeparation(parent, request);
    }
    [[nodiscard]] RenameConfigurationAnswer askRenameConfiguration(
        QWidget* parent, const RenameConfigurationRequest& request) override
    {
        return m_delegate->askRenameConfiguration(parent, request);
    }
    [[nodiscard]] std::optional<MotorChoice> chooseMotor(
        QWidget* parent, const GuiServices& services, const MotorMount& mount,
        const FlightConfigurationId& configurationId) override
    {
        return m_delegate->chooseMotor(parent, services, mount, configurationId);
    }
    [[nodiscard]] PresetAnswer choosePreset(QWidget* parent, const GuiServices& services,
                                            const PresetRequest& request) override
    {
        return m_delegate->choosePreset(parent, services, request);
    }
    void showMessage(QWidget* parent, const Message& message) override
    {
        m_delegate->showMessage(parent, message);
    }
    void showWarnings(QWidget* parent, const WarningsMessage& message) override
    {
        m_delegate->showWarnings(parent, message);
    }
    void showDatabaseProblems(QWidget* parent, std::span<const DatabaseProblem> problems) override
    {
        m_delegate->showDatabaseProblems(parent, problems);
    }
    [[nodiscard]] InternalErrorAnswer showInternalError(QWidget*              parent,
                                                        const Guard::Failure& failure) override
    {
        return m_delegate->showInternalError(parent, failure);
    }

private:
    ScriptedUserPrompts*  m_delegate;
    std::function<void()> m_whileAsked;
    DiscardAnswer         m_answer;
    int                   m_asked{0};
    const QWidget*        m_parent{nullptr};
};

/// Counts the phases a selection tells its listeners while it lives.
class PhaseCounter
{
public:
    explicit PhaseCounter(ComponentSelection& selection)
      : m_connection(
            selection.addListener([this](ComponentSelection::Phase /*phase*/) { ++m_count; }))
    {
    }

    [[nodiscard]] int count() const noexcept { return m_count; }

private:
    int                                               m_count{0};
    ComponentSelection::PhaseSignal::ScopedConnection m_connection;
};

/// The services of @p services with the questions of @p prompts.
[[nodiscard]] GuiServices withPrompts(const GuiServices& services, UserPrompts& prompts)
{
    return {services.preferences(), services.materials(),      services.motors(),
            services.presets(),     services.loadingContext(), prompts};
}

/// Undoes @p document (NestedLoopPrompts' action): an UNDO change, which closes the dialog.
struct UndoDocument
{
    OpenRocketDocument* document;

    void operator()() const { document->undo(); }
};

/// Deletes the window (NestedLoopPrompts' action), and with it the dialog it owns.
struct DeleteWindow
{
    std::unique_ptr<QWidget>* window;

    void operator()() const { window->reset(); }
};

/// The dialog of @p component, shown with @p services and active, or null.
[[nodiscard]] ComponentConfigDialog* showActiveDialog(QWidget& window, const GuiServices& services,
                                                      OpenRocketDocument& document,
                                                      RocketComponent&    component)
{
    ComponentConfigDialog::showDialog(window, services, document, component);
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

TEST_F(ComponentConfigDialogTest, ADialogClosedWhileTheQuestionIsOpenOutlivesTheQuestion)
{
    // Cancel's question, whose parent is the dialog (Java: the panel), runs an event loop in which
    // an undo closes the dialog (an UNDO change). The dialog is deleted only once the question has
    // its answer, and it undoes nothing more: the nose cone keeps the name of the step before
    // (proved under ASan: a dialog deleted in the loop was read afterwards).
    ConfigDialogServices services;
    EveryKindDesign      design;
    design.document->addUndoPosition("Rename");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    const Uuid        noseId = design.component(ComponentKind::NOSE_CONE).getId();
    auto&             tube   = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const Uuid        tubeId = tube.getId();
    const double      length = tube.getLength();
    NestedLoopPrompts prompts(services.prompts(), UndoDocument{design.document.get()},
                              {.discard = true, .doNotAskAgain = false});
    const GuiServices own = withPrompts(services.services(), prompts);
    QWidget           window;
    ComponentConfigDialog* dialog = showActiveDialog(window, own, *design.document, tube);
    ASSERT_NE(dialog, nullptr);
    tube.setLength(0.77);
    ASSERT_TRUE(dialog->isModified());
    const QPointer<ComponentConfigDialog> guard(dialog);

    dialog->cancel();
    EXPECT_EQ(prompts.asked(), 1);
    ASSERT_FALSE(guard.isNull());
    EXPECT_EQ(prompts.parent(), dialog);
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    const Rocket& rocket = design.document->getRocket();
    ASSERT_NE(rocket.findComponent(noseId), nullptr);
    EXPECT_EQ(rocket.findComponent(noseId)->getName(), "Pointy");
    const auto* restored = dynamic_cast<const BodyTube*>(rocket.findComponent(tubeId));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getLength(), length);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(guard.isNull());
}

TEST_F(ComponentConfigDialogTest, ADialogDeletedWhileTheQuestionIsOpenIsLeftAlone)
{
    // Its window goes while Cancel's question is open, and the dialog with it: Cancel touches
    // nothing of it afterwards (proved under ASan) and undoes nothing, but "Don't ask me again"
    // is kept.
    ConfigDialogServices services;
    EveryKindDesign      design;
    auto&                tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    std::unique_ptr<QWidget> window = std::make_unique<QWidget>();
    NestedLoopPrompts        prompts(services.prompts(), DeleteWindow{&window},
                                     {.discard = true, .doNotAskAgain = true});
    const GuiServices        own    = withPrompts(services.services(), prompts);
    ComponentConfigDialog*   dialog = showActiveDialog(*window, own, *design.document, tube);
    ASSERT_NE(dialog, nullptr);
    tube.setLength(0.77);
    const QPointer<ComponentConfigDialog> guard(dialog);

    dialog->cancel();
    EXPECT_EQ(prompts.asked(), 1);
    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(window.get(), nullptr);
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_DOUBLE_EQ(tube.getLength(), 0.77);
    EXPECT_EQ(design.document->getUndoDescription(), "Modify Body Tube");
    EXPECT_FALSE(services.services().preferences().isShowDiscardConfirmation());
}

TEST_F(ComponentConfigDialogTest, CancelDropsAPendingEditOfAnUnchangedDialog)
{
    // The bindings end before the dialog hides (RocketComponentConfig::release()): the focus
    // loss of the field, whose edit Cancel leaves pending (a button that takes no focus, as on
    // macOS), reaches no component.
    ConfigDialogServices   services;
    EveryKindDesign        design;
    QWidget                window;
    const RocketComponent& tube   = design.component(ComponentKind::BODY_TUBE);
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Pending"));
    ASSERT_TRUE(name.hasFocus());
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(tube.getName(), "Body");
    EXPECT_FALSE(design.document->isUndoAvailable());
    EXPECT_TRUE(services.prompts().prompts().empty());
}

TEST_F(ComponentConfigDialogTest, CancelDropsAPendingEditBeforeItUndoes)
{
    // A changed dialog, a number pending in its focused field: after Cancel the redo step holds
    // the committed change but not the pending number.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    services.services().preferences().setShowDiscardConfirmation(false);
    auto const& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    const Uuid  id   = tube.getId();
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    dialog->configurator().setSelectedTab(QStringLiteral("Override"));
    auto* check = childNamed<BoolCheckBox>(*dialog, QStringLiteral("MassOverridden"));
    auto* field = childNamed<UnitSpinBox>(*dialog, QStringLiteral("OverrideMass"));
    ASSERT_NE(check, nullptr);
    ASSERT_NE(field, nullptr);
    check->click();
    ASSERT_TRUE(tube.isMassOverridden());
    const double mass = tube.getOverrideMass();
    ASSERT_NE(mass, 0.077);
    typeInto(*field, QStringLiteral("77"));
    ASSERT_TRUE(field->hasFocus());

    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    ASSERT_TRUE(design.document->isRedoAvailable());
    design.document->redo();
    const auto* redone =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(redone, nullptr);
    EXPECT_TRUE(redone->isMassOverridden());
    EXPECT_EQ(redone->getOverrideMass(), mass);
}

TEST_F(ComponentConfigDialogTest, AnUndoClosesTheDialogBeforeItsFieldsAreTold)
{
    // The dialog listens to the rocket before its selection does (Java's order): on an UNDO
    // change it ends its bindings first, so that the selection tells its fields nothing.
    ConfigDialogServices services;
    EveryKindDesign      design;
    QWidget              window;
    design.document->addUndoPosition("Change");
    design.component(ComponentKind::NOSE_CONE).setName("Pointy");
    const ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    const std::shared_ptr<ComponentSelection> selection = dialog->configurator().selection();
    ASSERT_NE(selection, nullptr);
    const PhaseCounter phases(*selection);
    design.document->undo();
    EXPECT_TRUE(dialog->isDisposed());
    EXPECT_FALSE(selection->isValid());
    EXPECT_EQ(phases.count(), 0);
}

TEST_F(ComponentConfigDialogTest, TheNameFieldsContextMenuLeavesAStageNamePending)
{
    // Qt's line edit has a context menu, which takes the focus (Qt::PopupFocusReason); Java's
    // text field has none. A stage's new name (a TREE change, which closes the dialog) stays
    // pending while the menu is open, as Qt's editingFinished() does, and OK commits it.
    ConfigDialogServices         services;
    EveryKindDesign              design;
    QWidget                      window;
    const std::string            before = design.booster->getName();
    ComponentConfigDialog* const dialog =
        showActiveDialog(window, services.services(), *design.document, *design.booster);
    ASSERT_NE(dialog, nullptr);
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Upper"));
    {
        const OffscreenPopupWarnings warnings;
        QContextMenuEvent            event(QContextMenuEvent::Mouse, QPoint(5, 5),
                                           name.mapToGlobal(QPoint(5, 5)));
        QApplication::sendEvent(&name, &event);
        QWidget* const menu = QApplication::activePopupWidget();
        ASSERT_NE(menu, nullptr);
        EXPECT_EQ(design.booster->getName(), before);
        EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
        menu->close();
    }
    EXPECT_EQ(design.booster->getName(), before);
    dialog->ok();
    EXPECT_EQ(design.booster->getName(), "Upper");
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
}

TEST_F(ComponentConfigDialogTest, AUnitMenuCommitsAPendingStageName)
{
    // As in OpenRocket: its unit selector's popup takes the focus (a temporary focus loss, which
    // RocketComponentConfig's listener commits), so the stage's new name is committed and the
    // dialog closes (a TREE change) under the menu.
    ConfigDialogServices               services;
    EveryKindDesign                    design;
    QWidget                            window;
    const ComponentConfigDialog* const dialog =
        showActiveDialog(window, services.services(), *design.document, *design.booster);
    ASSERT_NE(dialog, nullptr);
    dialog->configurator().setSelectedTab(QStringLiteral("Override"));
    auto* check    = childNamed<BoolCheckBox>(*dialog, QStringLiteral("MassOverridden"));
    auto* selector = childNamed<UnitSelector>(*dialog, QStringLiteral("OverrideMass"));
    ASSERT_NE(check, nullptr);
    ASSERT_NE(selector, nullptr);
    check->click();
    ASSERT_TRUE(selector->isEnabled());
    QLineEdit& name = dialog->configurator().nameField();
    name.setFocus();
    name.selectAll();
    QTest::keyClicks(&name, QStringLiteral("Upper"));
    {
        const OffscreenPopupWarnings warnings;
        selector->showUnitMenu();
        EXPECT_EQ(design.booster->getName(), "Upper");
        EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
        QWidget* const menu = QApplication::activePopupWidget();
        ASSERT_NE(menu, nullptr);
        menu->close();
    }
}

TEST_F(ComponentConfigDialogTest, APanelsOwnButtonIsNoDefaultButton)
{
    // A QPushButton a panel adds (run 3: "Convert to freeform", "Split fins") would become the
    // dialog's default button when it is shown (QDialog), and Enter in a combo box would click
    // it; Java's dialog has no default button.
    ConfigDialogServices   services;
    EveryKindDesign        design;
    QWidget                window;
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    auto* finish = childNamed<EnumComboBox>(*dialog, QStringLiteral("Finish"));
    ASSERT_NE(finish, nullptr);
    QWidget& page = *finish->parentWidget();
    ASSERT_NE(page.layout(), nullptr);
    auto* button = makeChild<QPushButton>(&page, QStringLiteral("Panel action"));
    page.layout()->addWidget(button);
    const QSignalSpy clicks(button, &QPushButton::clicked);
    dialog->hide();
    dialog->show();
    dialog->activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(dialog));
    EXPECT_FALSE(button->isDefault());
    EXPECT_FALSE(button->autoDefault());
    finish->setFocus();
    QTest::keyClick(finish, Qt::Key_Return);
    EXPECT_EQ(clicks.count(), 0);
    EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
}

// =========================================================================== the parts library

/// The kinds whose new component's dialog opens the parts library with @p preferences.
[[nodiscard]] std::vector<std::string_view> kindsOpeningThePartsLibrary(
    const Preferences& preferences)
{
    std::vector<std::string_view> kinds;
    for (const ComponentKind kind : kAllComponentKinds)
    {
        if (ComponentConfigDialog::opensPartsLibrary(preferences, kind))
        {
            kinds.push_back(className(kind));
        }
    }
    return kinds;
}

/// The kinds that take presets (RocketComponent.getPresetType() != null).
[[nodiscard]] std::vector<std::string_view> presetKinds()
{
    std::vector<std::string_view> kinds;
    for (const ComponentKind kind : kAllComponentKinds)
    {
        if (presetTypeOf(kind).has_value())
        {
            kinds.push_back(className(kind));
        }
    }
    return kinds;
}

/// @p kinds without @p kind.
[[nodiscard]] std::vector<std::string_view> without(std::vector<std::string_view> kinds,
                                                    ComponentKind                 kind)
{
    std::erase(kinds, className(kind));
    return kinds;
}

TEST_F(ComponentConfigDialogTest, ThePartsLibraryOpensForTheKindsThatTakePresetsAsThePreferencesSay)
{
    // ComponentConfigDialog.showDialog(): getPresetType() != null &&
    // getBoolean(getComponentName() + "AlwaysOpenPreset", true) && isAutoOpenPartsLibrary().
    InMemoryPreferences preferences;
    EXPECT_EQ(presetKinds().size(), 13U);
    EXPECT_EQ(kindsOpeningThePartsLibrary(preferences), presetKinds());
    // Read without storing (Java's getBoolean() stores the defaults).
    EXPECT_TRUE(preferences.keys().empty());
    // One key per kind: the inner tube and the tube fin set, which take body tubes, keep theirs.
    preferences.putBoolean(GuiPreferenceKeys::alwaysOpenPresetKey(ComponentKind::BODY_TUBE), false);
    EXPECT_EQ(kindsOpeningThePartsLibrary(preferences),
              without(presetKinds(), ComponentKind::BODY_TUBE));
    preferences.putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, false);
    EXPECT_TRUE(kindsOpeningThePartsLibrary(preferences).empty());
}

/// Services whose parts library opens for a new component (the product's default; the test
/// services turn it off), with the small preset database.
class PartsLibraryServices : public ConfigDialogServices
{
public:
    PartsLibraryServices()
    {
        preferences().putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, true);
        presets() = QtRocket::Test::smallPresetDatabase();
    }
};

/// A body tube added to the booster of @p design as the add button adds it: the undo position
/// "Add Body Tube", then the tube.
RocketComponent& addBodyTube(const EveryKindDesign& design)
{
    design.document->addUndoPosition("Add Body Tube");
    return design.booster->addChild(std::make_unique<BodyTube>());
}

TEST_F(ComponentConfigDialogTest, ANewComponentsDialogOpensThePartsLibraryOnceItIsShown)
{
    // showDialog(): the dialog shown, then (no undo position for a new component) selectPreset(),
    // whose invokeLater asks one turn later, for the new component and its neighbours' diameters.
    PartsLibraryServices services;
    EveryKindDesign      design;
    QWidget              window;
    RocketComponent&     added = addBodyTube(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, added, {},
                                      true);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isVisible());
    EXPECT_TRUE(services.prompts().presetRequests().empty());

    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CLOSED, .preset = nullptr});
    QCoreApplication::processEvents();
    ASSERT_EQ(services.prompts().presetRequests().size(), 1U);
    const UserPrompts::PresetRequest& request = services.prompts().presetRequests().front();
    const PresetDiameterFilters       filters = PresetDiameterFilters::forComponent(added);
    EXPECT_EQ(request.componentKind, ComponentKind::BODY_TUBE);
    // Behind the booster's tube: its aft diameter; nothing after it.
    ASSERT_TRUE(filters.foreDiameter.has_value());
    EXPECT_EQ(request.foreDiameter, filters.foreDiameter);
    EXPECT_EQ(request.aftDiameter, std::nullopt);
    EXPECT_FALSE(request.flippedNoseCone);
    ASSERT_EQ(services.prompts().prompts().size(), 1U);
    EXPECT_EQ(services.prompts().prompts().front().kind, ScriptedUserPrompts::Kind::PRESET);
    EXPECT_EQ(services.prompts().prompts().front().texts.at(0), QStringLiteral("Body Tube"));

    // Closed without a choice: the dialog stays as it was.
    EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
    EXPECT_FALSE(dialog->isModified());
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Add Body Tube");
    QCoreApplication::processEvents();
    EXPECT_EQ(services.prompts().presetRequests().size(), 1U);
}

/// Whether the dialog of @p main (with @p others), shown with @p services, asks for a preset
/// within a turn of the event loop; the dialog is closed afterwards.
[[nodiscard]] bool asksForAPreset(QWidget& window, const PartsLibraryServices& services,
                                  OpenRocketDocument& document, RocketComponent& main,
                                  std::span<RocketComponent* const> others, bool isNew)
{
    const std::size_t before = services.prompts().presetRequests().size();
    ComponentConfigDialog::showDialog(window, services.services(), document, main, others, isNew);
    QCoreApplication::processEvents();
    ComponentConfigDialog::disposeDialog();
    return services.prompts().presetRequests().size() > before;
}

TEST_F(ComponentConfigDialogTest, ThePartsLibraryOpensOnlyForOneNewComponent)
{
    PartsLibraryServices services;
    EveryKindDesign      design;
    QWidget              window;
    // An existing component.
    EXPECT_FALSE(asksForAPreset(window, services, *design.document,
                                design.component(ComponentKind::BODY_TUBE), {}, false));
    // A new one with others selected (Java: getConfigListeners().isEmpty()).
    RocketComponent&                      added = addBodyTube(design);
    const std::array<RocketComponent*, 1> others{design.boosterTube};
    EXPECT_FALSE(asksForAPreset(window, services, *design.document, added, others, true));
    // A new one of a kind without presets.
    design.document->addUndoPosition("Add Mass Component");
    RocketComponent& mass = design.boosterTube->addChild(std::make_unique<MassComponent>());
    EXPECT_FALSE(asksForAPreset(window, services, *design.document, mass, {}, true));
    // A new one whose kind's "Always open" is off; another kind's is on.
    services.preferences().putBoolean(
        GuiPreferenceKeys::alwaysOpenPresetKey(ComponentKind::BODY_TUBE), false);
    EXPECT_FALSE(asksForAPreset(window, services, *design.document, added, {}, true));
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CLOSED, .preset = nullptr});
    EXPECT_TRUE(asksForAPreset(window, services, *design.document,
                               design.component(ComponentKind::INNER_TUBE), {}, true));
    // The General preferences' box off.
    services.preferences().putBoolean(GuiPreferenceKeys::kAutoOpenPartsLibrary, false);
    EXPECT_FALSE(asksForAPreset(window, services, *design.document,
                                design.component(ComponentKind::INNER_TUBE), {}, true));
    EXPECT_EQ(services.prompts().presetRequests().size(), 1U);
}

TEST_F(ComponentConfigDialogTest, ADialogClosedBeforeItsPartsLibraryOpensAsksNothing)
{
    PartsLibraryServices services;
    EveryKindDesign      design;
    QWidget              window;
    RocketComponent&     added = addBodyTube(design);
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, added, {},
                                      true);
    ComponentConfigDialog::disposeDialog();
    QCoreApplication::processEvents();
    EXPECT_TRUE(services.prompts().presetRequests().empty());
}

// The Cancel of a new component after a preset (bug 15 of the plan's section 10.3). Expectations
// marked "probe" are OpenRocket's, printed by
// tools/openrocket-gui-pins/preset-chooser/probe/NewComponentCancelProbe.java (the family's
// hand-pinned.txt keeps them): the same steps on a new design, OpenRocket's Cancel being one
// document.undo().

/// The undo and redo descriptions of @p document and the body tube @p id, as the probe prints
/// them.
[[nodiscard]] std::string cancelState(const OpenRocketDocument& document, const Uuid& id)
{
    const auto quoted = [](const std::optional<std::string>& text) {
        return text.has_value() ? "'" + *text + "'" : std::string("null");
    };
    const std::string line = "undo " + quoted(document.getUndoDescription()) + " redo " +
                             quoted(document.getRedoDescription()) + "; ";
    const auto*       tube = dynamic_cast<const BodyTube*>(document.getRocket().findComponent(id));
    if (tube == nullptr)
    {
        return line + "no tube";
    }
    const ComponentPreset* preset = tube->getPresetComponent();
    return line + std::format("tube preset {} OR {} L {}",
                              preset != nullptr ? preset->getPartNo() : std::string("null"),
                              tube->getOuterRadius(), tube->getLength());
}

/// The probe's design: a new rocket whose stage gets a body tube as the add button adds it.
struct NewTubeDesign
{
    std::unique_ptr<OpenRocketDocument> document = OpenRocketDocumentFactory::createNewRocket();
    Uuid                                id       = addTube(*document);

    [[nodiscard]] RocketComponent& tube() const
    {
        RocketComponent* found = document->getRocket().findComponent(id);
        QTROCKET_ASSERT(found != nullptr);
        return *found;
    }

    /// The undo position "Add Body Tube" and the tube, in a history that starts there; its id.
    [[nodiscard]] static Uuid addTube(OpenRocketDocument& document)
    {
        document.clearUndo();
        document.addUndoPosition("Add Body Tube");
        return document.getRocket().getChild(0).addChild(std::make_unique<BodyTube>()).getId();
    }
};

TEST_F(ComponentConfigDialogTest, CancelAfterAPresetRemovesTheNewComponent)
{
    PartsLibraryServices services;
    const NewTubeDesign  design;
    QWidget              window;
    // Probe: "with a preset, added".
    EXPECT_EQ(cancelState(*design.document, design.id),
              "undo 'Add Body Tube' redo null; tube preset null OR 0.025 L 0.2");
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_GT(tubes.size(), 7U);
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = tubes.at(7)});
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, design.tube(),
                                      {}, true);
    QCoreApplication::processEvents();
    // Probe: "with a preset, chosen".
    EXPECT_EQ(cancelState(*design.document, design.id),
              "undo 'Use Preset Body Tube' redo null; tube preset 85865 OR 0.03175 L 0.3937");
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(dialog->isModified());

    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog->cancel();
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    // OpenRocket's one undo leaves the tube (probe: "with a preset, after Cancel's undo": undo
    // 'Add Body Tube' redo 'Use Preset Body Tube'; tube preset null OR 0.025 L 0.2). QtRocket
    // undoes on as the question says, to the probe's "after a second undo".
    EXPECT_EQ(cancelState(*design.document, design.id), "undo null redo 'Add Body Tube'; no tube");
    const std::vector<ScriptedUserPrompts::Prompt>& prompts = services.prompts().prompts();
    ASSERT_EQ(prompts.size(), 2U);
    EXPECT_EQ(prompts.at(0).kind, ScriptedUserPrompts::Kind::PRESET);
    EXPECT_EQ(prompts.at(1).kind, ScriptedUserPrompts::Kind::DISCARD_CHANGES);
    EXPECT_EQ(prompts.at(1).texts, QStringList{QStringLiteral("COMPONENT_ADDITION")});
}

TEST_F(ComponentConfigDialogTest, CancelOfANewComponentWithoutAPresetUndoesOnce)
{
    // Probe: "without a preset": the one undo removes the tube.
    PartsLibraryServices services;
    const NewTubeDesign  design;
    QWidget              window;
    services.prompts().answerPreset(
        {.outcome = UserPrompts::PresetOutcome::CLOSED, .preset = nullptr});
    ComponentConfigDialog::showDialog(window, services.services(), *design.document, design.tube(),
                                      {}, true);
    QCoreApplication::processEvents();
    EXPECT_EQ(cancelState(*design.document, design.id),
              "undo 'Add Body Tube' redo null; tube preset null OR 0.025 L 0.2");
    ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    // The preference "ask" off: Cancel discards at once.
    services.preferences().setShowDiscardConfirmation(false);
    dialog->cancel();
    EXPECT_EQ(cancelState(*design.document, design.id), "undo null redo 'Add Body Tube'; no tube");
}

TEST_F(ComponentConfigDialogTest, CancelOfAnExistingComponentUndoesThePresetOnly)
{
    // As OpenRocket (no bug of the plan's list): the preset chosen after another edit of the
    // session is a step of its own, and Cancel's one undo leaves the edit (PresetModelProbe: "two
    // tubes": "chosen", then "one undo" back to the edited length).
    PartsLibraryServices   services;
    EveryKindDesign        design;
    QWidget                window;
    const Uuid             id     = design.component(ComponentKind::BODY_TUBE).getId();
    ComponentConfigDialog* dialog = showBodyTubeDialog(window, services, design);
    ASSERT_NE(dialog, nullptr);
    auto& tube = dynamic_cast<BodyTube&>(design.component(ComponentKind::BODY_TUBE));
    tube.setLength(0.25);
    PresetComboBox* combo = dialog->configurator().presetComboBox();
    ASSERT_NE(combo, nullptr);
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_FALSE(tubes.empty());
    EXPECT_TRUE(combo->choosePreset(tubes.front()));
    EXPECT_EQ(design.document->getUndoDescription(),
              std::optional<std::string>("Use Preset Body Tube"));

    services.prompts().answerDiscardChanges({.discard = true, .doNotAskAgain = false});
    dialog->cancel();
    const auto* restored =
        dynamic_cast<const BodyTube*>(design.document->getRocket().findComponent(id));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getPresetComponent(), nullptr);
    EXPECT_EQ(restored->getLength(), 0.25);
    EXPECT_EQ(design.document->getUndoDescription(),
              std::optional<std::string>("Modify Body Tube"));
}

/// A parts library that runs an event loop, as the modal dialog does, in which the test does what
/// it gives (an undo, the window going) and the deferred deletions posted meanwhile are carried
/// out (as Qt 6 does in the loop that posted them); then it answers @p answer.
class NestedPartsLibrary final : public Test::ForwardingUserPrompts
{
public:
    NestedPartsLibrary(UserPrompts& delegate, std::function<void()> whileOpen, PresetAnswer answer)
      : ForwardingUserPrompts(delegate),
        m_whileOpen(std::move(whileOpen)),
        m_answer(std::move(answer))
    {
    }

    [[nodiscard]] int asked() const noexcept { return m_asked; }

    [[nodiscard]] PresetAnswer choosePreset(QWidget* /*parent*/, const GuiServices& /*services*/,
                                            const PresetRequest& /*request*/) override
    {
        ++m_asked;
        QEventLoop loop;
        QTimer::singleShot(0, &loop, [this, &loop] {
            m_whileOpen();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            loop.quit();
        });
        loop.exec();
        return m_answer;
    }

private:
    std::function<void()> m_whileOpen;
    PresetAnswer          m_answer;
    int                   m_asked{0};
};

TEST_F(ComponentConfigDialogTest, AnUndoWhileThePartsLibraryIsOpenAppliesNothing)
{
    // The undo closes the dialog (and deletes it, in the parts library's loop); the preset then
    // chosen reaches no component (proved under ASan).
    PartsLibraryServices                        services;
    EveryKindDesign                             design;
    QWidget                                     window;
    RocketComponent&                            added = addBodyTube(design);
    const Uuid                                  id    = added.getId();
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_FALSE(tubes.empty());
    NestedPartsLibrary prompts(
        services.prompts(), UndoDocument{design.document.get()},
        {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = tubes.front()});
    const GuiServices own = Gui::Test::withPrompts(services.services(), prompts);
    ComponentConfigDialog::showDialog(window, own, *design.document, added, {}, true);
    const QPointer<ComponentConfigDialog> guard(ComponentConfigDialog::dialog());
    ASSERT_FALSE(guard.isNull());
    QCoreApplication::processEvents();
    EXPECT_EQ(prompts.asked(), 1);
    EXPECT_TRUE(guard.isNull());
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(design.document->getRocket().findComponent(id), nullptr);
    EXPECT_EQ(design.document->getRedoDescription(), std::optional<std::string>("Add Body Tube"));
}

TEST_F(ComponentConfigDialogTest, TheWindowGoingWhileThePartsLibraryIsOpenLeavesThePanelAlone)
{
    // The dialog goes with its window while the parts library is open: nothing of the panel is
    // touched afterwards (proved under ASan) and nothing is applied.
    PartsLibraryServices                        services;
    EveryKindDesign                             design;
    std::unique_ptr<QWidget>                    window = std::make_unique<QWidget>();
    RocketComponent&                            added  = addBodyTube(design);
    const std::vector<PresetBinding::PresetPtr> tubes =
        services.presets().listForType(ComponentPresetType::BODY_TUBE);
    ASSERT_FALSE(tubes.empty());
    NestedPartsLibrary prompts(
        services.prompts(), DeleteWindow{&window},
        {.outcome = UserPrompts::PresetOutcome::CHOSEN, .preset = tubes.front()});
    const GuiServices own = Gui::Test::withPrompts(services.services(), prompts);
    ComponentConfigDialog::showDialog(*window, own, *design.document, added, {}, true);
    QCoreApplication::processEvents();
    EXPECT_EQ(prompts.asked(), 1);
    EXPECT_EQ(window, nullptr);
    EXPECT_EQ(ComponentConfigDialog::dialog(), nullptr);
    EXPECT_EQ(added.getPresetComponent(), nullptr);
    EXPECT_EQ(design.document->getUndoDetail().nextDescription, "Add Body Tube");
}

/// What the application's parts library showed: its size and parent, and the preset of the row
/// it closed with.
struct ShownPartsLibrary
{
    QSize                    size;
    const QWidget*           parent{nullptr};
    PresetBinding::PresetPtr preset;
};

/// Records @p shown of the parts library, then selects its first row and clicks Close.
struct RecordAndClose
{
    ShownPartsLibrary* shown;

    void operator()(QWidget& widget) const
    {
        auto* const dialog = qobject_cast<ComponentPresetChooserDialog*>(&widget);
        ASSERT_NE(dialog, nullptr);
        shown->size   = dialog->size();
        shown->parent = dialog->parentWidget();
        dialog->table().selectRow(0);
        shown->preset = dialog->table().selectedPreset();
        dialog->closeButton().click();
    }
};

TEST_F(ComponentConfigDialogTest, TheApplicationsPartsLibraryOpensOverTheMainWindowAndApplies)
{
    // The application's questions (WidgetUserPrompts): the parts library is a child of the panel
    // (Java: SwingUtilities.getWindowAncestor(RocketComponentConfig.this), the dialog), at 70 % of
    // the window the dialog belongs to (Java: owner.getParent()); its Close applies the selected
    // row's preset to the new component.
    const PartsLibraryServices services;
    EveryKindDesign            design;
    QWidget                    main;
    main.resize(1000, 800);
    WidgetUserPrompts                widgets;
    const GuiServices                own   = withPrompts(services.services(), widgets);
    RocketComponent&                 added = addBodyTube(design);
    ShownPartsLibrary                shown;
    const Gui::Test::NextModalDialog close(RecordAndClose{.shown = &shown});
    ComponentConfigDialog::showDialog(main, own, *design.document, added, {}, true);
    const ComponentConfigDialog* dialog = ComponentConfigDialog::dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_TRUE(QTest::qWaitFor([&close] { return close.acted(); }));
    EXPECT_EQ(shown.size, QSize(700, 560));
    EXPECT_EQ(shown.parent, &dialog->configurator());
    ASSERT_NE(shown.preset, nullptr);
    EXPECT_EQ(added.getPresetComponent(), shown.preset.get());
    EXPECT_EQ(design.document->getUndoDescription(),
              std::optional<std::string>("Use Preset Body Tube"));
    EXPECT_EQ(ComponentConfigDialog::dialog(), dialog);
}

}  // namespace

}  // namespace QtRocket::Gui
